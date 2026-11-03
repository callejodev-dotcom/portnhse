// NHSE Puente: abre la partida de Animal Crossing: New Horizons (partida del dispositivo) y la deja leer y escribir
// por Wi-Fi a NHSE para Mac, con el juego cerrado. Al empezar guarda una copia de la partida en la SD, y lo que llega
// solo se queda en la partida cuando NHSE dice «GUARDA» (hasta entonces, cerrar el puente lo deshace todo).
//
// Protocolo (TCP, puerto 7001, órdenes de una línea acabada en \n):
//   HOLA                      -> NHSE-PUENTE 1
//   LISTA                     -> una línea «<tamaño> <ruta>» por archivo, y FIN
//   LEE <ruta>                -> «<tamaño>» y los bytes, o ERROR <motivo>
//   ESCRIBE <tamaño> <ruta>   -> (llegan los bytes) OK, o ERROR <motivo>
//   GUARDA                    -> OK, o ERROR <motivo>
//   ADIOS                     -> cierra la conexión
#include <switch.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ACNH 0x01006F8002326000ULL
#define PORT 7001
#define DEV "acnh"
#define ROOT "acnh:/"
#define MAX_FILES 512
#define PATH_LEN 256

static char g_files[MAX_FILES][PATH_LEN];
static size_t g_sizes[MAX_FILES];
static int g_count;
static bool g_dirty; // something written and not saved yet

static void say(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    consoleUpdate(NULL);
}

// ---------------------------------------------------------------------------------------------------------------- files
static void scan(const char* rel)
{
    char path[PATH_LEN + 16];
    snprintf(path, sizeof(path), ROOT "%s", rel);
    DIR* d = opendir(path);
    if (!d)
        return;
    struct dirent* e;
    while ((e = readdir(d)) && g_count < MAX_FILES)
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char sub[PATH_LEN];
        snprintf(sub, sizeof(sub), "%s%s%s", rel, *rel ? "/" : "", e->d_name);
        char full[PATH_LEN + 16];
        snprintf(full, sizeof(full), ROOT "%s", sub);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            scan(sub);
        else
        {
            strncpy(g_files[g_count], sub, PATH_LEN - 1);
            g_sizes[g_count] = st.st_size;
            g_count++;
        }
    }
    closedir(d);
}

static void mkdirs(const char* path)
{
    char tmp[PATH_LEN * 2];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    for (char* p = strchr(tmp + 1, '/'); p; p = strchr(p + 1, '/'))
    {
        *p = 0;
        if (strchr(tmp, ':') + 1 != p) // not the "sdmc:" part
            mkdir(tmp, 0777);
        *p = '/';
    }
}

static bool copy_file(const char* from, const char* to)
{
    FILE* in = fopen(from, "rb");
    if (!in)
        return false;
    mkdirs(to);
    FILE* out = fopen(to, "wb");
    if (!out)
    {
        fclose(in);
        return false;
    }
    static u8 buf[0x40000];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        ok &= fwrite(buf, 1, n, out) == n;
    fclose(in);
    fclose(out);
    return ok;
}

