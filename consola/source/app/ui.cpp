#include "ui.hpp"
#include "../core/acnh.hpp"
#include "dibujos.hpp"
#include "registro.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>

namespace ui {

SDL_Renderer* g_r;
TTF_Font* g_fuente;
TTF_Font* g_pequena;
TTF_Font* g_grande;
TTF_Font* g_negrita;
TTF_Font* g_titulo;

// Los colores de NHSE para Mac (claro, como macOS). Los nombres son los de antes para que las pantallas sigan igual.
const SDL_Color CREMA = {0xF5, 0xF5, 0xF7, 255};        // background
const SDL_Color CREMA_OSCURA = {0xE5, 0xE5, 0xEA, 255}; // bars, lines
const SDL_Color VERDE = {0x34, 0xA8, 0x5A, 255};        // the green of selections
const SDL_Color VERDE_OSCURO = {0x24, 0x8A, 0x46, 255};
const SDL_Color MARRON = {0x1D, 0x1D, 0x1F, 255};       // text
const SDL_Color MARRON_SUAVE = {0x6E, 0x6E, 0x73, 255}; // secondary text
const SDL_Color BLANCO = {255, 255, 255, 255};
const SDL_Color GRIS = {0xEF, 0xEF, 0xF2, 255};         // disabled
const SDL_Color ROJO = {0xD7, 0x3A, 0x2F, 255};
const SDL_Color AMARILLO = {0xF2, 0xC1, 0x4E, 255};
const SDL_Color AZUL = {0x0A, 0x7A, 0xFF, 255};         // main action and focus
namespace {
const SDL_Color BORDE = {0xD1, 0xD1, 0xD6, 255};
const SDL_Color VERDE_CLARO = {0xE3, 0xF4, 0xE8, 255};
const SDL_Color LATERAL_FONDO = {0xE8, 0xE8, 0xED, 255};
const SDL_Color APAGADO = {0xAE, 0xAE, 0xB2, 255};
} // namespace

// ---------------------------------------------------------------------------------------------------------- dibujo
// Las letras ya dibujadas se guardan: dibujar texto cada vez es lento.
namespace {
std::map<std::tuple<std::string, TTF_Font*, uint32_t, int>, Letras> g_letras;
}

Letras letras(const std::string& s, TTF_Font* f, SDL_Color c, int ancho)
{
    const uint32_t color = (c.r << 24) | (c.g << 16) | (c.b << 8) | c.a;
    auto clave = std::make_tuple(s, f, color, ancho);
    auto it = g_letras.find(clave);
    if (it != g_letras.end())
        return it->second;
    if (g_letras.size() > 800)
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

void redondo(const SDL_Rect& r, int radio, SDL_Color c)
{
    // With triangles (not lines): the screens are drawn scaled, and lines of one point would get lost.
    radio = std::max(0, std::min({radio, r.w / 2, r.h / 2}));
    if (radio == 0)
    {
        rect(r, c);
        return;
    }
    constexpr int TRAMOS = 6;
    static std::vector<SDL_Vertex> v;
    static std::vector<int> idx;
    v.clear();
    idx.clear();
    const SDL_Color col = c;
    v.push_back({{r.x + r.w / 2.0f, r.y + r.h / 2.0f}, col, {0, 0}});
    const float cx[4] = {(float)(r.x + r.w - radio), (float)(r.x + radio), (float)(r.x + radio), (float)(r.x + r.w - radio)};
    const float cy[4] = {(float)(r.y + radio), (float)(r.y + radio), (float)(r.y + r.h - radio), (float)(r.y + r.h - radio)};
    for (int q = 0; q < 4; q++)
        for (int i = 0; i <= TRAMOS; i++)
        {
            const double a = (q * 90.0 + i * 90.0 / TRAMOS) * 3.14159265358979 / 180.0;
            v.push_back({{cx[q] + (float)(radio * std::cos(a)), cy[q] - (float)(radio * std::sin(a))}, col, {0, 0}});
        }
    const int n = (int)v.size() - 1;
    for (int i = 1; i <= n; i++)
    {
        idx.push_back(0);
        idx.push_back(i);
        idx.push_back(i % n + 1);
    }
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_r, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

// ---------------------------------------------------------------------------------- revisar el diseño (simulador)
// Con NHSE_REVISAR en el simulador, la app dice cada texto que se sale de su botón o de la pantalla y cada botón que se
// sale o se monta sobre otro. En la Switch no hace nada.
namespace {
const bool g_revisar = getenv("NHSE_REVISAR") != nullptr;
bool g_enPantalla = false;             // drawing the screen (virtual coordinates), not the bars
const SDL_Rect* g_dentroDe = nullptr;  // the button being drawn
std::string g_pantallaActual;
std::set<std::string> g_avisados;
std::vector<std::pair<SDL_Rect, std::string>> g_libres; // texts drawn outside buttons this frame

// NHSE_EXPLORAR (simulador): la app se recorre sola, pulsando cada botón de cada pantalla, para revisarlas todas. Guarda
// una foto de cada pantalla nueva en la carpeta de NHSE_EXPLORAR.
const char* const g_explorar = getenv("NHSE_EXPLORAR");
struct Explorando {
    size_t profundidad;
    int siguiente;
};
std::vector<Explorando> g_exploracion;
std::set<std::string> g_vistas; // the screens already explored (title + buttons)
std::set<std::string> g_fotos;  // titles with a photo
int g_seccionExplorada = 0;
int g_pasos = 0;
std::string g_hacerFoto;

void revisar(const std::string& que, const std::string& texto)
{
    const std::string m = "[" + g_pantallaActual + "] " + que + ": «" + texto + "»";
    if (g_avisados.insert(m).second)
        fprintf(stderr, "REVISAR %s\n", m.c_str());
}

// The letters of a line have some room above and below: a few points of margin.
void revisarTexto(const SDL_Rect& d, const std::string& s)
{
    if (!g_revisar || !g_enPantalla)
        return;
    constexpr int M = 4;
    if (g_dentroDe)
    {
        const SDL_Rect& r = *g_dentroDe;
        if (d.x < r.x - 1 || d.x + d.w > r.x + r.w + 1 || d.y < r.y - M || d.y + d.h > r.y + r.h + M)
            revisar("se sale del botón", s);
    }
    else
    {
        if (d.x < 0 || d.x + d.w > W || d.y < 0 || d.y + d.h > ABAJO + M)
            revisar("se sale de la pantalla", s);
        g_libres.push_back({{d.x + 1, d.y + M, d.w - 2, d.h - 2 * M}, s});
    }
}
} // namespace

void marco(const SDL_Rect& r, SDL_Color c, int grosor)
{
    grosor = std::max(2, grosor);
    rect({r.x - grosor, r.y - grosor, r.w + 2 * grosor, grosor}, c);
    rect({r.x - grosor, r.y + r.h, r.w + 2 * grosor, grosor}, c);
    rect({r.x - grosor, r.y, grosor, r.h}, c);
    rect({r.x + r.w, r.y, grosor, r.h}, c);
}

void linea(int x1, int y1, int x2, int y2)
{
    SDL_Rect r = x1 == x2 ? SDL_Rect{x1, std::min(y1, y2), 2, std::abs(y2 - y1) + 1} : SDL_Rect{std::min(x1, x2), y1, std::abs(x2 - x1) + 1, 2};
    SDL_RenderFillRect(g_r, &r);
}

void borde(const SDL_Rect& r)
{
    const SDL_Rect lados[4] = {{r.x, r.y, r.w, 2}, {r.x, r.y + r.h - 2, r.w, 2}, {r.x, r.y, 2, r.h}, {r.x + r.w - 2, r.y, 2, r.h}};
    SDL_RenderFillRects(g_r, lados, 4);
}

void poligono(const std::vector<SDL_FPoint>& p, SDL_Color c)
{
    if (p.size() < 3)
        return;
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    for (const auto& q : p)
        v.push_back({q, c, {0, 0}});
    for (int i = 1; i + 1 < (int)p.size(); i++)
        idx.insert(idx.end(), {0, i, i + 1});
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_r, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

int texto(const std::string& s, int x, int y, SDL_Color c, TTF_Font* f, Alinear a, int ancho)
{
    if (s.empty())
        return 0;
    Letras l = letras(s, f ? f : g_fuente, c, ancho);
    if (!l.t)
        return 0;
    SDL_Rect d = {a == Centro ? x - l.w / 2 : a == Derecha ? x - l.w : x, a == Centro ? y - l.h / 2 : y, l.w, l.h};
    revisarTexto(d, s);
    SDL_RenderCopy(g_r, l.t, nullptr, &d);
    return l.h;
}

int anchoTexto(const std::string& s, TTF_Font* f)
{
    int w = 0;
    TTF_SizeUTF8(f ? f : g_fuente, s.c_str(), &w, nullptr);
    return w;
}

std::string encajar(std::string s, int ancho, TTF_Font* f)
{
    f = f ? f : g_fuente;
    if (anchoTexto(s, f) <= ancho)
        return s;
    if (g_revisar && g_enPantalla)
        revisar("recortado", s);
    while (!s.empty())
    {
        size_t i = s.size() - 1;
        while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
            i--;
        s.erase(i);
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        if (anchoTexto(s + "…", f) <= ancho)
            break;
    }
    return s + "…";
}

// Un rectángulo redondeado con borde.
static void tarjeta(const SDL_Rect& r, int radio, SDL_Color fondo, SDL_Color borde, int grosor = 1)
{
    redondo(r, radio, borde);
    redondo({r.x + grosor, r.y + grosor, r.w - 2 * grosor, r.h - 2 * grosor}, std::max(0, radio - grosor), fondo);
}

static void dibujarBoton(const Boton& b, bool foco);

void dibujar(const Boton& b, bool foco)
{
    g_dentroDe = &b.r;
    dibujarBoton(b, foco);
    g_dentroDe = nullptr;
}

static void dibujarBoton(const Boton& b, bool foco)
{
    if (b.vertical)
    {
        if (foco)
            redondo({b.r.x - 4, b.r.y - 4, b.r.w + 8, b.r.h + 8}, 16, AZUL);
        tarjeta(b.r, 12, !b.activo ? GRIS : BLANCO, b.marcado ? VERDE : BORDE, b.marcado ? 3 : 2);
        // The picture on top and, below, the name and the detail, all inside the card.
        const int h1 = TTF_FontHeight(g_fuente), h2 = b.detalle.empty() ? 0 : TTF_FontHeight(g_pequena) - 2;
        const int textos = h1 + h2;
        const int lado = b.imagen ? std::max(0, std::min(b.r.w - 40, b.r.h - 10 - textos - 10)) : 0;
        if (b.imagen)
        {
            SDL_Rect d = {b.r.x + (b.r.w - lado) / 2, b.r.y + 10, lado, lado};
            SDL_RenderCopy(g_r, b.imagen, nullptr, &d);
        }
        const SDL_Color letra = b.activo ? MARRON : MARRON_SUAVE;
        const int y = b.imagen ? b.r.y + 10 + lado + 2 : b.r.y + (b.r.h - textos) / 2;
        texto(encajar(b.texto, b.r.w - 16), b.r.x + b.r.w / 2, y + h1 / 2, letra, nullptr, Centro);
        if (!b.detalle.empty())
            texto(encajar(b.detalle, b.r.w - 16, g_pequena), b.r.x + b.r.w / 2, y + h1 + h2 / 2 - 1, MARRON_SUAVE, g_pequena, Centro);
        return;
    }
    if (b.casilla)
    {
        if (foco)
            redondo({b.r.x - 4, b.r.y - 4, b.r.w + 8, b.r.h + 8}, 10, AZUL);
        const SDL_Color fondo = !b.activo ? GRIS : b.color.a ? b.color : BLANCO;
        tarjeta(b.r, 6, fondo, b.marcado ? VERDE : BORDE, b.marcado ? 3 : 1);
        if (b.imagen)
        {
            const int lado = b.r.w - 12;
            SDL_Rect d = {b.r.x + 6, b.r.y + (b.r.h - lado) / 2, lado, lado};
            SDL_RenderCopy(g_r, b.imagen, nullptr, &d);
        }
        else if (!b.texto.empty())
            texto(b.texto, b.r.x + b.r.w / 2, b.r.y + b.r.h / 2, APAGADO, g_pequena, Centro);
        if (!b.detalle.empty())
        {
            // The number with a white edge, so it is read on any colour.
            const int x = b.r.x + b.r.w - 5, y = b.r.y + b.r.h - TTF_FontHeight(g_pequena) - 2;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    if (dx || dy)
                        texto(b.detalle, x + dx, y + dy, BLANCO, g_pequena, Derecha);
            texto(b.detalle, x, y, MARRON, g_pequena, Derecha);
        }
        return;
    }
    const int radio = std::min(12, b.r.h / 3);
    SDL_Color fondo = !b.activo ? GRIS : b.principal ? AZUL : b.marcado ? VERDE_CLARO : BLANCO;
    SDL_Color borde = !b.activo ? CREMA_OSCURA : b.principal ? AZUL : b.marcado ? VERDE : BORDE;
    const SDL_Color letra = !b.activo ? APAGADO : b.principal ? BLANCO : MARRON;
    if (foco)
        redondo({b.r.x - 4, b.r.y - 4, b.r.w + 8, b.r.h + 8}, radio + 4, AZUL);
    // With the cursor on it, the blue ring is enough (the green edge of the chosen one would make it thick).
    tarjeta(b.r, radio, fondo, foco ? fondo : borde, b.marcado && !b.principal && !foco ? 3 : 2);
    int x0 = b.r.x, w0 = b.r.w;
    const bool conDibujo = b.color.a > 0 || b.imagen;
    if (b.color.a > 0)
    {
        const int lado = std::min(b.r.h - 12, 40);
        redondo({b.r.x + 10, b.r.y + (b.r.h - lado) / 2, lado, lado}, 6, b.color);
        x0 += lado + 16;
        w0 -= lado + 16;
    }
    if (b.imagen)
    {
        const int lado = b.grande ? std::min(b.r.h - 20, 60) : std::min(b.r.h - 6, 64);
        SDL_Rect d = {b.r.x + (b.grande ? 16 : 10), b.r.y + (b.r.h - lado) / 2, lado, lado};
        SDL_RenderCopy(g_r, b.imagen, nullptr, &d);
        x0 = d.x + lado + 14 - (b.grande ? 0 : 0);
        w0 = b.r.x + b.r.w - x0;
    }
    if (b.grande && !b.valor.empty())
    {
        // A number card, as the money of the Mac: what it is, the number big, and a note.
        const int x = conDibujo ? x0 : b.r.x + 24;
        const int ancho = b.r.x + b.r.w - x - 16;
        const int h1 = TTF_FontHeight(g_pequena), h2 = TTF_FontHeight(g_titulo), h3 = b.detalle.empty() ? 0 : TTF_FontHeight(g_pequena);
        const int junto = 8;
        int y = b.r.y + (b.r.h - (h1 + h2 + h3 - 2 * junto)) / 2;
        texto(encajar(b.texto, ancho, g_pequena), x, y, MARRON_SUAVE, g_pequena);
        y += h1 - junto;
        texto(encajar(b.valor, ancho, g_titulo), x, y, letra, g_titulo);
        y += h2 - junto;
        if (!b.detalle.empty())
            texto(encajar(b.detalle, ancho, g_pequena), x, y, MARRON_SUAVE, g_pequena);
        return;
    }
    if (b.grande)
    {
        // A card of the Mac: title in bold, and what there is now in grey.
        const int x = conDibujo ? x0 : b.r.x + 24;
        const int ancho = b.r.x + b.r.w - x - 16;
        const SDL_Color gris = !b.activo ? APAGADO : b.principal ? BLANCO : MARRON_SUAVE;
        if (b.detalle.empty())
            texto(encajar(b.texto, ancho, g_negrita), x, b.r.y + b.r.h / 2 - 17, letra, g_negrita);
        else
        {
            texto(encajar(b.texto, ancho, g_negrita), x, b.r.y + b.r.h / 2 - 31, letra, g_negrita);
            texto(encajar(b.detalle, ancho, g_pequena), x, b.r.y + b.r.h / 2 + 3, gris, g_pequena);
        }
        return;
    }
    const int margen = std::min(24, w0 / 8);
    // Small letters if the button is low or the text does not fit with the normal ones.
    TTF_Font* f = b.pequena || b.r.h < 50 || anchoTexto(b.texto) > w0 - margen ? g_pequena : g_fuente;
    const SDL_Color gris = b.principal ? BLANCO : !b.activo ? APAGADO : MARRON_SUAVE;
    // With a picture, the text on the left, as the lists of the Mac; if not, centred.
    const int ancho = conDibujo ? w0 - 12 : w0 - margen;
    const int cx = conDibujo ? x0 : x0 + w0 / 2;
    const Alinear al = conDibujo ? Izquierda : Centro;
    auto linea = [&](const std::string& t, TTF_Font* fu, SDL_Color c, int arriba) {
        const std::string e = encajar(t, ancho, fu);
        texto(e, cx, conDibujo ? arriba : arriba + TTF_FontHeight(fu) / 2, c, fu, al);
    };
    if (b.detalle.empty())
    {
        linea(b.texto, f, letra, b.r.y + (b.r.h - TTF_FontHeight(f)) / 2);
        return;
    }
    // Two lines: the title (smaller if the button is low) and the detail, together in the middle; if they do not fit,
    // in one line.
    TTF_Font* f1 = b.r.h >= 66 && anchoTexto(b.texto) <= ancho ? g_fuente : g_pequena;
    const int h1 = TTF_FontHeight(f1), h2 = TTF_FontHeight(g_pequena);
    // The lines of the font have room above and below: they can come a bit closer.
    const int junto = std::max(6, h1 / 4);
    if (h1 + h2 - junto - 4 > b.r.h)
    {
        linea(b.texto + " · " + b.detalle, g_pequena, letra, b.r.y + (b.r.h - h2) / 2);
        return;
    }
    const int arriba = b.r.y + (b.r.h - (h1 + h2 - junto)) / 2;
    linea(b.texto, f1, letra, arriba);
    linea(b.detalle, g_pequena, gris, arriba + h1 - junto);
}

namespace {
// La barra de abajo: un aviso a la izquierda y la ayuda de los botones a la derecha.
void barra(const std::string& aviso, bool malo, bool suave, const Teclas& teclas)
{
    // White, as the Mac; a notice goes in green (done) or red (something went wrong), with a dot.
    rect({0, ABAJO, W, H - ABAJO}, BLANCO);
    rect({0, ABAJO, W, 1}, BORDE);
    int x = W - 24;
    // Each key as a light grey button with its letter (two keys, «L R», two buttons), as the Mac shows its shortcuts.
    const SDL_Color teclaFondo = {0xE8, 0xE8, 0xED, 255}, teclaBorde = {0xD1, 0xD1, 0xD6, 255};
    for (auto it = teclas.rbegin(); it != teclas.rend(); ++it)
    {
        Letras l = letras(it->second, g_pequena, MARRON_SUAVE, 0);
        x -= l.w;
        texto(it->second, x, H - 24 - l.h / 2, MARRON_SUAVE, g_pequena);
        x -= 8;
        std::vector<std::string> partes;
        std::string w;
        for (char c : it->first + " ")
        {
            if (c == ' ')
            {
                if (!w.empty())
                    partes.push_back(w);
                w.clear();
            }
            else
                w += c;
        }
        for (auto p = partes.rbegin(); p != partes.rend(); ++p)
        {
            const int ancho = std::max(28, anchoTexto(*p, g_negrita) + 14);
            x -= ancho;
            redondo({x, H - 38, ancho, 28}, 8, teclaBorde);
            redondo({x + 1, H - 37, ancho - 2, 25}, 7, teclaFondo);
            texto(*p, x + ancho / 2, H - 25, MARRON, g_pequena, Centro);
            x -= 4;
        }
        x -= 22;
    }
    if (!aviso.empty())
    {
        const SDL_Color c = malo ? ROJO : suave ? MARRON_SUAVE : VERDE_OSCURO;
        int ax = 24;
        if (!suave || malo)
        {
            redondo({ax, H - 30, 12, 12}, 6, c);
            ax += 22;
        }
        texto(encajar(aviso, x - ax - 20, g_pequena), ax, H - 24 - letras("Ag", g_pequena, c, 0).h / 2, c, g_pequena);
    }
}

// ---------------------------------------------------------------------------------------------- pantallas apiladas
struct Entrada {
    Pantalla p;
    int foco;
    bool vista = false; // drawn at least once
};
std::vector<Entrada> g_pila;
std::vector<std::function<void()>> g_pendiente; // changes to the stack, done after the action
bool g_salir = false;
std::string g_aviso;
bool g_avisoMalo = false;
std::string g_avisoFijo; // the one shown when there is none (set by the home screen)
uint64_t g_fotograma = 1;
} // namespace

namespace {
std::function<Lateral()> g_lateral;
constexpr int LAT = 132;                       // width of the side bar, in real pixels
constexpr float ESCALA = (float)(W - LAT) / W; // the screens (made for 1280 × 720) are drawn a bit smaller
constexpr int ARRIBA_REAL = (int)((ABAJO - ABAJO * ESCALA) / 2);

// Real point (of the screen) to the point of the screens, and back.
int aVirtualX(int x) { return (int)std::floor((x - LAT) / ESCALA); }
int aVirtualY(int y) { return (int)std::floor((y - ARRIBA_REAL) / ESCALA); }

// The side bar: where each entry is (real pixels).
struct HuecoLateral {
    SDL_Rect r;
    std::function<void()> accion;
    bool activo;
};
std::vector<HuecoLateral> huecosLateral(const Lateral& l)
{
    std::vector<HuecoLateral> h;
    int y = 12;
    for (const auto& e : l.entradas)
    {
        h.push_back({{8, y, LAT - 16, 86}, e.accion, true});
        y += 92;
    }
    h.push_back({{8, ABAJO - 64 - 8 - 56 - 8, LAT - 16, 64}, l.guardar, l.hayCambios});
    h.push_back({{8, ABAJO - 56 - 8, LAT - 16, 56}, l.salir, true});
    return h;
}

void dibujarLateral(const Lateral& l, int focoLateral)
{
    rect({0, 0, LAT, ABAJO}, LATERAL_FONDO);
    rect({LAT - 1, 0, 1, ABAJO}, BORDE);
    const auto h = huecosLateral(l);
    for (size_t i = 0; i < h.size(); i++)
    {
        const SDL_Rect& r = h[i].r;
        if ((int)i == focoLateral)
            redondo({r.x - 3, r.y - 3, r.w + 6, r.h + 6}, 15, AZUL);
        if (i < l.entradas.size())
        {
            const auto& e = l.entradas[i];
            if (e.elegida)
                redondo(r, 12, {0xD6, 0xEE, 0xDD, 255});
            else if ((int)i == focoLateral)
                redondo(r, 12, LATERAL_FONDO);
            const int lado = 44;
            SDL_Rect d = {r.x + (r.w - lado) / 2, r.y + 8, lado, lado};
            if (e.icono)
                SDL_RenderCopy(g_r, e.icono, nullptr, &d);
            else
            {
                redondo(d, lado / 2, {0x2C, 0x2C, 0x3A, 255});
                texto(e.inicial, d.x + lado / 2, d.y + lado / 2, BLANCO, g_negrita, Centro);
            }
            texto(encajar(e.texto, r.w - 8, g_pequena), r.x + r.w / 2, r.y + 68, MARRON, g_pequena, Centro);
        }
        else if (i == l.entradas.size())
        {
            // «Guardar cambios», blue as in the Mac (grey if there is nothing to save).
            redondo(r, 12, h[i].activo ? AZUL : CREMA_OSCURA);
            texto("Guardar", r.x + r.w / 2, r.y + r.h / 2, h[i].activo ? BLANCO : APAGADO, g_pequena, Centro);
        }
        else
        {
            tarjeta(r, 12, BLANCO, BORDE, 2);
            texto("Salir", r.x + r.w / 2, r.y + r.h / 2, MARRON, g_pequena, Centro);
        }
    }
}
} // namespace

void ponerLateral(std::function<Lateral()> f) { g_lateral = std::move(f); }
void raiz(Pantalla p)
{
    g_pendiente.push_back([p] { g_pila = {{p, 0}}; });
}
int profundidad() { return (int)g_pila.size(); }

void abrir(Pantalla p, int foco) { g_pendiente.push_back([p, foco] { g_pila.push_back({p, foco}); }); }
void volver()
{
    g_pendiente.push_back([] {
        if (g_pila.size() > 1)
            g_pila.pop_back();
    });
}
void reemplazar(Pantalla p, int foco)
{
    g_pendiente.push_back([p, foco] {
        if (!g_pila.empty())
            g_pila.back() = {p, foco};
    });
}
void alInicio()
{
    g_pendiente.push_back([] {
        if (!g_pila.empty())
            g_pila.resize(1);
    });
}
void enfocar(int f)
{
    g_pendiente.push_back([f] {
        if (!g_pila.empty())
            g_pila.back().foco = f;
    });
}
uint64_t fotograma() { return g_fotograma; }
int foco() { return g_pila.empty() ? 0 : g_pila.back().foco; }
void terminar() { g_salir = true; }
bool terminando() { return g_salir; }

void avisar(const std::string& s, bool malo)
{
    g_aviso = s, g_avisoMalo = malo;
    if (malo)
        registro::apuntar("aviso: %s", s.c_str());
}
void cambiado(const std::string& que) { avisar(que + ". Falta guardar."); }

// --------------------------------------------------------------------------------------------------------- teclado
bool pedirNumero(const std::string& titulo, uint32_t& valor, uint32_t maximo, uint32_t minimo)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetStringLenMax(&kbd, 10);
    swkbdConfigSetHeaderText(&kbd, titulo.c_str());
    char inicial[16];
    snprintf(inicial, sizeof(inicial), "%u", valor);
    swkbdConfigSetInitialText(&kbd, inicial);
    char out[16] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc) || !out[0])
        return false;
    unsigned long v = strtoul(out, nullptr, 10);
    valor = (uint32_t)std::clamp<unsigned long>(v, minimo, maximo);
    return true;
}

bool pedirTexto(const std::string& titulo, const std::string& pista, std::string& valor, int largo)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, titulo.c_str());
    swkbdConfigSetGuideText(&kbd, pista.c_str());
    swkbdConfigSetStringLenMax(&kbd, largo);
    swkbdConfigSetInitialText(&kbd, valor.c_str());
    char out[512] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc))
        return false;
    valor = out;
    return true;
}

