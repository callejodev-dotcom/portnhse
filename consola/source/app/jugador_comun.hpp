// Piezas que usan varias pantallas de la sección Jugador: elegir una fecha, el día de hoy y un sí/no.
#pragma once
#include <functional>
#include <string>
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

} // namespace jugador
