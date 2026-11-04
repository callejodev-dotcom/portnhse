// El suelo de la isla pintado como el mapa del juego (como IslandPainter de NHSE para Mac), y el mapa entero con lo
// que hay encima: edificios y objetos de las dos capas, en trozos de textura que solo se vuelven a dibujar cuando cambia
// algo de ese trozo.
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "../core/mapa.hpp"

namespace mapa {

// Puntos por casilla de objeto en el dibujo (16 por cuadro, como el Mac).
constexpr int PX = 8;
// Desde este zoom (puntos de pantalla por casilla) las cosas se ven con sus dibujos, como en el Mac; más lejos, con
// formas y colores (los dibujos no se distinguirían y costaría dibujarlos).
constexpr double ZOOM_DIBUJOS = 8;

// El suelo entero (288 × 256 casillas, con el mar) en puntos ARGB. Tarda: una vez por partida.
std::vector<uint32_t> pintarSuelo(const std::vector<uint8_t>& main);
uint32_t colorMar();

// El mapa dibujado: el suelo, la plaza, los puentes y cuestas, los objetos del suelo, los edificios y lo que está
// encima de otras cosas. Se guarda en texturas de 256 × 256 puntos.
class Lienzo {
public:
    Lienzo();
    ~Lienzo();
    // Pinta el suelo (lento) si no está pintado o cambió el terreno.
    void preparar(std::vector<uint8_t>& main);
    bool listo() const { return !suelo.empty(); }
    // Mira qué casillas han cambiado desde la última vez y vuelve a dibujar esos trozos. «capa» es la que se edita
    // (la otra se ve más tenue).
    void actualizar(std::vector<uint8_t>& main, int capa);
    // Dibuja la parte del mapa que se ve: «x0, y0» es la casilla (puede tener decimales) en la esquina de «destino» y
    // «zoom» los puntos de pantalla por casilla. Con zoom suficiente (ZOOM_DIBUJOS), con los dibujos de las cosas: en
    // trozos de textura que se hacen una vez (unos pocos por fotograma) y se guardan los vistos hace poco.
    void dibujar(std::vector<uint8_t>& main, const SDL_Rect& destino, double x0, double y0, double zoom);
    // Vuelve a dibujar todos los trozos (han cambiado los puentes o las cuestas, que van con el suelo).
    void marcarTodo();

private:
    struct Trozo;
    std::vector<uint32_t> suelo;
    uint32_t sumaTerreno = 0;
    std::vector<uint8_t> visto; // las dos capas de objetos como se dibujaron
    int capaVista = -1;
    std::vector<std::unique_ptr<Trozo>> trozos;
    std::vector<bool> sucio;
    uint32_t reloj = 0;
    void marcar(int t);
    void componer(std::vector<uint8_t>& main, int t);
    void componerFoto(std::vector<uint8_t>& main, int t);
};

// Los edificios y la plaza que se dibujan (de la partida abierta).
void ponerEdificios(const std::vector<Edificio>& e, uint32_t plazaX, uint32_t plazaZ);
// El color de los objetos de cada categoría (mapa::categoria) en el mapa y en las casas.
uint32_t colorCategoria(int c);

// El dibujo de un objeto en el mapa y en las casas (GridItems.GetPicture): el suyo; los de suelo, el de lo que dan; las
// rocas, hoyos y tocones, uno de lo que son. nullptr si no hay.
SDL_Texture* dibujoDe(const Objeto& o);
// El objeto cuyo dibujo lleva un edificio (el buzón en las casas, la hoja en los servicios...), o 0.
uint16_t iconoDeEdificio(int tipo);

// El lienzo de la isla, compartido entre aperturas del mapa (pintar el suelo tarda).
Lienzo& lienzo();

} // namespace mapa
