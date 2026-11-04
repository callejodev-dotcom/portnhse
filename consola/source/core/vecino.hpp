// Todo lo de un vecino (main.dat 3.0.x, como Villager2 y VillagerHouse2 de NHSE): personalidad, muletilla, amistad con
// cada jugador, ropa y muebles que guarda, paredes y suelo, su casa por fuera, el bricolaje, la mudanza, y pasarlo a un
// archivo y traerlo de uno (.nhv2 y .nhvh2, los mismos que NHSE).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "acnh.hpp"

namespace acnh {

// Posiciones dentro de un vecino.
constexpr uint32_t V_PERSONALIDAD = 0x2;
constexpr uint32_t V_RECUERDOS = 0x4;       // 8 recuerdos de jugadores
constexpr uint32_t TAM_RECUERDO = 0x5F0;
constexpr int NUM_RECUERDOS = 8;
constexpr uint32_t V_FRASE = 0x10768 + 0x2C; // 24 letras
constexpr int LARGO_FRASE = 24;
constexpr uint32_t V_ROPA = 0x1094C;          // 24 prendas
constexpr uint32_t V_MUEBLES = 0x10D6C;       // 32 muebles
constexpr uint32_t TAM_OBJETO_VECINO = 0x2C;  // un objeto (8 bytes) y más cosas que no se tocan
constexpr int NUM_ROPA = 24;
constexpr int NUM_MUEBLES = 32;
constexpr uint32_t V_MUDANZA = 0x1267A;
constexpr uint32_t V_BANDERAS = 0x1267C;      // 0x100 números de 16 bits
constexpr int BANDERA_IRSE = 24;              // «ForceMoveOut»: para que el juego se lo lleve de verdad
constexpr uint32_t V_HABITACION = 0x12880;    // pared de acento, pared y suelo (8 bytes cada uno)
constexpr uint32_t V_BRICOLAJE = 0x13151;     // hora, minuto y segundo en que termina; luego la receta (16 bits)
constexpr uint32_t TAM_VECINO1 = 0x12AB0;     // .nhv (antes de la 1.5)
constexpr uint32_t TAM_CASA1 = 0x1D4;         // .nhvh (antes de la 1.5)
constexpr uint16_t SIN_ACENTO = 0x134B;       // la pared blanca: lo que guarda el juego si no hay pared de acento

uint8_t* datosVecino(std::vector<uint8_t>& main, int hueco);
const uint8_t* datosVecino(const std::vector<uint8_t>& main, int hueco);

// Textos en UTF-16 de la partida (como StringUtil de NHSE: «largo» letras, rellenas con ceros).
std::string leerTextoVecino(const uint8_t* p, int largo);
void escribirTextoVecino(uint8_t* p, const std::string& texto, int largo);

// ---------------------------------------------------------------------------------------------- personalidad y frase
// 0 perezoso, 1 deportista, 2 gruñón, 3 presumido, 4 normal, 5 alegre, 6 esnob, 7 hermana mayor.
extern const char* const PERSONALIDADES[8];
int personalidad(const std::vector<uint8_t>& main, int hueco);
void ponerPersonalidad(std::vector<uint8_t>& main, int hueco, int p);
std::string frase(const std::vector<uint8_t>& main, int hueco);
void ponerFrase(std::vector<uint8_t>& main, int hueco, const std::string& texto);
// La muletilla con la que viene el vecino (por su nombre interno), o vacío si no la sé.
std::string fraseOriginal(const std::string& interno);

// ---------------------------------------------------------------------------------------------- mudanza
bool seMuda(const std::vector<uint8_t>& main, int hueco);
// Hace las maletas (y pone «ForceMoveOut» para que el juego se lo lleve de verdad), o se queda, como el Mac.
void ponerMudanza(std::vector<uint8_t>& main, int hueco, bool siONo);
int mudarTodos(std::vector<uint8_t>& main); // devuelve cuántos

// ---------------------------------------------------------------------------------------------- amistad
// Lo que un vecino recuerda de un jugador (8 por vecino). Los vacíos no tienen nombre de jugador.
struct Recuerdo {
    std::string jugador, isla, apodo;
    uint8_t amistad = 0;
};
Recuerdo recuerdo(const std::vector<uint8_t>& main, int hueco, int r);
bool recuerdoUsado(const std::vector<uint8_t>& main, int hueco, int r);
void ponerAmistad(std::vector<uint8_t>& main, int hueco, int r, uint8_t puntos);
void ponerApodo(std::vector<uint8_t>& main, int hueco, int r, const std::string& apodo); // hasta 10 letras
// ¿Es de la isla de ese jugador (su personal.dat)? ¿Y es ese jugador?
bool mismaIsla(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& personal);
bool mismoJugador(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& personal);
// El recuerdo que tiene de ese jugador, o -1 si no lo conoce.
int recuerdoDe(const std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& personal);
// Niveles de amistad (0 a 7 corazones) como los del juego, y su nombre.
extern const uint8_t INICIO_NIVEL[8];
extern const char* const NIVELES[8];
int nivelAmistad(uint8_t puntos);
// Los puntos con que empieza un nivel (el 7, el máximo: 255), como al tocar un corazón en el Mac.
inline uint8_t puntosDeNivel(int nivel) { return nivel >= 7 ? 255 : INICIO_NIVEL[nivel < 0 ? 0 : nivel]; }

// ---------------------------------------------------------------------------------------------- ropa y muebles
// lista: 0 ropa, 1 muebles. Solo se tocan los 8 bytes del objeto, como el Mac.
Objeto objetoVecino(const std::vector<uint8_t>& main, int hueco, int lista, int i);
void ponerObjetoVecino(std::vector<uint8_t>& main, int hueco, int lista, int i, const Objeto& o);

// ---------------------------------------------------------------------------------------------- paredes y suelo
enum ParteCuarto { CuartoAcento = 0, CuartoPared = 1, CuartoSuelo = 2 };
uint16_t parteCuarto(const std::vector<uint8_t>& main, int hueco, int parte);
// Pone ese papel o suelo (todo el objeto: sin parámetros), como RoomWindow.Put. Si ya estaba, no toca nada.
void ponerParteCuarto(std::vector<uint8_t>& main, int hueco, int parte, uint16_t id);

// ---------------------------------------------------------------------------------------------- la casa
// La casa en la que vive (índice en CASAS), o -1.
int casaDe(const std::vector<uint8_t>& main, int hueco);
uint8_t* datosCasa(std::vector<uint8_t>& main, int casa);
enum ParteCasa { CasaFachada = 0, CasaTejado = 1, CasaPuerta = 2 }; // en la casa: 0x08, 0x0A, 0x0C (y lo pedido, 6 más allá)
uint16_t parteCasa(const std::vector<uint8_t>& main, int casa, int parte);
void ponerParteCasa(std::vector<uint8_t>& main, int casa, int parte, uint16_t id); // también lo pedido para mañana
Objeto decoracionPuerta(const std::vector<uint8_t>& main, int casa);
void ponerDecoracionPuerta(std::vector<uint8_t>& main, int casa, const Objeto& o);

// ---------------------------------------------------------------------------------------------- bricolaje
struct Bricolaje {
    uint8_t hora = 255, minuto = 255, segundo = 0;
    uint16_t receta = 0;
    bool haciendo() const { return hora < 24 && minuto < 60; }
};
Bricolaje bricolaje(const std::vector<uint8_t>& main, int hueco);
void ponerBricolaje(std::vector<uint8_t>& main, int hueco, const Bricolaje& b);

// ---------------------------------------------------------------------------------------------- archivos
// El vecino tal cual (.nhv2), como «Exportar» de NHSE.
std::vector<uint8_t> exportarVecino(const std::vector<uint8_t>& main, int hueco);
// Un archivo de vecino (.nhv2 o .nhv, que se convierte) listo para esta partida; vacío si no lo es.
std::vector<uint8_t> vecinoDeArchivo(const std::vector<uint8_t>& archivo);
// ¿Viene de otra isla o de otro jugador (que el primer jugador de esta, su personal.dat)? Y el nombre de su isla.
bool deOtraIsla(const std::vector<uint8_t>& vecino, const std::vector<uint8_t>& personal);
std::string islaDe(const std::vector<uint8_t>& vecino);
// Que recuerde esta isla y a este jugador en lugar de los suyos (VillagerTools.AdaptToIsland).
void hacerDeLaIsla(std::vector<uint8_t>& vecino, const std::vector<uint8_t>& personal);
// Cambia el vecino del hueco por el del archivo (la casa sigue igual). Devuelve el motivo si no puede.
std::string importarVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& archivo, bool hacerSuyo, const std::vector<uint8_t>& personal);

