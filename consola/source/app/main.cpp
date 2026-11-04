// NHSE para Switch: editor de la partida de Animal Crossing: New Horizons que se abre desde el Álbum (con el juego
// cerrado). Hace una copia de toda la partida en la SD al empezar, y al guardar comprueba lo escrito leyéndolo de vuelta.
// La forma de abrir la partida del dispositivo viene de cbreezy210/ACNH-Save-Editor (MIT).
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <sys/stat.h>
#include <tuple>
#include <vector>

#include "../core/acnh.hpp"
#include "../core/listas.hpp"
#include "../core/objetos.hpp"
#include "../core/tablas.hpp"
#include "../core/vecinos.hpp"

using namespace acnh;

namespace {

constexpr u64 ACNH = 0x01006F8002326000ULL;
constexpr int W = 1280, H = 720;
template <typename T, size_t N> constexpr int cuantos(const T (&)[N]) { return (int)N; }

// ------------------------------------------------------------------------------------------------------- la partida
FsFileSystem g_fs;
bool g_abierta = false;
Archivo g_main;
bool g_mainCambiado = false;
struct Jugador {
    std::string carpeta; // «/Villager0»
    std::string nombre;
    Archivo a;
    bool cambiado = false;
};
std::vector<Jugador> g_jugadores;
int g_yo = 0;
std::string g_isla, g_copia;

Jugador& yo() { return g_jugadores[g_yo]; }
std::vector<uint8_t>& pers() { return yo().a.datos; }
bool hayCambios()
{
    bool c = g_mainCambiado;
    for (const auto& j : g_jugadores)
        c |= j.cambiado;
    return c;
}

bool leer(const char* ruta, std::vector<uint8_t>& out)
{
    FsFile f;
    if (R_FAILED(fsFsOpenFile(&g_fs, ruta, FsOpenMode_Read, &f)))
        return false;
    s64 tam = 0;
    fsFileGetSize(&f, &tam);
    out.resize(tam);
    u64 leidos = 0;
    Result rc = fsFileRead(&f, 0, out.data(), tam, FsReadOption_None, &leidos);
    fsFileClose(&f);
    return R_SUCCEEDED(rc) && (s64)leidos == tam;
}

bool escribir(const char* ruta, const std::vector<uint8_t>& datos)
{
    FsFile f;
    if (R_FAILED(fsFsOpenFile(&g_fs, ruta, FsOpenMode_Write, &f)))
        return false;
    Result rc = fsFileWrite(&f, 0, datos.data(), datos.size(), FsWriteOption_Flush);
    fsFileClose(&f);
    return R_SUCCEEDED(rc);
}

void mkdirs(const std::string& ruta)
{
    for (size_t i = ruta.find('/', 6); i != std::string::npos; i = ruta.find('/', i + 1))
        mkdir(ruta.substr(0, i).c_str(), 0777);
}

// Copia todo lo que hay en una carpeta de la partida a la SD.
bool copiarCarpeta(const std::string& dentro, const std::string& sd)
{
    FsDir dir;
    if (R_FAILED(fsFsOpenDirectory(&g_fs, dentro.c_str(), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir)))
        return false;
    std::vector<FsDirectoryEntry> entradas(64);
    bool ok = true;
    while (ok)
    {
        s64 n = 0;
        if (R_FAILED(fsDirRead(&dir, &n, entradas.size(), entradas.data())) || n == 0)
            break;
        for (s64 i = 0; i < n && ok; i++)
        {
            std::string nombre = entradas[i].name;
            std::string ruta = dentro + (dentro == "/" ? "" : "/") + nombre;
            if (entradas[i].type == FsDirEntryType_Dir)
                ok = copiarCarpeta(ruta, sd + nombre + "/");
            else
            {
                std::vector<uint8_t> datos;
                std::string destino = sd + nombre;
                mkdirs(destino);
                FILE* f = leer(ruta.c_str(), datos) ? fopen(destino.c_str(), "wb") : nullptr;
                ok = f && fwrite(datos.data(), 1, datos.size(), f) == datos.size();
                if (f)
                    fclose(f);
            }
        }
    }
    fsDirClose(&dir);
    return ok;
}

// Abre la partida, la descifra, comprueba que está sana y hace la copia de seguridad. Devuelve el motivo si no puede.
std::string abrirPartida()
{
    FsSaveDataAttribute attr = {};
    attr.application_id = ACNH;
    attr.save_data_type = FsSaveDataType_Device;
    if (R_FAILED(fsOpenSaveDataFileSystem(&g_fs, FsSaveDataSpaceId_User, &attr)))
        return "No encuentro la partida de Animal Crossing. ¿Está el juego cerrado?";
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
        if (!leer((j.carpeta + "/personal.dat").c_str(), j.a.datos) || !leer((j.carpeta + "/personalHeader.dat").c_str(), j.a.cabecera))
            continue;
        if (j.a.datos.size() != TAM_PERSONAL)
            return "Esta partida no es de la versión 3.0 del juego: no la toco.";
        cifrar(j.a.cabecera, j.a.datos);
        if (comprobacionesMal(j.a.datos) != 0)
            return "La partida no está sana (sus comprobaciones no cuadran): no la toco.";
        j.nombre = nombreJugador(j.a.datos);
        g_jugadores.push_back(std::move(j));
    }
    if (g_jugadores.empty())
        return "No he podido leer la partida.";
    g_isla = nombreIsla(g_jugadores[0].a.datos);

