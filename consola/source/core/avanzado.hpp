// Núcleo de la sección «Avanzado»: marcas de progreso (event flags), clima, huecos abiertos (como el editor clásico),
// las comprobaciones de cada archivo de la partida una a una y leer los archivos descifrados de otra carpeta. Las
// posiciones son las de NHSE 3.0 (MainSaveOffsets30, PersonalOffsets30, FileHashRevision.REV_300).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace acnh {

// ------------------------------------------------------------------------------------------------ marcas de progreso
// Las marcas de la isla (main.dat, MainSave.EventFlagLand) y de cada jugador (personal.dat, Personal.EventFlagsPlayer):
// números de 16 bits con signo. Los nombres van en avanzado_marcas.hpp.
constexpr uint32_t MARCAS_ISLA_POS = 0x110 + 0x22EBF0;
constexpr int NUM_MARCAS_ISLA = 0x400;
constexpr uint32_t MARCAS_JUGADOR_POS = 0x110 + 0xC170;
constexpr int NUM_MARCAS_JUGADOR = 0x1000 / 2;

int16_t leerMarca(const std::vector<uint8_t>& datos, bool isla, int indice);
void escribirMarca(std::vector<uint8_t>& datos, bool isla, int indice, int16_t valor);
// Todas las marcas en bruto (como el archivo .nhfl de NHSE: 2 bytes por marca) y cargarlas de vuelta (false si el
// tamaño no es el de las marcas).
std::vector<uint8_t> marcasEnBruto(const std::vector<uint8_t>& datos, bool isla);
bool cargarMarcas(std::vector<uint8_t>& datos, bool isla, const std::vector<uint8_t>& bruto);

// --------------------------------------------------------------------------------------------------------- clima
// Las semillas del clima de la isla y de las islas misteriosas (MainSave.WeatherSeed y TourWeatherSeed), y el
// hemisferio de las islas misteriosas (MainSave.TourHemisphere: 0 norte, 1 sur).
constexpr uint32_t SEMILLA_CLIMA = 0x110 + 0x1E35F0 + 0x18;
constexpr uint32_t SEMILLA_TOUR = 0x5B3D50 + 0x3C970C + 0x18;
constexpr uint32_t HEMISFERIO_TOUR = 0x5B3D50 + 0x3C970C + 0x14;
uint32_t leer32(const std::vector<uint8_t>& datos, uint32_t pos);
void escribir32(std::vector<uint8_t>& datos, uint32_t pos, uint32_t valor);

// ---------------------------------------------------------------------------------------- huecos abiertos (jugador)
// Cuántos huecos tiene abiertos el jugador, como los cambia el editor clásico de NHSE: las dos partes de los bolsillos
// (Personal.PocketCount y BagCount, tras cada lista de 20) y el almacén (ItemChestCount, tras sus 9.000 huecos).
constexpr uint32_t HUECOS_BOLSILLOS1 = 0x37BE0 + 0x10 + 20 * 8 + 0x18 + 20 * 8; // PocketCount (BOLSILLOS2 + 20 * 8)
constexpr uint32_t HUECOS_BOLSILLOS2 = 0x37BE0 + 0x10 + 20 * 8;                 // BagCount (BOLSILLOS1 + 20 * 8)
constexpr uint32_t HUECOS_ALMACEN = 0x37BE0 + 0x18C + 9000 * 8;                  // ItemChestCount
constexpr uint32_t ALMACEN_MAX = 9000;

// ---------------------------------------------------------------------------------------------- comprobaciones
// Una comprobación que no cuadra: dónde va el valor y qué bytes cubre (como FileHashRegion.ToString de NHSE).
struct ComprobacionMal {
    uint32_t pos, desde, hasta;
};
// Las comprobaciones de un archivo de la partida por su nombre («main.dat», «personal.dat», «postbox.dat»,
// «photo_studio_island.dat», «profile.dat», «wherearen.dat»). «total» = cuántas tiene; -1 si no conozco el archivo o
// su tamaño no es el de la 3.0.
struct Comprobado {
    int total = -1;
    std::vector<ComprobacionMal> mal;
};
Comprobado comprobarArchivo(const std::string& nombre, const std::vector<uint8_t>& datos);
// El tamaño que tiene ese archivo en la 3.0 (0 si no lo conozco).
size_t tamanoArchivo(const std::string& nombre);
std::string hex(uint32_t n, int cifras = 0); // «0x1A2B»

// ------------------------------------------------------------------------------------------- partida descifrada
// Los archivos que NHSE guarda al «exportar descifrada»: main.dat y, en cada VillagerN, estos.
extern const char* const ARCHIVOS_JUGADOR[5];
// Lo que se exporta de main.dat o personal.dat: los datos con las comprobaciones al día (sin tocar los de la partida).
std::vector<uint8_t> paraExportar(const std::vector<uint8_t>& datos);
// Cargar un archivo descifrado encima de uno de la partida, como GameFileLoader de NHSE: solo si tiene el mismo
// tamaño. Devuelve si lo ha cargado.
bool cargarDescifrado(std::vector<uint8_t>& datos, const std::vector<uint8_t>& leido);
// Leer bytes en hexadecimal («1F», «01 a0 ff», «0x1F»): false si no son bytes.
bool leerHex(const std::string& texto, std::vector<uint8_t>& bytes);
// Una posición en hexadecimal («1A2B», «0x1A2B»): false si no lo es.
bool leerPosicion(const std::string& texto, uint32_t& pos);

} // namespace acnh
