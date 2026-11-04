// Sección «Isla»: el museo, como el de NHSE para Mac. Una lista por sala (peces, bichos, criaturas marinas, fósiles y
// arte) donde se marca lo que ya está expuesto, con cuándo y quién lo donó; «Donar todos» y «Donar todo lo que falta».
// En «Fechas y más…», lo del editor clásico del Mac: cambiar el día, quién donó y la pieza de cada donación, ordenar
// la lista por fecha, y guardar el museo en la SD o traerlo (el mismo .nhm que NHSE).
#include <cstring>
#include <ctime>
#include <memory>
#include "../core/isla.hpp"
#include "jugador_comun.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

// Lo que ya está donado: se mira una vez por fotograma (son 1024 huecos), no por fila.
std::vector<Donacion> donadas() { return donaciones(partida::principal()); }

int cuantasDonadas(const std::vector<Donacion>& d, int sala)
{
    int n = 0;
    for (int i = INICIO_SALAS[sala]; i < INICIO_SALAS[sala + 1]; i++)
        n += d[i].donada();
    return n;
}

std::string nombreJugador(int j)
{
    return j >= 0 && j < (int)partida::jugadores.size() ? partida::jugadores[j].nombre : std::string();
}

// «Donado el 3 may 2026 por Carli», como MuseumWindow.
std::string detalle(const Donacion& d)
{
    if (!d.donada())
        return "Aún sin donar";
    const std::string quien = nombreJugador(d.jugador);
    if (d.fecha.vacia())
        return quien.empty() ? "Donado" : "Donado por " + quien;
    return "Donado el " + textoFechaIsla(d.fecha) + (quien.empty() ? "" : " por " + quien);
}

// Dona (hoy, a nombre del jugador que se edita) o retira una pieza.
void marcar(int i, bool si)
{
    auto& m = partida::principal();
    const auto& p = PIEZAS_MUSEO[i];
    const bool hecho = si ? donar(m, p.id, partida::yo, hoyIsla()) : retirar(m, p.id);
    if (!hecho)
    {
        avisar(si ? "No queda sitio en el museo" : "No estaba en el museo", true);
        return;
    }
    partida::cambiaPrincipal();
    cambiado(std::string(si ? "Donado: " : "Retirado: ") + p.nombre);
}

// Dona lo que falta de esas piezas, en su orden. Devuelve cuántas.
int donarTodas(int desde, int hasta)
{
    const auto d = donadas();
    const FechaIsla f = hoyIsla();
    int n = 0;
    for (int i = desde; i < hasta; i++)
        if (!d[i].donada())
            n += donar(partida::principal(), PIEZAS_MUSEO[i].id, partida::yo, f);
    if (n)
        partida::cambiaPrincipal();
    return n;
}

const char* const QUE_SALA[NUM_SALAS] = {"peces donados", "bichos donados", "criaturas marinas donadas", "fósiles donados", "obras de arte donadas"};
const char* const FILTROS[3] = {"Ver todo", "Sin donar", "Ya donado"};

