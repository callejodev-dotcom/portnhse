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
#include <memory>
#include "dibujos.hpp"
#include "foto.hpp"
#include "partida.hpp"
#include "registro.hpp"
#include "ui.hpp"

using namespace ui;

namespace {

const char* const SECCIONES[NUM_SECCIONES] = {"Jugador", "Vecinos", "Isla", "Diseños", "Avanzado"};
// Los iconos de la barra de la izquierda, los mismos que NHSE para Mac.
const char* const ICONOS_SECCION[NUM_SECCIONES] = {nullptr, "Leaf", "PltPalm", "MyDesignPro", "ToolChangeStick"};
Seccion g_seccion = SeccionJugador;

// El icono de cada tarjeta, como en NHSE para Mac (por cómo empieza su título).
const char* iconoDe(const Tarjeta& t, const std::string& titulo)
{
    if (t.icono)
        return t.icono;
    static const std::pair<const char*, const char*> ICONOS[] = {
        {"Dinero", "MoneyBag039"}, {"Nombres", "Haniwa"}, {"Bolsillos", "Bag"}, {"Almacén", "Cardboard"},
        {"Papelera", "Can"}, {"Recetas", "BookRecipe"}, {"Catálogo", "Book"}, {"Reacciones", "Love"},
        {"Récords", "StarPiece"}, {"Correo", "Post"}, {"Perfil", "BdayCupcake"}, {"Aprender", "DIYRecipePlus"},
        {"Tus vecinos", "Leaf"}, {"Mejores amigos", "Love"}, {"Amistad", "Present"}, {"Que se muden", "YellowPaperBag"},
        {"Guardar todos", "Cardboard"}, {"Mapa", "Fence"}, {"Casas", "Porch"}, {"Flores", "PltCosmos0"},
        {"Quitar malas", "Scoop"}, {"Hemisferio", "PltPalm"}, {"Fruta", "Peach"}, {"Museo", "Fossil"},
        {"Zona de acampada", "Tent"}, {"Nabos", "Kabu"}, {"Visitantes", "Balloon"}, {"Tablón", "Megaphone"},
        {"Mis diseños", "MyDesignPro"}, {"Diseños PRO", "MyDesignPro"}, {"Bandera", "Flowershower"},
        {"Hermanas", "TailorTicket"}, {"Progreso", "Book"}, {"Clima", "Syabon"}, {"Huecos", "Bag"},
        {"Comprobar", "Timer"}, {"Exportar", "Cardboard"}, {"Importar", "Cardboard"}, {"Memoria", "ToolChangeStick"},
    };
    for (const auto& i : ICONOS)
        if (titulo.rfind(i.first, 0) == 0)
            return i.second;
    return "Leaf";
}

std::vector<Tarjeta> deSeccion(Seccion s)
{
    std::vector<Tarjeta> r;
    for (const auto& t : tarjetas())
        if (t.seccion == s)
            r.push_back(t);
    std::stable_sort(r.begin(), r.end(), [](const Tarjeta& a, const Tarjeta& b) { return a.orden < b.orden; });
    return r;
}

void guardar()
{
    registro::apuntar("guardar");
    const std::string mal = partida::guardar();
    registro::apuntar("guardar: %s", mal.empty() ? "bien" : mal.c_str());
    avisar(mal.empty() ? "Guardado y comprobado. Ya puedes abrir el juego." : mal, !mal.empty());
}

void salir()
{
    if (!partida::hayCambios())
        terminar();
    else
        confirmar("¿Salir sin guardar?", "Hay cambios sin guardar. Si sales ahora, la partida se queda como estaba.", "Salir sin guardar", [] { terminar(); });
}

// Una sección, como las páginas de NHSE para Mac: sus tarjetas con icono, 2 por fila, en páginas de 12.
Pantalla seccion(Seccion s)
{
    auto pagina = std::make_shared<int>(0);
    return [s, pagina](Vista& v) {
        const auto ts = deSeccion(s);
        const int porPagina = 12;
        const int paginas = std::max<int>(1, (ts.size() + porPagina - 1) / porPagina);
        *pagina = std::clamp(*pagina, 0, paginas - 1);
        if (s == SeccionJugador)
        {
            v.titulo = partida::jugador().nombre;
            v.explicacion = "Isla " + partida::isla + ". Nada se guarda en la partida hasta que pulses «Guardar».";
        }
        else
        {
            v.titulo = s == SeccionIsla ? "Tu isla" : s == SeccionVecinos ? "Tus vecinos" : SECCIONES[s];
            v.explicacion = s == SeccionVecinos ? "Quién vive en la isla, su amistad, sus casas…" : s == SeccionIsla ? "El mapa, las casas, el museo, los nabos…"
                : s == SeccionDisenos ? "Diseños normales, profesionales, la bandera y los de la sastrería." : "Herramientas para expertos.";
        }
        if (paginas > 1)
            v.explicacion += " (página " + std::to_string(*pagina + 1) + " de " + std::to_string(paginas) + ")";
        for (int k = 0; k < porPagina && *pagina * porPagina + k < (int)ts.size(); k++)
        {
            const Tarjeta& t = ts[*pagina * porPagina + k];
            const std::string titulo = t.titulo();
            Boton b{{60 + (k % 2) * 590, 128 + (k / 2) * 88, 570, 78}, titulo, t.abrir, !t.activa || t.activa()};
            b.detalle = t.detalle ? t.detalle() : "";
            b.grande = true;
            b.imagen = dibujos::menu(iconoDe(t, titulo));
            b.tira = miniaturasDe(titulo);
            v.b.push_back(b);
        }
        if (paginas > 1)
        {
            v.b.push_back({{60, ABAJO - 70, 260, 56}, "Anteriores", [pagina] { (*pagina)--; }, *pagina > 0});
            v.b.push_back({{340, ABAJO - 70, 260, 56}, "Siguientes", [pagina] { (*pagina)++; }, *pagina + 1 < paginas});
            v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + d, 0, paginas - 1); };
        }
        if (profundidad() == 1)
            v.atras = salir;
        v.teclas = {{"A", "Elegir"}, {"Y", "Guardar"}, {"+", "Salir"}};
        if (paginas > 1)
            v.teclas.insert(v.teclas.begin() + 1, {"L R", "Página"});
        avisoFijo(partida::hayCambios() ? "Hay cambios sin guardar." : "Al abrir se hizo una copia de seguridad de la partida.");
    };
}

