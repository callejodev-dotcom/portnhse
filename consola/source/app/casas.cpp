// Sección «Isla»: las casas de los jugadores, como HouseWindow de NHSE para Mac: elegir la casa y la habitación, verla
// desde arriba con sus muebles, lo que hay sobre las mesas, alfombras, paredes y techo; un cursor por cuadros para ver
// qué hay, quitarlo, girarlo, moverlo, cambiarlo o poner cosas nuevas (la misma pantalla de un sitio que el mapa); el
// papel pintado y el suelo de cada habitación; el tamaño; la casa por fuera con su tejado, fachada y puerta; y las
// opciones avanzadas (los sucesos de la casa, los valores sueltos y pasar la casa o una habitación a la tarjeta SD, en
// el mismo formato que NHSE). Con los dibujos de los objetos, como RoomView del Mac.
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include "../core/casas.hpp"
#include "../core/mapa_casas.hpp"
#include "casas_fuera.hpp"
#include "dibujos.hpp"
#include "mapa_panel.hpp"
#include "mapa_pintor.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace ui;
using namespace mapa;
using acnh::Aspecto;
using acnh::FACHADAS;
using acnh::NOMBRES_FACHADA;
using acnh::NOMBRES_PUERTA;
using acnh::NOMBRES_TEJADO;
using acnh::Objeto;
using acnh::PUERTAS;
using acnh::TEJADOS;

namespace {

const SDL_Rect AREA = {0, 96, 880, ABAJO - 96};
constexpr int PANEL_X = 880;

// Lo que se edita (las capas de la habitación, en el orden del Mac: muebles, mesas, alfombras, paredes, techo).
struct Superficie {
    int capa;
    const char* nombre;
    const char* donde;
};
const Superficie SUPERFICIES[] = {
    {casas::Suelo, "Muebles", "En el suelo"},
    {casas::SobreMuebles, "Sobre las mesas", "Encima de un mueble"},
    {casas::Alfombras, "Alfombras", "Alfombra"},
    {casas::ParedFondo, "Pared del fondo", "En la pared · Pared del fondo"},
    {casas::ParedIzquierda, "Pared izquierda", "En la pared · Pared izquierda"},
    {casas::ParedEntrada, "Pared de la entrada", "En la pared · Pared de la entrada"},
    {casas::ParedDerecha, "Pared derecha", "En la pared · Pared derecha"},
    {casas::Techo, "Techo", "Colgado del techo"},
};
constexpr int NUM_SUPERFICIES = sizeof(SUPERFICIES) / sizeof(SUPERFICIES[0]);
const char* const AYUDAS[] = {"Todo lo que está en el suelo.", "Cosas pequeñas puestas encima de los muebles, como una lámpara sobre una mesa.",
    "Las alfombras van debajo de los muebles.", "Lo que está colgado en las paredes.", "Lo que está colgado en las paredes.",
    "Lo que está colgado en las paredes.", "Lo que está colgado en las paredes.", "Lámparas y cosas colgadas del techo."};

const char* const HABITACIONES[] = {"Habitación principal", "Habitación del fondo", "Habitación izquierda", "Habitación derecha",
    "Planta de arriba", "Sótano"};
const char* const HABITACIONES_MIN[] = {"habitación principal", "habitación del fondo", "habitación izquierda", "habitación derecha",
    "planta de arriba", "sótano"};
const char* const NIVELES[] = {"Tienda de campaña", "Casa", "Habitación principal ampliada", "Con habitación del fondo",
    "Con habitación izquierda", "Con habitación derecha", "Con planta de arriba", "Con sótano"};

enum Modo { Mirar, Llevar, Poniendo };

struct Estado {
    int casa = 0, hab = 0, sup = 0;
    int cx = 8, cy = 8; // cursor (casilla de la habitación, par)
    Modo modo = Mirar;
    Objeto llevado;
    int desdeX = 0, desdeY = 0, dx = 0, dy = 0;
    std::string aviso;
};

std::vector<uint8_t>& principal() { return partida::principal(); }

int numCasas() { return std::max(1, std::min((int)partida::jugadores.size(), casas::NUM_CASAS)); }
std::string nombreCasa(int c) { return "Casa de " + (c < (int)partida::jugadores.size() ? partida::jugadores[c].nombre : std::to_string(c + 1)); }

Rejilla rejilla(const Estado& e) { return casas::capa(principal(), e.casa, e.hab, SUPERFICIES[e.sup].capa); }
Zona limites(const Estado& e) { return casas::limites(casas::nivel(principal(), e.casa), e.hab, SUPERFICIES[e.sup].capa); }
bool esPared(const Estado& e) { return SUPERFICIES[e.sup].capa >= casas::ParedFondo && SUPERFICIES[e.sup].capa <= casas::ParedDerecha; }

void alCentro(Estado& e)
{
    const Zona l = limites(e);
    e.cx = (l.x + l.w / 2 - 1) & ~1;
    e.cy = (l.y + l.h / 2 - 1) & ~1;
}

void ponerHabitacion(Estado& e, int h)
{
    e.hab = h;
    e.modo = Mirar;
    e.aviso.clear();
    alCentro(e);
}

const char* nombreAspecto(const Aspecto* lista, int n, uint16_t v)
{
    for (int i = 0; i < n; i++)
        if (lista[i].valor == v)
            return lista[i].nombre;
    return "?";
}
#define NOMBRE_DE(lista, v) nombreAspecto(lista, sizeof(lista) / sizeof(lista[0]), v)

std::string nombrePared(const Objeto& o) { return o.id == mapa::NADA || o.id == 0 ? std::string("—") : nombre(o); }

// La cosa del cursor (cualquiera de las casillas del cuadro): su raíz.
bool cosaEn(const Estado& e, int x, int y, Objeto& o, int& rx, int& ry)
{
    Rejilla r = rejilla(e);
    for (int k = 0; k < 4; k++)
        if (raiz(r, x + (k & 1), y + (k >> 1), o, rx, ry))
            return true;
    return false;
}

// ------------------------------------------------------------------------------------------------------- dibujo
struct Medidas {
    SDL_Rect suelo;   // el suelo (o la cara de la pared) en la pantalla
    double u;         // puntos por casilla
    int x0, y0;       // la casilla de su esquina
    SDL_Rect cuarto;  // con las paredes alrededor
};

Medidas medir(const Estado& e)
{
    const Zona l = limites(e);
    const double fx = AREA.x + 12, fy = AREA.y + 12, fw = AREA.w - 24, fh = AREA.h - 24;
    Medidas m;
    m.x0 = l.x, m.y0 = l.y;
    if (esPared(e))
    {
        m.u = std::min(fw / l.w, fh / (l.h + 2.5));
        const int w = (int)(l.w * m.u), h = (int)(l.h * m.u);
        m.suelo = {(int)(fx + (fw - w) / 2), (int)(fy + (fh - h - 2 * m.u) / 2), w, h};
        m.cuarto = m.suelo;
        return m;
    }
    const double grosor = 1.1;
    m.u = std::min(fw / (l.w + 2 * grosor), fh / (l.h + 2 * grosor));
    const int rw = (int)((l.w + 2 * grosor) * m.u), rh = (int)((l.h + 2 * grosor) * m.u);
    m.cuarto = {(int)(fx + (fw - rw) / 2), (int)(fy + (fh - rh) / 2), rw, rh};
    const int g = (int)(grosor * m.u);
    m.suelo = {m.cuarto.x + g, m.cuarto.y + g, (int)(l.w * m.u), (int)(l.h * m.u)};
    return m;
}

SDL_Rect enPantalla(const Medidas& m, double x, double y, double w, double h)
{
    const int a = (int)std::lround(m.suelo.x + (x - m.x0) * m.u), b = (int)std::lround(m.suelo.y + (y - m.y0) * m.u);
    return {a, b, (int)std::lround(m.suelo.x + (x + w - m.x0) * m.u) - a, (int)std::lround(m.suelo.y + (y + h - m.y0) * m.u) - b};
}

SDL_Color mezclar(SDL_Color a, SDL_Color b, double t)
{
    return {(uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t), 255};
}

SDL_Color conAlfa(SDL_Color c, double a) { return {c.r, c.g, c.b, (uint8_t)(255 * a)}; }

enum Ficha { Mueble, SobreMesa, Colgado, Plana, Ajustada };

void marco(SDL_Rect r, SDL_Color c, int grosor)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < grosor; i++)
    {
        SDL_Rect q = {r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i};
        SDL_RenderDrawRect(g_r, &q);
    }
}


