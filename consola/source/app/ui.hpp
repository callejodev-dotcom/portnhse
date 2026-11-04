// Las piezas de la interfaz de NHSE para Switch: dibujar, botones, pantallas apiladas, teclado y avisos.
//
// Cada pantalla es una función que rellena una Vista (título, botones, qué hacen B, L/R, X, Y...) y se vuelve a llamar
// en cada fotograma, así siempre muestra lo último. Se abren encima de la anterior con abrir() y se vuelve con volver().
// Las secciones (jugador, vecinos, isla...) añaden sus tarjetas con un Registro (ver tarjetas()).
#pragma once
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {

constexpr int W = 1280, H = 720;
constexpr int ABAJO = H - 48; // where the help bar starts: buttons end above it

extern SDL_Renderer* g_r;
extern TTF_Font* g_fuente;  // 28
extern TTF_Font* g_pequena; // 22
extern TTF_Font* g_grande;  // 40
extern TTF_Font* g_negrita; // 28, bold (titles of cards)
extern TTF_Font* g_titulo;  // 40, bold (title of the screen)

extern const SDL_Color CREMA, CREMA_OSCURA, VERDE, VERDE_OSCURO, MARRON, MARRON_SUAVE, BLANCO, GRIS, ROJO, AMARILLO, AZUL;

struct Letras {
    SDL_Texture* t;
    int w, h;
};
Letras letras(const std::string& s, TTF_Font* f, SDL_Color c, int ancho = 0);
// Alto de un renglón de esa letra (el mismo en la consola y en el simulador; ver letras). Siempre este, nunca
// TTF_FontHeight.
int alto(TTF_Font* f = nullptr);
// Al abrir cada letra, su tamaño, para que mida igual en la consola que en el simulador.
void medirLetra(TTF_Font* f, int pt);
void rect(const SDL_Rect& r, SDL_Color c);
void redondo(const SDL_Rect& r, int radio, SDL_Color c);
// Un borde de «grosor» puntos por fuera del rectángulo, y un polígono convexo relleno. Las pantallas se dibujan un poco
// más pequeñas: las líneas de un punto (SDL_RenderDrawLine/Rect) se pueden perder; mejor estas.
void marco(const SDL_Rect& r, SDL_Color c, int grosor);
void poligono(const std::vector<SDL_FPoint>& p, SDL_Color c);
// Como SDL_RenderDrawLine (solo horizontales o verticales) y SDL_RenderDrawRect, con el color puesto, pero de 2 puntos.
void linea(int x1, int y1, int x2, int y2);
void borde(const SDL_Rect& r);
enum Alinear { Izquierda, Centro, Derecha };
// Texto en (x, y): con Centro, centrado en x e y; con Derecha, acaba en x. Con «ancho», en varias líneas.
int texto(const std::string& s, int x, int y, SDL_Color c, TTF_Font* f = nullptr, Alinear a = Izquierda, int ancho = 0);
// Recorta un texto con «…» para que quepa en ese ancho.
std::string encajar(std::string s, int ancho, TTF_Font* f = nullptr);
int anchoTexto(const std::string& s, TTF_Font* f = nullptr);

struct Boton {
    SDL_Rect r;
    std::string texto;
    std::function<void()> accion;
    bool activo = true;
    bool marcado = false;
    std::string detalle; // second line, smaller
    bool grande = false;  // white card with a stripe (home and sections)
    SDL_Color color = {0, 0, 0, 0}; // a square of this colour on the left (designs, maps), if alpha > 0
    SDL_Texture* imagen = nullptr;  // a picture on the left (the item's, see dibujos.hpp)
    bool principal = false;         // the main action (blue, as «Guardar» in the Mac)
    bool casilla = false;           // a square of items, as the Mac: «color» is its background, «imagen» in the middle,
                                    // «detalle» (how many) at the bottom right
    bool vertical = false;          // a card with the picture big on top and the text below (villagers, as the Mac)
    bool pequena = false;           // small letters (all the buttons of a row the same)
    std::vector<SDL_Texture*> tira; // with «grande»: small pictures on the right (the faces of the visitors...)
    std::string valor;              // with «grande»: a number card, as the money of the Mac («texto» small on top,
                                    // «valor» big, «detalle» below)
};
void dibujar(const Boton& b, bool foco);

using Teclas = std::vector<std::pair<std::string, std::string>>;

// Una pantalla en un fotograma.
struct Vista {
    std::string titulo;      // top right, on the green bar
    std::string explicacion; // under the bar
    std::vector<Boton> b;
    std::function<void()> atras;      // B; by default volver()
    std::function<void(int)> paginar; // L/R (-1 / +1); ZL/ZR give -10 / +10
    std::function<void()> x, y, menos; // X, Y and − buttons
    Teclas teclas;                      // the help at the bottom; by default «A Elegir, B Volver»
    // Free drawing before the buttons (a map, a design) and own input; «entrada» returns true if it used the input.
    std::function<void()> dibujo;
    std::function<bool(u64 down, u64 held, int tx, int ty)> entrada;
};

using Pantalla = std::function<void(Vista&)>;

void abrir(Pantalla p, int foco = 0); // on top of the current one
void volver();                        // back to the previous one
void reemplazar(Pantalla p, int foco = 0);
void alInicio();                      // back to the first one
void enfocar(int foco);               // focus that button of the current screen
int foco();
void terminar();                      // leave the app (after this frame)
bool terminando();

// Un aviso en la barra de abajo hasta la siguiente acción.
void avisar(const std::string& s, bool malo = false);
// Lo que pone la barra de abajo en la primera pantalla cuando no hay aviso.
void avisoFijo(const std::string& s);
// «Falta guardar»: el aviso de siempre tras un cambio.
void cambiado(const std::string& que = "Cambiado");

// El teclado de la consola. Devuelven false si se cancela.
bool pedirNumero(const std::string& titulo, uint32_t& valor, uint32_t maximo, uint32_t minimo = 0);
bool pedirTexto(const std::string& titulo, const std::string& pista, std::string& valor, int largo = 40);

std::string numero(uint32_t n); // 1.234.567

// ------------------------------------------------------------------------------------------- pantallas que se repiten
// Una fila de una lista.
struct Fila {
    std::string texto;
    std::function<void()> accion;
    bool activo = true;
    bool marcado = false;
    std::string detalle;
    SDL_Color color = {0, 0, 0, 0};
    SDL_Texture* imagen = nullptr;
};

struct Lista {
    std::string titulo;
    std::function<std::string()> explicacion;
    std::function<int()> cuantas;      // rows
    std::function<Fila(int)> fila;     // row i (only the ones shown are asked)
    std::vector<std::function<Fila()>> abajo; // buttons at the bottom (after Anteriores/Siguientes)
    int columnas = 2;
    int porColumna = 8;
    int alto = 0;                       // of each row; 0 = what fits
    std::function<void(Vista&)> extra;  // more things on the view (X, Y, help...)
};
// Una lista en páginas: L/R pasan de página, con «Anteriores» y «Siguientes» si hay más de una.
Pantalla lista(Lista l);

// Elegir una de varias opciones; «elegido» se llama con su número y luego se vuelve.
void elegir(const std::string& titulo, const std::string& explicacion, const std::vector<std::string>& opciones, int actual,
    std::function<void(int)> elegido, int columnas = 2);

// ¿Seguro? Con «Sí» hace la acción y vuelve; con «No» solo vuelve.
void confirmar(const std::string& titulo, const std::string& pregunta, const std::string& si, std::function<void()> accion);

// Una lista de cosas que se marcan o no (recetas, catálogo, museo...): con buscar (X), marcar todas y desmarcar todas
// (de lo que se ve).
struct Marcas {
    std::string titulo;
    int total;
    std::function<std::string(int)> nombre;
    std::function<std::string(int)> clave;   // for searching: lower case without accents (by default, from nombre)
    std::function<bool(int)> marcada;
    std::function<void(int, bool)> marcar;   // marks it in the save (and says it changed)
    std::string queEs = "cosas";              // «recetas», «objetos»...
    std::function<SDL_Texture*(int)> imagen;  // a picture for each one (the items of the catalog...), if there is
};
Pantalla marcas(Marcas m);

// ----------------------------------------------------------------------------------------------- secciones y tarjetas
enum Seccion { SeccionJugador, SeccionVecinos, SeccionIsla, SeccionDisenos, SeccionAvanzado, NUM_SECCIONES };

// Una tarjeta de una sección: un título, un detalle (lo que hay ahora) y qué abre.
struct Tarjeta {
    Seccion seccion;
    int orden;
    std::function<std::string()> titulo;
    std::function<std::string()> detalle;
    std::function<void()> abrir;
    std::function<bool()> activa; // null = always
    const char* icono = nullptr;  // a menu icon of NHSE («Bag», «Kabu»...), as the cards of the Mac
};
std::vector<Tarjeta>& tarjetas();
// Dibujitos para la tarjeta de una sección (por cómo empieza su título), como el Mac: la fruta y la flor, las caras de
// los visitantes, las miniaturas de los diseños...
struct Miniaturas {
    Miniaturas(const char* titulo, std::function<std::vector<SDL_Texture*>()> f);
};
std::vector<SDL_Texture*> miniaturasDe(const std::string& titulo);
struct Registro {
    explicit Registro(Tarjeta t) { tarjetas().push_back(std::move(t)); }
};

// La barra de la izquierda, como la de NHSE para Mac: cada jugador, las secciones, guardar y salir. La pone main.cpp.
struct EntradaLateral {
    std::string texto;
    SDL_Texture* icono;   // or nullptr: then «inicial» in a circle
    std::string inicial;
    bool elegida;
    std::function<void()> accion;
};
struct Lateral {
    std::vector<EntradaLateral> entradas;
    bool hayCambios;
    std::function<void()> guardar, salir;
};
void ponerLateral(std::function<Lateral()> f);
// La pila de pantallas pasa a ser solo esta (al elegir una sección en la barra).
void raiz(Pantalla p);
int profundidad(); // 1 = the screen at the bottom of the stack

// Un recuadro como las tarjetas de NHSE para Mac: blanco con borde, y arriba un icono (de menú, «Kabu»...), un título
// en negrita y una explicación en gris. Devuelve dónde empieza lo de dentro (y).
int panel(const SDL_Rect& r, const std::string& titulo, const char* icono = nullptr, const std::string& explicacion = "");

// El icono de menú que va con un texto (por sus palabras: «Recetas…» → el libro de recetas), o nullptr. La app se lo
// pone sola a los botones de los menús que no tienen dibujo; las pantallas también lo pueden usar.
SDL_Texture* iconoPara(const std::string& texto);

// Un buscador en tiempo real, con su propio teclado en pantalla (el de la consola no deja ver los resultados mientras
// se escribe): arriba lo encontrado, abajo las letras. «filtrar» se llama al cambiar el texto y devuelve cuántos hay;
// «fila» da el resultado i. «alSalir» (si hay) recibe el texto al volver.
struct Buscador {
    std::string titulo;
    std::string explicacion;
    std::string texto;                                     // what is written at the start
    std::function<int(const std::string&)> filtrar;
    std::function<Fila(int)> fila;
    std::function<void(const std::string&)> alSalir;
};
Pantalla buscador(Buscador b);

// Con poca memoria (abierta desde el Álbum), lo que se puede volver a hacer (dibujos guardados...) se tira. Cada parte
// que guarda cosas así pone aquí cómo tirarlas; se llama entre fotogramas.
void alFaltarMemoria(std::function<void()> f);
// La memoria libre, en MB (en el Mac, mucha).
int memoriaLibre();

// Cuántos fotogramas se han dibujado (para saber qué se ha usado en este).
uint64_t fotograma();

// El bucle: lee los botones, dibuja la pantalla de arriba y hace lo que se pulse. Devuelve al salir.
void bucle(Pantalla inicio);

} // namespace ui
