// Núcleo de la sección Jugador: almacén, papelera, recetas, catálogo y reacciones una a una, la rueda de reacciones,
// récords de millas Nook, perfil (pasaporte) y el buzón (postbox.dat). Todo como las ventanas del jugador de NHSE para
// Mac, con las posiciones de NHSE 3.0 (PersonalOffsets30, MainSaveOffsets30, PostBoxOffsets30, Mail).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "acnh.hpp"

namespace acnh {

// Un objeto de 8 bytes (Item de NHSE) en cualquier sitio.
Objeto leerObjeto(const uint8_t* d);
void escribirObjeto(uint8_t* d, const Objeto& o);

// Textos de la partida (UTF-16, «max» letras, rellenos con ceros como StringUtil de NHSE).
std::string leerTexto(const uint8_t* p, int max);
void escribirTexto(uint8_t* p, const std::string& s, int max);
int largoTexto(const std::string& s); // in UTF-16 letters, as the game counts them

// Cambiar el nombre del jugador o de la isla, como NHSE (HorizonSave.ChangeIdentity): el nombre va también en cartas,
// recuerdos de los vecinos..., así que se cambia cada copia de la «identidad» (número + nombre, 24 bytes) en todos
// los archivos. reemplazarTodo devuelve cuántas copias ha cambiado en ese archivo.
constexpr size_t TAM_IDENTIDAD = 24;
constexpr uint32_t ID_ISLA = 0xC248;          // in personal.dat: the island (number + name)
constexpr uint32_t ID_PERSONA = 0xC248 + 0x1C; // and the player (number + name)
int reemplazarTodo(std::vector<uint8_t>& datos, const uint8_t* antes, const uint8_t* despues, size_t n);

// Las Poki (el dinero del hotel), en wherearen.dat.
constexpr uint32_t POKI = 0xB84228;

// Una fecha de la partida (GSaveDate: año, mes, día; todo 0 = sin fecha).
struct Fecha {
    uint16_t anio = 0;
    uint8_t mes = 0, dia = 0;
    bool vacia() const { return anio == 0 && mes == 0 && dia == 0; }
    bool valida() const; // a real day (as PlayerUi.ToDate of the Mac)
};
Fecha leerFecha(const uint8_t* p);
void escribirFecha(uint8_t* p, const Fecha& f);
Fecha masUnDia(Fecha f);
int diasDelMes(int mes, int anio); // anio 0: 29 for February (birthdays)
std::string textoFecha(const Fecha& f); // «12 abr 2020», or «Sin fecha»
extern const char* const MESES[12];

// ---------------------------------------------------------------------------------------- almacén y papelera
// El almacén de casa (personal.dat): 9000 huecos; tras ellos, cuántos ha abierto el jugador. Se usan los abiertos (al
// menos 50), como el Mac.
constexpr uint32_t ALMACEN = 0x37D6C;
constexpr int ALMACEN_MAX = 9000;
int almacenAbiertos(const std::vector<uint8_t>& personal);
Objeto leerAlmacen(const std::vector<uint8_t>& personal, int hueco);
void escribirAlmacen(std::vector<uint8_t>& personal, int hueco, const Objeto& o);

// La papelera de reciclaje (main.dat, LostItemBox): 40 objetos.
constexpr uint32_t PAPELERA = 0x978D10;
constexpr int NUM_PAPELERA = 40;
Objeto leerPapelera(const std::vector<uint8_t>& main, int hueco);
void escribirPapelera(std::vector<uint8_t>& main, int hueco, const Objeto& o);

// Billetes del hotel (3.0): un número cifrado; solo si la partida ya tiene el hotel.
constexpr uint32_t BILLETES_HOTEL = 0x742B0;
bool hayHotel(const std::vector<uint8_t>& personal);

// ------------------------------------------------------------------------------------------------------- recetas
// Las cuatro listas del recetario: sabidas, fabricadas, nuevas y favoritas.
enum ListaReceta { Sabida = 0, Fabricada = 1, Nueva = 2, Favorita = 3 };
bool receta(const std::vector<uint8_t>& personal, int indice, ListaReceta l);
void ponerReceta(std::vector<uint8_t>& personal, int indice, ListaReceta l, bool si);
// Aprender o olvidar una (como «Aprendida» del Mac: sale como nueva; al olvidarla deja de estar fabricada y favorita).
void aprenderReceta(std::vector<uint8_t>& personal, int indice, bool si);
int fabricarSabidas(std::vector<uint8_t>& personal); // CraftAll: the known ones as made (and not new); returns how many
void olvidarRecetas(std::vector<uint8_t>& personal);  // ClearAll

// ------------------------------------------------------------------------------------------------------ catálogo
bool recibido(const std::vector<uint8_t>& personal, uint16_t id);
// Lo marca (con todos sus colores) como recibido o no, como CatalogWindow. «k» es su número en CATALOGO_NOMBRES.
void recibir(std::vector<uint8_t>& personal, int k, bool si);

// ---------------------------------------------------------------------------------------------------- reacciones
bool sabeReaccion(const std::vector<uint8_t>& personal, uint8_t r);
bool aprenderReaccion(std::vector<uint8_t>& personal, uint8_t r); // false if there is no room
void olvidarReaccion(std::vector<uint8_t>& personal, uint8_t r);   // and it leaves the wheel
constexpr int NUM_RUEDA = 8;
uint8_t rueda(const std::vector<uint8_t>& personal, int hueco);    // 0 = empty
void ponerRueda(std::vector<uint8_t>& personal, int hueco, uint8_t r);
const char* nombreReaccion(uint8_t r); // «Felicidad», or nullptr

// --------------------------------------------------------------------------------------------- récords (millas)
constexpr uint32_t RECORDS_POS = 0xE118; // CountAchievement: cuentas, sellos vistos, fechas y «visible»
constexpr int NUM_RECORDS_MAX = 512;
uint32_t cuentaRecord(const std::vector<uint8_t>& personal, int indice);
void ponerCuentaRecord(std::vector<uint8_t>& personal, int indice, uint32_t n);
Fecha fechaSello(const std::vector<uint8_t>& personal, int indice, int sello);
void ponerFechaSello(std::vector<uint8_t>& personal, int indice, int sello, const Fecha& f);
// k: número en RECORDS.
bool selloConseguido(const std::vector<uint8_t>& personal, int k, int sello);
int sellosConseguidos(const std::vector<uint8_t>& personal, int k);
bool recordPorPasos(int k); // its stamps are separate steps (one bit each), not a count to reach
void ponerPaso(std::vector<uint8_t>& personal, int k, int sello, bool si);
// «Al máximo» (GiveAll de un récord y todos sus pasos), con los sellos nuevos fechados desde «hoy».
void completarRecord(std::vector<uint8_t>& personal, int k, Fecha hoy);
void completarRecords(std::vector<uint8_t>& personal, Fecha hoy); // all of them (and clears the unknown ones), as the Mac
void borrarRecord(std::vector<uint8_t>& personal, int indice);
void borrarRecords(std::vector<uint8_t>& personal);
int recordsCompletos(const std::vector<uint8_t>& personal);

// --------------------------------------------------------------------------------------------- perfil (pasaporte)
constexpr uint32_t CUMPLE = 0x1352C;         // mes, día
constexpr uint32_t CUMPLE_PERFIL = 0x36598;  // the same, on the passport
constexpr uint32_t FRUTA_PERFIL = 0x3659C;
constexpr uint32_t REGISTRO_PERFIL = 0x3660C; // GSaveDate
constexpr uint32_t FUNDADOR_PERFIL = 0x36610;
struct Perfil {
    uint8_t mes = 0, dia = 0; // birthday (0 = none)
    uint16_t fruta = 0;
    Fecha registro;
    bool fundador = false;
};
Perfil leerPerfil(const std::vector<uint8_t>& personal);
// Como «Guardar» de ProfileWindow: el cumpleaños solo si lo hay; el resto, siempre.
void escribirPerfil(std::vector<uint8_t>& personal, const Perfil& p);

// ---------------------------------------------------------------------------------------------- buzón (postbox.dat)
constexpr uint32_t CARTAS = 0x110;
constexpr uint32_t TAM_CARTA = 0x3C8;
constexpr int NUM_CARTAS = 300;          // slots 0-299 (NHSE only lists 0-298)
constexpr uint32_t ULTIMA_CARTA = 0x47420; // next sort number
constexpr int LARGO_PARA = 47, LARGO_MENSAJE = 239, LARGO_FIRMA = 47;
inline uint8_t* carta(std::vector<uint8_t>& buzon, int slot) { return buzon.data() + CARTAS + slot * TAM_CARTA; }
inline const uint8_t* carta(const std::vector<uint8_t>& buzon, int slot) { return buzon.data() + CARTAS + slot * TAM_CARTA; }

// Las cartas del buzón, la más nueva primero (como la lista del Mac).
std::vector<int> cartasUsadas(const std::vector<uint8_t>& buzon);
int cartaLibre(const std::vector<uint8_t>& buzon); // -1 if full

// Quién la manda: 0 un jugador, 1 un vecino, 2 un personaje especial.
int tipoRemitente(const std::vector<uint8_t>& buzon, int slot);
std::string remitente(const std::vector<uint8_t>& buzon, int slot); // its name
// Cambia quién la manda, como el Mac. «especie» y «variante»: del vecino, o el número del personaje (PERSONAJES).
void ponerRemitente(std::vector<uint8_t>& buzon, int slot, int tipo, const std::vector<uint8_t>& personal, int especie = 0, int variante = 0);

bool leida(const std::vector<uint8_t>& buzon, int slot);
void ponerLeida(std::vector<uint8_t>& buzon, int slot, bool si);
bool favorita(const std::vector<uint8_t>& buzon, int slot);
void ponerFavorita(std::vector<uint8_t>& buzon, int slot, bool si);
Fecha fechaCarta(const std::vector<uint8_t>& buzon, int slot);
void ponerFechaCarta(std::vector<uint8_t>& buzon, int slot, const Fecha& f);

enum TextoCarta { Para, Mensaje, Firma, Destinatario, IslaDestino };
std::string textoCarta(const std::vector<uint8_t>& buzon, int slot, TextoCarta t);
void ponerTextoCarta(std::vector<uint8_t>& buzon, int slot, TextoCarta t, const std::string& s);
int largoMaximo(TextoCarta t);

Objeto regalo(const std::vector<uint8_t>& buzon, int slot);
// Pone un regalo (id y cantidad, o NADA para quitarlo) y deja el envoltorio que tenía.
void ponerRegalo(std::vector<uint8_t>& buzon, int slot, uint16_t id, uint16_t cuenta);
bool esCarta(const std::vector<uint8_t>& buzon, int slot); // letter (true) or parcel
void ponerEsCarta(std::vector<uint8_t>& buzon, int slot, bool carta); // and clears the wrapping (and the paper of a parcel)
uint16_t papelCarta(const std::vector<uint8_t>& buzon, int slot);
void ponerPapelCarta(std::vector<uint8_t>& buzon, int slot, uint16_t codigo);
uint8_t envoltorio(const std::vector<uint8_t>& buzon, int slot);
void ponerEnvoltorio(std::vector<uint8_t>& buzon, int slot, uint8_t codigo);

// Una carta nueva sin leer en el primer hueco libre, del primer vecino de la isla (o de Tom Nook), con fecha «hoy».
// Devuelve el hueco o -1 si el buzón está lleno.
int nuevaCarta(std::vector<uint8_t>& buzon, const std::vector<uint8_t>& main, const std::vector<uint8_t>& personal, Fecha hoy);
void borrarCarta(std::vector<uint8_t>& buzon, int slot);
// Una carta exportada (.nhpb, TAM_CARTA bytes) en un hueco libre. Devuelve el hueco, -1 si está lleno o -2 si no es
// una carta.
int importarCarta(std::vector<uint8_t>& buzon, const std::vector<uint8_t>& datos);
std::vector<uint8_t> exportarCarta(const std::vector<uint8_t>& buzon, int slot);

} // namespace acnh
