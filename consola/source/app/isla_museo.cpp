// Sección «Isla»: el museo, como el de NHSE para Mac. Una lista por sala (peces, bichos, criaturas marinas, fósiles y
// arte) donde se marca lo que ya está expuesto, con cuándo y quién lo donó; «Donar todos» y «Donar todo lo que falta».
#include <memory>
#include "../core/isla.hpp"
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
    v.b.push_back({{60, ABAJO - 76, 640, 64}, faltan ? "Donar todo lo que falta (" + std::to_string(faltan) + ")…" : std::string("Ya está todo donado"), [faltan] {
        confirmar("Museo", "¿Donar las " + std::to_string(faltan) + " piezas que faltan? Las dona hoy " + partida::jugador().nombre + ".", "Donar", [] {
            const int n = donarTodas(0, NUM_PIEZAS);
            cambiado(std::to_string(n) + " piezas donadas");
        });
    }, faltan > 0});
    v.b.push_back({{820, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
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