// --------------------------------------------------------------------------------------- dibujos de papeles y suelos
// ¿Se pueden usar los dibujos para el papel y el suelo? Solo con los de cada objeto (los genéricos son un icono).
bool dibujosDeVerdad() { return dibujos::completos(); }

// El color medio de una parte del dibujo de un objeto (el dibujo de un papel, el centro de una alfombra), sin lo
// transparente; como AverageColor del Mac. Se lee del dibujo dibujándolo pequeño.
SDL_Color colorMedio(uint16_t id, double cx, double cy, double cw, double ch, SDL_Color siNo)
{
    static std::map<uint64_t, SDL_Color> hechos;
    const uint64_t clave = ((uint64_t)id << 32) | ((uint64_t)(cx * 100) << 24) | ((uint64_t)(cy * 100) << 16) | ((uint64_t)(cw * 100) << 8) | (uint64_t)(ch * 100);
    auto it = hechos.find(clave);
    if (it != hechos.end())
        return it->second;
    SDL_Texture* t = dibujos::de(id);
    static SDL_Texture* lienzo = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    SDL_Color c = siNo;
    if (t && lienzo)
    {
        int w = 1, h = 1;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
        const SDL_Rect src = {(int)(w * cx), (int)(h * cy), std::max(1, (int)(w * cw)), std::max(1, (int)(h * ch))};
        SDL_Texture* antes = SDL_GetRenderTarget(g_r);
        SDL_SetRenderTarget(g_r, lienzo);
        SDL_SetRenderDrawColor(g_r, 0, 0, 0, 0);
        SDL_RenderClear(g_r);
        SDL_BlendMode modo;
        SDL_GetTextureBlendMode(t, &modo);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
        SDL_RenderCopy(g_r, t, &src, nullptr);
        SDL_SetTextureBlendMode(t, modo);
        uint32_t px[256];
        const bool leido = SDL_RenderReadPixels(g_r, nullptr, SDL_PIXELFORMAT_ARGB8888, px, 16 * 4) == 0;
        SDL_SetRenderTarget(g_r, antes);
        long r = 0, g = 0, b = 0, n = 0;
        for (int i = 0; leido && i < 256; i++)
            if ((px[i] >> 24) >= 200)
                r += (px[i] >> 16) & 255, g += (px[i] >> 8) & 255, b += px[i] & 255, n++;
        if (n)
            c = {(uint8_t)(r / n), (uint8_t)(g / n), (uint8_t)(b / n), 255};
    }
    return hechos[clave] = c;
}

// Llena una zona con el centro del dibujo de un suelo o un papel, repetido; uno de cada dos dado la vuelta, para que
// los bordes de los vecinos casen (TileTexture del Mac).
void mosaico(SDL_Texture* t, const SDL_Rect& zona, double lado, double cx, double cy, double cw, double ch, uint8_t alfa = 255)
{
    if (!t || lado <= 2)
        return;
    int w = 1, h = 1;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
    const SDL_Rect src = {(int)(w * cx), (int)(h * cy), std::max(1, (int)(w * cw)), std::max(1, (int)(h * ch))};
    const double alto = lado * src.h / src.w;
    SDL_RenderSetClipRect(g_r, &zona);
    SDL_SetTextureAlphaMod(t, alfa);
    int fila = 0;
    for (double y = zona.y; y < zona.y + zona.h; y += alto, fila++)
    {
        int col = 0;
        for (double x = zona.x; x < zona.x + zona.w; x += lado, col++)
        {
            const SDL_FRect d = {(float)x, (float)y, (float)(lado + 0.5), (float)(alto + 0.5)};
            const int vuelta = ((col & 1) ? SDL_FLIP_HORIZONTAL : 0) | ((fila & 1) ? SDL_FLIP_VERTICAL : 0);
            SDL_RenderCopyExF(g_r, t, &src, &d, 0, nullptr, (SDL_RendererFlip)vuelta);
        }
    }
    SDL_SetTextureAlphaMod(t, 255);
    SDL_RenderSetClipRect(g_r, nullptr);
}

// Una pared con su papel: el dibujo arriba, en filas pequeñas, y la parte de abajo (a menudo un zócalo de madera) con
// una moldura entre las dos (PaintWall del Mac).
void pintarPared(SDL_Texture* t, const SDL_Rect& pared)
{
    if (!t)
        return;
    int w = 1, h = 1;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
    const int bajo = (int)(pared.h * 0.36);
    const SDL_Rect arriba = {pared.x, pared.y, pared.w, pared.h - bajo}, abajo = {pared.x, pared.y + pared.h - bajo, pared.w, bajo};
    auto filas = [](double zona, double quiero) { return zona / std::max(1.0, std::round(zona / std::max(1.0, quiero))); };
    const double fa = filas(arriba.h, arriba.h / 4.0), fb = filas(abajo.h, abajo.h / 1.5);
    mosaico(t, arriba, fa * (w * 0.42) / (h * 0.25), 0.26, 0.26, 0.42, 0.25);
    mosaico(t, abajo, fb * (w * 0.42) / (h * 0.14), 0.26, 0.65, 0.42, 0.14);
    rect({pared.x, abajo.y - 1, pared.w, 3}, {0x20, 0x14, 0x08, 0x48});
}

