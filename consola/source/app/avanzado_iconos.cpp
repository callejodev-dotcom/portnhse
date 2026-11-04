// Dibujitos de la sección «Avanzado» que no son iconos del juego: la casilla de marcar (como la del Mac), la de un
// número, la lupa del buscador y el estado de cada archivo al comprobar la partida. Se dibujan una vez, punto a punto
// (con bordes suaves), y se guardan.
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <vector>
#include "avanzado.hpp"
#include "ui.hpp"

using namespace ui;

namespace {

constexpr int LADO = 64;
using Forma = std::function<float(float, float)>; // < 0 inside

struct Capa {
    Forma f;
    SDL_Color c;
};

Forma rectRedondo(float x0, float y0, float x1, float y1, float r)
{
    return [=](float x, float y) {
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
        const float dx = std::max(std::fabs(x - cx) - hx, 0.f), dy = std::max(std::fabs(y - cy) - hy, 0.f);
        const float fuera = std::sqrt(dx * dx + dy * dy);
        const float dentro = std::min(std::max(std::fabs(x - cx) - hx, std::fabs(y - cy) - hy), 0.f);
        return fuera + dentro - r;
    };
}
Forma circulo(float cx, float cy, float r)
{
    return [=](float x, float y) { return std::hypot(x - cx, y - cy) - r; };
}
Forma anillo(float cx, float cy, float r, float grosor)
{
    return [=](float x, float y) { return std::fabs(std::hypot(x - cx, y - cy) - r) - grosor / 2; };
}
// Una línea de varios tramos, de ese grosor.
Forma trazo(std::vector<SDL_FPoint> p, float grosor)
{
    return [=](float x, float y) {
        float d = 1e9f;
        for (size_t i = 0; i + 1 < p.size(); i++)
        {
            const float ax = p[i].x, ay = p[i].y, bx = p[i + 1].x, by = p[i + 1].y;
            const float vx = bx - ax, vy = by - ay;
            const float t = std::clamp(((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy), 0.f, 1.f);
            d = std::min(d, std::hypot(x - ax - t * vx, y - ay - t * vy));
        }
        return d - grosor / 2;
    };
}

SDL_Texture* pintar(const std::vector<Capa>& capas)
{
    std::vector<uint8_t> px(LADO * LADO * 4, 0);
    for (int y = 0; y < LADO; y++)
        for (int x = 0; x < LADO; x++)
        {
            float r = 0, g = 0, b = 0, a = 0;
            for (const auto& c : capas)
            {
                int n = 0;
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++)
                        n += c.f(x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4) <= 0;
                const float k = n / 16.f;
                if (k <= 0)
                    continue;
                // Over what there is (colours not premultiplied).
                const float na = k + a * (1 - k);
                r = (c.c.r * k + r * a * (1 - k)) / na;
                g = (c.c.g * k + g * a * (1 - k)) / na;
                b = (c.c.b * k + b * a * (1 - k)) / na;
                a = na;
            }
            uint8_t* p = &px[(y * LADO + x) * 4];
            p[0] = (uint8_t)std::lround(r), p[1] = (uint8_t)std::lround(g), p[2] = (uint8_t)std::lround(b), p[3] = (uint8_t)std::lround(a * 255);
        }
    SDL_Texture* t = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, LADO, LADO);
    if (t)
    {
        SDL_UpdateTexture(t, nullptr, px.data(), LADO * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    }
    return t;
}

const SDL_Color GRIS_BORDE = {0xAE, 0xAE, 0xB2, 255};

SDL_Texture* guardado(int clave, std::function<std::vector<Capa>()> capas)
{
    static std::map<int, SDL_Texture*> hechos;
    auto it = hechos.find(clave);
    if (it != hechos.end())
        return it->second;
    return hechos[clave] = pintar(capas());
}

} // namespace

namespace avanzado {

SDL_Texture* casilla(bool marcada)
{
    return guardado(marcada ? 1 : 0, [marcada] {
        std::vector<Capa> c;
        if (marcada)
        {
            c.push_back({rectRedondo(12, 12, 52, 52, 9), AZUL});
            c.push_back({trazo({{21, 33}, {29, 41}, {43, 24}}, 5.5f), BLANCO});
        }
        else
        {
            c.push_back({rectRedondo(12, 12, 52, 52, 9), GRIS_BORDE});
            c.push_back({rectRedondo(15, 15, 49, 49, 7), BLANCO});
        }
        return c;
    });
}

SDL_Texture* cifra()
{
    return guardado(2, [] {
        return std::vector<Capa>{
            {rectRedondo(14, 8, 50, 56, 9), GRIS_BORDE},
            {rectRedondo(17, 11, 47, 53, 7), BLANCO},
            {trazo({{24, 28}, {32, 20}, {40, 28}}, 4), MARRON_SUAVE},
            {trazo({{24, 36}, {32, 44}, {40, 36}}, 4), MARRON_SUAVE},
        };
    });
}

SDL_Texture* lupa()
{
    return guardado(3, [] {
        return std::vector<Capa>{
            {anillo(27, 27, 13, 5), MARRON_SUAVE},
            {trazo({{37, 37}, {49, 49}}, 7), MARRON_SUAVE},
        };
    });
}

SDL_Texture* estado(int e)
{
    return guardado(10 + e, [e] {
        const SDL_Color fondo = e == 0 ? VERDE : e == 1 ? ROJO : e == 2 ? AMARILLO : GRIS_BORDE;
        std::vector<Capa> c = {{circulo(32, 32, 22), fondo}};
        if (e == 0)
            c.push_back({trazo({{21, 33}, {29, 41}, {43, 24}}, 5.5f), BLANCO});
        else if (e == 1)
        {
            c.push_back({trazo({{23, 23}, {41, 41}}, 5.5f), BLANCO});
            c.push_back({trazo({{41, 23}, {23, 41}}, 5.5f), BLANCO});
        }
        else if (e == 2)
            for (int k = 0; k < 3; k++)
                c.push_back({circulo(21 + k * 11, 32, 3.5f), BLANCO});
        else
            c.push_back({trazo({{22, 32}, {42, 32}}, 5.5f), BLANCO});
        return c;
    });
}

} // namespace avanzado
