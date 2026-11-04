// El mapa de la isla dibujado (ver mapa_pintor.hpp): el suelo pintado, y encima la plaza, los puentes y cuestas, los
// objetos y los edificios, como IslandMapView de NHSE para Mac. Se compone en la CPU por trozos de 256 × 256 puntos y
// cada trozo se sube a su textura solo cuando cambia algo de lo que se ve en él.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include "../core/mapa.hpp"
#include "../core/mapa_objetos.hpp"
#include "dibujos.hpp"
#include "retratos.hpp"
#include "foto.hpp"
#include "mapa_pintor.hpp"
#include "ui.hpp"

namespace mapa {

using namespace acnh;

namespace {

constexpr int LADO = 256;                       // puntos de cada trozo
constexpr int BORDE = 1;                        // un punto de más alrededor (para que no se vean costuras al ampliar)
constexpr int TEX = LADO + 2 * BORDE;
constexpr int TROZOS_W = MAPA_ANCHO * PX / LADO; // 9
constexpr int TROZOS_H = MAPA_ALTO * PX / LADO;  // 8
constexpr int SUELO_W = MAPA_ANCHO * PX, SUELO_H = MAPA_ALTO * PX;
constexpr int CASILLAS_TROZO = LADO / PX;        // 32

// ------------------------------------------------------------------------------------------ dibujar en la memoria
// Un trozo de puntos: «ox, oy» es el punto del mapa de su esquina. Las coordenadas que se dan son casillas de objeto.
struct Lamina {
    uint32_t* p;
    int w, h;
    double ox, oy;
    double e = PX; // puntos por casilla

    double px(double x) const { return x * e - ox; }
    double py(double y) const { return y * e - oy; }

    void punto(int x, int y, uint32_t c, int a)
    {
        uint32_t& d = p[y * w + x];
        if (a >= 255)
        {
            d = 0xFF000000u | c;
            return;
        }
        // Sobre lo que aún no tiene nada (alfa 0: una forma suelta), queda con su alfa.
        const int da = (int)(d >> 24);
        if (da == 0)
        {
            d = ((uint32_t)a << 24) | (c & 0xFFFFFF);
            return;
        }
        const int r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
        const int g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
        const int b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
        const uint32_t na = (uint32_t)(a + da * (255 - a) / 255);
        d = (na << 24) | (r << 16) | (g << 8) | b;
    }

    // Filas de puntos cuyo centro cae dentro (en puntos del trozo).
    template <class F>
    void rellenar(double x0, double y0, double x1, double y1, uint32_t c, int a, F dentro)
    {
        const int ya = std::max(0, (int)std::floor(y0)), yb = std::min(h - 1, (int)std::ceil(y1));
        const int xa = std::max(0, (int)std::floor(x0)), xb = std::min(w - 1, (int)std::ceil(x1));
        for (int y = ya; y <= yb; y++)
            for (int x = xa; x <= xb; x++)
                if (dentro(x + 0.5, y + 0.5))
                    punto(x, y, c, a);
    }

    void caja(double x, double y, double cw, double ch, uint32_t c, int a = 255)
    {
        const double x0 = px(x), y0 = py(y), x1 = px(x + cw), y1 = py(y + ch);
        rellenar(x0, y0, x1, y1, c, a, [&](double X, double Y) { return X >= x0 && X < x1 && Y >= y0 && Y < y1; });
    }

    void cajaRedonda(double x, double y, double cw, double ch, double radio, uint32_t c, int a = 255)
    {
        const double x0 = px(x), y0 = py(y), x1 = px(x + cw), y1 = py(y + ch), r = radio * e;
        rellenar(x0, y0, x1, y1, c, a, [&](double X, double Y) {
            if (X < x0 || X >= x1 || Y < y0 || Y >= y1)
                return false;
            const double cx = std::clamp(X, x0 + r, x1 - r), cy = std::clamp(Y, y0 + r, y1 - r);
            return (X - cx) * (X - cx) + (Y - cy) * (Y - cy) <= r * r;
        });
    }

    void elipse(double cx, double cy, double rx, double ry, uint32_t c, int a = 255)
    {
        const double X0 = px(cx), Y0 = py(cy), RX = rx * e, RY = ry * e;
        if (RX <= 0 || RY <= 0)
            return;
        rellenar(X0 - RX, Y0 - RY, X0 + RX, Y0 + RY, c, a, [&](double X, double Y) {
            const double dx = (X - X0) / RX, dy = (Y - Y0) / RY;
            return dx * dx + dy * dy <= 1;
        });
    }

    // Un anillo de elipse de grosor «g» (casillas).
    void aro(double cx, double cy, double rx, double ry, double g, uint32_t c, int a = 255)
    {
        const double X0 = px(cx), Y0 = py(cy), RX = rx * e, RY = ry * e, G = g * e / 2;
        rellenar(X0 - RX - G, Y0 - RY - G, X0 + RX + G, Y0 + RY + G, c, a, [&](double X, double Y) {
            const double dx = (X - X0) / (RX + G), dy = (Y - Y0) / (RY + G);
            const double ex = (X - X0) / std::max(0.1, RX - G), ey = (Y - Y0) / std::max(0.1, RY - G);
            return dx * dx + dy * dy <= 1 && ex * ex + ey * ey > 1;
        });
    }

