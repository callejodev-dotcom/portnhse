#include "registro.hpp"
#include <switch.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

namespace registro {

namespace {
FILE* g_f = nullptr;
constexpr const char* RUTA = "sdmc:/switch/NHSE/registro.txt";
constexpr const char* ANTERIOR = "sdmc:/switch/NHSE/registro-anterior.txt";
} // namespace

void empezar(const char* version)
{
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/NHSE", 0777);
    remove(ANTERIOR);
    rename(RUTA, ANTERIOR);
    g_f = fopen(RUTA, "w");
    time_t t = time(nullptr);
    char fecha[32];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", localtime(&t));
    apuntar("NHSE para Switch %s, empieza el %s", version, fecha);
#ifdef __SWITCH__
    if (R_SUCCEEDED(setsysInitialize()))
    {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw)))
            apuntar("firmware %s", fw.display_version);
        setsysExit();
    }
    apuntar("modo %s", appletGetAppletType() == AppletType_Application ? "juego (memoria completa)" : "Álbum (memoria limitada)");
#endif
}

void apuntar(const char* formato, ...)
{
    if (!g_f)
        return;
    const time_t t = time(nullptr);
    char hora[16];
    strftime(hora, sizeof(hora), "%H:%M:%S", localtime(&t));
    fprintf(g_f, "%s  ", hora);
    va_list a;
    va_start(a, formato);
    vfprintf(g_f, formato, a);
    va_end(a);
    fputc('\n', g_f);
    fflush(g_f); // so the line is there even if it hangs right after
}

void terminar()
{
    apuntar("termina bien");
    if (g_f)
        fclose(g_f);
    g_f = nullptr;
}

} // namespace registro

#ifdef __SWITCH__
// Si la app se cuelga (un fallo de memoria...), libnx llama aquí: se apunta qué y dónde (la posición dentro de NHSE.nro,
// para buscarla con addr2line en NHSE.elf) y se sale.
extern "C" {
alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
extern char __start__;

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    const u64 base = (u64)&__start__;
    registro::apuntar("¡CUELGUE! tipo %u, en 0x%lx (NHSE+0x%lx), vuelta a 0x%lx (NHSE+0x%lx), dirección 0x%lx",
        ctx->error_desc, ctx->pc.x, ctx->pc.x - base, ctx->lr.x, ctx->lr.x - base, ctx->far.x);
    // A few return addresses on the stack, to know the way it came.
    const u64* sp = (const u64*)ctx->sp.x;
    for (int i = 0; i < 64 && sp; i++)
        if (sp[i] >= base && sp[i] < base + 0x4000000)
            registro::apuntar("  pila: NHSE+0x%lx", sp[i] - base);
    registro::terminar();
}
}
#endif
