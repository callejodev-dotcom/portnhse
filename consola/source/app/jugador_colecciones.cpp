// Sección «Jugador»: recetas, catálogo y reacciones una a una (con buscar, marcar y desmarcar), y la rueda de
// reacciones. Como RecipesWindow, CatalogWindow y ReactionsWindow de NHSE para Mac.
#include <algorithm>
#include <memory>
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "jugador_comun.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int NUM_RECETAS = sizeof(RECETAS_NOMBRES) / sizeof(RECETAS_NOMBRES[0]);
constexpr int NUM_CATALOGO = sizeof(CATALOGO_NOMBRES) / sizeof(CATALOGO_NOMBRES[0]);
constexpr int NUM_CATEGORIAS = sizeof(CATEGORIAS) / sizeof(CATEGORIAS[0]);
constexpr int NUM_REACCIONES = sizeof(REACCIONES_NOMBRES) / sizeof(REACCIONES_NOMBRES[0]);

std::vector<uint8_t>& yo() { return partida::personal(); }

// ------------------------------------------------------------------------------------------------------- recetas
std::string nombreReceta(int k) { return std::string(RECETAS_NOMBRES[k].nombre) + (RECETAS_NOMBRES[k].historia ? " (de la historia)" : ""); }

// Las aprendidas: aprender u olvidar una a una.
void recetasAprendidas()
{
    Marcas m;
    m.titulo = "Recetas de " + partida::jugador().nombre;
    m.total = NUM_RECETAS;
    m.nombre = nombreReceta;
    m.marcada = [](int k) { return receta(yo(), RECETAS_NOMBRES[k].indice, Sabida); };
    m.marcar = [](int k, bool si) {
        aprenderReceta(yo(), RECETAS_NOMBRES[k].indice, si);
        partida::cambiaPersonal();
        cambiado(si ? "Aprendida" : "Olvidada");
    };
    m.queEs = "recetas";
    abrir(marcas(m));
}

// Fabricadas o favoritas, de las que ya sabe (como el Mac, que solo las deja marcar en las aprendidas).
void recetasMarca(ListaReceta l)
{
    auto sabidas = std::make_shared<std::vector<int>>();
    for (int k = 0; k < NUM_RECETAS; k++)
        if (receta(yo(), RECETAS_NOMBRES[k].indice, Sabida))
            sabidas->push_back(k);
    if (sabidas->empty())
    {
        avisar("Todavía no sabe ninguna receta.", true);
        return;
    }
    Marcas m;
    m.titulo = l == Fabricada ? "Recetas ya fabricadas" : "Recetas favoritas";
    m.total = (int)sabidas->size();
    m.nombre = [sabidas](int i) { return nombreReceta((*sabidas)[i]); };
    m.marcada = [sabidas, l](int i) { return receta(yo(), RECETAS_NOMBRES[(*sabidas)[i]].indice, l); };
    m.marcar = [sabidas, l](int i, bool si) {
        ponerReceta(yo(), RECETAS_NOMBRES[(*sabidas)[i]].indice, l, si);
        partida::cambiaPersonal();
        cambiado();
    };
    m.queEs = "recetas";
    abrir(marcas(m));
}

void recetas(Vista& v)
{
    v.titulo = "Recetas de " + partida::jugador().nombre;
    const Progreso p = acnh::recetas(yo());
    v.explicacion = "Sabe " + numero(p.tiene) + " de " + numero(p.total) + " recetas de bricolaje.";
    auto fila = [&](int i, int col, const std::string& t, const std::string& d, std::function<void()> f, bool activo = true) {
        Boton b{{60 + col * 590, 160 + i * 105, 570, 90}, t, std::move(f), activo};
        b.detalle = d;
        v.b.push_back(b);
    };
    fila(0, 0, "Aprendidas, una a una", "Buscar, aprender u olvidar", recetasAprendidas);
    fila(1, 0, "Fabricadas", "Las que ya ha hecho alguna vez", [] { recetasMarca(Fabricada); });
    fila(2, 0, "Favoritas", "Las marcadas con una estrella", [] { recetasMarca(Favorita); });
    fila(0, 1, "Aprender todas", "Las nuevas salen como nuevas", [] {
        const int n = aprenderRecetas(yo());
        partida::cambiaPersonal();
        cambiado(numero(n) + " recetas nuevas");
    }, p.tiene < p.total);
    fila(1, 1, "Marcar las aprendidas como fabricadas", "Como si ya las hubiera hecho", [] {
        const int n = fabricarSabidas(yo());
        partida::cambiaPersonal();
        cambiado(numero(n) + " marcadas como fabricadas");
    }, p.tiene > 0);
    fila(2, 1, "Olvidar todas", "Tendrá que volver a aprenderlas", [] {
        confirmar("Olvidar todas las recetas", "¿Olvidar todas las recetas? Tendrá que volver a aprenderlas.", "Olvidar", [] {
            olvidarRecetas(yo());
            partida::cambiaPersonal();
            cambiado("Recetas olvidadas");
        });
    }, p.tiene > 0);
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
}