    // Un polígono convexo (puntos en casillas, en orden).
    void poligono(const double* xs, const double* ys, int n, uint32_t c, int a = 255)
    {
        double X[8], Y[8], x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (int i = 0; i < n; i++)
        {
            X[i] = px(xs[i]), Y[i] = py(ys[i]);
            x0 = std::min(x0, X[i]), x1 = std::max(x1, X[i]), y0 = std::min(y0, Y[i]), y1 = std::max(y1, Y[i]);
        }
        double area = 0;
        for (int i = 0; i < n; i++)
            area += X[i] * Y[(i + 1) % n] - X[(i + 1) % n] * Y[i];
        const double s = area >= 0 ? 1 : -1;
        rellenar(x0, y0, x1, y1, c, a, [&](double px_, double py_) {
            for (int i = 0; i < n; i++)
            {
                const int j = (i + 1) % n;
                if (s * ((X[j] - X[i]) * (py_ - Y[i]) - (Y[j] - Y[i]) * (px_ - X[i])) < 0)
                    return false;
            }
            return true;
        });
    }

    void linea(double xa, double ya, double xb, double yb, double g, uint32_t c, int a = 255)
    {
        const double dx = xb - xa, dy = yb - ya, l = std::sqrt(dx * dx + dy * dy);
        if (l <= 0)
            return;
        const double nx = -dy / l * g / 2, ny = dx / l * g / 2;
        const double xs[4] = {xa + nx, xb + nx, xb - nx, xa - nx}, ys[4] = {ya + ny, yb + ny, yb - ny, ya - ny};
        poligono(xs, ys, 4, c, a);
    }
};

uint32_t oscuro(uint32_t c, double f)
{
    return ((uint32_t)(((c >> 16) & 255) * f) << 16) | ((uint32_t)(((c >> 8) & 255) * f) << 8) | (uint32_t)((c & 255) * f);
}

uint32_t claro(uint32_t c, double f)
{
    auto k = [f](uint32_t v) { return (uint32_t)std::min(255.0, v + (255 - v) * f); };
    return (k((c >> 16) & 255) << 16) | (k((c >> 8) & 255) << 8) | k(c & 255);
}

const uint32_t SOMBRA = 0x103010;
constexpr int SOMBRA_A = 0x38;

// ------------------------------------------------------------------------------------------------- objetos
struct Dibujo {
    double x, y, w, h; // casillas del mapa entero
    Objeto o;
};

void dibujarObjeto(Lamina& l, const Dibujo& d, bool encima, int a)
{
    const Objeto& o = d.o;
    const double x = d.x, y = d.y, w = d.w, h = d.h, cx = x + w / 2, cy = y + h / 2, abajo = y + h;
    const auto at = [a](int v) { return v * a / 255; };
    if (enterrado(o))
    {
        // La grieta que el juego pone sobre lo enterrado.
        const uint32_t c = 0x6B4E30;
        const double s = w * 0.22;
        l.linea(cx - s, cy - s, cx + s, cy + s, 0.16, c, a);
        l.linea(cx - s, cy + s, cx + s, cy - s, 0.16, c, a);
        l.linea(cx, cy - s * 1.2, cx, cy + s * 1.2, 0.16, c, a);
        return;
    }
    const int p = plantaDe(o.id);
    if (p < 0)
    {
        const uint32_t c = colorCategoria(categoria(o.id));
        const double pad = std::min(w, h) * (encima ? 0.12 : 0.04) + 0.1;
        const double r = std::min(w, h) * 0.18;
        l.cajaRedonda(x + pad, y + pad, w - 2 * pad, h - 2 * pad, r, oscuro(c, 0.62), a);
        l.cajaRedonda(x + pad + 0.18, y + pad + 0.18, w - 2 * pad - 0.36, h - 2 * pad - 0.36, std::max(0.0, r - 0.18), c, a);
        l.caja(x + pad + 0.4, y + pad + 0.35, std::max(0.0, (w - 2 * pad) * 0.35), 0.18, claro(c, 0.45), at(160));
        return;
    }
    const uint32_t color = PLANTAS[p].color;
    switch (PLANTAS[p].aspecto)
    {
        case 1: // árbol: más alto que su cuadro, con sombra al pie
        {
            l.elipse(cx + 0.15, abajo - 0.35, w * 0.42, h * 0.2, SOMBRA, at(SOMBRA_A));
            const double t = w * 1.6;
            l.caja(cx - t * 0.06, abajo - t * 0.42, t * 0.12, t * 0.36, 0x7A5232, a);
            const double copaY = abajo - t * 0.6, r = t * 0.34;
            l.elipse(cx, copaY, r, r * 0.92, oscuro(color, 0.7), a);
            l.elipse(cx, copaY - 0.08, r - 0.18, r * 0.92 - 0.2, color, a);
            l.elipse(cx - r * 0.3, copaY - r * 0.35, r * 0.35, r * 0.25, claro(color, 0.3), at(170));
            break;
        }
        case 2: // arbusto
        {
            l.elipse(cx + 0.1, abajo - 0.3, w * 0.4, h * 0.18, SOMBRA, at(SOMBRA_A));
            const double t = w * 1.2, r = t * 0.3, by = abajo - t * 0.35;
            const uint32_t hoja = 0x4E8F3E;
            l.elipse(cx - r * 0.6, by + r * 0.2, r, r * 0.85, oscuro(hoja, 0.85), a);
            l.elipse(cx + r * 0.6, by + r * 0.2, r, r * 0.85, oscuro(hoja, 0.85), a);
            l.elipse(cx, by - r * 0.3, r, r * 0.9, hoja, a);
            for (int k = 0; k < 4; k++)
                l.elipse(cx + std::cos(k * 1.7 + 0.5) * r * 0.8, by + std::sin(k * 1.7 + 0.5) * r * 0.5 - r * 0.1, r * 0.22, r * 0.22, color, a);
            break;
        }
        case 3: // flor
        {
            const double r = w * 0.16, d = w * 0.17;
            for (int k = 0; k < 4; k++)
            {
                const double ang = k * 1.5708 + 0.785;
                l.elipse(cx + std::cos(ang) * d, cy + std::sin(ang) * d, r, r, oscuro(color, 0.8), a);
                l.elipse(cx + std::cos(ang) * d, cy + std::sin(ang) * d, r - 0.12, r - 0.12, color, a);
            }
            l.elipse(cx, cy, w * 0.09, w * 0.09, 0xF2D24B, a);
            break;
        }
        case 4: // tocón
            l.elipse(cx, cy, w * 0.3, h * 0.26, 0x6E4B2C, a);
            l.elipse(cx, cy, w * 0.3 - 0.12, h * 0.26 - 0.12, 0xA87A4E, a);
            l.aro(cx, cy, w * 0.15, h * 0.12, 0.08, 0xD9B382, a);
            break;
        case 5: // roca
        {
            const double rx = x + w * 0.08, ry = y + h * 0.15, rw = w * 0.84, rh = h * 0.75;
            l.cajaRedonda(rx + 0.15, ry + 0.2, rw, rh, std::min(w * 0.35, h * 0.3), SOMBRA, at(SOMBRA_A));
            l.cajaRedonda(rx, ry, rw, rh, std::min(w * 0.35, h * 0.3), 0x6C6E74, a);
            l.cajaRedonda(rx + 0.12, ry + 0.12, rw - 0.24, rh - 0.24, std::min(w * 0.35, h * 0.3) - 0.12, 0x9C9EA3, a);
            l.elipse(rx + rw * 0.35, ry + rh * 0.3, rw * 0.18, rh * 0.12, 0xFFFFFF, at(0x70));
            break;
        }
        case 6: // hoyo
            l.elipse(cx, cy, w * 0.32, h * 0.25, 0x5A402A, a);
            break;
        case 7: // mala hierba
        {
            const uint32_t c = 0x5E9A34;
            for (int k = -1; k <= 1; k++)
            {
                const double bx = cx + k * w * 0.18;
                const double xs[3] = {bx - w * 0.1, bx + w * 0.1, bx + k * w * 0.08}, ys[3] = {cy + h * 0.25, cy + h * 0.25, cy - h * 0.25};
                l.poligono(xs, ys, 3, k ? c : claro(c, 0.15), a);
            }
            break;
        }
        default: // vallas y lo demás: un cuadro de su color
        {
            const double pad = std::min(w, h) * 0.12;
            l.cajaRedonda(x + pad, y + pad, w - 2 * pad, h - 2 * pad, std::min(w, h) * 0.12, oscuro(color, 0.65), a);
            l.cajaRedonda(x + pad + 0.15, y + pad + 0.15, w - 2 * pad - 0.3, h - 2 * pad - 0.3, std::min(w, h) * 0.08, color, a);
            break;
        }
    }
}

// --------------------------------------------------------------------------------------------------- edificios
void dibujarCruce(Lamina& l, const Edificio& b)
{
    const double ang = b.angulo * 3.14159265358979 / 180, c = std::cos(ang), s = std::sin(ang);
    auto rot = [&](double dx, double dy, double& X, double& Y) { X = b.x + dx * c - dy * s, Y = b.y + dx * s + dy * c; };
    auto caja = [&](double x0, double y0, double x1, double y1, uint32_t col, int a) {
        double xs[4], ys[4];
        rot(x0, y0, xs[0], ys[0]);
        rot(x1, y0, xs[1], ys[1]);
        rot(x1, y1, xs[2], ys[2]);
        rot(x0, y1, xs[3], ys[3]);
        l.poligono(xs, ys, 4, col, a);
    };
    auto raya = [&](double x0, double y0, double x1, double y1, double g, uint32_t col) {
        double ax, ay, bx, by;
        rot(x0, y0, ax, ay);
        rot(x1, y1, bx, by);
        l.linea(ax, ay, bx, by, g, col);
    };
    const double hw = b.w / 2, hh = b.h / 2;
    caja(-hw + 0.3, -hh + 0.4, hw + 0.3, hh + 0.4, SOMBRA, SOMBRA_A);
    if (b.tipo == 26)
    {
        caja(-hw, -hh, hw, hh, oscuro(b.color, 0.65), 255);
        caja(-hw + 0.18, -hh + 0.18, hw - 0.18, hh - 0.18, b.color, 255);
        for (double x = -hw + 1; x < hw - 0.5; x += 1)
            raya(x, -hh + 0.35, x, hh - 0.35, 0.1, oscuro(b.color, 0.8));
        raya(-hw + 0.3, -hh + 0.25, hw - 0.3, -hh + 0.25, 0.35, oscuro(b.color, 0.55));
        raya(-hw + 0.3, hh - 0.25, hw - 0.3, hh - 0.25, 0.35, oscuro(b.color, 0.55));
    }
    else
    {
        caja(-hw, -hh, hw, hh, oscuro(b.color, 0.7), 255);
        caja(-hw + 0.18, -hh + 0.18, hw - 0.18, hh - 0.18, b.color, 255);
        for (double x = -hw + 0.9; x < hw - 0.4; x += 0.9)
            raya(x, -hh + 0.2, x, hh - 0.2, 0.14, oscuro(b.color, 0.78));
    }
}

void dibujarEdificio(Lamina& l, const Edificio& b)
{
    const double x = b.x - b.w / 2, y = b.y - b.h / 2, w = b.w, h = b.h;
    if (b.tipo == 25 || b.tipo == 23)
    {
        // Tienda de campaña.
        const double xs[3] = {b.x, x + w * 0.95, x + w * 0.05}, ys[3] = {y + h * 0.05, y + h * 0.92, y + h * 0.92};
        const double sx[3] = {xs[0] + 0.45, xs[1] + 0.45, xs[2] + 0.45}, sy[3] = {ys[0] + 0.5, ys[1] + 0.5, ys[2] + 0.5};
        l.poligono(sx, sy, 3, SOMBRA, SOMBRA_A);
        l.poligono(xs, ys, 3, oscuro(b.color, 0.62));
        const double ix[3] = {b.x, x + w * 0.95 - 0.35, x + w * 0.05 + 0.35}, iy[3] = {y + h * 0.05 + 0.4, y + h * 0.92 - 0.2, y + h * 0.92 - 0.2};
        l.poligono(ix, iy, 3, b.color);
        const double dx[3] = {b.x, b.x + w * 0.12, b.x - w * 0.12}, dy[3] = {y + h * 0.45, y + h * 0.92, y + h * 0.92};
        l.poligono(dx, dy, 3, oscuro(b.color, 0.55));
    }
    else
    {
        // Una casa vista desde arriba y un poco de frente: paredes, un tejado grande y la puerta.
        const double wx = x + w * 0.1, wy = y + h * 0.3, ww = w * 0.8, wh = h * 0.66;
        l.cajaRedonda(wx + 0.45, wy + 0.55, ww, wh, 0.5, SOMBRA, SOMBRA_A);
        l.cajaRedonda(wx, wy, ww, wh, 0.35, 0x9C8B73);
        l.cajaRedonda(wx + 0.15, wy + 0.15, ww - 0.3, wh - 0.3, 0.25, 0xF6EEDF);
        l.cajaRedonda(b.x - w * 0.09, wy + wh - h * 0.3, w * 0.18, h * 0.3, 0.25, 0x8A5A36);
        const double ih = h * 0.66, in = w * 0.12;
        const double xs[4] = {x + in, x + w - in, x + w, x}, ys[4] = {y, y, y + ih, y + ih};
        l.poligono(xs, ys, 4, oscuro(b.color, 0.62));
        const double xi[4] = {x + in + 0.2, x + w - in - 0.2, x + w - 0.25, x + 0.25}, yi[4] = {y + 0.18, y + 0.18, y + ih - 0.18, y + ih - 0.18};
        l.poligono(xi, yi, 4, b.color);
        l.linea(x + 0.2, y + ih - 0.2, x + w - 0.2, y + ih - 0.2, 0.3, oscuro(b.color, 0.75));
        l.linea(x + w * 0.2, y + 0.35, x + w - w * 0.2, y + 0.35, 0.25, 0xFFFFFF, 0x60);
    }
    // Un círculo en medio, como el sitio de la cara o el dibujo del edificio en el Mac.
    const double t = std::min(w, h) * 0.5;
    const double cx = b.x, cy = y + h * 0.36;
    l.elipse(cx, cy, t / 2, t / 2, oscuro(b.color, 0.7));
    l.elipse(cx, cy, t / 2 - 0.2, t / 2 - 0.2, 0xFFFFFF, 0xF0);
    l.elipse(cx, cy, t / 4, t / 4, b.color);
}

} // namespace

uint32_t colorCategoria(int c)
{
    static const uint32_t COLORES[14] = {0xB8A88A, 0xB98552, 0xD6B98C, 0xB07CC6, 0x7E8EA0, 0xA88B5C, 0xE07A5A, 0x5A9AD8,
        0x8AAA4A, 0x4AB0B8, 0xD8C8A0, 0xD8B040, 0x6CB45A, 0xF0E6C8};
    return COLORES[c >= 0 && c < 14 ? c : 0];
}

// ----------------------------------------------------------------------------------------------------------- lienzo
struct Lienzo::Trozo {
    SDL_Texture* t = nullptr;    // con formas y colores (CPU, 8 puntos por casilla)
    SDL_Texture* foto = nullptr; // con los dibujos de los objetos (GPU, 16 puntos por casilla), solo los vistos hace poco
    bool fotoSucia = true;
    uint32_t usado = 0;
    ~Trozo()
    {
        if (t)
            SDL_DestroyTexture(t);
        if (foto)
            SDL_DestroyTexture(foto);
    }
};

Lienzo::Lienzo() = default;
Lienzo::~Lienzo() = default;

static std::vector<Edificio> g_edificios;
static uint32_t g_plaza[2];
static std::vector<SDL_Texture*> g_texEdificios; // cada edificio dibujado (para los trozos con dibujos)

void ponerEdificios(const std::vector<Edificio>& e, uint32_t plazaX, uint32_t plazaZ)
{
    g_edificios = e;
    g_plaza[0] = plazaX, g_plaza[1] = plazaZ;
    for (auto* t : g_texEdificios)
        if (t)
            SDL_DestroyTexture(t);
    g_texEdificios.assign(e.size(), nullptr);
}

void Lienzo::preparar(std::vector<uint8_t>& main)
{
    uint32_t suma = 2166136261u;
    for (uint32_t i = TERRENO; i < TERRENO + TERRENO_ANCHO * TERRENO_ALTO * 14; i++)
        suma = (suma ^ main[i]) * 16777619u;
    for (uint32_t i = ZONAS; i < ZONAS + 144; i++)
        suma = (suma ^ main[i]) * 16777619u;
    if (!suelo.empty() && suma == sumaTerreno)
        return;
    suelo = pintarSuelo(main);
    sumaTerreno = suma;
    visto.clear();
    trozos.clear();
    for (int i = 0; i < TROZOS_W * TROZOS_H; i++)
        trozos.push_back(std::make_unique<Trozo>());
    sucio.assign(TROZOS_W * TROZOS_H, true);
}

void Lienzo::marcar(int t)
{
    sucio[t] = true;
    trozos[t]->fotoSucia = true;
}

void Lienzo::actualizar(std::vector<uint8_t>& main, int capa)
{
    const uint8_t* ahora = main.data() + CAMPO;
    if (visto.size() != 2 * TAM_CAPA || capa != capaVista)
    {
        visto.assign(ahora, ahora + 2 * TAM_CAPA);
        capaVista = capa;
        for (int t = 0; t < TROZOS_W * TROZOS_H; t++)
            marcar(t);
        return;
    }
    if (std::memcmp(visto.data(), ahora, 2 * TAM_CAPA) == 0)
        return;
    for (uint32_t i = 0; i < 2 * (uint32_t)ANCHO * ALTO; i++)
    {
        if (std::memcmp(visto.data() + i * 8, ahora + i * 8, 8) == 0)
            continue;
        const uint32_t k = i % (ANCHO * ALTO);
        const int x = k / ALTO + ORIGEN_X, y = k % ALTO + ORIGEN_Y;
        // Lo que se dibuja de una casilla puede salirse un poco (los árboles son más altos que su cuadro).
        const int ta = std::max(0, (x - 3) / CASILLAS_TROZO), tb = std::min(TROZOS_W - 1, (x + 4) / CASILLAS_TROZO);
        const int ua = std::max(0, (y - 4) / CASILLAS_TROZO), ub = std::min(TROZOS_H - 1, (y + 3) / CASILLAS_TROZO);
        for (int tx = ta; tx <= tb; tx++)
            for (int ty = ua; ty <= ub; ty++)
                marcar(ty * TROZOS_W + tx);
    }
    std::memcpy(visto.data(), ahora, 2 * TAM_CAPA);
}

// Lo de debajo de todo de un trozo: el suelo pintado, la plaza, y los puentes y cuestas (8 puntos por casilla, con su
// borde).
static void componerSuelo(const std::vector<uint32_t>& suelo, std::vector<uint32_t>& buf, int t, Lamina& l)
{
    const int tx = t % TROZOS_W, ty = t / TROZOS_W;
    buf.assign(TEX * TEX, 0);
    const int ox = tx * LADO - BORDE, oy = ty * LADO - BORDE;
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++)
        {
            const int sx = std::clamp(ox + x, 0, SUELO_W - 1), sy = std::clamp(oy + y, 0, SUELO_H - 1);
            buf[y * TEX + x] = suelo[sy * SUELO_W + sx];
        }
    l = Lamina{buf.data(), TEX, TEX, (double)ox, (double)oy};
    l.cajaRedonda(g_plaza[0], g_plaza[1], 24, 20, 2.5, 0xF2E6C8, 0x70);
    for (const auto& b : g_edificios)
        if (b.cruce())
            dibujarCruce(l, b);
}