// Un rectángulo redondeado con borde.
void conBorde(const SDL_Rect& r, int radio, SDL_Color borde, SDL_Color relleno)
{
    redondo(r, radio, borde);
    redondo({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, std::max(0, radio - 1), relleno);
}

void dibujoEn(SDL_Texture* t, SDL_Rect d, uint8_t alfa)
{
    SDL_SetTextureAlphaMod(t, alfa);
    SDL_RenderCopy(g_r, t, nullptr, &d);
    SDL_SetTextureAlphaMod(t, 255);
}

SDL_Rect encoger(SDL_Rect r, double m) { return {(int)(r.x + m), (int)(r.y + m), (int)(r.w - 2 * m), (int)(r.h - 2 * m)}; }

// El dibujo cuadrado más grande que cabe en «r», centrado.
SDL_Rect cuadrado(SDL_Rect r)
{
    const int l = std::min(r.w, r.h);
    return {r.x + (r.w - l) / 2, r.y + (r.h - l) / 2, l, l};
}

void fichas(const Estado& e, const Medidas& m, int capa, Ficha f, double alfa)
{
    if (alfa <= 0)
        return;
    Rejilla r = casas::capa(principal(), e.casa, e.hab, capa);
    struct Cosa {
        int x, y, w, h;
        Objeto o;
    };
    std::vector<Cosa> v;
    for (int x = 0; x < casas::LADO; x++)
        for (int y = 0; y < casas::LADO; y++)
        {
            const Objeto o = r.leer(x, y);
            if (esNada(o) || !esRaiz(o))
                continue;
            int w, h;
            tamano(o, w, h);
            v.push_back({x, y, w, h, o});
        }
    std::stable_sort(v.begin(), v.end(), [](const Cosa& a, const Cosa& b) { return a.y + a.h < b.y + b.h; });
    const uint8_t a8 = (uint8_t)(255 * alfa);
    for (const auto& c : v)
    {
        SDL_Rect q = enPantalla(m, c.x, c.y, c.w, c.h);
        const SDL_Color col = colorDe(c.o);
        const int borde = std::max(2, (int)(m.u * 0.08));
        if (SDL_Texture* foto = dibujoDe(c.o))
        {
            // Con su dibujo, como RoomView del Mac.
            const int menor = std::min(q.w, q.h);
            switch (f)
            {
                case Plana:
                {
                    // Las alfombras, planas: una zona de su color, del tamaño que ocupan, con su dibujo en medio.
                    const SDL_Color alfombra = dibujosDeVerdad() ? colorMedio(idVisible(c.o), 0.3, 0.3, 0.4, 0.4, {0xC8, 0xB4, 0x96, 255}) : col;
                    const SDL_Rect rr = encoger(q, 1);
                    const int radio = (int)(std::min(rr.w, rr.h) * 0.12);
                    conBorde(rr, radio, conAlfa(mezclar(alfombra, {0, 0, 0, 255}, 0.4), alfa), conAlfa(alfombra, 0.85 * alfa));
                    const int lado = (int)(std::min(rr.w, rr.h) * 0.7);
                    dibujoEn(foto, {rr.x + (rr.w - lado) / 2, rr.y + (rr.h - lado) / 2, lado, lado}, a8);
                    break;
                }
                case Colgado:
                {
                    const SDL_Rect rr = encoger(q, 1.5);
                    marco(rr, {0xFF, 0xFF, 0xFF, (uint8_t)(0xB0 * alfa)}, 2);
                    dibujoEn(foto, cuadrado(encoger(rr, menor * 0.1)), a8);
                    break;
                }
                case Ajustada:
                    dibujoEn(foto, cuadrado(encoger(q, menor * 0.04)), a8);
                    break;
                case SobreMesa:
                {
                    const SDL_Rect rr = encoger(q, 1.5);
                    const SDL_Rect t = encoger(rr, std::min(rr.w, rr.h) * 0.22);
                    const int radio = std::min(t.w, t.h) / 2;
                    redondo({t.x + 1, t.y + 2, t.w, t.h}, radio, conAlfa({0, 0, 0, 255}, 0.3 * alfa));
                    conBorde(t, radio, conAlfa({0x40, 0x30, 0x20, 255}, 0.45 * alfa), conAlfa({0xFF, 0xFF, 0xFF, 255}, 0.94 * alfa));
                    dibujoEn(foto, cuadrado(encoger(t, std::min(t.w, t.h) * 0.1)), a8);
                    break;
                }
                default:
                {
                    // Los muebles: una ficha clara del tamaño que ocupan, con su dibujo dentro.
                    const SDL_Rect rr = encoger(q, 1.5);
                    const int radio = (int)(std::min(rr.w, rr.h) * 0.16);
                    redondo({rr.x + 1, rr.y + 3, rr.w, rr.h}, radio, conAlfa({0, 0, 0, 255}, 0.22 * alfa));
                    conBorde(rr, radio, conAlfa({0x40, 0x30, 0x20, 255}, 0.44 * alfa), conAlfa({0xFF, 0xFC, 0xF5, 255}, 0.61 * alfa));
                    dibujoEn(foto, cuadrado(encoger(rr, std::min(rr.w, rr.h) * 0.06)), a8);
                    break;
                }
            }
            continue;
        }
        switch (f)
        {
            case Plana:
            {
                // Las alfombras: una zona de su color del tamaño que ocupan.
                q = {q.x + 1, q.y + 1, q.w - 2, q.h - 2};
                redondo(q, (int)(std::min(q.w, q.h) * 0.12), conAlfa(mezclar(col, {0, 0, 0, 255}, 0.4), alfa));
                redondo({q.x + borde, q.y + borde, q.w - 2 * borde, q.h - 2 * borde}, (int)(std::min(q.w, q.h) * 0.1), conAlfa(col, alfa * 0.85));
                break;
            }
            case Colgado:
            {
                q = {q.x + 3, q.y + 3, q.w - 6, q.h - 6};
                SDL_SetRenderDrawColor(g_r, col.r, col.g, col.b, (uint8_t)(255 * alfa));
                for (int i = 0; i < 3; i++)
                {
                    SDL_Rect k = {q.x + i, q.y + i, q.w - 2 * i, q.h - 2 * i};
                    SDL_RenderDrawRect(g_r, &k);
                }
                break;
            }
            default:
            {
                const int pad = f == SobreMesa ? std::max(3, (int)(m.u * 0.3)) : std::max(2, (int)(m.u * 0.08));
                q = {q.x + pad, q.y + pad, q.w - 2 * pad, q.h - 2 * pad};
                const int radio = (int)(std::min(q.w, q.h) * 0.18);
                redondo({q.x + 2, q.y + 3, q.w, q.h}, radio, conAlfa({0x10, 0x10, 0x10, 255}, 0.25 * alfa));
                redondo(q, radio, conAlfa(mezclar(col, {0, 0, 0, 255}, 0.38), alfa));
                redondo({q.x + borde, q.y + borde, q.w - 2 * borde, q.h - 2 * borde}, std::max(0, radio - borde), conAlfa(col, alfa));
                break;
            }
        }
    }
}

// Lo que cuelga de la pared del fondo, pequeño, en su sitio a lo largo de la pared (vista desde arriba).
void tiraPared(const Estado& e, const Medidas& m)
{
    Rejilla r = casas::capa(principal(), e.casa, e.hab, casas::ParedFondo);
    const int alto = m.suelo.y - m.cuarto.y;
    for (int x = 0; x < casas::LADO; x++)
        for (int y = 0; y < casas::LADO; y++)
        {
            const Objeto o = r.leer(x, y);
            if (esNada(o) || !esRaiz(o))
                continue;
            int w, h;
            tamano(o, w, h);
            if (SDL_Texture* foto = dibujoDe(o))
                dibujoEn(foto, cuadrado({(int)(m.suelo.x + (x - m.x0) * m.u) + 1, m.cuarto.y + 1, (int)(w * m.u) - 2, alto - 2}), 255);
        }
}

void flecha(const SDL_Rect& r, int giro)
{
    // Hacia dónde mira el mueble (0 abajo, 1 derecha, 2 arriba, 3 izquierda).
    const int dx = giro == 1 ? 1 : giro == 3 ? -1 : 0, dy = giro == 0 ? 1 : giro == 2 ? -1 : 0;
    const float cx = r.x + r.w / 2.f + dx * (r.w / 2.f + 1), cy = r.y + r.h / 2.f + dy * (r.h / 2.f + 1);
    redondo({(int)cx - 11, (int)cy - 11, 22, 22}, 11, BLANCO);
    const SDL_Color v = {0x2F, 0x9E, 0x62, 255};
    SDL_Vertex t[3] = {
        {{cx + dx * 7, cy + dy * 7}, v, {0, 0}},
        {{cx - dx * 4 - dy * 6, cy - dy * 4 + dx * 6}, v, {0, 0}},
        {{cx - dx * 4 + dy * 6, cy - dy * 4 - dx * 6}, v, {0, 0}},
    };
    SDL_RenderGeometry(g_r, nullptr, t, 3, nullptr, 0);
}

void dibujarHabitacion(Estado& e)
{
    const Medidas m = medir(e);
    const Objeto papel = casas::pared(principal(), e.casa, e.hab, false), suelo = casas::pared(principal(), e.casa, e.hab, true);
    const bool hayPapel = papel.id != mapa::NADA && papel.id != 0, haySuelo = suelo.id != mapa::NADA && suelo.id != 0;
    SDL_Texture* fotoPapel = hayPapel && dibujosDeVerdad() ? dibujoDe(papel) : nullptr;
    SDL_Texture* fotoSuelo = haySuelo && dibujosDeVerdad() ? dibujoDe(suelo) : nullptr;
    // Los colores primero (leerlos de un dibujo cambia dónde se dibuja).
    const SDL_Color colorPapel = !hayPapel ? SDL_Color{0xC9, 0xB8, 0x9C, 255}
        : fotoPapel ? colorMedio(papel.id, 0.26, 0.26, 0.42, 0.25, {0xC9, 0xB8, 0x9C, 255}) : mezclar(colorDe(papel), {0xEF, 0xE7, 0xD8, 255}, 0.35);
    const SDL_Color colorSuelo = !haySuelo ? SDL_Color{0xE2, 0xCF, 0xAE, 255} : mezclar(colorDe(suelo), {0xE2, 0xCF, 0xAE, 255}, 0.5);
    if (dibujosDeVerdad())
    {
        // Las alfombras también tienen su color medio: se mira antes de empezar a dibujar.
        Rejilla r = casas::capa(principal(), e.casa, e.hab, casas::Alfombras);
        for (int x = 0; x < casas::LADO; x++)
            for (int y = 0; y < casas::LADO; y++)
            {
                const Objeto o = r.leer(x, y);
                if (esRaiz(o))
                    colorMedio(o.id, 0.3, 0.3, 0.4, 0.4, {0xC8, 0xB4, 0x96, 255});
            }
    }
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    const int capa = SUPERFICIES[e.sup].capa;
    if (esPared(e))
    {
        // La pared de frente, con una tira de suelo debajo.
        const SDL_Rect tira = {m.suelo.x, m.suelo.y + m.suelo.h, m.suelo.w, (int)(m.u * 2)};
        rect(tira, colorSuelo);
        mosaico(fotoSuelo, tira, m.u * 3.8, 0.2, 0.2, 0.6, 0.6);
        rect(tira, {0, 0, 0, 0x30});
        rect(m.suelo, fotoPapel ? SDL_Color{0xEF, 0xE7, 0xD8, 255} : colorPapel);
        pintarPared(fotoPapel, m.suelo);
        SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_r, 0x30, 0x20, 0x10, 0x1C);
        for (int i = 1; i < m.suelo.w / std::max(1, (int)m.u); i++)
            SDL_RenderDrawLine(g_r, (int)(m.suelo.x + i * m.u), m.suelo.y, (int)(m.suelo.x + i * m.u), m.suelo.y + m.suelo.h);
        for (int i = 1; i < casas::FILAS_PARED; i++)
            SDL_RenderDrawLine(g_r, m.suelo.x, (int)(m.suelo.y + i * m.u), m.suelo.x + m.suelo.w, (int)(m.suelo.y + i * m.u));
        rect({m.suelo.x, m.suelo.y + m.suelo.h - (int)(m.u * 0.3), m.suelo.w, (int)(m.u * 0.3)}, {0x5A, 0x40, 0x28, 0x80});
        fichas(e, m, capa, Ajustada, 1);
    }
    else
    {
        // Un plano: el suelo con su forma, y las paredes como un marco del color del papel.
        redondo({m.cuarto.x + 3, m.cuarto.y + 6, m.cuarto.w, m.cuarto.h}, 8, {0, 0, 0, 0x30});
        conBorde(m.cuarto, 8, mezclar(colorPapel, {0, 0, 0, 255}, 0.5), mezclar(colorPapel, {0, 0, 0, 255}, 0.18));
        rect(m.suelo, colorSuelo);
        mosaico(fotoSuelo, m.suelo, m.u * 3.8, 0.2, 0.2, 0.6, 0.6);
        SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_r, 0x30, 0x20, 0x10, 0x30);
        const Zona l = limites(e);
        for (int i = 1; i < l.w / 2; i++)
            SDL_RenderDrawLine(g_r, (int)(m.suelo.x + i * 2 * m.u), m.suelo.y, (int)(m.suelo.x + i * 2 * m.u), m.suelo.y + m.suelo.h);
        for (int i = 1; i < l.h / 2; i++)
            SDL_RenderDrawLine(g_r, m.suelo.x, (int)(m.suelo.y + i * 2 * m.u), m.suelo.x + m.suelo.w, (int)(m.suelo.y + i * 2 * m.u));
        // Las paredes dan un poco de sombra al suelo.
        marco(encoger(m.suelo, m.u * 0.25 - 1), {0, 0, 0, 0x18}, std::max(1, (int)(m.u * 0.25)));
        // Las puertas: la entrada en medio de la pared de abajo de la principal; las demás, hacia la principal.
        const int ancho = (int)(2 * m.u), fondo = m.suelo.y - m.cuarto.y;
        const SDL_Color hueco = {0xE8, 0xDC, 0xC4, 255};
        const int mx = m.suelo.x + m.suelo.w / 2, my = m.suelo.y + m.suelo.h / 2;
        auto habs = casas::habitaciones(principal(), e.casa);
        const bool hayFondo = std::find(habs.begin(), habs.end(), 1) != habs.end();
        if (e.hab == 0 || e.hab == 1)
            rect({mx - ancho / 2, m.suelo.y + m.suelo.h, ancho, fondo}, hueco);
        if (e.hab == 0 && hayFondo)
            rect({mx - ancho / 2, m.cuarto.y, ancho, fondo}, hueco);
        if (e.hab == 2)
            rect({m.suelo.x + m.suelo.w, my - ancho / 2, fondo, ancho}, hueco);
        if (e.hab == 3)
            rect({m.cuarto.x, my - ancho / 2, fondo, ancho}, hueco);
        // Lo que cuelga de la pared del fondo, pequeño, en lo alto del plano.
        tiraPared(e, m);
        // Lo que se edita se ve claro; lo demás, más tenue (como el Mac).
        double alfombras = 1, muebles = 1, mesas = 1, techo = 0;
        if (capa == casas::SobreMuebles)
            alfombras = 0.6, muebles = 0.55, mesas = 1;
        else if (capa == casas::Alfombras)
            alfombras = 1, muebles = 0.35, mesas = 0.3;
        else if (capa == casas::Techo)
            alfombras = 0.5, muebles = 0.4, mesas = 0.35, techo = 1;
        fichas(e, m, casas::Alfombras, Plana, alfombras);
        fichas(e, m, casas::Suelo, Mueble, muebles);
        fichas(e, m, casas::SobreMuebles, SobreMesa, mesas);
        fichas(e, m, casas::Techo, Colgado, techo);
    }

    // El cursor.
    if (e.modo == Mirar)
    {
        Objeto o;
        int rx, ry;
        SDL_Rect r;
        int giro = -1;
        if (cosaEn(e, e.cx, e.cy, o, rx, ry) && esRaiz(o))
        {
            int w, h;
            tamano(o, w, h);
            r = enPantalla(m, rx, ry, w, h);
            if (girable(o) && !esPared(e))
                giro = mapa::giro(o);
        }
        else
            r = enPantalla(m, e.cx, e.cy, 2, 2);
        marco({r.x - 3, r.y - 3, r.w + 6, r.h + 6}, {0, 0, 0, 0x90}, 2);
        marco({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, BLANCO, 3);
        if (giro >= 0 && r.w >= 18)
            flecha(r, giro);
    }
    else
    {
        int w, h;
        tamano(e.llevado, w, h);
        const int x = e.cx + e.dx, y = e.cy + e.dy;
        Rejilla rj = rejilla(e);
        bool cabe = casas::cabe(limites(e), e.llevado, x, y);
        for (int i = 0; i < w && cabe; i++)
            for (int j = 0; j < h && cabe; j++)
            {
                Objeto o;
                int rx, ry;
                if (raiz(rj, x + i, y + j, o, rx, ry) && !(e.modo == Llevar && rx == e.desdeX && ry == e.desdeY))
                    cabe = false;
            }
        const SDL_Rect r = enPantalla(m, x, y, w, h);
        const SDL_Color verde = {0x2F, 0x9E, 0x62, 255}, rojo = {0xD0, 0x40, 0x40, 255};
        if (SDL_Texture* foto = dibujoDe(e.llevado))
        {
            redondo(r, 6, conAlfa(cabe ? verde : rojo, 0.25));
            dibujoEn(foto, cuadrado(encoger(r, std::min(r.w, r.h) * 0.06)), 0xD8);
        }
        else
            redondo(r, 6, conAlfa(colorDe(e.llevado), 0.75));
        marco(r, cabe ? SDL_Color{0x2F, 0x9E, 0x62, 255} : SDL_Color{0xD0, 0x40, 0x40, 255}, 3);
    }
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_NONE);
}

