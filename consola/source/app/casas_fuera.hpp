// La casa de un jugador por fuera, como HousePreview y HouseComposer de NHSE para Mac: con los dibujos de las fachadas,
// tejados y puertas si en la SD está /switch/NHSE/fachadas.bin (se hace en el Mac con consola/herramientas/fachadas.py
// a partir de los que descarga NHSE; no se sube al repo), y si no, dibujada (las paredes en su material y color).
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

namespace casas_fuera {

enum Parte { Tejado, Fachada, Puerta }; // como casas::aspecto (0, 1, 2)

// La casa con ese tejado, fachada y puerta, en su jardín, dentro de «r».
void dibujar(const SDL_Rect& r, uint16_t tejado, uint16_t fachada, uint16_t puerta);

// La casa entera con ese tejado, fachada y puerta, ya dibujada en un cuadrado (para elegir entre varias, como las
// vistas previas del Mac). Se guarda: no liberarla.
SDL_Texture* foto(uint16_t tejado, uint16_t fachada, uint16_t puerta);

// El dibujo de un tejado, una fachada o una puerta (para elegirlos), o nullptr si no está. Se guarda: no liberarlo.
SDL_Texture* dibujo(Parte parte, uint16_t valor);
// ¿Están los dibujos (fachadas.bin)?
bool hayDibujos();
// Un color para lo que no tiene dibujo: el que dice su nombre («tejado pizarra rojo»), o uno de su clase.
SDL_Color color(Parte parte, uint16_t valor);

} // namespace casas_fuera
