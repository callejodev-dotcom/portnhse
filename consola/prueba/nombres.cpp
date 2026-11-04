// Órdenes de prueba de los nombres (jugador e isla, en todas sus copias) y las Poki.
//   nombre jugador|isla <nuevo>
//   poki <cantidad>
#include "prueba.hpp"
#include "../source/core/jugador.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using namespace acnh;

namespace {

const char* const OTROS[] = {"postbox", "photo_studio_island", "profile", "wherearen"};

// Otro archivo de un jugador: leer y descifrar, y luego cifrar y escribir.
bool abrirOtro(const Contexto& c, int j, const char* nombre, Archivo& a)
{
    const std::string base = c.carpeta + "/Villager" + std::to_string(j) + "/" + nombre;
    a.cabecera = leerArchivo(base + "Header.dat");
    a.datos = leerArchivo(base + ".dat");
    if (a.datos.empty())
        return false;
    cifrar(a.cabecera, a.datos);
    return comprobacionesMal(a.datos) == 0;
}

void guardarOtro(const Contexto& c, int j, const char* nombre, Archivo a)
{
    arreglarComprobaciones(a.datos);
    cifrar(a.cabecera, a.datos);
    std::ofstream(c.carpeta + "/Villager" + std::to_string(j) + "/" + nombre + ".dat", std::ios::binary).write((const char*)a.datos.data(), a.datos.size());
}

OrdenPrueba nombre("nombre", [](Contexto& c) {
    const bool isla = c.arg(0) == "isla";
    auto& p = c.pers();
    const uint32_t pos = isla ? ID_ISLA : ID_PERSONA;
    uint8_t antes[TAM_IDENTIDAD], despues[TAM_IDENTIDAD];
    memcpy(antes, p.data() + pos, TAM_IDENTIDAD);
    escribirTexto(p.data() + pos + 4, c.arg(1), 10);
    memcpy(despues, p.data() + pos, TAM_IDENTIDAD);
    int n = reemplazarTodo(c.main.datos, antes, despues, TAM_IDENTIDAD);
    for (size_t j = 0; j < c.jugadores.size(); j++)
    {
        n += reemplazarTodo(c.jugadores[j].datos, antes, despues, TAM_IDENTIDAD);
        for (const char* f : OTROS)
        {
            Archivo a;
            if (!abrirOtro(c, (int)j, f, a))
                continue;
            if (int k = reemplazarTodo(a.datos, antes, despues, TAM_IDENTIDAD))
                n += k, guardarOtro(c, (int)j, f, a);
        }
    }
    printf("copias %d\n", n);
    return 0;
});

OrdenPrueba poki("poki", [](Contexto& c) {
    Archivo a;
    if (!abrirOtro(c, 0, "wherearen", a))
        return 1;
    escribirNumero(a.datos.data() + POKI, strtoul(c.arg(0).c_str(), nullptr, 10));
    guardarOtro(c, 0, "wherearen", a);
    c.guardar = false;
    return 0;
});

} // namespace
