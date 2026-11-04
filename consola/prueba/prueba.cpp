// Pruebas del núcleo en el Mac: abre una copia de la partida, hace los cambios pedidos y la vuelve a guardar, como hará
// la Switch. Luego NHSE (C#) comprueba que la partida está sana y que los cambios son los que hace NHSE.
//   prueba <carpeta> estado
//   prueba <carpeta> flores <id,id,...> alterna|azar
//   prueba <carpeta> hierbas
//   prueba <carpeta> dinero <monedero> <banco> <millas>
//   prueba <carpeta> vecino <hueco> <interno> <vecinos.bin>
//   prueba <carpeta> bolsillo <hueco> <id> <cuenta>
//   prueba <carpeta> recetas|catalogo|reacciones|amigos
//   prueba <carpeta> nabos <compra> <venta>
//   prueba <carpeta> buscar <texto>
#include "../source/core/acnh.hpp"
#include "../source/core/objetos.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace acnh;

static std::vector<uint8_t> leer(const std::string& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void escribir(const std::string& p, const std::vector<uint8_t>& d)
{
    std::ofstream(p, std::ios::binary).write((const char*)d.data(), d.size());
}

static Archivo abrir(const std::string& carpeta, const std::string& nombre)
{
    Archivo a{leer(carpeta + "/" + nombre + "Header.dat"), leer(carpeta + "/" + nombre + ".dat")};
    cifrar(a.cabecera, a.datos);
    return a;
}

static void guardar(const std::string& carpeta, const std::string& nombre, Archivo a)
{
    arreglarComprobaciones(a.datos);
    cifrar(a.cabecera, a.datos);
    escribir(carpeta + "/" + nombre + ".dat", a.datos);
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    std::string carpeta = argv[1], orden = argv[2];
    auto main = abrir(carpeta, "main");
    auto pers = abrir(carpeta, "Villager0/personal");
    printf("comprobaciones mal: main %d, personal %d\n", comprobacionesMal(main.datos), comprobacionesMal(pers.datos));
    if (comprobacionesMal(main.datos) != 0 || comprobacionesMal(pers.datos) != 0)
        return 1;
    auto r = contar(main.datos);
    printf("flores %d, malas hierbas %d, monedero %u, banco %u, millas %u\n", r.flores, r.malasHierbas,
        leerNumero(pers.datos.data() + MONEDERO), leerNumero(pers.datos.data() + BANCO), leerNumero(pers.datos.data() + MILLAS));
    auto pr = recetas(pers.datos), pc = catalogo(pers.datos), pa = reacciones(pers.datos);
    printf("jugador %s de %s, bolsillos abiertos %d+%d, recetas %d/%d, catalogo %d/%d, reacciones %d/%d\n",
        nombreJugador(pers.datos).c_str(), nombreIsla(pers.datos).c_str(), bolsillosAbiertos(pers.datos, 0),
        bolsillosAbiertos(pers.datos, 1), pr.tiene, pr.total, pc.tiene, pc.total, pa.tiene, pa.total);
    for (int i = 0; i < NUM_BOLSILLOS; i++)
    {
        Objeto o = leerBolsillo(pers.datos, i);
        int k = objetoDe(o);
        if (!o.vacio())
            printf("bolsillo %d: %s x%d\n", i, k >= 0 ? OBJETOS[k].nombre : "?", o.cuenta + 1);
    }
    if (orden == "estado")
        return 0;
    if (orden == "buscar")
    {
        auto r = buscarObjetos(argv[3]);
        printf("encontrados %zu\n", r.size());
        for (size_t i = 0; i < r.size() && i < 10; i++)
            printf("  %s\n", OBJETOS[r[i]].nombre);
        return 0;
    }
    if (orden == "flores")
    {
        std::vector<uint16_t> ids;
        std::stringstream ss(argv[3]);
        for (std::string s; std::getline(ss, s, ',');)
            ids.push_back((uint16_t)strtoul(s.c_str(), nullptr, 16));
        printf("cambiadas %d\n", cambiarFlores(main.datos, ids, !strcmp(argv[4], "azar")));
    }
    else if (orden == "hierbas")
        printf("quitadas %d\n", quitarMalasHierbas(main.datos));
    else if (orden == "vecino")
    {
        printf("antes: %s\n", vecinoEn(main.datos, atoi(argv[3])).c_str());
        std::string mal = cambiarVecino(main.datos, atoi(argv[3]), argv[4], leer(argv[5]));
        if (!mal.empty())
        {
            printf("%s\n", mal.c_str());
            return 1;
        }
        printf("ahora: %s\n", vecinoEn(main.datos, atoi(argv[3])).c_str());
    }
    else if (orden == "bolsillo")
    {
        Objeto o;
        o.id = (uint16_t)strtoul(argv[4], nullptr, 0);
        o.cuenta = (uint16_t)strtoul(argv[5], nullptr, 0);
        escribirBolsillo(pers.datos, atoi(argv[3]), o);
    }
    else if (orden == "recetas")
        printf("nuevas %d\n", aprenderRecetas(pers.datos));
    else if (orden == "catalogo")
        printf("nuevos %d\n", completarCatalogo(pers.datos));
    else if (orden == "reacciones")
        printf("nuevas %d\n", aprenderReacciones(pers.datos));
    else if (orden == "amigos")
        printf("amistades %d\n", mejoresAmigos(main.datos));
    else if (orden == "nabos")
    {
        Nabos n = leerNabos(main.datos);
        n.compra = strtoul(argv[3], nullptr, 10);
        for (int i = 2; i < 14; i++)
            n.venta[i] = strtoul(argv[4], nullptr, 10);
        escribirNabos(main.datos, n);
    }
    else if (orden == "dinero")
    {
        escribirNumero(pers.datos.data() + MONEDERO, strtoul(argv[3], nullptr, 10));
        escribirNumero(pers.datos.data() + BANCO, strtoul(argv[4], nullptr, 10));
        escribirNumero(pers.datos.data() + MILLAS, strtoul(argv[5], nullptr, 10));
    }
    guardar(carpeta, "main", main);
    guardar(carpeta, "Villager0/personal", pers);
    return 0;
}
