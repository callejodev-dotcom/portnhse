// Las casas de los jugadores (main.dat 3.0.x), como HouseWindow de NHSE para Mac: tamaño, tejado, fachada y puerta, y
// cada habitación con sus ocho capas de objetos (20 × 20 casillas), su papel pintado y su suelo.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "mapa.hpp"

namespace casas {

using acnh::Objeto;

constexpr uint32_t CASAS = 0x33CBE0;      // PlayerHouseList
constexpr uint32_t TAM_CASA = 0x28A28;    // PlayerHouse2
constexpr int NUM_CASAS = 8;
constexpr int NUM_HABITACIONES = 6;
constexpr uint32_t HABITACIONES = 0x120;  // dentro de la casa
constexpr uint32_t TAM_HABITACION = 0x6C24;
constexpr int LADO = 20;                  // casillas de cada capa de una habitación
constexpr uint32_t TAM_CAPA = LADO * LADO * 8;
constexpr uint32_t PAREDES = 0x65A4;      // RoomFloorWall dentro de la habitación: papel en +8, suelo en +0x10
constexpr int FILAS_PARED = 8;            // casillas de alto de una pared

// Las capas de una habitación (RoomLayerSurface de NHSE).
enum Superficie { Suelo, SobreMuebles, ParedFondo, ParedIzquierda, ParedEntrada, ParedDerecha, Alfombras, Techo };

uint8_t* casa(std::vector<uint8_t>& main, int c);
const uint8_t* casa(const std::vector<uint8_t>& main, int c);

// Tamaño de la casa (HouseLevel: 1 tienda, 2 casa, 3 ampliada, 4 fondo, 5 izquierda, 6 derecha, 7 arriba, 8 sótano).
uint32_t nivel(const std::vector<uint8_t>& main, int c);
void ponerNivel(std::vector<uint8_t>& main, int c, uint32_t n);

// Tejado, fachada y puerta (0, 1, 2): el que tiene y el «pedido» (lo que será al día siguiente), que se cambian juntos.
uint16_t aspecto(const std::vector<uint8_t>& main, int c, int parte);
void ponerAspecto(std::vector<uint8_t>& main, int c, int parte, uint16_t valor);

// Una capa de una habitación.
mapa::Rejilla capa(std::vector<uint8_t>& main, int c, int habitacion, int superficie);

// Papel pintado y suelo (true) de una habitación.
Objeto pared(const std::vector<uint8_t>& main, int c, int habitacion, bool suelo);
void ponerPared(std::vector<uint8_t>& main, int c, int habitacion, bool suelo, const Objeto& o);

// Las habitaciones que tiene: una por ampliación, y las que tengan algo dentro (HouseWindow.GetRooms).
std::vector<int> habitaciones(std::vector<uint8_t>& main, int c);

// El suelo de una habitación en casillas de su rejilla (HouseWindow.GetRoomArea).
mapa::Zona sueloDe(uint32_t nivel, int habitacion);
// Donde van las cosas de esa capa: el suelo, o la cara de la pared (a lo largo, y 8 de alto).
mapa::Zona limites(uint32_t nivel, int habitacion, int superficie);
bool cabe(const mapa::Zona& l, const Objeto& o, int x, int y);

// Como HouseWindow: poner (en lugar de lo que haya en esa casilla, si cabe en la habitación), girar, mover y poner uno
// nuevo en un sitio libre.
mapa::Resultado poner(mapa::Rejilla& r, const mapa::Zona& l, const Objeto& o, int x, int y, Objeto* estorbo, int* enX, int* enY);
mapa::Resultado girar(mapa::Rejilla& r, const mapa::Zona& l, int x, int y, Objeto* estorbo);
mapa::Resultado mover(mapa::Rejilla& r, const mapa::Zona& l, int x, int y, int aX, int aY, Objeto* estorbo);
mapa::Resultado ponerNuevo(mapa::Rejilla& r, const mapa::Zona& l, const Objeto& o, int x, int y, Objeto* estorbo);

// ------------------------------------------------------------------------------------------- opciones avanzadas
// Las de «Opciones avanzadas» de HouseWindow: los sucesos de la casa, los valores sueltos y pasar una casa o una
// habitación a un archivo (el mismo formato que NHSE: .nhph2 / .nhpr2, y los de antes de la 2.0, .nhph / .nhpr).
constexpr uint32_t TAM_CASA1 = 0x26400;         // PlayerHouse1 (antes de la 2.0)
constexpr uint32_t TAM_HABITACION1 = 0x65C8;    // PlayerRoom1
constexpr uint32_t SUCESOS = 0x20;              // EventFlags: 128 números de 16 bits
constexpr int NUM_SUCESOS = 128;

int16_t suceso(const std::vector<uint8_t>& main, int c, int i);
void ponerSuceso(std::vector<uint8_t>& main, int c, int i, int16_t valor);
// Los 256 bytes de los sucesos (un .nhfl de NHSE) y ponerlos de un archivo; false si no mide eso.
std::vector<uint8_t> sucesos(const std::vector<uint8_t>& main, int c);
bool ponerSucesos(std::vector<uint8_t>& main, int c, const std::vector<uint8_t>& datos);

// La casa entera y una habitación, tal cual (lo que NHSE guarda en el archivo).
std::vector<uint8_t> exportar(const std::vector<uint8_t>& main, int c);
std::vector<uint8_t> exportarHabitacion(const std::vector<uint8_t>& main, int c, int h);
// Ponerlas de un archivo (como MiscDumpHelper.LoadHouse / LoadRoom): convierte las de antes de la 2.0; vacío si fue
// bien, o por qué no.
std::string importar(std::vector<uint8_t>& main, int c, const std::vector<uint8_t>& datos);
std::string importarHabitacion(std::vector<uint8_t>& main, int c, int h, const std::vector<uint8_t>& datos);

// Los valores sueltos de la casa (los que enseña el PropertyGrid de NHSE para Mac).
enum Valor {
    ValorNivel,          // HouseLevel
    ValorFachada, ValorTejado, ValorPuerta,             // los que tiene
    ValorPedidoFachada, ValorPedidoTejado, ValorPedidoPuerta, // los pedidos (lo que será al día siguiente)
    ValorPrestamo,       // Loan (número cifrado)
    ValorOtro,           // Unknown (número cifrado)
    ValorNpc1, ValorNpc2, // NPC1 y NPC2 (con signo)
    ValorBandera,        // PlayerHouseFlag (0 o 1)
    NUM_VALORES
};
int64_t valor(const std::vector<uint8_t>& main, int c, Valor v);
void ponerValor(std::vector<uint8_t>& main, int c, Valor v, int64_t n);
// Los objetos sueltos: la decoración de la puerta, el buzón y el buzón pedido (0, 1, 2).
Objeto objetoCasa(const std::vector<uint8_t>& main, int c, int cual);
void ponerObjetoCasa(std::vector<uint8_t>& main, int c, int cual, const Objeto& o);

} // namespace casas
