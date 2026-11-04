#include "partida.hpp"
#include "registro.hpp"
#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <map>
#include <sys/stat.h>

using namespace acnh;

namespace partida {

std::vector<Jugador> jugadores;
int yo = 0;
std::string isla;
std::string copia;

namespace {
constexpr u64 ACNH = 0x01006F8002326000ULL;
FsFileSystem g_fs;
bool g_abierta = false;
Archivo g_main;
bool g_mainCambiado = false;
struct Otro {
    Archivo a;
    bool cambiado = false;
};
std::map<std::string, Otro> g_otros; // other encrypted files, by path

bool leer(const char* ruta, std::vector<uint8_t>& out, bool callar = false)
{
    FsFile f;
    Result rc = fsFsOpenFile(&g_fs, ruta, FsOpenMode_Read, &f);
    if (R_FAILED(rc))
    {
        if (!callar)
            registro::apuntar("no se abre %s (error 0x%x)", ruta, rc);
        return false;
    }
    s64 tam = 0;
    fsFileGetSize(&f, &tam);
    out.resize(tam);
    u64 leidos = 0;
    rc = fsFileRead(&f, 0, out.data(), tam, FsReadOption_None, &leidos);
    fsFileClose(&f);
    if (R_FAILED(rc) || (s64)leidos != tam)
        registro::apuntar("no se lee %s (error 0x%x, %lu de %ld bytes)", ruta, rc, (unsigned long)leidos, (long)tam);
    return R_SUCCEEDED(rc) && (s64)leidos == tam;
}

bool escribir(const char* ruta, const std::vector<uint8_t>& datos)
{
    FsFile f;
    Result rc = fsFsOpenFile(&g_fs, ruta, FsOpenMode_Write, &f);
    if (R_FAILED(rc))
    {
        registro::apuntar("no se abre para escribir %s (error 0x%x)", ruta, rc);
        return false;
    }
    rc = fsFileWrite(&f, 0, datos.data(), datos.size(), FsWriteOption_Flush);
    fsFileClose(&f);
    registro::apuntar("escrito %s: %s (0x%x, %zu bytes)", ruta, R_SUCCEEDED(rc) ? "bien" : "MAL", rc, datos.size());
    return R_SUCCEEDED(rc);
}

void mkdirs(const std::string& ruta)
{
    for (size_t i = ruta.find('/', 6); i != std::string::npos; i = ruta.find('/', i + 1))
        mkdir(ruta.substr(0, i).c_str(), 0777);
}

// Copia un archivo de la partida a la SD. Si falta y se puede saltar (callar), no es un error.
bool copiarArchivo(const std::string& ruta, const std::string& destino, bool callar = false, bool* estaba = nullptr)
{
    std::vector<uint8_t> datos;
    if (estaba)
        *estaba = false;
    if (!leer(ruta.c_str(), datos, callar))
        return callar;
    if (estaba)
        *estaba = true;
    mkdirs(destino);
    FILE* f = fopen(destino.c_str(), "wb");
    if (!f)
    {
        registro::apuntar("no se crea %s en la SD (errno %d)", destino.c_str(), errno);
        return false;
    }
    const bool ok = fwrite(datos.data(), 1, datos.size(), f) == datos.size();
    if (fclose(f) != 0 || !ok)
    {
        registro::apuntar("no se escribe %s en la SD (errno %d, la SD puede estar llena)", destino.c_str(), errno);
        return false;
    }
    return true;
}

// Copia todo lo que hay en una carpeta de la partida a la SD.
bool copiarCarpeta(const std::string& dentro, const std::string& sd)
{
    FsDir dir;
    if (Result rc = fsFsOpenDirectory(&g_fs, dentro.c_str(), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir); R_FAILED(rc))
    {
        registro::apuntar("no se abre la carpeta %s de la partida (error 0x%x)", dentro.c_str(), rc);
        return false;
    }
    std::vector<FsDirectoryEntry> entradas(64);
    bool ok = true;
    int cuantos = 0;
    while (ok)
    {
        s64 n = 0;
        if (Result rc = fsDirRead(&dir, &n, entradas.size(), entradas.data()); R_FAILED(rc))
        {
            registro::apuntar("no se lee la carpeta %s de la partida (error 0x%x)", dentro.c_str(), rc);
            ok = false;
            break;
        }
        if (n == 0)
            break;
        for (s64 i = 0; i < n && ok; i++, cuantos++)
        {
            std::string nombre = entradas[i].name;
            std::string ruta = dentro + (dentro == "/" ? "" : "/") + nombre;
            ok = entradas[i].type == FsDirEntryType_Dir ? copiarCarpeta(ruta, sd + nombre + "/") : copiarArchivo(ruta, sd + nombre);
        }
    }
    fsDirClose(&dir);
    if (ok && dentro == "/" && cuantos == 0)
    {
        registro::apuntar("la carpeta de la partida sale vacía");
        return false;
    }
    return ok;
}

// Si no se puede mirar qué hay en la partida, se copian los archivos que tiene (los que lee NHSE). Los que no estén se
// saltan; main.dat y los de cada jugador tienen que estar.
bool copiarConocidos(const std::string& sd)
{
    static const char* RAIZ[] = {"main", "landname", "wherearen"};
    static const char* JUGADOR[] = {"personal", "photo_studio_island", "postbox", "profile"};
    for (const char* n : RAIZ)
        for (const char* fin : {".dat", "Header.dat"})
        {
            const std::string a = std::string("/") + n + fin;
            if (!copiarArchivo(a, sd + a.substr(1), std::string(n) != "main"))
                return false;
        }
    for (int i = 0; i < 8; i++)
    {
        const std::string c = "/Villager" + std::to_string(i);
        bool hay = false;
        if (!copiarArchivo(c + "/personal.dat", sd + c.substr(1) + "/personal.dat", true, &hay))
            return false;
        if (!hay)
            continue;
        for (const char* n : JUGADOR)
            for (const char* fin : {".dat", "Header.dat"})
                if (std::string(n) != "personal" || std::string(fin) != ".dat")
                    if (!copiarArchivo(c + "/" + n + fin, sd + c.substr(1) + "/" + n + fin, std::string(n) != "personal"))
                        return false;
    }
    registro::apuntar("copia hecha archivo a archivo");
    return true;
}

// Prepara un archivo para escribirlo: comprobaciones al día y cifrado. Devuelve los datos sin cifrar que se guardan.
std::vector<uint8_t> preparar(Archivo& a)
{
    arreglarComprobaciones(a.datos);
    std::vector<uint8_t> plano = a.datos;
    cifrar(a.cabecera, a.datos);
    return plano;
}

// Lee un archivo guardado y dice si es exactamente lo que se quería guardar y está sano.
bool comprobar(const std::string& ruta, const std::vector<uint8_t>& cabecera, const std::vector<uint8_t>& plano)
{
    Archivo v;
    v.cabecera = cabecera;
    if (!leer(ruta.c_str(), v.datos))
        return false;
    cifrar(v.cabecera, v.datos);
    return v.datos == plano && comprobacionesMal(v.datos) == 0;
}
} // namespace

std::vector<uint8_t>& principal() { return g_main.datos; }
std::vector<uint8_t>& personal(int j) { return jugador(j).a.datos; }
Jugador& jugador(int j) { return jugadores[j < 0 ? yo : j]; }
void cambiaPrincipal() { g_mainCambiado = true; }
void cambiaPersonal(int j) { jugador(j).cambiado = true; }
bool hayCambios()
{
    bool c = g_mainCambiado;
    for (const auto& j : jugadores)
        c |= j.cambiado;
    for (const auto& o : g_otros)
        c |= o.second.cambiado;
    return c;
}

bool leerArchivo(const std::string& ruta, std::vector<uint8_t>& datos) { return g_abierta && leer(ruta.c_str(), datos); }

Archivo* otro(const std::string& ruta)
{
    auto i = g_otros.find(ruta);
    if (i != g_otros.end())
        return &i->second.a;
    Otro o;
    const std::string cabecera = ruta.substr(0, ruta.size() - 4) + "Header.dat";
    if (!g_abierta || !leer(ruta.c_str(), o.a.datos) || !leer(cabecera.c_str(), o.a.cabecera))
        return nullptr;
    cifrar(o.a.cabecera, o.a.datos);
    if (comprobacionesMal(o.a.datos) != 0)
        return nullptr;
    return &(g_otros[ruta] = std::move(o)).a;
}

void cambiaOtro(const std::string& ruta) { g_otros[ruta].cambiado = true; }

std::vector<std::string> archivos(const std::string& carpeta)
{
    std::vector<std::string> r;
    FsDir dir;
    if (!g_abierta || R_FAILED(fsFsOpenDirectory(&g_fs, carpeta.c_str(), FsDirOpenMode_ReadFiles, &dir)))
        return r;
    std::vector<FsDirectoryEntry> e(64);
    for (;;)
    {
        s64 n = 0;
        if (R_FAILED(fsDirRead(&dir, &n, e.size(), e.data())) || n == 0)
            break;
        for (s64 i = 0; i < n; i++)
            r.push_back(e[i].name);
    }
    fsDirClose(&dir);
    std::sort(r.begin(), r.end());
    return r;
}

std::string abrir()
{
    FsSaveDataAttribute attr = {};
    attr.application_id = ACNH;
    attr.save_data_type = FsSaveDataType_Device;
    if (Result rc = fsOpenSaveDataFileSystem(&g_fs, FsSaveDataSpaceId_User, &attr); R_FAILED(rc))
    {
        registro::apuntar("no se abre la partida del juego (error 0x%x)", rc);
        return "No encuentro la partida de Animal Crossing. ¿Está el juego cerrado?";
    }
    g_abierta = true;
    if (!leer("/main.dat", g_main.datos) || !leer("/mainHeader.dat", g_main.cabecera))
        return "No he podido leer la partida.";
    if (g_main.datos.size() != TAM_MAIN)
        return "Esta partida no es de la versión 3.0 del juego: no la toco.";
    cifrar(g_main.cabecera, g_main.datos);
    if (comprobacionesMal(g_main.datos) != 0)
        return "La partida no está sana (sus comprobaciones no cuadran): no la toco.";

    // Los jugadores: Villager0 (el que llegó primero) a Villager7.
    for (int i = 0; i < 8; i++)
    {
        Jugador j;
        j.carpeta = "/Villager" + std::to_string(i);
        if (!leer((j.carpeta + "/personal.dat").c_str(), j.a.datos, true) || !leer((j.carpeta + "/personalHeader.dat").c_str(), j.a.cabecera))
            continue;
        if (j.a.datos.size() != TAM_PERSONAL)
            return "Esta partida no es de la versión 3.0 del juego: no la toco.";
        cifrar(j.a.cabecera, j.a.datos);
        if (comprobacionesMal(j.a.datos) != 0)
            return "La partida no está sana (sus comprobaciones no cuadran): no la toco.";
        j.nombre = nombreJugador(j.a.datos);
        jugadores.push_back(std::move(j));
    }
    if (jugadores.empty())
        return "No he podido leer la partida.";
    isla = nombreIsla(jugadores[0].a.datos);

    time_t t = time(nullptr);
    char fecha[32];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d %H.%M.%S", localtime(&t));
    copia = std::string(CARPETA) + "copias/" + fecha + "/";
    registro::apuntar("copia de seguridad en %s", copia.c_str());
    if (!copiarCarpeta("/", copia) && !copiarConocidos(copia))
        return "No he podido hacer la copia de seguridad en la SD: no toco nada.";
    return "";
}

std::string guardar()
{
    struct Pendiente {
        std::string ruta;
        Archivo cifrado;
        std::vector<uint8_t> plano;
        int jugador; // -1 = main
    };
    std::vector<Pendiente> pendientes;
    if (g_mainCambiado)
    {
        Pendiente p{"/main.dat", g_main, {}, -1};
        p.plano = preparar(p.cifrado);
        pendientes.push_back(std::move(p));
    }
    for (int i = 0; i < (int)jugadores.size(); i++)
        if (jugadores[i].cambiado)
        {
            Pendiente p{jugadores[i].carpeta + "/personal.dat", jugadores[i].a, {}, i};
            p.plano = preparar(p.cifrado);
            pendientes.push_back(std::move(p));
        }
    for (auto& o : g_otros)
        if (o.second.cambiado)
        {
            Pendiente p{o.first, o.second.a, {}, -2}; // -2: another file
            p.plano = preparar(p.cifrado);
            pendientes.push_back(std::move(p));
        }
    for (const auto& p : pendientes)
        if (!escribir(p.ruta.c_str(), p.cifrado.datos))
            return "No he podido escribir. La partida no ha cambiado (sales sin guardar).";
    if (Result rc = fsFsCommit(&g_fs); R_FAILED(rc))
    {
        registro::apuntar("commit MAL (error 0x%x)", rc);
        return "No he podido guardar. La partida no ha cambiado.";
    }
    for (const auto& p : pendientes)
        if (!comprobar(p.ruta, p.cifrado.cabecera, p.plano))
            return "¡Ojo! Lo guardado no coincide. Restaura la copia de la SD.";

    // Ya guardado: lo de memoria queda con las comprobaciones al día.
    for (auto& p : pendientes)
        (p.jugador == -2 ? g_otros[p.ruta].a.datos : p.jugador < 0 ? g_main.datos : jugadores[p.jugador].a.datos) = std::move(p.plano);
    g_mainCambiado = false;
    for (auto& j : jugadores)
        j.cambiado = false;
    for (auto& o : g_otros)
        o.second.cambiado = false;
    return "";
}

void cerrar()
{
    g_otros.clear();
    if (g_abierta)
        fsFsClose(&g_fs);
    g_abierta = false;
}

bool escribirSd(const std::string& ruta, const std::vector<uint8_t>& datos)
{
    mkdirs(ruta);
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = fwrite(datos.data(), 1, datos.size(), f) == datos.size();
    fclose(f);
    return ok;
}

bool leerSd(const std::string& ruta, std::vector<uint8_t>& datos)
{
    FILE* f = fopen(ruta.c_str(), "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    datos.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    const bool ok = fread(datos.data(), 1, datos.size(), f) == datos.size();
    fclose(f);
    return ok;
}

std::vector<std::string> listarSd(const std::string& carpeta, const std::string& extension)
{
    std::vector<std::string> r;
    DIR* d = opendir(carpeta.c_str());
    if (!d)
        return r;
    while (dirent* e = readdir(d))
    {
        const std::string n = e->d_name;
        if (n.size() > extension.size() && n.compare(n.size() - extension.size(), extension.size(), extension) == 0)
            r.push_back(n);
    }
    closedir(d);
    std::sort(r.begin(), r.end());
    return r;
}

} // namespace partida