// ------------------------------------------------------------------------------------------------------ catálogo
// Una categoría del catálogo (0: todo), una a una.
void catalogoDe(int categoria)
{
    auto ks = std::make_shared<std::vector<int>>();
    for (int k = 0; k < NUM_CATALOGO; k++)
        if (categoria == 0 || CATALOGO_NOMBRES[k].categoria == categoria)
            ks->push_back(k);
    Marcas m;
    m.titulo = std::string("Catálogo: ") + CATEGORIAS[categoria];
    m.total = (int)ks->size();
    m.nombre = [ks](int i) { return std::string(CATALOGO_NOMBRES[(*ks)[i]].nombre); };
    m.marcada = [ks](int i) { return recibido(yo(), CATALOGO_NOMBRES[(*ks)[i]].id); };
    m.marcar = [ks](int i, bool si) {
        recibir(yo(), (*ks)[i], si);
        partida::cambiaPersonal();
        cambiado(si ? "En el catálogo" : "Quitado del catálogo");
    };
    m.queEs = "cosas";
    abrir(marcas(m));
}

Pantalla catalogo()
{
    auto cuenta = std::make_shared<std::vector<std::pair<int, int>>>(NUM_CATEGORIAS); // received, total
    Lista l;
    l.titulo = "Catálogo de " + partida::jugador().nombre;
    l.explicacion = [cuenta] {
        // Lo que tiene de cada categoría (una pasada por fotograma).
        std::fill(cuenta->begin(), cuenta->end(), std::make_pair(0, 0));
        const auto& p = yo();
        for (const auto& c : CATALOGO_NOMBRES)
        {
            const bool si = recibido(p, c.id);
            (*cuenta)[c.categoria].first += si, (*cuenta)[c.categoria].second++;
            (*cuenta)[0].first += si, (*cuenta)[0].second++;
        }
        return "Lo que se puede pedir en Nook Shopping. Elige qué parte quieres ver.";
    };
    l.cuantas = [] { return NUM_CATEGORIAS; };
    l.columnas = 2;
    l.porColumna = 7;
    l.fila = [cuenta](int i) {
        const auto [tiene, total] = (*cuenta)[i];
        Fila f{CATEGORIAS[i], [i] { catalogoDe(i); }, true, total > 0 && tiene == total};
        f.detalle = "Tiene " + numero(tiene) + " de " + numero(total);
        return f;
    };
    return lista(l);
}

// ---------------------------------------------------------------------------------------------------- reacciones
void reaccionesUnaAUna()
{
    Marcas m;
    m.titulo = "Reacciones de " + partida::jugador().nombre;
    m.total = NUM_REACCIONES;
    m.nombre = [](int i) { return std::string(REACCIONES_NOMBRES[i].nombre); };
    m.marcada = [](int i) { return sabeReaccion(yo(), REACCIONES_NOMBRES[i].valor); };
    m.marcar = [](int i, bool si) {
        const uint8_t r = REACCIONES_NOMBRES[i].valor;
        if (si && !aprenderReaccion(yo(), r))
        {
            avisar("En esta partida no caben más reacciones.", true);
            return;
        }
        if (!si)
            olvidarReaccion(yo(), r);
        partida::cambiaPersonal();
        cambiado(si ? "Aprendida" : "Olvidada");
    };
    m.queEs = "reacciones";
    abrir(marcas(m));
}

