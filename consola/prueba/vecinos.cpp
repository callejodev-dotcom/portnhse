// Órdenes de prueba de la ficha de los vecinos: personalidad, muletilla, mudanza, amistad, ropa y muebles, paredes y
// suelo, casa, bricolaje, exportar e importar vecinos y casas, recuerdos, marcas sueltas, su diseño y valores en bruto.
#include "prueba.hpp"
#include "../source/core/disenos.hpp"
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

// ------------------------------------------------------------------------------------------------ recuerdos
// vecino-saludo <hueco> <recuerdo> <i> <texto>
OrdenPrueba saludoO("vecino-saludo", [](Contexto& c) {
    const int h = num(c.arg(0)), r = num(c.arg(1)), i = num(c.arg(2));
    ponerSaludo(c.main.datos, h, r, i, c.arg(3));
    printf("saludo «%s»\n", saludo(c.main.datos, h, r, i).c_str());
    return 0;
});

// vecino-recuerdo-nombres <hueco> <recuerdo> <jugador> <isla>
OrdenPrueba nombresO("vecino-recuerdo-nombres", [](Contexto& c) {
    const int h = num(c.arg(0)), r = num(c.arg(1));
    ponerJugadorRecuerdo(c.main.datos, h, r, c.arg(2));
    ponerIslaRecuerdo(c.main.datos, h, r, c.arg(3));
    return 0;
});

// vecino-fecha <hueco> <recuerdo> <año> <mes> <día> (0 0 0: sin fecha)
OrdenPrueba fechaO("vecino-fecha", [](Contexto& c) {
    FechaVecino f;
    f.anio = (uint16_t)num(c.arg(2)), f.mes = (uint8_t)num(c.arg(3)), f.dia = (uint8_t)num(c.arg(4));
    ponerFechaSaludo(c.main.datos, num(c.arg(0)), num(c.arg(1)), f);
    return 0;
});

// vecino-marca-recuerdo <hueco> <recuerdo> <i> <valor>
OrdenPrueba marcaRecuerdoO("vecino-marca-recuerdo", [](Contexto& c) {
    ponerMarcaRecuerdo(c.main.datos, num(c.arg(0)), num(c.arg(1)), num(c.arg(2)), (uint8_t)num(c.arg(3)));
    return 0;
});

// vecino-recuerdos <hueco>: lo que enseña la pantalla de recuerdos
OrdenPrueba recuerdosO("vecino-recuerdos", [](Contexto& c) {
    const int h = num(c.arg(0));
    const auto& m = c.main.datos;
    for (int r = 0; r < NUM_RECUERDOS; r++)
    {
        const Recuerdo x = recuerdo(m, h, r);
        const FechaVecino f = fechaSaludo(m, h, r);
        printf("recuerdo %d: «%s» de «%s», apodo «%s», saludo «%s», fecha %d-%d-%d, amistad %d, visitas %d %d\n", r, x.jugador.c_str(),
            x.isla.c_str(), x.apodo.c_str(), saludo(m, h, r, 0).c_str(), f.anio, f.mes, f.dia, marcaRecuerdo(m, h, r, 10),
            marcaRecuerdo(m, h, r, 8), marcaRecuerdo(m, h, r, 9));
    }
    c.guardar = false;
    return 0;
});

// vecino-recuerdo-exportar <hueco> <recuerdo> <archivo.nhvpm>
OrdenPrueba recuerdoExportar("vecino-recuerdo-exportar", [](Contexto& c) {
    c.guardar = false;
    return escribir(c.arg(2), exportarRecuerdo(c.main.datos, num(c.arg(0)), num(c.arg(1)))) ? 0 : 1;
});

