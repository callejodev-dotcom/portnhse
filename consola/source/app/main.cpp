// NHSE para Switch: editor de la partida de Animal Crossing: New Horizons que se abre desde el Álbum (con el juego
// cerrado). Hace una copia de toda la partida en la SD al empezar, y al guardar comprueba lo escrito leyéndolo de vuelta.
// La forma de abrir la partida del dispositivo viene de cbreezy210/ACNH-Save-Editor (MIT).
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "../core/acnh.hpp"
#include "../core/tablas.hpp"

using namespace acnh;

namespace {

constexpr u64 ACNH = 0x01006F8002326000ULL;
constexpr int W = 1280, H = 720;

// ------------------------------------------------------------------------------------------------------- la partida
FsFileSystem g_fs;
bool g_abierta = false;
Archivo g_main, g_pers;
std::string g_isla, g_jugador, g_copia;

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

std::string utf16(const uint8_t* p, int max)
{
    std::string s;
    for (int i = 0; i < max; i++)
    {
        uint16_t c = p[i * 2] | (p[i * 2 + 1] << 8);
        if (c == 0)
            break;
        if (c < 0x80)
            s += (char)c;
        else if (c < 0x800)
            s += (char)(0xC0 | (c >> 6)), s += (char)(0x80 | (c & 0x3F));
        else
            s += (char)(0xE0 | (c >> 12)), s += (char)(0x80 | ((c >> 6) & 0x3F)), s += (char)(0x80 | (c & 0x3F));
    }
    return s;
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
    if (!leer("/main.dat", g_main.datos) || !leer("/mainHeader.dat", g_main.cabecera) ||
        !leer("/Villager0/personal.dat", g_pers.datos) || !leer("/Villager0/personalHeader.dat", g_pers.cabecera))
        return "No he podido leer la partida.";
    if (g_main.datos.size() != TAM_MAIN || g_pers.datos.size() != TAM_PERSONAL)
        return "Esta partida no es de la versión 3.0 del juego: no la toco.";
    cifrar(g_main.cabecera, g_main.datos);
    cifrar(g_pers.cabecera, g_pers.datos);
    if (comprobacionesMal(g_main.datos) != 0 || comprobacionesMal(g_pers.datos) != 0)
        return "La partida no está sana (sus comprobaciones no cuadran): no la toco.";
    g_isla = utf16(g_pers.datos.data() + 0xC248 + 4, 10);
    g_jugador = utf16(g_pers.datos.data() + 0xC248 + 0x20, 10);

    time_t t = time(nullptr);
    char fecha[32];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d %H.%M.%S", localtime(&t));
    g_copia = std::string("sdmc:/switch/NHSE/copias/") + fecha + "/";
    if (!copiarCarpeta("/", g_copia))
        return "No he podido hacer la copia de seguridad en la SD: no toco nada.";
    return "";
}

// Guarda los dos archivos, los vuelve a leer y comprueba que están sanos y son lo que se quería guardar.
std::string guardarPartida()
{
    Archivo m = g_main, p = g_pers;
    arreglarComprobaciones(m.datos);
    arreglarComprobaciones(p.datos);
    std::vector<uint8_t> mPlano = m.datos, pPlano = p.datos;
    cifrar(m.cabecera, m.datos);
    cifrar(p.cabecera, p.datos);
    if (!escribir("/main.dat", m.datos) || !escribir("/Villager0/personal.dat", p.datos))
        return "No he podido escribir. La partida no ha cambiado (sales sin guardar).";
    if (R_FAILED(fsFsCommit(&g_fs)))
        return "No he podido guardar. La partida no ha cambiado.";
    Archivo vm, vp;
    vm.cabecera = m.cabecera;
    vp.cabecera = p.cabecera;
    if (!leer("/main.dat", vm.datos) || !leer("/Villager0/personal.dat", vp.datos))
        return "Guardado, pero no he podido comprobarlo leyéndolo.";
    cifrar(vm.cabecera, vm.datos);
    cifrar(vp.cabecera, vp.datos);
    if (vm.datos != mPlano || vp.datos != pPlano || comprobacionesMal(vm.datos) || comprobacionesMal(vp.datos))
        return "¡Ojo! Lo guardado no coincide. Restaura la copia de la SD.";
    g_main.datos = mPlano;
    g_pers.datos = pPlano;
    return "";
}

// ---------------------------------------------------------------------------------------------------------- dibujo
SDL_Renderer* g_r;
TTF_Font* g_fuente;
TTF_Font* g_grande;

const SDL_Color CREMA = {0xF6, 0xF1, 0xE3, 255};
const SDL_Color VERDE = {0x3F, 0x9E, 0x5C, 255};
const SDL_Color VERDE_OSCURO = {0x2C, 0x6E, 0x40, 255};
const SDL_Color MARRON = {0x5A, 0x4A, 0x3A, 255};
const SDL_Color BLANCO = {255, 255, 255, 255};
const SDL_Color GRIS = {0xC9, 0xC2, 0xB0, 255};
const SDL_Color ROJO = {0xC0, 0x45, 0x38, 255};

void rect(const SDL_Rect& r, SDL_Color c)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(g_r, &r);
}

