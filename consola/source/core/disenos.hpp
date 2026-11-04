// Los diseños de la partida (main.dat 3.0.x), como DesignPattern y DesignPatternPRO de NHSE: los normales (100), los
// profesionales (100), el de la bandera (1) y los de la sastrería (8). Leer, cambiar el nombre, hacerlos de un jugador,
// pasarlos a archivos (.nhd y .nhpd, los mismos de NHSE) y a una imagen PNG.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace disenos {

enum Tipo { Normales, Pro, Bandera, Sastreria, NUM_TIPOS };

constexpr int TAM_NORMAL = 0x2A8; // DesignPattern.SIZE
constexpr int TAM_PRO = 0x8A8;    // DesignPatternPRO.SIZE
constexpr int LADO = 32;          // pixels per side of each sheet
constexpr int COLORES = 15;       // the 16th colour (15) is see-through

// Posiciones en main.dat (MainSaveOffsets30).
constexpr uint32_t NORMALES = 0x110 + 0x1E3848;          // LandMyDesign
constexpr uint32_t PROS = NORMALES + 100 * TAM_NORMAL;   // PatternsPRO
constexpr uint32_t BANDERA = PROS + 100 * TAM_PRO;       // PatternFlag
constexpr uint32_t SASTRERIA = BANDERA + TAM_NORMAL;     // PatternTailor
constexpr uint32_t EDITADOS = 0x5B3D50 + 0x3C9640;       // PatternsEditFlagStart (one byte per normal design)
constexpr uint32_t EDITADOS_PRO = EDITADOS + 0x64;        // PatternsProEditFlagStart

int cuantos(Tipo t);     // huecos de ese tipo
int tamano(Tipo t);      // bytes de cada diseño (y de su archivo)
bool esPro(Tipo t);      // con 4 hojas (delante, detrás y mangas)
int hojas(Tipo t);       // 1 o 4
const char* extension(Tipo t); // «.nhd» o «.nhpd»
uint32_t posicion(Tipo t, int i);

// Leer y escribir un diseño de la partida. Escribir uno normal o profesional pone a 0 la marca «editado» de TODOS los
// de su tipo, como hace NHSE al guardar la ventana de diseños (SetDesigns escribe los 100).
std::vector<uint8_t> leer(const std::vector<uint8_t>& main, Tipo t, int i);
void escribir(std::vector<uint8_t>& main, Tipo t, int i, const std::vector<uint8_t>& d);

// Lo que dice un diseño (sus bytes, de tamano(t)).
std::string nombre(const std::vector<uint8_t>& d);
void ponerNombre(std::vector<uint8_t>& d, const std::string& utf8); // máx. 20 letras, como NHSE
std::string pueblo(const std::vector<uint8_t>& d);
std::string autor(const std::vector<uint8_t>& d);
uint32_t idAutor(const std::vector<uint8_t>& d);
uint32_t uso(const std::vector<uint8_t>& d); // UsageCompatibility
bool deJuego(const std::vector<uint8_t>& d); // de muestra o hueco sin usar (no lo hizo un jugador)
bool vacio(const std::vector<uint8_t>& d, Tipo t); // el hueco sin usar del juego (el cuadro gris)
// Color i (0-14) en RGB.
void color(const std::vector<uint8_t>& d, int i, uint8_t& r, uint8_t& g, uint8_t& b);
// Qué color (0-14, o 15 = transparente) tiene un píxel de una hoja.
int pixel(const std::vector<uint8_t>& d, int hoja, int x, int y);

// ¿Lo hizo este jugador (de esta isla)? Compara con la identidad de su personal.dat, como IsOriginatedFrom.
bool esDe(const std::vector<uint8_t>& d, const std::vector<uint8_t>& personal);
// Que sea de este jugador: cambia su isla y su jugador por los de personal.dat (ChangeOrigins de NHSE).
void hacerMio(std::vector<uint8_t>& d, const std::vector<uint8_t>& personal);
// El valor de «uso» que el juego espera tras traer un diseño de un archivo (FixUsage de la app del Mac).
void arreglarUso(std::vector<uint8_t>& d, Tipo t);

// El hueco sin usar del juego, para vaciar otro de ese tipo: una copia del primero vacío de los normales (o de los PRO,
// para los PRO y los de la sastrería). Devuelve false si no queda ninguno vacío.
bool blanco(const std::vector<uint8_t>& main, Tipo t, std::vector<uint8_t>& d);

// El dibujo entero en RGBA (transparente = 0,0,0,0): 32x32 los normales, 64x64 los profesionales (las 4 hojas en
// cuadro: delante, detrás / manga derecha, manga izquierda), agrandado «escala» veces.
std::vector<uint8_t> rgba(const std::vector<uint8_t>& d, Tipo t, int escala, int& ancho, int& alto);
// El mismo dibujo en un archivo PNG (como «Guardar como imagen» del Mac: 8 veces los normales y 4 los profesionales).
std::vector<uint8_t> png(const std::vector<uint8_t>& d, Tipo t);

// Nombre de archivo para un diseño: «nombre.nhd» (sin letras que la SD no admita); con número, «07 - nombre.nhd».
std::string nombreArchivo(const std::vector<uint8_t>& d, Tipo t, int numero = -1);

} // namespace disenos
