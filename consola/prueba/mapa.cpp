// Órdenes de prueba del mapa de la isla y de las casas de los jugadores (core/mapa y core/casas). Las coordenadas de la
// isla son casillas de la capa (no del mapa entero); las zonas, «x y ancho alto» al final (sin ellas, toda la capa).
#include "prueba.hpp"
#include "../source/core/casas.hpp"
#include "../source/core/mapa.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace mapa;

namespace {

int num(const std::string& s) { return (int)strtol(s.c_str(), nullptr, 0); }

const char* const RESULTADOS[] = {"bien", "fuera", "no cabe"};

int decir(Resultado r, const Objeto& estorbo)
{
    printf("resultado %s", RESULTADOS[r]);
    if (r == NoCabe)
        printf(" (estorba %s)", nombre(estorbo).c_str());
    printf("\n");
    return 0;
}

Zona zonaDesde(Contexto& c, size_t i)
{
    if (c.args.size() < i + 4)
        return {};
    return {num(c.arg(i)), num(c.arg(i + 1)), num(c.arg(i + 2)), num(c.arg(i + 3))};
}

Tipo tipoDe(const std::string& s)
{
    const char* const nombres[] = {"hierbas", "flores", "arboles", "arbustos", "vallas", "hoyos", "ramas", "conchas", "cosas", "todo"};
    for (int i = 0; i < NUM_TIPOS; i++)
        if (s == nombres[i])
            return (Tipo)i;
    return Todo;
}

// mapa-info: cuántas cosas hay de cada tipo y los edificios.
OrdenPrueba info("mapa-info", [](Contexto& c) {
    for (int k = 0; k < 2; k++)
    {
        Rejilla r = capa(c.main.datos, k);
        printf("capa %d:", k);
        const char* const nombres[] = {"hierbas", "flores", "arboles", "arbustos", "vallas", "hoyos", "ramas", "conchas", "cosas", "todo"};
        for (int t = 0; t < NUM_TIPOS; t++)
            printf(" %s %d", nombres[t], contar(r, (Tipo)t));
        printf("\n");
    }
    for (const auto& e : edificios(c.main.datos, {"Uno", "Dos"}))
        printf("edificio %d %s en %.0f,%.0f (%gx%g)\n", e.tipo, e.nombre.c_str(), e.x, e.y, e.w, e.h);
    c.guardar = false;
    return 0;
});

// mapa-ver <capa> <x> <y>: qué hay.
OrdenPrueba ver("mapa-ver", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    Objeto o;
    int rx, ry;
    raiz(r, num(c.arg(1)), num(c.arg(2)), o, rx, ry);
    int w, h;
    tamano(o, w, h);
    printf("%s en %d,%d (%dx%d)\n", nombre(o).c_str(), rx, ry, w, h);
    c.guardar = false;
    return 0;
});

// mapa-poner <capa> <x> <y> <id> [cuenta] [sistema]: como «Poner aquí» del mapa del Mac (lo que no es planta ni mueble
// queda tirado en el suelo, como al soltarlo en el juego).
OrdenPrueba poner("mapa-poner", [](Contexto& c) {
    const int k = num(c.arg(0));
    Rejilla r = capa(c.main.datos, k);
    Objeto o = nada();
    o.id = (uint16_t)num(c.arg(3));
    if (c.args.size() > 4)
        o.cuenta = (uint16_t)num(c.arg(4));
    if (c.args.size() > 5)
        o.sistema = (uint8_t)num(c.arg(5));
    if (k == 0 && !esPlanta(o) && !esNada(o) && !girable(o) && o.sistema == 0)
        o.sistema |= 0x20;
    Objeto estorbo;
    return decir(mapa::poner(r, o, num(c.arg(1)), num(c.arg(2)), &estorbo), estorbo);
});

OrdenPrueba quitarUno("mapa-quitar", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    printf("quitado %d\n", quitar(r, num(c.arg(1)), num(c.arg(2))) ? 1 : 0);
    return 0;
});

OrdenPrueba girarUno("mapa-girar", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    Objeto estorbo;
    return decir(girar(r, num(c.arg(1)), num(c.arg(2)), &estorbo), estorbo);
});

// mapa-mover <capa> <x> <y> <a x> <a y>
OrdenPrueba moverUno("mapa-mover", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    Objeto estorbo;
    return decir(mover(r, num(c.arg(1)), num(c.arg(2)), num(c.arg(3)), num(c.arg(4)), &estorbo), estorbo);
});

// mapa-quitar-tipo <capa> <tipo> [zona]
OrdenPrueba quitarTipo("mapa-quitar-tipo", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    printf("quitadas %d\n", quitarTodos(r, tipoDe(c.arg(1)), zonaDesde(c, 2)));
    return 0;
});

// mapa-quitar-iguales <capa> <x> <y> [zona]: todas las que se llaman como la de esa casilla.
OrdenPrueba quitarIguales("mapa-quitar-iguales", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    Objeto o;
    int rx, ry;
    raiz(r, num(c.arg(1)), num(c.arg(2)), o, rx, ry);
    printf("%s: quitadas %d\n", nombre(o).c_str(), quitarNombre(r, claveNombre(o), zonaDesde(c, 3)));
    return 0;
});

