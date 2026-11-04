// Sección «Jugador»: dinero, bolsillos, almacén, papelera y aprender todo (recetas, catálogo, reacciones). Las demás
// tarjetas de la sección están en jugador_*.cpp (colecciones, récords, correo, perfil).
#include "../core/jugador.hpp"
#include <cstring>
#include <functional>
#include "dibujos.hpp"
#include "jugador_comun.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

// Las Poki van en wherearen.dat (no lo tienen las partidas muy viejas).
acnh::Archivo* dondeEstan() { return partida::otro(partida::jugador().carpeta + "/wherearen.dat"); }

void dinero(Vista& v)
{
    v.titulo = "Dinero y millas de " + partida::jugador().nombre;
    v.explicacion = "Toca una cantidad para cambiarla.";
    struct Cifra {
        const char* nombre;
        const char* icono;
        std::string nota;
        const char* titulo;
        uint32_t maximo;
        std::function<uint8_t*()> donde;
        std::function<void()> cambia;
    };
    auto pers = [](uint32_t pos) { return [pos] { return partida::personal().data() + pos; }; };
    auto cambiaPers = [] { partida::cambiaPersonal(); };
    // As the money of NHSE for Mac: the same cards, icons and notes.
    std::vector<Cifra> cifras = {
        {"Monedero", "MoneyBag039", "Como mucho 99.999", "Bayas en el monedero (máx. 99.999)", 99999, pers(MONEDERO), cambiaPers},
        {"Ahorros", "MoneyBag069", "Las bayas del cajero", "Bayas en el banco (máx. 999.999.999)", 999999999, pers(BANCO), cambiaPers},
        {"Millas Nook", "Coin", numero(leerNumero(partida::personal().data() + MILLAS + 8)) + " ganadas en total", "Millas Nook", 999999999, pers(MILLAS), cambiaPers},
    };
    // Los billetes del hotel y las Poki, solo si la partida ya los tiene (como el Mac).
    if (dondeEstan())
        cifras.push_back({"Poki", "Coin", "Moneda del hotel", "Poki", 999999999, [] { return dondeEstan()->datos.data() + POKI; },
            [] { partida::cambiaOtro(partida::jugador().carpeta + "/wherearen.dat"); }});
    if (hayHotel(partida::personal()))
        cifras.push_back({"Billetes del hotel", "RollanTicket", "Para gastar en el hotel", "Billetes del hotel", 999999999, pers(BILLETES_HOTEL), cambiaPers});
    for (int i = 0; i < (int)cifras.size(); i++)
    {
        const Cifra c = cifras[i];
        Boton b{{60 + (i % 3) * 390, 130 + (i / 3) * 150, 375, 136}, c.nombre, [c] {
            uint32_t n = leerNumero(c.donde());
            if (pedirNumero(c.titulo, n, c.maximo))
            {
                escribirNumero(c.donde(), n);
                c.cambia();
                cambiado();
            }
        }};
        b.grande = true;
        b.valor = numero(leerNumero(c.donde()));
        b.detalle = c.nota;
        b.imagen = dibujos::menu(c.icono);
        v.b.push_back(b);
    }
    v.b.push_back({{W / 2 - 200, ABAJO - 80, 400, 64}, "Volver", [] { volver(); }});
}