void irA(Seccion s)
{
    g_seccion = s;
    raiz(seccion(s));
}

// La barra de la izquierda: el jugador, las secciones, guardar y salir.
Lateral lateral()
{
    Lateral l;
    const auto& j = partida::jugador();
    SDL_Texture* cara = foto::deJugador(partida::yo);
    l.entradas.push_back({j.nombre, cara ? cara : dibujos::menu("Onepiece"), j.nombre.substr(0, j.nombre.empty() ? 0 : (((unsigned char)j.nombre[0] & 0xE0) == 0xC0 ? 2 : 1)),
        g_seccion == SeccionJugador, [] {
            if (partida::jugadores.size() > 1 && g_seccion == SeccionJugador)
            {
                std::vector<std::string> nombres;
                for (const auto& x : partida::jugadores)
                    nombres.push_back(x.nombre);
                elegir("Jugadores de " + partida::isla, "Elige a quién cambiar el dinero, los bolsillos, las recetas…", nombres, partida::yo, [](int i) {
                    partida::yo = i;
                    avisar("Ahora cambias a " + partida::jugadores[i].nombre + ".");
                });
            }
            else
                irA(SeccionJugador);
        }});
    for (int s = 1; s < NUM_SECCIONES; s++)
        l.entradas.push_back({SECCIONES[s], dibujos::menu(ICONOS_SECCION[s]), "", g_seccion == s, [s] { irA((Seccion)s); }});
    l.hayCambios = partida::hayCambios();
    l.guardar = guardar;
    l.salir = salir;
    return l;
}

} // namespace

