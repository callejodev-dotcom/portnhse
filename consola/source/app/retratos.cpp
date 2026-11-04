#include "retratos.hpp"
#include "cache_texturas.hpp"
#include "partida.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <zlib.h>

namespace retratos {

namespace {
// romfs/retratos.bin, en el formato «DIBU» de herramientas/dibujos.py: el índice en memoria y cada retrato se lee al
// pedirlo. La clave es el CRC-32 del nombre interno.
struct Paquete {
    FILE* f = nullptr;
    uint32_t lado = 0;
    struct Entrada {
        uint32_t clave, pos, tam;
    };
    std::vector<Entrada> indice;

    Paquete()
    {
        f = fopen("romfs:/retratos.bin", "rb");
        if (!f)
            return;
        uint8_t cab[16];
        if (fread(cab, 1, 16, f) != 16 || memcmp(cab, "DIBU", 4) != 0)
        {
            cerrar();
            return;
        }
        lado = cab[8] | (cab[9] << 8);
        const uint32_t n = cab[12] | (cab[13] << 8) | (cab[14] << 16) | ((uint32_t)cab[15] << 24);
        indice.resize(n);
        if (fread(indice.data(), 12, n, f) != n) // little endian, as the Switch and the Mac
            cerrar();
    }
    void cerrar()
    {
        if (f)
            fclose(f);
        f = nullptr;
        indice.clear();
    }
    SDL_Texture* textura(uint32_t clave) const
    {
        auto e = std::lower_bound(indice.begin(), indice.end(), clave, [](const Entrada& x, uint32_t k) { return x.clave < k; });
        if (!f || e == indice.end() || e->clave != clave)
            return nullptr;
        std::vector<uint8_t> z(e->tam);
        if (fseek(f, e->pos, SEEK_SET) != 0 || fread(z.data(), 1, e->tam, f) != e->tam)
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
        std::vector<uint8_t> rgba(lado * lado * 4);
        for (uint32_t i = 0; i < lado * lado; i++)
            memcpy(&rgba[4 * i], pal + 4 * std::min<int>(px[i], colores - 1), 4);
        SDL_Texture* t = SDL_CreateTexture(ui::g_r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, lado, lado);
        if (t)
        {
            SDL_UpdateTexture(t, nullptr, rgba.data(), lado * 4);
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        }
        return t;
    }
};

Paquete& paquete()
{
    static Paquete p;
    return p;
}

CacheTexturas<uint32_t> g_cache(120);
} // namespace

SDL_Texture* de(const std::string& interno)
{
    if (interno.empty())
        return nullptr;
    const uint32_t clave = (uint32_t)crc32(0, (const Bytef*)interno.data(), (uInt)interno.size());
    bool hallada;
    if (SDL_Texture* ya = g_cache.buscar(clave, hallada); hallada)
        return ya;
    return g_cache.poner(clave, paquete().textura(clave));
}

SDL_Texture* deHueco(int hueco) { return de(acnh::vecinoEn(partida::principal(), hueco)); }

} // namespace retratos
