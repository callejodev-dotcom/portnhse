// La isla (sin el mapa ni las casas): hemisferio, aeródromo, fruta y flores, zona de acampada, visitantes especiales,
// tendencia de los nabos, museo y tablón de anuncios. Cada cambio deja la partida igual que la app de NHSE para Mac
// (sus ventanas de Isla y Museo); las posiciones están en isla_tablas.hpp.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "isla_tablas.hpp"

namespace acnh {

// Una fecha de la partida (año, mes, día); todo 0 = sin fecha.
struct Fecha {
    int anio = 0, mes = 0, dia = 0;
    bool vacia() const { return anio == 0 && mes == 0 && dia == 0; }
};
Fecha hoy();
// «3 may 2026»; vacío si no hay fecha.
std::string textoFecha(const Fecha& f);

// ---------------------------------------------------------------------------------------- hemisferio y aeródromo
constexpr int NUM_AEROPUERTOS = 4;
extern const char* const AEROPUERTOS[NUM_AEROPUERTOS]; // Azul, Amarillo, Naranja, Verde
int hemisferio(const std::vector<uint8_t>& main); // 0 norte, 1 sur
void ponerHemisferio(std::vector<uint8_t>& main, int h);
int aeropuerto(const std::vector<uint8_t>& main);
void ponerAeropuerto(std::vector<uint8_t>& main, int color);

// ------------------------------------------------------------------------------------------------- fruta y flores
constexpr int NUM_FLORES_ISLA = 8;
extern const char* const FLORES_ISLA[NUM_FLORES_ISLA]; // Anémonas, Jacintos...
struct FrutasFlores {
    uint16_t fruta = 0, frutaHermana = 0;
    uint8_t flor = 0, florHermana = 0;
};
// Lo que enseña NHSE para Mac al abrir: la fruta de la isla si es una de las cinco; si no, la del pasaporte de «yo».
FrutasFlores leerFrutasFlores(const std::vector<uint8_t>& main, const std::vector<uint8_t>& personalYo);
// Guarda como FruitsFlowersWindow: la fruta de la isla también va en el pasaporte de «yo» y en el de quien tenía la
// anterior; las banderas de fruta marcan la nueva.
void guardarFrutasFlores(std::vector<uint8_t>& main, std::vector<std::vector<uint8_t>*> personales, int yo, const FrutasFlores& f);
std::string nombreFruta(uint16_t id); // «Pera», o «Objeto 1234» si no es una de las cinco

// ---------------------------------------------------------------------------------------------- zona de acampada
// 2 vacía, 3 con alguien; otro valor = aún no hay zona de acampada.
int estadoCamping(const std::vector<uint8_t>& main);
inline bool hayCamping(const std::vector<uint8_t>& main) { const int e = estadoCamping(main); return e == 2 || e == 3; }
std::string campista(const std::vector<uint8_t>& main); // nombre interno («cat01»), o vacío si no hay nadie
Fecha fechaCamping(const std::vector<uint8_t>& main);
// Pone a alguien (nombre interno) en la zona de acampada ese día, o a nadie (""). No hace nada si no hay zona.
bool guardarCamping(std::vector<uint8_t>& main, const std::string& interno, const Fecha& dia);

// ---------------------------------------------------------------------------------------------------- visitantes
// Los días van como en el juego: 0 domingo, 1 lunes... 6 sábado. Los visitantes, como VISITANTES (0 = nadie).
constexpr int NUM_VISITANTES = 12;
int visitante(const std::vector<uint8_t>& main, int dia);
void ponerVisitante(std::vector<uint8_t>& main, int dia, int quien);
bool concierto(const std::vector<uint8_t>& main, int dia, bool cumple);
void ponerConcierto(std::vector<uint8_t>& main, int dia, bool cumple, bool si);

// -------------------------------------------------------------------------------------------------------- nabos
// Tendencia de la semana: 0 fluctuante, 1 pico grande, 2 decreciente, 3 pico pequeño. El pico empieza en un medio
// día (2 = lunes por la mañana ... 13 = sábado por la tarde).
constexpr int NUM_TENDENCIAS = 4;
extern const char* const TENDENCIAS[NUM_TENDENCIAS];
extern const char* const TENDENCIAS_EXPLICACION[NUM_TENDENCIAS];
uint32_t tendenciaNabos(const std::vector<uint8_t>& main);
void ponerTendenciaNabos(std::vector<uint8_t>& main, uint32_t t);
uint32_t picoNabos(const std::vector<uint8_t>& main);
void ponerPicoNabos(std::vector<uint8_t>& main, uint32_t medioDia);

// --------------------------------------------------------------------------------------------------------- museo
constexpr int NUM_SALAS = 5;
constexpr int NUM_PIEZAS = INICIO_SALAS[NUM_SALAS];
constexpr int HUECOS_MUSEO = 1024;
// Una donación: en qué hueco de la lista del museo está, cuándo y quién (número de jugador, 255 = nadie).
struct Donacion {
    int hueco = -1;
    Fecha fecha;
    int jugador = 255;
    bool donada() const { return hueco >= 0; }
};
// Las donaciones de todas las piezas de PIEZAS_MUSEO (por su índice). Para no recorrer el museo en cada fotograma.
std::vector<Donacion> donaciones(const std::vector<uint8_t>& main);
// Dona una pieza (objeto) ese día a nombre de ese jugador, en el primer hueco libre, como MuseumWindow. Devuelve
// false si ya estaba o no hay sitio.
bool donar(std::vector<uint8_t>& main, uint16_t id, int jugador, const Fecha& dia);
// La quita del museo: las donaciones que van detrás suben un hueco, como las guarda el juego.
bool retirar(std::vector<uint8_t>& main, uint16_t id);

// -------------------------------------------------------------------------------------------- tablón de anuncios
constexpr int NUM_NOTAS = 30;
constexpr int LARGO_NOTA = 239;  // letras del texto
constexpr int LARGO_FIRMA = 47;
struct Nota {
    int hueco = 0;
    Fecha fecha;
    std::string texto, firma, autor; // autor: la firma, el jugador o «Servicios a residentes»
    bool aMano = false;              // escrita a mano: solo se ve en el juego
    uint16_t dibujo = 0;             // 1 cumpleaños, 2 pesca, 3 bichos...
};
// Las notas del tablón, de la más nueva a la más vieja (como BulletinWindow).
std::vector<Nota> notas(const std::vector<uint8_t>& main);
// Cambia el texto (como mucho LARGO_NOTA letras: lo que sobra se corta) o la firma de la nota de ese hueco.
void escribirNota(std::vector<uint8_t>& main, int hueco, const std::string& texto);
void escribirFirma(std::vector<uint8_t>& main, int hueco, const std::string& firma);
// Letras (como las cuenta el juego) de un texto.
int letras16(const std::string& texto);

} // namespace acnh
