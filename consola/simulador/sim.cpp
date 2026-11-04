// Simulador de NHSE para Switch en el Mac (ver switch.h). Variables de entorno:
//   NHSE_SIM_PARTIDA  carpeta con una COPIA de la partida (main.dat, Villager0...): se escribe en ella al guardar
//   NHSE_SIM_GUION    archivo con lo que se pulsa, una línea por fotograma:
//                       a b x y l r + arriba abajo izq der   pulsar ese botón
//                       toque X Y                            tocar la pantalla
//                       teclado TEXTO                        lo que se escribirá la próxima vez que salga el teclado
//                       foto NOMBRE                          guardar la pantalla en NOMBRE.bmp
//                       esperar N                            N fotogramas sin hacer nada
//   NHSE_SIM_FUENTE   la letra (por defecto, Hiragino del Mac: mide casi como la de la Switch; las pantallas se hicieron con Arial Unicode)
// La copia de seguridad va a «sdmc:/...» y los datos se leen de «romfs:/...», carpetas con esos nombres donde se
// ejecuta.
#include <SDL2/SDL.h>
#include "switch.h"
#undef SDL_RenderPresent
#undef SDL_CreateRenderer
#undef SDL_CreateWindow
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::deque<std::string> g_guion;
std::deque<std::string> g_teclado;
std::string g_foto;
int g_esperar = 0;
bool g_fin = false;
HidTouchScreenState g_toque = {};
std::string g_raiz;
std::vector<char> g_fuente;

std::string ruta(const char* p) { return g_raiz + p; }
} // namespace

extern "C" SDL_Window* sim_ventana(const char* t, int x, int y, int w, int h, Uint32)
{
    return SDL_CreateWindow(t, x, y, w, h, SDL_WINDOW_HIDDEN);
}

extern "C" SDL_Renderer* sim_renderer(SDL_Window* w, int, Uint32)
{
    return SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
}

extern "C" void sim_present(SDL_Renderer* r)
{
    if (!g_foto.empty())
    {
        int w, h;
        SDL_GetRendererOutputSize(r, &w, &h);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        SDL_SaveBMP(s, (g_foto + ".bmp").c_str());
        SDL_FreeSurface(s);
        printf("foto %s\n", g_foto.c_str());
        g_foto.clear();
    }
    SDL_RenderPresent(r);
}

Result romfsInit()
{
    const char* g = getenv("NHSE_SIM_GUION");
    std::ifstream f(g ? g : "");
    for (std::string l; std::getline(f, l);)
        if (!l.empty() && l[0] != '#')
            g_guion.push_back(l);
    const char* p = getenv("NHSE_SIM_PARTIDA");
    g_raiz = p ? p : ".";
    return 0;
}
Result romfsExit() { return 0; }
bool appletMainLoop() { return !g_fin; }

Result plInitialize(PlServiceType) { return 0; }
void plExit() {}
Result plGetSharedFontByType(PlFontData* d, PlSharedFontType)
{
    const char* f = getenv("NHSE_SIM_FUENTE");
    std::ifstream in(f ? f : "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", std::ios::binary);
    g_fuente.assign(std::istreambuf_iterator<char>(in), {});
    d->address = g_fuente.data();
    d->size = (u32)g_fuente.size();
    return 0;
}

void padConfigureInput(int, int) {}
void padInitializeDefault(PadState* p) { *p = {}; }
void padUpdate(PadState* p)
{
    p->down = 0;
    p->held = 0;
    g_toque.count = 0;
    if (g_esperar > 0)
    {
        g_esperar--;
        return;
    }
    if (g_guion.empty())
    {
        // Unos fotogramas más para la última foto, y se acaba.
        static int fin = 3;
        if (--fin <= 0)
            g_fin = true;
        return;
    }
    std::string l = g_guion.front();
    g_guion.pop_front();
    const std::string orden = l.substr(0, l.find(' '));
    const std::string resto = l.find(' ') == std::string::npos ? "" : l.substr(l.find(' ') + 1);
    struct { const char* n; u64 b; } botones[] = {{"a", HidNpadButton_A}, {"b", HidNpadButton_B}, {"x", HidNpadButton_X},
        {"y", HidNpadButton_Y}, {"l", HidNpadButton_L}, {"r", HidNpadButton_R}, {"+", HidNpadButton_Plus},
        {"arriba", HidNpadButton_AnyUp}, {"abajo", HidNpadButton_AnyDown}, {"izq", HidNpadButton_AnyLeft}, {"der", HidNpadButton_AnyRight}};
    for (auto& b : botones)
        if (orden == b.n)
            p->down = p->held = b.b;
    if (orden == "toque")
    {
        g_toque.count = 1;
        sscanf(resto.c_str(), "%u %u", &g_toque.touches[0].x, &g_toque.touches[0].y);
        g_esperar = 1; // the finger is lifted before the next touch
    }
    else if (orden == "teclado")
        g_teclado.push_back(resto);
    else if (orden == "foto")
        g_foto = resto;
    else if (orden == "esperar")
        g_esperar = atoi(resto.c_str());
}
u64 padGetButtonsDown(const PadState* p) { return p->down; }
u64 padGetButtons(const PadState* p) { return p->held; }

