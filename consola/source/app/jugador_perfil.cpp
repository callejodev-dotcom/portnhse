// Sección «Jugador»: el perfil del pasaporte, como ProfileWindow de NHSE para Mac: cumpleaños, fruta, fecha de
// registro y si fundó la isla.
#include <algorithm>
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "dibujos.hpp"
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

[[maybe_unused]] std::string fruta(uint16_t id)
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

// El día del cumpleaños, del mes que ya tiene (si no tiene mes, primero el mes).
void diaCumple()
{
    const Perfil p = leerPerfil(partida::personal());
    if (p.mes < 1 || p.mes > 12)
    {
        cumpleanos();
        return;
    }
    std::vector<std::string> dias;
    for (int d = 1; d <= diasDelMes(p.mes, 0); d++)
        dias.push_back(std::to_string(d));
    const int m = p.mes - 1;
    elegir(std::string("Cumpleaños: el día de ") + MESES[m], "", dias, p.dia >= 1 && p.dia <= (int)dias.size() ? p.dia - 1 : -1, [m](int d) {
        cambiar([m, d](Perfil& x) { x.mes = (uint8_t)(m + 1), x.dia = (uint8_t)(d + 1); });
    }, 4);
}

// Como ProfileWindow del Mac: arriba quién es, y una tarjeta para cada cosa del pasaporte.
void perfil(Vista& v)
{
    const Perfil p = leerPerfil(partida::personal());
    v.titulo = "Perfil";
    v.explicacion = "Lo que aparece en el pasaporte de " + partida::jugador().nombre + ".";
    const std::string nombre = partida::jugador().nombre, isla = "Isla " + nombreIsla(partida::personal());
    v.dibujo = [nombre, isla] {
        // Who it is.
        jugador::recuadro({60, 112, 570, 110});
        redondo({80, 126, 82, 82}, 14, {0x2B, 0x2B, 0x40, 255});
        jugador::imagen(dibujos::menu("Onepiece"), 89, 135, 64);
        texto(encajar(nombre, 430, g_titulo), 180, 128, MARRON, g_titulo);
        texto(encajar(isla, 430, g_pequena), 180, 178, MARRON_SUAVE, g_pequena);
        // The cards.
        panel({60, 236, 570, 168}, "Cumpleaños", "BdayCupcake", "Ese día los vecinos lo celebran contigo.");
        texto("de", 270, 359, MARRON_SUAVE, nullptr, Centro);
        panel({60, 418, 570, 168}, "Fecha de registro", "PlaneTicket", "El día en que le dieron el pasaporte.");
        panel({650, 112, 570, 286}, "Fruta favorita", "Apple", "La fruta que aparece en el pasaporte.");
        panel({650, 412, 570, 174}, "Opciones avanzadas", "ToolChangeStick", "Mejor no tocarlo.");
    };
    // Birthday: the day and the month.
    Boton dia{{148, 332, 100, 54}, p.dia >= 1 ? std::to_string(p.dia) : "—", diaCumple};
    Boton mes{{292, 332, 314, 54}, p.mes >= 1 && p.mes <= 12 ? MESES[p.mes - 1] : "Sin elegir", cumpleanos};
    v.b.push_back(dia);
    v.b.push_back(mes);
    // Registration date.
    Boton reg{{148, 514, 300, 54}, textoFecha(p.registro), [p] {
        jugador::pedirFecha("Fecha de registro", p.registro, true, [](Fecha f) { cambiar([f](Perfil& x) { x.registro = f; }); });
    }};
    reg.imagen = dibujos::menu("Timer");
    v.b.push_back(reg);
    // The fruits, with their picture; the chosen one in blue.
    for (int i = 0; i < NUM_FRUTAS; i++)
    {
        Boton f{{738 + (i % 2) * 238, 200 + (i / 2) * 64, 226, 56}, FRUTAS[i].nombre, [i] { cambiar([i](Perfil& x) { x.fruta = FRUTAS[i].id; }); }, true, FRUTAS[i].id == p.fruta};
        f.imagen = dibujos::de(FRUTAS[i].id);
        v.b.push_back(f);
    }
    // Founder.
    v.b.push_back(jugador::interruptor({738, 504, 458, 56}, "Fundó la isla (la representa)", p.fundador, [] {
        cambiar([](Perfil& x) { x.fundador = !x.fundador; });
    }));
    v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
}

Registro r1({SeccionJugador, 80, [] { return std::string("Perfil"); },
    [] {
        const Perfil p = leerPerfil(partida::personal());
        return "Cumpleaños: " + jugador::diaYMes(p.dia, p.mes);
    },
    [] { abrir(perfil); }});

} // namespace
