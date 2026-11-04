// Sección «Jugador»: récords de millas Nook (Nook Miles+), como AchievementsWindow de NHSE para Mac: a la izquierda la
// lista (cada uno con sus sellos), a la derecha el elegido: la cuenta, «al máximo», los sellos, sus fechas y empezar de
// cero; abajo, completar o borrar todos.
#include <algorithm>
#include <memory>
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "dibujos.hpp"
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

// «1.234 → próximo sello al llegar a 2.000», como el Mac.
std::string progreso(int k)
{
    const Record& r = RECORDS[k];
    if (recordPorPasos(k))
        return sellos(k);
    const uint32_t n = cuentaRecord(yo(), r.indice);
    for (int i = 0; i < r.sellos && i < 5; i++)
        if (n < r.umbral[i])
            return numero(n) + " → próximo sello al llegar a " + numero(r.umbral[i]);
    return numero(n) + " · todos los sellos conseguidos";
}

std::string etiquetaSello(int k, int i) { return recordPorPasos(k) ? "Sello " + std::to_string(i + 1) : numero(RECORDS[k].umbral[i]); }

SDL_Texture* dibujoSellos(int k) { return jugador::sellos(sellosConseguidos(yo(), k), RECORDS[k].sellos); }

// Las fechas en que consiguió cada sello.
void fechas(int k)
{
    abrir([k](Vista& v) {
        const Record& r = RECORDS[k];
        v.titulo = r.nombre;
        v.explicacion = "El día en que consiguió cada sello. Toca uno para cambiarlo.";
        for (int i = 0; i < r.sellos; i++)
        {
            const bool si = selloConseguido(yo(), k, i);
            Boton b{{60 + (i % 2) * 590, 112 + (i / 2) * 100, 570, 88}, recordPorPasos(k) ? etiquetaSello(k, i) : "Sello de " + etiquetaSello(k, i), [k, i] {
                const int indice = RECORDS[k].indice;
                jugador::pedirFecha("Fecha del sello", fechaSello(yo(), indice, i), true, [indice, i](Fecha f) {
                    ponerFechaSello(yo(), indice, i, f);
                    cambia();
                });
            }};
            b.grande = true;
            b.imagen = jugador::sellos(si ? i + 1 : 0, r.sellos);
            b.detalle = textoFecha(fechaSello(yo(), r.indice, i)) + (si ? "" : " · aún no lo tiene");
            v.b.push_back(b);
        }
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
    });
}

