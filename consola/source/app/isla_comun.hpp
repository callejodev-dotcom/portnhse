// Piezas de las pantallas de la sección «Isla» (nabos, visitantes, museo, tablón...) para que se parezcan a las ventanas
// de NHSE para Mac: recuadros con una cara en vez de un icono, dibujitos hechos a mano (la tendencia de los nabos, los
// interruptores de los conciertos, los círculos de elegir) y una barra de progreso.
#pragma once
#include <string>
#include "ui.hpp"

namespace isla {

// Como ui::panel, pero con cualquier dibujo arriba a la izquierda (la cara de Juliana, la de Totakeke...).
int recuadro(const SDL_Rect& r, const std::string& titulo, SDL_Texture* icono, const std::string& explicacion = "");

// Los dibujitos: cuadrados (los botones los dibujan en un cuadrado a la izquierda), se hacen una vez y se guardan.
SDL_Texture* tendencia(int t);      // la línea de la tendencia de los nabos (0 fluctuante ... 3 pico pequeño)
SDL_Texture* interruptor(bool si);  // un interruptor encendido (verde) o apagado (gris)
SDL_Texture* redondel(bool si);     // el círculo de elegir una opción (azul con punto si está elegida)
SDL_Texture* vacio();               // nada: para que el texto quede a la izquierda, alineado con los que tienen dibujo

// Una barra de progreso verde, como la del museo del Mac.
void progreso(const SDL_Rect& r, double fraccion);

// El icono de NHSE de un objeto (romfs/iconos.bin), sin mirar los dibujos de la SD: para los peces, bichos y criaturas
// marinas del museo, que en la SD son su acuario y aquí se ven como en el museo del Mac. nullptr si no lo hay.
SDL_Texture* iconoObjeto(uint16_t id);

// Las caras de los visitantes especiales (VISITANTES: 0 nadie, 1 Gulliver...), o nullptr.
SDL_Texture* caraVisitante(int v);

} // namespace isla
