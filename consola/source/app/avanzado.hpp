// Piezas de la sección «Avanzado» que usan sus dos archivos (avanzado.cpp y avanzado_bruto.cpp).
#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <string>

namespace avanzado {

// Abre una pantalla con «mensaje» y, cuando ya se ha visto, hace el trabajo (lento) y vuelve. El trabajo puede abrir
// otra pantalla o avisar.
void conEspera(const std::string& titulo, const std::string& mensaje, std::function<void()> trabajo);
std::string fecha();             // «2026-11-04 18.30.00», como las copias de seguridad
std::string carpetaJugador(int j); // «Villager0»

// Dibujitos propios (avanzado_iconos.cpp), para «imagen» de un botón: la casilla de marcar, la de un número, la lupa y
// el estado de un archivo (0 bien, 1 mal, 2 con cambios sin guardar, 3 no se puede comprobar).
SDL_Texture* casilla(bool marcada);
SDL_Texture* cifra();
SDL_Texture* lupa();
SDL_Texture* estado(int e);

} // namespace avanzado