void hidInitializeTouchScreen() {}
int hidGetTouchScreenStates(HidTouchScreenState* s, int)
{
    *s = g_toque;
    return 1;
}

Result swkbdCreate(SwkbdConfig* c, int) { c->cabecera[0] = 0; return 0; }
void swkbdConfigMakePresetDefault(SwkbdConfig*) {}
void swkbdConfigSetType(SwkbdConfig*, SwkbdType) {}
void swkbdConfigSetStringLenMax(SwkbdConfig*, int) {}
void swkbdConfigSetHeaderText(SwkbdConfig* c, const char* t) { snprintf(c->cabecera, sizeof(c->cabecera), "%s", t); }
void swkbdConfigSetGuideText(SwkbdConfig*, const char*) {}
void swkbdConfigSetInitialText(SwkbdConfig*, const char*) {}
Result swkbdShow(SwkbdConfig* c, char* out, size_t n)
{
    if (g_teclado.empty())
    {
        printf("teclado «%s»: cancelado\n", c->cabecera);
        return 1;
    }
    snprintf(out, n, "%s", g_teclado.front().c_str());
    printf("teclado «%s»: %s\n", c->cabecera, out);
    g_teclado.pop_front();
    return 0;
}
void swkbdClose(SwkbdConfig*) {}

Result fsOpenSaveDataFileSystem(FsFileSystem* fs, FsSaveDataSpaceId, const FsSaveDataAttribute*)
{
    struct stat st;
    fs->abierto = stat(ruta("/main.dat").c_str(), &st) == 0;
    return fs->abierto ? 0 : 1;
}
Result fsFsOpenFile(FsFileSystem*, const char* p, int modo, FsFile* f)
{
    f->f = fopen(ruta(p).c_str(), modo == FsOpenMode_Write ? "r+b" : "rb");
    return f->f ? 0 : 1;
}
Result fsFileGetSize(FsFile* f, s64* tam)
{
    fseek((FILE*)f->f, 0, SEEK_END);
    *tam = ftell((FILE*)f->f);
    return 0;
}
Result fsFileRead(FsFile* f, s64 pos, void* d, u64 n, int, u64* leidos)
{
    fseek((FILE*)f->f, pos, SEEK_SET);
    *leidos = fread(d, 1, n, (FILE*)f->f);
    return 0;
}
Result fsFileWrite(FsFile* f, s64 pos, const void* d, u64 n, int)
{
    fseek((FILE*)f->f, pos, SEEK_SET);
    return fwrite(d, 1, n, (FILE*)f->f) == n ? 0 : 1;
}
void fsFileClose(FsFile* f) { fclose((FILE*)f->f); }
Result fsFsCommit(FsFileSystem*) { printf("guardado\n"); return 0; }
void fsFsClose(FsFileSystem*) {}
Result fsFsOpenDirectory(FsFileSystem*, const char* p, int, FsDir* d)
{
    d->d = opendir(ruta(p).c_str());
    return d->d ? 0 : 1;
}
Result fsDirRead(FsDir* d, s64* n, size_t max, FsDirectoryEntry* e)
{
    *n = 0;
    while ((size_t)*n < max)
    {
        dirent* x = readdir((DIR*)d->d);
        if (!x)
            break;
        if (!strcmp(x->d_name, ".") || !strcmp(x->d_name, ".."))
            continue;
        snprintf(e[*n].name, sizeof(e[*n].name), "%s", x->d_name);
        e[*n].type = x->d_type == DT_DIR ? FsDirEntryType_Dir : FsDirEntryType_File;
        (*n)++;
    }
    return 0;
}
void fsDirClose(FsDir* d) { closedir((DIR*)d->d); }
