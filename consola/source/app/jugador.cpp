// Sección «Jugador»: dinero, bolsillos y aprender todo (recetas, catálogo, reacciones).
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
    const uint32_t pos[3] = {MONEDERO, BANCO, MILLAS};
    const char* nombres[3] = {"Monedero", "Banco", "Millas Nook"};
    const char* titulos[3] = {"Bayas en el monedero (máx. 99.999)", "Bayas en el banco (máx. 999.999.999)", "Millas Nook"};
    const uint32_t maximos[3] = {99999, 999999999, 999999999};
    for (int i = 0; i < 3; i++)
    {
        const uint32_t p = pos[i], maximo = maximos[i];
        const std::string t = titulos[i];
        v.b.push_back({{100, 170 + i * 120, 1080, 100}, std::string(nombres[i]) + ": " + numero(leerNumero(partida::personal().data() + p)) + (i < 2 ? " bayas" : ""), [p, maximo, t] {
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

Registro r3({SeccionJugador, 90, [] { return std::string("Aprender todo"); },
    [] { return std::string("Recetas, catálogo y reacciones de golpe"); },
    [] { abrir(aprenderTodo); }});

} // namespace
