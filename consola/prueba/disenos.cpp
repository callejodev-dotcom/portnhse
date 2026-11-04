// Órdenes de prueba de los diseños: ver, cambiar el nombre, hacer mío, exportar, importar, copiar, borrar y la imagen.
// <tipo> es normal, pro, bandera o sastreria.
#include "prueba.hpp"
#include "../source/core/disenos.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace disenos;

namespace {

Tipo tipo(const std::string& s)
{
    return s == "pro" ? Pro : s == "bandera" ? Bandera : s == "sastreria" ? Sastreria : Normales;
}

void escribirArchivo(const std::string& p, const std::vector<uint8_t>& d)
{
    std::ofstream(p, std::ios::binary).write((const char*)d.data(), d.size());
}

// disenos <tipo>: cada hueco con su nombre y de quién es.
OrdenPrueba ver("disenos", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    for (int i = 0; i < cuantos(t); i++)
    {
        auto d = leer(c.main.datos, t, i);
        printf("%d: «%s» de %s (%s) uso %X%s%s%s\n", i, nombre(d).c_str(), autor(d).c_str(), pueblo(d).c_str(), uso(d),
            vacio(d, t) ? " vacio" : "", deJuego(d) ? " juego" : "", esDe(d, c.pers()) ? " mio" : "");
    }
    c.guardar = false;
    return 0;
});

// diseno-nombre <tipo> <hueco> <nombre>
OrdenPrueba renombrar("diseno-nombre", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    const int i = atoi(c.arg(1).c_str());
    auto d = leer(c.main.datos, t, i);
    ponerNombre(d, c.arg(2));
    escribir(c.main.datos, t, i, d);
    printf("nombre «%s»\n", nombre(d).c_str());
    return 0;
});

// diseno-mio <tipo> <hueco>
OrdenPrueba mio("diseno-mio", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    const int i = atoi(c.arg(1).c_str());
    auto d = leer(c.main.datos, t, i);
    hacerMio(d, c.pers());
    escribir(c.main.datos, t, i, d);
    printf("mio %d\n", esDe(d, c.pers()));
    return 0;
});

// diseno-exportar <tipo> <hueco> <archivo>: el archivo .nhd/.nhpd (la partida no cambia).
OrdenPrueba exportar("diseno-exportar", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    auto d = leer(c.main.datos, t, atoi(c.arg(1).c_str()));
    escribirArchivo(c.arg(2), d);
    printf("archivo %s\n", nombreArchivo(d, t, atoi(c.arg(1).c_str()) + 1).c_str());
    c.guardar = false;
    return 0;
});

// diseno-png <tipo> <hueco> <archivo>
OrdenPrueba imagen("diseno-png", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    escribirArchivo(c.arg(2), png(leer(c.main.datos, t, atoi(c.arg(1).c_str())), t));
    c.guardar = false;
    return 0;
});

// diseno-importar <tipo> <hueco> <si|no> <archivo>...: en ese hueco y los siguientes; «si» los hace del jugador.
OrdenPrueba importar("diseno-importar", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    const int i = atoi(c.arg(1).c_str());
    const bool hacer = c.arg(2) == "si";
    for (size_t k = 3; k < c.args.size() && i + (int)k - 3 < cuantos(t); k++)
    {
        auto d = leerArchivo(c.arg(k));
        if ((int)d.size() != tamano(t))
        {
            printf("no es un diseño: %s\n", c.arg(k).c_str());
            return 1;
        }
        if (hacer && !deJuego(d) && !esDe(d, c.pers()))
            hacerMio(d, c.pers());
        arreglarUso(d, t);
        escribir(c.main.datos, t, i + (int)k - 3, d);
    }
    return 0;
});

// diseno-copiar <tipo> <de> <a>
OrdenPrueba copiar("diseno-copiar", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    auto d = leer(c.main.datos, t, atoi(c.arg(1).c_str()));
    arreglarUso(d, t);
    escribir(c.main.datos, t, atoi(c.arg(2).c_str()), d);
    return 0;
});

// diseno-vaciar <tipo> <hueco>: le pone el hueco sin usar del juego.
OrdenPrueba vaciar("diseno-vaciar", [](Contexto& c) {
    const Tipo t = tipo(c.arg(0));
    std::vector<uint8_t> d;
    if (!blanco(c.main.datos, t, d))
    {
        printf("no queda ninguno vacío\n");
        return 1;
    }
    escribir(c.main.datos, t, atoi(c.arg(1).c_str()), d);
    return 0;
});

} // namespace
