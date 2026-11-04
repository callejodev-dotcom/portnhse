#include "jugador_comun.hpp"
#include <algorithm>
#include <ctime>
#include <memory>

using namespace acnh;
using namespace ui;

namespace jugador {

Fecha hoy()
{
    const time_t t = time(nullptr);
    const tm* l = localtime(&t);
    Fecha f;
    f.anio = (uint16_t)(l->tm_year + 1900);
    f.mes = (uint8_t)(l->tm_mon + 1);
    f.dia = (uint8_t)l->tm_mday;
    return f;
}

std::string diaYMes(int dia, int mes)
{
    if (mes < 1 || mes > 12 || dia < 1)
        return "Sin elegir";
    return std::to_string(dia) + " de " + MESES[mes - 1];
}

void pedirFecha(const std::string& titulo, Fecha actual, bool sinFecha, std::function<void(Fecha)> elegida)
{
    auto f = std::make_shared<Fecha>(actual.valida() ? actual : hoy());
    abrir([titulo, f, sinFecha, elegida](Vista& v) {
        v.titulo = titulo;
        v.explicacion = "Toca el día, el mes o el año para cambiarlo.";
        Fecha& x = *f;
        x.dia = (uint8_t)std::min<int>(x.dia, diasDelMes(x.mes, x.anio));
        Boton dia{{100, 180, 340, 110}, std::to_string(x.dia), [f] {
            uint32_t n = f->dia;
            if (pedirNumero("Día", n, diasDelMes(f->mes, f->anio), 1))
                f->dia = (uint8_t)n;
        }};
        dia.detalle = "Día";
        Boton mes{{470, 180, 340, 110}, MESES[x.mes - 1], [f] {
            std::vector<std::string> meses(MESES, MESES + 12);
            elegir("Mes", "", meses, f->mes - 1, [f](int i) { f->mes = (uint8_t)(i + 1); });
        }};
        mes.detalle = "Mes";
        Boton anio{{840, 180, 340, 110}, std::to_string(x.anio), [f] {
            uint32_t n = f->anio;
            if (pedirNumero("Año", n, 2099, 2000))
                f->anio = (uint16_t)n;
        }};
        anio.detalle = "Año";
        v.b.push_back(dia);
        v.b.push_back(mes);
        v.b.push_back(anio);
        v.b.push_back({{100, 340, 520, 80}, "Hoy", [f] { *f = hoy(); }});
        if (sinFecha)
            v.b.push_back({{660, 340, 520, 80}, "Sin fecha", [elegida] {
                volver();
                elegida(Fecha{});
            }});
        v.b.push_back({{100, ABAJO - 90, 520, 70}, "Aceptar", [f, elegida] {
            const Fecha r = *f;
            volver();
            elegida(r);
        }});
        v.b.push_back({{660, ABAJO - 90, 520, 70}, "Volver", [] { volver(); }});
    });
}

} // namespace jugador
