// Órdenes de prueba de lo primero: estado, flores, hierbas, dinero, vecinos, bolsillos, aprender todo, nabos, buscar.
#include "prueba.hpp"
#include "../source/core/objetos.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace acnh;

namespace {

OrdenPrueba estado("estado", [](Contexto& c) {
    auto& pers = c.pers();
    auto r = contar(c.main.datos);
    printf("flores %d, malas hierbas %d, monedero %u, banco %u, millas %u\n", r.flores, r.malasHierbas,
        leerNumero(pers.data() + MONEDERO), leerNumero(pers.data() + BANCO), leerNumero(pers.data() + MILLAS));
    auto pr = recetas(pers), pc = catalogo(pers), pa = reacciones(pers);
    printf("jugador %s de %s, bolsillos abiertos %d+%d, recetas %d/%d, catalogo %d/%d, reacciones %d/%d\n",
        nombreJugador(pers).c_str(), nombreIsla(pers).c_str(), bolsillosAbiertos(pers, 0), bolsillosAbiertos(pers, 1),
        pr.tiene, pr.total, pc.tiene, pc.total, pa.tiene, pa.total);
    for (int i = 0; i < NUM_BOLSILLOS; i++)
    {
        Objeto o = leerBolsillo(pers, i);
        int k = objetoDe(o);
        if (!o.vacio())
            printf("bolsillo %d: %s x%d\n", i, k >= 0 ? OBJETOS[k].nombre : "?", o.cuenta + 1);
    }
    c.guardar = false;
    return 0;
});

OrdenPrueba buscar("buscar", [](Contexto& c) {
    auto r = buscarObjetos(c.arg(0));
    printf("encontrados %zu\n", r.size());
    for (size_t i = 0; i < r.size() && i < 10; i++)
        printf("  %s\n", OBJETOS[r[i]].nombre);
    c.guardar = false;
    return 0;
});

OrdenPrueba flores("flores", [](Contexto& c) {
    std::vector<uint16_t> ids;
    std::stringstream ss(c.arg(0));
    for (std::string s; std::getline(ss, s, ',');)
        ids.push_back((uint16_t)strtoul(s.c_str(), nullptr, 16));
    printf("cambiadas %d\n", cambiarFlores(c.main.datos, ids, c.arg(1) == "azar"));
    return 0;
});

OrdenPrueba hierbas("hierbas", [](Contexto& c) {
    printf("quitadas %d\n", quitarMalasHierbas(c.main.datos));
    return 0;
});

OrdenPrueba vecino("vecino", [](Contexto& c) {
    const int hueco = atoi(c.arg(0).c_str());
    printf("antes: %s\n", vecinoEn(c.main.datos, hueco).c_str());
    std::string mal = cambiarVecino(c.main.datos, hueco, c.arg(1), leerArchivo(c.arg(2)));
    if (!mal.empty())
    {
        printf("%s\n", mal.c_str());
        return 1;
    }
    printf("ahora: %s\n", vecinoEn(c.main.datos, hueco).c_str());
    return 0;
});

OrdenPrueba dinero("dinero", [](Contexto& c) {
    escribirNumero(c.pers().data() + MONEDERO, strtoul(c.arg(0).c_str(), nullptr, 10));
    escribirNumero(c.pers().data() + BANCO, strtoul(c.arg(1).c_str(), nullptr, 10));
    escribirNumero(c.pers().data() + MILLAS, strtoul(c.arg(2).c_str(), nullptr, 10));
    return 0;
});

// bolsillo <hueco> <id> <cuenta> [extra]
OrdenPrueba bolsillo("bolsillo", [](Contexto& c) {
    Objeto o;
    o.id = (uint16_t)strtoul(c.arg(1).c_str(), nullptr, 0);
    o.cuenta = (uint16_t)strtoul(c.arg(2).c_str(), nullptr, 0);
    if (c.args.size() > 3)
        o.extra = (uint8_t)strtoul(c.arg(3).c_str(), nullptr, 0);
    escribirBolsillo(c.pers(), atoi(c.arg(0).c_str()), o);
    return 0;
});

OrdenPrueba recetasTodas("recetas", [](Contexto& c) {
    printf("nuevas %d\n", aprenderRecetas(c.pers()));
    return 0;
});
OrdenPrueba catalogoTodo("catalogo", [](Contexto& c) {
    printf("nuevos %d\n", completarCatalogo(c.pers()));
    return 0;
});
OrdenPrueba reaccionesTodas("reacciones", [](Contexto& c) {
    printf("nuevas %d\n", aprenderReacciones(c.pers()));
    return 0;
});
OrdenPrueba amigos("amigos", [](Contexto& c) {
    printf("amistades %d\n", mejoresAmigos(c.main.datos));
    return 0;
});

OrdenPrueba nabos("nabos", [](Contexto& c) {
    Nabos n = leerNabos(c.main.datos);
    n.compra = strtoul(c.arg(0).c_str(), nullptr, 10);
    for (int i = 2; i < 14; i++)
        n.venta[i] = strtoul(c.arg(1).c_str(), nullptr, 10);
    escribirNabos(c.main.datos, n);
    return 0;
});

} // namespace
