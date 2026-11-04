// Sección «Isla»: las casas de los jugadores, como HouseWindow de NHSE para Mac: elegir la casa y la habitación, verla
// desde arriba con sus muebles, lo que hay sobre las mesas, alfombras, paredes y techo; un cursor por cuadros para ver
// qué hay, quitarlo, girarlo, moverlo, cambiarlo o poner cosas nuevas (la misma pantalla de un sitio que el mapa); el
// papel pintado y el suelo de cada habitación; y el tamaño, el tejado, la fachada y la puerta de la casa.
#include <algorithm>
#include <cmath>
#include <memory>
#include "../core/casas.hpp"
#include "../core/mapa_casas.hpp"
#include "mapa_panel.hpp"
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
    for (const auto& c : v)
    {
        SDL_Rect q = enPantalla(m, c.x, c.y, c.w, c.h);
        const SDL_Color col = colorDe(c.o);
        const int borde = std::max(2, (int)(m.u * 0.08));
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

void marco(SDL_Rect r, SDL_Color c, int grosor)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < grosor; i++)
    {
        SDL_Rect q = {r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i};
        SDL_RenderDrawRect(g_r, &q);
    }
}

void dibujarHabitacion(Estado& e)
{
    const Medidas m = medir(e);
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    const Objeto papel = casas::pared(principal(), e.casa, e.hab, false), suelo = casas::pared(principal(), e.casa, e.hab, true);
    const SDL_Color colorPapel = papel.id == mapa::NADA || papel.id == 0 ? SDL_Color{0xC9, 0xB8, 0x9C, 255} : mezclar(colorDe(papel), {0xEF, 0xE7, 0xD8, 255}, 0.35);
    const SDL_Color colorSuelo = suelo.id == mapa::NADA || suelo.id == 0 ? SDL_Color{0xE2, 0xCF, 0xAE, 255} : mezclar(colorDe(suelo), {0xE2, 0xCF, 0xAE, 255}, 0.5);
    const int capa = SUPERFICIES[e.sup].capa;
    if (esPared(e))
    {
        // La pared de frente, con una tira de suelo debajo.
        const SDL_Rect tira = {m.suelo.x, m.suelo.y + m.suelo.h, m.suelo.w, (int)(m.u * 2)};
        rect(tira, colorSuelo);
        rect(m.suelo, colorPapel);
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
        redondo(m.cuarto, 8, mezclar(colorPapel, {0, 0, 0, 255}, 0.18));
        rect(m.suelo, colorSuelo);
        SDL_SetRenderDrawColor(g_r, 0x30, 0x20, 0x10, 0x30);
        const Zona l = limites(e);
        for (int i = 1; i < l.w / 2; i++)
            SDL_RenderDrawLine(g_r, (int)(m.suelo.x + i * 2 * m.u), m.suelo.y, (int)(m.suelo.x + i * 2 * m.u), m.suelo.y + m.suelo.h);
        for (int i = 1; i < l.h / 2; i++)
            SDL_RenderDrawLine(g_r, m.suelo.x, (int)(m.suelo.y + i * 2 * m.u), m.suelo.x + m.suelo.w, (int)(m.suelo.y + i * 2 * m.u));
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
    Objeto o;
    int rx, ry;
    if (e.modo == Llevar || e.modo == Poniendo)
    {
        titulo = std::string(e.modo == Llevar ? "Moviendo: " : "Poniendo: ") + nombre(e.llevado);
        detalle = e.modo == Llevar ? "Lleva el cursor a su nuevo sitio y pulsa A. B para dejarlo donde estaba."
                                   : "Pulsa A donde lo quieras; cada vez pone uno más. B para terminar.";
        color = colorDe(e.llevado);
    }
    else if (cosaEn(e, e.cx, e.cy, o, rx, ry))
    {
        titulo = nombre(o);
        detalle = "A para cambiarlo, girarlo, moverlo o quitarlo.";
        color = colorDe(o);
    }
    else
    {
        titulo = "Sitio vacío";
        detalle = std::string(AYUDAS[e.sup]) + " A para poner algo aquí.";
    }
    redondo({x, 160, 44, 44}, 8, color);
    const int alto = std::max(44, texto(titulo, x + 56, 164, MARRON, nullptr, Izquierda, ancho - 56));
    texto(detalle, x, 160 + std::min(alto, 100) + 12, MARRON_SUAVE, g_pequena, Izquierda, ancho);
    auto boton = [](SDL_Rect r, const std::string& k, const std::string& t) {
        redondo(r, 12, VERDE);
        const int kw = std::max(30, anchoTexto(k, g_pequena) + 16);
        redondo({r.x + 8, r.y + (r.h - 30) / 2, kw, 30}, 15, CREMA);
        texto(k, r.x + 8 + kw / 2, r.y + r.h / 2, VERDE_OSCURO, g_pequena, Centro);
        texto(encajar(t, r.w - kw - 26, g_pequena), r.x + kw + 18, r.y + r.h / 2 - 14, BLANCO, g_pequena);
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

// La casa y la habitación (Y): elegir casa y habitación, papel, suelo, tamaño, tejado, fachada, puerta y poner cosas.
void abrirCasa(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        const int c = e->casa;
        v.titulo = nombreCasa(c);
        v.explicacion = std::string("Habitación: ") + HABITACIONES[e->hab] + ". El tejado, la fachada y la puerta se ven en el juego al día siguiente.";
        std::vector<Boton> bs;
        auto fila = [&](const std::string& t, const std::string& d, std::function<void()> f, bool activo = true) {
            Boton b{{}, t, f, activo};
            b.detalle = d;
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
        fila("Papel pintado", nombrePared(casas::pared(principal(), c, e->hab, false)), [e] {
            buscarObjeto([e](const Objeto& o) {
                Objeto n = mapa::nada();
                n.id = o.id;
                casas::ponerPared(principal(), e->casa, e->hab, false, n);
                partida::cambiaPrincipal();
                cambiado("Papel pintado: " + nombre(n));
            }, "las paredes", 2);
        });
        fila("Suelo", nombrePared(casas::pared(principal(), c, e->hab, true)), [e] {
            buscarObjeto([e](const Objeto& o) {
                Objeto n = mapa::nada();
                n.id = o.id;
                casas::ponerPared(principal(), e->casa, e->hab, true, n);
                partida::cambiaPrincipal();
                cambiado("Suelo: " + nombre(n));
            }, "el suelo", 2);
        });
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
        auto aspecto = [&](int parte, const char* titulo, const Aspecto* lista, int n, const char* actual) {
            fila(titulo, actual, [e, parte, titulo, lista, n] {
                std::vector<std::string> ops;
                int ahora = -1;
                const uint16_t v = casas::aspecto(principal(), e->casa, parte);
                for (int i = 0; i < n; i++)
                {
                    ops.push_back(lista[i].nombre);
                    if (lista[i].valor == v)
                        ahora = i;
                }
                elegir(titulo, "Elige cómo quieres que sea.", ops, ahora, [e, parte, titulo, lista](int i) {
                    casas::ponerAspecto(principal(), e->casa, parte, lista[i].valor);
                    partida::cambiaPrincipal();
                    cambiado(std::string(titulo) + ": " + lista[i].nombre);
                });
            });
        };
        aspecto(0, "Tejado", TEJADOS, sizeof(TEJADOS) / sizeof(TEJADOS[0]), NOMBRE_DE(NOMBRES_TEJADO, casas::aspecto(principal(), c, 0)));
        aspecto(1, "Fachada", FACHADAS, sizeof(FACHADAS) / sizeof(FACHADAS[0]), NOMBRE_DE(NOMBRES_FACHADA, casas::aspecto(principal(), c, 1)));
        const uint16_t puerta = casas::aspecto(principal(), c, 2);
        const int diseno = disenoPuerta(puerta);
        const bool recta = diseno >= 0 && PUERTAS[diseno].recta == puerta;
        fila("Puerta", NOMBRE_DE(NOMBRES_PUERTA, puerta), [e, recta] {
            std::vector<std::string> ops;
            const int n = sizeof(PUERTAS) / sizeof(PUERTAS[0]);
            for (int i = 0; i < n; i++)
                ops.push_back(PUERTAS[i].nombre);
            elegir("Puerta", "Elige el diseño (la forma, en arco o recta, se cambia aparte).", ops, disenoPuerta(casas::aspecto(principal(), e->casa, 2)),
                [e, recta](int i) { ponerPuerta(e->casa, i, recta); });
        });
        fila("Forma de la puerta", diseno < 0 ? "—" : recta ? "Recta" : "En arco", [e, diseno, recta] {
            if (!recta && PUERTAS[diseno].recta < 0)
            {
                avisar("Esta puerta solo es en arco.", true);
                return;
            }
            ponerPuerta(e->casa, diseno, !recta);
        }, diseno >= 0);
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
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 150 + (int)(i / 2) * 88, 570, 78};
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