    time_t t = time(nullptr);
    char fecha[32];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d %H.%M.%S", localtime(&t));
    g_copia = std::string("sdmc:/switch/NHSE/copias/") + fecha + "/";
    if (!copiarCarpeta("/", g_copia))
        return "No he podido hacer la copia de seguridad en la SD: no toco nada.";
    return "";
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

// Guarda lo que ha cambiado, lo vuelve a leer y comprueba que está sano y es lo que se quería guardar.
std::string guardarPartida()
{
    struct Pendiente { std::string ruta; Archivo cifrado; std::vector<uint8_t> plano; };
    std::vector<Pendiente> pendientes;
    if (g_mainCambiado)
    {
        Pendiente p{"/main.dat", g_main, {}};
        p.plano = preparar(p.cifrado);
        pendientes.push_back(std::move(p));
    }
    for (const auto& j : g_jugadores)
        if (j.cambiado)
        {
            Pendiente p{j.carpeta + "/personal.dat", j.a, {}};
            p.plano = preparar(p.cifrado);
            pendientes.push_back(std::move(p));
        }
    for (const auto& p : pendientes)
        if (!escribir(p.ruta.c_str(), p.cifrado.datos))
            return "No he podido escribir. La partida no ha cambiado (sales sin guardar).";
    if (R_FAILED(fsFsCommit(&g_fs)))
        return "No he podido guardar. La partida no ha cambiado.";
    for (const auto& p : pendientes)
        if (!comprobar(p.ruta, p.cifrado.cabecera, p.plano))
            return "¡Ojo! Lo guardado no coincide. Restaura la copia de la SD.";

    // Ya guardado: lo de memoria queda con las comprobaciones al día.
    for (auto& p : pendientes)
    {
        if (p.ruta == "/main.dat")
            g_main.datos = std::move(p.plano);
        else
            for (auto& j : g_jugadores)
                if (p.ruta == j.carpeta + "/personal.dat")
                    j.a.datos = std::move(p.plano);
    }
    g_mainCambiado = false;
    for (auto& j : g_jugadores)
        j.cambiado = false;
    return "";
}

// ---------------------------------------------------------------------------------------------------------- dibujo
SDL_Renderer* g_r;
TTF_Font* g_fuente;
TTF_Font* g_pequena;
TTF_Font* g_grande;

const SDL_Color CREMA = {0xF7, 0xF2, 0xE4, 255};
const SDL_Color CREMA_OSCURA = {0xEA, 0xE2, 0xCC, 255};
const SDL_Color VERDE = {0x3F, 0x9E, 0x5C, 255};
const SDL_Color VERDE_OSCURO = {0x2C, 0x6E, 0x40, 255};
const SDL_Color MARRON = {0x5A, 0x4A, 0x3A, 255};
const SDL_Color MARRON_SUAVE = {0x8C, 0x7C, 0x68, 255};
const SDL_Color BLANCO = {255, 255, 255, 255};
const SDL_Color GRIS = {0xCF, 0xC8, 0xB6, 255};
const SDL_Color ROJO = {0xC0, 0x45, 0x38, 255};
const SDL_Color AMARILLO = {0xF2, 0xC1, 0x4E, 255};

// Las letras ya dibujadas se guardan: dibujar texto cada vez es lento.
struct Letras {
    SDL_Texture* t;
    int w, h;
};
std::map<std::tuple<std::string, TTF_Font*, uint32_t, int>, Letras> g_letras;

Letras letras(const std::string& s, TTF_Font* f, SDL_Color c, int ancho)
{
    const uint32_t color = (c.r << 24) | (c.g << 16) | (c.b << 8) | c.a;
    auto clave = std::make_tuple(s, f, color, ancho);
    auto it = g_letras.find(clave);
    if (it != g_letras.end())
        return it->second;
    if (g_letras.size() > 600)
    {
        for (auto& l : g_letras)
            SDL_DestroyTexture(l.second.t);
        g_letras.clear();
    }
    SDL_Surface* sf = ancho > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), c, ancho) : TTF_RenderUTF8_Blended(f, s.c_str(), c);
    Letras l{nullptr, 0, 0};
    if (sf)
    {
        l = {SDL_CreateTextureFromSurface(g_r, sf), sf->w, sf->h};
        SDL_FreeSurface(sf);
    }
    g_letras[clave] = l;
    return l;
}

void rect(const SDL_Rect& r, SDL_Color c)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(g_r, &r);
}

// Rectángulo con las esquinas redondeadas.
void redondo(const SDL_Rect& r, int radio, SDL_Color c)
{
    radio = std::min({radio, r.w / 2, r.h / 2});
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_Rect centro = {r.x, r.y + radio, r.w, r.h - 2 * radio};
    SDL_RenderFillRect(g_r, &centro);
    for (int dy = 0; dy < radio; dy++)
    {
        const double d = radio - dy - 0.5;
        const int dx = radio - (int)std::lround(std::sqrt(std::max(0.0, (double)radio * radio - d * d)));
        SDL_RenderDrawLine(g_r, r.x + dx, r.y + dy, r.x + r.w - 1 - dx, r.y + dy);
        SDL_RenderDrawLine(g_r, r.x + dx, r.y + r.h - 1 - dy, r.x + r.w - 1 - dx, r.y + r.h - 1 - dy);
    }
}

enum Alinear { Izquierda, Centro, Derecha };

// Texto en (x, y); centrado en x (y en y) o a la derecha de x. Con «ancho», en varias líneas.
int texto(const std::string& s, int x, int y, SDL_Color c, TTF_Font* f = nullptr, Alinear a = Izquierda, int ancho = 0)
{
    if (s.empty())
        return 0;
    Letras l = letras(s, f ? f : g_fuente, c, ancho);
    if (!l.t)
        return 0;
    SDL_Rect d = {a == Centro ? x - l.w / 2 : a == Derecha ? x - l.w : x, a == Centro ? y - l.h / 2 : y, l.w, l.h};
    SDL_RenderCopy(g_r, l.t, nullptr, &d);
    return l.h;
}

// Recorta un texto con «…» para que quepa en ese ancho.
std::string encajar(std::string s, int ancho, TTF_Font* f = nullptr)
{
    f = f ? f : g_fuente;
    int w = 0;
    if (TTF_SizeUTF8(f, s.c_str(), &w, nullptr) != 0 || w <= ancho)
        return s;
    while (!s.empty())
    {
        // Quita un carácter entero (UTF-8).
        size_t i = s.size() - 1;
        while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
            i--;
        s.erase(i);
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        if (TTF_SizeUTF8(f, (s + "…").c_str(), &w, nullptr) == 0 && w <= ancho)
            break;
    }
    return s + "…";
}

