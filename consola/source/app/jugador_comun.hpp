// Piezas que usan varias pantallas de la sección Jugador: elegir una fecha, el día de hoy, un sí/no, y las piezas de
// dibujo de sus ventanas (casillas de marcar, sellos, una lista con casillas como las del Mac).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../core/jugador.hpp"
#include "ui.hpp"

namespace jugador {

// Hoy, en la consola.
acnh::Fecha hoy();

// Elegir una fecha (día, mes y año); con «sinFecha», también dejarla vacía. Al aceptar llama a «elegida» y vuelve.
void pedirFecha(const std::string& titulo, acnh::Fecha actual, bool sinFecha, std::function<void(acnh::Fecha)> elegida);

// «Sí» o «No».
inline const char* siNo(bool si) { return si ? "Sí" : "No"; }

// «1 de julio»
std::string diaYMes(int dia, int mes);

// ------------------------------------------------------------------------------------------------- piezas de dibujo
// Colores que no están en ui.hpp: el borde gris de las tarjetas del Mac y un gris muy claro.
extern const SDL_Color BORDE_MAC, GRIS_CLARO;

// El dibujo de una casilla de marcar, como las del Mac: azul con su marca, o blanca con borde gris. Para Boton::imagen.
SDL_Texture* casilla(bool marcada);
// Un botón que se marca y se desmarca: la casilla a la izquierda y la etiqueta.
ui::Boton interruptor(const SDL_Rect& r, const std::string& etiqueta, bool marcada, std::function<void()> accion);
// Los sellos de un récord como un círculo partido en «total» trozos: verdes los conseguidos. Para Boton::imagen.
SDL_Texture* sellos(int conseguidos, int total);
// Un recuadro blanco con borde, como las tarjetas del Mac (sin título).
void recuadro(const SDL_Rect& r);
// Una elipse rellena (la rueda de reacciones).
void elipse(int cx, int cy, int rx, int ry, SDL_Color c);
// Un dibujo cuadrado en ese sitio (si lo hay).
void imagen(SDL_Texture* t, int x, int y, int lado);

// ------------------------------------------------------------------------------- una lista con casillas, como el Mac
// Las recetas o el catálogo: cada fila con su dibujo, su nombre (y una nota en gris) y sus casillas a la derecha; arriba
// lo que lleva (en negrita), botones (Aprender todas…), el buscador y los filtros (Todas, Aprendidas…); en páginas.
struct Casilla {
    std::string etiqueta;                    // «Aprendida»
    int ancho = 170;
    std::function<bool(int)> marcada;
    std::function<void(int, bool)> marcar;   // marks it in the save (and says it changed)
    std::function<bool(int)> visible;        // null = always
};
struct Filtro {
    std::string nombre;
    std::function<bool(int)> pasa;           // null = all
};
struct ListaCasillas {
    std::string titulo;
    std::string explicacion;
    int total = 0;
    std::function<std::string(int)> nombre;
    std::function<std::string(int)> nota;     // grey, under the name (or null)
    std::function<SDL_Texture*(int)> imagen;  // or null
    std::vector<Casilla> casillas;
    std::vector<Filtro> filtros;
    std::function<std::string()> resumen;     // «291 de 922 aprendidas»
    std::vector<std::function<ui::Fila()>> arriba; // buttons on the top right (the first one, blue)
    std::vector<std::function<ui::Fila()>> abajo;  // buttons at the bottom (before «Volver»)
    std::string pista = "Buscar…";            // in the search box when it is empty
    int columnas = 1;
};
ui::Pantalla listaCasillas(ListaCasillas l);

} // namespace jugador