// La casa tal cual (.nhvh2), y traerla de un archivo (.nhvh2 o .nhvh); quien vive en ella sigue igual.
std::vector<uint8_t> exportarCasa(const std::vector<uint8_t>& main, int casa);
std::string importarCasa(std::vector<uint8_t>& main, int casa, const std::vector<uint8_t>& archivo);

// ---------------------------------------------------------------------------------------------- recuerdos
// Todo lo que un vecino recuerda de un jugador, como VillagerMemoryEditor (GSaveMemory): nombre e isla del jugador,
// apodo, saludos y su fecha, y 256 marcas (de 0 a 255; la 10 es la amistad).
constexpr int NUM_SALUDOS = 11;
constexpr int NUM_MARCAS_RECUERDO = 0x100;
std::string saludo(const std::vector<uint8_t>& main, int hueco, int r, int i);           // hasta 22 letras, como lo lee NHSE
void ponerSaludo(std::vector<uint8_t>& main, int hueco, int r, int i, const std::string& texto); // hasta 47
void ponerJugadorRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::string& nombre); // hasta 10
void ponerIslaRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::string& nombre);    // hasta 10
struct FechaVecino {
    uint16_t anio = 0;
    uint8_t mes = 0, dia = 0;
    bool vacia() const { return anio == 0 && mes == 0 && dia == 0; }
};
FechaVecino fechaSaludo(const std::vector<uint8_t>& main, int hueco, int r);
void ponerFechaSaludo(std::vector<uint8_t>& main, int hueco, int r, const FechaVecino& f);
// ¿Es una fecha que NHSE enseña? (las de antes de 1753 o que no existen las deja vacías al guardar)
bool fechaValida(const FechaVecino& f);
uint8_t marcaRecuerdo(const std::vector<uint8_t>& main, int hueco, int r, int i);
void ponerMarcaRecuerdo(std::vector<uint8_t>& main, int hueco, int r, int i, uint8_t valor);
// El recuerdo tal cual (.nhvpm) y traerlo de un archivo. Si es de otro jugador u otra isla que el recuerdo de ahora y
// «hacerSuyo», pasa a ser de los de ahora (ChangeOrigins, como MiscDumpHelper.LoadVillagerMemoryPlayer).
std::vector<uint8_t> exportarRecuerdo(const std::vector<uint8_t>& main, int hueco, int r);
bool recuerdoDeOtro(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& archivo);
std::string importarRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& archivo, bool hacerSuyo);