std::string numero(uint32_t n)
{
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert(i, ".");
    return s;
}


// ------------------------------------------------------------------------------------------------ iconos y recuadros
SDL_Texture* iconoPara(const std::string& t)
{
    // The first word that matches (more specific ones first). Icons of NHSE's menus, as the Mac uses them.
    static const std::pair<const char*, const char*> PALABRAS[] = {
        {"criaturas marinas", "Tako"}, {"marina", "Tako"}, {"pez", "FishAji"}, {"peces", "FishAji"}, {"bicho", "InsMonshirocho"},
        {"fosil", "Fossil"}, {"arte", "Haniwa"}, {"obra", "Haniwa"}, {"receta", "BookRecipe"}, {"fabricad", "DIYWoodNormal"},
        {"favorit", "StarPiece"}, {"olvidar", "Can"}, {"vaciar", "Can"}, {"borrar", "Can"}, {"quitar", "Can"}, {"retirar", "Can"},
        {"aprender", "DIYRecipePlus"}, {"catalogo", "Book"}, {"todo", "Book"}, {"mueble", "Cardboard"}, {"pared", "Wall"},
        {"suelo", "Floor"}, {"alfombra", "Carpet"}, {"ropa", "Onepiece"}, {"prenda", "Onepiece"}, {"herramienta", "Axe"},
        {"material", "DIYWoodNormal"}, {"comida", "Dishes"}, {"planta", "PltCosmos0"}, {"flor", "PltCosmos0"}, {"otros", "Present"},
        {"reaccion", "Love"}, {"rueda", "Love"}, {"record", "StarPiece"}, {"sello", "StarPiece"}, {"carta", "Post"},
        {"correo", "Post"}, {"buzon", "Post"}, {"regalo", "Present"}, {"envolver", "Present"}, {"tejado", "Porch"},
        {"fachada", "Porch"}, {"puerta", "Porch"}, {"casa", "Porch"}, {"amistad", "Love"}, {"amigos", "Love"},
        {"muletilla", "Megaphone"}, {"saludo", "Megaphone"}, {"personalidad", "Leaf"}, {"bricolaje", "DIYWoodNormal"},
        {"marcas", "Book"}, {"progreso", "Book"}, {"diseno", "MyDesignPro"}, {"exportar", "Cardboard"}, {"importar", "Cardboard"},
        {"guardar todos", "Cardboard"}, {"mudar", "YellowPaperBag"}, {"recuerdo", "Book"}, {"fecha", "Timer"}, {"dia", "Timer"},
        {"hemisferio", "PltPalm"}, {"aerodromo", "PlaneTicket"}, {"fruta", "Peach"}, {"museo", "Fossil"}, {"nabo", "Kabu"},
        {"tendencia", "BadKabu"}, {"visitante", "Balloon"}, {"concierto", "Music"}, {"tablon", "Megaphone"}, {"nota", "Megaphone"},
        {"monedero", "MoneyBag039"}, {"banco", "MoneyBag069"}, {"ahorro", "MoneyBag069"}, {"bayas", "MoneyBag039"}, {"millas", "Coin"},
        {"poki", "Coin"}, {"billete", "RollanTicket"}, {"perfil", "BdayCupcake"}, {"cumpleanos", "BdayCupcake"}, {"nombre", "Haniwa"},
        {"jugador", "Onepiece"}, {"isla", "PltPalm"}, {"clima", "Syabon"}, {"comprobar", "Timer"}, {"memoria", "ToolChangeStick"},
        {"bruto", "ToolChangeStick"}, {"hueco", "Bag"}, {"bolsillo", "Bag"}, {"almacen", "Cardboard"}, {"papelera", "Can"},
        {"mapa", "Fence"}, {"acampada", "Tent"}, {"campista", "Tent"}, {"color", "Syabon"}, {"tela", "Onepiece"}, {"vecino", "Leaf"},
        {"cambiar por otro", "Leaf"}, {"fosiles", "Fossil"},
    };
    std::string n = acnh::normalizar(t);
    for (const auto& p : PALABRAS)
    {
        const size_t i = n.find(p.first);
        // At the start of a word.
        if (i != std::string::npos && (i == 0 || !isalnum((unsigned char)n[i - 1])))
            return dibujos::menu(p.second);
    }
    return nullptr;
}

