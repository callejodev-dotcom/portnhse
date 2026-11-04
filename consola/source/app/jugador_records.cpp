// Sección «Jugador»: récords de millas Nook (Nook Miles+), como AchievementsWindow de NHSE para Mac: qué cuenta cada
// uno, por dónde va y los sellos; cambiar la cuenta, «al máximo», empezar de cero, las fechas de los sellos, y
// completar o borrar todos.
#include <memory>
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "jugador_comun.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int NUM_RECORDS = sizeof(RECORDS) / sizeof(RECORDS[0]);

std::vector<uint8_t>& yo() { return partida::personal(); }

void cambia(const std::string& que = "Cambiado")
{
    partida::cambiaPersonal();
    cambiado(que);
}

bool completo(int k) { return sellosConseguidos(yo(), k) == RECORDS[k].sellos; }

std::string sellos(int k) { return numero(sellosConseguidos(yo(), k)) + " de " + numero(RECORDS[k].sellos) + " sellos"; }

// «1.234 · próximo sello al llegar a 2.000», como el Mac.
std::string progreso(int k)
{
    const Record& r = RECORDS[k];
    if (recordPorPasos(k))
        return sellos(k);
    const uint32_t n = cuentaRecord(yo(), r.indice);
    for (int i = 0; i < r.sellos && i < 5; i++)
        if (n < r.umbral[i])
            return numero(n) + " · próximo sello al llegar a " + numero(r.umbral[i]);
    return numero(n) + " · todos los sellos conseguidos";
}

std::string etiquetaSello(int k, int i) { return recordPorPasos(k) ? "Sello " + std::to_string(i + 1) : numero(RECORDS[k].umbral[i]); }

// Las fechas en que consiguió cada sello.
void fechas(int k)
{
    abrir([k](Vista& v) {
        const Record& r = RECORDS[k];
        v.titulo = r.nombre;
        v.explicacion = "El día en que consiguió cada sello. Puedes dejarlas como están.";
        for (int i = 0; i < r.sellos; i++)
        {
            Boton b{{60 + (i % 2) * 590, 160 + (i / 2) * 105, 570, 90}, textoFecha(fechaSello(yo(), r.indice, i)), [k, i] {
                const int indice = RECORDS[k].indice;
                jugador::pedirFecha("Fecha del sello", fechaSello(yo(), indice, i), true, [indice, i](Fecha f) {
                    ponerFechaSello(yo(), indice, i, f);
                    cambia();
                });
            }};
            b.detalle = etiquetaSello(k, i);
            v.b.push_back(b);
        }
        v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
    });
}

// Un récord: la cuenta (o los pasos), los sellos y qué hacer con él.
void record(int k)
{
    abrir([k](Vista& v) {
        const Record& r = RECORDS[k];
        v.titulo = r.nombre;
        v.explicacion = progreso(k);
        const bool pasos = recordPorPasos(k);
        if (!pasos)
        {
            Boton c{{60, 160, 570, 100}, numero(cuentaRecord(yo(), r.indice)), [k] {
                uint32_t n = cuentaRecord(yo(), RECORDS[k].indice);
                if (pedirNumero(RECORDS[k].nombre, n, 0xFFFFFFFFu))
                {
                    ponerCuentaRecord(yo(), RECORDS[k].indice, n);
                    cambia();
                }
            }};
            c.detalle = "Cuántas veces (toca para cambiarlo)";
            v.b.push_back(c);
        }
        else
        {
            Boton c{{60, 160, 570, 100}, sellos(k), [] { avisar("Toca cada sello para ponerlo o quitarlo."); }};
            c.detalle = "Toca cada sello para ponerlo o quitarlo";
            v.b.push_back(c);
        }
        v.b.push_back({{650, 160, 570, 100}, "Al máximo", [k] {
            completarRecord(yo(), k, jugador::hoy());
            cambia("Récord completado");
        }, !completo(k)});

        // Los sellos: los conseguidos, marcados. En los de pasos se ponen y se quitan tocándolos.
        const int ancho = (1160 - (r.sellos - 1) * 12) / std::max<int>(r.sellos, 1);
        for (int i = 0; i < r.sellos; i++)
        {
            const bool si = selloConseguido(yo(), k, i);
            Boton s{{60 + i * (ancho + 12), 300, ancho, 80}, (si ? "● " : "") + etiquetaSello(k, i), nullptr, true, si};
            if (pasos)
                s.accion = [k, i, si] {
                    ponerPaso(yo(), k, i, !si);
                    cambia(si ? "Sello quitado" : "Sello puesto");
                };
            else
                s.accion = [] { avisar("Este sello sale al llegar a la cuenta: cambia la cuenta de arriba."); };
            v.b.push_back(s);
        }

        Boton f{{60, 410, 570, 90}, "Fechas de los sellos", [k] { fechas(k); }};
        f.detalle = "El día en que consiguió cada uno";
        v.b.push_back(f);
        Boton z{{650, 410, 570, 90}, "Empezar este récord de cero", [k] {
            borrarRecord(yo(), RECORDS[k].indice);
            cambia("Récord a cero");
        }};
        z.detalle = "Sin cuenta ni sellos";
        v.b.push_back(z);
        v.b.push_back({{440, ABAJO - 80, 400, 64}, "Volver", [] { volver(); }});
    });
}