void dibujarPanel(Estado& e)
{
    rect({PANEL_X, 96, W - PANEL_X, ABAJO - 96}, CREMA);
    redondo({PANEL_X + 16, 108, W - PANEL_X - 32, 296}, 14, BLANCO);
    const int x = PANEL_X + 34, ancho = W - PANEL_X - 68;
    texto(encajar(std::string("Editando: ") + SUPERFICIES[e.sup].nombre, ancho, g_pequena), x, 124, VERDE_OSCURO, g_pequena);
    std::string titulo, detalle;
    SDL_Color color = CREMA_OSCURA;
    SDL_Texture* foto = nullptr;
    Objeto o;
    int rx, ry;
    if (e.modo == Llevar || e.modo == Poniendo)
    {
        titulo = std::string(e.modo == Llevar ? "Moviendo: " : "Poniendo: ") + nombre(e.llevado);
        detalle = e.modo == Llevar ? "Lleva el cursor a su nuevo sitio y pulsa A. B para dejarlo donde estaba."
                                   : "Pulsa A donde lo quieras; cada vez pone uno más. B para terminar.";
        color = colorDe(e.llevado);
        foto = dibujoDe(e.llevado);
    }
    else if (cosaEn(e, e.cx, e.cy, o, rx, ry))
    {
        titulo = nombre(o);
        detalle = "A para cambiarlo, girarlo, moverlo o quitarlo.";
        color = colorDe(o);
        foto = dibujoDe(o);
    }
    else
    {
        titulo = "Sitio vacío";
        detalle = std::string(AYUDAS[e.sup]) + " A para poner algo aquí.";
    }
    redondo({x, 160, 44, 44}, 8, foto ? CREMA : color);
    if (foto)
        dibujoEn(foto, {x + 2, 162, 40, 40}, 255);
    const int alto = std::max(44, texto(titulo, x + 56, 164, MARRON, nullptr, Izquierda, ancho - 56));
    texto(detalle, x, 160 + std::min(alto, 100) + 12, MARRON_SUAVE, g_pequena, Izquierda, ancho);
    auto boton = [](SDL_Rect r, const std::string& k, const std::string& t) {
        redondo(r, 10, {0xD1, 0xD1, 0xD6, 255});
        redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 8, BLANCO);
        const int kw = std::max(30, anchoTexto(k, g_pequena) + 16);
        redondo({r.x + 8, r.y + (r.h - 30) / 2, kw, 30}, 15, MARRON);
        texto(k, r.x + 8 + kw / 2, r.y + r.h / 2, BLANCO, g_pequena, Centro);
        texto(encajar(t, r.w - kw - 26, g_pequena), r.x + kw + 18, r.y + r.h / 2 - 14, MARRON, g_pequena);
    };
    boton({PANEL_X + 16, 420, W - PANEL_X - 32, 48}, "L R", "Otra habitación");
    boton({PANEL_X + 16, 476, W - PANEL_X - 32, 48}, "A", e.modo == Mirar ? "Qué hay aquí" : e.modo == Llevar ? "Dejarlo aquí" : "Poner uno");
    boton({PANEL_X + 16, 532, W - PANEL_X - 32, 48}, "X", "Editar otra cosa");
    boton({PANEL_X + 16, 588, W - PANEL_X - 32, 48}, "Y", "La casa y la habitación");
}

