// Órdenes de prueba de «Avanzado»: marcas de progreso, clima, huecos abiertos, bytes en bruto, comprobaciones,
// exportar e importar la partida descifrada.
#include "prueba.hpp"
#include "../source/core/avanzado.hpp"
#include "../source/core/avanzado_marcas.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

using namespace acnh;

namespace {

void escribir(const std::string& ruta, const std::vector<uint8_t>& d)
{
    std::ofstream(ruta, std::ios::binary).write((const char*)d.data(), d.size());
}

// «main» o el número del jugador.
std::vector<uint8_t>& archivo(Contexto& c, const std::string& cual)
{
    return cual == "main" ? c.main.datos : c.pers(atoi(cual.c_str()));
}

// marca isla|jugador <indice> <valor>
OrdenPrueba marca("marca", [](Contexto& c) {
    const bool isla = c.arg(0) == "isla";
    const int i = atoi(c.arg(1).c_str());
    auto& d = isla ? c.main.datos : c.pers();
    printf("antes %d\n", leerMarca(d, isla, i));
    escribirMarca(d, isla, i, (int16_t)atoi(c.arg(2).c_str()));
    printf("ahora %d\n", leerMarca(d, isla, i));
    return 0;
});

// Cuántas marcas tienen nombre y las que no son 0.
OrdenPrueba marcasVer("marcas", [](Contexto& c) {
    int n = 0;
    for (const auto& m : MARCAS_ISLA)
        n += leerMarca(c.main.datos, true, m.indice) != 0;
    printf("isla %zu con nombre, %d no son 0\n", sizeof(MARCAS_ISLA) / sizeof(MARCAS_ISLA[0]), n);
    n = 0;
    for (const auto& m : MARCAS_JUGADOR)
        n += leerMarca(c.pers(), false, m.indice) != 0;
    printf("jugador %zu con nombre, %d no son 0\n", sizeof(MARCAS_JUGADOR) / sizeof(MARCAS_JUGADOR[0]), n);
    c.guardar = false;
    return 0;
});

// marcas-exportar isla|jugador <archivo>; marcas-cargar isla|jugador <archivo>
OrdenPrueba marcasExportar("marcas-exportar", [](Contexto& c) {
    const bool isla = c.arg(0) == "isla";
    escribir(c.arg(1), marcasEnBruto(isla ? c.main.datos : c.pers(), isla));
    c.guardar = false;
    return 0;
});
OrdenPrueba marcasCargar("marcas-cargar", [](Contexto& c) {
    const bool isla = c.arg(0) == "isla";
    return cargarMarcas(isla ? c.main.datos : c.pers(), isla, leerArchivo(c.arg(1))) ? 0 : 1;
});

// clima <semilla> <semilla de las islas misteriosas> <hemisferio de las islas misteriosas>
OrdenPrueba clima("clima", [](Contexto& c) {
    escribir32(c.main.datos, SEMILLA_CLIMA, (uint32_t)strtoul(c.arg(0).c_str(), nullptr, 10));
    escribir32(c.main.datos, SEMILLA_TOUR, (uint32_t)strtoul(c.arg(1).c_str(), nullptr, 10));
    c.main.datos[HEMISFERIO_TOUR] = (uint8_t)atoi(c.arg(2).c_str());
    return 0;
});

// huecos <bolsillos parte 1> <bolsillos parte 2> <almacén>
OrdenPrueba huecos("huecos", [](Contexto& c) {
    escribir32(c.pers(), HUECOS_BOLSILLOS1, (uint32_t)strtoul(c.arg(0).c_str(), nullptr, 10));
    escribir32(c.pers(), HUECOS_BOLSILLOS2, (uint32_t)strtoul(c.arg(1).c_str(), nullptr, 10));
    escribir32(c.pers(), HUECOS_ALMACEN, (uint32_t)strtoul(c.arg(2).c_str(), nullptr, 10));
    return 0;
});

// bytes main|<jugador> <posición en hex> <bytes en hex>
OrdenPrueba bytes("bytes", [](Contexto& c) {
    auto& d = archivo(c, c.arg(0));
    uint32_t pos;
    std::vector<uint8_t> b;
    if (!leerPosicion(c.arg(1), pos) || !leerHex(c.arg(2), b) || pos + b.size() > d.size())
        return 1;
    for (size_t i = 0; i < b.size(); i++)
        d[pos + i] = b[i];
    printf("escritos %zu en %s\n", b.size(), hex(pos).c_str());
    return 0;
});

// Las comprobaciones de todos los archivos de la partida (sin cambiar nada). Con «romper», antes cambia un byte de
// postbox.dat del primer jugador en memoria, para ver que lo detecta.
OrdenPrueba comprobar("comprobar", [](Contexto& c) {
    auto mostrar = [](const std::string& nombre, const std::vector<uint8_t>& d) {
        const Comprobado r = comprobarArchivo(nombre.substr(nombre.rfind('/') + 1), d);
        printf("%s: %d de %d mal", nombre.c_str(), (int)r.mal.size(), r.total);
        for (const auto& m : r.mal)
            printf(" %s: (%s-%s)", hex(m.pos).c_str(), hex(m.desde).c_str(), hex(m.hasta).c_str());
        printf("\n");
    };
    mostrar("main.dat", c.main.datos);
    for (size_t j = 0; j < c.jugadores.size(); j++)
        for (const char* n : ARCHIVOS_JUGADOR)
        {
            const std::string base = c.carpeta + "/Villager" + std::to_string(j) + "/" + std::string(n, strlen(n) - 4);
            std::vector<uint8_t> d = leerArchivo(base + ".dat");
            if (d.empty())
                continue;
            cifrar(leerArchivo(base + "Header.dat"), d);
            if (j == 0 && std::string(n) == "postbox.dat" && !c.args.empty() && c.arg(0) == "romper")
                d[0x200] ^= 1;
            mostrar("Villager" + std::to_string(j) + "/" + n, d);
        }
    c.guardar = false;
    return 0;
});

// exportar <carpeta>: la partida descifrada, como «Exportar descifrada» (main.dat y VillagerN/...).
OrdenPrueba exportar("exportar", [](Contexto& c) {
    const std::string dest = c.arg(0);
    mkdir(dest.c_str(), 0777);
    escribir(dest + "/main.dat", paraExportar(c.main.datos));
    for (size_t j = 0; j < c.jugadores.size(); j++)
    {
        const std::string carpeta = "Villager" + std::to_string(j);
        mkdir((dest + "/" + carpeta).c_str(), 0777);
        for (const char* n : ARCHIVOS_JUGADOR)
        {
            const std::string base = c.carpeta + "/" + carpeta + "/" + std::string(n, strlen(n) - 4);
            std::vector<uint8_t> d = j < c.jugadores.size() && std::string(n) == "personal.dat" ? paraExportar(c.pers(j)) : leerArchivo(base + ".dat");
            if (d.empty())
                continue;
            if (std::string(n) != "personal.dat")
                cifrar(leerArchivo(base + "Header.dat"), d);
            escribir(dest + "/" + carpeta + "/" + n, d);
        }
    }
    c.guardar = false;
    return 0;
});

// cargar <carpeta>: main.dat y los personal.dat descifrados de esa carpeta, si tienen el tamaño bueno.
OrdenPrueba cargar("cargar", [](Contexto& c) {
    const std::string orig = c.arg(0);
    int n = cargarDescifrado(c.main.datos, leerArchivo(orig + "/main.dat"));
    for (size_t j = 0; j < c.jugadores.size(); j++)
        n += cargarDescifrado(c.pers(j), leerArchivo(orig + "/Villager" + std::to_string(j) + "/personal.dat"));
    printf("cargados %d\n", n);
    return 0;
});

} // namespace
