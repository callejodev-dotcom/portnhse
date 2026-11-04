// Sección «Jugador»: dinero, bolsillos, almacén, papelera y aprender todo (recetas, catálogo, reacciones). Las demás
// tarjetas de la sección están en jugador_*.cpp (colecciones, récords, correo, perfil).
#include "../core/jugador.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

void dinero(Vista& v)
{
    v.titulo = "Dinero y millas de " + partida::jugador().nombre;
    v.explicacion = "Toca un número para cambiarlo.";
    const uint32_t pos[4] = {MONEDERO, BANCO, MILLAS, BILLETES_HOTEL};
    const char* nombres[4] = {"Monedero", "Banco", "Millas Nook", "Billetes del hotel"};
    const char* titulos[4] = {"Bayas en el monedero (máx. 99.999)", "Bayas en el banco (máx. 999.999.999)", "Millas Nook", "Billetes del hotel"};
    const uint32_t maximos[4] = {99999, 999999999, 999999999, 999999999};
    // Los billetes del hotel, solo si la partida ya tiene el hotel (como el Mac).
    const int filas = hayHotel(partida::personal()) ? 4 : 3;
    for (int i = 0; i < filas; i++)
    {
        const uint32_t p = pos[i], maximo = maximos[i];
        const std::string t = titulos[i];
        v.b.push_back({{100, 160 + i * 105, 1080, 90}, std::string(nombres[i]) + ": " + numero(leerNumero(partida::personal().data() + p)) + (i < 2 ? " bayas" : ""), [p, maximo, t] {
            uint32_t n = leerNumero(partida::personal().data() + p);
            if (pedirNumero(t, n, maximo))
            {
                escribirNumero(partida::personal().data() + p, n);
                partida::cambiaPersonal();
                cambiado();
            }
        }});
    }
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
        Boton bt{{100, 160 + i * 120, 1080, 100}, t, [hacer, que] {
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

} // namespace
