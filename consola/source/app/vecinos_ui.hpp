// Piezas de la sección «Vecinos» repartidas en varios archivos: la ficha (vecinos.cpp), la amistad
// (vecinos_amistad.cpp), la casa, las paredes y el suelo (vecinos_casa.cpp), los recuerdos (vecinos_recuerdos.cpp), las
// marcas (vecinos_marcas.cpp), su diseño (vecinos_diseno.cpp) y los valores en bruto (vecinos_bruto.cpp). Las caras, en
// retratos.hpp.
#pragma once
#include <functional>
#include <string>
#include <utility>
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
// Los 7 corazones de un nivel de amistad, como el Mac: el corazón del juego, los que faltan muy claros. Uno cada «paso».
void filaCorazones(int x, int y, int tam, int paso, int nivel);

// La habitación (paredes y suelo, con su dibujo) y la casa por fuera.
void cuarto(int hueco);
void casa(int hueco);
std::string resumenCuarto(int hueco);
std::string resumenCasa(int hueco);
// El nombre de una pared o un suelo («Pared cerámica amarilla»).
std::string nombreParte(uint16_t id);

// Lo que recuerda de cada jugador (saludos, apodo, fecha, marcas...), como VillagerMemoryEditor (vecinos_recuerdos.cpp).
void recuerdos(int hueco);
std::string resumenRecuerdos(int hueco);
// Sus marcas sueltas, como VillagerFlagEditor (vecinos_marcas.cpp).
void marcas(int hueco);
std::string resumenMarcas(int hueco);
// El diseño que lleva, como PatternEditorPRO (vecinos_diseno.cpp).
void diseno(int hueco);
std::string resumenDiseno(int hueco);
// Los valores en bruto de su habitación y de su casa (vecinos_bruto.cpp).
void brutoCuarto(int hueco);
void brutoCasa(int hueco);

// Una lista de marcas con nombre y valor (las del vecino y las de cada recuerdo), en páginas, con buscar (X) y ver (Y).
struct ListaMarcas {
    std::string titulo;
    std::string explicacion;
    std::vector<int> indices;                         // the flag number of each named one
    std::vector<std::string> textos, internos;
    std::vector<int> maximos;                         // usual maximum of each named one (1 = yes or no)
    int total = 0;                                    // all the flags (also those without a name)
    uint32_t tope = 0;                                // the most it can hold (255 or 65535)
    std::function<int(int)> leer;                     // value of flag number i
    std::function<void(int, int)> poner;              // and says it changed
    std::vector<std::pair<std::string, std::function<void()>>> mas; // «Más…» options
};
void listaMarcas(ListaMarcas l);

// Elegir uno de una lista larga, en páginas, con buscar (X) si hay claves; se abre en la página del actual.
struct Opcion {
    std::string nombre;
    std::string clave; // for searching (lower case without accents); empty = no search
};
void elegirLargo(const std::string& titulo, const std::string& explicacion, std::vector<Opcion> opciones, int actual,
    std::function<void(int)> elegido);

} // namespace vecinos