// mapa-cambiar <capa> <tipo> <ids en hexadecimal, con comas> [zona]: alternando.
OrdenPrueba cambiar("mapa-cambiar", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    std::vector<uint16_t> ids;
    std::stringstream ss(c.arg(2));
    for (std::string s; std::getline(ss, s, ',');)
        ids.push_back((uint16_t)strtoul(s.c_str(), nullptr, 16));
    printf("cambiadas %d\n", cambiarTodos(r, tipoDe(c.arg(1)), ids, false, zonaDesde(c, 3)));
    return 0;
});

// mapa-regar <capa> [zona]
OrdenPrueba regarFlores("mapa-regar", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    printf("regadas %d\n", regar(r, zonaDesde(c, 1)));
    return 0;
});

// mapa-goma <capa> <tipo> <zona> [x y: solo lo que se llama como lo de esa casilla]
OrdenPrueba goma("mapa-goma", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    uint64_t clave = 0;
    if (c.args.size() > 7)
    {
        Objeto o;
        int rx, ry;
        raiz(r, num(c.arg(6)), num(c.arg(7)), o, rx, ry);
        clave = claveNombre(o);
    }
    printf("borradas %d\n", borrar(r, zonaDesde(c, 2), tipoDe(c.arg(1)), clave));
    return 0;
});

// mapa-clases <capa> [zona]
OrdenPrueba verClases("mapa-clases", [](Contexto& c) {
    Rejilla r = capa(c.main.datos, num(c.arg(0)));
    for (const auto& k : clases(r, zonaDesde(c, 1)))
        printf("%d %s\n", k.cuantas, nombre(k.ejemplo).c_str());
    c.guardar = false;
    return 0;
});

// ------------------------------------------------------------------------------------------------------------- casas
// casa-ver <casa>
OrdenPrueba casaVer("casa-ver", [](Contexto& c) {
    const int k = num(c.arg(0));
    printf("nivel %u, tejado %u, fachada %u, puerta %u, habitaciones", casas::nivel(c.main.datos, k), casas::aspecto(c.main.datos, k, 0),
        casas::aspecto(c.main.datos, k, 1), casas::aspecto(c.main.datos, k, 2));
    for (int h : casas::habitaciones(c.main.datos, k))
        printf(" %d", h);
    printf("\n");
    c.guardar = false;
    return 0;
});

struct Sitio {
    int casa, hab, sup;
    Rejilla r;
    Zona l;
};

Sitio sitio(Contexto& c)
{
    Sitio s;
    s.casa = num(c.arg(0)), s.hab = num(c.arg(1)), s.sup = num(c.arg(2));
    s.r = casas::capa(c.main.datos, s.casa, s.hab, s.sup);
    s.l = casas::limites(casas::nivel(c.main.datos, s.casa), s.hab, s.sup);
    return s;
}

// casa-poner <casa> <hab> <capa> <x> <y> <id> [cuenta]
OrdenPrueba casaPoner("casa-poner", [](Contexto& c) {
    Sitio s = sitio(c);
    Objeto o = nada(), estorbo;
    o.id = (uint16_t)num(c.arg(5));
    if (c.args.size() > 6)
        o.cuenta = (uint16_t)num(c.arg(6));
    return decir(casas::poner(s.r, s.l, o, num(c.arg(3)), num(c.arg(4)), &estorbo, nullptr, nullptr), estorbo);
});

// casa-nuevo <casa> <hab> <capa> <x> <y> <id>: «poner algo nuevo» en un sitio libre.
OrdenPrueba casaNuevo("casa-nuevo", [](Contexto& c) {
    Sitio s = sitio(c);
    Objeto o = nada(), estorbo;
    o.id = (uint16_t)num(c.arg(5));
    return decir(casas::ponerNuevo(s.r, s.l, o, num(c.arg(3)), num(c.arg(4)), &estorbo), estorbo);
});

OrdenPrueba casaQuitar("casa-quitar", [](Contexto& c) {
    Sitio s = sitio(c);
    printf("quitado %d\n", quitar(s.r, num(c.arg(3)), num(c.arg(4))) ? 1 : 0);
    return 0;
});

OrdenPrueba casaGirar("casa-girar", [](Contexto& c) {
    Sitio s = sitio(c);
    Objeto estorbo;
    return decir(casas::girar(s.r, s.l, num(c.arg(3)), num(c.arg(4)), &estorbo), estorbo);
});

// casa-mover <casa> <hab> <capa> <x> <y> <a x> <a y>
OrdenPrueba casaMover("casa-mover", [](Contexto& c) {
    Sitio s = sitio(c);
    Objeto estorbo;
    return decir(casas::mover(s.r, s.l, num(c.arg(3)), num(c.arg(4)), num(c.arg(5)), num(c.arg(6)), &estorbo), estorbo);
});

// casa-nivel <casa> <nivel>
OrdenPrueba casaNivel("casa-nivel", [](Contexto& c) {
    casas::ponerNivel(c.main.datos, num(c.arg(0)), (uint32_t)num(c.arg(1)));
    return 0;
});

