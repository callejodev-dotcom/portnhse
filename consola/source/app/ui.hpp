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

extern const SDL_Color CREMA, CREMA_OSCURA, VERDE, VERDE_OSCURO, MARRON, MARRON_SUAVE, BLANCO, GRIS, ROJO, AMARILLO, AZUL;

struct Letras {
    SDL_Texture* t;
    int w, h;
};
Letras letras(const std::string& s, TTF_Font* f, SDL_Color c, int ancho = 0);
void rect(const SDL_Rect& r, SDL_Color c);
void redondo(const SDL_Rect& r, int radio, SDL_Color c);
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
};
std::vector<Tarjeta>& tarjetas();
struct Registro {
    explicit Registro(Tarjeta t) { tarjetas().push_back(std::move(t)); }
};

// El bucle: lee los botones, dibuja la pantalla de arriba y hace lo que se pulse. Devuelve al salir.
void bucle(Pantalla inicio);

} // namespace ui