// ---------------------------------------------------------------------------------------------- marcas sueltas
// Las 256 marcas del vecino (números de 16 bits), como VillagerFlagEditor; en un archivo .nhfl, tal cual.
constexpr int NUM_MARCAS_VECINO = 0x100;
uint16_t marcaVecino(const std::vector<uint8_t>& main, int hueco, int i);
void ponerMarcaVecino(std::vector<uint8_t>& main, int hueco, int i, uint16_t valor);
std::vector<uint8_t> marcasVecinoEnBruto(const std::vector<uint8_t>& main, int hueco);
bool cargarMarcasVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& archivo); // false si no es

// ---------------------------------------------------------------------------------------------- su diseño
// El diseño profesional que lleva el vecino (Villager2.Design), como un .nhpd.
constexpr uint32_t V_DISENO = 0x128A8;
constexpr uint32_t TAM_DISENO_VECINO = 0x8A8;
std::vector<uint8_t> disenoVecino(const std::vector<uint8_t>& main, int hueco);
void ponerDisenoVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& d);

// ---------------------------------------------------------------------------------------------- valores en bruto
// Los valores sueltos de la habitación (GSaveRoomFloorWall) y de la casa (VillagerHouse2), como la tabla de «Opciones
// avanzadas» de RoomWindow y VillagerHouseWindow. «tipo» dice qué es el número, para enseñar su nombre.
enum TipoCampo { CampoNumero, CampoObjeto, CampoFachada, CampoTejado, CampoPuerta, CampoVecino };
struct CampoBruto {
    const char* nombre; // en castellano
    const char* interno; // el de NHSE
    uint16_t pos;
    uint8_t tam;        // 1, 2 o 4 bytes
    bool signo;
    TipoCampo tipo;
};
extern const CampoBruto CAMPOS_CUARTO[];
extern const int NUM_CAMPOS_CUARTO;
extern const CampoBruto CAMPOS_CASA[];
extern const int NUM_CAMPOS_CASA;
int64_t leerCampo(const uint8_t* base, const CampoBruto& c);
void ponerCampo(uint8_t* base, const CampoBruto& c, int64_t valor);
int64_t minimoCampo(const CampoBruto& c);
int64_t maximoCampo(const CampoBruto& c);

} // namespace acnh
