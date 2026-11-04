// Órdenes de prueba de la isla (sin el mapa ni las casas): hemisferio, aeródromo, fruta y flores, zona de acampada,
// visitantes, tendencia de los nabos, museo y tablón. Todas empiezan por «isla-».
#include "prueba.hpp"
#include "../source/core/isla.hpp"
#include <cstdio>
#include <cstdlib>

using namespace acnh;

namespace {

int num(const Contexto& c, size_t i) { return (int)strtol(c.arg(i).c_str(), nullptr, 0); }

// «2026-05-03»
Fecha fecha(const std::string& s)
{
    Fecha f;
    if (sscanf(s.c_str(), "%d-%d-%d", &f.anio, &f.mes, &f.dia) != 3)
        return hoy();
    return f;
}

OrdenPrueba estado("isla-estado", [](Contexto& c) {
    const auto& m = c.main.datos;
    const FrutasFlores f = leerFrutasFlores(m, c.pers());
    printf("hemisferio %d, aeropuerto %d\n", hemisferio(m), aeropuerto(m));
    printf("fruta %d %d, flor %d %d\n", f.fruta, f.frutaHermana, f.flor, f.florHermana);
    printf("camping %d %s %s\n", estadoCamping(m), campista(m).c_str(), textoFecha(fechaCamping(m)).c_str());
    for (int d = 0; d < 7; d++)
        printf("dia %d: %s%s%s\n", d, VISITANTES[visitante(m, d)], concierto(m, d, false) ? " +concierto" : "", concierto(m, d, true) ? " +cumple" : "");
    printf("nabos: %s, pico %u\n", TENDENCIAS[tendenciaNabos(m) % 4], picoNabos(m));
    int n = 0;
    for (auto& d : donaciones(m))
        n += d.donada();
    printf("museo %d de %d\n", n, NUM_PIEZAS);
    for (auto& t : notas(m))
        printf("nota %d (%s, %s%s): %s\n", t.hueco, textoFecha(t.fecha).c_str(), t.autor.c_str(), t.aMano ? ", a mano" : "", t.texto.c_str());
    c.guardar = false;
    return 0;
});

OrdenPrueba hemi("isla-hemisferio", [](Contexto& c) {
    ponerHemisferio(c.main.datos, num(c, 0));
    return 0;
});

OrdenPrueba aero("isla-aeropuerto", [](Contexto& c) {
    ponerAeropuerto(c.main.datos, num(c, 0));
    return 0;
});

// isla-frutas <fruta> <segunda fruta> <flor> <segunda flor> <jugador>
OrdenPrueba frutas("isla-frutas", [](Contexto& c) {
    FrutasFlores f;
    f.fruta = (uint16_t)num(c, 0);
    f.frutaHermana = (uint16_t)num(c, 1);
    f.flor = (uint8_t)num(c, 2);
    f.florHermana = (uint8_t)num(c, 3);
    std::vector<std::vector<uint8_t>*> pers;
    for (auto& j : c.jugadores)
        pers.push_back(&j.datos);
    guardarFrutasFlores(c.main.datos, pers, num(c, 4), f);
    return 0;
});

// isla-camping <interno|nadie> [aaaa-mm-dd]
OrdenPrueba camping("isla-camping", [](Contexto& c) {
    const std::string quien = c.arg(0) == "nadie" ? "" : c.arg(0);
    if (!guardarCamping(c.main.datos, quien, c.args.size() > 1 ? fecha(c.arg(1)) : hoy()))
    {
        printf("no se puede\n");
        return 1;
    }
    printf("campista «%s»\n", campista(c.main.datos).c_str());
    return 0;
});

// isla-visitante <día 0-6> <quién 0-11>
OrdenPrueba visit("isla-visitante", [](Contexto& c) {
    ponerVisitante(c.main.datos, num(c, 0), num(c, 1));
    return 0;
});

// isla-concierto <día> <normal|cumple> <0|1>
OrdenPrueba conc("isla-concierto", [](Contexto& c) {
    ponerConcierto(c.main.datos, num(c, 0), c.arg(1) == "cumple", num(c, 2) != 0);
    return 0;
});

// isla-tendencia <0-3> <medio día del pico>
OrdenPrueba tend("isla-tendencia", [](Contexto& c) {
    ponerTendenciaNabos(c.main.datos, (uint32_t)num(c, 0));
    ponerPicoNabos(c.main.datos, (uint32_t)num(c, 1));
    return 0;
});

// isla-museo-lista: las piezas en el orden de la app (para compararlas con NHSE para Mac).
OrdenPrueba museoLista("isla-museo-lista", [](Contexto& c) {
    for (int i = 0; i < NUM_PIEZAS; i++)
        printf("pieza %d %d\n", PIEZAS_MUSEO[i].sala, PIEZAS_MUSEO[i].id);
    c.guardar = false;
    return 0;
});

// isla-donar <objeto> <jugador> [aaaa-mm-dd]
OrdenPrueba don("isla-donar", [](Contexto& c) {
    printf("donado %d\n", donar(c.main.datos, (uint16_t)num(c, 0), num(c, 1), c.args.size() > 2 ? fecha(c.arg(2)) : hoy()));
    return 0;
});

// isla-retirar <objeto>
OrdenPrueba ret("isla-retirar", [](Contexto& c) {
    printf("retirado %d\n", retirar(c.main.datos, (uint16_t)num(c, 0)));
    return 0;
});

// isla-donar-sala <sala 0-4 o «todo»> <jugador>: lo que falta, en el orden de la app (como «Donar todos»).
OrdenPrueba donSala("isla-donar-sala", [](Contexto& c) {
    const bool todo = c.arg(0) == "todo";
    const int sala = todo ? 0 : num(c, 0);
    int n = 0;
    const Fecha f = hoy();
    for (int i = todo ? 0 : INICIO_SALAS[sala]; i < (todo ? NUM_PIEZAS : INICIO_SALAS[sala + 1]); i++)
        n += donar(c.main.datos, PIEZAS_MUSEO[i].id, num(c, 1), f);
    printf("donadas %d\n", n);
    return 0;
});

// isla-nota <hueco> <texto> («\n» en el texto = otra línea)
OrdenPrueba not_("isla-nota", [](Contexto& c) {
    std::string t = c.arg(1);
    for (size_t p; (p = t.find("\\n")) != std::string::npos;)
        t.replace(p, 2, "\n");
    escribirNota(c.main.datos, num(c, 0), t);
    return 0;
});

// isla-firma <hueco> <firma>
OrdenPrueba fir("isla-firma", [](Contexto& c) {
    escribirFirma(c.main.datos, num(c, 0), c.arg(1));
    return 0;
});

} // namespace
