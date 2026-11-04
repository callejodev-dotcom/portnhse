// Sección «Isla»: hemisferio y aeródromo, fruta y flores de la isla, flores del suelo, malas hierbas, zona de
// acampada, nabos (precios y tendencia) y visitantes (con sus opciones avanzadas), cada una como su ventana de NHSE
// para Mac. El museo y el tablón van en isla_museo.cpp e isla_tablon.cpp; lo que comparten, en isla_comun.cpp; el mapa
// y las casas, en los suyos.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include "../core/isla.hpp"
#include "../core/tablas.hpp"
#include "../core/vecinos.hpp"
#include "partida.hpp"
#include "dibujos.hpp"
#include "isla_comun.hpp"
#include "retratos.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int NUM_FLORES = sizeof(FLORES_ABIERTAS) / sizeof(FLORES_ABIERTAS[0]);
constexpr int NUM_VECINOS_LISTA = sizeof(LISTA_VECINOS) / sizeof(LISTA_VECINOS[0]);

// Los días como los guarda el juego (0 domingo ... 6 sábado), y la semana como se ve (de lunes a domingo).
const char* const DIA[7] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
const char* const DIA_MIN[7] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
const int SEMANA[7] = {1, 2, 3, 4, 5, 6, 0};

// «Lunes por la mañana» (medio día: 0 domingo por la mañana ... 13 sábado por la tarde).
std::string medioDia(uint32_t m, bool mayuscula = true)
{
    if (m >= 14)
        return std::to_string(m);
    return std::string(mayuscula ? DIA[m / 2] : DIA_MIN[m / 2]) + (m % 2 ? " por la tarde" : " por la mañana");
}

void cambiaMain(const std::string& que = "Cambiado")
{
    partida::cambiaPrincipal();
    cambiado(que);
}