Pantalla sala(int s)
{
    struct Estado {
        std::string busqueda;
        int filtro = 0;
        std::vector<Donacion> d;
        std::vector<int> vistas;
        std::vector<std::string> claves; // nombre sin tildes, para buscar
    };
    auto e = std::make_shared<Estado>();
    for (int i = INICIO_SALAS[s]; i < INICIO_SALAS[s + 1]; i++)
        e->claves.push_back(normalizar(PIEZAS_MUSEO[i].nombre));
    auto buscar = [e] {
        std::string q = e->busqueda;
        if (pedirTexto("Buscar", "Escribe parte del nombre (vacío: todas)", q))
            e->busqueda = q;
    };
    Lista l;
    l.titulo = SALAS_MUSEO[s];
    // Lo primero que se pide en cada fotograma: aquí se mira el museo y se filtra.
    l.cuantas = [e, s] {
        e->d = donadas();
        e->vistas.clear();
        const std::string q = normalizar(e->busqueda);
        for (int i = INICIO_SALAS[s]; i < INICIO_SALAS[s + 1]; i++)
        {
            const bool si = e->d[i].donada();
            if ((e->filtro == 1 && si) || (e->filtro == 2 && !si))
                continue;
            if (!q.empty() && e->claves[i - INICIO_SALAS[s]].find(q) == std::string::npos)
                continue;
            e->vistas.push_back(i);
        }
        return (int)e->vistas.size();
    };
    l.explicacion = [e, s] {
        std::string t = std::to_string(cuantasDonadas(e->d, s)) + " de " + std::to_string(INICIO_SALAS[s + 1] - INICIO_SALAS[s]) + " " + QUE_SALA[s] + ".";
        if (e->filtro)
            t += std::string(" Se ven: ") + (e->filtro == 1 ? "sin donar" : "donadas") + ".";
        if (!e->busqueda.empty())
            t += " «" + e->busqueda + "».";
        if (e->vistas.empty())
            t += " No hay nada aquí.";
        return t;
    };
    l.fila = [e](int k) {
        const int i = e->vistas[k];
        const bool si = e->d[i].donada();
        Fila f{(si ? "● " : "") + std::string(PIEZAS_MUSEO[i].nombre), [i, si] { marcar(i, !si); }, true, si};
        f.detalle = detalle(e->d[i]);
        return f;
    };
    l.porColumna = 6;
    l.abajo.push_back([e] { return Fila{FILTROS[e->filtro], [e] { e->filtro = (e->filtro + 1) % 3; }}; });
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.abajo.push_back([e, s] {
        return Fila{"Donar todos", [s] {
            const int n = donarTodas(INICIO_SALAS[s], INICIO_SALAS[s + 1]);
            cambiado(std::to_string(n) + " donados");
        }, cuantasDonadas(e->d, s) < INICIO_SALAS[s + 1] - INICIO_SALAS[s]};
    });
    l.extra = [buscar, e](Vista& v) {
        v.x = buscar;
        v.y = [e] { e->filtro = (e->filtro + 1) % 3; };
        v.teclas = {{"A", "Marcar"}, {"X", "Buscar"}, {"Y", "Ver"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

// ------------------------------------------------------------------------------------------------ fechas y más
std::string nombrePieza(uint16_t id)
{
    for (const auto& p : PIEZAS_MUSEO)
        if (p.id == id)
            return p.nombre;
    Objeto o;
    o.id = id;
    const std::string n = objetos::nombre(o);
    return n.empty() ? "Objeto " + std::to_string(id) : n;
}

// Quién donó: el nombre del jugador, «nadie» (255) o su número si no es de esta isla.
std::string donante(int j)
{
    const std::string n = nombreJugador(j);
    if (!n.empty())
        return n;
    return j == 255 ? std::string("nadie") : "jugador " + std::to_string(j);
}

std::string cuandoYQuien(const EntradaMuseo& e)
{
    return (e.fecha.vacia() ? std::string("Sin fecha") : textoFechaIsla(e.fecha)) + " · " + donante(e.jugador);
}

bool buscarEntrada(int hueco, EntradaMuseo& r)
{
    for (const auto& e : entradasMuseo(partida::principal()))
        if (e.hueco == hueco)
        {
            r = e;
            return true;
        }
    return false;
}

void cambiaMuseo(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

void cambiarFecha(int hueco, FechaIsla actual)
{
    Fecha f;
    f.anio = (uint16_t)actual.anio;
    f.mes = (uint8_t)actual.mes;
    f.dia = (uint8_t)actual.dia;
    jugador::pedirFecha("Día de la donación", f, true, [hueco](Fecha n) {
        const FechaIsla x{n.anio, n.mes, n.dia};
        if (!x.vacia() && !fechaIslaValida(x))
        {
            avisar("Ese día no existe", true);
            return;
        }
        ponerFechaDonacion(partida::principal(), hueco, x);
        cambiaMuseo(x.vacia() ? std::string("Sin fecha") : "Donado el " + textoFechaIsla(x));
    });
}

void cambiarDonante(int hueco, int actual)
{
    std::vector<std::string> op;
    for (const auto& j : partida::jugadores)
        op.push_back(j.nombre);
    const int n = (int)op.size();
    op.push_back("Nadie");
    op.push_back(actual < n || actual == 255 ? std::string("Otro número…") : "Otro número: " + std::to_string(actual) + "…");
    const int marcada = actual < n ? actual : actual == 255 ? n : n + 1;
    elegir("¿Quién la donó?", "El jugador que sale en el museo como quien la donó.", op, marcada, [hueco, n, actual](int i) {
        uint32_t j = i < n ? (uint32_t)i : i == n ? 255u : (uint32_t)actual;
        if (i == n + 1 && !pedirNumero("Número de jugador (0-255)", j, 255))
            return;
        ponerDonante(partida::principal(), hueco, (int)j);
        cambiaMuseo("Donado por " + donante((int)j));
    });
}

// Una donación de la lista: su día, quién la donó y qué pieza es.
void entrada(int hueco, Vista& v)
{
    EntradaMuseo e;
    if (!buscarEntrada(hueco, e))
    {
        volver();
        return;
    }
    v.titulo = "Donación";
    v.explicacion = nombrePieza(e.id) + " (número " + std::to_string(hueco + 1) + " de la lista del museo).";
    Boton dia{{60, 150, 570, 100}, e.fecha.vacia() ? std::string("Sin fecha") : textoFechaIsla(e.fecha), [hueco, e] { cambiarFecha(hueco, e.fecha); }};
    dia.detalle = "El día que se donó. Toca para cambiarlo.";
    v.b.push_back(dia);
    Boton hoy{{650, 150, 570, 100}, "Poner hoy", [hueco] {
        const FechaIsla f = hoyIsla();
        ponerFechaDonacion(partida::principal(), hueco, f);
        cambiaMuseo("Donado el " + textoFechaIsla(f));
    }};
    hoy.detalle = textoFechaIsla(hoyIsla());
    v.b.push_back(hoy);
    Boton quien{{60, 270, 570, 100}, "Donó: " + donante(e.jugador), [hueco, e] { cambiarDonante(hueco, e.jugador); }};
    quien.detalle = "Quién sale como que la donó.";
    v.b.push_back(quien);
    Boton pieza{{650, 270, 570, 100}, "Cambiar la pieza…", [hueco] {
        objetos::buscar([hueco](const Objeto& o) {
            ponerPiezaDonacion(partida::principal(), hueco, o.id);
            cambiaMuseo("Ahora es: " + nombrePieza(o.id));
        }, "esta donación");
    }};
    pieza.detalle = "Otra pieza en su lugar, con el mismo día.";
    v.b.push_back(pieza);
    Boton quitar{{60, 390, 570, 100}, "Retirar del museo", [id = e.id] {
        confirmar("Retirar", "¿Retirar " + nombrePieza(id) + " del museo? Las donaciones de detrás suben un puesto.", "Retirar", [id] {
            retirar(partida::principal(), id);
            volver();
            cambiaMuseo("Retirado: " + nombrePieza(id));
        });
    }};
    quitar.detalle = "Como desmarcarla en su sala.";
    v.b.push_back(quitar);
    v.b.push_back({{440, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
}

// Todas las donaciones, en el orden de la lista del museo.
Pantalla listaEntradas()
{
    struct Estado {
        std::string busqueda;
        std::vector<EntradaMuseo> vistas;
    };
    auto e = std::make_shared<Estado>();
    auto buscar = [e] {
        std::string q = e->busqueda;
        if (pedirTexto("Buscar", "Escribe parte del nombre (vacío: todas)", q))
            e->busqueda = q;
    };
    Lista l;
    l.titulo = "Fechas y donantes";
    l.cuantas = [e] {
        e->vistas.clear();
        const std::string q = normalizar(e->busqueda);
        for (const auto& x : entradasMuseo(partida::principal()))
            if (q.empty() || normalizar(nombrePieza(x.id)).find(q) != std::string::npos)
                e->vistas.push_back(x);
        return (int)e->vistas.size();
    };
    l.explicacion = [e] {
        if (e->vistas.empty())
            return e->busqueda.empty() ? std::string("No hay nada en el museo.") : "«" + e->busqueda + "»: nada.";
        return (e->busqueda.empty() ? std::string() : "«" + e->busqueda + "»: ") + std::to_string(e->vistas.size()) +
               " donaciones, en el orden del museo. Toca una para cambiarla.";
    };
    l.fila = [e](int k) {
        const EntradaMuseo x = e->vistas[k];
        Fila f{nombrePieza(x.id), [h = x.hueco] { abrir([h](Vista& v) { entrada(h, v); }); }};
        f.detalle = cuandoYQuien(x);
        return f;
    };
    l.porColumna = 6;
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Cambiar"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

std::string carpetaMuseo() { return std::string(partida::CARPETA) + "museo/"; }

void exportarMuseoSd()
{
    std::string isla;
    for (char c : partida::isla)
        isla += strchr("\\/:*?\"<>|", c) ? '_' : c;
    char cuando[32];
    const time_t t = time(nullptr);
    strftime(cuando, sizeof(cuando), "%Y%m%d%H%M%S", localtime(&t));
    const std::string n = "museo_" + (isla.empty() ? std::string("isla") : isla) + "_" + cuando + ".nhm";
    if (partida::escribirSd(carpetaMuseo() + n, exportarMuseo(partida::principal())))
        avisar("Guardado en la tarjeta SD: /switch/NHSE/museo/" + n);
    else
        avisar("No he podido guardarlo en la tarjeta SD.", true);
}

void importarMuseoSd()
{
    const std::vector<std::string> nombres = partida::listarSd(carpetaMuseo(), ".nhm");
    if (nombres.empty())
    {
        avisar("No hay museos en la carpeta /switch/NHSE/museo de la tarjeta SD.", true);
        return;
    }
    Lista l;
    l.titulo = "Traer un museo";
    l.explicacion = [] { return std::string("Los museos guardados en /switch/NHSE/museo de la tarjeta SD (.nhm). Toca uno para traerlo."); };
    l.cuantas = [nombres] { return (int)nombres.size(); };
    l.fila = [nombres](int i) {
        const std::string n = nombres[i];
        return Fila{n, [n] {
            confirmar("Traer un museo", "¿Cambiar todo el museo por «" + n + "»? Las donaciones de ahora se pierden.", "Cambiar", [n] {
                std::vector<uint8_t> d;
                if (!partida::leerSd(carpetaMuseo() + n, d))
                {
                    avisar("No he podido leer el archivo.", true);
                    return;
                }
                if (!importarMuseo(partida::principal(), d))
                {
                    avisar("Este archivo no es un museo (tiene " + numero((uint32_t)d.size()) + " bytes y un museo tiene " + numero(ISLA_TAM_MUSEO) + ").", true);
                    return;
                }
                volver();
                cambiaMuseo("Museo traído de la SD");
            });
        }};
    };
    l.columnas = 1;
    l.porColumna = 7;
    abrir(lista(l));
}

void museoMas(Vista& v)
{
    v.titulo = "Museo: fechas y más";
    v.explicacion = "Lo mismo que el editor clásico del museo de NHSE.";
    const int n = (int)entradasMuseo(partida::principal()).size();
    struct Opcion4 { std::string t, d; std::function<void()> a; bool activa; };
    const Opcion4 op[4] = {
        {"Fechas y quién donó cada pieza", std::to_string(n) + " donaciones. Cambia el día, quién la donó o la pieza.", [] { abrir(listaEntradas()); }, n > 0},
        {"Ordenar por fecha", "Pone la lista de donaciones de la más vieja a la más nueva (las que no tienen fecha, al final).", [] {
            confirmar("Ordenar por fecha", "¿Ordenar las donaciones del museo por su fecha?", "Ordenar", [] {
                ordenarMuseoPorFecha(partida::principal());
                cambiaMuseo("Museo ordenado por fecha");
            });
        }, n > 1},
        {"Guardar el museo en la tarjeta SD", "En /switch/NHSE/museo, en el formato de NHSE (.nhm).", exportarMuseoSd, true},
        {"Traer un museo de la tarjeta SD", "Cambia todo el museo por uno guardado antes.", importarMuseoSd, true},
    };
    for (int i = 0; i < 4; i++)
    {
        Boton b{{60, 140 + i * 104, 1160, 92}, op[i].t, op[i].a, op[i].activa};
        b.detalle = op[i].d;
        b.grande = true;
        v.b.push_back(b);
    }
    v.b.push_back({{440, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
}

void museo(Vista& v)
{
    v.titulo = "Museo";
    v.explicacion = "Marca lo que ya está expuesto. Si desmarcas una pieza, se retira del museo.";
    const auto d = donadas();
    int faltan = 0;
    for (int s = 0; s < NUM_SALAS; s++)
    {
        const int hay = cuantasDonadas(d, s), total = INICIO_SALAS[s + 1] - INICIO_SALAS[s];
        faltan += total - hay;
        Boton b{{60, 140 + s * 88, 1160, 78}, SALAS_MUSEO[s], [s] { abrir(sala(s)); }};
        b.detalle = std::to_string(hay) + " de " + std::to_string(total) + " " + QUE_SALA[s] + (hay == total ? " · completo" : "");
        b.grande = true;
        v.b.push_back(b);
    }
    v.b.push_back({{60, ABAJO - 76, 560, 64}, faltan ? "Donar todo lo que falta (" + std::to_string(faltan) + ")…" : std::string("Ya está todo donado"), [faltan] {
        confirmar("Museo", "¿Donar las " + std::to_string(faltan) + " piezas que faltan? Las dona hoy " + partida::jugador().nombre + ".", "Donar", [] {
            const int n = donarTodas(0, NUM_PIEZAS);
            cambiado(std::to_string(n) + " piezas donadas");
        });
    }, faltan > 0});
    v.b.push_back({{640, ABAJO - 76, 300, 64}, "Fechas y más…", [] { abrir(museoMas); }});
    v.b.push_back({{960, ABAJO - 76, 260, 64}, "Volver", [] { volver(); }});
}

Registro r({SeccionIsla, 50, [] { return std::string("Museo"); },
    [] {
        const auto d = donadas();
        int n = 0;
        for (const auto& x : d)
            n += x.donada();
        return std::to_string(n) + " de " + std::to_string(NUM_PIEZAS) + " piezas donadas";
    },
    [] { abrir(museo); }});

} // namespace
