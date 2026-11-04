// Lo que comparten el mapa de la isla y las casas (como TilePanel y FieldItemPicker de NHSE para Mac): la pantalla de un
// sitio elegido (qué hay, quitarlo, girarlo, moverlo, cambiarlo por otra cosa), buscar un objeto para ponerlo y elegir
// plantas, vallas y rocas.
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "../core/mapa.hpp"
#include "ui.hpp"

namespace mapa {

// Un sitio elegido de una rejilla (la isla o una habitación).
struct Sitio {
    std::string donde;                              // «En el suelo», «En la pared · Pared izquierda»...
    std::function<Objeto()> objeto;                 // lo que hay (su raíz), o nada()
    std::function<std::string()> aviso;             // lo que pasó la última vez (no cabe...), o vacío
    std::function<bool(const Objeto&)> poner;       // pone otra cosa ahí; true si se pudo
    std::function<void()> quitar;
    std::function<void()> girar;                    // null si no se gira
    std::function<void()> mover;                    // null si no se mueve desde aquí
    bool plantas = false;                           // ofrecer también plantas, vallas y rocas
    int soloCategoria = -1;                         // solo objetos de esa categoría (paredes y suelos)
    std::vector<std::pair<std::string, std::function<void()>>> mas; // más botones (quitar todas iguales...)
    std::function<std::pair<int, int>()> posicion;  // la casilla, para «Opciones avanzadas» (null: no se dice)
};

// La pantalla de un sitio: su nombre y lo que se puede hacer con lo que hay.
ui::Pantalla panelSitio(Sitio s);

// «Opciones avanzadas» de un sitio (como el editor clásico de objetos del Mac): cada valor del objeto por separado (el
// número del objeto, la cantidad, los usos, el giro, enterrado, tirado, los bits del sistema y el extra). Cada cambio se
// pone con «poner» (que dice si se pudo). «donde»: la línea de debajo del nombre (la casilla...).
ui::Pantalla editorBruto(const std::string& titulo, std::function<Objeto()> leer, std::function<bool(const Objeto&)> poner,
    std::function<std::string()> donde);

// Busca un objeto por su nombre (con el teclado) y lo da como lo da el Mac (sin cantidad: uno). Con «categoria», solo
// los de esa categoría (2: paredes, suelos y alfombras).
void buscarObjeto(std::function<void(const Objeto&)> elegido, const std::string& para = "", int categoria = -1);

// Elegir una planta, valla o roca (una por nombre, como la lista del Mac). Con «varias», se marcan varias y se elige
// cómo mezclarlas.
void elegirPlanta(std::function<void(uint16_t)> elegida, const std::string& para = "");
void elegirPlantas(std::function<void(const std::vector<uint16_t>&, bool alAzar)> elegidas, const std::string& para);

// «Aquí no cabe: X está en medio.» o «Aquí no cabe: se saldría del borde.»
std::string noCabe(Resultado r, const Objeto& estorbo, bool habitacion = false);

// El color de un objeto para un cuadrito (el de su planta o el de su categoría).
SDL_Color colorDe(const Objeto& o);

} // namespace mapa
