#include "dibujos.hpp"
#include "cache_texturas.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <zlib.h>

namespace dibujos {

namespace {
// Un archivo de dibujos (ver herramientas/dibujos.py): el índice en memoria y los dibujos se leen al pedirlos.
struct Paquete {
    FILE* f = nullptr;
    uint32_t lado = 0;
    struct Entrada {
        uint32_t clave, pos, tam;
    };
    std::vector<Entrada> indice;

    bool abrir(const char* ruta)
    {
        f = fopen(ruta, "rb");
        if (!f)
            return false;
        uint8_t cab[16];
        if (fread(cab, 1, 16, f) != 16 || memcmp(cab, "DIBU", 4) != 0)
            return cerrar();
        lado = cab[8] | (cab[9] << 8);
        const uint32_t n = cab[12] | (cab[13] << 8) | (cab[14] << 16) | ((uint32_t)cab[15] << 24);
        indice.resize(n);
        if (fread(indice.data(), 12, n, f) != n) // little endian, as the Switch and the Mac
            return cerrar();
        return true;
    }
    bool cerrar()
    {
        if (f)
            fclose(f);
        f = nullptr;
        indice.clear();
        return false;
    }
    const Entrada* buscar(uint32_t clave) const
    {
        auto it = std::lower_bound(indice.begin(), indice.end(), clave, [](const Entrada& e, uint32_t k) { return e.clave < k; });
        return it != indice.end() && it->clave == clave ? &*it : nullptr;
    }
    SDL_Texture* textura(const Entrada& e) const
    {
        std::vector<uint8_t> z(e.tam);
        if (fseek(f, e.pos, SEEK_SET) != 0 || fread(z.data(), 1, e.tam, f) != e.tam)
            return nullptr;
        std::vector<uint8_t> d(1 + 256 * 4 + lado * lado);
        uLongf tam = d.size();
        if (uncompress(d.data(), &tam, z.data(), z.size()) != Z_OK)
            return nullptr;
        const int colores = d[0] ? d[0] : 256;
        const uint8_t* pal = d.data() + 1;
        const uint8_t* px = pal + colores * 4;
        if ((size_t)(1 + colores * 4) + lado * lado > tam)
            return nullptr;
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, lado, lado, 32, SDL_PIXELFORMAT_ABGR8888);
        if (!s)
            return nullptr;
        for (uint32_t y = 0; y < lado; y++)
        {
            uint8_t* fila = (uint8_t*)s->pixels + y * s->pitch;
            for (uint32_t x = 0; x < lado; x++)
            {
                const uint8_t* c = pal + 4 * std::min<int>(px[y * lado + x], colores - 1);
                fila[4 * x] = c[0], fila[4 * x + 1] = c[1], fila[4 * x + 2] = c[2], fila[4 * x + 3] = c[3];
            }
        }
        SDL_Texture* t = SDL_CreateTextureFromSurface(ui::g_r, s);
        SDL_FreeSurface(s);
        if (t)
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        return t;
    }
};

Paquete& sd()
{
    static Paquete p = [] {
        Paquete q;
        q.abrir("sdmc:/switch/NHSE/dibujos.bin");
        return q;
    }();
    return p;
}

Paquete& iconos()
{
    static Paquete p = [] {
        Paquete q;
        q.abrir("romfs:/iconos.bin");
        return q;
    }();
    return p;
}

CacheTexturas<uint32_t> g_cache(400);
} // namespace

SDL_Texture* de(uint16_t id, uint16_t variante)
{
    if (id >= acnh::EXTENSION)
        return nullptr;
    const uint32_t clave = ((uint32_t)id << 16) | variante;
    bool hallada;
    if (SDL_Texture* ya = g_cache.buscar(clave, hallada); hallada)
        return ya;
    SDL_Texture* t = nullptr;
    const Paquete::Entrada* e = nullptr;
    if (sd().f && ((e = sd().buscar(clave)) || (e = sd().buscar((uint32_t)id << 16))))
        t = sd().textura(*e);
    if (!t && iconos().f && (e = iconos().buscar((uint32_t)id << 16)))
        t = iconos().textura(*e);
    return g_cache.poner(clave, t);
}

SDL_Texture* de(const acnh::Objeto& o)
{
    if (o.vacio())
        return nullptr;
    return de(o.id, o.cuenta);
}

bool completos() { return sd().f != nullptr; }

} // namespace dibujos
