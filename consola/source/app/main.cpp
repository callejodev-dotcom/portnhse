// NHSE para Switch: editor de la partida de Animal Crossing: New Horizons que se abre desde el Álbum (con el juego
// cerrado). Hace una copia de toda la partida en la SD al empezar, y al guardar comprueba lo escrito leyéndolo de vuelta.
// La forma de abrir la partida del dispositivo viene de cbreezy210/ACNH-Save-Editor (MIT).
//
// Esta es la pantalla de inicio (las secciones, guardar y salir); cada sección pone sus tarjetas desde su archivo
// (jugador.cpp, vecinos.cpp, isla.cpp...).
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include "partida.hpp"
#include "ui.hpp"

using namespace ui;

namespace {

const char* const SECCIONES[NUM_SECCIONES] = {"Jugador", "Vecinos", "Isla", "Diseños", "Avanzado"};
const char* const DETALLES[NUM_SECCIONES] = {"Dinero, bolsillos, almacén, recetas…", "Quién vive, amistad, sus casas…",
    "Mapa, flores, museo, nabos, casas…", "Diseños normales y profesionales", "Para expertos"};

std::vector<Tarjeta> deSeccion(Seccion s)
{
    std::vector<Tarjeta> r;
    for (const auto& t : tarjetas())
        if (t.seccion == s)
            r.push_back(t);
    std::stable_sort(r.begin(), r.end(), [](const Tarjeta& a, const Tarjeta& b) { return a.orden < b.orden; });
    return r;
}

// Una sección: sus tarjetas, 3 por fila, en páginas de 9.
Pantalla seccion(Seccion s)
{
    auto pagina = std::make_shared<int>(0);
    return [s, pagina](Vista& v) {
        const auto ts = deSeccion(s);
        const int paginas = std::max<int>(1, (ts.size() + 8) / 9);
        *pagina = std::clamp(*pagina, 0, paginas - 1);
        v.titulo = std::string(SECCIONES[s]) + (s == SeccionJugador ? ": " + partida::jugador().nombre : "");
        v.explicacion = paginas > 1 ? "Página " + std::to_string(*pagina + 1) + " de " + std::to_string(paginas) + " (L y R para pasar)." : "";
        for (int k = 0; k < 9 && *pagina * 9 + k < (int)ts.size(); k++)
        {
            const Tarjeta& t = ts[*pagina * 9 + k];
            Boton b{{50 + (k % 3) * 390, 140 + (k / 3) * 125, 380, 105}, t.titulo(), t.abrir, !t.activa || t.activa()};
            b.detalle = t.detalle ? t.detalle() : "";
            b.grande = true;
            v.b.push_back(b);
        }
        if (paginas > 1)
        {
            v.b.push_back({{50, ABAJO - 72, 260, 60}, "Anteriores", [pagina] { (*pagina)--; }, *pagina > 0});
            v.b.push_back({{330, ABAJO - 72, 260, 60}, "Siguientes", [pagina] { (*pagina)++; }, *pagina + 1 < paginas});
            v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + d, 0, paginas - 1); };
        }
        v.b.push_back({{paginas > 1 ? W - 430 : W / 2 - 190, ABAJO - 72, 380, 60}, "Volver", [] { volver(); }});
    };
}

void guardar()
{
    const std::string mal = partida::guardar();
    avisar(mal.empty() ? "Guardado y comprobado. Ya puedes abrir el juego." : mal, !mal.empty());
}

void salir()
{
    if (!partida::hayCambios())
        terminar();
    else
        confirmar("¿Salir sin guardar?", "Hay cambios sin guardar. Si sales ahora, la partida se queda como estaba.", "Salir sin guardar", [] { terminar(); });
}

void inicio(Vista& v)
{
    v.titulo = partida::isla + " · " + partida::jugador().nombre;
    v.explicacion = "Elige qué quieres cambiar. Nada se guarda en la partida hasta que pulses «Guardar».";
    for (int s = 0; s < NUM_SECCIONES; s++)
    {
        Boton b{{50 + (s % 3) * 390, 150 + (s / 3) * 130, 380, 110}, SECCIONES[s], [s] { abrir(seccion((Seccion)s)); }};
        b.detalle = DETALLES[s];
        b.grande = true;
        v.b.push_back(b);
    }
    const bool cambios = partida::hayCambios();
    Boton g{{830, 280, 380, 110}, "Guardar", guardar, cambios, cambios};
    g.detalle = cambios ? "Escribir los cambios en la partida" : "No hay cambios";
    g.grande = true;
    v.b.push_back(g);
    if (partida::jugadores.size() > 1)
        v.b.push_back({{50, 440, 570, 70}, "Jugador: " + partida::jugador().nombre + " (cambiar)", [] {
            std::vector<std::string> nombres;
            for (const auto& j : partida::jugadores)
                nombres.push_back(j.nombre);
            elegir("Jugadores de " + partida::isla, "Elige a quién cambiar el dinero, los bolsillos, las recetas…", nombres, partida::yo, [](int i) {
                partida::yo = i;
                avisar("Ahora cambias a " + partida::jugadores[i].nombre + ".");
            });
        }});
    v.b.push_back({{partida::jugadores.size() > 1 ? 640 : 440, 440, partida::jugadores.size() > 1 ? 570 : 400, 70}, cambios ? "Salir sin guardar" : "Salir", salir});
    v.atras = salir;
    v.y = [] {
        if (partida::hayCambios())
            guardar();
    };
    v.teclas = {{"A", "Elegir"}, {"Y", "Guardar"}, {"+", "Salir"}};
    avisoFijo(cambios ? "Hay cambios sin guardar." : "Copia de seguridad en " + partida::copia.substr(5));
}

} // namespace

int main(int, char**)
{
    romfsInit();
    plInitialize(PlServiceType_User);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("NHSE", 0, 0, W, H, 0);
    g_r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    PlFontData font;
    plGetSharedFontByType(&font, PlSharedFontType_Standard);
    g_fuente = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 28);
    g_pequena = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 22);
    g_grande = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, 40);

    // Mientras se hace la copia de seguridad (unos segundos), un aviso.
    rect({0, 0, W, H}, CREMA);
    texto("Haciendo una copia de la partida en la SD…", W / 2, H / 2, MARRON, g_grande, Centro);
    SDL_RenderPresent(g_r);

    const std::string error = partida::abrir();
    if (error.empty())
        bucle(inicio);
    else
        bucle([error](Vista& v) {
            v.titulo = "No se puede abrir la partida";
            v.b.push_back({{490, 560, 300, 80}, "Salir", [] { terminar(); }});
            v.atras = [] { terminar(); };
            v.teclas = {{"A", "Salir"}};
            v.dibujo = [error] { texto(error, 100, 200, ROJO, g_grande, Izquierda, 1080); };
        });

    partida::cerrar();
    TTF_CloseFont(g_fuente);
    TTF_CloseFont(g_pequena);
    TTF_CloseFont(g_grande);
    TTF_Quit();
    SDL_DestroyRenderer(g_r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    plExit();
    romfsExit();
    return 0;
}
