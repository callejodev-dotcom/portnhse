// Sección «Jugador»: dinero, bolsillos, almacén, papelera y aprender todo (recetas, catálogo, reacciones). Las demás
// tarjetas de la sección están en jugador_*.cpp (colecciones, récords, correo, perfil).
#include "../core/jugador.hpp"
#include <cstring>
#include <functional>
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
    v.explicacion = "Toca un número para cambiarlo.";
    struct Cifra {
        const char* nombre;
        const char* titulo;
        uint32_t maximo;
        const char* unidad;
        std::function<uint8_t*()> donde;
        std::function<void()> cambia;
    };
    auto pers = [](uint32_t pos) { return [pos] { return partida::personal().data() + pos; }; };
    auto cambiaPers = [] { partida::cambiaPersonal(); };
    std::vector<Cifra> cifras = {
        {"Monedero", "Bayas en el monedero (máx. 99.999)", 99999, " bayas", pers(MONEDERO), cambiaPers},
        {"Banco", "Bayas en el banco (máx. 999.999.999)", 999999999, " bayas", pers(BANCO), cambiaPers},
        {"Millas Nook", "Millas Nook", 999999999, "", pers(MILLAS), cambiaPers},
    };
    // Los billetes del hotel y las Poki, solo si la partida ya los tiene (como el Mac).
    if (hayHotel(partida::personal()))
        cifras.push_back({"Billetes del hotel", "Billetes del hotel", 999999999, "", pers(BILLETES_HOTEL), cambiaPers});
    if (dondeEstan())
        cifras.push_back({"Poki", "Poki", 999999999, "", [] { return dondeEstan()->datos.data() + POKI; },
            [] { partida::cambiaOtro(partida::jugador().carpeta + "/wherearen.dat"); }});
    for (int i = 0; i < (int)cifras.size(); i++)
    {
        const Cifra c = cifras[i];
        v.b.push_back({{60 + (i % 2) * 590, 160 + (i / 2) * 120, 570, 100}, std::string(c.nombre) + ": " + numero(leerNumero(c.donde())) + c.unidad, [c] {
            uint32_t n = leerNumero(c.donde());
            if (pedirNumero(c.titulo, n, c.maximo))
            {
                escribirNumero(c.donde(), n);
                c.cambia();
                cambiado();
            }
        }});
    }
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
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
    v.b.push_back({{60, 170, 1160, 100}, "Jugador: " + partida::jugador().nombre, [] { renombrar(false); }});
    v.b.push_back({{60, 290, 1160, 100}, "Isla: " + partida::isla, [] { renombrar(true); }});
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
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
    v.titulo = "Aprender todo: " + partida::jugador().nombre;
    v.explicacion = "Lo que ya tiene se queda como está; lo nuevo sale como nuevo en el juego.";
    const Progreso r = recetas(partida::personal()), c = catalogo(partida::personal()), e = reacciones(partida::personal());
    auto fila = [&](int i, const std::string& t, Progreso p, int (*hacer)(std::vector<uint8_t>&), const char* cosa) {
        const std::string que = cosa;
        Boton bt{{60, 160 + i * 120, 1160, 100}, t, [hacer, que] {
            const int n = hacer(partida::personal());
            partida::cambiaPersonal();
            cambiado(std::to_string(n) + " " + que + " nuevas");
        }, p.tiene < p.total};
        bt.detalle = p.tiene < p.total ? "Tiene " + numero(p.tiene) + " de " + numero(p.total) : "Ya lo tiene todo (" + numero(p.total) + ")";
        v.b.push_back(bt);
    };
    fila(0, "Aprender todas las recetas de bricolaje", r, aprenderRecetas, "recetas");
    fila(1, "Completar el catálogo (con todos los colores)", c, completarCatalogo, "cosas");
    fila(2, "Aprender todas las reacciones", e, aprenderReacciones, "reacciones");
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
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
