// Sección «Isla»: flores, malas hierbas y nabos.
#include <algorithm>
#include <ctime>
#include <memory>
#include "../core/tablas.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int NUM_FLORES = sizeof(FLORES_ABIERTAS) / sizeof(FLORES_ABIERTAS[0]);

Pantalla flores()
{
    struct Estado {
        std::vector<uint16_t> elegidas;
        bool alAzar = false;
    };
    auto e = std::make_shared<Estado>();
    Lista l;
    l.titulo = "Flores";
    l.explicacion = [] { return "Elige una o varias: todas las flores de la isla (" + std::to_string(contar(partida::principal()).flores) + ") se cambian por esas."; };
    l.cuantas = [] { return NUM_FLORES; };
    l.porColumna = 6;
    l.fila = [e](int i) {
        const auto& f = FLORES_ABIERTAS[i];
        const bool marcada = std::find(e->elegidas.begin(), e->elegidas.end(), f.id) != e->elegidas.end();
        return Fila{std::string(marcada ? "● " : "") + f.nombre, [e, id = f.id] {
            auto it = std::find(e->elegidas.begin(), e->elegidas.end(), id);
            if (it == e->elegidas.end())
                e->elegidas.push_back(id);
            else
                e->elegidas.erase(it);
        }, true, marcada};
    };
    l.abajo.push_back([e] { return Fila{e->alAzar ? "Mezcla: al azar" : "Mezcla: alternando", [e] { e->alAzar = !e->alAzar; }}; });
    l.abajo.push_back([e] {
        return Fila{e->elegidas.empty() ? "Cambiar" : "Cambiar (" + std::to_string(e->elegidas.size()) + ")", [e] {
            const int n = cambiarFlores(partida::principal(), e->elegidas, e->alAzar, (uint32_t)time(nullptr));
            partida::cambiaPrincipal();
            volver();
            cambiado("Cambiadas " + std::to_string(n) + " flores");
        }, !e->elegidas.empty()};
    });
    l.extra = [](Vista& v) { v.teclas = {{"A", "Marcar"}, {"L R", "Página"}, {"B", "Volver"}}; };
    return lista(l);
}

const char* const DIAS[] = {"Domingo", "Domingo", "Lunes", "Lunes", "Martes", "Martes", "Miércoles", "Miércoles",
    "Jueves", "Jueves", "Viernes", "Viernes", "Sábado", "Sábado"};

void nabos(Vista& v)
{
    v.titulo = "Nabos";
    v.explicacion = "Precios de esta semana. Toca uno para cambiarlo.";
    const Nabos n = leerNabos(partida::principal());
    auto cambiar = [](int i) {
        Nabos m = leerNabos(partida::principal());
        uint32_t valor = i < 0 ? m.compra : m.venta[i];
        const std::string t = i < 0 ? std::string("Precio de compra (domingo)") : std::string(DIAS[i]) + (i % 2 ? " por la tarde" : " por la mañana");
        if (!pedirNumero(t, valor, 999))
            return;
        (i < 0 ? m.compra : m.venta[i]) = valor;
        escribirNabos(partida::principal(), m);
        partida::cambiaPrincipal();
        cambiado();
    };
    v.b.push_back({{60, 145, 1160, 50}, "Daisy Mae los vende a " + std::to_string(n.compra) + " bayas", [cambiar] { cambiar(-1); }});
    for (int i = 2; i < 14; i++)
        v.b.push_back({{60 + (i % 2) * 590, 208 + (i / 2 - 1) * 58, 570, 50},
            std::string(DIAS[i]) + (i % 2 ? " tarde: " : " mañana: ") + std::to_string(n.venta[i]), [cambiar, i] { cambiar(i); }});
    v.b.push_back({{60, 570, 570, 56}, "Poner todos a…", [] {
        uint32_t valor = 600;
        if (!pedirNumero("Precio de venta para toda la semana", valor, 999))
            return;
        Nabos m = leerNabos(partida::principal());
        for (int i = 2; i < 14; i++)
            m.venta[i] = valor;
        escribirNabos(partida::principal(), m);
        partida::cambiaPrincipal();
        cambiado("Toda la semana a " + std::to_string(valor));
    }});
    v.b.push_back({{650, 570, 570, 56}, "Volver", [] { volver(); }});
}

Registro r1({SeccionIsla, 20, [] { return std::string("Flores"); },
    [] { return std::to_string(contar(partida::principal()).flores) + " en la isla"; },
    [] { abrir(flores()); }, [] { return contar(partida::principal()).flores > 0; }});

Registro r2({SeccionIsla, 30, [] { return std::string("Quitar malas hierbas"); },
    [] {
        const int n = contar(partida::principal()).malasHierbas;
        return n ? std::to_string(n) + " en la isla" : std::string("No hay ninguna");
    },
    [] {
        const int n = quitarMalasHierbas(partida::principal());
        partida::cambiaPrincipal();
        cambiado("Quitadas " + std::to_string(n) + " malas hierbas");
    },
    [] { return contar(partida::principal()).malasHierbas > 0; }});

Registro r3({SeccionIsla, 60, [] { return std::string("Nabos"); },
    [] {
        const Nabos n = leerNabos(partida::principal());
        return n.compra ? "Se compran a " + std::to_string(n.compra) + " bayas" : std::string("Precios de la semana");
    },
    [] { abrir(nabos); }});

} // namespace
