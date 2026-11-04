#include "prueba.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>

using namespace acnh;

namespace {
std::map<std::string, Orden>& ordenes()
{
    static std::map<std::string, Orden> o;
    return o;
}

void escribir(const std::string& p, const std::vector<uint8_t>& d)
{
    std::ofstream(p, std::ios::binary).write((const char*)d.data(), d.size());
}

Archivo abrir(const std::string& carpeta, const std::string& nombre)
{
    Archivo a{leerArchivo(carpeta + "/" + nombre + "Header.dat"), leerArchivo(carpeta + "/" + nombre + ".dat")};
    if (!a.datos.empty())
        cifrar(a.cabecera, a.datos);
    return a;
}

void guardar(const std::string& carpeta, const std::string& nombre, Archivo a)
{
    arreglarComprobaciones(a.datos);
    cifrar(a.cabecera, a.datos);
    escribir(carpeta + "/" + nombre + ".dat", a.datos);
}
} // namespace

OrdenPrueba::OrdenPrueba(const char* nombre, Orden orden) { ordenes()[nombre] = std::move(orden); }

std::vector<uint8_t> leerArchivo(const std::string& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "uso: prueba <carpeta> <orden> [argumentos]; órdenes:");
        for (auto& o : ordenes())
            fprintf(stderr, " %s", o.first.c_str());
        fprintf(stderr, "\n");
        return 2;
    }
    Contexto c;
    c.carpeta = argv[1];
    c.main = abrir(c.carpeta, "main");
    for (int i = 0; i < 8; i++)
    {
        Archivo a = abrir(c.carpeta, "Villager" + std::to_string(i) + "/personal");
        if (a.datos.empty())
            break;
        c.jugadores.push_back(std::move(a));
    }
    int mal = comprobacionesMal(c.main.datos);
    for (auto& j : c.jugadores)
        mal += comprobacionesMal(j.datos);
    printf("comprobaciones mal: main %d, personal %d\n", comprobacionesMal(c.main.datos), comprobacionesMal(c.pers()));
    if (mal != 0)
        return 1;
    for (int i = 3; i < argc; i++)
        c.args.push_back(argv[i]);
    auto o = ordenes().find(argv[2]);
    if (o == ordenes().end())
    {
        fprintf(stderr, "no conozco la orden «%s»\n", argv[2]);
        return 2;
    }
    const int r = o->second(c);
    if (r != 0 || !c.guardar)
        return r;
    guardar(c.carpeta, "main", c.main);
    for (size_t i = 0; i < c.jugadores.size(); i++)
        guardar(c.carpeta, "Villager" + std::to_string(i) + "/personal", c.jugadores[i]);
    return 0;
}