// ------------------------------------------------------------------------------------------------------ acciones
void abrirSitio(std::shared_ptr<Estado> e)
{
    Objeto o;
    int rx, ry;
    const bool hay = cosaEn(*e, e->cx, e->cy, o, rx, ry);
    if (!hay)
        rx = e->cx, ry = e->cy;
    auto pos = std::make_shared<std::pair<int, int>>(rx, ry);
    Sitio s;
    s.donde = SUPERFICIES[e->sup].donde;
    s.posicion = [pos] { return *pos; };
    s.objeto = [e, pos] {
        const Objeto o = rejilla(*e).leer(pos->first, pos->second);
        return esRaiz(o) ? o : mapa::nada();
    };
    s.aviso = [e] { return e->aviso; };
    e->aviso.clear();
    s.poner = [e, pos](const Objeto& n) {
        Rejilla r = rejilla(*e);
        Objeto estorbo;
        int x, y;
        const Resultado res = casas::poner(r, limites(*e), n, pos->first, pos->second, &estorbo, &x, &y);
        if (res != Bien)
        {
            e->aviso = noCabe(res, estorbo, true);
            avisar(e->aviso, true);
            return false;
        }
        e->aviso.clear();
        *pos = {x, y};
        partida::cambiaPrincipal();
        return true;
    };
    s.quitar = [e, pos] {
        Rejilla r = rejilla(*e);
        quitar(r, pos->first, pos->second);
        partida::cambiaPrincipal();
    };
    if (hay && girable(o))
        s.girar = [e, pos] {
            Rejilla r = rejilla(*e);
            Objeto estorbo;
            const Resultado res = casas::girar(r, limites(*e), pos->first, pos->second, &estorbo);
            if (res != Bien)
            {
                e->aviso = noCabe(res, estorbo, true);
                avisar(e->aviso, true);
                return;
            }
            e->aviso.clear();
            partida::cambiaPrincipal();
            cambiado("Girado");
        };
    if (hay)
        s.mover = [e, pos] {
            e->llevado = rejilla(*e).leer(pos->first, pos->second);
            e->desdeX = pos->first, e->desdeY = pos->second;
            e->cx = pos->first & ~1, e->cy = pos->second & ~1;
            e->dx = pos->first - e->cx, e->dy = pos->second - e->cy;
            e->modo = Llevar;
        };
    abrir(panelSitio(s));
}

void pulsarA(std::shared_ptr<Estado> e)
{
    if (e->modo == Mirar)
    {
        abrirSitio(e);
        return;
    }
    Rejilla r = rejilla(*e);
    Objeto estorbo;
    const int x = e->cx + e->dx, y = e->cy + e->dy;
    if (e->modo == Llevar)
    {
        const Resultado res = casas::mover(r, limites(*e), e->desdeX, e->desdeY, x, y, &estorbo);
        if (res != Bien)
        {
            avisar(noCabe(res, estorbo, true), true);
            return;
        }
        e->modo = Mirar;
        partida::cambiaPrincipal();
        cambiado("Movido: " + nombre(e->llevado));
        return;
    }
    const Resultado res = casas::ponerNuevo(r, limites(*e), e->llevado, x, y, &estorbo);
    if (res != Bien)
    {
        avisar(noCabe(res, estorbo, true), true);
        return;
    }
    partida::cambiaPrincipal();
    cambiado("Puesto: " + nombre(e->llevado));
}

void otraHabitacion(Estado& e, int d)
{
    const auto habs = casas::habitaciones(principal(), e.casa);
    auto it = std::find(habs.begin(), habs.end(), e.hab);
    int i = it == habs.end() ? 0 : (int)(it - habs.begin());
    i = (i + d + (int)habs.size()) % (int)habs.size();
    ponerHabitacion(e, habs[i]);
}

// La puerta: su diseño (el de arco) y si es recta, como el Mac.
int disenoPuerta(uint16_t v)
{
    const int n = sizeof(PUERTAS) / sizeof(PUERTAS[0]);
    for (int i = 0; i < n; i++)
        if (PUERTAS[i].arco == v || PUERTAS[i].recta == v)
            return i;
    return -1;
}

void ponerPuerta(int casa, int diseno, bool recta)
{
    const auto& p = PUERTAS[diseno];
    const uint16_t v = recta && p.recta >= 0 ? (uint16_t)p.recta : p.arco;
    casas::ponerAspecto(principal(), casa, 2, v);
    partida::cambiaPrincipal();
    cambiado("Puerta cambiada");
}

// La casa y la habitación (Y): elegir casa y habitación, papel, suelo, tamaño, la casa por fuera, poner cosas y las
// opciones avanzadas.
void abrirFuera(std::shared_ptr<Estado> e);
void abrirAvanzado(std::shared_ptr<Estado> e);

void abrirCasa(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        const int c = e->casa;
        v.titulo = nombreCasa(c);
        v.explicacion = std::string("Habitación: ") + HABITACIONES[e->hab] + ".";
        std::vector<Boton> bs;
        auto fila = [&](const std::string& t, const std::string& d, std::function<void()> f, bool activo = true, SDL_Texture* imagen = nullptr) {
            Boton b{{}, t, f, activo};
            b.detalle = d;
            b.imagen = imagen;
            bs.push_back(b);
        };
        if (numCasas() > 1)
            fila("Otra casa", nombreCasa(c), [e] {
                std::vector<std::string> ops;
                for (int i = 0; i < numCasas(); i++)
                    ops.push_back(nombreCasa(i));
                elegir("Casa", "Elige la casa.", ops, e->casa, [e](int i) {
                    e->casa = i;
                    ponerHabitacion(*e, casas::habitaciones(principal(), i)[0]);
                });
            });
        fila("Habitación", HABITACIONES[e->hab], [e] {
            const auto habs = casas::habitaciones(principal(), e->casa);
            std::vector<std::string> ops;
            int actual = 0;
            for (size_t i = 0; i < habs.size(); i++)
            {
                ops.push_back(HABITACIONES[habs[i]]);
                if (habs[i] == e->hab)
                    actual = (int)i;
            }
            elegir("Habitación", "Elige la habitación.", ops, actual, [e, habs](int i) { ponerHabitacion(*e, habs[i]); });
        });
        const Objeto papel = casas::pared(principal(), c, e->hab, false), suelo = casas::pared(principal(), c, e->hab, true);
        fila("Papel pintado", nombrePared(papel), [e] {
            buscarObjeto([e](const Objeto& o) {
                Objeto n = mapa::nada();
                n.id = o.id;
                casas::ponerPared(principal(), e->casa, e->hab, false, n);
                partida::cambiaPrincipal();
                cambiado("Papel pintado: " + nombre(n));
            }, "las paredes", 2);
        }, true, papel.id != mapa::NADA && papel.id != 0 ? dibujoDe(papel) : nullptr);
        fila("Suelo", nombrePared(suelo), [e] {
            buscarObjeto([e](const Objeto& o) {
                Objeto n = mapa::nada();
                n.id = o.id;
                casas::ponerPared(principal(), e->casa, e->hab, true, n);
                partida::cambiaPrincipal();
                cambiado("Suelo: " + nombre(n));
            }, "el suelo", 2);
        }, true, suelo.id != mapa::NADA && suelo.id != 0 ? dibujoDe(suelo) : nullptr);
        const uint32_t nivel = casas::nivel(principal(), c);
        fila("Tamaño", nivel >= 1 && nivel <= 8 ? NIVELES[nivel - 1] : std::to_string(nivel), [e, nivel] {
            std::vector<std::string> ops(NIVELES, NIVELES + 8);
            elegir("Tamaño de la casa", "Hasta dónde está ampliada.", ops, (int)nivel - 1, [e](int i) {
                casas::ponerNivel(principal(), e->casa, (uint32_t)(i + 1));
                const auto habs = casas::habitaciones(principal(), e->casa);
                if (std::find(habs.begin(), habs.end(), e->hab) == habs.end())
                    ponerHabitacion(*e, habs[0]);
                else
                    alCentro(*e);
                partida::cambiaPrincipal();
                cambiado(std::string("Tamaño: ") + NIVELES[i]);
            });
        });
        fila("La casa por fuera…", "El tejado, la fachada y la puerta", [e] { abrirFuera(e); });
        fila("Poner algo nuevo…", "Lo buscas y luego lo pones donde quieras", [e] {
            buscarObjeto([e](const Objeto& o) {
                // Como en el Mac, el cursor lo lleva por el medio.
                e->llevado = o;
                int w, h;
                tamano(o, w, h);
                e->dx = -((w - 1) / 2), e->dy = -((h - 1) / 2);
                e->modo = Poniendo;
                volver();
            }, "ponerlo en la habitación");
        });
        fila("Opciones avanzadas…", "Sucesos, valores sueltos, pasar a la tarjeta SD", [e] { abrirAvanzado(e); });
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 150 + (int)(i / 2) * 100, 570, 86};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 66, 400, 56}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------- la casa por fuera
// Elegir un tejado, una fachada o una puerta por su dibujo (HousePartPicker del Mac): los que el juego ofrece, y el que
// tiene ahora aunque no esté entre ellos.
void elegirParte(casas_fuera::Parte parte, const std::string& titulo, std::vector<std::pair<uint16_t, std::string>> ops, uint16_t ahora,
    std::function<void(uint16_t)> elegido)
{
    auto lista_ = std::make_shared<std::vector<std::pair<uint16_t, std::string>>>(std::move(ops));
    Lista l;
    l.titulo = titulo;
    l.explicacion = [] {
        return casas_fuera::hayDibujos() ? std::string("Elige cómo quieres que sea.")
                                         : std::string("Elige cómo quieres que sea (sin fachadas.bin en la tarjeta SD, con su color).");
    };
    l.columnas = 3;
    l.porColumna = 5;
    l.alto = 84;
    l.cuantas = [lista_] { return (int)lista_->size(); };
    l.fila = [lista_, parte, ahora, elegido](int i) {
        const auto& o = (*lista_)[i];
        Fila f{o.second, [elegido, v = o.first] {
            volver();
            elegido(v);
        }};
        f.marcado = o.first == ahora;
        f.imagen = casas_fuera::dibujo(parte, o.first);
        if (!f.imagen)
            f.color = casas_fuera::color(parte, o.first);
        return f;
    };
    abrir(lista(l));
}