// vecino-recuerdo-importar <hueco> <recuerdo> <archivo> <hacerSuyo 0|1>
OrdenPrueba recuerdoImportar("vecino-recuerdo-importar", [](Contexto& c) {
    const int h = num(c.arg(0)), r = num(c.arg(1));
    const auto a = leerArchivo(c.arg(2));
    printf("de otro %d\n", recuerdoDeOtro(c.main.datos, h, r, a));
    const std::string mal = importarRecuerdo(c.main.datos, h, r, a, num(c.arg(3)) != 0);
    if (!mal.empty())
    {
        printf("%s\n", mal.c_str());
        return 1;
    }
    return 0;
});

// ------------------------------------------------------------------------------------------------ marcas sueltas
// vecino-marca <hueco> <i> <valor>
OrdenPrueba marcaO("vecino-marca", [](Contexto& c) {
    ponerMarcaVecino(c.main.datos, num(c.arg(0)), num(c.arg(1)), (uint16_t)num(c.arg(2)));
    return 0;
});

// vecino-marcas-exportar <hueco> <archivo.nhfl>
OrdenPrueba marcasExportar("vecino-marcas-exportar", [](Contexto& c) {
    c.guardar = false;
    return escribir(c.arg(1), marcasVecinoEnBruto(c.main.datos, num(c.arg(0)))) ? 0 : 1;
});

// vecino-marcas-cargar <hueco> <archivo.nhfl>
OrdenPrueba marcasCargar("vecino-marcas-cargar", [](Contexto& c) {
    return cargarMarcasVecino(c.main.datos, num(c.arg(0)), leerArchivo(c.arg(1))) ? 0 : 1;
});

// ------------------------------------------------------------------------------------------------ su diseño
// vecino-diseno <hueco> <archivo.nhpd> <hacerSuyo 0|1> [jugador]: como «Importar» del editor de diseños PRO
OrdenPrueba disenoO("vecino-diseno", [](Contexto& c) {
    std::vector<uint8_t> d = leerArchivo(c.arg(1));
    if (d.size() != TAM_DISENO_VECINO)
        return 1;
    const auto& yo = c.pers(c.args.size() > 3 ? num(c.arg(3)) : 0);
    printf("de otro %d\n", !disenos::esDe(d, yo));
    if (num(c.arg(2)) && !disenos::esDe(d, yo))
        disenos::hacerMio(d, yo);
    disenos::arreglarUso(d, disenos::Pro);
    ponerDisenoVecino(c.main.datos, num(c.arg(0)), d);
    return 0;
});

// vecino-diseno-de <hueco> <diseño PRO de la partida>
OrdenPrueba disenoDe("vecino-diseno-de", [](Contexto& c) {
    ponerDisenoVecino(c.main.datos, num(c.arg(0)), disenos::leer(c.main.datos, disenos::Pro, num(c.arg(1))));
    return 0;
});

// vecino-diseno-exportar <hueco> <archivo.nhpd>
OrdenPrueba disenoExportar("vecino-diseno-exportar", [](Contexto& c) {
    c.guardar = false;
    const auto d = disenoVecino(c.main.datos, num(c.arg(0)));
    printf("diseño «%s»\n", disenos::nombre(d).c_str());
    return escribir(c.arg(1), d) ? 0 : 1;
});

// ------------------------------------------------------------------------------------------------ valores en bruto
// vecino-bruto <cuarto|casa> <hueco> <campo> <valor>
OrdenPrueba bruto("vecino-bruto", [](Contexto& c) {
    const int h = num(c.arg(1)), k = num(c.arg(2));
    const long long valor = strtoll(c.arg(3).c_str(), nullptr, 0);
    if (c.arg(0) == "cuarto")
        ponerCampo(datosVecino(c.main.datos, h) + V_HABITACION, CAMPOS_CUARTO[k], valor);
    else
    {
        const int casa = casaDe(c.main.datos, h);
        if (casa < 0)
            return 1;
        ponerCampo(datosCasa(c.main.datos, casa), CAMPOS_CASA[k], valor);
    }
    return 0;
});

} // namespace
