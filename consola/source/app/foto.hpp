// La foto de cada jugador (la de su pasaporte, que la partida guarda en JPEG), como la enseña NHSE para Mac.
#pragma once
#include <SDL2/SDL.h>

namespace foto {
// La foto del jugador j (0-7), o nullptr si no tiene. Se guarda: no hay que liberarla.
SDL_Texture* deJugador(int j);
} // namespace foto
