// Piezas de dibujo del mapa y de las casas (ver mapa_vista.hpp).
#include "mapa_vista.hpp"
#include <algorithm>
#include <cmath>

using namespace ui;

namespace mapa {

const SDL_Color BORDE_MAC = {0xD1, 0xD1, 0xD6, 255};
const SDL_Color FONDO_FOTO = {0xDD, 0xEE, 0xE3, 255};
const SDL_Color GRIS_CHIP = {0xE3, 0xE3, 0xE8, 255};
const SDL_Color APAGADO_MAC = {0xAE, 0xAE, 0xB2, 255};

int tecla(const std::string& k, int x, int y)
{
    const int ancho = std::max(26, anchoTexto(k, g_pequena) + 12);
    redondo({x - ancho / 2, y - 13, ancho, 26}, 7, BORDE_MAC);
    redondo({x - ancho / 2 + 1, y - 12, ancho - 2, 23}, 6, {0xF4, 0xF4, 0xF7, 255});
    texto(k, x, y, MARRON_SUAVE, g_pequena, Centro);
    return ancho;
}

void dibujarChip(const Chip& c)
{
    const SDL_Rect& r = c.r;
    const int radio = std::min(10, r.h / 3);
    const SDL_Color fondo = c.marcado || c.principal ? (c.activo ? AZUL : SDL_Color{0x9C, 0xC4, 0xF2, 255})
                                                     : c.enGrupo ? SDL_Color{0, 0, 0, 0} : !c.activo ? SDL_Color{0xF0, 0xF0, 0xF3, 255} : GRIS_CHIP;
    if (fondo.a)
        redondo(r, radio, fondo);
    const SDL_Color letra = c.marcado || c.principal ? BLANCO : c.activo ? MARRON : APAGADO_MAC;
    // What goes inside: picture, text and key, centred together.
    const int lado = c.icono ? std::min(r.h - 12, 32) : 0;
    const int kw = c.tecla.empty() ? 0 : std::max(26, anchoTexto(c.tecla, g_pequena) + 12);
    const int libre = r.w - 16 - (lado ? lado + 6 : 0) - (kw ? kw + 8 : 0);
    const std::string t = encajar(c.texto, std::max(10, libre), g_pequena);
    const int tw = t.empty() ? 0 : anchoTexto(t, g_pequena);
    const int total = (lado ? lado + (tw ? 6 : 0) : 0) + tw + (kw ? kw + 8 : 0);
    int x = r.x + (r.w - total) / 2;
    if (lado)
    {
        if (!c.activo)
            SDL_SetTextureAlphaMod(c.icono, 110);
        SDL_Rect d = {x, r.y + (r.h - lado) / 2, lado, lado};
        SDL_RenderCopy(g_r, c.icono, nullptr, &d);
        SDL_SetTextureAlphaMod(c.icono, 255);
        x += lado + (tw ? 6 : 0);
    }
    if (tw)
    {
        texto(t, x, r.y + r.h / 2 - ui::alto(g_pequena) / 2, letra, g_pequena);
        x += tw;
    }
    if (kw)
        tecla(c.tecla, x + 8 + kw / 2, r.y + r.h / 2);
}

void Chips::dibujar() const
{
    for (const auto& c : v)
        dibujarChip(c);
}

bool Chips::encima(int x, int y) const
{
    for (const auto& c : v)
        if (x >= c.r.x && x < c.r.x + c.r.w && y >= c.r.y && y < c.r.y + c.r.h)
            return true;
    return false;
}

bool Chips::tocar(int x, int y) const
{
    // The last one drawn is on top.
    for (auto it = v.rbegin(); it != v.rend(); ++it)
        if (x >= it->r.x && x < it->r.x + it->r.w && y >= it->r.y && y < it->r.y + it->r.h)
        {
            if (it->activo && it->accion)
            {
                auto f = it->accion; // the action may change the chips
                f();
            }
            return true;
        }
    return false;
}

void segmentado(Chips& chips, const SDL_Rect& r, const std::vector<std::string>& textos, int elegido, std::function<void(int)> elegir,
    const std::vector<SDL_Texture*>& iconos)
{
    // A light grey bar, and each option in it; the chosen one in blue (NSSegmentedControl of the Mac).
    redondo(r, std::min(12, r.h / 3), GRIS_CHIP);
    const int n = std::max(1, (int)textos.size());
    // Each one as wide as its text needs, sharing the width.
    std::vector<int> anchos(n);
    int suma = 0;
    for (int i = 0; i < n; i++)
        suma += anchos[i] = anchoTexto(textos[i], g_pequena) + 24 + (i < (int)iconos.size() && iconos[i] ? 34 : 0);
    const double k = (double)(r.w - 6) / suma;
    double x = r.x + 3;
    for (int i = 0; i < n; i++)
    {
        const int w = (int)std::lround(anchos[i] * k);
        Chip c{{(int)std::lround(x), r.y + 3, i == n - 1 ? r.x + r.w - 3 - (int)std::lround(x) : w, r.h - 6}, textos[i], [elegir, i] { elegir(i); }};
        c.marcado = i == elegido;
        c.enGrupo = true;
        if (i < (int)iconos.size())
            c.icono = iconos[i];
        chips.poner(c);
        x += anchos[i] * k;
    }
}

void tarjeta(const SDL_Rect& r, int radio)
{
    redondo(r, radio, BORDE_MAC);
    redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, std::max(0, radio - 2), BLANCO);
}

