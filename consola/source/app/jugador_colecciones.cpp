// Sección «Jugador»: recetas, catálogo y reacciones, como RecipesWindow, CatalogWindow y ReactionsWindow de NHSE para
// Mac: cada lista con su dibujo y sus casillas, el buscador y los filtros; el catálogo por partes (cada una con su
// icono), y la rueda de reacciones dibujada como la del juego.
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include "../core/jugador.hpp"
#include "../core/jugador_listas.hpp"
#include "../core/mapa_objetos.hpp"
#include "jugador_comun.hpp"
#include "partida.hpp"
#include "dibujos.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;
using jugador::Casilla;
using jugador::Filtro;
using jugador::ListaCasillas;

namespace {

constexpr int NUM_RECETAS = sizeof(RECETAS_NOMBRES) / sizeof(RECETAS_NOMBRES[0]);
constexpr int NUM_CATALOGO = sizeof(CATALOGO_NOMBRES) / sizeof(CATALOGO_NOMBRES[0]);
constexpr int NUM_CATEGORIAS = sizeof(CATEGORIAS) / sizeof(CATEGORIAS[0]);
constexpr int NUM_REACCIONES = sizeof(REACCIONES_NOMBRES) / sizeof(REACCIONES_NOMBRES[0]);
constexpr int NUM_GRUPOS = sizeof(GRUPOS_REACCIONES) / sizeof(GRUPOS_REACCIONES[0]);

std::vector<uint8_t>& yo() { return partida::personal(); }

// ------------------------------------------------------------------------------------------------------- recetas
std::string nombreReceta(int k) { return std::string(RECETAS_NOMBRES[k].nombre) + (RECETAS_NOMBRES[k].historia ? " (de la historia)" : ""); }

// El dibujo de una receta: el del objeto que se fabrica (como el Mac).
SDL_Texture* dibujoReceta(int k)
{
    static std::map<uint16_t, uint16_t> objetos;
    if (objetos.empty())
        for (const auto& r : MAPA_RECETAS)
            objetos[r.receta] = r.objeto;
    const auto it = objetos.find(RECETAS_NOMBRES[k].indice);
    return it != objetos.end() ? dibujos::de(it->second) : dibujos::menu("BookRecipe");
}

bool sabe(int k) { return receta(yo(), RECETAS_NOMBRES[k].indice, Sabida); }

Pantalla recetas()
{
    ListaCasillas l;
    l.titulo = "Recetas de bricolaje";
    l.explicacion = "Marca «Aprendida» en las recetas que quieres tener en la app de bricolaje del teléfono.";
    l.total = NUM_RECETAS;
    l.nombre = nombreReceta;
    l.imagen = dibujoReceta;
    l.pista = "Busca una receta (mesa, hacha…)";
    Casilla aprendida{"Aprendida", 184, sabe, [](int k, bool si) {
        aprenderReceta(yo(), RECETAS_NOMBRES[k].indice, si);
        partida::cambiaPersonal();
        cambiado(si ? "Aprendida" : "Olvidada");
    }};
    // Fabricada y favorita, solo en las que sabe (como el Mac).
    auto marca = [](const char* que, int ancho, ListaReceta lr) {
        return Casilla{que, ancho, [lr](int k) { return receta(yo(), RECETAS_NOMBRES[k].indice, lr); }, [lr](int k, bool si) {
            ponerReceta(yo(), RECETAS_NOMBRES[k].indice, lr, si);
            partida::cambiaPersonal();
            cambiado();
        }, sabe};
    };
    l.casillas = {aprendida, marca("Fabricada", 184, Fabricada), marca("Favorita", 170, Favorita)};
    l.filtros = {{"Todas", nullptr}, {"Aprendidas", sabe}, {"Por aprender", [](int k) { return !sabe(k); }},
        {"Favoritas", [](int k) { return receta(yo(), RECETAS_NOMBRES[k].indice, Favorita); }}};
    l.resumen = [] {
        const Progreso p = acnh::recetas(yo());
        return numero(p.tiene) + " de " + numero(p.total) + " aprendidas";
    };
    l.arriba.push_back([] {
        const Progreso p = acnh::recetas(yo());
        return Fila{"Aprender todas", [] {
            const int n = aprenderRecetas(yo());
            partida::cambiaPersonal();
            cambiado(numero(n) + " recetas nuevas");
        }, p.tiene < p.total};
    });
    l.arriba.push_back([] {
        const Progreso p = acnh::recetas(yo());
        return Fila{"Olvidar todas", [] {
            confirmar("Olvidar todas las recetas", "¿Olvidar todas las recetas? Tendrá que volver a aprenderlas.", "Olvidar", [] {
                olvidarRecetas(yo());
                partida::cambiaPersonal();
                cambiado("Recetas olvidadas");
            });
        }, p.tiene > 0};
    });
    l.abajo.push_back([] {
        const Progreso p = acnh::recetas(yo());
        return Fila{"Marcar las aprendidas como fabricadas", [] {
            const int n = fabricarSabidas(yo());
            partida::cambiaPersonal();
            cambiado(numero(n) + " marcadas como fabricadas");
        }, p.tiene > 0};
    });
    return jugador::listaCasillas(l);
}

// ------------------------------------------------------------------------------------------------------ catálogo
const char* iconoCategoria(int c)
{
    static const char* const ICONOS[] = {"Book", "Cardboard", "Wall", "Onepiece", "Axe", "DIYWoodNormal", "Dishes", "PltCosmos0",
        "FishAji", "InsMonshirocho", "Tako", "Fossil", "Haniwa", "Present"};
    return c >= 0 && c < (int)(sizeof(ICONOS) / sizeof(ICONOS[0])) ? ICONOS[c] : "Book";
}

// Una parte del catálogo (0: todo), con el dibujo de cada cosa.
void catalogoDe(int categoria)
{
    auto ks = std::make_shared<std::vector<int>>();
    for (int k = 0; k < NUM_CATALOGO; k++)
        if (categoria == 0 || CATALOGO_NOMBRES[k].categoria == categoria)
            ks->push_back(k);
    ListaCasillas l;
    l.titulo = std::string("Catálogo: ") + CATEGORIAS[categoria];
    l.explicacion = "Lo recibido se puede volver a pedir en Nook Shopping (con todos sus colores).";
    l.total = (int)ks->size();
    l.nombre = [ks](int i) { return std::string(CATALOGO_NOMBRES[(*ks)[i]].nombre); };
    if (categoria == 0)
        l.nota = [ks](int i) { return std::string(CATEGORIAS[CATALOGO_NOMBRES[(*ks)[i]].categoria]); };
    l.imagen = [ks](int i) { return dibujos::de(CATALOGO_NOMBRES[(*ks)[i]].id); };
    l.columnas = 2;
    l.pista = "Busca por nombre (manzana, sofá…)";
    auto tiene = [ks](int i) { return recibido(yo(), CATALOGO_NOMBRES[(*ks)[i]].id); };
    l.casillas = {Casilla{"Recibido", 160, tiene, [ks](int i, bool si) {
        recibir(yo(), (*ks)[i], si);
        partida::cambiaPersonal();
        cambiado(si ? "En el catálogo" : "Quitado del catálogo");
    }}};
    l.filtros = {{"Todo", nullptr}, {"Recibidos", tiene}, {"Sin recibir", [tiene](int i) { return !tiene(i); }}};
    l.resumen = [ks] {
        int n = 0;
        for (int k : *ks)
            n += recibido(yo(), CATALOGO_NOMBRES[k].id);
        return numero(n) + " de " + numero((uint32_t)ks->size()) + " recibidos";
    };
    l.arriba.push_back([ks] {
        return Fila{"Marcar todos", [ks] {
            int n = 0;
            for (int k : *ks)
                if (!recibido(yo(), CATALOGO_NOMBRES[k].id))
                    recibir(yo(), k, true), n++;
            partida::cambiaPersonal();
            cambiado(numero(n) + " marcados");
        }};
    });
    l.arriba.push_back([ks] {
        return Fila{"Desmarcar todos", [ks] {
            confirmar("Desmarcar todos", "¿Quitar del catálogo las " + numero((uint32_t)ks->size()) + " cosas de esta parte?", "Desmarcar", [ks] {
                for (int k : *ks)
                    if (recibido(yo(), CATALOGO_NOMBRES[k].id))
                        recibir(yo(), k, false);
                partida::cambiaPersonal();
                cambiado("Desmarcados");
            });
        }};
    });
    abrir(jugador::listaCasillas(l));
}

// Las partes del catálogo, cada una con su icono y lo que tiene de ella.
void catalogo(Vista& v)
{
    v.titulo = "Catálogo";
    v.explicacion = "Lo que se puede volver a pedir en Nook Shopping. Elige una parte para verla.";
    // What there is of each part (only every second: it is long).
    static std::vector<std::pair<int, int>> cuenta;
    static uint32_t cuando = 0;
    if (cuenta.empty() || SDL_GetTicks() - cuando > 1000)
    {
        cuando = SDL_GetTicks();
        cuenta.assign(NUM_CATEGORIAS, {0, 0});
        const auto& p = yo();
        for (const auto& c : CATALOGO_NOMBRES)
        {
            const bool si = recibido(p, c.id);
            cuenta[c.categoria].first += si, cuenta[c.categoria].second++;
            cuenta[0].first += si, cuenta[0].second++;
        }
    }
    // «Todo», big on top; the rest, in three columns.
    {
        Boton b{{60, 112, 1160, 92}, "Todo el catálogo", [] { catalogoDe(0); }};
        b.grande = true;
        b.imagen = dibujos::menu(iconoCategoria(0));
        b.detalle = numero(cuenta[0].first) + " de " + numero(cuenta[0].second) + " recibidos";
        b.marcado = cuenta[0].first == cuenta[0].second;
        v.b.push_back(b);
    }
    for (int i = 1; i < NUM_CATEGORIAS; i++)
    {
        const int k = i - 1;
        Boton b{{60 + (k % 3) * 392, 220 + (k / 3) * 76, 376, 68}, CATEGORIAS[i], [i] { catalogoDe(i); }};
        b.grande = true;
        b.imagen = dibujos::menu(iconoCategoria(i));
        b.detalle = numero(cuenta[i].first) + " de " + numero(cuenta[i].second);
        b.marcado = cuenta[i].second > 0 && cuenta[i].first == cuenta[i].second;
        v.b.push_back(b);
    }
    v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
}

// ---------------------------------------------------------------------------------------------------- reacciones
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

// La rueda como en el juego: un círculo con las ocho reacciones alrededor (la primera arriba, en el sentido del reloj).
constexpr int RUEDA_X = 640, RUEDA_Y = 362, RUEDA_R = 200;

SDL_Point huecoRueda(int i)
{
    static const float SENO[8] = {0, 0.7071f, 1, 0.7071f, 0, -0.7071f, -1, -0.7071f};
    static const float COSENO[8] = {1, 0.7071f, 0, -0.7071f, -1, -0.7071f, 0, 0.7071f};
    return {RUEDA_X + (int)(RUEDA_R * SENO[i]), RUEDA_Y - (int)(RUEDA_R * COSENO[i])};
}

void ruedaReacciones(Vista& v)
{
    v.titulo = "Rueda de reacciones";
    v.explicacion = "Las ocho reacciones que tiene a mano en el juego. Toca un hueco para elegir cuál va en él.";
    v.dibujo = [] {
        // The wheel: a round board with the eight places on its edge and, in the middle, the heart of the reactions.
        jugador::elipse(RUEDA_X, RUEDA_Y, 242, 242, jugador::BORDE_MAC);
        jugador::elipse(RUEDA_X, RUEDA_Y, 239, 239, {0xFF, 0xF4, 0xD6, 255});
        jugador::elipse(RUEDA_X, RUEDA_Y, 150, 150, {0xF6, 0xE3, 0xA8, 255});
        // The eight spokes, from the middle to each place.
        for (int i = 0; i < NUM_RUEDA; i++)
        {
            const SDL_Point c = huecoRueda(i);
            const float dx = (float)(c.x - RUEDA_X), dy = (float)(c.y - RUEDA_Y), l = std::sqrt(dx * dx + dy * dy);
            const float nx = -dy / l * 4, ny = dx / l * 4;
            poligono({{RUEDA_X + nx, RUEDA_Y + ny}, {c.x + nx, c.y + ny}, {c.x - nx, c.y - ny}, {RUEDA_X - nx, RUEDA_Y - ny}}, {0xEA, 0xCF, 0x84, 255});
        }
        jugador::elipse(RUEDA_X, RUEDA_Y, 82, 82, jugador::BORDE_MAC);
        jugador::elipse(RUEDA_X, RUEDA_Y, 79, 79, BLANCO);
        jugador::imagen(dibujos::menu("Love"), RUEDA_X - 32, RUEDA_Y - 46, 64);
        texto("Reacciones", RUEDA_X, RUEDA_Y + 34, MARRON_SUAVE, g_pequena, Centro);
    };
    for (int i = 0; i < NUM_RUEDA; i++)
    {
        const SDL_Point c = huecoRueda(i);
        const uint8_t r = rueda(yo(), i);
        Boton b{{c.x - 100, c.y - 28, 200, 56}, nombreEnRueda(r), [i] { elegirRueda(i); }};
        b.detalle = "Hueco " + std::to_string(i + 1);
        v.b.push_back(b);
    }
    v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
}

void aprenderUna(int i, bool si)
{
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
}

// Las reacciones por grupos, con su casilla, y a la derecha lo que sabe y la rueda (como el Mac).
Pantalla reacciones()
{
    struct Estado {
        int grupo = 0;
        int pagina = 0;
    };
    auto e = std::make_shared<Estado>();
    return [e](Vista& v) {
        v.titulo = "Reacciones";
        v.explicacion = "Marca las reacciones que sabe hacer y elige las ocho de la rueda.";
        std::vector<int> delGrupo;
        for (int i = 0; i < NUM_REACCIONES; i++)
            if (REACCIONES_NOMBRES[i].grupo == e->grupo)
                delGrupo.push_back(i);
        // The groups, as tabs.
        int x = 60;
        for (int g = 0; g < NUM_GRUPOS; g++)
        {
            const int w = anchoTexto(GRUPOS_REACCIONES[g], g_pequena) + 48;
            Boton b{{x, 108, w, 46}, GRUPOS_REACCIONES[g], [e, g] {
                e->grupo = g;
                e->pagina = 0;
            }, true, e->grupo == g};
            b.pequena = true;
            v.b.push_back(b);
            x += w + 10;
        }
        // The reactions of the group: three columns (two if some name is long), in pages.
        static int columnas[NUM_GRUPOS] = {};
        if (!columnas[e->grupo])
        {
            columnas[e->grupo] = 3;
            for (int i : delGrupo)
                if (anchoTexto(REACCIONES_NOMBRES[i].nombre, g_pequena) > 230 - 74)
                    columnas[e->grupo] = 2;
        }
        const int COLS = columnas[e->grupo], FILAS = 7, POR_PAGINA = COLS * FILAS;
        const int ANCHO = COLS == 3 ? 230 : 350;
        const int paginas = std::max(1, ((int)delGrupo.size() + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        const int desde = e->pagina * POR_PAGINA, hasta = std::min((int)delGrupo.size(), desde + POR_PAGINA);
        const int filas = std::min(FILAS, (hasta - desde + COLS - 1) / COLS);
        const Progreso p = acnh::reacciones(yo());
        std::string sabidas[NUM_RUEDA];
        for (int i = 0; i < NUM_RUEDA; i++)
            sabidas[i] = nombreEnRueda(rueda(yo(), i));
        const std::string grupo = GRUPOS_REACCIONES[e->grupo] + (paginas > 1 ? " (" + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ")" : "");
        for (int k = desde; k < hasta; k++)
        {
            const int i = delGrupo[k], pos = k - desde;
            const bool si = sabeReaccion(yo(), REACCIONES_NOMBRES[i].valor);
            v.b.push_back(jugador::interruptor({76 + (pos % COLS) * (ANCHO + 10), 216 + (pos / COLS) * 52, ANCHO, 46}, REACCIONES_NOMBRES[i].nombre, si,
                [i, si] { aprenderUna(i, !si); }));
        }
        // On the right: how many it knows, and the wheel.
        v.dibujo = [filas, grupo, p, sabidas] {
            jugador::recuadro({60, 164, 744, 64 + filas * 52});
            texto(grupo, 84, 172, MARRON, g_negrita);
            jugador::recuadro({824, 108, 396, 132});
            texto(numero(p.tiene) + " de " + numero(p.total), 848, 114, MARRON, g_titulo);
            texto("aprendidas", 848 + anchoTexto(numero(p.tiene) + " de " + numero(p.total), g_titulo) + 10, 130, MARRON_SUAVE, g_pequena);
            jugador::recuadro({824, 256, 396, 340});
            texto("Rueda de reacciones", 848, 266, MARRON, g_negrita);
            texto("Las ocho que tiene a mano.", 848, 300, MARRON_SUAVE, g_pequena);
            for (int i = 0; i < NUM_RUEDA; i++)
            {
                const int y = 334 + (i % 4) * 34, x = 848 + (i / 4) * 180;
                texto(std::to_string(i + 1), x, y, MARRON_SUAVE, g_pequena);
                texto(encajar(sabidas[i], 150, g_pequena), x + 20, y, MARRON, g_pequena);
            }
        };
        Boton todas{{848, 176, 220, 50}, "Aprender todas", [] {
            const int n = aprenderReacciones(yo());
            partida::cambiaPersonal();
            cambiado(numero(n) + " reacciones nuevas");
        }, p.tiene < p.total};
        todas.principal = p.tiene < p.total;
        todas.pequena = true;
        v.b.push_back(todas);
        Boton r{{848, 482, 348, 96}, "Cambiar la rueda", [] { abrir(ruedaReacciones); }};
        r.grande = true;
        r.imagen = dibujos::menu("Love");
        r.detalle = "Como en el juego";
        v.b.push_back(r);
        if (paginas > 1)
        {
            v.b.push_back({{60, ABAJO - 66, 180, 56}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            v.b.push_back({{252, ABAJO - 66, 180, 56}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        }
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
        v.teclas = {{"A", "Marcar"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}

Registro r1({SeccionJugador, 40, [] { return std::string("Recetas"); },
    [] {
        const Progreso p = acnh::recetas(yo());
        return "Sabe " + numero(p.tiene) + " de " + numero(p.total);
    },
    [] { abrir(recetas()); }});

Registro r2({SeccionJugador, 45, [] { return std::string("Catálogo"); },
    [] {
        const Progreso p = acnh::catalogo(yo());
        return "Tiene " + numero(p.tiene) + " de " + numero(p.total);
    },
    [] { abrir(catalogo); }});

Registro r3({SeccionJugador, 50, [] { return std::string("Reacciones"); },
    [] {
        const Progreso p = acnh::reacciones(yo());
        return "Sabe " + numero(p.tiene) + " de " + numero(p.total) + " · rueda";
    },
    [] { abrir(reacciones()); }});

} // namespace
