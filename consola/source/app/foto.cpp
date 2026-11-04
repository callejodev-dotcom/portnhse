#include "foto.hpp"
#include <SDL2/SDL_image.h>
#include <map>
#include "partida.hpp"
#include "registro.hpp"
#include "ui.hpp"

namespace foto {

namespace {
constexpr uint32_t FOTO = 0x110 + 0x13430 + 0x14; // PersonalOffsets30.ProfilePhoto; its size goes just before

SDL_Texture* leer(const std::vector<uint8_t>& p)
{
    // As Personal.GetPhotoData of NHSE: a JPEG, with its size in the 4 bytes before.
    if (p.size() < FOTO + 4 || p[FOTO] != 0xFF || p[FOTO + 1] != 0xD8)
        return nullptr;
    const uint32_t tam = p[FOTO - 4] | (p[FOTO - 3] << 8) | (p[FOTO - 2] << 16) | ((uint32_t)p[FOTO - 1] << 24);
    if (tam == 0 || FOTO + tam > p.size())
        return nullptr;
    SDL_RWops* rw = SDL_RWFromConstMem(p.data() + FOTO, (int)tam);
    SDL_Surface* s = rw ? IMG_LoadJPG_RW(rw) : nullptr;
    if (rw)
        SDL_RWclose(rw);
    if (!s)
    {
        registro::apuntar("la foto del jugador no se lee (%s)", IMG_GetError());
        return nullptr;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ui::g_r, s);
    SDL_FreeSurface(s);
    return t;
}
} // namespace

SDL_Texture* deJugador(int j)
{
    static std::map<int, SDL_Texture*> hechas;
    if (j < 0 || j >= (int)partida::jugadores.size())
        return nullptr;
    auto it = hechas.find(j);
    if (it != hechas.end())
        return it->second;
    return hechas[j] = leer(partida::personal(j));
}

} // namespace foto