void flotante(const SDL_Rect& r, int radio)
{
    redondo({r.x - 1, r.y + 1, r.w + 2, r.h + 3}, radio + 1, {0, 0, 0, 0x22});
    redondo({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, radio + 1, {0, 0, 0, 0x14});
    redondo(r, radio, BLANCO);
}

void esquinas(const SDL_Rect& r, int radio, SDL_Color c)
{
    constexpr int TRAMOS = 8;
    std::vector<SDL_Vertex> v;
    const float px[4] = {(float)r.x, (float)(r.x + r.w), (float)(r.x + r.w), (float)r.x};
    const float py[4] = {(float)r.y, (float)r.y, (float)(r.y + r.h), (float)(r.y + r.h)};
    const float cx[4] = {(float)(r.x + radio), (float)(r.x + r.w - radio), (float)(r.x + r.w - radio), (float)(r.x + radio)};
    const float cy[4] = {(float)(r.y + radio), (float)(r.y + radio), (float)(r.y + r.h - radio), (float)(r.y + r.h - radio)};
    const double desde[4] = {180, 270, 0, 90};
    for (int q = 0; q < 4; q++)
        for (int i = 0; i < TRAMOS; i++)
        {
            const double a0 = (desde[q] + i * 90.0 / TRAMOS) * 3.14159265358979 / 180, a1 = (desde[q] + (i + 1) * 90.0 / TRAMOS) * 3.14159265358979 / 180;
            v.push_back({{px[q], py[q]}, c, {0, 0}});
            v.push_back({{cx[q] + (float)(radio * std::cos(a0)), cy[q] + (float)(radio * std::sin(a0))}, c, {0, 0}});
            v.push_back({{cx[q] + (float)(radio * std::cos(a1)), cy[q] + (float)(radio * std::sin(a1))}, c, {0, 0}});
        }
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_r, nullptr, v.data(), (int)v.size(), nullptr, 0);
}

void foto(const SDL_Rect& r, SDL_Texture* t, SDL_Color siNo, bool redonda)
{
    const int radio = redonda ? std::min(r.w, r.h) / 2 : std::max(8, r.w / 7);
    redondo(r, radio, t || !siNo.a ? FONDO_FOTO : siNo);
    if (t)
    {
        const int m = redonda ? r.w / 5 : r.w / 9;
        SDL_Rect d = {r.x + m, r.y + m, r.w - 2 * m, r.h - 2 * m};
        SDL_RenderCopy(g_r, t, nullptr, &d);
    }
}

int encabezado(const std::string& titulo, const std::string& explicacion, int x, int y, int ancho)
{
    if (!titulo.empty())
        y += texto(encajar(titulo, ancho, g_negrita), x, y, MARRON, g_negrita) - 2;
    if (!explicacion.empty())
        y += texto(explicacion, x, y, MARRON_SUAVE, g_pequena, Izquierda, ancho);
    return y;
}

} // namespace mapa