int panel(const SDL_Rect& r, const std::string& titulo, const char* icono, const std::string& explicacion)
{
    redondo(r, 14, {0xD1, 0xD1, 0xD6, 255});
    redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 12, BLANCO);
    int x = r.x + 24;
    if (SDL_Texture* t = icono ? dibujos::menu(icono) : nullptr)
    {
        SDL_Rect d = {r.x + 20, r.y + 18, 52, 52};
        SDL_RenderCopy(g_r, t, nullptr, &d);
        x += 64;
    }
    int y = r.y + 16;
    y += texto(encajar(titulo, r.x + r.w - x - 20, g_negrita), x, y, MARRON, g_negrita) - 4;
    if (!explicacion.empty())
        y += texto(explicacion, x, y, MARRON_SUAVE, g_pequena, Izquierda, r.x + r.w - x - 20);
    return std::max(y, icono ? r.y + 78 : y) + 10;
}


// ------------------------------------------------------------------------------------------ buscador en tiempo real
Pantalla buscador(Buscador bu)
{
    struct Estado {
        std::string texto;
        std::string filtrado = "\x01"; // the text of the last search (none yet)
        int total = 0;
        int pagina = 0;
    };
    auto e = std::make_shared<Estado>();
    e->texto = bu.texto;
    return [bu, e](Vista& v) {
        if (e->filtrado != e->texto)
        {
            e->total = bu.filtrar(e->texto);
            e->filtrado = e->texto;
            e->pagina = 0;
        }
        constexpr int COLS = 2, FILAS = 4, POR_PAGINA = COLS * FILAS;
        const int paginas = std::max(1, (e->total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = bu.titulo;
        v.explicacion = bu.explicacion;
        // What is written, and how many there are.
        const std::string escrito = e->texto;
        const std::string cuantos = e->texto.empty() ? "Escribe para buscar" : e->total == 0 ? "Nada" : numero((uint32_t)e->total) + (e->total == 1 ? " resultado" : " resultados") +
            (paginas > 1 ? " · página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) : "");
        v.dibujo = [escrito, cuantos] {
            redondo({60, 108, W - 120, 52}, 12, AZUL);
            redondo({62, 110, W - 124, 48}, 10, BLANCO);
            const int ancho = anchoTexto(escrito);
            texto(escrito, 80, 134 - TTF_FontHeight(g_fuente) / 2, MARRON);
            // The cursor blinks.
            if ((SDL_GetTicks() / 500) % 2 == 0)
                rect({80 + ancho + 2, 118, 3, 32}, AZUL);
            texto(cuantos, W - 80, 134 - TTF_FontHeight(g_pequena) / 2, MARRON_SUAVE, g_pequena, Derecha);
        };
        // The results.
        for (int k = 0; k < POR_PAGINA && e->pagina * POR_PAGINA + k < e->total; k++)
        {
            Fila f = bu.fila(e->pagina * POR_PAGINA + k);
            Boton b{{60 + (k % COLS) * 590, 172 + (k / COLS) * 52, 570, 46}, f.texto, f.accion, f.activo, f.marcado, f.detalle};
            b.imagen = f.imagen;
            b.color = f.color;
            v.b.push_back(b);
        }
        // The keyboard.
        static const char* const FILAS_TECLADO[4] = {"1234567890", "qwertyuiop", "asdfghjklñ", "zxcvbnm,.-"};
        const int arriba = 172 + FILAS * 52 + 10;
        for (int f = 0; f < 4; f++)
        {
            const std::string fila = FILAS_TECLADO[f];
            int col = 0;
            for (size_t i = 0; i < fila.size(); col++)
            {
                // One letter (UTF-8: the ñ is two bytes).
                const size_t n = ((unsigned char)fila[i] & 0xE0) == 0xC0 ? 2 : 1;
                const std::string letra = fila.substr(i, n);
                i += n;
                Boton b{{60 + col * 117, arriba + f * 50, 109, 44}, letra, [e, letra] { e->texto += letra; }};
                v.b.push_back(b);
            }
        }
        auto borrar = [e] {
            if (e->texto.empty())
                return;
            size_t i = e->texto.size() - 1;
            while (i > 0 && ((unsigned char)e->texto[i] & 0xC0) == 0x80)
                i--;
            e->texto.erase(i);
        };
        const int y = arriba + 4 * 50;
        v.b.push_back({{60, y, 226, 44}, "Borrar", borrar, !e->texto.empty()});
        v.b.push_back({{294, y, 460, 44}, "Espacio", [e] { e->texto += " "; }});
        v.b.push_back({{762, y, 226, 44}, "Teclado…", [e] {
            std::string t = e->texto;
            if (pedirTexto("Buscar", "Escribe parte del nombre", t))
                e->texto = t;
        }});
        v.b.push_back({{996, y, 224, 44}, "Volver", [bu, e] {
            volver();
            if (bu.alSalir)
                bu.alSalir(e->texto);
        }});
        v.atras = [bu, e] {
            volver();
            if (bu.alSalir)
                bu.alSalir(e->texto);
        };
        v.x = borrar;
        v.y = [e] { e->texto += " "; };
        if (paginas > 1)
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        v.teclas = {{"A", "Escribir"}, {"X", "Borrar"}, {"Y", "Espacio"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Más resultados"});
        v.teclas.push_back({"B", "Volver"});
    };
}

// ------------------------------------------------------------------------------------------- pantallas que se repiten
Pantalla lista(Lista l)
{
    auto pagina = std::make_shared<int>(0);
    return [l, pagina](Vista& v) {
        const int total = l.cuantas();
        const int porPagina = l.columnas * l.porColumna;
        const int paginas = std::max(1, (total + porPagina - 1) / porPagina);
        int& p = *pagina;
        p = std::clamp(p, 0, paginas - 1);
        v.titulo = l.titulo;
        v.explicacion = l.explicacion ? l.explicacion() : "";
        if (paginas > 1)
        {
            while (!v.explicacion.empty() && (v.explicacion.back() == '.' || v.explicacion.back() == ' '))
                v.explicacion.pop_back();
            v.explicacion += (v.explicacion.empty() ? std::string("P") : std::string(" · p")) + "ágina " + std::to_string(p + 1) + " de " + std::to_string(paginas) + ".";
        }
        // Rows between y=140 and the bottom buttons.
        const int arriba = 140, abajoFilas = ABAJO - 72;
        const int hueco = 8;
        const int cabe = (abajoFilas - arriba) / l.porColumna - hueco;
        const int alto = std::min(l.alto > 0 ? l.alto : 70, cabe);
        const int ancho = (W - 120 - (l.columnas - 1) * 20) / l.columnas;
        for (int k = 0; k < porPagina && p * porPagina + k < total; k++)
        {
            Fila f = l.fila(p * porPagina + k);
            Boton bt{{60 + (k / l.porColumna) * (ancho + 20), arriba + (k % l.porColumna) * (alto + hueco), ancho, alto}, f.texto, f.accion, f.activo, f.marcado, f.detalle};
            bt.color = f.color;
            bt.imagen = f.imagen;
            v.b.push_back(bt);
        }
        std::vector<Fila> abajo;
        if (paginas > 1)
        {
            abajo.push_back({"Anteriores", [pagina] { (*pagina)--; }, p > 0});
            abajo.push_back({"Siguientes", [pagina] { (*pagina)++; }, p + 1 < paginas});
        }
        for (const auto& a : l.abajo)
            abajo.push_back(a());
        abajo.push_back({"Volver", [] { volver(); }});
        // Each one as wide as its text needs (and «Volver» on the right), sharing the width.
        const int n = (int)abajo.size();
        std::vector<int> anchos(n);
        int suma = 0;
        for (int i = 0; i < n; i++)
            suma += anchos[i] = std::max(150, anchoTexto(abajo[i].texto) + 48);
        const int libre = W - 120 - (n - 1) * 14;
        TTF_Font* letra = g_fuente;
        if (suma > libre)
        {
            // Smaller letters, and widths in proportion.
            suma = 0;
            for (int i = 0; i < n; i++)
                suma += anchos[i] = std::max(110, anchoTexto(abajo[i].texto, g_pequena) + 32);
            letra = g_pequena;
        }
        const double k = suma > libre ? (double)libre / suma : 1.0;
        int x = 60;
        for (int i = 0; i < n; i++)
        {
            const int w = (int)(anchos[i] * k);
            const int xi = i == n - 1 ? W - 60 - w : x;
            Boton b{{xi, ABAJO - 66, w, 56}, abajo[i].texto, abajo[i].accion, abajo[i].activo, abajo[i].marcado};
            b.pequena = letra == g_pequena;
            v.b.push_back(b);
            x += w + 14;
        }
        if (paginas > 1)
            v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + d, 0, paginas - 1); };
        if (l.extra)
            l.extra(v);
    };
}

void elegir(const std::string& titulo, const std::string& explicacion, const std::vector<std::string>& opciones, int actual,
    std::function<void(int)> elegido, int columnas)
{
    Lista l;
    l.titulo = titulo;
    l.explicacion = [explicacion] { return explicacion; };
    l.cuantas = [opciones] { return (int)opciones.size(); };
    l.fila = [opciones, actual, elegido](int i) {
        return Fila{opciones[i], [elegido, i] {
            volver();
            elegido(i);
        }, true, i == actual};
    };
    l.columnas = columnas;
    // Few options: fewer rows per column, so they are taller.
    l.porColumna = std::clamp((int)(opciones.size() + columnas - 1) / columnas, 4, 8);
    const int porPagina = l.columnas * l.porColumna;
    abrir(lista(l), actual >= 0 && actual < (int)opciones.size() ? actual % porPagina : 0);
}

void confirmar(const std::string& titulo, const std::string& pregunta, const std::string& si, std::function<void()> accion)
{
    abrir([=](Vista& v) {
        v.titulo = titulo;
        v.explicacion = pregunta;
        Boton s{{60, 150, 570, 90}, si, [accion] {
            volver();
            accion();
        }};
        v.b.push_back(s);
        v.b.push_back({{650, 150, 570, 90}, "No", [] { volver(); }});
    }, 1);
}

Pantalla marcas(Marcas m)
{
    struct Estado {
        std::string busqueda;
        std::vector<int> vistas; // the indexes shown
    };
    auto e = std::make_shared<Estado>();
    auto filtrar = [m, e] {
        e->vistas.clear();
        std::vector<std::string> palabras;
        std::string w;
        for (char c : acnh::normalizar(e->busqueda) + " ")
        {
            if (c == ' ')
            {
                if (!w.empty())
                    palabras.push_back(w);
                w.clear();
            }
            else
                w += c;
        }
        for (int i = 0; i < m.total; i++)
        {
            if (palabras.empty())
            {
                e->vistas.push_back(i);
                continue;
            }
            const std::string clave = m.clave ? m.clave(i) : acnh::normalizar(m.nombre(i));
            bool todas = true;
            for (const auto& p : palabras)
                todas = todas && clave.find(p) != std::string::npos;
            if (todas)
                e->vistas.push_back(i);
        }
    };
    filtrar();
    auto buscar = [m, e, filtrar] {
        // While writing, the ones that match (they can be marked there); on going back, the list stays with them.
        auto vistas = std::make_shared<std::vector<int>>();
        Buscador bu;
        bu.titulo = "Buscar en " + m.titulo;
        bu.explicacion = "Toca una para marcarla o desmarcarla.";
        bu.texto = e->busqueda;
        bu.filtrar = [m, vistas](const std::string& t) {
            vistas->clear();
            const std::string q = acnh::normalizar(t);
            for (int i = 0; i < m.total; i++)
                if (q.empty() || (m.clave ? m.clave(i) : acnh::normalizar(m.nombre(i))).find(q) != std::string::npos)
                    vistas->push_back(i);
            return (int)vistas->size();
        };
        bu.fila = [m, vistas](int k) {
            const int i = (*vistas)[k];
            const bool si = m.marcada(i);
            Fila f{(si ? "● " : "") + m.nombre(i), [m, i, si] { m.marcar(i, !si); }, true, si};
            if (m.imagen)
                f.imagen = m.imagen(i);
            return f;
        };
        bu.alSalir = [e, filtrar](const std::string& t) {
            e->busqueda = t;
            filtrar();
        };
        abrir(buscador(bu), 11);
    };
    Lista l;
    l.titulo = m.titulo;
    l.explicacion = [m, e] {
        int marcadas = 0;
        for (int i : e->vistas)
            marcadas += m.marcada(i);
        std::string s = e->busqueda.empty() ? "" : "«" + e->busqueda + "»: ";
        return s + numero(marcadas) + " de " + numero((uint32_t)e->vistas.size()) + " " + m.queEs + " marcadas.";
    };
    l.cuantas = [e] { return (int)e->vistas.size(); };
    l.fila = [m, e](int k) {
        const int i = e->vistas[k];
        const bool si = m.marcada(i);
        Fila f{(si ? "● " : "") + m.nombre(i), [m, i, si] { m.marcar(i, !si); }, true, si};
        if (m.imagen)
            f.imagen = m.imagen(i);
        return f;
    };
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.abajo.push_back([m, e] {
        return Fila{"Marcar todas", [m, e] {
            int n = 0;
            for (int i : e->vistas)
                if (!m.marcada(i))
                    m.marcar(i, true), n++;
            cambiado(std::to_string(n) + " marcadas");
        }};
    });
    l.abajo.push_back([m, e] {
        return Fila{"Desmarcar todas", [m, e] {
            const auto vistas = e->vistas;
            confirmar(m.titulo, "¿Desmarcar las " + numero((uint32_t)vistas.size()) + " " + m.queEs + " que se ven?", "Desmarcar", [m, vistas] {
                for (int i : vistas)
                    if (m.marcada(i))
                        m.marcar(i, false);
                cambiado("Desmarcadas");
            });
        }};
    });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Marcar"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

// ----------------------------------------------------------------------------------------------------------- bucle
std::vector<Tarjeta>& tarjetas()
{
    static std::vector<Tarjeta> t;
    return t;
}

void bucle(Pantalla inicio)
{
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    g_pila = {{inicio, 0}};

    u64 repiteTecla = 0;
    u32 repiteDesde = 0;
    bool touchAntes = false;

    while (!g_salir && appletMainLoop())
    {
        g_fotograma++;
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        const u64 held = padGetButtons(&pad);
        const u64 flechas = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
        if (down & flechas)
            repiteTecla = down & flechas, repiteDesde = SDL_GetTicks() + 350;
        else if ((held & repiteTecla) && SDL_GetTicks() >= repiteDesde)
            down |= repiteTecla & held, repiteDesde = SDL_GetTicks() + 80;
        HidTouchScreenState ts = {};
        hidGetTouchScreenStates(&ts, 1);
        const bool toque = ts.count > 0 && !touchAntes;
        touchAntes = ts.count > 0;
        int tx = toque ? (int)ts.touches[0].x : -1, ty = toque ? (int)ts.touches[0].y : -1;

        Vista v;
        Entrada actual = g_pila.back();
        const u32 antes = SDL_GetTicks();
        actual.p(v);
        // The log: each new screen, and the slow ones (to know where it hangs or goes slow on the console).
        {
            static std::string ultima;
            static u32 inicioFotograma = 0;
            const u32 ahora = SDL_GetTicks();
            if (v.titulo != ultima)
            {
#ifdef __SWITCH__
                u64 usada = 0, total = 0;
                svcGetInfo(&usada, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
                svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
                registro::apuntar("pantalla: %s (memoria %lu de %lu MB)", v.titulo.c_str(), (unsigned long)(usada >> 20), (unsigned long)(total >> 20));
#else
                registro::apuntar("pantalla: %s", v.titulo.c_str());
#endif
                ultima = v.titulo;
            }
            else if (inicioFotograma && ahora - inicioFotograma > 250)
                registro::apuntar("lento: %u ms en «%s» (la pantalla tardó %u ms)", ahora - inicioFotograma, v.titulo.c_str(), ahora - antes);
            inicioFotograma = ahora;
        }
        // The menus get the icon that goes with their text, as the Mac (only buttons with two lines or cards, without
        // their own picture).
        for (auto& b : v.b)
            if (!b.imagen && b.color.a == 0 && !b.casilla && !b.vertical && b.valor.empty() && (b.grande || !b.detalle.empty()) && b.r.w >= 260 && b.r.h >= 50)
                b.imagen = iconoPara(b.texto);
        // The side bar is part of every screen: its entries go after the buttons of the screen, at their place in
        // the coordinates of the screens (to the left of 0), so the D-pad reaches them from the left.
        const int propios = (int)v.b.size();
        Lateral lat;
        std::vector<HuecoLateral> hl;
        if (g_lateral)
        {
            lat = g_lateral();
            hl = huecosLateral(lat);
            // Screens with their own controls (the map, the rooms) leave the D-pad to them: the bar is only touched.
            if (!v.entrada)
                for (const auto& h : hl)
                {
                    Boton b{{aVirtualX(h.r.x), aVirtualY(h.r.y), (int)(h.r.w / ESCALA), (int)(h.r.h / ESCALA)}, "", h.accion, h.activo};
                    v.b.push_back(b);
                }
        }
        int& foco = g_pila.back().foco;
        // A screen with no buttons of its own: no cursor until the D-pad goes to the side bar.
        if (!g_pila.back().vista && propios == 0)
            foco = -1;
        g_pila.back().vista = true;
        if (foco >= (int)v.b.size() || (foco < 0 && propios > 0))
            foco = propios > 0 ? 0 : -1;

        // A touch: on the side bar, or (in the coordinates of the screens) on the screen.
        int vx = -1, vy = -1, pulsadoLateral = -1;
        if (tx >= 0)
        {
            if (tx < LAT)
            {
                for (size_t i = 0; i < hl.size(); i++)
                    if (tx >= hl[i].r.x && tx < hl[i].r.x + hl[i].r.w && ty >= hl[i].r.y && ty < hl[i].r.y + hl[i].r.h)
                        pulsadoLateral = propios + (int)i;
            }
            else if (ty < ABAJO)
                vx = aVirtualX(tx), vy = aVirtualY(ty);
        }

        auto accion = [&](const std::function<void()>& f) {
            g_aviso.clear();
            f();
        };
        if (g_explorar && g_lateral && ++g_pasos < 200000)
        {
            // A new screen: explore it (if it has not been seen).
            std::string firma = v.titulo;
            for (int i = 0; i < propios; i++)
                firma += "|" + v.b[i].texto;
            // Every screen (and every page of it) is drawn at least once before going on, to check it.
            const bool sinVer = g_vistas.insert(firma).second;
            if (g_exploracion.empty() || g_exploracion.back().profundidad < g_pila.size())
                g_exploracion.push_back({g_pila.size(), sinVer ? 0 : 1 << 30});
            while (!g_exploracion.empty() && g_exploracion.back().profundidad > g_pila.size())
                g_exploracion.pop_back();
            auto& ex = g_exploracion.back();
            auto prohibido = [&](const Boton& b) {
                return b.texto == "Salir" || b.texto == "Salir sin guardar" || b.texto == "Guardar" || b.texto.rfind("Exportar", 0) == 0 ||
                    b.texto.rfind("Guardar ", 0) == 0;
            };
            while (ex.siguiente < propios && (!v.b[ex.siguiente].activo || !v.b[ex.siguiente].accion || prohibido(v.b[ex.siguiente])))
                ex.siguiente++;
            const std::string titulo = v.titulo;
            const bool foto = g_fotos.insert(titulo).second;
            if (sinVer && !foto)
            {
                // Only drawn this frame.
            }
            else if (foto)
            {
                // First the photo (this frame is only drawn); the next frame goes on.
                char n[16];
                snprintf(n, sizeof(n), "%03d", (int)g_fotos.size());
                g_hacerFoto = std::string(g_explorar) + "/" + n + ".bmp";
                fprintf(stderr, "FOTO %s [%s]\n", n, titulo.c_str());
            }
            else if (ex.siguiente < propios && ex.profundidad < 6)
            {
                const int i = ex.siguiente++;
                fprintf(stderr, "EXPLORAR %zu [%s] → «%s»\n", ex.profundidad, titulo.c_str(), v.b[i].texto.c_str());
                foco = i;
                accion(v.b[i].accion);
            }
            else if (g_pila.size() > 1)
                accion([] { volver(); });
            else if (++g_seccionExplorada < (int)lat.entradas.size())
            {
                g_exploracion.clear();
                accion(lat.entradas[g_seccionExplorada].accion);
            }
            else
                g_salir = true;
            // Nothing else this frame (the action may have changed the stack).
            if (!g_pendiente.empty() || g_salir)
            {
                auto p = std::move(g_pendiente);
                g_pendiente.clear();
                for (auto& f : p)
                    f();
                continue;
            }
            down = 0;
            tx = ty = vx = vy = -1;
            pulsadoLateral = -1;
        }
        bool usada = pulsadoLateral < 0 && v.entrada && v.entrada(down, held, vx, vy);
        if (pulsadoLateral >= 0)
            usada = false;
        if (!usada)
        {
            auto mover = [&](int dx, int dy) {
                if (v.b.empty())
                    return;
                if (foco < 0)
                {
                    foco = propios;
                    return;
                }
                const SDL_Rect& a = v.b[foco].r;
                const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
                int mejor = -1;
                long mejorPeso = 0;
                for (int i = 0; i < (int)v.b.size(); i++)
                {
                    if (i == foco)
                        continue;
                    const SDL_Rect& r = v.b[i].r;
                    const int bx = r.x + r.w / 2, by = r.y + r.h / 2;
                    const long lejos = dx ? (long)(bx - ax) * dx : (long)(by - ay) * dy;
                    if (lejos <= 4)
                        continue;
                    long cruce = dx ? std::abs(by - ay) : std::abs(bx - ax);
                    const bool solapan = dx ? (r.y < a.y + a.h && a.y < r.y + r.h) : (r.x < a.x + a.w && a.x < r.x + r.w);
                    if (solapan)
                        cruce /= 4;
                    // Up and down do not jump between the side bar and the screen.
                    if (dy && ((i >= propios) != (foco >= propios)))
                        continue;
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

            int pulsado = pulsadoLateral < (int)v.b.size() ? pulsadoLateral : -1;
            if (pulsadoLateral >= (int)v.b.size() && hl[pulsadoLateral - propios].activo)
            {
                accion(hl[pulsadoLateral - propios].accion);
                pulsadoLateral = -1;
            }
            if ((down & HidNpadButton_A) && foco >= 0)
                pulsado = foco;
            for (int i = 0; i < propios && vx >= 0; i++)
                if (vx >= v.b[i].r.x && vx < v.b[i].r.x + v.b[i].r.w && vy >= v.b[i].r.y && vy < v.b[i].r.y + v.b[i].r.h)
                    pulsado = foco = i;
            if (pulsado >= 0 && pulsado < (int)v.b.size() && v.b[pulsado].activo && v.b[pulsado].accion)
                accion(v.b[pulsado].accion);
            else if (down & HidNpadButton_B)
                accion(v.atras ? v.atras : [] { volver(); });
            else if ((down & HidNpadButton_X) && v.x)
                accion(v.x);
            else if ((down & HidNpadButton_Y) && v.y)
                accion(v.y);
            else if ((down & HidNpadButton_Y) && g_lateral && lat.hayCambios && !v.y)
                accion(lat.guardar); // Y saves everywhere (if the screen does not use it)
            else if ((down & HidNpadButton_Minus) && v.menos)
                accion(v.menos);
            else if (v.paginar && (down & (HidNpadButton_L | HidNpadButton_R)))
                v.paginar(down & HidNpadButton_L ? -1 : 1);
            else if (v.paginar && (down & (HidNpadButton_ZL | HidNpadButton_ZR)))
                v.paginar(down & HidNpadButton_ZL ? -10 : 10);
            else if ((down & HidNpadButton_Plus) && g_lateral)
                accion(lat.salir);
            else if ((down & HidNpadButton_Plus) && g_pila.size() == 1 && v.atras)
                accion(v.atras);
        }

        if (!g_pendiente.empty() || g_salir)
        {
            // The stack changed: the next frame shows the new screen.
            auto p = std::move(g_pendiente);
            g_pendiente.clear();
            for (auto& f : p)
                f();
            continue;
        }

        rect({0, 0, W, H}, CREMA);
        // The screen, a bit smaller, to the right of the side bar.
        SDL_RenderSetScale(g_r, ESCALA, ESCALA);
        SDL_Rect vista = {(int)std::lround(LAT / ESCALA), (int)std::lround(ARRIBA_REAL / ESCALA), W, ABAJO};
        SDL_RenderSetViewport(g_r, &vista);
        g_enPantalla = true;
        if (g_revisar)
        {
            g_pantallaActual = v.titulo;
            for (int i = 0; i < propios; i++)
            {
                const SDL_Rect& a = v.b[i].r;
                const std::string n = v.b[i].texto.empty() ? "(" + std::to_string(i) + ")" : v.b[i].texto;
                if (a.x < 0 || a.y < 0 || a.x + a.w > W || a.y + a.h > ABAJO)
                    revisar("botón fuera de la pantalla", n);
                for (int j = i + 1; j < propios; j++)
                {
                    SDL_Rect x;
                    if (SDL_IntersectRect(&a, &v.b[j].r, &x))
                        revisar("botones montados", n + "» y «" + (v.b[j].texto.empty() ? "(" + std::to_string(j) + ")" : v.b[j].texto));
                }
            }
        }
        texto(encajar(v.titulo, W - 120, g_titulo), 60, 14, MARRON, g_titulo);
        texto(v.explicacion, 60, 78, MARRON_SUAVE, g_pequena, Izquierda, 1160);
        if (v.dibujo)
            v.dibujo();
        for (int i = 0; i < propios; i++)
            dibujar(v.b[i], i == foco);
        if (g_revisar)
        {
            // Texts drawn outside the buttons, under a button or on top of each other.
            for (size_t k = 0; k < g_libres.size(); k++)
            {
                SDL_Rect x;
                for (int i = 0; i < propios; i++)
                    if (SDL_IntersectRect(&g_libres[k].first, &v.b[i].r, &x) && x.w > 2 && x.h > 2)
                        revisar("texto debajo de un botón", g_libres[k].second + "» bajo «" + v.b[i].texto);
                for (size_t j = k + 1; j < g_libres.size(); j++)
                    if (SDL_IntersectRect(&g_libres[k].first, &g_libres[j].first, &x) && x.w > 2 && x.h > 2)
                        revisar("textos montados", g_libres[k].second + "» y «" + g_libres[j].second);
            }
        }
        g_libres.clear();
        g_enPantalla = false;
        SDL_RenderSetViewport(g_r, nullptr);
        SDL_RenderSetScale(g_r, 1, 1);
        if (g_lateral)
            dibujarLateral(lat, foco >= propios && foco < (int)v.b.size() ? foco - propios : -1);
        Teclas teclas = v.teclas;
        if (teclas.empty())
        {
            teclas.push_back({"A", "Elegir"});
            if (v.paginar)
                teclas.push_back({"L R", "Página"});
            teclas.push_back({"B", "Volver"});
        }
        if (!g_aviso.empty())
            barra(g_aviso, g_avisoMalo, false, teclas);
        else
            barra(g_pila.size() == 1 ? g_avisoFijo : "", false, true, teclas);
        if (!g_hacerFoto.empty())
        {
            SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(g_r, nullptr, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch);
            SDL_SaveBMP(sf, g_hacerFoto.c_str());
            SDL_FreeSurface(sf);
            g_hacerFoto.clear();
        }
        SDL_RenderPresent(g_r);
    }
    for (auto& l : g_letras)
        SDL_DestroyTexture(l.second.t);
    g_letras.clear();
}

// The home screen sets the message shown when there is none.
void avisoFijo(const std::string& s) { g_avisoFijo = s; }

} // namespace ui