template <size_t N>
std::vector<std::pair<uint16_t, std::string>> opciones(const Aspecto (&lista)[N], const Aspecto* todos, size_t numTodos, uint16_t ahora)
{
    std::vector<std::pair<uint16_t, std::string>> v;
    bool esta = false;
    for (const auto& a : lista)
    {
        v.push_back({a.valor, a.nombre});
        esta |= a.valor == ahora;
    }
    if (!esta)
        v.insert(v.begin(), {ahora, nombreAspecto(todos, (int)numTodos, ahora)});
    return v;
}

void abrirFuera(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        const int c = e->casa;
        const uint16_t tejado = casas::aspecto(principal(), c, 0), fachada = casas::aspecto(principal(), c, 1), puerta = casas::aspecto(principal(), c, 2);
        v.titulo = nombreCasa(c) + " por fuera";
        v.explicacion = casas_fuera::hayDibujos() ? "Se ve así en el juego desde el día siguiente."
                                                  : "Desde el día siguiente. Dibujo aproximado (con fachadas.bin en /switch/NHSE, como en el juego).";
        v.dibujo = [tejado, fachada, puerta] {
            const SDL_Rect r = {60, 160, 580, 418};
            redondo({r.x - 6, r.y - 6, r.w + 12, r.h + 12}, 16, BLANCO);
            casas_fuera::dibujar(r, tejado, fachada, puerta);
        };
        auto boton = [&](int y, int h, const std::string& t, const std::string& d, std::function<void()> f, SDL_Texture* imagen, bool activo = true) {
            Boton b{{690, y, 530, h}, t, std::move(f), activo};
            b.detalle = d;
            b.imagen = imagen;
            v.b.push_back(b);
        };
        constexpr size_t NT = sizeof(NOMBRES_TEJADO) / sizeof(NOMBRES_TEJADO[0]), NF = sizeof(NOMBRES_FACHADA) / sizeof(NOMBRES_FACHADA[0]);
        boton(154, 96, "Tejado", NOMBRE_DE(NOMBRES_TEJADO, tejado), [e, tejado] {
            elegirParte(casas_fuera::Tejado, "Tejado", opciones(TEJADOS, NOMBRES_TEJADO, NT, tejado), tejado, [e](uint16_t v) {
                casas::ponerAspecto(principal(), e->casa, 0, v);
                partida::cambiaPrincipal();
                cambiado(std::string("Tejado: ") + NOMBRE_DE(NOMBRES_TEJADO, v));
            });
        }, casas_fuera::dibujo(casas_fuera::Tejado, tejado));
        boton(262, 96, "Fachada", NOMBRE_DE(NOMBRES_FACHADA, fachada), [e, fachada] {
            elegirParte(casas_fuera::Fachada, "Fachada", opciones(FACHADAS, NOMBRES_FACHADA, NF, fachada), fachada, [e](uint16_t v) {
                casas::ponerAspecto(principal(), e->casa, 1, v);
                partida::cambiaPrincipal();
                cambiado(std::string("Fachada: ") + NOMBRE_DE(NOMBRES_FACHADA, v));
            });
        }, casas_fuera::dibujo(casas_fuera::Fachada, fachada));
        const int diseno = disenoPuerta(puerta);
        const bool recta = diseno >= 0 && PUERTAS[diseno].recta == puerta;
        boton(370, 96, "Puerta", NOMBRE_DE(NOMBRES_PUERTA, puerta), [e, recta] {
            std::vector<std::pair<uint16_t, std::string>> ops;
            for (const auto& p : PUERTAS)
                ops.push_back({p.arco, p.nombre});
            const int d = disenoPuerta(casas::aspecto(principal(), e->casa, 2));
            elegirParte(casas_fuera::Puerta, "Puerta", ops, d >= 0 ? PUERTAS[d].arco : 0xFFFF, [e, recta](uint16_t v) {
                ponerPuerta(e->casa, disenoPuerta(v), recta);
            });
        }, casas_fuera::dibujo(casas_fuera::Puerta, puerta));
        boton(478, 96, "Forma de la puerta", diseno < 0 ? "—" : recta ? "Recta (pulsa para en arco)" : "En arco (pulsa para recta)", [e, diseno, recta] {
            if (!recta && PUERTAS[diseno].recta < 0)
            {
                avisar("Esta puerta solo es en arco.", true);
                return;
            }
            ponerPuerta(e->casa, diseno, !recta);
        }, nullptr, diseno >= 0);
        v.b.push_back({{690, ABAJO - 66, 530, 56}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------- opciones avanzadas
constexpr const char* CARPETA_CASAS = "sdmc:/switch/NHSE/casas/";

std::string archivoSd(const std::string& nombre, const std::string& extension)
{
    std::string r;
    for (char c : nombre)
        if (!strchr("\\/:*?\"<>|", c))
            r += c;
    return (r.empty() ? std::string("casa") : r) + "." + extension;
}

// Elegir un archivo de la carpeta de las casas con alguna de esas terminaciones.
void elegirArchivo(const std::string& titulo, const std::string& explicacion, const std::vector<std::string>& extensiones,
    std::function<void(const std::string&)> elegido)
{
    std::vector<std::string> nombres;
    for (const auto& x : extensiones)
        for (const auto& n : partida::listarSd(CARPETA_CASAS, x))
            nombres.push_back(n);
    if (nombres.empty())
    {
        avisar("No hay ninguno en la carpeta /switch/NHSE/casas de la tarjeta SD.", true);
        return;
    }
    std::sort(nombres.begin(), nombres.end());
    elegir(titulo, explicacion, nombres, -1, [nombres, elegido](int i) { elegido(nombres[i]); });
}

// Los sucesos de la casa que tienen nombre (EventFlagHouse de NHSE).
std::string nombreSuceso(int i)
{
    switch (i)
    {
        case 1: return "Pedida la casa (era tienda)";
        case 2: return "Pedida la casa más grande";
        case 3: return "Pedida la habitación del fondo";
        case 4: return "Pedida la habitación izquierda";
        case 5: return "Pedida la habitación derecha";
        case 6: return "Pedida la planta de arriba";
        case 7: return "Pedido el sótano";
        case 9: return "Días desde que es casa";
        case 10: return "Días desde que tiene sótano";
        case 11: return "Días desde la última ampliación";
        default: return "Sin nombre";
    }
}

void abrirSucesos(std::shared_ptr<Estado> e)
{
    Lista l;
    l.titulo = "Sucesos de la casa de " + nombreCasa(e->casa).substr(8);
    l.explicacion = [] { return std::string("Lo que la casa recuerda: ampliaciones pedidas (1 sí, 0 no) y días. Elige uno para cambiarlo."); };
    l.cuantas = [] { return casas::NUM_SUCESOS; };
    l.columnas = 2;
    l.porColumna = 8;
    l.fila = [e](int i) {
        const int16_t v = casas::suceso(principal(), e->casa, i);
        char num[8];
        snprintf(num, sizeof num, "%02d", i);
        Fila f{std::string(num) + " · " + nombreSuceso(i) + ": " + std::to_string(v), [e, i] {
            uint32_t n = (uint32_t)std::max<int>(0, casas::suceso(principal(), e->casa, i));
            if (pedirNumero("Suceso " + std::to_string(i) + ": " + nombreSuceso(i), n, 32767))
            {
                casas::ponerSuceso(principal(), e->casa, i, (int16_t)n);
                partida::cambiaPrincipal();
                cambiado("Suceso " + std::to_string(i) + ": " + std::to_string(n));
            }
        }};
        f.marcado = v != 0;
        return f;
    };
    l.abajo.push_back([e] {
        return Fila{"Exportar", [e] {
            const std::string n = archivoSd("Sucesos de " + nombreCasa(e->casa).substr(8), "nhfl");
            if (partida::escribirSd(std::string(CARPETA_CASAS) + n, casas::sucesos(principal(), e->casa)))
                avisar("Guardados en la tarjeta SD: /switch/NHSE/casas/" + n);
            else
                avisar("No he podido guardarlos en la tarjeta SD.", true);
        }};
    });
    l.abajo.push_back([e] {
        return Fila{"Importar", [e] {
            elegirArchivo("Importar sucesos", "Los sucesos guardados en /switch/NHSE/casas (.nhfl, de NHSE o de aquí).", {".nhfl"}, [e](const std::string& n) {
                std::vector<uint8_t> d;
                if (!partida::leerSd(std::string(CARPETA_CASAS) + n, d) || !casas::ponerSucesos(principal(), e->casa, d))
                {
                    avisar("«" + n + "» no son sucesos de una casa (tienen que ser 256 bytes).", true);
                    return;
                }
                partida::cambiaPrincipal();
                cambiado("Sucesos de «" + n + "»");
            });
        }};
    });
    abrir(lista(l));
}

// Elegir uno de todos los valores de un aspecto (como el ComboBox del PropertyGrid del Mac), por su nombre.
template <size_t N>
void elegirValorAspecto(const std::string& titulo, const Aspecto (&todos)[N], uint16_t ahora, std::function<void(uint16_t)> elegido)
{
    std::vector<std::string> ops;
    std::vector<uint16_t> valores;
    int actual = -1;
    for (const auto& a : todos)
    {
        if (std::find(valores.begin(), valores.end(), a.valor) != valores.end())
            continue;
        if (a.valor == ahora)
            actual = (int)valores.size();
        valores.push_back(a.valor);
        ops.push_back(std::to_string(a.valor) + " · " + a.nombre);
    }
    elegir(titulo, "Todos los que hay en el juego (también los de los vecinos y los de las casas a medias).", ops, actual,
        [valores, elegido](int i) { elegido(valores[i]); }, 3);
}

// Los valores sueltos de la casa (el PropertyGrid de «Opciones avanzadas» del Mac).
void abrirValores(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        const int c = e->casa;
        auto& m = principal();
        v.titulo = "Valores sueltos de " + nombreCasa(c).substr(8);
        v.explicacion = "Lo que guarda la partida de la casa, uno por uno. Para quien sabe lo que toca.";
        std::vector<Boton> bs;
        auto fila = [&](const std::string& t, const std::string& d, std::function<void()> f) {
            Boton b{{}, t, std::move(f)};
            b.detalle = d;
            bs.push_back(b);
        };
        auto poner = [e](casas::Valor val, int64_t n, const std::string& que) {
            casas::ponerValor(principal(), e->casa, val, n);
            partida::cambiaPrincipal();
            cambiado(que);
        };
        auto numero_ = [e, poner](casas::Valor val, const std::string& t, uint32_t maximo) {
            uint32_t n = (uint32_t)casas::valor(principal(), e->casa, val);
            if (pedirNumero(t, n, maximo))
                poner(val, n, t + ": " + std::to_string(n));
        };
        const uint32_t nivel = (uint32_t)casas::valor(m, c, casas::ValorNivel);
        fila("Tamaño (HouseLevel)", std::to_string(nivel) + (nivel >= 1 && nivel <= 8 ? std::string(" · ") + NIVELES[nivel - 1] : ""), [numero_] {
            numero_(casas::ValorNivel, "Tamaño (1 a 8)", UINT32_MAX);
        });
        struct Aspectos {
            casas::Valor val;
            const char* t;
            int cual; // 0 tejado, 1 fachada, 2 puerta
        };
        const Aspectos AS[] = {{casas::ValorTejado, "Tejado", 0}, {casas::ValorFachada, "Fachada", 1}, {casas::ValorPuerta, "Puerta", 2},
            {casas::ValorPedidoTejado, "Tejado pedido", 0}, {casas::ValorPedidoFachada, "Fachada pedida", 1}, {casas::ValorPedidoPuerta, "Puerta pedida", 2}};
        for (const auto& a : AS)
        {
            const uint16_t ahora = (uint16_t)casas::valor(m, c, a.val);
            const char* n = a.cual == 0 ? NOMBRE_DE(NOMBRES_TEJADO, ahora) : a.cual == 1 ? NOMBRE_DE(NOMBRES_FACHADA, ahora) : NOMBRE_DE(NOMBRES_PUERTA, ahora);
            fila(a.t, std::to_string(ahora) + " · " + n, [poner, a, ahora] {
                auto f = [poner, a](uint16_t v) { poner(a.val, v, std::string(a.t) + ": " + std::to_string(v)); };
                if (a.cual == 0)
                    elegirValorAspecto(a.t, NOMBRES_TEJADO, ahora, f);
                else if (a.cual == 1)
                    elegirValorAspecto(a.t, NOMBRES_FACHADA, ahora, f);
                else
                    elegirValorAspecto(a.t, NOMBRES_PUERTA, ahora, f);
            });
        }
        fila("Préstamo (Loan)", numero((uint32_t)casas::valor(m, c, casas::ValorPrestamo)) + " bayas", [numero_] {
            numero_(casas::ValorPrestamo, "Lo que queda por pagar de la casa", UINT32_MAX);
        });
        fila("Otro número (Unknown)", std::to_string(casas::valor(m, c, casas::ValorOtro)), [numero_] {
            numero_(casas::ValorOtro, "Otro número cifrado", UINT32_MAX);
        });
        for (int k = 0; k < 2; k++)
        {
            const casas::Valor val = k ? casas::ValorNpc2 : casas::ValorNpc1;
            const int64_t ahora = casas::valor(m, c, val);
            fila(k ? "NPC2" : "NPC1", std::to_string(ahora) + (ahora < 0 ? " · nadie" : ""), [poner, val, k] {
                std::vector<std::string> ops = {"-1 · nadie"};
                for (int i = 0; i < 10; i++)
                    ops.push_back(std::to_string(i));
                ops.push_back("Otro número…");
                elegir(k ? "NPC2" : "NPC1", "Un número de -128 a 127 (-1: nadie).", ops, -1, [poner, val](int i) {
                    if (i <= 10)
                    {
                        poner(val, i - 1, "Cambiado: " + std::to_string(i - 1));
                        return;
                    }
                    uint32_t n = 0;
                    if (pedirNumero("De 0 a 127", n, 127))
                        poner(val, n, "Cambiado: " + std::to_string(n));
                }, 3);
            });
        }
        const bool bandera = casas::valor(m, c, casas::ValorBandera) != 0;
        fila("PlayerHouseFlag", bandera ? "Sí" : "No", [poner, bandera] { poner(casas::ValorBandera, bandera ? 0 : 1, bandera ? "No" : "Sí"); });
        const char* const OBJETOS_CASA[] = {"Decoración de la puerta", "Buzón", "Buzón pedido"};
        for (int k = 0; k < 3; k++)
        {
            const Objeto o = casas::objetoCasa(m, c, k);
            fila(OBJETOS_CASA[k], esNada(o) ? "Nada" : nombre(o), [e, k, t = std::string(OBJETOS_CASA[k])] {
                abrir(editorBruto(t, [e, k] { return casas::objetoCasa(principal(), e->casa, k); },
                    [e, k](const Objeto& n) {
                        casas::ponerObjetoCasa(principal(), e->casa, k, n);
                        partida::cambiaPrincipal();
                        return true;
                    },
                    [e] { return nombreCasa(e->casa); }));
            });
        }
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 3) * 393, 140 + (int)(i / 3) * 90, 373, 80};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 66, 400, 56}, "Volver", [] { volver(); }});
    });
}

