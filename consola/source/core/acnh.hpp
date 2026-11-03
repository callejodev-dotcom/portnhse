// Núcleo de NHSE para Switch: descifra y vuelve a cifrar la partida de Animal Crossing: New Horizons 3.0.x, recalcula
// sus comprobaciones (Murmur3) y hace los cambios. Portable: se compila igual en la Switch y en el Mac para las pruebas.
// El cifrado (AES-CTR con la clave sacada de la cabecera) viene de cbreezy210/ACNH-Save-Editor (MIT); las regiones de
// las comprobaciones y las posiciones, de NHSE (kwsch/NHSE).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace acnh {

// Un archivo de la partida: la cabecera (xxxHeader.dat) y los datos ya descifrados.
struct Archivo {
    std::vector<uint8_t> cabecera;
    std::vector<uint8_t> datos;
};

// AES-CTR con la clave y el contador de la cabecera: cifrar y descifrar es la misma operación.
void cifrar(const std::vector<uint8_t>& cabecera, std::vector<uint8_t>& datos);

// Recalcula todas las comprobaciones de main.dat o personal.dat (según su tamaño). Devuelve false si el tamaño no es
// el de la 3.0.x (otra versión del juego: no se toca nada).
bool arreglarComprobaciones(std::vector<uint8_t>& datos);

// Cuántas comprobaciones no cuadran (0 = archivo sano).
int comprobacionesMal(const std::vector<uint8_t>& datos);

constexpr size_t TAM_MAIN = 0x9B0E90;
constexpr size_t TAM_PERSONAL = 0x74A40;

// Números cifrados de la partida (dinero, millas): valor, ajuste, desplazamiento y suma de control.
uint32_t leerNumero(const uint8_t* p);
void escribirNumero(uint8_t* p, uint32_t valor);

// Posiciones en personal.dat (3.0.x).
constexpr uint32_t MONEDERO = 0x37D60;
constexpr uint32_t BANCO = 0x6527C;
constexpr uint32_t MILLAS = 0x12718;
constexpr uint32_t BOLSILLOS1 = 0x37BF0;
constexpr uint32_t BOLSILLOS2 = 0x37CA8;

// El suelo de la isla en main.dat: dos capas de casillas de 8 bytes, por columnas (índice = x * ALTO + y).
constexpr uint32_t CAMPO = 0x22F500;
constexpr int ANCHO = 288;
constexpr int ALTO = 192;
constexpr uint16_t NADA = 0xFFFE;
constexpr uint16_t EXTENSION = 0xFFFD;

// Lo que hay en el suelo (capa de abajo).
struct Recuento {
    int flores = 0;
    int malasHierbas = 0;
};

bool esFlor(uint16_t id);
bool esMalaHierba(uint16_t id);
Recuento contar(const std::vector<uint8_t>& main);

// Cambia todas las flores por las elegidas (mezcladas alternando por posición, o al azar). Devuelve cuántas.
int cambiarFlores(std::vector<uint8_t>& main, const std::vector<uint16_t>& nuevas, bool alAzar, uint32_t semilla = 1);

// Quita todas las malas hierbas. Devuelve cuántas.
int quitarMalasHierbas(std::vector<uint8_t>& main);

// Los vecinos (main.dat 3.0.x): 10 huecos, y sus casas (cada casa dice qué vecino vive en ella).
constexpr uint32_t POS_VECINOS = 0x120;
constexpr uint32_t TAM_VECINO = 0x13230;
constexpr uint32_t CASAS = 0x481D20;
constexpr uint32_t TAM_CASA = 0x12E8;
constexpr int NUM_VECINOS = 10;

// Nombre interno del vecino de un hueco («kal09»), o vacío si el hueco está libre.
std::string vecinoEn(const std::vector<uint8_t>& main, int hueco);

// Cambia el vecino de un hueco por otro (por su nombre interno), en la misma casa, como hace NHSE. Los datos de cada
// vecino van en vecinos.bin (consola/romfs). Devuelve el motivo si no puede.
std::string cambiarVecino(std::vector<uint8_t>& main, int hueco, const std::string& interno, const std::vector<uint8_t>& vecinosBin);

} // namespace acnh
