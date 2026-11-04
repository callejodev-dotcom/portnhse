// Los dibujos de los objetos: los de cada objeto si en la SD está /switch/NHSE/dibujos.bin (se hace en el Mac con
// consola/herramientas/dibujos.py a partir de los dibujos que descarga NHSE), y si no, los iconos genéricos de NHSE
// (romfs/iconos.bin, van en la app).
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include "../core/acnh.hpp"

namespace dibujos {
// El dibujo de un objeto (con su variante), o nullptr si no hay. Se guarda: no hay que liberarlo.
SDL_Texture* de(const acnh::Objeto& o);
SDL_Texture* de(uint16_t id, uint16_t variante = 0);
// ¿Están los dibujos de cada objeto (el paquete de la SD)?
bool completos();
} // namespace dibujos
