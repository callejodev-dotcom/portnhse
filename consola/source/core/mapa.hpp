// Rejillas de objetos (el suelo de la isla y las habitaciones de las casas) y el mapa de la isla, como hace NHSE para Mac
// (GridItems e IslandMapWindow): poner, quitar, girar y mover cosas con todas sus casillas (las cosas grandes ocupan
// varias: la primera es la «raíz» y las demás son «extensiones» que apuntan a ella), y ordenar la isla por tipos o por
// nombres. Portable: se compila igual en la Switch y en el Mac.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "acnh.hpp"

namespace mapa {

using acnh::Objeto;

// Posiciones en main.dat (3.0.x).
constexpr uint32_t CAMPO = acnh::CAMPO;                        // las dos capas de objetos de la isla
constexpr int ANCHO = acnh::ANCHO, ALTO = acnh::ALTO;           // 288 × 192 casillas de objeto
constexpr uint32_t TAM_CAPA = ANCHO * ALTO * 8;
constexpr uint32_t TERRENO = 0x30AB00;                          // 112 × 96 piezas de 14 bytes
constexpr int TERRENO_ANCHO = 112, TERRENO_ALTO = 96;
constexpr uint32_t EDIFICIOS = 0x32F700;                        // 46 edificios de 0x14 bytes
constexpr int NUM_EDIFICIOS = 46;
constexpr uint32_t ZONAS = 0x32FAAC;                            // las 9 × 8 zonas de la isla (plantillas de los bordes)
constexpr uint32_t PLAZA_X = ZONAS + 144 + 4, PLAZA_Z = ZONAS + 144 + 8;
// El mapa entero (con el mar) en casillas de objeto, y dónde empieza la capa de objetos en él.
constexpr int MAPA_ANCHO = 288, MAPA_ALTO = 256;
constexpr int ORIGEN_X = 0, ORIGEN_Y = 32;

constexpr uint16_t NADA = 0xFFFE, EXTENSION = 0xFFFD;

// --------------------------------------------------------------------------------------------------------- objetos
inline bool esNada(const Objeto& o) { return o.id == NADA; }
inline bool esExtension(const Objeto& o) { return o.id == EXTENSION; }
inline bool esRaiz(const Objeto& o) { return o.id < EXTENSION; }
inline bool esPlanta(const Objeto& o) { return esRaiz(o) && o.id >= 60000; } // IsFieldItem
inline bool enterrado(const Objeto& o) { return (o.sistema & 0x04) != 0; }
inline bool tirado(const Objeto& o) { return (o.sistema & 0x20) != 0; }
inline int giro(const Objeto& o) { return o.sistema & 3; }
// La extensión: id de la raíz (en la cantidad) y a cuántas casillas está de ella.
inline uint16_t idExtension(const Objeto& o) { return o.cuenta; }
inline int extX(const Objeto& o) { return o.usos & 0xFF; }
inline int extY(const Objeto& o) { return o.usos >> 8; }
inline uint16_t idVisible(const Objeto& o) { return esExtension(o) ? o.cuenta : o.id; } // DisplayItemId
Objeto nada();

// Ancho y alto en casillas, girado como está (GridItems.GetSize).
void tamano(const Objeto& o, int& w, int& h);
// Se ve distinto al girarlo: muebles y parecidos (GridItems.CanRotate).
bool girable(const Objeto& o);
// Categoría del Mac (0 otros, 1 muebles, 2 paredes y suelos, 3 ropa...), ver mapa_objetos.hpp.
int categoria(uint16_t id);

// Las cosas plantadas (id ≥ 60000): su clase (1 mala hierba, 2 flor, 3 árbol, 4 arbusto, 5 valla, 6 hoyo, 7 roca, 0
// otra), cómo se dibujan y su color; -1 si no es planta.
int plantaDe(uint16_t id); // índice en PLANTAS
int clasePlanta(uint16_t id);

// El nombre como lo dice el Mac (GridItems.GetName) y una clave: dos cosas con la misma clave se llaman igual.
std::string nombre(const Objeto& o);
uint64_t claveNombre(const Objeto& o);

// ------------------------------------------------------------------------------------------------------ rejillas
// Una rejilla de objetos de 8 bytes, por columnas (índice = x * alto + y), dentro de un archivo de la partida.
struct Rejilla {
    uint8_t* datos = nullptr;
    int ancho = 0, alto = 0;
    bool dentro(int x, int y) const { return x >= 0 && y >= 0 && x < ancho && y < alto; }
    Objeto leer(int x, int y) const;
    Objeto leerSeguro(int x, int y) const { return dentro(x, y) ? leer(x, y) : nada(); }
    void escribir(int x, int y, const Objeto& o);
};

enum Resultado { Bien, FueraDelMapa, NoCabe };

// El objeto que cubre una casilla: ella misma o la raíz de la cosa grande de la que es parte. False si está vacía.
bool raiz(const Rejilla& r, int x, int y, Objeto& o, int& rx, int& ry);
// Quita la cosa que cubre la casilla (todas sus casillas).
bool quitar(Rejilla& r, int x, int y);
// Pone «o» con su esquina en (x, y), en lugar de lo que haya en esa casilla. Si no cabe no cambia nada y dice qué
// estorba. Con «ajustar», las plantas, vallas y rocas van en cuadros enteros (casillas pares), como en el juego.
Resultado poner(Rejilla& r, const Objeto& o, int x, int y, Objeto* estorbo = nullptr, int* enX = nullptr, int* enY = nullptr, bool ajustar = true);
// Gira la cosa de esa casilla un cuarto a la derecha (con su espacio).
Resultado girar(Rejilla& r, int x, int y, Objeto* estorbo = nullptr);
// Cambia una cosa de sitio, de su esquina a otra (como al arrastrarla en el Mac). Si algo estorba, no se mueve.
Resultado mover(Rejilla& r, int x, int y, int aX, int aY, Objeto* estorbo = nullptr);
// ¿Cabe «o» con la esquina ahí sin pisar nada?
bool cabeLibre(const Rejilla& r, const Objeto& o, int x, int y);

// ------------------------------------------------------------------------------------------------- ordenar la isla
enum Tipo { Hierbas, Flores, Arboles, Arbustos, Vallas, Hoyos, Ramas, Conchas, Cosas, Todo, NUM_TIPOS };
bool esTipo(const Objeto& o, Tipo t);

// Una zona en casillas de la capa (x, y, ancho, alto); toda la capa si ancho = 0.
struct Zona {
    int x = 0, y = 0, w = 0, h = 0;
};
// Las esquinas (raíces) de lo que cumple, con la esquina dentro de la zona.
struct Punto {
    int x, y;
};
std::vector<Punto> buscar(const Rejilla& r, Zona z, Tipo t);
std::vector<Punto> buscarNombre(const Rejilla& r, Zona z, uint64_t clave);
int contar(const Rejilla& r, Tipo t, Zona z = {});
int quitarTodos(Rejilla& r, Tipo t, Zona z = {});
int quitarNombre(Rejilla& r, uint64_t clave, Zona z = {});
// Cambia cada cosa de un tipo por una de las plantas elegidas: alternando por posición (rayas en diagonal; con dos, un
// tablero de ajedrez) o al azar.
int cambiarTodos(Rejilla& r, Tipo t, const std::vector<uint16_t>& ids, bool alAzar, Zona z = {}, uint32_t semilla = 1);
// Riega todas las flores de la zona.
int regar(Rejilla& r, Zona z = {});
// La goma: quita lo que toca (las cosas grandes enteras) de un tipo, o con ese nombre si clave != 0.
int borrar(Rejilla& r, Zona z, Tipo t, uint64_t clave = 0);

// Cada clase de cosa de la zona por su nombre, las que más hay primero.
struct Clase {
    uint64_t clave;
    Objeto ejemplo;
    int cuantas;
};
std::vector<Clase> clases(const Rejilla& r, Zona z = {});

// ------------------------------------------------------------------------------------------------------ la isla
// Las dos capas de objetos de la isla en main.dat (0 en el suelo, 1 encima de otras cosas).
Rejilla capa(std::vector<uint8_t>& main, int c);

// Las casillas (de la capa) con algo encima de otras cosas pero nada debajo (GetUnsupportedTiles de NHSE: el Mac
// pregunta antes de guardar si hay alguna).
std::vector<Punto> sinApoyo(std::vector<uint8_t>& main);

// Un edificio del mapa, con lo que necesita el dibujo (en casillas de objeto del mapa entero).
struct Edificio {
    int tipo;            // BuildingType de NHSE
    double x, y, w, h;   // centro y tamaño
    double angulo;       // grados
    std::string nombre;
    uint32_t color;      // tejado
    int indice;          // en la lista de la partida
    bool casa() const { return tipo >= 1 && tipo <= 18; }
    bool cruce() const { return tipo == 26 || tipo == 27; }
    bool contiene(double px, double py) const { return px >= x - w / 2 && px < x + w / 2 && py >= y - h / 2 && py < y + h / 2; }
};
// «jugadores»: los nombres de los jugadores (para sus casas).
std::vector<Edificio> edificios(const std::vector<uint8_t>& main, const std::vector<std::string>& jugadores);

} // namespace mapa