// Las cosas de una capa que tocan una zona del mapa (casillas del mapa entero), de arriba abajo (las de delante tapan
// a las de detrás).
static std::vector<Dibujo> cosasEn(std::vector<uint8_t>& main, int c, double x0, double y0, double x1, double y1)
{
    Rejilla r = mapa::capa(main, c);
    std::vector<Dibujo> v;
    const int xa = std::max(0, (int)x0 - ORIGEN_X - 12), xb = std::min(ANCHO - 1, (int)x1 - ORIGEN_X + 2);
    const int ya = std::max(0, (int)y0 - ORIGEN_Y - 12), yb = std::min(ALTO - 1, (int)y1 - ORIGEN_Y + 4);
    for (int x = xa; x <= xb; x++)
        for (int y = ya; y <= yb; y++)
        {
            const Objeto o = r.leer(x, y);
            if (esNada(o) || !esRaiz(o))
                continue;
            int w, h;
            tamano(o, w, h);
            const double X = x + ORIGEN_X, Y = y + ORIGEN_Y;
            if (X + w + 1 < x0 || X - 1 > x1 || Y + h < y0 || Y - 2 > y1)
                continue;
            v.push_back({X, Y, (double)w, (double)h, o});
        }
    std::stable_sort(v.begin(), v.end(), [](const Dibujo& a, const Dibujo& b) { return a.y + a.h < b.y + b.h; });
    return v;
}

