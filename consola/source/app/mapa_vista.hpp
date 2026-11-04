// Piezas de dibujo del mapa de la isla y de las casas, como las de sus ventanas en NHSE para Mac: los botones pequeños
// grises («Quitar», «Girar»), los selectores de pestañas (azul el elegido), los recuadros flotantes encima del mapa, la
// casilla grande con el dibujo de lo elegido... Son «chips»: se dibujan en Vista::dibujo y se tocan desde
// Vista::entrada (esas pantallas usan la cruceta para su cursor, no para ir de botón en botón).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "ui.hpp"

namespace mapa {

// Colores de NHSE para Mac que no están en ui.hpp.
extern const SDL_Color BORDE_MAC;   // the grey edge of the cards
extern const SDL_Color FONDO_FOTO;  // the light green behind the picture of what is chosen
extern const SDL_Color GRIS_CHIP;   // the small grey buttons
extern const SDL_Color APAGADO_MAC; // disabled text

struct Chip {
    SDL_Rect r;
    std::string texto;
    std::function<void()> accion;
    bool marcado = false;          // chosen (blue, as the selected segment of the Mac)
    bool activo = true;
    SDL_Texture* icono = nullptr;  // a small picture on the left
    std::string tecla;             // the key that does the same (a small badge on the right), or empty
    bool principal = false;        // blue, the main action
    bool enGrupo = false;          // part of a segmented control (drawn without its own edge)
};

// Los chips de una pantalla: se vuelven a poner en cada fotograma (al dibujarla) y se tocan con «tocar».
struct Chips {
    std::vector<Chip> v;
    void limpiar() { v.clear(); }
    Chip& poner(Chip c)
    {
        v.push_back(std::move(c));
        return v.back();
    }
    void dibujar() const;
    // Hace lo del chip que está en (x, y), si hay uno activo. true si había uno.
    bool tocar(int x, int y) const;
    // Está (x, y) encima de algún chip (activo o no)?
    bool encima(int x, int y) const;
};

void dibujarChip(const Chip& c);

// Un selector de pestañas («En el suelo | Encima de otros»): repartidas en «r», cada una lleva a «elegir(i)».
void segmentado(Chips& chips, const SDL_Rect& r, const std::vector<std::string>& textos, int elegido, std::function<void(int)> elegir,
    const std::vector<SDL_Texture*>& iconos = {});

// Un recuadro blanco con borde gris, como las tarjetas del Mac.
void tarjeta(const SDL_Rect& r, int radio = 14);
// Un recuadro blanco con sombra, flotando encima del mapa.
void flotante(const SDL_Rect& r, int radio = 12);
// Pinta las esquinas de «r» por fuera de un arco de radio «radio» con «c»: así lo dibujado en «r» queda redondeado.
void esquinas(const SDL_Rect& r, int radio, SDL_Color c);
// La casilla grande con el dibujo de lo elegido (fondo verde claro, como el Mac). Sin dibujo, el color «siNo».
void foto(const SDL_Rect& r, SDL_Texture* t, SDL_Color siNo = {0, 0, 0, 0}, bool redonda = false);
// Una tecla de la consola pequeña (como las de la barra de abajo), con su centro en (x, y). Devuelve su ancho.
int tecla(const std::string& k, int x, int y);
// Un título en negrita y, debajo, una explicación gris en varias líneas. Devuelve la y de debajo.
int encabezado(const std::string& titulo, const std::string& explicacion, int x, int y, int ancho);

} // namespace mapa