struct Boton {
    SDL_Rect r;
    std::string texto;
    std::function<void()> accion;
    bool activo = true;
    bool marcado = false;
    std::string detalle; // segunda línea, más pequeña
    bool grande = false;  // las tarjetas de la pantalla de inicio
};

void dibujar(const Boton& b, bool foco)
{
    SDL_Color fondo = !b.activo ? GRIS : b.marcado ? VERDE_OSCURO : b.grande ? BLANCO : VERDE;
    SDL_Color letra = !b.activo ? MARRON_SUAVE : b.grande && !b.marcado ? MARRON : BLANCO;
    if (foco)
        redondo({b.r.x - 5, b.r.y - 5, b.r.w + 10, b.r.h + 10}, 18, AMARILLO);
    if (b.grande && b.activo && !b.marcado)
        redondo({b.r.x, b.r.y + 4, b.r.w, b.r.h}, 14, CREMA_OSCURA);
    redondo(b.r, 14, fondo);
    if (b.grande)
    {
        redondo({b.r.x, b.r.y, 10, b.r.h}, 5, b.marcado ? AMARILLO : b.activo ? VERDE : GRIS);
        const int x = b.r.x + 28;
        if (b.detalle.empty())
            texto(encajar(b.texto, b.r.w - 40), x, b.r.y + b.r.h / 2 - 17, letra);
        else
        {
            texto(encajar(b.texto, b.r.w - 40), x, b.r.y + b.r.h / 2 - 34, letra);
            texto(encajar(b.detalle, b.r.w - 40, g_pequena), x, b.r.y + b.r.h / 2 + 4, b.marcado ? CREMA : MARRON_SUAVE, g_pequena);
        }
        return;
    }
    const int margen = std::min(24, b.r.w / 8);
    if (b.detalle.empty())
        texto(encajar(b.texto, b.r.w - margen), b.r.x + b.r.w / 2, b.r.y + b.r.h / 2, letra, nullptr, Centro);
    else
    {
        texto(encajar(b.texto, b.r.w - margen), b.r.x + b.r.w / 2, b.r.y + b.r.h / 2 - 14, letra, nullptr, Centro);
        texto(encajar(b.detalle, b.r.w - margen, g_pequena), b.r.x + b.r.w / 2, b.r.y + b.r.h / 2 + 18, letra, g_pequena, Centro);
    }
}

// La barra de abajo: un aviso a la izquierda y la ayuda de los botones a la derecha («(A) Elegir (B) Volver»).
void barra(const std::string& aviso, bool malo, bool suave, const std::vector<std::pair<std::string, std::string>>& teclas)
{
    rect({0, H - 48, W, 48}, malo ? ROJO : !aviso.empty() && !suave ? VERDE_OSCURO : CREMA_OSCURA);
    int x = W - 24;
    const SDL_Color letra = malo || (!aviso.empty() && !suave) ? BLANCO : MARRON;
    for (auto it = teclas.rbegin(); it != teclas.rend(); ++it)
    {
        Letras l = letras(it->second, g_pequena, letra, 0);
        x -= l.w;
        texto(it->second, x, H - 24 - l.h / 2, letra, g_pequena);
        Letras k = letras(it->first, g_pequena, malo ? ROJO : MARRON, 0);
        const int ancho = std::max(30, k.w + 16);
        x -= 8 + ancho;
        redondo({x, H - 39, ancho, 30}, 15, letra);
        texto(it->first, x + ancho / 2, H - 24, malo || (!aviso.empty() && !suave) ? (malo ? ROJO : VERDE_OSCURO) : CREMA, g_pequena, Centro);
        x -= 26;
    }
    if (!aviso.empty())
        texto(encajar(aviso, x - 40, g_pequena), 24, H - 24 - letras("Ag", g_pequena, letra, 0).h / 2, letra, g_pequena);
}

// ------------------------------------------------------------------------------------------------------- utilidades
std::string nombreVecino(const std::string& interno)
{
    for (const auto& v : LISTA_VECINOS)
        if (interno == v.interno)
            return v.nombre;
    return interno.empty() ? "(libre)" : interno;
}

// La letra por la que empieza un nombre, sin tildes (para buscar por letra).
char inicial(const char* nombre)
{
    const std::string n = normalizar(std::string(nombre, strnlen(nombre, 4)));
    return (char)toupper((unsigned char)n[0]);
}

std::vector<uint8_t> leerRomfs(const char* ruta)
{
    std::vector<uint8_t> datos;
    FILE* f = fopen(ruta, "rb");
    if (!f)
        return datos;
    fseek(f, 0, SEEK_END);
    datos.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(datos.data(), 1, datos.size(), f) != datos.size())
        datos.clear();
    fclose(f);
    return datos;
}

std::string numero(uint32_t n)
{
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert(i, ".");
    return s;
}

// Pide un número con el teclado de la consola.
bool pedirNumero(const char* titulo, uint32_t& valor, uint32_t maximo)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetStringLenMax(&kbd, 10);
    swkbdConfigSetHeaderText(&kbd, titulo);
    char inicial[16];
    snprintf(inicial, sizeof(inicial), "%u", valor);
    swkbdConfigSetInitialText(&kbd, inicial);
    char out[16] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc) || !out[0])
        return false;
    unsigned long v = strtoul(out, nullptr, 10);
    valor = (uint32_t)std::min<unsigned long>(v, maximo);
    return true;
}

// Pide un texto con el teclado de la consola.
bool pedirTexto(const char* titulo, const char* pista, std::string& valor)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, titulo);
    swkbdConfigSetGuideText(&kbd, pista);
    swkbdConfigSetStringLenMax(&kbd, 40);
    swkbdConfigSetInitialText(&kbd, valor.c_str());
    char out[256] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc) || !out[0])
        return false;
    valor = out;
    return true;
}