void abrirAvanzado(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        const int c = e->casa;
        v.titulo = "Opciones avanzadas";
        v.explicacion = nombreCasa(c) + ". Las casas y habitaciones se guardan en /switch/NHSE/casas de la tarjeta SD, en el formato de NHSE: "
                        "se pueden pasar al Mac y del Mac.";
        std::vector<Boton> bs;
        auto fila = [&](const std::string& t, const std::string& d, std::function<void()> f) {
            Boton b{{}, t, std::move(f)};
            b.detalle = d;
            bs.push_back(b);
        };
        fila("Sucesos de la casa…", "Ampliaciones pedidas y días", [e] { abrirSucesos(e); });
        fila("Valores sueltos…", "Tamaño, aspecto, préstamo, buzón…", [e] { abrirValores(e); });
        fila("Exportar la casa", "A la tarjeta SD (.nhph2)", [e] {
            const std::string n = archivoSd(nombreCasa(e->casa), "nhph2");
            if (partida::escribirSd(std::string(CARPETA_CASAS) + n, casas::exportar(principal(), e->casa)))
                avisar("Guardada en la tarjeta SD: /switch/NHSE/casas/" + n);
            else
                avisar("No he podido guardarla en la tarjeta SD.", true);
        });
        fila("Importar una casa", "De la tarjeta SD: todo, por fuera y por dentro", [e] {
            elegirArchivo("Importar una casa", "Las casas guardadas en /switch/NHSE/casas (.nhph2 o .nhph, de NHSE o de aquí).", {".nhph2", ".nhph"},
                [e](const std::string& n) {
                    confirmar("Importar una casa", "¿Poner la casa «" + n + "» en lugar de la de " +
                        nombreCasa(e->casa).substr(8) + "? Cambian el tamaño, el aspecto y todo lo de dentro.", "Sí, ponerla", [e, n] {
                        std::vector<uint8_t> d;
                        const std::string mal = !partida::leerSd(std::string(CARPETA_CASAS) + n, d) ? "No he podido leer el archivo." : casas::importar(principal(), e->casa, d);
                        if (!mal.empty())
                        {
                            avisar("«" + n + "»: " + mal, true);
                            return;
                        }
                        partida::cambiaPrincipal();
                        const auto habs = casas::habitaciones(principal(), e->casa);
                        ponerHabitacion(*e, std::find(habs.begin(), habs.end(), e->hab) == habs.end() ? habs[0] : e->hab);
                        cambiado("Casa cambiada");
                    });
                });
        });
        fila("Exportar esta habitación", std::string(HABITACIONES[e->hab]) + " (.nhpr2)", [e] {
            const std::string n = archivoSd(nombreCasa(e->casa).substr(8) + " - " + HABITACIONES[e->hab], "nhpr2");
            if (partida::escribirSd(std::string(CARPETA_CASAS) + n, casas::exportarHabitacion(principal(), e->casa, e->hab)))
                avisar("Guardada en la tarjeta SD: /switch/NHSE/casas/" + n);
            else
                avisar("No he podido guardarla en la tarjeta SD.", true);
        });
        fila("Importar una habitación", std::string("En la ") + HABITACIONES_MIN[e->hab], [e] {
            elegirArchivo("Importar una habitación", "Las habitaciones guardadas en /switch/NHSE/casas (.nhpr2 o .nhpr). Se pone en la " +
                std::string(HABITACIONES_MIN[e->hab]) + ".", {".nhpr2", ".nhpr"}, [e](const std::string& n) {
                confirmar("Importar una habitación", "¿Poner «" + n + "» en la " + HABITACIONES_MIN[e->hab] + "? Lo que hay ahora en ella se va.", "Sí, ponerla", [e, n] {
                    std::vector<uint8_t> d;
                    const std::string mal = !partida::leerSd(std::string(CARPETA_CASAS) + n, d) ? "No he podido leer el archivo." : casas::importarHabitacion(principal(), e->casa, e->hab, d);
                    if (!mal.empty())
                    {
                        avisar("«" + n + "»: " + mal, true);
                        return;
                    }
                    partida::cambiaPrincipal();
                    cambiado("Habitación cambiada");
                });
            });
        });
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 170 + (int)(i / 2) * 110, 570, 94};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 66, 400, 56}, "Volver", [] { volver(); }});
    });
}