// A copy of the whole save in sdmc:/switch/nhse-puente/copias/<date>/ before anything can change it.
static bool backup(char* where, size_t where_size)
{
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(where, where_size, "sdmc:/switch/nhse-puente/copias/%04d%02d%02d-%02d%02d%02d/", tm->tm_year + 1900,
        tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    for (int i = 0; i < g_count; i++)
    {
        char from[PATH_LEN + 16], to[PATH_LEN * 2];
        snprintf(from, sizeof(from), ROOT "%s", g_files[i]);
        snprintf(to, sizeof(to), "%s%s", where, g_files[i]);
        if (!copy_file(from, to))
            return false;
    }
    return true;
}

static bool valid_path(const char* p)
{
    return *p && *p != '/' && !strstr(p, "..") && strlen(p) < PATH_LEN;
}

// ------------------------------------------------------------------------------------------------------------- network
static bool send_all(int s, const void* data, size_t len)
{
    const u8* p = data;
    while (len)
    {
        ssize_t n = send(s, p, len, 0);
        if (n <= 0)
            return false;
        p += n;
        len -= n;
    }
    return true;
}

static bool send_line(int s, const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(line, sizeof(line) - 1, fmt, args);
    va_end(args);
    line[n++] = '\n';
    return send_all(s, line, n);
}

static bool recv_line(int s, char* line, size_t size)
{
    size_t n = 0;
    while (n < size - 1)
    {
        char c;
        if (recv(s, &c, 1, 0) != 1)
            return false;
        if (c == '\n')
            break;
        if (c != '\r')
            line[n++] = c;
    }
    line[n] = 0;
    return true;
}

static bool recv_all(int s, void* data, size_t len)
{
    u8* p = data;
    while (len)
    {
        ssize_t n = recv(s, p, len, 0);
        if (n <= 0)
            return false;
        p += n;
        len -= n;
    }
    return true;
}

static void cmd_read(int s, const char* rel)
{
    char path[PATH_LEN + 16];
    snprintf(path, sizeof(path), ROOT "%s", rel);
    FILE* f = valid_path(rel) ? fopen(path, "rb") : NULL;
    if (!f)
    {
        send_line(s, "ERROR no existe %s", rel);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    send_line(s, "%ld", size);
    static u8 buf[0x40000];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        if (!send_all(s, buf, n))
            break;
    fclose(f);
    say("  enviado %s (%ld bytes)", rel, size);
}

static void cmd_write(int s, size_t size, const char* rel)
{
    u8* data = malloc(size ? size : 1);
    if (!data)
    {
        // Drain what is coming so the connection stays in step.
        static u8 junk[0x10000];
        for (size_t left = size; left;)
        {
            size_t k = left < sizeof(junk) ? left : sizeof(junk);
            if (!recv_all(s, junk, k))
                return;
            left -= k;
        }
        send_line(s, "ERROR sin memoria");
        return;
    }
    if (!recv_all(s, data, size))
    {
        free(data);
        return;
    }
    if (!valid_path(rel))
    {
        free(data);
        send_line(s, "ERROR ruta %s", rel);
        return;
    }
    char path[PATH_LEN + 16];
    snprintf(path, sizeof(path), ROOT "%s", rel);
    mkdirs(path);
    FILE* f = fopen(path, "wb");
    bool ok = f && fwrite(data, 1, size, f) == size;
    if (f)
        fclose(f);
    free(data);
    if (!ok)
    {
        send_line(s, "ERROR no se pudo escribir %s (%d)", rel, errno);
        return;
    }
    g_dirty = true;
    say("  recibido %s (%zu bytes)", rel, size);
    send_line(s, "OK");
}

static void cmd_save(int s)
{
    Result rc = fsdevCommitDevice(DEV);
    if (R_FAILED(rc))
    {
        send_line(s, "ERROR 0x%x", rc);
        say("  ERROR al guardar: 0x%x", rc);
        return;
    }
    g_dirty = false;
    say("  GUARDADO en la partida.");
    send_line(s, "OK");
}

static void serve(int s)
{
    char line[PATH_LEN + 64];
    while (recv_line(s, line, sizeof(line)))
    {
        if (!strcmp(line, "HOLA"))
            send_line(s, "NHSE-PUENTE 1");
        else if (!strcmp(line, "LISTA"))
        {
            g_count = 0;
            scan("");
            for (int i = 0; i < g_count; i++)
                send_line(s, "%zu %s", g_sizes[i], g_files[i]);
            send_line(s, "FIN");
        }
        else if (!strncmp(line, "LEE ", 4))
            cmd_read(s, line + 4);
        else if (!strncmp(line, "ESCRIBE ", 8))
        {
            char* sp = strchr(line + 8, ' ');
            if (!sp)
            {
                send_line(s, "ERROR orden");
                continue;
            }
            *sp = 0;
            cmd_write(s, strtoull(line + 8, NULL, 10), sp + 1);
        }
        else if (!strcmp(line, "GUARDA"))
            cmd_save(s);
        else if (!strcmp(line, "ADIOS"))
            break;
        else
            send_line(s, "ERROR orden desconocida");
    }
}

// --------------------------------------------------------------------------------------------------------------- main
int main(int argc, char* argv[])
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    appletSetAutoSleepDisabled(true);

    say("NHSE Puente - Animal Crossing: New Horizons");
    say("-------------------------------------------");

    FsFileSystem fs;
    Result rc = fsOpen_DeviceSaveData(&fs, ACNH);
    bool ready = false;
    char where[PATH_LEN];
    if (R_FAILED(rc))
        say("No encuentro la partida de Animal Crossing (0x%x). Abre el juego una vez y vuelve.", rc);
    else if (fsdevMountDevice(DEV, fs) < 0)
        say("No he podido abrir la partida.");
    else
    {
        scan("");
        say("Partida abierta: %d archivos.", g_count);
        say("Copia de seguridad...");
        if (backup(where, sizeof(where)))
        {
            say("Copia guardada en %s", where + 5);
            ready = true;
        }
        else
            say("No he podido hacer la copia de seguridad: no toco nada.");
    }

    int server = -1;
    if (ready && R_SUCCEEDED(socketInitializeDefault()))
    {
        server = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY };
        int yes = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(server, 1) < 0)
        {
            say("No he podido abrir el puerto %d.", PORT);
            close(server);
            server = -1;
        }
        else
        {
            fcntl(server, F_SETFL, fcntl(server, F_GETFL, 0) | O_NONBLOCK);
            struct in_addr ip = { .s_addr = gethostid() };
            say("");
            say("Listo. En NHSE (Mac): «Abrir desde el puente» con la IP %s", inet_ntoa(ip));
            say("Con el juego cerrado. Pulsa + para salir.");
        }
    }
    else if (ready)
        say("Sin red: activa el Wi-Fi.");

    while (appletMainLoop())
    {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        if (server >= 0)
        {
            int client = accept(server, NULL, NULL);
            if (client >= 0)
            {
                fcntl(client, F_SETFL, fcntl(client, F_GETFL, 0) & ~O_NONBLOCK);
                say("NHSE conectado.");
                serve(client);
                close(client);
                say(g_dirty ? "NHSE desconectado (sin guardar lo último)." : "NHSE desconectado.");
            }
        }
        consoleUpdate(NULL);
        svcSleepThread(50000000ULL);
    }

    if (server >= 0)
        close(server);
    socketExit();
    // Unmounting without a commit drops whatever was written and not saved.
    fsdevUnmountDevice(DEV);
    appletSetAutoSleepDisabled(false);
    consoleExit(NULL);
    return 0;
}
