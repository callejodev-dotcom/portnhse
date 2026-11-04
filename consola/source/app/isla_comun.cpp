// Ver isla_comun.hpp.
#include "isla_comun.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <zlib.h>
#include <functional>
#include <map>
#include "retratos.hpp"

using namespace ui;

namespace isla {

namespace {

constexpr int LADO = 64;

// Un dibujo de LADO × LADO a partir del color de cada punto (con 4 × 4 muestras por píxel, para que los bordes salgan
// suaves). «punto» da el color en (x, y), de 0 a LADO; alfa 0 = nada.
SDL_Texture* pintar(const std::function<SDL_Color(float, float)>& punto)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, LADO, LADO, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return nullptr;
    auto* px = (uint8_t*)s->pixels;
    for (int y = 0; y < LADO; y++)
        for (int x = 0; x < LADO; x++)
        {
            float r = 0, g = 0, b = 0, a = 0;
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                {
                    const SDL_Color c = punto(x + (i + 0.5f) / 4, y + (j + 0.5f) / 4);
                    const float al = c.a / 255.0f;
                    r += c.r * al, g += c.g * al, b += c.b * al, a += al;
                }
            uint8_t* p = px + y * s->pitch + x * 4;
            p[0] = a > 0 ? (uint8_t)(r / a) : 0;
            p[1] = a > 0 ? (uint8_t)(g / a) : 0;
            p[2] = a > 0 ? (uint8_t)(b / a) : 0;
            p[3] = (uint8_t)std::lround(a / 16 * 255);
        }
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_r, s);
    SDL_FreeSurface(s);
    if (t)
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

float distanciaSegmento(float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float l = dx * dx + dy * dy;
    const float t = l > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l, 0.0f, 1.0f) : 0;
    const float x = ax + t * dx - px, y = ay + t * dy - py;
    return std::sqrt(x * x + y * y);
}

constexpr SDL_Color NADA = {0, 0, 0, 0};
constexpr SDL_Color GRIS_BORDE = {0xD1, 0xD1, 0xD6, 255};
constexpr SDL_Color GRIS_APAGADO = {0xAE, 0xAE, 0xB2, 255};
constexpr SDL_Color GRIS_OSCURO = {0x8E, 0x8E, 0x93, 255};

} // namespace

int recuadro(const SDL_Rect& r, const std::string& titulo, SDL_Texture* icono, const std::string& explicacion)
{
    redondo(r, 14, GRIS_BORDE);
    redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 12, BLANCO);
    int x = r.x + 24;
    if (icono)
    {
        SDL_Rect d = {r.x + 20, r.y + 16, 56, 56};
        SDL_RenderCopy(g_r, icono, nullptr, &d);
        x += 68;
    }
    int y = r.y + 16;
    y += texto(encajar(titulo, r.x + r.w - x - 20, g_negrita), x, y, MARRON, g_negrita) - 4;
    if (!explicacion.empty())
        y += texto(explicacion, x, y, MARRON_SUAVE, g_pequena, Izquierda, r.x + r.w - x - 20);
    return std::max(y, icono ? r.y + 78 : y) + 10;
}

SDL_Texture* tendencia(int t)
{
    static std::map<int, SDL_Texture*> hechas;
    auto it = hechas.find(t);
    if (it != hechas.end())
        return it->second;
    // Los mismos puntos que TurnipsWindow (en 44 × 21).
    static const std::vector<float> PUNTOS[4] = {
        {0, 12, 7, 6, 14, 16, 21, 8, 28, 18, 35, 7, 44, 13},
        {0, 12, 8, 14, 16, 17, 22, 19, 27, 1, 32, 17, 38, 20, 44, 21},
        {0, 4, 11, 8, 22, 12, 33, 16, 44, 20},
        {0, 12, 8, 14, 16, 17, 22, 11, 27, 7, 32, 11, 38, 17, 44, 19},
    };
    const auto& p = PUNTOS[std::clamp(t, 0, 3)];
    std::vector<SDL_FPoint> q;
    for (size_t i = 0; i + 1 < p.size(); i += 2)
        q.push_back({4 + p[i] * 56 / 44, 18 + p[i + 1] * 28 / 21});
    SDL_Texture* x = pintar([q](float px, float py) {
        float d = 1e9f;
        for (size_t i = 0; i + 1 < q.size(); i++)
            d = std::min(d, distanciaSegmento(px, py, q[i].x, q[i].y, q[i + 1].x, q[i + 1].y));
        return d <= 2.6f ? VERDE : NADA;
    });
    hechas[t] = x;
    return x;
}