static SDL_Texture* texturaSuave(int w, int h, int acceso)
{
    const char* antes = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    const std::string guardado = antes ? antes : "";
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Texture* t = SDL_CreateTexture(ui::g_r, SDL_PIXELFORMAT_ARGB8888, acceso, w, h);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, guardado.empty() ? "0" : guardado.c_str());
    return t;
}

void Lienzo::componer(std::vector<uint8_t>& main, int t)
{
    std::vector<uint32_t> buf;
    Lamina l{nullptr, 0, 0, 0, 0};
    componerSuelo(suelo, buf, t, l);
    const double x0 = l.ox / (double)PX, y0 = l.oy / (double)PX, x1 = (l.ox + TEX) / (double)PX, y1 = (l.oy + TEX) / (double)PX;
    for (const auto& d : cosasEn(main, 0, x0, y0, x1, y1))
        dibujarObjeto(l, d, false, capaVista == 1 ? 115 : 255);
    for (const auto& b : g_edificios)
        if (!b.cruce() && b.x + b.w / 2 >= x0 && b.x - b.w / 2 <= x1 && b.y + b.h / 2 + 1 >= y0 && b.y - b.h / 2 <= y1)
            dibujarEdificio(l, b);
    for (const auto& d : cosasEn(main, 1, x0, y0, x1, y1))
        dibujarObjeto(l, d, true, 255);

    Trozo& tr = *trozos[t];
    if (!tr.t)
        tr.t = texturaSuave(TEX, TEX, SDL_TEXTUREACCESS_STATIC);
    if (tr.t)
        SDL_UpdateTexture(tr.t, nullptr, buf.data(), TEX * 4);
}

