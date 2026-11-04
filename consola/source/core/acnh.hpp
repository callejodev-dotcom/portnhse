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

// Pone la amistad de todos los vecinos con todos los jugadores que conocen al máximo (como «Mejores amigos de todos»
// de NHSE). Devuelve cuántas amistades ha cambiado.
int mejoresAmigos(std::vector<uint8_t>& main);

// ------------------------------------------------------------------------------------------------------- jugador
// El nombre de la isla y el del jugador, de su personal.dat.
std::string nombreIsla(const std::vector<uint8_t>& personal);
std::string nombreJugador(const std::vector<uint8_t>& personal);

// Un objeto de los bolsillos (8 bytes, como Item de NHSE). En los que se apilan, la cantidad es «cuenta + 1».
struct Objeto {
    uint16_t id = NADA;
    uint8_t sistema = 0;
    uint8_t extra = 0;
    uint16_t cuenta = 0;
    uint16_t usos = 0;
    bool vacio() const { return id == NADA; }
};

// Los 40 huecos de los bolsillos en el orden del juego: 0-19 la primera parte (BOLSILLOS2), 20-39 la segunda
// (BOLSILLOS1). Tras cada parte va cuántos huecos de ella hay abiertos.
constexpr int NUM_BOLSILLOS = 40;
int bolsillosAbiertos(const std::vector<uint8_t>& personal, int parte); // cuántos huecos hay abiertos en esa parte (0 o 1)
bool bolsilloAbierto(const std::vector<uint8_t>& personal, int hueco);
Objeto leerBolsillo(const std::vector<uint8_t>& personal, int hueco);
void escribirBolsillo(std::vector<uint8_t>& personal, int hueco, const Objeto& o);

// Buscar objetos por nombre (sin mayúsculas ni tildes; todas las palabras): los índices en OBJETOS (objetos.hpp), primero
// los que empiezan por lo buscado.
std::vector<int> buscarObjetos(const std::string& texto);
// El índice en OBJETOS de un objeto de los bolsillos, o -1.
int objetoDe(const Objeto& o);
std::string normalizar(const std::string& texto);

// «Aprender todo» del jugador. Devuelven cuántas cosas nuevas.
int aprenderRecetas(std::vector<uint8_t>& personal);
int completarCatalogo(std::vector<uint8_t>& personal);
int aprenderReacciones(std::vector<uint8_t>& personal);
struct Progreso { int tiene = 0; int total = 0; };
Progreso recetas(const std::vector<uint8_t>& personal);
Progreso catalogo(const std::vector<uint8_t>& personal);
Progreso reacciones(const std::vector<uint8_t>& personal);

// Posiciones en personal.dat (3.0.x).
constexpr uint32_t ID_JUGADOR = 0xC248;
constexpr uint32_t RECETAS_POS = 0x6528C;   // 4 listas de 0x100 bytes: sabidas, hechas, nuevas, favoritas
constexpr uint32_t CATALOGO_POS = 0x49938;  // objetos recibidos (un bit por objeto)
constexpr uint32_t CATALOGO_FIN = 0x4A08C;  // y aquí empiezan los colores recibidos
constexpr uint32_t COLORES_TAM = 8000;
constexpr uint32_t REACCIONES_POS = 0x4A85C; // 256 reacciones sabidas, 8 de la rueda y 256 «nueva»

// ---------------------------------------------------------------------------------------------------------- nabos
// Precio de compra del domingo y los de venta por medios días: domingo por la mañana y por la tarde (no se usan),
// lunes por la mañana, lunes por la tarde... sábado por la tarde.
constexpr uint32_t NABOS = 0x490770;
struct Nabos {
    uint32_t compra = 0;
    uint32_t venta[14] = {};
};
Nabos leerNabos(const std::vector<uint8_t>& main);
void escribirNabos(std::vector<uint8_t>& main, const Nabos& n);

} // namespace acnh