Pantalla records()
{
    struct Estado {
        std::string busqueda;
        int filtro = 0; // 0 all, 1 in progress, 2 completed
        int pagina = 0;
        int elegido = 0; // k
        std::vector<int> vistas;
    };
    auto e = std::make_shared<Estado>();
    static const char* const FILTROS[3] = {"Todos", "En curso", "Completos"};
    return [e](Vista& v) {
        // Which are shown, with the search and the filter (the filter changes with what is completed).
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
        v.titulo = "Récords de millas Nook";
        v.explicacion = numero(completos) + " de " + numero(NUM_RECORDS) + " completos. Elige uno para ver cuánto lleva y sus sellos.";
        constexpr int POR_PAGINA = 6, ALTO = 66;
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        if (paginas > 1)
            v.explicacion = numero(completos) + " de " + numero(NUM_RECORDS) + " completos · página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ".";
        const int k = e->elegido;
        const Record& r = RECORDS[k];
        const bool pasos = recordPorPasos(k);

        // On the left, the list.
        const int desde = e->pagina * POR_PAGINA, hasta = std::min(total, desde + POR_PAGINA);
        for (int j = desde; j < hasta; j++)
        {
            const int kk = e->vistas[j];
            Boton b{{60, 164 + (j - desde) * (ALTO + 6), 580, ALTO}, RECORDS[kk].nombre, [e, kk] { e->elegido = kk; }, true, kk == k};
            b.imagen = dibujoSellos(kk);
            b.detalle = progreso(kk);
            v.b.push_back(b);
        }
        // On the right, the chosen one.
        const std::string titulo = r.nombre, nota = progreso(k), nadie = e->busqueda;
        const bool vacia = desde == hasta;
        v.dibujo = [titulo, nota, vacia, nadie] {
            if (vacia)
            {
                jugador::recuadro({60, 164, 580, 120});
                texto(nadie.empty() ? "No hay ninguno." : "Ninguno coincide con «" + nadie + "».", 350, 224, MARRON_SUAVE, g_pequena, Centro);
            }
            jugador::recuadro({660, 108, 560, 488});
            texto(encajar(titulo, 512, g_negrita), 684, 120, MARRON, g_negrita);
            texto(encajar(nota, 512, g_pequena), 684, 266, MARRON_SUAVE, g_pequena);
            texto("Sellos", 684, 302, MARRON, g_negrita);
        };
        if (!pasos)
        {
            Boton c{{684, 154, 300, 104}, "Cuenta", [k] {
                uint32_t n = cuentaRecord(yo(), RECORDS[k].indice);
                if (pedirNumero(RECORDS[k].nombre, n, 0xFFFFFFFFu))
                {
                    ponerCuentaRecord(yo(), RECORDS[k].indice, n);
                    cambia();
                }
            }};
            c.grande = true;
            c.valor = numero(cuentaRecord(yo(), r.indice));
            c.detalle = "Toca para cambiarla";
            v.b.push_back(c);
        }
        else
        {
            Boton c{{684, 154, 300, 104}, "Sellos", [] { avisar("Toca cada sello para ponerlo o quitarlo."); }};
            c.grande = true;
            c.valor = numero(sellosConseguidos(yo(), k)) + " de " + numero(r.sellos);
            c.detalle = "Por pasos, uno a uno";
            v.b.push_back(c);
        }
        Boton max{{1000, 178, 196, 56}, "Al máximo", [k] {
            completarRecord(yo(), k, jugador::hoy());
            cambia("Récord completado");
        }, !completo(k)};
        v.b.push_back(max);
        // The stamps: the ones it has, green. The step ones are put and taken by touching them.
        const int n = std::max<int>(r.sellos, 1);
        const int ancho = std::min(130, (512 - (n - 1) * 10) / n);
        for (int i = 0; i < r.sellos; i++)
        {
            const bool si = selloConseguido(yo(), k, i);
            Boton s{{684 + i * (ancho + 10), 340, ancho, 52}, etiquetaSello(k, i), nullptr, true, si};
            s.pequena = true;
            if (pasos)
                s.accion = [k, i, si] {
                    ponerPaso(yo(), k, i, !si);
                    cambia(si ? "Sello quitado" : "Sello puesto");
                };
            else
                s.accion = [] { avisar("Este sello sale al llegar a la cuenta: cambia la cuenta de arriba."); };
            v.b.push_back(s);
        }
        Boton f{{684, 410, 512, 76}, "Fechas de los sellos", [k] { fechas(k); }};
        f.grande = true;
        f.imagen = dibujos::menu("Timer");
        f.detalle = "El día en que consiguió cada uno";
        v.b.push_back(f);
        Boton z{{684, 504, 512, 60}, "Empezar este récord de cero", [k] {
            borrarRecord(yo(), RECORDS[k].indice);
            cambia("Récord a cero");
        }};
        z.pequena = true;
        v.b.push_back(z);

        // On top: the search and the filters.
        Boton bu{{60, 108, 0, 46}, e->busqueda.empty() ? "Buscar…" : "«" + e->busqueda + "»", [e] {
            auto vistas = std::make_shared<std::vector<int>>();
            Buscador b;
            b.titulo = "Buscar un récord";
            b.explicacion = "Por su nombre: peces, nabos, flores…";
            b.texto = e->busqueda;
            b.filtrar = [vistas](const std::string& t) {
                vistas->clear();
                const std::string q = normalizar(t);
                for (int k = 0; k < NUM_RECORDS; k++)
                    if (q.empty() || normalizar(RECORDS[k].nombre).find(q) != std::string::npos)
                        vistas->push_back(k);
                return (int)vistas->size();
            };
            b.fila = [e, vistas](int j) {
                const int k = (*vistas)[j];
                Fila f{RECORDS[k].nombre, [e, k] {
                    // Chosen: back to the list, with it.
                    e->busqueda.clear();
                    e->filtro = 0;
                    e->elegido = k;
                    e->pagina = k / POR_PAGINA;
                    volver();
                }, true, completo(k)};
                f.imagen = dibujoSellos(k);
                return f;
            };
            b.alSalir = [e](const std::string& t) {
                e->busqueda = t;
                e->pagina = 0;
            };
            abrir(buscador(b), 11);
        }};
        bu.pequena = true;
        v.b.push_back(bu);
        int anchos[3], suma = 0;
        for (int i = 0; i < 3; i++)
            suma += anchos[i] = anchoTexto(FILTROS[i], g_pequena) + 40;
        v.b.back().r.w = 580 - suma - 24;
        int x = 60 + v.b.back().r.w + 8;
        for (int i = 0; i < 3; i++)
        {
            const int w = anchos[i];
            Boton fb{{x, 108, w, 46}, FILTROS[i], [e, i] {
                e->filtro = i;
                e->pagina = 0;
            }, true, e->filtro == i};
            fb.pequena = true;
            v.b.push_back(fb);
            x += w + 8;
        }

        // At the bottom: pages, all of them, and back.
        int xa = 60;
        auto abajo = [&](const std::string& t, std::function<void()> f, bool activo = true) {
            const int w = anchoTexto(t, g_pequena) + 44;
            Boton b{{xa, ABAJO - 66, w, 56}, t, std::move(f), activo};
            b.pequena = true;
            v.b.push_back(b);
            xa += w + 12;
        };
        if (paginas > 1)
        {
            abajo("Anteriores", [e] { e->pagina--; }, e->pagina > 0);
            abajo("Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas);
        }
        abajo("Completar todos", [] {
            confirmar("Completar todos", "¿Completar todos los récords y conseguir todos sus sellos?", "Completar", [] {
                completarRecords(yo(), jugador::hoy());
                cambia("Récords completados");
            });
        });
        abajo("Todos a cero", [] {
            confirmar("Todos a cero", "¿Poner todos los récords a cero y quitar todos los sellos?", "Poner a cero", [] {
                borrarRecords(yo());
                cambia("Récords a cero");
            });
        });
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
        if (paginas > 1)
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        v.teclas = {{"A", "Elegir"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
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