// Cambiar el nombre del jugador o de la isla en todas sus copias (como el Mac).
void renombrar(bool isla)
{
    auto& p = partida::personal();
    const uint32_t pos = isla ? ID_ISLA : ID_PERSONA;
    const std::string actual = leerTexto(p.data() + pos + 4, 10);
    std::string nuevo = actual;
    if (!pedirTexto(isla ? "Nombre de la isla" : "Nombre de " + actual, "Hasta 10 letras", nuevo, 10) || nuevo.empty() || nuevo == actual)
        return;
    if (largoTexto(nuevo) > 10)
    {
        avisar("Como mucho 10 letras.", true);
        return;
    }
    uint8_t antes[TAM_IDENTIDAD], despues[TAM_IDENTIDAD];
    memcpy(antes, p.data() + pos, TAM_IDENTIDAD);
    escribirTexto(p.data() + pos + 4, nuevo, 10);
    memcpy(despues, p.data() + pos, TAM_IDENTIDAD);
    partida::cambiaPersonal();
    int copias = 1;
    if (int n = reemplazarTodo(partida::principal(), antes, despues, TAM_IDENTIDAD))
        partida::cambiaPrincipal(), copias += n;
    const char* const OTROS[] = {"postbox.dat", "photo_studio_island.dat", "profile.dat", "wherearen.dat"};
    for (int j = 0; j < (int)partida::jugadores.size(); j++)
    {
        if (int n = reemplazarTodo(partida::personal(j), antes, despues, TAM_IDENTIDAD))
            partida::cambiaPersonal(j), copias += n;
        for (const char* f : OTROS)
        {
            const std::string ruta = partida::jugadores[j].carpeta + "/" + f;
            if (acnh::Archivo* o = partida::otro(ruta))
                if (int n = reemplazarTodo(o->datos, antes, despues, TAM_IDENTIDAD))
                    partida::cambiaOtro(ruta), copias += n;
        }
        partida::jugadores[j].nombre = nombreJugador(partida::personal(j));
    }
    partida::isla = nombreIsla(partida::personal(0));
    cambiado((isla ? "La isla se llama ahora " : "Ahora se llama ") + nuevo + " (" + std::to_string(copias) + " sitios)");
}

void nombres(Vista& v)
{
    v.titulo = "Nombres";
    v.explicacion = "El nombre se cambia en todas partes: cartas, recuerdos de los vecinos, fotos…";
    const std::string jugador = partida::jugador().nombre, isla = partida::isla;
    v.dibujo = [jugador, isla] {
        auto tarjeta = [](int x, const char* titulo, const char* icono, const std::string& nombre, const char* nota) {
            const int y = panel({x, 112, 570, 300}, titulo, icono, nota);
            texto(encajar(nombre, 520, g_titulo), x + 24, y + 16, MARRON, g_titulo);
            texto("Hasta 10 letras", x + 24, y + 70, MARRON_SUAVE, g_pequena);
        };
        tarjeta(60, "Jugador", "Onepiece", jugador, "Como te llaman los vecinos.");
        tarjeta(650, "Isla", "PltPalm", isla, "La isla de todos los jugadores.");
    };
    Boton a{{84, 330, 300, 60}, "Cambiar el nombre", [] { renombrar(false); }};
    a.principal = true;
    Boton b{{674, 330, 300, 60}, "Cambiar el nombre", [] { renombrar(true); }};
    b.principal = true;
    v.b.push_back(a);
    v.b.push_back(b);
    v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
}

objetos::Huecos bolsillos()
{
    objetos::Huecos h;
    h.nombre = "Bolsillos de " + partida::jugador().nombre;
    h.cuantos = [] { return NUM_BOLSILLOS; };
    h.leer = [](int i) { return leerBolsillo(partida::personal(), i); };
    h.escribir = [](int i, const Objeto& o) {
        escribirBolsillo(partida::personal(), i, o);
        partida::cambiaPersonal();
    };
    h.abierto = [](int i) { return bolsilloAbierto(partida::personal(), i); };
    return h;
}

// El almacén de casa: solo los huecos que ha abierto el jugador (como el Mac), y de ellos solo los usados.
objetos::Huecos almacen()
{
    objetos::Huecos h;
    h.nombre = "Almacén de " + partida::jugador().nombre;
    h.cuantos = [] { return almacenAbiertos(partida::personal()); };
    h.leer = [](int i) { return leerAlmacen(partida::personal(), i); };
    h.escribir = [](int i, const Objeto& o) {
        escribirAlmacen(partida::personal(), i, o);
        partida::cambiaPersonal();
    };
    return h;
}

// La papelera de reciclaje (de la isla, en main.dat).
objetos::Huecos papelera()
{
    objetos::Huecos h;
    h.nombre = "Papelera de reciclaje";
    h.cuantos = [] { return NUM_PAPELERA; };
    h.leer = [](int i) { return leerPapelera(partida::principal(), i); };
    h.escribir = [](int i, const Objeto& o) {
        escribirPapelera(partida::principal(), i, o);
        partida::cambiaPrincipal();
    };
    return h;
}

