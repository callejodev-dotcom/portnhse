// Pruebas del núcleo en el Mac: «prueba <carpeta> <orden> [argumentos]» abre una copia de la partida, hace el cambio
// pedido y la vuelve a guardar, como hará la Switch. Luego NHSE (C#) comprueba en tests/NHSE.Mac.Tests/ConsolaTests*.cs
// que la partida está sana y que los cambios son los que hace NHSE.
//
// Cada orden se registra con un OrdenPrueba desde su archivo (prueba/*.cpp), así cada parte añade las suyas sin tocar
// las demás. Las órdenes escriben por la salida lo que las pruebas comprueban.
#pragma once
#include "../source/core/acnh.hpp"
#include <functional>
#include <string>
#include <vector>

struct Contexto {
    std::string carpeta;
    acnh::Archivo main;
    std::vector<acnh::Archivo> jugadores; // Villager0, Villager1... (the ones there are)
    std::vector<std::string> args;        // after the order
    std::vector<uint8_t>& pers(int j = 0) { return jugadores[j].datos; }
    const std::string& arg(size_t i) const { return args.at(i); }
    bool guardar = true; // false: only reads
};

// Devuelve 0 si fue bien (y se guarda la partida), otro número si no.
using Orden = std::function<int(Contexto&)>;
struct OrdenPrueba {
    OrdenPrueba(const char* nombre, Orden orden);
};

std::vector<uint8_t> leerArchivo(const std::string& ruta);
