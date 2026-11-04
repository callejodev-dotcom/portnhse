// Órdenes de prueba de la sección Jugador: almacén, papelera, recetas, catálogo y reacciones una a una, rueda,
// récords, perfil, billetes del hotel y el buzón.
#include "prueba.hpp"
#include "../source/core/jugador.hpp"
#include "../source/core/jugador_listas.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace acnh;

namespace {

unsigned long num(const std::string& s) { return strtoul(s.c_str(), nullptr, 0); }

// «2024-05-31»
Fecha fecha(const std::string& s)
{
    Fecha f;
    if (s == "-")
        return f;
    f.anio = (uint16_t)atoi(s.c_str());
    f.mes = (uint8_t)atoi(s.c_str() + 5);
    f.dia = (uint8_t)atoi(s.c_str() + 8);
    return f;
}

Objeto objeto(const std::string& id, const std::string& cuenta)
{
    Objeto o;
    o.id = (uint16_t)num(id);
    o.cuenta = (uint16_t)num(cuenta);
    return o;
}

// almacen <hueco> <id> <cuenta>
OrdenPrueba almacen("almacen", [](Contexto& c) {
    printf("abiertos %d\n", almacenAbiertos(c.pers()));
    escribirAlmacen(c.pers(), atoi(c.arg(0).c_str()), objeto(c.arg(1), c.arg(2)));
    return 0;
});

OrdenPrueba almacenVer("almacen-ver", [](Contexto& c) {
    int usados = 0;
    for (int i = 0; i < almacenAbiertos(c.pers()); i++)
        usados += !leerAlmacen(c.pers(), i).vacio();
    printf("abiertos %d, usados %d\n", almacenAbiertos(c.pers()), usados);
    c.guardar = false;
    return 0;
});

// papelera <hueco> <id> <cuenta>
OrdenPrueba papelera("papelera", [](Contexto& c) {
    escribirPapelera(c.main.datos, atoi(c.arg(0).c_str()), objeto(c.arg(1), c.arg(2)));
    return 0;
});

// receta <indice> <0|1> [lista]: aprender u olvidar; con lista (1 fabricada, 3 favorita), solo esa marca.
OrdenPrueba receta1("receta", [](Contexto& c) {
    const int i = atoi(c.arg(0).c_str());
    const bool si = c.arg(1) == "1";
    if (c.args.size() > 2)
        ponerReceta(c.pers(), i, (ListaReceta)atoi(c.arg(2).c_str()), si);
    else
        aprenderReceta(c.pers(), i, si);
    return 0;
});
OrdenPrueba fabricar("fabricar-sabidas", [](Contexto& c) {
    printf("fabricadas %d\n", fabricarSabidas(c.pers()));
    return 0;
});
OrdenPrueba olvidar("olvidar-recetas", [](Contexto& c) {
    olvidarRecetas(c.pers());
    return 0;
});

// recibir <id> <0|1>...: del catálogo (con sus colores)
OrdenPrueba recibir1("recibir", [](Contexto& c) {
    for (size_t a = 0; a + 1 < c.args.size(); a += 2)
    {
        const int id = (int)num(c.arg(a));
        int k = -1;
        for (int i = 0; i < (int)(sizeof(CATALOGO_NOMBRES) / sizeof(CATALOGO_NOMBRES[0])); i++)
            if (CATALOGO_NOMBRES[i].id == id)
                k = i;
        if (k < 0)
            return 1;
        recibir(c.pers(), k, c.arg(a + 1) == "1");
    }
    return 0;
});

// reaccion <valor> <0|1>...
OrdenPrueba reaccion("reaccion", [](Contexto& c) {
    for (size_t a = 0; a + 1 < c.args.size(); a += 2)
    {
        const uint8_t r = (uint8_t)num(c.arg(a));
        if (c.arg(a + 1) == "1")
            printf("aprendida %d\n", aprenderReaccion(c.pers(), r));
        else
            olvidarReaccion(c.pers(), r);
    }
    return 0;
});

// rueda <hueco> <valor>
OrdenPrueba rueda1("rueda", [](Contexto& c) {
    ponerRueda(c.pers(), atoi(c.arg(0).c_str()), (uint8_t)num(c.arg(1)));
    return 0;
});

int recordDe(int indice)
{
    for (int k = 0; k < (int)(sizeof(RECORDS) / sizeof(RECORDS[0])); k++)
        if (RECORDS[k].indice == indice)
            return k;
    return -1;
}

// records <fecha>: completar todos; records borrar
OrdenPrueba records("records", [](Contexto& c) {
    if (c.arg(0) == "borrar")
        borrarRecords(c.pers());
    else
        completarRecords(c.pers(), fecha(c.arg(0)));
    printf("completos %d\n", recordsCompletos(c.pers()));
    return 0;
});

// record <indice> max <fecha> | cuenta <n> | paso <sello> <0|1> | borrar | fecha <sello> <fecha>
OrdenPrueba record("record", [](Contexto& c) {
    const int i = atoi(c.arg(0).c_str());
    const int k = recordDe(i);
    const std::string& que = c.arg(1);
    if (que == "max")
        completarRecord(c.pers(), k, fecha(c.arg(2)));
    else if (que == "cuenta")
        ponerCuentaRecord(c.pers(), i, (uint32_t)num(c.arg(2)));
    else if (que == "paso")
        ponerPaso(c.pers(), k, atoi(c.arg(2).c_str()), c.arg(3) == "1");
    else if (que == "borrar")
        borrarRecord(c.pers(), i);
    else if (que == "fecha")
        ponerFechaSello(c.pers(), i, atoi(c.arg(2).c_str()), fecha(c.arg(3)));
    else
        return 1;
    printf("sellos %d de %d\n", sellosConseguidos(c.pers(), k), RECORDS[k].sellos);
    return 0;
});

// perfil <mes> <dia> <fruta> <registro> <fundador>
OrdenPrueba perfil("perfil", [](Contexto& c) {
    Perfil p = leerPerfil(c.pers());
    printf("antes: cumple %d/%d, fruta %d, registro %s, fundador %d\n", p.dia, p.mes, p.fruta, textoFecha(p.registro).c_str(), p.fundador);
    p.mes = (uint8_t)atoi(c.arg(0).c_str());
    p.dia = (uint8_t)atoi(c.arg(1).c_str());
    p.fruta = (uint16_t)num(c.arg(2));
    p.registro = fecha(c.arg(3));
    p.fundador = c.arg(4) == "1";
    escribirPerfil(c.pers(), p);
    return 0;
});

// hotel <billetes>
OrdenPrueba hotel("hotel", [](Contexto& c) {
    if (!hayHotel(c.pers()))
        return 1;
    escribirNumero(c.pers().data() + BILLETES_HOTEL, (uint32_t)num(c.arg(0)));
    return 0;
});

// buzon <órdenes...>: lee Villager0/postbox.dat, hace los cambios en orden y lo guarda. Órdenes:
//   lista | nueva <fecha> | borrar <slot> | remitente <slot> <tipo> <especie> <variante> | texto <slot> <0-4> <texto>
//   regalo <slot> <id> <cuenta> | leida <slot> <0|1> | favorita <slot> <0|1> | fecha <slot> <fecha>
//   carta <slot> <0|1> | papel <slot> <codigo> | envoltorio <slot> <codigo> | importar <archivo> | exportar <slot> <archivo>
// «slot» puede ser «n» (la última nueva o importada).
OrdenPrueba buzon("buzon", [](Contexto& c) {
    const std::string ruta = c.carpeta + "/Villager0/postbox";
    Archivo b{leerArchivo(ruta + "Header.dat"), leerArchivo(ruta + ".dat")};
    cifrar(b.cabecera, b.datos);
    printf("buzón: tamaño %s, comprobaciones mal %d\n", b.datos.size() == TAM_BUZON ? "bien" : "mal", comprobacionesMal(b.datos));
    if (b.datos.size() != TAM_BUZON || comprobacionesMal(b.datos) != 0)
        return 1;
    auto& d = b.datos;
    int ultima = -1;
    bool cambia = false;
    size_t a = 0;
    auto sig = [&]() -> const std::string& { return c.arg(a++); };
    auto slot = [&] {
        const std::string& s = sig();
        return s == "n" ? ultima : atoi(s.c_str());
    };
    while (a < c.args.size())
    {
        const std::string que = sig();
        if (que == "lista")
        {
            for (int s : cartasUsadas(d))
                printf("carta %d: %s, %s, %s%s «%s»\n", s, remitente(d, s).c_str(), textoFecha(fechaCarta(d, s)).c_str(),
                    leida(d, s) ? "leída" : "sin leer", favorita(d, s) ? ", favorita" : "", textoCarta(d, s, Mensaje).substr(0, 30).c_str());
            continue;
        }
        cambia = true;
        if (que == "nueva")
            printf("nueva %d\n", ultima = nuevaCarta(d, c.main.datos, c.pers(), fecha(sig())));
        else if (que == "borrar")
            borrarCarta(d, slot());
        else if (que == "remitente")
        {
            const int s = slot(), t = atoi(sig().c_str()), e = atoi(sig().c_str()), v = atoi(sig().c_str());
            ponerRemitente(d, s, t, c.pers(), e, v);
            printf("remitente %s\n", remitente(d, s).c_str());
        }
        else if (que == "texto")
        {
            const int s = slot(), t = atoi(sig().c_str());
            std::string x = sig();
            for (size_t p; (p = x.find("\\n")) != std::string::npos;)
                x.replace(p, 2, "\n");
            ponerTextoCarta(d, s, (TextoCarta)t, x);
        }
        else if (que == "regalo")
        {
            const int s = slot();
            const uint16_t id = (uint16_t)num(sig());
            ponerRegalo(d, s, id, (uint16_t)num(sig()));
        }
        else if (que == "leida")
        {
            const int s = slot();
            ponerLeida(d, s, sig() == "1");
        }
        else if (que == "favorita")
        {
            const int s = slot();
            ponerFavorita(d, s, sig() == "1");
        }
        else if (que == "fecha")
        {
            const int s = slot();
            ponerFechaCarta(d, s, fecha(sig()));
        }
        else if (que == "carta")
        {
            const int s = slot();
            ponerEsCarta(d, s, sig() == "1");
        }
        else if (que == "papel")
        {
            const int s = slot();
            ponerPapelCarta(d, s, (uint16_t)num(sig()));
        }
        else if (que == "envoltorio")
        {
            const int s = slot();
            ponerEnvoltorio(d, s, (uint8_t)num(sig()));
        }
        else if (que == "importar")
            printf("importada %d\n", ultima = importarCarta(d, leerArchivo(sig())));
        else if (que == "exportar")
        {
            const int s = slot();
            const auto x = exportarCarta(d, s);
            std::ofstream(sig(), std::ios::binary).write((const char*)x.data(), x.size());
            cambia = false;
        }
        else
        {
            fprintf(stderr, "no conozco «%s»\n", que.c_str());
            return 2;
        }
    }
    c.guardar = false; // only the mailbox
    if (!cambia)
        return 0;
    arreglarComprobaciones(d);
    cifrar(b.cabecera, d);
    std::ofstream(ruta + ".dat", std::ios::binary).write((const char*)d.data(), d.size());
    return 0;
});

} // namespace