Pantalla pantallaCasa()
{
    auto e = std::make_shared<Estado>();
    e->casa = std::clamp(partida::yo, 0, numCasas() - 1);
    ponerHabitacion(*e, casas::habitaciones(principal(), e->casa)[0]);
    return [e](Vista& v) {
        v.titulo = nombreCasa(e->casa) + " · " + HABITACIONES[e->hab];
        v.dibujo = [e] {
            rect(AREA, {0xF0, 0xE8, 0xD8, 255});
            dibujarHabitacion(*e);
            dibujarPanel(*e);
        };
        v.entrada = [e](u64 down, u64, int tx, int ty) {
            int dx = 0, dy = 0;
            if (down & HidNpadButton_AnyLeft) dx = -1;
            if (down & HidNpadButton_AnyRight) dx = 1;
            if (down & HidNpadButton_AnyUp) dy = -1;
            if (down & HidNpadButton_AnyDown) dy = 1;
            if (dx || dy)
            {
                e->cx = std::clamp(e->cx + dx * 2, 0, casas::LADO - 2);
                e->cy = std::clamp(e->cy + dy * 2, 0, (esPared(*e) ? casas::FILAS_PARED : casas::LADO) - 2);
            }
            if (tx >= 0 && tx < PANEL_X && ty >= AREA.y && ty < AREA.y + AREA.h)
            {
                const Medidas m = medir(*e);
                if (tx >= m.suelo.x && ty >= m.suelo.y && tx < m.suelo.x + m.suelo.w && ty < m.suelo.y + m.suelo.h)
                {
                    const int x = (m.x0 + (int)((tx - m.suelo.x) / m.u)) & ~1, y = (m.y0 + (int)((ty - m.suelo.y) / m.u)) & ~1;
                    const bool mismo = x == e->cx && y == e->cy;
                    e->cx = x, e->cy = y;
                    if (mismo)
                        pulsarA(e);
                }
            }
            if (tx >= PANEL_X && ty >= 420)
            {
                const int i = (ty - 420) / 56;
                if (i == 0) otraHabitacion(*e, 1);
                if (i == 1) pulsarA(e);
                if (i == 2) e->sup = (e->sup + 1) % NUM_SUPERFICIES, e->modo = Mirar, alCentro(*e);
                if (i == 3) abrirCasa(e);
                return true;
            }
            if (down & HidNpadButton_A)
                pulsarA(e);
            return false;
        };
        v.x = [e] {
            e->sup = (e->sup + 1) % NUM_SUPERFICIES;
            e->modo = Mirar;
            alCentro(*e);
            avisar(std::string("Editando: ") + SUPERFICIES[e->sup].nombre + ". " + AYUDAS[e->sup]);
        };
        v.y = [e] { abrirCasa(e); };
        v.paginar = [e](int d) { otraHabitacion(*e, d > 0 ? 1 : -1); };
        v.atras = [e] {
            if (e->modo != Mirar)
                e->modo = Mirar;
            else
                volver();
        };
        if (e->modo == Mirar)
            v.teclas = {{"A", "Qué hay"}, {"X", "Editar otra cosa"}, {"Y", "Casa"}, {"L R", "Habitación"}, {"B", "Volver"}};
        else
            v.teclas = {{"A", e->modo == Llevar ? "Dejar aquí" : "Poner uno"}, {"B", e->modo == Llevar ? "Cancelar" : "Terminar"}};
    };
}

Registro r1({SeccionIsla, 15, [] { return std::string("Casas de los jugadores"); },
    [] {
        const int c = std::clamp(partida::yo, 0, numCasas() - 1);
        const uint32_t n = casas::nivel(partida::principal(), c);
        return nombreCasa(c) + ": " + (n >= 1 && n <= 8 ? NIVELES[n - 1] : "?");
    },
    [] { abrir(pantallaCasa()); }});

} // namespace
