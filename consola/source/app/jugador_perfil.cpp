// Sección «Jugador»: el perfil del pasaporte, como ProfileWindow de NHSE para Mac: cumpleaños, fruta, fecha de
// registro y si fundó la isla.
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "jugador_comun.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int NUM_FRUTAS = sizeof(FRUTAS) / sizeof(FRUTAS[0]);

// Cambia algo del perfil y lo guarda como el Mac (todo el perfil a la vez).
void cambiar(std::function<void(Perfil&)> f)
{
    Perfil p = leerPerfil(partida::personal());
    f(p);
    escribirPerfil(partida::personal(), p);
    partida::cambiaPersonal();
    cambiado();
}

std::string fruta(uint16_t id)
{
    for (const auto& f : FRUTAS)
        if (f.id == id)
            return f.nombre;
    return "Sin elegir";
}

void cumpleanos()
{
    const Perfil p = leerPerfil(partida::personal());
    std::vector<std::string> meses(MESES, MESES + 12);
    elegir("Cumpleaños: el mes", "Ese día los vecinos lo celebran contigo.", meses, p.mes >= 1 && p.mes <= 12 ? p.mes - 1 : -1, [p](int m) {
        std::vector<std::string> dias;
        for (int d = 1; d <= diasDelMes(m + 1, 0); d++)
            dias.push_back(std::to_string(d));
        const int actual = p.mes == m + 1 && p.dia >= 1 && p.dia <= (int)dias.size() ? p.dia - 1 : -1;
        elegir(std::string("Cumpleaños: el día de ") + MESES[m], "", dias, actual, [m](int d) {
            cambiar([m, d](Perfil& x) { x.mes = (uint8_t)(m + 1), x.dia = (uint8_t)(d + 1); });
        }, 4);
    });
}

void perfil(Vista& v)
{
    const Perfil p = leerPerfil(partida::personal());
    v.titulo = "Perfil de " + partida::jugador().nombre;
    v.explicacion = "El pasaporte de " + partida::jugador().nombre + ", de la isla " + nombreIsla(partida::personal()) + ".";
    auto fila = [&](int i, const std::string& t, const std::string& d, std::function<void()> f, bool marcado = false) {
        Boton b{{100, 160 + i * 100, 1080, 86}, t, std::move(f), true, marcado};
        b.detalle = d;
        v.b.push_back(b);
    };
    fila(0, "Cumpleaños: " + jugador::diaYMes(p.dia, p.mes), "Ese día los vecinos lo celebran contigo", cumpleanos);
    fila(1, "Fruta favorita: " + fruta(p.fruta), "La fruta que aparece en el pasaporte", [p] {
        std::vector<std::string> nombres;
        int actual = -1;
        for (int i = 0; i < NUM_FRUTAS; i++)
        {
            nombres.push_back(FRUTAS[i].nombre);
            if (FRUTAS[i].id == p.fruta)
                actual = i;
        }
        elegir("Fruta favorita", "La fruta que aparece en el pasaporte.", nombres, actual, [](int i) {
            cambiar([i](Perfil& x) { x.fruta = FRUTAS[i].id; });
        });
    });
    fila(2, "Fecha de registro: " + textoFecha(p.registro), "El día en que le dieron el pasaporte", [p] {
        jugador::pedirFecha("Fecha de registro", p.registro, true, [](Fecha f) { cambiar([f](Perfil& x) { x.registro = f; }); });
    });
    fila(3, std::string("Fundó la isla: ") + jugador::siNo(p.fundador), "El representante de la isla (mejor no tocarlo)", [] {
        cambiar([](Perfil& x) { x.fundador = !x.fundador; });
    }, p.fundador);
    v.b.push_back({{440, ABAJO - 80, 400, 64}, "Volver", [] { volver(); }});
}

Registro r1({SeccionJugador, 80, [] { return std::string("Perfil"); },
    [] {
        const Perfil p = leerPerfil(partida::personal());
        return "Cumpleaños: " + jugador::diaYMes(p.dia, p.mes);
    },
    [] { abrir(perfil); }});

} // namespace
