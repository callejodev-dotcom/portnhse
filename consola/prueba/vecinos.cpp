// Órdenes de prueba de la ficha de los vecinos: personalidad, muletilla, mudanza, amistad, ropa y muebles, paredes y
// suelo, casa, bricolaje, y exportar e importar vecinos y casas.
#include "prueba.hpp"
#include "../source/core/vecino.hpp"
#include <cstdio>
#include <cstdlib>

using namespace acnh;

namespace {

int num(const std::string& s) { return (int)strtol(s.c_str(), nullptr, 0); }

bool escribir(const std::string& ruta, const std::vector<uint8_t>& d)
{
    FILE* f = fopen(ruta.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = fwrite(d.data(), 1, d.size(), f) == d.size();
    fclose(f);
    return ok;
}

// vecino-ficha <hueco>: lo que enseña la ficha
OrdenPrueba ficha("vecino-ficha", [](Contexto& c) {
    const int h = num(c.arg(0));
    const auto& m = c.main.datos;
    printf("vecino %s, personalidad %d, frase «%s», original «%s», se muda %d\n", vecinoEn(m, h).c_str(), personalidad(m, h),
        frase(m, h).c_str(), fraseOriginal(vecinoEn(m, h)).c_str(), seMuda(m, h));
    for (int r = 0; r < NUM_RECUERDOS; r++)
        if (recuerdoUsado(m, h, r))
        {
            Recuerdo x = recuerdo(m, h, r);
            printf("recuerdo %d: %s de %s, amistad %d (%s), apodo «%s», isla %d\n", r, x.jugador.c_str(), x.isla.c_str(), x.amistad,
                NIVELES[nivelAmistad(x.amistad)], x.apodo.c_str(), mismaIsla(m, h, r, c.pers()));
        }
    printf("del jugador: %d\n", recuerdoDe(m, h, c.pers()));
    printf("cuarto %d %d %d, casa %d\n", parteCuarto(m, h, CuartoAcento), parteCuarto(m, h, CuartoPared), parteCuarto(m, h, CuartoSuelo), casaDe(m, h));
    Bricolaje b = bricolaje(m, h);
    printf("bricolaje %d:%d:%d receta %d\n", b.hora, b.minuto, b.segundo, b.receta);
    c.guardar = false;
    return 0;
});

OrdenPrueba personalidadO("vecino-personalidad", [](Contexto& c) {
    ponerPersonalidad(c.main.datos, num(c.arg(0)), num(c.arg(1)));
    return 0;
});

// vecino-frase <hueco> <texto> («-» la original)
OrdenPrueba fraseO("vecino-frase", [](Contexto& c) {
    const int h = num(c.arg(0));
    ponerFrase(c.main.datos, h, c.arg(1) == "-" ? fraseOriginal(vecinoEn(c.main.datos, h)) : c.arg(1));
    printf("frase «%s»\n", frase(c.main.datos, h).c_str());
    return 0;
});

OrdenPrueba mudanza("vecino-mudanza", [](Contexto& c) {
    ponerMudanza(c.main.datos, num(c.arg(0)), num(c.arg(1)) != 0);
    return 0;
});

OrdenPrueba todos("vecinos-mudanza", [](Contexto& c) {
    printf("se mudan %d\n", mudarTodos(c.main.datos));
    return 0;
});

// vecino-amistad <hueco> <recuerdo> <puntos> [apodo]
OrdenPrueba amistad("vecino-amistad", [](Contexto& c) {
    const int h = num(c.arg(0)), r = num(c.arg(1));
    ponerAmistad(c.main.datos, h, r, (uint8_t)num(c.arg(2)));
    if (c.args.size() > 3)
        ponerApodo(c.main.datos, h, r, c.arg(3));
    return 0;
});

// vecino-objeto <hueco> <0 ropa | 1 muebles> <i> <id> <cuenta>
OrdenPrueba objeto("vecino-objeto", [](Contexto& c) {
    Objeto o;
    o.id = (uint16_t)num(c.arg(3));
    o.cuenta = (uint16_t)num(c.arg(4));
    ponerObjetoVecino(c.main.datos, num(c.arg(0)), num(c.arg(1)), num(c.arg(2)), o);
    return 0;
});

// vecino-cuarto <hueco> <0 acento | 1 pared | 2 suelo> <id>
OrdenPrueba cuarto("vecino-cuarto", [](Contexto& c) {
    ponerParteCuarto(c.main.datos, num(c.arg(0)), num(c.arg(1)), (uint16_t)num(c.arg(2)));
    return 0;
});

// vecino-casa <hueco> <fachada> <tejado> <puerta> <deco id> <deco cuenta>
OrdenPrueba casa("vecino-casa", [](Contexto& c) {
    const int k = casaDe(c.main.datos, num(c.arg(0)));
    if (k < 0)
        return 1;
    for (int p = 0; p < 3; p++)
        ponerParteCasa(c.main.datos, k, p, (uint16_t)num(c.arg(1 + p)));
    Objeto o;
    o.id = (uint16_t)num(c.arg(4));
    o.cuenta = (uint16_t)num(c.arg(5));
    ponerDecoracionPuerta(c.main.datos, k, o);
    return 0;
});

// vecino-bricolaje <hueco> <hora> <minuto> <segundo> <receta>
OrdenPrueba brico("vecino-bricolaje", [](Contexto& c) {
    Bricolaje b;
    b.hora = (uint8_t)num(c.arg(1)), b.minuto = (uint8_t)num(c.arg(2)), b.segundo = (uint8_t)num(c.arg(3));
    b.receta = (uint16_t)num(c.arg(4));
    ponerBricolaje(c.main.datos, num(c.arg(0)), b);
    return 0;
});

// vecino-exportar <hueco> <archivo.nhv2> [<archivo.nhvh2>]
OrdenPrueba exportar("vecino-exportar", [](Contexto& c) {
    const int h = num(c.arg(0));
    if (!escribir(c.arg(1), exportarVecino(c.main.datos, h)))
        return 1;
    if (c.args.size() > 2 && !escribir(c.arg(2), exportarCasa(c.main.datos, casaDe(c.main.datos, h))))
        return 1;
    c.guardar = false;
    return 0;
});

// vecino-importar <hueco> <archivo> <hacerSuyo 0|1>
OrdenPrueba importar("vecino-importar", [](Contexto& c) {
    const auto a = leerArchivo(c.arg(1));
    const auto v = vecinoDeArchivo(a);
    if (!v.empty())
        printf("de otra isla %d (%s)\n", deOtraIsla(v, c.pers()), islaDe(v).c_str());
    const std::string mal = importarVecino(c.main.datos, num(c.arg(0)), a, num(c.arg(2)) != 0, c.pers());
    if (!mal.empty())
    {
        printf("%s\n", mal.c_str());
        return 1;
    }
    return 0;
});

// casa-importar <hueco> <archivo>
OrdenPrueba importarCasaO("casa-importar", [](Contexto& c) {
    const std::string mal = importarCasa(c.main.datos, casaDe(c.main.datos, num(c.arg(0))), leerArchivo(c.arg(1)));
    if (!mal.empty())
    {
        printf("%s\n", mal.c_str());
        return 1;
    }
    return 0;
});

} // namespace