void aprenderTodo(Vista& v)
{
    v.titulo = "Aprender todo";
    v.explicacion = "Lo que ya tiene se queda como está; lo nuevo sale como nuevo en el juego.";
    const Progreso r = recetas(partida::personal()), c = catalogo(partida::personal()), e = reacciones(partida::personal());
    struct Parte {
        const char* titulo;
        const char* icono;
        const char* boton;
        Progreso p;
        int (*hacer)(std::vector<uint8_t>&);
        const char* cosa;
    };
    const Parte partes[3] = {
        {"Recetas de bricolaje", "BookRecipe", "Aprender todas", r, aprenderRecetas, "recetas"},
        {"Catálogo (con todos los colores)", "Book", "Completarlo", c, completarCatalogo, "cosas"},
        {"Reacciones", "Love", "Aprender todas", e, aprenderReacciones, "reacciones"},
    };
    v.dibujo = [partes] {
        for (int i = 0; i < 3; i++)
        {
            const Parte& p = partes[i];
            const int y0 = 112 + i * 162;
            const std::string nota = p.p.tiene < p.p.total ? "Tiene " + numero(p.p.tiene) + " de " + numero(p.p.total)
                                                             : "Ya lo tiene todo (" + numero(p.p.total) + ")";
            const int y = panel({60, y0, 1160, 148}, p.titulo, p.icono, nota);
            // How far it is, as a bar.
            const int ancho = 760, lleno = p.p.total ? (int)((long)ancho * p.p.tiene / p.p.total) : 0;
            redondo({148, y + 8, ancho, 16}, 8, jugador::GRIS_CLARO);
            if (lleno > 0)
                redondo({148, y + 8, std::max(lleno, 16), 16}, 8, p.p.tiene == p.p.total ? VERDE : AZUL);
        }
    };
    for (int i = 0; i < 3; i++)
    {
        const Parte p = partes[i];
        const std::string que = p.cosa;
        Boton b{{956, 112 + i * 162 + 44, 240, 60}, p.boton, [p, que] {
            const int n = p.hacer(partida::personal());
            partida::cambiaPersonal();
            cambiado(std::to_string(n) + " " + que + " nuevas");
        }, p.p.tiene < p.p.total};
        b.principal = p.p.tiene < p.p.total;
        v.b.push_back(b);
    }
    v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
}

Registro r1({SeccionJugador, 10, [] { return std::string("Dinero y millas"); },
    [] { return numero(leerNumero(partida::personal().data() + MONEDERO)) + " bayas en el monedero"; },
    [] { abrir(dinero); }});

Registro r2({SeccionJugador, 20, [] { return std::string("Bolsillos"); },
    [] {
        int libres = 0;
        for (int i = 0; i < NUM_BOLSILLOS; i++)
            libres += bolsilloAbierto(partida::personal(), i) && leerBolsillo(partida::personal(), i).vacio();
        return libres ? std::to_string(libres) + " huecos libres" : std::string("Llenos");
    },
    [] { abrir(objetos::todos(bolsillos())); }});

Registro r4({SeccionJugador, 30, [] { return std::string("Almacén de casa"); },
    [] {
        const auto& p = partida::personal();
        const int abiertos = almacenAbiertos(p);
        int usados = 0;
        for (int i = 0; i < abiertos; i++)
            usados += !leerAlmacen(p, i).vacio();
        return numero(usados) + " de " + numero(abiertos) + " huecos usados";
    },
    [] { abrir(objetos::usados(almacen())); }});

Registro r5({SeccionJugador, 35, [] { return std::string("Papelera de reciclaje"); },
    [] {
        int n = 0;
        for (int i = 0; i < NUM_PAPELERA; i++)
            n += !leerPapelera(partida::principal(), i).vacio();
        return n ? numero(n) + (n == 1 ? " objeto" : " objetos") + " (de la isla)" : std::string("Vacía");
    },
    [] { abrir(objetos::todos(papelera())); }});

Registro r3({SeccionJugador, 90, [] { return std::string("Aprender todo"); },
    [] { return std::string("Recetas, catálogo y reacciones de golpe"); },
    [] { abrir(aprenderTodo); }});

Registro rNombres({SeccionJugador, 15, [] { return std::string("Nombres"); },
    [] { return partida::jugador().nombre + " de " + partida::isla; },
    [] { abrir(nombres); }});

} // namespace