void texto(const std::string& s, int x, int y, SDL_Color c, TTF_Font* f = nullptr, bool centrado = false, int ancho = 0)
{
    if (s.empty())
        return;
    SDL_Surface* sf = ancho > 0 ? TTF_RenderUTF8_Blended_Wrapped(f ? f : g_fuente, s.c_str(), c, ancho)
                                : TTF_RenderUTF8_Blended(f ? f : g_fuente, s.c_str(), c);
    if (!sf)
        return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_r, sf);
    SDL_Rect d = {centrado ? x - sf->w / 2 : x, centrado ? y - sf->h / 2 : y, sf->w, sf->h};
    SDL_RenderCopy(g_r, t, nullptr, &d);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(sf);
}

struct Boton {
    SDL_Rect r;
    std::string texto;
    bool marcado = false;
    bool activo = true;
};

void dibujar(const Boton& b, bool foco)
{
    SDL_Color fondo = !b.activo ? GRIS : b.marcado ? VERDE_OSCURO : VERDE;
    if (foco)
        rect({b.r.x - 4, b.r.y - 4, b.r.w + 8, b.r.h + 8}, MARRON);
    rect(b.r, fondo);
    texto(b.texto, b.r.x + b.r.w / 2, b.r.y + b.r.h / 2, BLANCO, nullptr, true);
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

} // namespace