// ------------------------------------------------------------------------------------------ hemisferio y aeródromo
void hemisferioYAeropuerto(Vista& v)
{
    v.titulo = "Hemisferio y aeródromo";
    v.explicacion = "Dónde está tu isla y de qué color es su aeródromo.";
    const auto& m = partida::principal();
    const int h = hemisferio(m), a = aeropuerto(m);
    v.dibujo = [] {
        isla::recuadro({60, 136, 1160, 196}, "Hemisferio", dibujos::menu("PltPalm"), "Cambia las estaciones, los peces y los bichos.");
        isla::recuadro({60, 348, 1160, 196}, "Color del aeródromo", dibujos::menu("PlaneTicket"), "El de los mostradores y las banderas del aeródromo.");
    };
    const char* hemis[2] = {"Norte", "Sur"};
    for (int i = 0; i < 2; i++)
    {
        Boton b{{148 + i * 316, 236, 300, 64}, hemis[i], [i] {
            if (hemisferio(partida::principal()) == i)
                return;
            ponerHemisferio(partida::principal(), i);
            cambiaMain(std::string("Hemisferio ") + (i ? "sur" : "norte"));
        }, true, h == i};
        b.imagen = isla::redondel(h == i);
        v.b.push_back(b);
    }
    const SDL_Color colores[NUM_AEROPUERTOS] = {{60, 120, 200, 255}, {235, 195, 40, 255}, {235, 130, 40, 255}, {150, 215, 70, 255}};
    for (int i = 0; i < NUM_AEROPUERTOS; i++)
    {
        Boton b{{148 + i * 260, 448, 244, 64}, AEROPUERTOS[i], [i] {
            if (aeropuerto(partida::principal()) == i)
                return;
            ponerAeropuerto(partida::principal(), i);
            cambiaMain(std::string("Aeródromo ") + AEROPUERTOS[i]);
        }, true, a == i};
        b.color = colores[i];
        v.b.push_back(b);
    }
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// ------------------------------------------------------------------------------------------------- fruta y flores
// Cambia una cosa y guarda todo como NHSE para Mac (también la fruta de los pasaportes).
void guardarFrutas(std::function<void(FrutasFlores&)> cambio, const std::string& que)
{
    auto& m = partida::principal();
    FrutasFlores f = leerFrutasFlores(m, partida::personal());
    cambio(f);
    std::vector<std::vector<uint8_t>*> pers;
    std::vector<uint16_t> antes;
    for (size_t j = 0; j < partida::jugadores.size(); j++)
    {
        pers.push_back(&partida::personal((int)j));
        antes.push_back((uint16_t)(pers.back()->at(PERSONAL_FRUTA) | (pers.back()->at(PERSONAL_FRUTA + 1) << 8)));
    }
    guardarFrutasFlores(m, pers, partida::yo, f);
    for (size_t j = 0; j < pers.size(); j++)
        if (antes[j] != (uint16_t)(pers[j]->at(PERSONAL_FRUTA) | (pers[j]->at(PERSONAL_FRUTA + 1) << 8)))
            partida::cambiaPersonal((int)j);
    cambiaMain(que);
}

// Otra fruta que no es de las cinco de siempre, por su número de objeto.
void otraFruta()
{
    elegir("Otra fruta", "¿Cuál cambias por un objeto que no es de las cinco frutas de siempre?", {"Fruta de tu isla", "Segunda fruta"}, -1,
        [](int i) {
            const bool hermana = i == 1;
            const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
            uint32_t id = hermana ? f.frutaHermana : f.fruta;
            if (!pedirNumero("Número de objeto de la fruta", id, 65534))
                return;
            guardarFrutas([hermana, id](FrutasFlores& f) { (hermana ? f.frutaHermana : f.fruta) = (uint16_t)id; },
                std::string(hermana ? "Segunda fruta: " : "Fruta de la isla: ") + nombreFruta((uint16_t)id));
        });
}

std::string nombreFlor(int i) { return i >= 0 && i < NUM_FLORES_ISLA ? FLORES_ISLA[i] : "Flor " + std::to_string(i); }

// Los dibujos, como FruitsFlowersWindow (en el orden de FRUTAS_ISLA y FLORES_ISLA).
const char* const DIBUJO_FRUTA[5] = {"Apple", "Orange", "Pear", "Peach", "Cherry"};
const char* const DIBUJO_FLOR[NUM_FLORES_ISLA] = {"Anemone1", "Hyacinth5", "Turip0", "Rose0", "Cosmos2", "Yuri2", "Mum1", "Pansi1"};

// Como FruitsFlowersWindow: la fruta y las flores con sus dibujos; la elegida, en verde.
void frutasYFlores(Vista& v)
{
    v.titulo = "Fruta y flores de tu isla";
    v.explicacion = "La fruta y las flores que crecen solas en tu isla. Toca una para elegirla.";
    const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
    static constexpr int ARRIBA = 136, FILA[2] = {224, 384}, DER = 550;
    constexpr int LADO_FRUTA = 78, LADO_FLOR = 70;
    struct Grupo {
        std::string titulo, explicacion;
    };
    const Grupo frutas[2] = {{"Fruta de tu isla: " + nombreFruta(f.fruta), "La de los árboles cuando llegaste."},
        {"Segunda fruta: " + nombreFruta(f.frutaHermana), "La de las islas misteriosas."}};
    const Grupo flores[2] = {{"Flor de tu isla: " + nombreFlor(f.flor), "La que crece sola en tu isla."},
        {"Segunda flor: " + nombreFlor(f.florHermana), "La otra flor propia de tu isla."}};
    v.dibujo = [frutas, flores] {
        isla::recuadro({60, ARRIBA, DER - 20 - 60, 404}, "Fruta", dibujos::menu("Peach"));
        isla::recuadro({DER, ARRIBA, 1220 - DER, 404}, "Flores", dibujos::menu("PltCosmos0"));
        for (int k = 0; k < 2; k++)
        {
            texto(encajar(frutas[k].titulo, 422, g_negrita), 84, FILA[k], MARRON, g_negrita);
            texto(frutas[k].explicacion, 84, FILA[k] + 32, MARRON_SUAVE, g_pequena);
            texto(encajar(flores[k].titulo, 622, g_negrita), DER + 24, FILA[k], MARRON, g_negrita);
            texto(flores[k].explicacion, DER + 24, FILA[k] + 32, MARRON_SUAVE, g_pequena);
        }
    };
    for (int k = 0; k < 2; k++)
    {
        const bool hermana = k == 1;
        const uint16_t actual = hermana ? f.frutaHermana : f.fruta;
        for (int i = 0; i < 5; i++)
        {
            Boton b{{84 + i * (LADO_FRUTA + 8), FILA[k] + 66, LADO_FRUTA, LADO_FRUTA}, FRUTAS_ISLA[i].nombre, [hermana, i] {
                const uint16_t id = FRUTAS_ISLA[i].id;
                guardarFrutas([hermana, id](FrutasFlores& f) { (hermana ? f.frutaHermana : f.fruta) = id; },
                    std::string(hermana ? "Segunda fruta: " : "Fruta de la isla: ") + FRUTAS_ISLA[i].nombre);
            }, true, FRUTAS_ISLA[i].id == actual};
            b.casilla = true;
            b.imagen = dibujos::menu(DIBUJO_FRUTA[i]);
            v.b.push_back(b);
        }
        const int flor = hermana ? f.florHermana : f.flor;
        for (int i = 0; i < NUM_FLORES_ISLA; i++)
        {
            Boton b{{DER + 24 + i * (LADO_FLOR + 8), FILA[k] + 66, LADO_FLOR, LADO_FLOR}, FLORES_ISLA[i], [hermana, i] {
                guardarFrutas([hermana, i](FrutasFlores& f) { (hermana ? f.florHermana : f.flor) = (uint8_t)i; },
                    std::string(hermana ? "Segunda flor: " : "Flor de la isla: ") + FLORES_ISLA[i]);
            }, true, i == flor};
            b.casilla = true;
            b.imagen = dibujos::menu(DIBUJO_FLOR[i]);
            v.b.push_back(b);
        }
    }
    v.b.push_back({{60, ABAJO - 66, 400, 56}, "Otra fruta por su número…", otraFruta});
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// ------------------------------------------------------------------------------------------- flores del suelo
Pantalla flores()
{
    struct Estado {
        std::vector<uint16_t> elegidas;
        bool alAzar = false;
    };
    auto e = std::make_shared<Estado>();
    Lista l;
    l.titulo = "Flores";
    l.explicacion = [] { return "Elige una o varias: todas las flores de la isla (" + std::to_string(contar(partida::principal()).flores) + ") se cambian por esas."; };
    l.cuantas = [] { return NUM_FLORES; };
    l.columnas = 3;
    l.porColumna = 6;
    l.fila = [e](int i) {
        const auto& f = FLORES_ABIERTAS[i];
        const bool marcada = std::find(e->elegidas.begin(), e->elegidas.end(), f.id) != e->elegidas.end();
        // Con su dibujo, como las listas del Mac; las elegidas, en verde.
        std::string nombre = f.nombre;
        if (!nombre.empty())
            nombre[0] = (char)toupper((unsigned char)nombre[0]);
        Fila x{nombre, [e, id = f.id] {
            auto it = std::find(e->elegidas.begin(), e->elegidas.end(), id);
            if (it == e->elegidas.end())
                e->elegidas.push_back(id);
            else
                e->elegidas.erase(it);
        }, true, marcada};
        x.imagen = dibujos::de(f.id);
        return x;
    };
    l.abajo.push_back([e] { return Fila{e->alAzar ? "Mezcla: al azar" : "Mezcla: alternando", [e] { e->alAzar = !e->alAzar; }}; });
    l.abajo.push_back([e] {
        return Fila{e->elegidas.empty() ? "Cambiar" : "Cambiar (" + std::to_string(e->elegidas.size()) + ")", [e] {
            const int n = cambiarFlores(partida::principal(), e->elegidas, e->alAzar, (uint32_t)time(nullptr));
            partida::cambiaPrincipal();
            volver();
            cambiado("Cambiadas " + std::to_string(n) + " flores");
        }, !e->elegidas.empty()};
    });
    l.extra = [](Vista& v) { v.teclas = {{"A", "Marcar"}, {"L R", "Página"}, {"B", "Volver"}}; };
    return lista(l);
}

// ------------------------------------------------------------------------------------------------ malas hierbas
void malasHierbas(Vista& v)
{
    v.titulo = "Malas hierbas";
    v.explicacion = "Quita de golpe todas las malas hierbas del suelo de la isla.";
    const int n = contar(partida::principal()).malasHierbas;
    v.dibujo = [n] {
        isla::recuadro({60, 136, 1160, 210}, n ? (n == 1 ? std::string("Hay 1 mala hierba") : "Hay " + std::to_string(n) + " malas hierbas") : std::string("No hay malas hierbas"),
            dibujos::menu("Weed"), "Las flores, los árboles y lo demás que hay en el suelo se quedan como están.");
    };
    Boton b{{148, 256, 460, 64}, n == 1 ? std::string("Quitar la mala hierba") : "Quitar las " + std::to_string(n) + " malas hierbas", [] {
        const int q = quitarMalasHierbas(partida::principal());
        partida::cambiaPrincipal();
        volver();
        cambiado("Quitadas " + std::to_string(q) + " malas hierbas");
    }, n > 0};
    b.principal = n > 0;
    v.b.push_back(b);
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// ---------------------------------------------------------------------------------------------- zona de acampada
std::string nombreVecino(const std::string& interno)
{
    for (const auto& x : LISTA_VECINOS)
        if (interno == x.interno)
            return x.nombre;
    return interno;
}

// Todos los vecinos, con buscar: al elegir uno, «elegido» y se vuelve.
Pantalla elegirVecino(const std::string& titulo, const std::string& actual, std::function<void(const std::string&)> elegido)
{
    struct Estado {
        std::string busqueda;
        std::vector<int> vistos;
    };
    auto e = std::make_shared<Estado>();
    auto filtrar = [](const std::string& texto, std::vector<int>& vistos) {
        vistos.clear();
        const std::string q = normalizar(texto);
        for (int i = 0; i < NUM_VECINOS_LISTA; i++)
            if (q.empty() || normalizar(LISTA_VECINOS[i].nombre).find(q) != std::string::npos)
                vistos.push_back(i);
        return (int)vistos.size();
    };
    filtrar("", e->vistos);
    // Una fila con la cara: al tocarla se elige (y se cierra todo lo que se abrió para elegir).
    auto fila = [actual, elegido](int i, int cerrar) {
        const auto& x = LISTA_VECINOS[i];
        const std::string interno = x.interno;
        Fila f{x.nombre, [interno, elegido, cerrar] {
            for (int k = 0; k < cerrar; k++)
                volver();
            elegido(interno);
        }, true, interno == actual};
        f.imagen = retratos::de(interno);
        return f;
    };
    // Buscar mientras se escribe; al volver, la lista se queda con lo buscado.
    auto buscar = [e, filtrar, fila] {
        auto encontrados = std::make_shared<std::vector<int>>();
        Buscador b;
        b.titulo = "Buscar un vecino";
        b.explicacion = "Escribe parte del nombre y toca el que quieras.";
        b.texto = e->busqueda;
        b.filtrar = [encontrados, filtrar](const std::string& t) { return filtrar(t, *encontrados); };
        b.fila = [encontrados, fila](int k) { return fila((*encontrados)[k], 2); };
        b.alSalir = [e, filtrar](const std::string& t) {
            e->busqueda = t;
            filtrar(t, e->vistos);
        };
        abrir(buscador(b));
    };
    Lista l;
    l.titulo = titulo;
    l.explicacion = [e] {
        return (e->busqueda.empty() ? std::string() : "«" + e->busqueda + "»: ") + std::to_string(e->vistos.size()) + " vecinos. Toca uno para elegirlo.";
    };
    l.cuantas = [e] { return (int)e->vistos.size(); };
    l.fila = [e, fila](int k) { return fila(e->vistos[k], 1); };
    l.columnas = 3;
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

void cambiarDiaCamping(const std::string& quien)
{
    FechaIsla n = fechaCamping(partida::principal());
    if (n.vacia())
        n = hoyIsla();
    uint32_t d = n.dia, mes = n.mes, a = n.anio;
    if (!pedirNumero("Día de la visita (1-31)", d, 31, 1) || !pedirNumero("Mes de la visita (1-12)", mes, 12, 1) ||
        !pedirNumero("Año de la visita", a, 2099, 2000))
        return;
    guardarCamping(partida::principal(), quien, {(int)a, (int)mes, (int)d});
    cambiaMain();
}

// Como CampsiteWindow: la cara del campista en grande con sus botones, y debajo el día de la visita.
void camping(Vista& v)
{
    v.titulo = "Zona de acampada";
    const auto& m = partida::principal();
    if (!hayCamping(m))
    {
        v.explicacion = "Elige quién se queda en la zona de acampada.";
        v.dibujo = [] {
            isla::recuadro({60, 136, 1160, 150}, "Aún no tienes zona de acampada", dibujos::menu("Tent"),
                "Cuando Tom Nook te la dé y la coloques en la isla, aquí podrás elegir quién se queda en ella.");
        };
        v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
        return;
    }
    v.explicacion = "Elige quién se queda en la zona de acampada.";
    const std::string quien = campista(m);
    bool vive = false;
    for (int i = 0; i < NUM_VECINOS && !quien.empty(); i++)
        vive = vive || vecinoEn(m, i) == quien;
    const std::string nombre = quien.empty() ? std::string("Nadie") : nombreVecino(quien);
    SDL_Texture* cara = quien.empty() ? dibujos::menu("Tent") : retratos::de(quien);
    constexpr int X = 300; // where the text starts, right of the face
    v.dibujo = [nombre, quien, vive, cara] {
        isla::recuadro({60, 136, 1160, 236}, "", nullptr);
        if (cara)
        {
            SDL_Rect d = {96, 156, 180, 180};
            SDL_RenderCopy(g_r, cara, nullptr, &d);
        }
        texto(nombre, X, 166, MARRON, g_titulo);
        texto(quien.empty() ? "No hay nadie en la zona de acampada." : "Está en la zona de acampada.", X, 220, MARRON_SUAVE, g_pequena);
        if (vive)
            texto(nombre + " ya vive en tu isla.", X, 248, ROJO, g_pequena);
        if (!quien.empty())
            isla::recuadro({60, 388, 1160, 196}, "Día de la visita", dibujos::menu("Timer"),
                "Los campistas solo se quedan un día: pon la fecha de hoy para encontrarlo al abrir el juego.");
    };
    Boton elegir{{X, 290, 320, 56}, "Elegir campista…", [quien] {
        abrir(elegirVecino("Elegir campista", quien, [](const std::string& interno) {
            // Como NHSE para Mac: al elegir a alguien, la visita es hoy.
            guardarCamping(partida::principal(), interno, hoyIsla());
            cambiaMain(nombreVecino(interno) + " está acampando");
        }));
    }};
    elegir.principal = true;
    v.b.push_back(elegir);
    v.b.push_back({{X + 336, 290, 280, 56}, "Dejarla vacía", [] {
        guardarCamping(partida::principal(), "", {});
        cambiaMain("No hay nadie acampando");
    }, !quien.empty()});
    if (!quien.empty())
    {
        const FechaIsla f = fechaCamping(m);
        Boton dia{{148, 500, 420, 64}, f.vacia() ? std::string("Sin fecha") : textoFechaIsla(f), [quien] { cambiarDiaCamping(quien); }};
        dia.detalle = "Toca para cambiarlo";
        dia.imagen = dibujos::menu("Timer");
        v.b.push_back(dia);
        const bool esHoy = !f.vacia() && f.anio == hoyIsla().anio && f.mes == hoyIsla().mes && f.dia == hoyIsla().dia;
        v.b.push_back({{584, 500, 320, 64}, esHoy ? std::string("Ya es hoy") : std::string("Poner la visita hoy"), [quien] {
            guardarCamping(partida::principal(), quien, hoyIsla());
            cambiaMain("La visita es hoy");
        }, !esHoy});
    }
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

std::string resumenCamping()
{
    const auto& m = partida::principal();
    if (!hayCamping(m))
        return "Aún no la tienes";
    const std::string q = campista(m);
    return q.empty() ? std::string("No hay nadie acampando") : nombreVecino(q) + " está acampando";
}

// -------------------------------------------------------------------------------------------------------- nabos
// Como TurnipsWindow: el precio de Juliana en su tarjeta, la semana en una tabla con barras debajo y la tendencia.
std::string nombreTendencia(uint32_t t) { return t < NUM_TENDENCIAS ? TENDENCIAS[t] : "Tendencia " + std::to_string(t); }

constexpr uint32_t MAX_NABOS = 9999; // como NHSE para Mac

// Cambiar un precio: -1 el de Juliana, 2...13 los medios días de venta.
void cambiarNabo(int i)
{
    Nabos m = leerNabos(partida::principal());
    uint32_t valor = i < 0 ? m.compra : m.venta[i];
    const std::string t = i < 0 ? std::string("Precio de Juliana (domingo)") : medioDia((uint32_t)i);
    if (!pedirNumero(t, valor, std::max<uint32_t>(MAX_NABOS, valor)))
        return;
    (i < 0 ? m.compra : m.venta[i]) = valor;
    escribirNabos(partida::principal(), m);
    partida::cambiaPrincipal();
    cambiado();
}

void elegirPico()
{
    const uint32_t actual = picoNabos(partida::principal());
    std::vector<std::string> op;
    std::vector<uint32_t> valores;
    for (uint32_t h = 2; h < 14; h++)
        op.push_back(medioDia(h)), valores.push_back(h);
    if (actual < 2 || actual >= 14)
        op.push_back(medioDia(actual)), valores.push_back(actual);
    const int marcada = (int)(std::find(valores.begin(), valores.end(), actual) - valores.begin());
    elegir("El pico empieza el…", "El medio día en que los precios empiezan a subir.", op, marcada, [valores](int i) {
        ponerPicoNabos(partida::principal(), valores[i]);
        cambiaMain("El pico empieza el " + medioDia(valores[i], false));
    });
}

void nabos(Vista& v)
{
    v.titulo = "Precio de los nabos";
    v.explicacion = "Lo que cobra Juliana el domingo y lo que te pagan por ellos cada día de esta semana. Toca un precio para cambiarlo.";
    const auto& m = partida::principal();
    const Nabos n = leerNabos(m);
    const uint32_t t = tendenciaNabos(m);
    // El mejor precio de la semana (el primero si hay varios), como el Mac.
    int mejor = 2;
    for (int i = 3; i < 14; i++)
        if (n.venta[i] > n.venta[mejor])
            mejor = i;
    const std::string mejorTexto = n.venta[mejor] == 0 ? std::string()
        : "Mejor precio: " + medioDia((uint32_t)mejor, false) + ", " + numero(n.venta[mejor]) + " bayas.";

    // La tabla: los días en columnas, mañana y tarde en filas.
    constexpr int TX = 396, TW = 1220 - TX;      // the card of the week
    constexpr int CX = 512, CW = 107, CP = 115; // where the cells start, their width and step
    constexpr int ARRIBA = 136, FILA1 = 246, FILA2 = 296, BASE = 400, ALTO_BARRAS = 52;
    const std::string tendenciaTexto = t < NUM_TENDENCIAS ? std::string(TENDENCIAS_EXPLICACION[t]) : std::string();
    v.dibujo = [n, mejorTexto, tendenciaTexto] {
        // Juliana: lo que hay debajo de su tarjeta.
        if (!mejorTexto.empty())
            texto(mejorTexto, 64, 306, VERDE_OSCURO, g_pequena, Izquierda, 312);
        // La semana.
        isla::recuadro({TX, ARRIBA, TW, 304}, "Precios de venta de esta semana", dibujos::menu("Kabu"),
            "Lo que pagan Tendo y Nendo por cada nabo. Los domingos no compran.");
        for (int d = 0; d < 6; d++)
            texto(DIA[d + 1], CX + d * CP + CW / 2, 230, MARRON, g_pequena, Centro);
        texto("Mañana", TX + 22, FILA1 + 22 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
        texto("Tarde", TX + 22, FILA2 + 22 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
        // Las barras: verdes las que ganan (más de lo que se pagó a Juliana), grises las demás.
        uint32_t tope = std::max<uint32_t>(n.compra, 1);
        for (int i = 2; i < 14; i++)
            tope = std::max(tope, n.venta[i]);
        const SDL_Color gana[2] = {VERDE, {0x6C, 0xC0, 0x8A, 255}}, pierde[2] = {{0xC7, 0xC7, 0xCC, 255}, {0xDD, 0xDD, 0xE2, 255}};
        for (int i = 2; i < 14; i++)
        {
            const int alto = (int)((uint64_t)n.venta[i] * ALTO_BARRAS / tope);
            if (alto <= 0)
                continue;
            const int x = CX + (i / 2 - 1) * CP + (i % 2 ? CW / 2 + 3 : 6);
            rect({x, BASE - alto, CW / 2 - 9, alto}, n.venta[i] > n.compra ? gana[i % 2] : pierde[i % 2]);
        }
        if (n.compra > 0)
        {
            const int y = BASE - (int)((uint64_t)n.compra * ALTO_BARRAS / tope);
            rect({CX, y - 1, 5 * CP + CW, 2}, MARRON_SUAVE);
            texto(numero(n.compra), CX - 10, y - TTF_FontHeight(g_pequena) / 2, MARRON_SUAVE, g_pequena, Derecha);
        }
        texto("Barras verdes: ganas más de lo que pagaste a Juliana.", CX, BASE + 6, MARRON_SUAVE, g_pequena);
        // La tendencia.
        isla::recuadro({60, 452, 1160, 144}, "Tendencia de esta semana", dibujos::menu("BadKabu"), tendenciaTexto);
    };

    Boton juliana{{60, ARRIBA, 320, 160}, "Precio de Juliana", [] { cambiarNabo(-1); }};
    juliana.grande = true;
    juliana.valor = numero(n.compra);
    juliana.detalle = "Toca para cambiarlo";
    juliana.imagen = retratos::de("boc");
    v.b.push_back(juliana);
    for (int d = 0; d < 6; d++)
        for (int pm = 0; pm < 2; pm++)
        {
            const int i = (d + 1) * 2 + pm;
            v.b.push_back({{CX + d * CP, pm ? FILA2 : FILA1, CW, 44}, numero(n.venta[i]), [i] { cambiarNabo(i); }});
        }
    v.b.push_back({{60, 384, 320, 56}, "Poner todos a…", [] {
        uint32_t valor = 500;
        if (!pedirNumero("Precio de venta para toda la semana", valor, MAX_NABOS))
            return;
        Nabos x = leerNabos(partida::principal());
        for (int i = 2; i < 14; i++)
            x.venta[i] = valor;
        escribirNabos(partida::principal(), x);
        partida::cambiaPrincipal();
        cambiado("Toda la semana a " + numero(valor));
    }});
    for (int i = 0; i < NUM_TENDENCIAS; i++)
    {
        Boton b{{84 + i * 282, 538, 270, 46}, TENDENCIAS[i], [i] {
            if (tendenciaNabos(partida::principal()) == (uint32_t)i)
                return;
            ponerTendenciaNabos(partida::principal(), (uint32_t)i);
            cambiaMain(std::string("Tendencia: ") + TENDENCIAS[i] + " (los precios de arriba no cambian)");
        }, true, t == (uint32_t)i};
        b.imagen = isla::tendencia(i);
        v.b.push_back(b);
    }
    // El día del pico, solo en las tendencias con pico (como el Mac).
    const bool conPico = t == 1 || t == 3;
    v.b.push_back({{60, ABAJO - 66, 640, 56}, conPico ? "El pico empieza el " + medioDia(picoNabos(m), false) + "…" : std::string("Sin pico esta semana"),
        elegirPico, conPico});
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// ---------------------------------------------------------------------------------------------------- visitantes
std::string nombreVisitante(int v) { return v >= 0 && v < NUM_VISITANTES ? VISITANTES[v] : "Visitante " + std::to_string(v); }

// Elegir un visitante especial (o nadie); «poner» lo guarda.
void elegirQuien(const std::string& titulo, const std::string& explicacion, int actual, std::function<void(int)> poner)
{
    // Nadie primero y luego por orden alfabético, como NHSE para Mac.
    std::vector<int> orden;
    for (int i = 1; i < NUM_VISITANTES; i++)
        orden.push_back(i);
    std::sort(orden.begin(), orden.end(), [](int a, int b) { return normalizar(VISITANTES[a]) < normalizar(VISITANTES[b]); });
    orden.insert(orden.begin(), 0);
    int marcada = 0;
    for (size_t k = 0; k < orden.size(); k++)
        if (orden[k] == actual)
            marcada = (int)k;
    // Con la cara de cada uno, como las listas del Mac.
    Lista l;
    l.titulo = titulo;
    l.explicacion = [explicacion] { return explicacion; };
    l.cuantas = [orden] { return (int)orden.size(); };
    l.fila = [orden, actual, poner](int k) {
        const int q = orden[k];
        Fila f{VISITANTES[q], [poner, q] {
            volver();
            poner(q);
        }, true, q == actual};
        f.imagen = q ? isla::caraVisitante(q) : nullptr;
        if (!f.imagen)
            f.imagen = isla::vacio();
        return f;
    };
    l.columnas = 3;
    l.porColumna = 4;
    abrir(lista(l), marcada);
}

void elegirVisitante(int dia)
{
    std::string nota = dia == 0 ? " Los domingos por la mañana viene Juliana." : dia == 6 ? " Los sábados por la noche toca Totakeke." : "";
    elegirQuien(std::string("¿Quién viene el ") + DIA_MIN[dia] + "?", "Quién visita tu isla ese día de esta semana." + nota,
        visitante(partida::principal(), dia), [dia](int q) {
            ponerVisitante(partida::principal(), dia, q);
            cambiaMain(std::string(DIA[dia]) + ": " + VISITANTES[q]);
        });
}

// ------------------------------------------------------------------------------ visitantes: opciones avanzadas
// Lo demás que guarda el juego de los visitantes, tal cual (lo que el Mac enseña en «Opciones avanzadas»).

// El número más corto que se vuelve a leer igual («12.5», no «12.50000000»).
std::string textoDecimal(float f)
{
    char s[32] = "0";
    for (int p = 1; p <= 9; p++)
    {
        snprintf(s, sizeof(s), "%.*g", p, (double)f);
        if (strtof(s, nullptr) == f)
            break;
    }
    return s;
}

std::string textoPosicion(const PosicionIsla& p)
{
    return "(" + textoDecimal(p.x) + "; " + textoDecimal(p.y) + "; " + textoDecimal(p.z) + ")";
}

void planSemana(Vista& v)
{
    v.titulo = "Quién viene y plan de la semana";
    v.explicacion = "Viene: quién está ese día. Plan: lo que el juego decidió al empezar la semana.";
    const auto& m = partida::principal();
    auto cara = [](int q) {
        SDL_Texture* t = isla::caraVisitante(q);
        return t ? t : isla::vacio();
    };
    std::vector<std::string> dias;
    for (int k = 0; k < 7; k++)
    {
        const int dia = SEMANA[k];
        const int y = 150 + k * 62;
        dias.push_back(DIA[dia]);
        v.b.push_back({{280, y, 460, 54}, "Viene: " + nombreVisitante(visitante(m, dia)), [dia] {
            elegirQuien(std::string("¿Quién viene el ") + DIA_MIN[dia] + "?", "Solo quién viene; el plan de la semana no cambia.",
                visitante(partida::principal(), dia), [dia](int q) {
                    ponerSoloVisitante(partida::principal(), dia, q);
                    cambiaMain(std::string(DIA[dia]) + ": viene " + VISITANTES[q]);
                });
        }});
        v.b.back().imagen = cara(visitante(m, dia));
        v.b.push_back({{760, y, 460, 54}, "Plan: " + nombreVisitante(planVisitante(m, dia)), [dia] {
            elegirQuien(std::string("Plan del ") + DIA_MIN[dia], "Solo el plan de la semana; quién viene no cambia.",
                planVisitante(partida::principal(), dia), [dia](int q) {
                    ponerPlanVisitante(partida::principal(), dia, q);
                    cambiaMain(std::string(DIA[dia]) + ": en el plan, " + VISITANTES[q]);
                });
        }});
        v.b.back().imagen = cara(planVisitante(m, dia));
    }
    v.dibujo = [dias] {
        for (int k = 0; k < 7; k++)
            texto(dias[k], 60, 150 + k * 62 + 27 - TTF_FontHeight(g_negrita) / 2, MARRON, g_negrita, Izquierda);
    };
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

void posicion(int cual, Vista& v)
{
    v.titulo = cual == 0 ? "Dónde aparecen" : "Otra posición";
    v.explicacion = cual == 0 ? "El sitio de la isla donde aparecen los visitantes (x, y, z)."
                              : "Otra posición que guarda el juego, sin nombre en NHSE (x, y, z).";
    const PosicionIsla p = posicionVisitantes(partida::principal(), cual);
    const float valores[3] = {p.x, p.y, p.z};
    const char* const ejes[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; i++)
    {
        Boton b{{60 + i * 395, 150, 370, 120}, ejes[i], [cual, i, ejes] {
            PosicionIsla q = posicionVisitantes(partida::principal(), cual);
            float* f = i == 0 ? &q.x : i == 1 ? &q.y : &q.z;
            std::string t = textoDecimal(*f);
            if (!pedirTexto(std::string("Posición ") + ejes[i], "Un número, como 12.5 o -3", t, 20))
                return;
            for (char& c : t)
                if (c == ',')
                    c = '.';
            char* fin = nullptr;
            const float n = strtof(t.c_str(), &fin);
            if (t.empty() || fin == t.c_str() || *fin)
            {
                avisar("Eso no es un número", true);
                return;
            }
            *f = n;
            ponerPosicionVisitantes(partida::principal(), cual, q);
            cambiaMain();
        }};
        b.grande = true;
        b.valor = textoDecimal(valores[i]);
        b.detalle = "Toca para cambiarlo";
        v.b.push_back(b);
    }
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

void marcasVisitantes(Vista& v)
{
    v.titulo = "Marcas sin nombre";
    v.explicacion = "Catorce marcas que guarda el juego de los visitantes; NHSE tampoco sabe qué son. Toca una para cambiarla.";
    const auto& m = partida::principal();
    for (int i = 0; i < NUM_MARCAS_VISITANTES; i++)
    {
        const bool si = marcaVisitantes(m, i);
        Boton b{{60 + (i / 7) * 590, 136 + (i % 7) * 64, 570, 56}, "Marca " + std::to_string(i + 1), [i] {
            const bool ahora = !marcaVisitantes(partida::principal(), i);
            ponerMarcaVisitantes(partida::principal(), i, ahora);
            cambiaMain("Marca " + std::to_string(i + 1) + (ahora ? ": sí" : ": no"));
        }};
        b.imagen = isla::interruptor(si);
        v.b.push_back(b);
    }
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

void pedirDiaVisitante(int cual)
{
    const char* const quien[2] = {"Buh", "Estela"};
    std::string t = std::to_string(diaVisitante(partida::principal(), cual));
    if (!pedirTexto(std::string("Número de ") + quien[cual], "Un número entero", t, 11))
        return;
    char* fin = nullptr;
    const long long n = strtoll(t.c_str(), &fin, 10);
    if (t.empty() || *fin || n < INT32_MIN || n > INT32_MAX)
    {
        avisar("Eso no es un número que quepa", true);
        return;
    }
    ponerDiaVisitante(partida::principal(), cual, (int32_t)n);
    cambiaMain(std::string("Número de ") + quien[cual] + ": " + std::to_string(n));
}

void visitantesAvanzado(Vista& v)
{
    v.titulo = "Visitantes: opciones avanzadas";
    v.explicacion = "Todo lo que guarda el juego de los visitantes de esta semana, tal cual. Mejor no tocarlo si no sabes qué es.";
    const auto& m = partida::principal();
    Boton plan{{60, 140, 1160, 88}, "Quién viene y plan de la semana…", [] { abrir(planSemana); }};
    plan.detalle = "Quién viene cada día y el plan de la semana, cada uno por su lado.";
    plan.grande = true;
    plan.imagen = dibujos::menu("Balloon");
    v.b.push_back(plan);
    const char* const quien[2] = {"Buh", "Estela"};
    for (int c = 0; c < 2; c++)
    {
        Boton b{{60 + c * 590, 244, 570, 88}, std::string("Número de ") + quien[c], [c] { pedirDiaVisitante(c); }};
        b.grande = true;
        b.valor = std::to_string(diaVisitante(m, c));
        b.imagen = c == 0 ? isla::caraVisitante(4) : dibujos::menu("StarPiece");
        v.b.push_back(b);
    }
    for (int c = 0; c < 2; c++)
    {
        Boton b{{60 + c * 590, 348, 570, 88}, std::string(c == 0 ? "Dónde aparecen" : "Otra posición") + "…", [c] {
            abrir([c](Vista& w) { posicion(c, w); });
        }};
        b.detalle = textoPosicion(posicionVisitantes(m, c));
        b.grande = true;
        b.imagen = dibujos::menu("Fence");
        v.b.push_back(b);
    }
    int puestas = 0;
    for (int i = 0; i < NUM_MARCAS_VISITANTES; i++)
        puestas += marcaVisitantes(m, i);
    Boton marcas{{60, 452, 1160, 88}, "Marcas sin nombre…", [] { abrir(marcasVisitantes); }};
    marcas.detalle = std::to_string(puestas) + " de " + std::to_string(NUM_MARCAS_VISITANTES) + " puestas.";
    marcas.grande = true;
    v.b.push_back(marcas);
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

void visitantes(Vista& v)
{
    v.titulo = "Visitantes especiales";
    v.explicacion = "Quién viene a tu isla cada día de esta semana, y los conciertos de Totakeke en la plaza.";
    // Como VisitorsWindow: a la izquierda la semana, con la cara de cada visitante; a la derecha los conciertos de
    // Totakeke, en la misma fila que su día.
    const auto& m = partida::principal();
    constexpr int ARRIBA = 136, FILA0 = 214, PASO = 54, ALTO = 48, DERECHA = 640;
    v.dibujo = [] {
        isla::recuadro({60, ARRIBA, DERECHA - 16 - 60, 464}, "Esta semana", dibujos::menu("Balloon"), "Toca un día para cambiar quién viene.");
        isla::recuadro({DERECHA, ARRIBA, 1220 - DERECHA, 464}, "Conciertos de Totakeke", retratos::de("tkkA"), "En la plaza: normal o de cumpleaños.");
        static const char* const CORTO[7] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
        for (int k = 0; k < 7; k++)
        {
            const int y = FILA0 + k * PASO + ALTO / 2 - TTF_FontHeight(g_negrita) / 2;
            texto(DIA[SEMANA[k]], 84, y, MARRON, g_negrita);
            texto(CORTO[SEMANA[k]], DERECHA + 24, y, MARRON, g_negrita);
        }
    };
    for (int k = 0; k < 7; k++)
    {
        const int dia = SEMANA[k];
        const int y = FILA0 + k * PASO;
        const int q = visitante(m, dia);
        Boton b{{240, y, DERECHA - 16 - 24 - 240, ALTO}, nombreVisitante(q), [dia] { elegirVisitante(dia); }};
        b.imagen = q ? isla::caraVisitante(q) : nullptr;
        if (!b.imagen)
            b.imagen = isla::vacio();
        v.b.push_back(b);
        for (int c = 0; c < 2; c++)
        {
            const bool cumple = c == 1, si = concierto(m, dia, cumple);
            Boton t{{DERECHA + 84 + c * 243, y, 233, ALTO}, cumple ? "Cumpleaños" : "Concierto", [dia, cumple] {
                const bool ahora = !concierto(partida::principal(), dia, cumple);
                ponerConcierto(partida::principal(), dia, cumple, ahora);
                cambiaMain(std::string(ahora ? "Con " : "Sin ") + (cumple ? "concierto de cumpleaños" : "concierto") + " el " + DIA_MIN[dia]);
            }};
            t.imagen = isla::interruptor(si);
            t.pequena = true;
            v.b.push_back(t);
        }
    }
    v.b.push_back({{60, ABAJO - 66, 400, 56}, "Opciones avanzadas…", [] { abrir(visitantesAvanzado); }});
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

std::string resumenVisitantes()
{
    std::string s;
    for (int k = 0; k < 7; k++)
    {
        const int q = visitante(partida::principal(), SEMANA[k]);
        if (q != 0)
            s += (s.empty() ? "" : ", ") + nombreVisitante(q);
    }
    return s.empty() ? std::string("Nadie esta semana") : s;
}

// --------------------------------------------------------------------------------------------------- tarjetas
Registro r1({SeccionIsla, 20, [] { return std::string("Flores"); },
    [] { return std::to_string(contar(partida::principal()).flores) + " en la isla"; },
    [] { abrir(flores()); }, [] { return contar(partida::principal()).flores > 0; }});

Registro r2({SeccionIsla, 30, [] { return std::string("Quitar malas hierbas"); },
    [] {
        const int n = contar(partida::principal()).malasHierbas;
        return n ? std::to_string(n) + " en la isla" : std::string("No hay ninguna");
    },
    [] { abrir(malasHierbas); },
    [] { return contar(partida::principal()).malasHierbas > 0; }});

Registro r4({SeccionIsla, 40, [] { return std::string("Hemisferio y aeródromo"); },
    [] {
        const auto& m = partida::principal();
        const int a = aeropuerto(m);
        std::string color = a >= 0 && a < NUM_AEROPUERTOS ? AEROPUERTOS[a] : "?";
        color[0] = (char)tolower((unsigned char)color[0]);
        return std::string(hemisferio(m) ? "Sur" : "Norte") + " · aeródromo " + color;
    },
    [] { abrir(hemisferioYAeropuerto); }});

Registro r5({SeccionIsla, 45, [] { return std::string("Fruta y flores de tu isla"); },
    [] {
        const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
        return nombreFruta(f.fruta) + " · " + nombreFlor(f.flor);
    },
    [] { abrir(frutasYFlores); }});

Registro r6({SeccionIsla, 55, [] { return std::string("Zona de acampada"); }, resumenCamping, [] { abrir(camping); }});

Registro r3({SeccionIsla, 60, [] { return std::string("Nabos"); },
    [] {
        const Nabos n = leerNabos(partida::principal());
        return "Juliana: " + std::to_string(n.compra) + " bayas · " + nombreTendencia(tendenciaNabos(partida::principal()));
    },
    [] { abrir(nabos); }});

Registro r7({SeccionIsla, 70, [] { return std::string("Visitantes especiales"); }, resumenVisitantes, [] { abrir(visitantes); }});

// Los dibujitos de las tarjetas de la sección, como la página de la isla del Mac.
Miniaturas m1("Fruta y flores", [] {
    const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
    std::vector<SDL_Texture*> r;
    for (int i = 0; i < 5; i++)
        if (FRUTAS_ISLA[i].id == f.fruta)
            r.push_back(dibujos::menu(DIBUJO_FRUTA[i]));
    if (f.flor < NUM_FLORES_ISLA)
        r.push_back(dibujos::menu(DIBUJO_FLOR[f.flor]));
    return r;
});
Miniaturas m2("Visitantes", [] {
    std::vector<SDL_Texture*> r;
    for (int dia = 0; dia < 7 && r.size() < 5; dia++)
        if (SDL_Texture* c = isla::caraVisitante(visitante(partida::principal(), dia)))
            if (std::find(r.begin(), r.end(), c) == r.end())
                r.push_back(c);
    return r;
});
Miniaturas m3("Zona de acampada", [] {
    std::vector<SDL_Texture*> r;
    if (SDL_Texture* c = retratos::de(campista(partida::principal())))
        r.push_back(c);
    return r;
});

} // namespace