std::string nombreEnRueda(uint8_t r)
{
    if (r == 0)
        return "(vacío)";
    const char* n = nombreReaccion(r);
    return n ? n : "Reacción " + std::to_string(r);
}

// Elegir la reacción de un hueco de la rueda: de las que sabe, por nombre (o dejarlo vacío).
void elegirRueda(int hueco)
{
    std::vector<uint8_t> valores{0};
    std::vector<std::pair<std::string, uint8_t>> sabidas;
    for (const auto& r : REACCIONES_NOMBRES)
        if (sabeReaccion(yo(), r.valor))
            sabidas.push_back({r.nombre, r.valor});
    std::sort(sabidas.begin(), sabidas.end(), [](const auto& a, const auto& b) { return normalizar(a.first) < normalizar(b.first); });
    std::vector<std::string> nombres{"(vacío)"};
    const uint8_t ahora = rueda(yo(), hueco);
    int actual = ahora == 0 ? 0 : -1;
    for (const auto& [n, r] : sabidas)
    {
        if (r == ahora)
            actual = (int)nombres.size();
        nombres.push_back(n);
        valores.push_back(r);
    }
    // Si tiene una que ya no sabe, que siga saliendo.
    if (actual < 0)
    {
        actual = (int)nombres.size();
        nombres.push_back(nombreEnRueda(ahora));
        valores.push_back(ahora);
    }
    elegir("Hueco " + std::to_string(hueco + 1) + " de la rueda", "Elige una de las reacciones que sabe.", nombres, actual,
        [hueco, valores](int i) {
            ponerRueda(yo(), hueco, valores[i]);
            partida::cambiaPersonal();
            cambiado();
        }, 3);
}

void ruedaReacciones(Vista& v)
{
    v.titulo = "Rueda de reacciones";
    v.explicacion = "Las ocho reacciones que tiene a mano. Toca un hueco para elegir cuál va en él.";
    for (int i = 0; i < NUM_RUEDA; i++)
    {
        Boton b{{60 + (i % 2) * 590, 160 + (i / 2) * 100, 570, 86}, nombreEnRueda(rueda(yo(), i)), [i] { elegirRueda(i); }};
        b.detalle = "Hueco " + std::to_string(i + 1);
        v.b.push_back(b);
    }
    v.b.push_back({{440, ABAJO - 80, 400, 64}, "Volver", [] { volver(); }});
}

void reacciones(Vista& v)
{
    v.titulo = "Reacciones de " + partida::jugador().nombre;
    const Progreso p = acnh::reacciones(yo());
    v.explicacion = "Sabe " + numero(p.tiene) + " de " + numero(p.total) + " reacciones.";
    Boton a{{100, 160, 1080, 90}, "Las que sabe, una a una", [] { reaccionesUnaAUna(); }};
    a.detalle = "Buscar, aprender u olvidar";
    Boton b{{100, 265, 1080, 90}, "Rueda de reacciones", [] { abrir(ruedaReacciones); }};
    b.detalle = "Elegir las ocho que tiene a mano";
    Boton c{{100, 370, 1080, 90}, "Aprender todas", [] {
        const int n = aprenderReacciones(yo());
        partida::cambiaPersonal();
        cambiado(numero(n) + " reacciones nuevas");
    }, p.tiene < p.total};
    c.detalle = p.tiene < p.total ? "Las que le faltan" : "Ya las sabe todas";
    v.b.push_back(a);
    v.b.push_back(b);
    v.b.push_back(c);
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
}

Registro r1({SeccionJugador, 40, [] { return std::string("Recetas"); },
    [] {
        const Progreso p = acnh::recetas(yo());
        return "Sabe " + numero(p.tiene) + " de " + numero(p.total);
    },
    [] { abrir(recetas); }});

Registro r2({SeccionJugador, 45, [] { return std::string("Catálogo"); },
    [] {
        const Progreso p = acnh::catalogo(yo());
        return "Tiene " + numero(p.tiene) + " de " + numero(p.total);
    },
    [] { abrir(catalogo()); }});

Registro r3({SeccionJugador, 50, [] { return std::string("Reacciones"); },
    [] {
        const Progreso p = acnh::reacciones(yo());
        return "Sabe " + numero(p.tiene) + " de " + numero(p.total) + " · rueda";
    },
    [] { abrir(reacciones); }});

} // namespace
