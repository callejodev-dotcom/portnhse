// Sección «Isla»: hemisferio y aeródromo, fruta y flores de la isla, flores del suelo, malas hierbas, zona de
// acampada, nabos (precios y tendencia) y visitantes. El museo y el tablón van en isla_museo.cpp e isla_tablon.cpp;
// el mapa y las casas, en los suyos.
#include <algorithm>
#include <ctime>
#include <memory>
#include "../core/isla.hpp"
#include "../core/tablas.hpp"
#include "../core/vecinos.hpp"
#include "partida.hpp"
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
    v.explicacion = "El hemisferio cambia las estaciones, los peces y los bichos.";
    const auto& m = partida::principal();
    const int h = hemisferio(m), a = aeropuerto(m);
    v.dibujo = [] {
        texto("Hemisferio", 60, 150, MARRON);
        texto("Color del aeródromo", 60, 330, MARRON);
    };
    const char* hemis[2] = {"Norte", "Sur"};
    for (int i = 0; i < 2; i++)
        v.b.push_back({{60 + i * 590, 195, 570, 90}, (h == i ? "● " : "") + std::string(hemis[i]), [i] {
            if (hemisferio(partida::principal()) == i)
                return;
            ponerHemisferio(partida::principal(), i);
            cambiaMain(std::string("Hemisferio ") + (i ? "sur" : "norte"));
        }, true, h == i});
    const SDL_Color colores[NUM_AEROPUERTOS] = {{60, 120, 200, 255}, {235, 195, 40, 255}, {235, 130, 40, 255}, {150, 215, 70, 255}};
    for (int i = 0; i < NUM_AEROPUERTOS; i++)
    {
        Boton b{{60 + i * 295, 375, 275, 90}, (a == i ? "● " : "") + std::string(AEROPUERTOS[i]), [i] {
            if (aeropuerto(partida::principal()) == i)
                return;
            ponerAeropuerto(partida::principal(), i);
            cambiaMain(std::string("Aeródromo ") + AEROPUERTOS[i]);
        }, true, a == i};
        b.color = colores[i];
        v.b.push_back(b);
    }
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
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

void elegirFruta(bool hermana)
{
    const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
    const uint16_t actual = hermana ? f.frutaHermana : f.fruta;
    std::vector<std::string> op;
    int marcada = 5;
    for (int i = 0; i < 5; i++)
    {
        op.push_back(FRUTAS_ISLA[i].nombre);
        if (FRUTAS_ISLA[i].id == actual)
            marcada = i;
    }
    op.push_back(marcada == 5 ? "Otra: objeto " + std::to_string(actual) + "…" : std::string("Otra (por su número de objeto)…"));
    elegir(hermana ? "Segunda fruta" : "Fruta de tu isla",
        hermana ? "La de las islas misteriosas (billete Nook) y la que te manda tu madre."
                : "La que tenían los árboles cuando llegaste. También sale en el pasaporte.",
        op, marcada, [hermana, actual](int i) {
            uint32_t id = i < 5 ? FRUTAS_ISLA[i].id : actual;
            if (i == 5 && !pedirNumero("Número de objeto de la fruta", id, 65534))
                return;
            guardarFrutas([hermana, id](FrutasFlores& f) { (hermana ? f.frutaHermana : f.fruta) = (uint16_t)id; },
                std::string(hermana ? "Segunda fruta: " : "Fruta de la isla: ") + nombreFruta((uint16_t)id));
        }, 3);
}

void elegirFlor(bool hermana)
{
    const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
    elegir(hermana ? "Segunda flor" : "Flor de tu isla", hermana ? "La otra flor propia de tu isla." : "La que crece sola en tu isla.",
        std::vector<std::string>(FLORES_ISLA, FLORES_ISLA + NUM_FLORES_ISLA), hermana ? f.florHermana : f.flor, [hermana](int i) {
            guardarFrutas([hermana, i](FrutasFlores& f) { (hermana ? f.florHermana : f.flor) = (uint8_t)i; },
                std::string(hermana ? "Segunda flor: " : "Flor de la isla: ") + FLORES_ISLA[i]);
        });
}

std::string nombreFlor(int i) { return i >= 0 && i < NUM_FLORES_ISLA ? FLORES_ISLA[i] : "Flor " + std::to_string(i); }