// casa-aspecto <casa> <0 tejado, 1 fachada, 2 puerta> <valor>
OrdenPrueba casaAspecto("casa-aspecto", [](Contexto& c) {
    casas::ponerAspecto(c.main.datos, num(c.arg(0)), num(c.arg(1)), (uint16_t)num(c.arg(2)));
    return 0;
});

// casa-pared <casa> <hab> <0 papel, 1 suelo> <id>
OrdenPrueba casaPared("casa-pared", [](Contexto& c) {
    Objeto o = nada();
    o.id = (uint16_t)num(c.arg(3));
    casas::ponerPared(c.main.datos, num(c.arg(0)), num(c.arg(1)), num(c.arg(2)) != 0, o);
    return 0;
});

// ------------------------------------------------------------------------------------------- opciones avanzadas
void escribirEn(const std::string& ruta, const std::vector<uint8_t>& d)
{
    FILE* f = fopen(ruta.c_str(), "wb");
    if (f)
    {
        fwrite(d.data(), 1, d.size(), f);
        fclose(f);
    }
}

int decirMal(const std::string& mal)
{
    printf("%s\n", mal.empty() ? "bien" : mal.c_str());
    return mal.empty() ? 0 : 1;
}

// mapa-sin-apoyo: las casillas con algo encima de nada.
OrdenPrueba mapaSinApoyo("mapa-sin-apoyo", [](Contexto& c) {
    const auto v = sinApoyo(c.main.datos);
    printf("sin apoyo %zu\n", v.size());
    for (const auto& p : v)
        printf("%03d,%03d\n", p.x, p.y);
    c.guardar = false;
    return 0;
});

// casa-suceso <casa> <i> <valor>
OrdenPrueba casaSuceso("casa-suceso", [](Contexto& c) {
    casas::ponerSuceso(c.main.datos, num(c.arg(0)), num(c.arg(1)), (int16_t)num(c.arg(2)));
    return 0;
});

// casa-sucesos-exportar <casa> <archivo> / casa-sucesos-importar <casa> <archivo> (un .nhfl)
OrdenPrueba casaSucesosExportar("casa-sucesos-exportar", [](Contexto& c) {
    escribirEn(c.arg(1), casas::sucesos(c.main.datos, num(c.arg(0))));
    c.guardar = false;
    return 0;
});

OrdenPrueba casaSucesosImportar("casa-sucesos-importar", [](Contexto& c) {
    return decirMal(casas::ponerSucesos(c.main.datos, num(c.arg(0)), leerArchivo(c.arg(1))) ? "" : "no mide 256 bytes");
});

// casa-jugador-exportar <casa> <archivo> / casa-jugador-importar <casa> <archivo>
OrdenPrueba casaExportar("casa-jugador-exportar", [](Contexto& c) {
    escribirEn(c.arg(1), casas::exportar(c.main.datos, num(c.arg(0))));
    c.guardar = false;
    return 0;
});

OrdenPrueba casaImportar("casa-jugador-importar", [](Contexto& c) {
    return decirMal(casas::importar(c.main.datos, num(c.arg(0)), leerArchivo(c.arg(1))));
});

// casa-hab-exportar <casa> <hab> <archivo> / casa-hab-importar <casa> <hab> <archivo>
OrdenPrueba casaHabExportar("casa-hab-exportar", [](Contexto& c) {
    escribirEn(c.arg(2), casas::exportarHabitacion(c.main.datos, num(c.arg(0)), num(c.arg(1))));
    c.guardar = false;
    return 0;
});

OrdenPrueba casaHabImportar("casa-hab-importar", [](Contexto& c) {
    return decirMal(casas::importarHabitacion(c.main.datos, num(c.arg(0)), num(c.arg(1)), leerArchivo(c.arg(2))));
});

// casa-valor <casa> <número de casas::Valor> <valor>
OrdenPrueba casaValor("casa-valor", [](Contexto& c) {
    casas::ponerValor(c.main.datos, num(c.arg(0)), (casas::Valor)num(c.arg(1)), strtoll(c.arg(2).c_str(), nullptr, 0));
    return 0;
});

// casa-valor-ver <casa>: todos los valores sueltos.
OrdenPrueba casaValorVer("casa-valor-ver", [](Contexto& c) {
    for (int v = 0; v < casas::NUM_VALORES; v++)
        printf("%lld ", (long long)casas::valor(c.main.datos, num(c.arg(0)), (casas::Valor)v));
    printf("\n");
    c.guardar = false;
    return 0;
});

// casa-objeto <casa> <0 puerta, 1 buzón, 2 buzón pedido> <id> [cuenta] [sistema]
OrdenPrueba casaObjeto("casa-objeto", [](Contexto& c) {
    Objeto o = nada();
    o.id = (uint16_t)num(c.arg(2));
    if (c.args.size() > 3)
        o.cuenta = (uint16_t)num(c.arg(3));
    if (c.args.size() > 4)
        o.sistema = (uint8_t)num(c.arg(4));
    casas::ponerObjetoCasa(c.main.datos, num(c.arg(0)), num(c.arg(1)), o);
    return 0;
});

} // namespace