// Cómo se ve un objeto de los bolsillos: «pepita de hierro ×30».
std::string nombreObjeto(const Objeto& o)
{
    if (o.vacio())
        return "";
    const int k = objetoDe(o);
    if (k < 0)
        return "Objeto nº " + std::to_string(o.id);
    std::string s = OBJETOS[k].nombre;
    if (OBJETOS[k].pila > 1)
        s += " ×" + std::to_string(o.cuenta + 1);
    return s;
}

const char* const DIAS[] = {"Domingo", "Domingo", "Lunes", "Lunes", "Martes", "Martes", "Miércoles", "Miércoles",
    "Jueves", "Jueves", "Viernes", "Viernes", "Sábado", "Sábado"};

} // namespace

// ------------------------------------------------------------------------------------------------------------ main
int main(int, char**)
{
    romfsInit();
    plInitialize(PlServiceType_User);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("NHSE", 0, 0, W, H, 0);
    g_r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    PlFontData font;
    plGetSharedFontByType(&font, PlSharedFontType_Standard);
    g_fuente = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 28);
    g_pequena = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 22);
    g_grande = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 40);

    // Mientras se hace la copia de seguridad (unos segundos), un aviso.
    rect({0, 0, W, H}, CREMA);
    texto("Haciendo una copia de la partida en la SD…", W / 2, H / 2, MARRON, g_grande, Centro);
    SDL_RenderPresent(g_r);

    const std::string error = abrirPartida();
    const std::vector<uint8_t> vecinosBin = leerRomfs("romfs:/vecinos.bin");
    std::string aviso;
    bool avisoMalo = false;
    auto avisar = [&](const std::string& s, bool malo = false) { aviso = s, avisoMalo = malo; };

    enum Pantalla { Inicio, Dinero, Bolsillos, Hueco, Buscar, Coleccion, Flores, Vecinos, ElegirVecino, PreciosNabos, Jugadores, Salir };
    Pantalla pantalla = Inicio;
    int foco = 0;
    bool salir = false;
    auto ir = [&](Pantalla p, int f = 0) { pantalla = p, foco = f, aviso.clear(); };

    int pagina = 0;            // the page of the screen with pages
    int hueco = 0;             // the pocket slot or villager being changed
    std::string busqueda;      // what was searched
    std::vector<int> hallados; // and what was found
    char letra = 'A';
    std::vector<uint16_t> elegidas;
    bool alAzar = false;

    // Hold a direction to move quickly.
    u64 repiteTecla = 0;
    u32 repiteDesde = 0;
    bool touchAntes = false;

    while (!salir && appletMainLoop())
    {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        const u64 held = padGetButtons(&pad);
        const u64 flechas = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
        if (down & flechas)
            repiteTecla = down & flechas, repiteDesde = SDL_GetTicks() + 350;
        else if ((held & repiteTecla) && SDL_GetTicks() >= repiteDesde)
            down |= repiteTecla, repiteDesde = SDL_GetTicks() + 90;
        HidTouchScreenState ts = {};
        hidGetTouchScreenStates(&ts, 1);
        const bool toque = ts.count > 0 && !touchAntes;
        touchAntes = ts.count > 0;
        const int tx = toque ? ts.touches[0].x : -1, ty = toque ? ts.touches[0].y : -1;

        // ------------------------------------------------------------------------------- the buttons of this screen
        std::vector<Boton> b;
        std::string titulo, explicacion;
        std::function<void()> atras;
        std::function<void(int)> paginar; // L / R
        std::vector<std::pair<std::string, std::string>> teclas = {{"A", "Elegir"}, {"B", "Volver"}};

        if (!error.empty())
        {
            titulo = "No se puede abrir la partida";
            b.push_back({{490, 560, 300, 80}, "Salir", [&] { salir = true; }});
            atras = [&] { salir = true; };
            teclas = {{"A", "Salir"}};
        }
        else if (pantalla == Inicio)
        {
            titulo = g_isla + " · " + yo().nombre;
            explicacion = "Elige qué quieres cambiar. Nada se guarda en la partida hasta que pulses «Guardar».";
            const Recuento rec = contar(g_main.datos);
            const auto din = leerNumero(pers().data() + MONEDERO);
            const int libres = [&] {
                int n = 0;
                for (int i = 0; i < NUM_BOLSILLOS; i++)
                    n += bolsilloAbierto(pers(), i) && leerBolsillo(pers(), i).vacio();
                return n;
            }();
            const int x[3] = {50, 440, 830};
            const int y[3] = {150, 275, 400};
            const int cw = 380, ch = 105;
            auto tarjeta = [&](int c, int f, const std::string& t, const std::string& d, std::function<void()> a, bool activo = true) {
                Boton bt{{x[c], y[f], cw, ch}, t, std::move(a), activo};
                bt.detalle = d;
                bt.grande = true;
                b.push_back(bt);
            };
            tarjeta(0, 0, "Dinero y millas", numero(din) + " bayas en el monedero", [&] { ir(Dinero); });
            tarjeta(1, 0, "Bolsillos", libres ? std::to_string(libres) + " huecos libres" : "Llenos", [&] { pagina = 0, ir(Bolsillos); });
            tarjeta(2, 0, "Recetas y catálogo", "Aprender todo de golpe", [&] { ir(Coleccion); });
            tarjeta(0, 1, "Flores", std::to_string(rec.flores) + " en la isla", [&] { pagina = 0, ir(Flores); }, rec.flores > 0);
            tarjeta(1, 1, "Quitar malas hierbas", rec.malasHierbas ? std::to_string(rec.malasHierbas) + " en la isla" : "No hay ninguna", [&] {
                const int n = quitarMalasHierbas(g_main.datos);
                g_mainCambiado = true;
                avisar("Quitadas " + std::to_string(n) + " malas hierbas. Falta guardar.");
            }, rec.malasHierbas > 0);
            tarjeta(2, 1, "Vecinos", "Cambiar quién vive en la isla", [&] { ir(Vecinos); }, !vecinosBin.empty());
            const Nabos nb = leerNabos(g_main.datos);
            tarjeta(0, 2, "Nabos", nb.compra ? "Se compran a " + std::to_string(nb.compra) + " bayas" : "Precios de la semana", [&] { ir(PreciosNabos); });
            tarjeta(1, 2, "Jugador", g_jugadores.size() > 1 ? "Cambiar de jugador (" + std::to_string(g_jugadores.size()) + ")" : "Solo hay un jugador",
                [&] { ir(Jugadores, g_yo); }, g_jugadores.size() > 1);
            Boton guardar{{830, 400, cw, ch}, "Guardar", [&] {
                const std::string mal = guardarPartida();
                avisar(mal.empty() ? "Guardado y comprobado. Ya puedes abrir el juego." : mal, !mal.empty());
            }, hayCambios()};
            guardar.detalle = hayCambios() ? "Escribir los cambios en la partida" : "No hay cambios";
            guardar.grande = true;
            guardar.marcado = hayCambios();
            b.push_back(guardar);
            b.push_back({{440, 560, 400, 64}, hayCambios() ? "Salir sin guardar" : "Salir", [&] {
                if (hayCambios())
                    ir(Salir, 1);
                else
                    salir = true;
            }});
            atras = b.back().accion;
            teclas = {{"A", "Elegir"}, {"Y", "Guardar"}, {"+", "Salir"}};
            if ((down & HidNpadButton_Y) && hayCambios())
                guardar.accion();
        }
        else if (pantalla == Dinero)
        {
            titulo = "Dinero y millas de " + yo().nombre;
            explicacion = "Toca un número para cambiarlo.";
            const uint32_t pos[3] = {MONEDERO, BANCO, MILLAS};
            const char* nombres[3] = {"Monedero", "Banco", "Millas Nook"};
            const char* titulos[3] = {"Bayas en el monedero (máx. 99.999)", "Bayas en el banco (máx. 999.999.999)", "Millas Nook"};
            const uint32_t maximos[3] = {99999, 999999999, 999999999};
            for (int i = 0; i < 3; i++)
                b.push_back({{100, 170 + i * 120, 1080, 100}, std::string(nombres[i]) + ": " + numero(leerNumero(pers().data() + pos[i])) + (i < 2 ? " bayas" : ""), [&, i] {
                    uint32_t v = leerNumero(pers().data() + pos[i]);
                    if (pedirNumero(titulos[i], v, maximos[i]))
                    {
                        escribirNumero(pers().data() + pos[i], v);
                        yo().cambiado = true;
                        avisar("Cambiado. Falta guardar.");
                    }
                }});
            b.push_back({{440, 540, 400, 70}, "Volver", [&] { ir(Inicio, 0); }});
            atras = b.back().accion;
        }
        else if (pantalla == Bolsillos)
        {
            titulo = "Bolsillos de " + yo().nombre;
            explicacion = "Elige un hueco para poner otro objeto o cambiar cuántos hay.";
            for (int k = 0; k < 20; k++)
            {
                const int i = pagina * 20 + k;
                const bool abierto = bolsilloAbierto(pers(), i);
                const Objeto o = leerBolsillo(pers(), i);
                std::string t = std::to_string(i + 1) + ". " + (!abierto ? "(sin ampliar)" : o.vacio() ? "—" : nombreObjeto(o));
                Boton bt{{60 + (k / 10) * 590, 140 + (k % 10) * 46, 570, 40}, t, [&, i] { hueco = i, ir(Hueco, 0); }, abierto};
                bt.marcado = abierto && !o.vacio();
                b.push_back(bt);
            }
            b.push_back({{60, 604, 280, 56}, "Huecos 1-20", [&] { pagina = 0; }, true, pagina == 0});
            b.push_back({{360, 604, 280, 56}, "Huecos 21-40", [&] { pagina = 1; }, true, pagina == 1});
            b.push_back({{940, 604, 280, 56}, "Volver", [&] { ir(Inicio, 1); }});
            atras = b.back().accion;
            paginar = [&](int d) { pagina = std::clamp(pagina + d, 0, 1); };
            teclas = {{"A", "Elegir"}, {"L R", "Otra página"}, {"B", "Volver"}};
        }
        else if (pantalla == Hueco)
        {
            const Objeto o = leerBolsillo(pers(), hueco);
            const int k = objetoDe(o);
            titulo = "Hueco " + std::to_string(hueco + 1);
            explicacion = o.vacio() ? "Está vacío." : "Ahora: " + nombreObjeto(o);
            auto buscar = [&] {
                std::string q = busqueda;
                if (!pedirTexto("Buscar objeto", "Escribe parte del nombre (por ejemplo, «pepita» o «receta escalera»)", q))
                    return;
                busqueda = q;
                hallados = buscarObjetos(q);
                pagina = 0;
                ir(Buscar);
                if (hallados.empty())
                    avisar("No hay ningún objeto que se llame así.", true);
            };
            b.push_back({{100, 200, 1080, 90}, o.vacio() ? "Poner un objeto…" : "Cambiar por otro objeto…", buscar});
            const bool apila = k >= 0 && OBJETOS[k].pila > 1;
            b.push_back({{100, 310, 530, 90}, apila ? "Cantidad: " + std::to_string(o.cuenta + 1) : "Cantidad: 1", [&, k] {
                uint32_t v = leerBolsillo(pers(), hueco).cuenta + 1;
                char t[64];
                snprintf(t, sizeof(t), "¿Cuántos? (de 1 a %d)", OBJETOS[k].pila);
                if (!pedirNumero(t, v, OBJETOS[k].pila))
                    return;
                Objeto n = leerBolsillo(pers(), hueco);
                n.cuenta = (uint16_t)(std::max<uint32_t>(v, 1) - 1);
                escribirBolsillo(pers(), hueco, n);
                yo().cambiado = true;
                avisar("Cambiado. Falta guardar.");
            }, apila});
            b.push_back({{650, 310, 530, 90}, apila ? "Máximo (" + std::to_string(OBJETOS[k].pila) + ")" : "Máximo", [&, k] {
                Objeto n = leerBolsillo(pers(), hueco);
                n.cuenta = OBJETOS[k].pila - 1;
                escribirBolsillo(pers(), hueco, n);
                yo().cambiado = true;
                avisar("Cambiado. Falta guardar.");
            }, apila && o.cuenta + 1 < OBJETOS[k].pila});
            b.push_back({{100, 420, 1080, 90}, "Vaciar el hueco", [&] {
                escribirBolsillo(pers(), hueco, Objeto{});
                yo().cambiado = true;
                avisar("Vaciado. Falta guardar.");
            }, !o.vacio()});
            b.push_back({{440, 540, 400, 70}, "Volver", [&] { pagina = hueco / 20, ir(Bolsillos, hueco % 20); }});
            atras = b.back().accion;
            teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"B", "Volver"}};
            if (down & HidNpadButton_X)
                buscar();
        }
        else if (pantalla == Buscar)
        {
            const int porPagina = 16, paginas = std::max<int>(1, (hallados.size() + porPagina - 1) / porPagina);
            pagina = std::min(pagina, paginas - 1);
            titulo = "«" + busqueda + "»: " + (hallados.size() == 1 ? "1 objeto" : std::to_string(hallados.size()) + " objetos");
            explicacion = "Elige el objeto para el hueco " + std::to_string(hueco + 1) + (paginas > 1 ? " (página " + std::to_string(pagina + 1) + " de " + std::to_string(paginas) + ")" : "") + ".";
            for (int k = 0; k < porPagina && pagina * porPagina + k < (int)hallados.size(); k++)
            {
                const int i = hallados[pagina * porPagina + k];
                std::string t = OBJETOS[i].nombre;
                if (OBJETOS[i].pila > 1)
                    t += " (hasta " + std::to_string(OBJETOS[i].pila) + ")";
                b.push_back({{60 + (k / 8) * 590, 140 + (k % 8) * 56, 570, 48}, t, [&, i] {
                    Objeto n;
                    n.id = OBJETOS[i].id;
                    n.cuenta = OBJETOS[i].pila > 1 ? OBJETOS[i].pila - 1 : OBJETOS[i].cuenta;
                    escribirBolsillo(pers(), hueco, n);
                    yo().cambiado = true;
                    ir(Hueco, OBJETOS[i].pila > 1 ? 1 : 0);
                    avisar("Puesto: " + nombreObjeto(n) + ". Falta guardar.");
                }});
            }
            b.push_back({{60, 604, 260, 56}, "Anteriores", [&] { pagina--; }, pagina > 0});
            b.push_back({{340, 604, 260, 56}, "Siguientes", [&] { pagina++; }, pagina + 1 < paginas});
            b.push_back({{620, 604, 300, 56}, "Buscar otra cosa", [&] { ir(Hueco, 0); }});
            b.push_back({{940, 604, 280, 56}, "Volver", [&] { ir(Hueco, 0); }});
            atras = b.back().accion;
            paginar = [&](int d) { pagina = std::clamp(pagina + d, 0, paginas - 1); };
            teclas = {{"A", "Elegir"}, {"L R", "Otra página"}, {"B", "Volver"}};
            // «Buscar otra cosa» opens the keyboard at once.
            b[b.size() - 2].accion = [&] {
                ir(Hueco, 0);
                std::string q = busqueda;
                if (pedirTexto("Buscar objeto", "Escribe parte del nombre", q))
                {
                    busqueda = q, hallados = buscarObjetos(q), pagina = 0;
                    ir(Buscar);
                    if (hallados.empty())
                        avisar("No hay ningún objeto que se llame así.", true);
                }
            };
        }
        else if (pantalla == Coleccion)
        {
            titulo = "Recetas, catálogo y reacciones de " + yo().nombre;
            explicacion = "Lo que ya tiene se queda como está; lo nuevo sale como nuevo en el juego.";
            const Progreso r = recetas(pers()), c = catalogo(pers()), e = reacciones(pers());
            auto fila = [&](int i, const std::string& t, Progreso p, std::function<int()> hacer, const char* cosa) {
                Boton bt{{100, 170 + i * 120, 1080, 100}, t, [&, hacer, cosa] {
                    const int n = hacer();
                    yo().cambiado = true;
                    avisar(std::to_string(n) + " " + cosa + " nuevas. Falta guardar.");
                }, p.tiene < p.total};
                bt.detalle = p.tiene < p.total ? "Tiene " + numero(p.tiene) + " de " + numero(p.total) : "Ya lo tiene todo (" + numero(p.total) + ")";
                b.push_back(bt);
            };
            fila(0, "Aprender todas las recetas de bricolaje", r, [&] { return aprenderRecetas(pers()); }, "recetas");
            fila(1, "Completar el catálogo (con todos los colores)", c, [&] { return completarCatalogo(pers()); }, "cosas");
            fila(2, "Aprender todas las reacciones", e, [&] { return aprenderReacciones(pers()); }, "reacciones");
            b.push_back({{440, 540, 400, 70}, "Volver", [&] { ir(Inicio, 2); }});
            atras = b.back().accion;
        }
        else if (pantalla == Flores)
        {
            const int total = cuantos(FLORES_ABIERTAS), paginas = (total + 11) / 12;
            pagina = std::min(pagina, paginas - 1);
            const Recuento rec = contar(g_main.datos);
            titulo = "Flores";
            explicacion = "Elige una o varias: todas las flores de la isla (" + std::to_string(rec.flores) + ") se cambian por esas.";
            for (int i = 0; i < 12 && pagina * 12 + i < total; i++)
            {
                const auto& f = FLORES_ABIERTAS[pagina * 12 + i];
                const bool marcada = std::find(elegidas.begin(), elegidas.end(), f.id) != elegidas.end();
                b.push_back({{60 + (i / 6) * 590, 140 + (i % 6) * 70, 570, 60}, std::string(marcada ? "● " : "") + f.nombre, [&, id = f.id] {
                    auto it = std::find(elegidas.begin(), elegidas.end(), id);
                    if (it == elegidas.end())
                        elegidas.push_back(id);
                    else
                        elegidas.erase(it);
                }, true, marcada});
            }
            b.push_back({{60, 580, 170, 64}, "Anteriores", [&] { pagina--; }, pagina > 0});
            b.push_back({{245, 580, 170, 64}, "Siguientes", [&] { pagina++; }, pagina + 1 < paginas});
            b.push_back({{430, 580, 270, 64}, alAzar ? "Mezcla: al azar" : "Mezcla: alternando", [&] { alAzar = !alAzar; }});
            b.push_back({{715, 580, 270, 64}, elegidas.empty() ? "Cambiar" : "Cambiar (" + std::to_string(elegidas.size()) + ")", [&] {
                const int n = cambiarFlores(g_main.datos, elegidas, alAzar, (uint32_t)time(nullptr));
                g_mainCambiado = true;
                ir(Inicio, 8);
                avisar("Cambiadas " + std::to_string(n) + " flores. Falta guardar.");
            }, !elegidas.empty()});
            b.push_back({{1000, 580, 220, 64}, "Volver", [&] { ir(Inicio, 3); }});
            atras = b.back().accion;
            paginar = [&](int d) { pagina = std::clamp(pagina + d, 0, paginas - 1); };
            teclas = {{"A", "Marcar"}, {"L R", "Otra página"}, {"B", "Volver"}};
        }
        else if (pantalla == Vecinos)
        {
            titulo = "Vecinos";
            explicacion = "Elige el vecino que se va: el nuevo vivirá en su casa.";
            for (int i = 0; i < NUM_VECINOS; i++)
            {
                const std::string v = vecinoEn(g_main.datos, i);
                b.push_back({{60 + (i / 5) * 590, 140 + (i % 5) * 76, 570, 66}, nombreVecino(v), [&, i] { hueco = i, letra = 'A', pagina = 0, ir(ElegirVecino); }, !v.empty()});
            }
            b.push_back({{60, 540, 570, 70}, "Mejores amigos de todos", [&] {
                const int n = mejoresAmigos(g_main.datos);
                if (n > 0)
                    g_mainCambiado = true;
                avisar(n > 0 ? "Amistad al máximo con todos (" + std::to_string(n) + "). Falta guardar." : "Ya eran mejores amigos de todos.");
            }});
            b.push_back({{650, 540, 570, 70}, "Volver", [&] { ir(Inicio, 5); }});
            atras = b.back().accion;
        }
        else if (pantalla == ElegirVecino)
        {
            std::vector<int> deLetra;
            for (int i = 0; i < cuantos(LISTA_VECINOS); i++)
                if (inicial(LISTA_VECINOS[i].nombre) == letra)
                    deLetra.push_back(i);
            const int paginas = std::max<int>(1, (deLetra.size() + 17) / 18);
            pagina = std::min(pagina, paginas - 1);
            titulo = "¿Quién viene en lugar de " + nombreVecino(vecinoEn(g_main.datos, hueco)) + "?";
            explicacion = "Elige la letra abajo (o con L y R).";
            for (int k = 0; k < 18 && pagina * 18 + k < (int)deLetra.size(); k++)
            {
                const auto& v = LISTA_VECINOS[deLetra[pagina * 18 + k]];
                bool yaEsta = false;
                for (int j = 0; j < NUM_VECINOS; j++)
                    yaEsta |= vecinoEn(g_main.datos, j) == v.interno;
                b.push_back({{60 + (k / 6) * 390, 140 + (k % 6) * 62, 370, 54}, v.nombre, [&, interno = std::string(v.interno), nombre = std::string(v.nombre)] {
                    const std::string antes = nombreVecino(vecinoEn(g_main.datos, hueco));
                    const std::string mal = cambiarVecino(g_main.datos, hueco, interno, vecinosBin);
                    if (mal.empty())
                        g_mainCambiado = true;
                    ir(Vecinos, hueco);
                    avisar(mal.empty() ? antes + " se va y llega " + nombre + ". Falta guardar." : mal, !mal.empty());
                }, !yaEsta});
            }
            for (int l = 0; l < 26; l++)
                b.push_back({{40 + l * 46, 525, 42, 50}, std::string(1, (char)('A' + l)), [&, l] { letra = (char)('A' + l), pagina = 0, foco = -1 - l; }, true, 'A' + l == letra});
            if (foco < 0) // a letter was just chosen: the focus stays on it
                foco = (int)b.size() - 26 + (-1 - foco);
            b.push_back({{60, 600, 260, 60}, "Anteriores", [&] { pagina--; }, pagina > 0});
            b.push_back({{340, 600, 260, 60}, "Siguientes", [&] { pagina++; }, pagina + 1 < paginas});
            b.push_back({{960, 600, 260, 60}, "Volver", [&] { ir(Vecinos, hueco); }});
            atras = b.back().accion;
            paginar = [&](int d) { letra = (char)('A' + (letra - 'A' + 26 + d) % 26), pagina = 0; };
            teclas = {{"A", "Elegir"}, {"L R", "Otra letra"}, {"B", "Volver"}};
        }
        else if (pantalla == PreciosNabos)
        {
            titulo = "Nabos";
            explicacion = "Precios de esta semana. Toca uno para cambiarlo.";
            const Nabos n = leerNabos(g_main.datos);
            auto cambiar = [&](int i) {
                Nabos m = leerNabos(g_main.datos);
                uint32_t v = i < 0 ? m.compra : m.venta[i];
                char t[64];
                if (i < 0)
                    snprintf(t, sizeof(t), "Precio de compra (domingo)");
                else
                    snprintf(t, sizeof(t), "%s por la %s", DIAS[i], i % 2 ? "tarde" : "mañana");
                if (!pedirNumero(t, v, 999))
                    return;
                (i < 0 ? m.compra : m.venta[i]) = v;
                escribirNabos(g_main.datos, m);
                g_mainCambiado = true;
                avisar("Cambiado. Falta guardar.");
            };
            b.push_back({{60, 145, 1160, 50}, "Daisy Mae los vende a " + std::to_string(n.compra) + " bayas", [&] { cambiar(-1); }});
            for (int i = 2; i < 14; i++)
                b.push_back({{60 + (i % 2) * 590, 208 + (i / 2 - 1) * 58, 570, 50},
                    std::string(DIAS[i]) + (i % 2 ? " tarde: " : " mañana: ") + std::to_string(n.venta[i]), [&, i] { cambiar(i); }});
            b.push_back({{60, 570, 570, 56}, "Poner todos a…", [&] {
                uint32_t v = 600;
                if (!pedirNumero("Precio de venta para toda la semana", v, 999))
                    return;
                Nabos m = leerNabos(g_main.datos);
                for (int i = 2; i < 14; i++)
                    m.venta[i] = v;
                escribirNabos(g_main.datos, m);
                g_mainCambiado = true;
                avisar("Toda la semana a " + std::to_string(v) + ". Falta guardar.");
            }});
            b.push_back({{650, 570, 570, 56}, "Volver", [&] { ir(Inicio, 6); }});
            atras = b.back().accion;
        }
        else if (pantalla == Jugadores)
        {
            titulo = "Jugadores de " + g_isla;
            explicacion = "Elige a quién cambiar el dinero, los bolsillos y las recetas.";
            for (int i = 0; i < (int)g_jugadores.size(); i++)
                b.push_back({{60 + (i / 4) * 590, 150 + (i % 4) * 90, 570, 76}, g_jugadores[i].nombre, [&, i] {
                    g_yo = i;
                    ir(Inicio, 7);
                    avisar("Ahora cambias a " + g_jugadores[i].nombre + ".");
                }, true, i == g_yo});
            b.push_back({{440, 540, 400, 70}, "Volver", [&] { ir(Inicio, 7); }});
            atras = b.back().accion;
        }
        else if (pantalla == Salir)
        {
            titulo = "¿Salir sin guardar?";
            explicacion = "Hay cambios sin guardar. Si sales ahora, la partida se queda como estaba.";
            b.push_back({{140, 330, 480, 100}, "Salir sin guardar", [&] { salir = true; }});
            b.push_back({{660, 330, 480, 100}, "Volver", [&] { ir(Inicio, 9); }});
            atras = b.back().accion;
        }

        // ------------------------------------------------------------------------------------ choose and move
        if (foco >= (int)b.size() || foco < 0)
            foco = 0;
        auto mover = [&](int dx, int dy) {
            if (b.empty())
                return;
            const SDL_Rect& a = b[foco].r;
            const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
            int mejor = -1;
            long mejorPeso = 0;
            for (int i = 0; i < (int)b.size(); i++)
            {
                if (i == foco)
                    continue;
                const SDL_Rect& r = b[i].r;
                // How far in that direction, and how far across (less if they overlap across).
                const int bx = r.x + r.w / 2, by = r.y + r.h / 2;
                const long lejos = dx ? (long)(bx - ax) * dx : (long)(by - ay) * dy;
                if (lejos <= 4)
                    continue;
                long cruce = dx ? std::abs(by - ay) : std::abs(bx - ax);
                const bool solapan = dx ? (r.y < a.y + a.h && a.y < r.y + r.h) : (r.x < a.x + a.w && a.x < r.x + r.w);
                if (solapan)
                    cruce /= 4;
                const long peso = lejos + 3 * cruce;
                if (mejor < 0 || peso < mejorPeso)
                    mejor = i, mejorPeso = peso;
            }
            if (mejor >= 0)
                foco = mejor;
        };
        if (down & HidNpadButton_AnyUp)
            mover(0, -1);
        else if (down & HidNpadButton_AnyDown)
            mover(0, 1);
        else if (down & HidNpadButton_AnyLeft)
            mover(-1, 0);
        else if (down & HidNpadButton_AnyRight)
            mover(1, 0);

        int pulsado = -1;
        if (down & HidNpadButton_A)
            pulsado = foco;
        for (size_t i = 0; i < b.size(); i++)
            if (tx >= b[i].r.x && tx < b[i].r.x + b[i].r.w && ty >= b[i].r.y && ty < b[i].r.y + b[i].r.h)
                pulsado = foco = (int)i;
        const Pantalla antes = pantalla;
        if (pulsado >= 0 && pulsado < (int)b.size() && b[pulsado].activo && b[pulsado].accion)
        {
            aviso.clear();
            b[pulsado].accion();
        }
        else if ((down & HidNpadButton_B) && atras)
            atras();
        else if ((down & HidNpadButton_Plus) && (pantalla == Inicio || !error.empty()))
            atras();
        else if (paginar && (down & (HidNpadButton_L | HidNpadButton_R | HidNpadButton_ZL | HidNpadButton_ZR)))
            paginar(down & (HidNpadButton_L | HidNpadButton_ZL) ? -1 : 1);
        if (pantalla != antes || salir)
            continue; // the next frame builds the new screen

        // -------------------------------------------------------------------------------------------- draw
        rect({0, 0, W, H}, CREMA);
        rect({0, 0, W, 96}, VERDE);
        texto("NHSE", 40, 22, BLANCO, g_grande);
        texto(encajar(titulo, 900), 1240, 29, BLANCO, nullptr, Derecha);
        if (!error.empty())
            texto(error, 100, 200, ROJO, g_grande, Izquierda, 1080);
        else
        {
            texto(explicacion, 60, 104, MARRON, g_pequena, Izquierda, 1160);
        }
        for (size_t i = 0; i < b.size(); i++)
            dibujar(b[i], (int)i == foco);
        if (!aviso.empty())
            barra(aviso, avisoMalo, false, teclas);
        else if (pantalla == Inicio && error.empty())
            barra(hayCambios() ? "Hay cambios sin guardar." : "Copia de seguridad en " + g_copia.substr(5), false, true, teclas);
        else
            barra("", false, true, teclas);
        SDL_RenderPresent(g_r);
    }

    if (g_abierta)
        fsFsClose(&g_fs);
    for (auto& l : g_letras)
        SDL_DestroyTexture(l.second.t);
    TTF_CloseFont(g_fuente);
    TTF_CloseFont(g_pequena);
    TTF_CloseFont(g_grande);
    TTF_Quit();
    SDL_DestroyRenderer(g_r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    plExit();
    romfsExit();
    return 0;
}