void frutasYFlores(Vista& v)
{
    v.titulo = "Fruta y flores de tu isla";
    v.explicacion = "La fruta y las flores que crecen solas en tu isla. Toca una para cambiarla.";
    const FrutasFlores f = leerFrutasFlores(partida::principal(), partida::personal());
    struct Fila4 { std::string t, d; std::function<void()> a; };
    const Fila4 filas[4] = {
        {"Fruta de tu isla: " + nombreFruta(f.fruta), "La que tenían los árboles cuando llegaste. También sale en el pasaporte.", [] { elegirFruta(false); }},
        {"Segunda fruta: " + nombreFruta(f.frutaHermana), "La de las islas misteriosas (billete Nook) y la que te manda tu madre.", [] { elegirFruta(true); }},
        {"Flor de tu isla: " + nombreFlor(f.flor), "La que crece sola en tu isla.", [] { elegirFlor(false); }},
        {"Segunda flor: " + nombreFlor(f.florHermana), "La otra flor propia de tu isla.", [] { elegirFlor(true); }},
    };
    for (int i = 0; i < 4; i++)
    {
        Boton b{{60, 145 + i * 112, 1160, 96}, filas[i].t, filas[i].a};
        b.detalle = filas[i].d;
        b.grande = true;
        v.b.push_back(b);
    }
    v.b.push_back({{440, ABAJO - 82, 400, 66}, "Volver", [] { volver(); }});
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
    l.porColumna = 6;
    l.fila = [e](int i) {
        const auto& f = FLORES_ABIERTAS[i];
        const bool marcada = std::find(e->elegidas.begin(), e->elegidas.end(), f.id) != e->elegidas.end();
        return Fila{std::string(marcada ? "● " : "") + f.nombre, [e, id = f.id] {
            auto it = std::find(e->elegidas.begin(), e->elegidas.end(), id);
            if (it == e->elegidas.end())
                e->elegidas.push_back(id);
            else
                e->elegidas.erase(it);
        }, true, marcada};
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
    auto filtrar = [e] {
        e->vistos.clear();
        const std::string q = normalizar(e->busqueda);
        for (int i = 0; i < NUM_VECINOS_LISTA; i++)
            if (q.empty() || normalizar(LISTA_VECINOS[i].nombre).find(q) != std::string::npos)
                e->vistos.push_back(i);
    };
    filtrar();
    auto buscar = [e, filtrar] {
        std::string q = e->busqueda;
        if (!pedirTexto("Buscar", "Escribe parte del nombre (vacío: todos)", q))
            return;
        e->busqueda = q;
        filtrar();
    };
    Lista l;
    l.titulo = titulo;
    l.explicacion = [e] {
        return (e->busqueda.empty() ? std::string() : "«" + e->busqueda + "»: ") + std::to_string(e->vistos.size()) + " vecinos. Toca uno para elegirlo.";
    };
    l.cuantas = [e] { return (int)e->vistos.size(); };
    l.fila = [e, actual, elegido](int k) {
        const auto& x = LISTA_VECINOS[e->vistos[k]];
        const std::string interno = x.interno;
        return Fila{x.nombre, [interno, elegido] {
            volver();
            elegido(interno);
        }, true, interno == actual};
    };
    l.columnas = 3;
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

void camping(Vista& v)
{
    v.titulo = "Zona de acampada";
    const auto& m = partida::principal();
    if (!hayCamping(m))
    {
        v.explicacion = "Aún no tienes zona de acampada.";
        v.dibujo = [] {
            texto("Cuando Tom Nook te la dé y la coloques en la isla, aquí podrás elegir quién se queda en ella.", 60, 200, MARRON, nullptr, Izquierda, 1160);
        };
        v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
        return;
    }
    v.explicacion = "Elige quién se queda en la zona de acampada.";
    const std::string quien = campista(m);
    bool vive = false;
    for (int i = 0; i < NUM_VECINOS && !quien.empty(); i++)
        vive = vive || vecinoEn(m, i) == quien;
    const std::string nombre = quien.empty() ? std::string("Nadie") : nombreVecino(quien);
    const FechaIsla f = fechaCamping(m);
    v.dibujo = [nombre, quien, vive] {
        texto(nombre, 60, 150, MARRON, g_grande);
        texto(quien.empty() ? "No hay nadie en la zona de acampada." : "Está en la zona de acampada.", 60, 205, MARRON_SUAVE, g_pequena);
        if (vive)
            texto(nombre + " ya vive en tu isla.", 60, 235, ROJO, g_pequena);
    };
    v.b.push_back({{60, 280, 570, 80}, "Elegir campista…", [quien] {
        abrir(elegirVecino("Elegir campista", quien, [](const std::string& interno) {
            // Como NHSE para Mac: al elegir a alguien, la visita es hoy.
            guardarCamping(partida::principal(), interno, hoyIsla());
            cambiaMain(nombreVecino(interno) + " está acampando");
        }));
    }});
    v.b.push_back({{650, 280, 570, 80}, "Dejarla vacía", [] {
        guardarCamping(partida::principal(), "", {});
        cambiaMain("No hay nadie acampando");
    }, !quien.empty()});
    if (!quien.empty())
    {
        v.b.push_back({{60, 400, 570, 80}, "Día de la visita: " + (f.vacia() ? std::string("sin fecha") : textoFechaIsla(f)), [quien] {
            FechaIsla n = fechaCamping(partida::principal());
            if (n.vacia())
                n = hoyIsla();
            uint32_t d = n.dia, mes = n.mes, a = n.anio;
            if (!pedirNumero("Día de la visita (1-31)", d, 31, 1) || !pedirNumero("Mes de la visita (1-12)", mes, 12, 1) ||
                !pedirNumero("Año de la visita", a, 2099, 2000))
                return;
            guardarCamping(partida::principal(), quien, {(int)a, (int)mes, (int)d});
            cambiaMain();
        }});
        v.b.push_back({{650, 400, 570, 80}, "Poner la visita hoy", [quien] {
            guardarCamping(partida::principal(), quien, hoyIsla());
            cambiaMain("La visita es hoy");
        }});
        v.dibujo = [d = v.dibujo] {
            d();
            texto("Los campistas solo se quedan un día: pon la fecha de hoy para encontrarlo al abrir el juego.", 60, 500, MARRON_SUAVE, g_pequena, Izquierda, 1160);
        };
    }
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
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
const char* const DIAS[] = {"Domingo", "Domingo", "Lunes", "Lunes", "Martes", "Martes", "Miércoles", "Miércoles",
    "Jueves", "Jueves", "Viernes", "Viernes", "Sábado", "Sábado"};

std::string nombreTendencia(uint32_t t) { return t < NUM_TENDENCIAS ? TENDENCIAS[t] : "Tendencia " + std::to_string(t); }

void tendencia(Vista& v)
{
    v.titulo = "Tendencia de esta semana";
    v.explicacion = "La elige el juego el domingo. No cambia los precios: decide la de la semana que viene.";
    const auto& m = partida::principal();
    const uint32_t t = tendenciaNabos(m);
    for (int i = 0; i < NUM_TENDENCIAS; i++)
    {
        Boton b{{60, 145 + i * 98, 1160, 86}, (t == (uint32_t)i ? "● " : "") + std::string(TENDENCIAS[i]), [i] {
            if (tendenciaNabos(partida::principal()) == (uint32_t)i)
                return;
            ponerTendenciaNabos(partida::principal(), (uint32_t)i);
            cambiaMain(std::string("Tendencia: ") + TENDENCIAS[i]);
        }, true, t == (uint32_t)i};
        b.detalle = TENDENCIAS_EXPLICACION[i];
        b.grande = true;
        v.b.push_back(b);
    }
    if (t == 1 || t == 3)
        v.b.push_back({{60, 545, 760, 60}, "El pico empieza el " + medioDia(picoNabos(m), false), [] {
            const uint32_t actual = picoNabos(partida::principal());
            std::vector<std::string> op;
            std::vector<uint32_t> valores;
            for (uint32_t h = 2; h < 14; h++)
                op.push_back(medioDia(h)), valores.push_back(h);
            if (actual < 2 || actual >= 14)
                op.push_back(medioDia(actual)), valores.push_back(actual);
            const int marcada = (int)(std::find(valores.begin(), valores.end(), actual) - valores.begin());
            elegir("El pico empieza el…", "El medio día en que empieza a subir.", op, marcada, [valores](int i) {
                ponerPicoNabos(partida::principal(), valores[i]);
                cambiaMain("El pico empieza el " + medioDia(valores[i], false));
            });
        }});
    v.b.push_back({{840, 545, 380, 60}, "Volver", [] { volver(); }});
}

void nabos(Vista& v)
{
    v.titulo = "Nabos";
    v.explicacion = "Precios de esta semana. Toca uno para cambiarlo.";
    const Nabos n = leerNabos(partida::principal());
    auto cambiar = [](int i) {
        Nabos m = leerNabos(partida::principal());
        uint32_t valor = i < 0 ? m.compra : m.venta[i];
        const std::string t = i < 0 ? std::string("Precio de compra (domingo)") : std::string(DIAS[i]) + (i % 2 ? " por la tarde" : " por la mañana");
        if (!pedirNumero(t, valor, 999))
            return;
        (i < 0 ? m.compra : m.venta[i]) = valor;
        escribirNabos(partida::principal(), m);
        partida::cambiaPrincipal();
        cambiado();
    };
    v.b.push_back({{60, 145, 1160, 50}, "Juliana los vende a " + std::to_string(n.compra) + " bayas", [cambiar] { cambiar(-1); }});
    for (int i = 2; i < 14; i++)
        v.b.push_back({{60 + (i % 2) * 590, 208 + (i / 2 - 1) * 58, 570, 50},
            std::string(DIAS[i]) + (i % 2 ? " tarde: " : " mañana: ") + std::to_string(n.venta[i]), [cambiar, i] { cambiar(i); }});
    v.b.push_back({{60, 570, 360, 56}, "Poner todos a…", [] {
        uint32_t valor = 600;
        if (!pedirNumero("Precio de venta para toda la semana", valor, 999))
            return;
        Nabos m = leerNabos(partida::principal());
        for (int i = 2; i < 14; i++)
            m.venta[i] = valor;
        escribirNabos(partida::principal(), m);
        partida::cambiaPrincipal();
        cambiado("Toda la semana a " + std::to_string(valor));
    }});
    v.b.push_back({{440, 570, 480, 56}, "Tendencia: " + nombreTendencia(tendenciaNabos(partida::principal())), [] { abrir(tendencia); }});
    v.b.push_back({{940, 570, 280, 56}, "Volver", [] { volver(); }});
}

// ---------------------------------------------------------------------------------------------------- visitantes
void elegirVisitante(int dia)
{
    // Nadie primero y luego por orden alfabético, como NHSE para Mac.
    std::vector<int> orden;
    for (int i = 1; i < NUM_VISITANTES; i++)
        orden.push_back(i);
    std::sort(orden.begin(), orden.end(), [](int a, int b) { return normalizar(VISITANTES[a]) < normalizar(VISITANTES[b]); });
    orden.insert(orden.begin(), 0);
    std::vector<std::string> op;
    const int actual = visitante(partida::principal(), dia);
    int marcada = -1;
    for (size_t k = 0; k < orden.size(); k++)
    {
        op.push_back(VISITANTES[orden[k]]);
        if (orden[k] == actual)
            marcada = (int)k;
    }
    std::string nota = dia == 0 ? " Los domingos por la mañana viene Juliana." : dia == 6 ? " Los sábados por la noche toca Totakeke." : "";
    elegir(std::string("¿Quién viene el ") + DIA_MIN[dia] + "?", "Quién visita tu isla ese día de esta semana." + nota, op, marcada, [dia, orden](int k) {
        ponerVisitante(partida::principal(), dia, orden[k]);
        cambiaMain(std::string(DIA[dia]) + ": " + VISITANTES[orden[k]]);
    }, 3);
}

std::string nombreVisitante(int v) { return v >= 0 && v < NUM_VISITANTES ? VISITANTES[v] : "Visitante " + std::to_string(v); }

void visitantes(Vista& v)
{
    v.titulo = "Visitantes especiales";
    v.explicacion = "Quién viene a tu isla cada día de esta semana, y los conciertos de Totakeke en la plaza.";
    const auto& m = partida::principal();
    for (int k = 0; k < 7; k++)
    {
        const int dia = SEMANA[k];
        const int y = 140 + k * 64;
        v.b.push_back({{60, y, 620, 56}, std::string(DIA[dia]) + ": " + nombreVisitante(visitante(m, dia)), [dia] { elegirVisitante(dia); }});
        for (int c = 0; c < 2; c++)
        {
            const bool cumple = c == 1, si = concierto(m, dia, cumple);
            v.b.push_back({{700 + c * 265, y, 255, 56}, std::string(si ? "● " : "") + (cumple ? "Cumpleaños" : "Concierto"), [dia, cumple] {
                const bool ahora = !concierto(partida::principal(), dia, cumple);
                ponerConcierto(partida::principal(), dia, cumple, ahora);
                cambiaMain(std::string(ahora ? "Con " : "Sin ") + (cumple ? "concierto de cumpleaños" : "concierto") + " el " + DIA_MIN[dia]);
            }, true, si});
        }
    }
    v.b.push_back({{440, ABAJO - 70, 400, 60}, "Volver", [] { volver(); }});
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
    [] {
        const int n = quitarMalasHierbas(partida::principal());
        partida::cambiaPrincipal();
        cambiado("Quitadas " + std::to_string(n) + " malas hierbas");
    },
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

} // namespace