// ------------------------------------------------------------------------------------------------------------ main
int main(int, char**)
{
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
    g_grande = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 44);

    std::string error = abrirPartida();
    std::string aviso;
    bool avisoMalo = false;
    bool cambios = false;

    enum Pantalla { Inicio, Dinero, Flores, Salir } pantalla = Inicio;
    int foco = 0;
    std::vector<uint16_t> elegidas;
    bool alAzar = false;
    int primeraFlor = 0; // scroll of the flower list

    bool touchAntes = false;
    while (appletMainLoop())
    {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        HidTouchScreenState ts = {};
        hidGetTouchScreenStates(&ts, 1);
        const bool toque = ts.count > 0 && !touchAntes;
        touchAntes = ts.count > 0;
        const int tx = toque ? ts.touches[0].x : -1, ty = toque ? ts.touches[0].y : -1;

        // Build this screen's buttons.
        std::vector<Boton> botones;
        Recuento rec = error.empty() ? contar(g_main.datos) : Recuento{};
        if (!error.empty())
            botones.push_back({{490, 560, 300, 80}, "Salir"});
        else if (pantalla == Inicio)
        {
            botones.push_back({{100, 220, 520, 110}, "Dinero y millas"});
            botones.push_back({{660, 220, 520, 110}, "Flores"});
            botones.push_back({{100, 360, 520, 110}, "Quitar malas hierbas (" + std::to_string(rec.malasHierbas) + ")", false, rec.malasHierbas > 0});
            botones.push_back({{660, 360, 520, 110}, "Guardar en la partida", false, cambios});
            botones.push_back({{380, 520, 520, 90}, cambios ? "Salir sin guardar" : "Salir"});
        }
        else if (pantalla == Dinero)
        {
            botones.push_back({{100, 200, 1080, 100}, "Monedero: " + numero(leerNumero(g_pers.datos.data() + MONEDERO)) + " bayas"});
            botones.push_back({{100, 320, 1080, 100}, "Banco: " + numero(leerNumero(g_pers.datos.data() + BANCO)) + " bayas"});
            botones.push_back({{100, 440, 1080, 100}, "Millas Nook: " + numero(leerNumero(g_pers.datos.data() + MILLAS))});
            botones.push_back({{440, 590, 400, 80}, "Volver"});
        }
        else if (pantalla == Flores)
        {
            // Two columns of 6 flowers, scrolled with the buttons below.
            const int total = sizeof(FLORES_ABIERTAS) / sizeof(FLORES_ABIERTAS[0]);
            for (int i = 0; i < 12 && primeraFlor + i < total; i++)
            {
                const auto& f = FLORES_ABIERTAS[primeraFlor + i];
                bool marcada = std::find(elegidas.begin(), elegidas.end(), f.id) != elegidas.end();
                botones.push_back({{80 + (i / 6) * 570, 150 + (i % 6) * 72, 540, 62}, std::string(marcada ? "+ " : "") + f.nombre, marcada});
            }
            botones.push_back({{80, 590, 160, 70}, "Anteriores", false, primeraFlor > 0});
            botones.push_back({{250, 590, 160, 70}, "Siguientes", false, primeraFlor + 12 < total});
            botones.push_back({{430, 590, 260, 70}, alAzar ? "Mezcla: al azar" : "Mezcla: alternando"});
            botones.push_back({{710, 590, 260, 70}, "Cambiar " + std::to_string(rec.flores), false, !elegidas.empty()});
            botones.push_back({{990, 590, 210, 70}, "Volver"});
        }
        else if (pantalla == Salir)
        {
            botones.push_back({{140, 400, 480, 100}, "Salir sin guardar"});
            botones.push_back({{660, 400, 480, 100}, "Volver"});
        }

        // Focus with the D-pad, choose with A or a touch.
        if (down & (HidNpadButton_Down | HidNpadButton_Right))
            foco = (foco + 1) % std::max<int>(1, botones.size());
        if (down & (HidNpadButton_Up | HidNpadButton_Left))
            foco = (foco + (int)botones.size() - 1) % std::max<int>(1, botones.size());
        int pulsado = -1;
        if (down & HidNpadButton_A)
            pulsado = foco;
        for (size_t i = 0; i < botones.size(); i++)
            if (tx >= botones[i].r.x && tx < botones[i].r.x + botones[i].r.w && ty >= botones[i].r.y && ty < botones[i].r.y + botones[i].r.h)
                pulsado = foco = (int)i;
        if (pulsado >= 0 && pulsado < (int)botones.size() && !botones[pulsado].activo)
            pulsado = -1;
        const bool atras = down & HidNpadButton_B;

        bool salir = false;
        if (!error.empty())
            salir = pulsado == 0 || atras || (down & HidNpadButton_Plus);
        else if (pantalla == Inicio)
        {
            if (pulsado == 0)
                pantalla = Dinero, foco = 0, aviso.clear();
            else if (pulsado == 1)
                pantalla = Flores, foco = 0, aviso.clear();
            else if (pulsado == 2)
            {
                int n = quitarMalasHierbas(g_main.datos);
                cambios = true;
                aviso = "Quitadas " + std::to_string(n) + " malas hierbas. Falta guardar.";
                avisoMalo = false;
            }
            else if (pulsado == 3)
            {
                                std::string mal = guardarPartida();
                avisoMalo = !mal.empty();
                aviso = mal.empty() ? "Guardado y comprobado. Ya puedes abrir el juego." : mal;
                if (mal.empty())
                    cambios = false;
            }
            else if (pulsado == 4 || atras || (down & HidNpadButton_Plus))
            {
                if (cambios)
                    pantalla = Salir, foco = 1;
                else
                    salir = true;
            }
        }
        else if (pantalla == Dinero)
        {
            const uint32_t pos[3] = {MONEDERO, BANCO, MILLAS};
            const char* titulos[3] = {"Bayas en el monedero (máx. 99.999)", "Bayas en el banco", "Millas Nook"};
            const uint32_t maximos[3] = {99999, 999999999, 999999999};
            if (pulsado >= 0 && pulsado < 3)
            {
                uint32_t v = leerNumero(g_pers.datos.data() + pos[pulsado]);
                if (pedirNumero(titulos[pulsado], v, maximos[pulsado]))
                {
                    escribirNumero(g_pers.datos.data() + pos[pulsado], v);
                    cambios = true;
                    aviso = "Cambiado. Falta guardar.";
                    avisoMalo = false;
                }
            }
            else if (pulsado == 3 || atras)
                pantalla = Inicio, foco = 0;
        }
        else if (pantalla == Flores)
        {
            const int total = sizeof(FLORES_ABIERTAS) / sizeof(FLORES_ABIERTAS[0]);
            const int visibles = std::min(12, total - primeraFlor);
            if (pulsado >= 0 && pulsado < visibles)
            {
                uint16_t id = FLORES_ABIERTAS[primeraFlor + pulsado].id;
                auto it = std::find(elegidas.begin(), elegidas.end(), id);
                if (it == elegidas.end())
                    elegidas.push_back(id);
                else
                    elegidas.erase(it);
            }
            else if (pulsado == visibles)
                primeraFlor = std::max(0, primeraFlor - 12);
            else if (pulsado == visibles + 1)
                primeraFlor += 12;
            else if (pulsado == visibles + 2)
                alAzar = !alAzar;
            else if (pulsado == visibles + 3)
            {
                int n = cambiarFlores(g_main.datos, elegidas, alAzar, (uint32_t)time(nullptr));
                cambios = true;
                aviso = "Cambiadas " + std::to_string(n) + " flores. Falta guardar.";
                avisoMalo = false;
                pantalla = Inicio, foco = 3;
            }
            else if (pulsado == visibles + 4 || atras)
                pantalla = Inicio, foco = 1;
        }
        else if (pantalla == Salir)
        {
            if (pulsado == 0)
                salir = true;
            else if (pulsado == 1 || atras)
                pantalla = Inicio, foco = 3;
        }
        if (salir)
            break;
        if (foco >= (int)botones.size())
            foco = 0;

        // Draw.
        rect({0, 0, W, H}, CREMA);
        rect({0, 0, W, 110}, VERDE);
        texto("NHSE", 40, 30, BLANCO, g_grande);
        if (error.empty())
            texto(g_isla + " · " + g_jugador, 1240 - 600, 38, BLANCO, nullptr, false, 600);
        if (!error.empty())
            texto(error, 100, 220, ROJO, g_grande, false, 1080);
        else if (pantalla == Inicio)
        {
            texto("Elige qué quieres cambiar. Antes de nada se hizo una copia de la partida en la SD.", 100, 140, MARRON, nullptr, false, 1080);
            if (aviso.empty())
                texto("Copia: " + g_copia.substr(5), 100, 640, MARRON, nullptr, false, 1080);
        }
        else if (pantalla == Dinero)
            texto("Toca un número para cambiarlo.", 100, 140, MARRON);
        else if (pantalla == Flores)
            texto("Elige una o varias flores: todas las de la isla (" + std::to_string(rec.flores) + ") se cambian por esas.", 80, 115, MARRON, nullptr, false, 1120);
        else if (pantalla == Salir)
            texto("Hay cambios sin guardar. Si sales ahora, la partida se queda como estaba.", 140, 250, MARRON, g_grande, false, 1000);
        for (size_t i = 0; i < botones.size(); i++)
            dibujar(botones[i], (int)i == foco);
        if (!aviso.empty() && (pantalla == Inicio || pantalla == Dinero))
            texto(aviso, 100, pantalla == Inicio ? 640 : 548, avisoMalo ? ROJO : VERDE_OSCURO, nullptr, false, 1080);
        SDL_RenderPresent(g_r);
    }

    if (g_abierta)
        fsFsClose(&g_fs);
    TTF_CloseFont(g_fuente);
    TTF_CloseFont(g_grande);
    TTF_Quit();
    SDL_DestroyRenderer(g_r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    plExit();
    return 0;
}