// ------------------------------------------------------------------------------------------ los trozos con dibujos
namespace {

constexpr int FP = 16;                      // puntos por casilla en los trozos con dibujos
constexpr int FBORDE = 2;
constexpr int FLADO = CASILLAS_TROZO * FP;  // 512
constexpr int FTEX = FLADO + 2 * FBORDE;
constexpr int MAX_FOTOS = 24;               // trozos con dibujos guardados (1 MB cada uno)
constexpr int FOTOS_POR_FOTOGRAMA = 3;      // los demás se ven con formas hasta el siguiente

// Una forma de las de dibujarObjeto en su propia textura (para los trozos con dibujos): las cosas sin dibujo, y las
// que el Mac también dibuja con formas (tocones, rocas, hoyos, lo enterrado). Cubre de (-1, -2·alto) a (ancho + 1,
// alto + 1) casillas desde su esquina.
SDL_Texture* forma(const Objeto& o, int w, int h, bool encima)
{
    static std::map<uint64_t, SDL_Texture*> hechas;
    const uint64_t clave = ((uint64_t)o.id << 32) | ((uint64_t)(o.sistema & 0x04) << 24) | (w << 16) | (h << 8) | (encima ? 1 : 0);
    auto it = hechas.find(clave);
    if (it != hechas.end())
        return it->second;
    if (hechas.size() > 300)
    {
        for (auto& x : hechas)
            if (x.second)
                SDL_DestroyTexture(x.second);
        hechas.clear();
    }
    const int tw = (w + 2) * FP, th = (3 * h + 1) * FP;
    std::vector<uint32_t> buf(tw * th, 0);
    // Se dibuja sobre un fondo «transparente» marcado (alfa 0), y lo pintado queda opaco.
    Lamina l{buf.data(), tw, th, -1.0 * FP, -2.0 * h * FP};
    l.e = FP;
    dibujarObjeto(l, {0, 0, (double)w, (double)h, o}, encima, 255);
    // dibujarObjeto mezcla con lo que hay: lo que tocó tiene alfa 255; lo mezclado con el fondo (sombras, brillos a
    // medias) se queda con su alfa para que se vea el suelo.
    SDL_Texture* t = texturaSuave(tw, th, SDL_TEXTUREACCESS_STATIC);
    if (t)
    {
        SDL_UpdateTexture(t, nullptr, buf.data(), tw * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return hechas[clave] = t;
}

// La sombra al pie de árboles y arbustos.
SDL_Texture* sombra()
{
    static SDL_Texture* t = [] {
        constexpr int W = 64, H = 32;
        std::vector<uint32_t> buf(W * H, 0);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                const double dx = (x + 0.5 - W / 2.0) / (W / 2.0), dy = (y + 0.5 - H / 2.0) / (H / 2.0);
                if (dx * dx + dy * dy <= 1)
                    buf[y * W + x] = ((uint32_t)SOMBRA_A << 24) | SOMBRA;
            }
        SDL_Texture* s = texturaSuave(W, H, SDL_TEXTUREACCESS_STATIC);
        if (s)
        {
            SDL_UpdateTexture(s, nullptr, buf.data(), W * 4);
            SDL_SetTextureBlendMode(s, SDL_BLENDMODE_BLEND);
        }
        return s;
    }();
    return t;
}

// Un edificio dibujado en su propia textura (con un poco de margen para la sombra).
SDL_Texture* edificio(size_t i)
{
    if (i >= g_texEdificios.size())
        return nullptr;
    if (g_texEdificios[i])
        return g_texEdificios[i];
    const Edificio& b = g_edificios[i];
    const double x0 = b.x - b.w / 2 - 1, y0 = b.y - b.h / 2 - 1;
    const int tw = (int)std::ceil((b.w + 2) * FP), th = (int)std::ceil((b.h + 2) * FP);
    std::vector<uint32_t> buf(tw * th, 0);
    Lamina l{buf.data(), tw, th, x0 * FP, y0 * FP};
    l.e = FP;
    dibujarEdificio(l, b);
    SDL_Texture* t = texturaSuave(tw, th, SDL_TEXTUREACCESS_STATIC);
    if (t)
    {
        SDL_UpdateTexture(t, nullptr, buf.data(), tw * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return g_texEdificios[i] = t;
}

// El dibujo que lleva cada edificio en el círculo (como el Mac: el buzón en las casas, la hoja en los servicios...).
uint16_t iconoEdificio(int tipo)
{
    if (tipo >= 1 && tipo <= 8)
        return 2571; // buzón
    switch (tipo)
    {
        case 19: return 3076;  // bolsa de papel
        case 20: case 23: return 2; // hoja
        case 21: return 2579;  // fósil
        case 22: case 42: return 5851; // billete de avión
        case 24: return 127;   // vestido
        case 28: return 14046; // figurilla
        case 29: return 14695; // cámara
        default: return 0;
    }
}

// Dentro de «d», con su forma, centrado.
SDL_FRect encajado(SDL_Texture* t, SDL_FRect d)
{
    int w = 1, h = 1;
    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
    const float s = std::min(d.w / w, d.h / h);
    return {d.x + (d.w - w * s) / 2, d.y + (d.h - h * s) / 2, w * s, h * s};
}

} // namespace

// Los dibujos se ven ampliados y reducidos: suaves, como el Mac (se queda así para todos).
static SDL_Texture* suave(SDL_Texture* t)
{
    if (t)
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return t;
}

void Lienzo::componerFoto(std::vector<uint8_t>& main, int t)
{
    SDL_Renderer* g = ui::g_r;
    Trozo& tr = *trozos[t];
    if (!tr.foto)
    {
        // Si ya hay muchos, se reutiliza el que lleva más tiempo sin verse.
        int fotos = 0, viejo = -1;
        for (int i = 0; i < (int)trozos.size(); i++)
            if (trozos[i]->foto)
            {
                fotos++;
                if (viejo < 0 || trozos[i]->usado < trozos[viejo]->usado)
                    viejo = i;
            }
        if (fotos >= MAX_FOTOS && viejo >= 0)
        {
            tr.foto = trozos[viejo]->foto;
            trozos[viejo]->foto = nullptr;
            trozos[viejo]->fotoSucia = true;
        }
        else
            tr.foto = texturaSuave(FTEX, FTEX, SDL_TEXTUREACCESS_TARGET);
        if (!tr.foto)
            return;
    }
    // El suelo, la plaza y los cruces, ampliados (se pintan en la CPU a 8 puntos por casilla).
    static SDL_Texture* bajo = nullptr;
    if (!bajo)
        bajo = texturaSuave(TEX, TEX, SDL_TEXTUREACCESS_STREAMING);
    std::vector<uint32_t> buf;
    Lamina l{nullptr, 0, 0, 0, 0};
    componerSuelo(suelo, buf, t, l);
    if (bajo)
        SDL_UpdateTexture(bajo, nullptr, buf.data(), TEX * 4);

    SDL_Texture* antes = SDL_GetRenderTarget(g);
    SDL_SetRenderTarget(g, tr.foto);
    SDL_SetRenderDrawBlendMode(g, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g, 0, 0, 0, 255);
    SDL_RenderClear(g);
    const int tx = t % TROZOS_W, ty = t / TROZOS_W;
    const double ox = tx * FLADO - FBORDE, oy = ty * FLADO - FBORDE; // punto del mapa (a 16 por casilla) de la esquina
    if (bajo)
    {
        // El trozo de abajo empieza 1 punto (de 8) antes; aquí, 2 (de 16): la misma casilla.
        SDL_FRect d = {(float)(-BORDE * 2 - ox + tx * LADO * 2), (float)(-BORDE * 2 - oy + ty * LADO * 2), (float)(TEX * 2), (float)(TEX * 2)};
        SDL_RenderCopyF(g, bajo, nullptr, &d);
    }
    auto pantalla = [&](double x, double y, double w, double h) {
        return SDL_FRect{(float)(x * FP - ox), (float)(y * FP - oy), (float)(w * FP), (float)(h * FP)};
    };
    const double x0 = ox / FP, y0 = oy / FP, x1 = (ox + FTEX) / FP, y1 = (oy + FTEX) / FP;
    auto cosas = [&](int c, bool encima, uint8_t alfa) {
        for (const auto& d : cosasEn(main, c, x0, y0, x1, y1))
        {
            const Objeto& o = d.o;
            const int p = plantaDe(o.id);
            const int aspecto = enterrado(o) ? -1 : p >= 0 ? PLANTAS[p].aspecto : 0;
            SDL_Texture* foto = aspecto == -1 || aspecto == 4 || aspecto == 5 || aspecto == 6 ? nullptr : dibujoDe(o);
            const double cx = d.x + d.w / 2, abajo = d.y + d.h;
            if (!foto)
            {
                if (SDL_Texture* f = forma(o, (int)d.w, (int)d.h, encima))
                {
                    SDL_SetTextureAlphaMod(f, alfa);
                    SDL_FRect r = pantalla(d.x - 1, d.y - 2 * d.h, d.w + 2, 3 * d.h + 1);
                    SDL_RenderCopyF(g, f, nullptr, &r);
                    SDL_SetTextureAlphaMod(f, 255);
                }
                continue;
            }
            SDL_FRect r;
            if (aspecto == 1 || aspecto == 2)
            {
                // Los árboles y arbustos, más altos que su cuadro y con sombra al pie (IslandMapView.DrawItems).
                const bool arbol = aspecto == 1;
                const double sx = d.w * (arbol ? 0.42 : 0.4), sy = d.h * (arbol ? 0.2 : 0.18);
                if (SDL_Texture* s = sombra())
                {
                    SDL_SetTextureAlphaMod(s, alfa);
                    SDL_FRect sr = pantalla(cx + (arbol ? 0.15 : 0.1) - sx, abajo - (arbol ? 0.35 : 0.3) - sy, 2 * sx, 2 * sy);
                    SDL_RenderCopyF(g, s, nullptr, &sr);
                    SDL_SetTextureAlphaMod(s, 255);
                }
                const double lado = d.w * (arbol ? 1.6 : 1.2);
                r = encajado(foto, pantalla(cx - lado / 2, abajo - lado + 0.1, lado, lado));
            }
            else
            {
                const double pad = std::min(d.w, d.h) * (encima ? 0.12 : 0.04);
                r = encajado(foto, pantalla(d.x + pad, d.y + pad, d.w - 2 * pad, d.h - 2 * pad));
            }
            SDL_SetTextureAlphaMod(foto, alfa);
            SDL_RenderCopyF(g, foto, nullptr, &r);
            SDL_SetTextureAlphaMod(foto, 255);
        }
    };
    cosas(0, false, capaVista == 1 ? 115 : 255);
    for (size_t i = 0; i < g_edificios.size(); i++)
    {
        const Edificio& b = g_edificios[i];
        if (b.cruce() || b.x + b.w / 2 + 1 < x0 || b.x - b.w / 2 - 1 > x1 || b.y + b.h / 2 + 1 < y0 || b.y - b.h / 2 - 1 > y1)
            continue;
        if (SDL_Texture* e = edificio(i))
        {
            SDL_FRect r = pantalla(b.x - b.w / 2 - 1, b.y - b.h / 2 - 1, std::ceil((b.w + 2) * FP) / FP, std::ceil((b.h + 2) * FP) / FP);
            SDL_RenderCopyF(g, e, nullptr, &r);
        }
        // Las casas de los vecinos llevan su cara (como el Mac); las demás, su dibujo.
        SDL_Texture* cara = nullptr;
        if (b.tipo >= 1 && b.tipo <= 8)
            cara = suave(foto::deJugador(b.tipo - 1)); // the houses of the players, with their photo (as the Mac)
        if (b.tipo >= 9 && b.tipo <= 18)
        {
            const int hueco = (int8_t)main[acnh::CASAS + (b.tipo - 9) * acnh::TAM_CASA + 0x1C4];
            if (hueco >= 0 && hueco < acnh::NUM_VECINOS)
                cara = suave(retratos::de(acnh::vecinoEn(main, hueco)));
        }
        if (cara)
        {
            const double lado = std::min(b.w, b.h) * 0.78 * 0.7;
            SDL_FRect r = encajado(cara, pantalla(b.x - lado / 2, b.y - b.h / 2 + b.h * 0.36 - lado / 2, lado, lado));
            SDL_RenderCopyF(g, cara, nullptr, &r);
        }
        else if (const uint16_t icono = iconoEdificio(b.tipo))
            if (SDL_Texture* f = suave(dibujos::de(icono)))
            {
                const double lado = std::min(b.w, b.h) * 0.5 * 0.7;
                SDL_FRect r = encajado(f, pantalla(b.x - lado / 2, b.y - b.h / 2 + b.h * 0.36 - lado / 2, lado, lado));
                SDL_RenderCopyF(g, f, nullptr, &r);
            }
    }
    cosas(1, true, 255);
    SDL_SetRenderTarget(g, antes);
    tr.fotoSucia = false;
}

void Lienzo::dibujar(std::vector<uint8_t>& main, const SDL_Rect& d, double x0, double y0, double zoom)
{
    if (suelo.empty())
        return;
    const uint32_t mar = colorMar();
    ui::rect(d, {(uint8_t)(mar >> 16), (uint8_t)(mar >> 8), (uint8_t)mar, 255});
    // A puntos de pantalla, redondeando los bordes igual en los trozos vecinos (sin huecos entre ellos).
    auto sx = [&](double casilla) { return (int)std::lround(d.x + (casilla - x0) * zoom); };
    auto sy = [&](double casilla) { return (int)std::lround(d.y + (casilla - y0) * zoom); };
    const bool fotos = zoom >= ZOOM_DIBUJOS;
    reloj++;
    // Primero se hace lo que haga falta (cambiar de destino de dibujo con un recorte puesto no siempre lo respeta).
    std::vector<int> vistos;
    int hechas = 0;
    for (int ty = 0; ty < TROZOS_H; ty++)
        for (int tx = 0; tx < TROZOS_W; tx++)
        {
            const int a = sx(tx * CASILLAS_TROZO), b = sx((tx + 1) * CASILLAS_TROZO);
            const int c = sy(ty * CASILLAS_TROZO), e = sy((ty + 1) * CASILLAS_TROZO);
            if (b <= d.x || a >= d.x + d.w || e <= d.y || c >= d.y + d.h)
                continue;
            const int t = ty * TROZOS_W + tx;
            vistos.push_back(t);
            Trozo& tr = *trozos[t];
            tr.usado = reloj;
            if (fotos && (tr.fotoSucia || !tr.foto) && hechas < FOTOS_POR_FOTOGRAMA)
            {
                componerFoto(main, t);
                hechas++;
            }
            if (sucio[t] && (!fotos || tr.fotoSucia || !tr.foto))
            {
                componer(main, t);
                sucio[t] = false;
            }
        }
    SDL_RenderSetClipRect(ui::g_r, &d);
    for (int t : vistos)
    {
        const int tx = t % TROZOS_W, ty = t / TROZOS_W;
        const int a = sx(tx * CASILLAS_TROZO), b = sx((tx + 1) * CASILLAS_TROZO);
        const int c = sy(ty * CASILLAS_TROZO), e = sy((ty + 1) * CASILLAS_TROZO);
        // Un punto más hacia la derecha y hacia abajo: las pantallas se dibujan escaladas, y al redondear quedarían
        // rayas finas entre trozos.
        const SDL_Rect dst = {a, c, b - a + 1, e - c + 1};
        Trozo& tr = *trozos[t];
        if (fotos && tr.foto && !tr.fotoSucia)
        {
            const SDL_Rect src = {FBORDE, FBORDE, FLADO, FLADO};
            SDL_RenderCopy(ui::g_r, tr.foto, &src, &dst);
        }
        else if (tr.t)
        {
            const SDL_Rect src = {BORDE, BORDE, LADO, LADO};
            SDL_RenderCopy(ui::g_r, tr.t, &src, &dst);
        }
    }
    SDL_RenderSetClipRect(ui::g_r, nullptr);
}

Lienzo& lienzo()
{
    static Lienzo l;
    return l;
}

// ----------------------------------------------------------------------------------------------- dibujos de objetos
uint16_t iconoDeEdificio(int tipo) { return iconoEdificio(tipo); }

SDL_Texture* dibujoDe(const Objeto& o)
{
    if (esNada(o))
        return nullptr;
    const uint16_t id = idVisible(o);
    const int p = plantaDe(id);
    if (p >= 0)
    {
        // Como GridItems.GetPicture: las rocas, los hoyos y los tocones con un dibujo de lo que son.
        if (PLANTAS[p].clase == 7)
            return suave(dibujos::de(2502)); // piedra
        if (PLANTAS[p].clase == 6)
            return suave(dibujos::de(773)); // pala
        if (PLANTAS[p].aspecto == 4)
            return suave(dibujos::de(2768)); // madera
    }
    return suave(dibujos::de(id, esExtension(o) ? 0 : o.cuenta));
}

} // namespace mapa