#ifndef VERSION
#define VERSION "simulador"
#endif

int main(int, char**)
{
    romfsInit();
    registro::empezar(VERSION);
    plInitialize(PlServiceType_User);
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // the screens are drawn a bit smaller: smooth
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("NHSE", 0, 0, W, H, 0);
    g_r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    PlFontData font;
    plGetSharedFontByType(&font, PlSharedFontType_Standard);
    // Las pantallas se hicieron con Arial Unicode (la del simulador). La letra de la Switch es más ancha: se abre algo
    // más pequeña, para que un texto ocupe lo mismo que con Arial Unicode y todo quepa igual.
    auto abrirLetra = [&font](int pt, bool negrita) {
        constexpr const char* MUESTRA = "Toca una cantidad para cambiarla. Bolsillos, Almacén y Vecinos: 1.234";
        constexpr int ARIAL_100 = 3158; // lo que mide MUESTRA con Arial Unicode de 100
        TTF_Font* f = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, pt);
        int w = 0;
        if (f && TTF_SizeUTF8(f, MUESTRA, &w, nullptr) == 0 && w > 0)
        {
            const int justo = (pt * ARIAL_100 * pt / 100 + w / 2) / w; // pt * (lo de Arial / lo de esta)
            if (justo != pt && justo > 8)
            {
                TTF_CloseFont(f);
                f = TTF_OpenFontRW(SDL_RWFromMem(font.address, font.size), 1, justo);
            }
            registro::apuntar("letra de %d: se abre de %d (mide %d, Arial %d)", pt, justo, w, ARIAL_100 * pt / 100);
        }
        if (f && negrita)
            TTF_SetFontStyle(f, TTF_STYLE_BOLD);
        medirLetra(f, pt);
        return f;
    };
    g_fuente = abrirLetra(28, false);
    g_pequena = abrirLetra(22, false);
    g_grande = abrirLetra(40, false);
    g_negrita = abrirLetra(26, true);
    g_titulo = abrirLetra(44, true);

    // Mientras se hace la copia de seguridad (unos segundos), un aviso.
    rect({0, 0, W, H}, CREMA);
    texto("Haciendo una copia de la partida en la SD…", W / 2, H / 2, MARRON, g_grande, Centro);
    SDL_RenderPresent(g_r);

    const std::string error = partida::abrir();
    if (error.empty())
        registro::apuntar("partida abierta: %d jugadores, isla %s", (int)partida::jugadores.size(), partida::isla.c_str());
    else
        registro::apuntar("no se abre la partida: %s", error.c_str());
    if (error.empty())
    {
#ifdef __SWITCH__
        // From the Album the app has much less memory: the map and the houses may not fit. Said at the start.
        if (appletGetAppletType() != AppletType_Application)
            avisar("Abierta desde el Álbum (poca memoria). Si el mapa o las casas fallan, ábrela manteniendo R al abrir un juego.");
#endif
        ponerLateral(lateral);
        bucle(seccion(SeccionJugador));
    }
    else
        bucle([error](Vista& v) {
            v.titulo = "No se puede abrir la partida";
            v.b.push_back({{490, 560, 300, 80}, "Salir", [] { terminar(); }});
            v.atras = [] { terminar(); };
            v.teclas = {{"A", "Salir"}};
            v.dibujo = [error] { texto(error, 100, 200, ROJO, g_grande, Izquierda, 1080); };
        });

    partida::cerrar();
    registro::apuntar("cerrando");
    TTF_CloseFont(g_fuente);
    TTF_CloseFont(g_pequena);
    TTF_CloseFont(g_grande);
    TTF_CloseFont(g_negrita);
    TTF_CloseFont(g_titulo);
    TTF_Quit();
    SDL_DestroyRenderer(g_r);
    g_r = nullptr; // las texturas que quedan guardadas (mapa, diseños...) ya no se tocan al salir
    SDL_DestroyWindow(win);
    SDL_Quit();
    plExit();
    romfsExit();
    registro::terminar();
    return 0;
}
