// Pantallas de objetos que usan los bolsillos, el almacén, la papelera, las casas...: ver huecos, cambiar uno (objeto,
// cantidad, color, tela, envoltorio) y buscar un objeto por su nombre.
#pragma once
#include <functional>
#include <string>
#include "../core/acnh.hpp"
#include "ui.hpp"

namespace objetos {

// Unos huecos de objetos de la partida.
struct Huecos {
    std::string nombre;                                       // «Bolsillos de Carli»
    std::function<int()> cuantos;
    std::function<acnh::Objeto(int)> leer;
    std::function<void(int, const acnh::Objeto&)> escribir;   // and says that the save changed
    std::function<bool(int)> abierto;                         // null = all are open
};

// «pepita de hierro ×30», «silla (Azul)»; vacío si no hay nada.
std::string nombre(const acnh::Objeto& o);

// Pide el nombre con el teclado y enseña lo que encuentra; al elegir, llama a «elegido» y vuelve.
void buscar(std::function<void(const acnh::Objeto&)> elegido, const std::string& para = "");

// Cambiar el objeto de un hueco.
ui::Pantalla editor(Huecos h, int i);
// Todos los huecos, en páginas.
ui::Pantalla todos(Huecos h, int porColumna = 10);
// Solo los huecos usados, y «Añadir» que pone en el primero libre (para el almacén, que tiene miles).
ui::Pantalla usados(Huecos h);

} // namespace objetos