SDL_Texture* interruptor(bool si)
{
    static SDL_Texture* hechos[2] = {nullptr, nullptr};
    if (hechos[si])
        return hechos[si];
    // Una píldora de 60 × 34 en el medio, y el botón blanco a un lado.
    hechos[si] = pintar([si](float x, float y) {
        const float cy = 32, radio = 17, x1 = 2 + radio, x2 = 62 - radio;
        const float d = distanciaSegmento(x, y, x1, cy, x2, cy);
        const float bx = si ? x2 : x1;
        const float db = std::hypot(x - bx, y - cy);
        if (db <= radio - 3)
            return BLANCO;
        if (db <= radio - 2)
            return si ? VERDE_OSCURO : GRIS_OSCURO;
        if (d <= radio)
            return si ? VERDE : GRIS_APAGADO;
        return NADA;
    });
    return hechos[si];
}

SDL_Texture* redondel(bool si)
{
    static SDL_Texture* hechos[2] = {nullptr, nullptr};
    if (hechos[si])
        return hechos[si];
    hechos[si] = pintar([si](float x, float y) {
        const float d = std::hypot(x - 32, y - 32);
        if (si)
            return d <= 8 ? BLANCO : d <= 20 ? AZUL : NADA;
        return d <= 17 ? BLANCO : d <= 20 ? GRIS_APAGADO : NADA;
    });
    return hechos[si];
}

SDL_Texture* vacio()
{
    static SDL_Texture* t = pintar([](float, float) { return NADA; });
    return t;
}

void progreso(const SDL_Rect& r, double fraccion)
{
    redondo(r, r.h / 2, CREMA_OSCURA);
    const int w = (int)std::lround(r.w * std::clamp(fraccion, 0.0, 1.0));
    if (w > 0)
        redondo({r.x, r.y, std::max(w, r.h), r.h}, r.h / 2, VERDE);
}

SDL_Texture* iconoObjeto(uint16_t id)
{
    // El mismo formato que dibujos.cpp (ver herramientas/dibujos.py): «DIBU», versión, lado, número; el índice (clave,
    // posición, tamaño) y cada dibujo con zlib (número de colores, colores RGBA y un byte por píxel).
    struct Entrada {
        uint32_t clave, pos, tam;
    };
    static FILE* f = nullptr;
    static uint32_t lado = 0;
    static std::vector<Entrada> indice;
    static bool abierto = false;
    static std::map<uint16_t, SDL_Texture*> hechos;
    if (!abierto)
    {
        abierto = true;
        f = fopen("romfs:/iconos.bin", "rb");
        uint8_t cab[16];
        if (f && fread(cab, 1, 16, f) == 16 && memcmp(cab, "DIBU", 4) == 0)
        {
            lado = cab[8] | (cab[9] << 8);
            const uint32_t n = cab[12] | (cab[13] << 8) | (cab[14] << 16) | ((uint32_t)cab[15] << 24);
            indice.resize(n);
            if (fread(indice.data(), 12, n, f) != n)
                indice.clear();
        }
    }
    auto ya = hechos.find(id);
    if (ya != hechos.end())
        return ya->second;
    SDL_Texture* t = nullptr;
    const uint32_t clave = (uint32_t)id << 16;
    auto it = std::lower_bound(indice.begin(), indice.end(), clave, [](const Entrada& e, uint32_t k) { return e.clave < k; });
    if (f && it != indice.end() && it->clave == clave && lado > 0 && lado <= 256)
    {
        std::vector<uint8_t> z(it->tam), d(1 + 256 * 4 + lado * lado);
        uLongf tam = d.size();
        if (fseek(f, it->pos, SEEK_SET) == 0 && fread(z.data(), 1, z.size(), f) == z.size() && uncompress(d.data(), &tam, z.data(), z.size()) == Z_OK)
        {
            const int colores = d[0] ? d[0] : 256;
            const uint8_t* pal = d.data() + 1;
            const uint8_t* px = pal + colores * 4;
            if ((size_t)(1 + colores * 4) + lado * lado <= tam)
                if (SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, lado, lado, 32, SDL_PIXELFORMAT_ABGR8888))
                {
                    for (uint32_t y = 0; y < lado; y++)
                        for (uint32_t x = 0; x < lado; x++)
                            memcpy((uint8_t*)s->pixels + y * s->pitch + 4 * x, pal + 4 * std::min<int>(px[y * lado + x], colores - 1), 4);
                    t = SDL_CreateTextureFromSurface(g_r, s);
                    SDL_FreeSurface(s);
                    if (t)
                        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                }
        }
    }
    hechos[id] = t;
    return t;
}

SDL_Texture* caraVisitante(int v)
{
    // Como IslandUi.VisitorCode, en el orden de VISITANTES.
    static const char* const CARAS[] = {"", "gul", "hgc", "cml", "gstA", "hgh", "bey", "chy", "skk", "slo", "fox", "gulB"};
    if (v <= 0 || v >= (int)(sizeof(CARAS) / sizeof(CARAS[0])))
        return nullptr;
    return retratos::de(CARAS[v]);
}

} // namespace isla