void mas()
{
    abrir([](Vista& v) {
        v.titulo = "Récords de " + partida::jugador().nombre;
        v.explicacion = "Para todos los récords a la vez.";
        Boton a{{100, 170, 1080, 100}, "Completar todos", [] {
            confirmar("Completar todos", "¿Completar todos los récords y conseguir todos sus sellos?", "Completar", [] {
                completarRecords(yo(), jugador::hoy());
                volver();
                cambia("Récords completados");
            });
        }};
        a.detalle = "Todos los sellos, con fecha de hoy";
        Boton b{{100, 290, 1080, 100}, "Empezar todos de cero", [] {
            confirmar("Empezar todos de cero", "¿Poner todos los récords a cero y quitar todos los sellos?", "Poner a cero", [] {
                borrarRecords(yo());
                volver();
                cambia("Récords a cero");
            });
        }};
        b.detalle = "Sin cuentas ni sellos";
        v.b.push_back(a);
        v.b.push_back(b);
        v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
    });
}

Pantalla records()
{
    struct Estado {
        std::string busqueda;
        int filtro = 0; // 0 all, 1 in progress, 2 completed
        std::vector<int> vistas;
    };
    auto e = std::make_shared<Estado>();
    static const char* const FILTROS[3] = {"Todos", "En curso", "Completados"};
    Lista l;
    l.titulo = "Récords de millas Nook";
    l.explicacion = [e] {
        // Qué se ve con la búsqueda y el filtro (el filtro cambia con lo que se completa).
        e->vistas.clear();
        const std::string q = normalizar(e->busqueda);
        int completos = 0;
        for (int k = 0; k < NUM_RECORDS; k++)
        {
            const bool c = completo(k);
            completos += c;
            if ((e->filtro == 1 && c) || (e->filtro == 2 && !c))
                continue;
            if (!q.empty() && normalizar(RECORDS[k].nombre).find(q) == std::string::npos)
                continue;
            e->vistas.push_back(k);
        }
        std::string s = numero(completos) + " de " + numero(NUM_RECORDS) + " completos.";
        if (!e->busqueda.empty())
            s = "«" + e->busqueda + "»: " + s;
        return e->vistas.empty() ? s + " No hay ninguno que coincida." : s;
    };
    l.cuantas = [e] { return (int)e->vistas.size(); };
    l.columnas = 2;
    l.porColumna = 6;
    l.fila = [e](int i) {
        const int k = e->vistas[i];
        Fila f{RECORDS[k].nombre, [k] { record(k); }, true, completo(k)};
        f.detalle = recordPorPasos(k) ? sellos(k) : sellos(k) + " · " + numero(cuentaRecord(yo(), RECORDS[k].indice));
        return f;
    };
    l.abajo.push_back([e] {
        return Fila{"Buscar…", [e] {
            std::string q = e->busqueda;
            if (pedirTexto("Buscar un récord", "Parte del nombre (peces, nabos…). Vacío: todos", q))
                e->busqueda = q;
        }};
    });
    l.abajo.push_back([e] { return Fila{std::string("Ver: ") + FILTROS[e->filtro], [e] { e->filtro = (e->filtro + 1) % 3; }}; });
    l.abajo.push_back([] { return Fila{"Todos…", mas}; });
    return lista(l);
}

Registro r1({SeccionJugador, 60, [] { return std::string("Récords de millas"); },
    [] {
        int n = 0;
        for (int k = 0; k < NUM_RECORDS; k++)
            n += completo(k);
        return numero(n) + " de " + numero(NUM_RECORDS) + " completos";
    },
    [] { abrir(records()); }});

} // namespace
