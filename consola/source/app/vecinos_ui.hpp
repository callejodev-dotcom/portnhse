// Piezas de la sección «Vecinos» repartidas en varios archivos: la ficha (vecinos.cpp), la amistad
// (vecinos_amistad.cpp) y la casa, las paredes y el suelo (vecinos_casa.cpp).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "ui.hpp"

namespace vecinos {

// El nombre en español del vecino del hueco («Rocío»), o «Parcela libre».
std::string nombre(int hueco);
// La carpeta de la tarjeta SD donde van los vecinos y sus casas.
constexpr const char* CARPETA = "sdmc:/switch/NHSE/vecinos/";
// Un nombre de archivo sin lo que no se puede usar en la tarjeta SD.
std::string archivo(const std::string& nombre, const std::string& extension);

// Amistad: la de un vecino con cada jugador, la de un jugador con cada vecino y los corazones de una.
void amistad(int hueco);
void amistadDeJugador(int jugador);
void corazones(int hueco, int recuerdo);
// «Grandes amigos con Carli», o «Todavía no conoce a Carli».
std::string resumenAmistad(int hueco, int jugador);
// Un corazón (tam = ancho) con su esquina de arriba a la izquierda en x, y.
void corazon(int x, int y, int tam, SDL_Color c);

// La habitación (paredes y suelo, con su dibujo) y la casa por fuera.
void cuarto(int hueco);
void casa(int hueco);
std::string resumenCuarto(int hueco);
std::string resumenCasa(int hueco);

// Elegir uno de una lista larga, en páginas, con buscar (X) si hay claves; se abre en la página del actual.
struct Opcion {
    std::string nombre;
    std::string clave; // for searching (lower case without accents); empty = no search
};
void elegirLargo(const std::string& titulo, const std::string& explicacion, std::vector<Opcion> opciones, int actual,
    std::function<void(int)> elegido);

} // namespace vecinos
