// Las caras de los vecinos (romfs/retratos.bin, va en la app; lo hace consola/herramientas/retratos.py a partir de los
// retratos de NHSE).
#pragma once
#include <SDL2/SDL.h>
#include <string>

namespace retratos {
// La cara del vecino con ese nombre interno («kal09»), o nullptr si no la hay. Se guarda: no hay que liberarla.
SDL_Texture* de(const std::string& interno);
// Lo mismo, por el hueco del vecino en la isla (nullptr si la parcela está libre).
SDL_Texture* deHueco(int hueco);
} // namespace retratos
