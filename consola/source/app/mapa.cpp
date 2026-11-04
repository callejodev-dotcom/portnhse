// Sección «Isla»: el mapa de la isla, como el de NHSE para Mac (IslandMapWindow): la isla dibujada con sus edificios y
// todo lo que hay en el suelo; un cursor que se mueve por cuadros (cruceta o palanca) o tocando, con acercar y alejar;
// ver qué hay en un sitio, quitarlo, girarlo, moverlo o cambiarlo por otra cosa; las dos capas (en el suelo y encima de
// otras cosas); marcar una zona; la goma; y ordenar la isla (quitar o cambiar todas las cosas de un tipo o de un nombre,
// regar). Cada cambio se puede deshacer.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <memory>
#include "../core/mapa.hpp"
#include "dibujos.hpp"
#include "mapa_panel.hpp"
#include "mapa_pintor.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace ui;
using namespace mapa;

namespace {

const SDL_Rect AREA = {0, 96, 880, ABAJO - 96};     // donde se ve el mapa
constexpr int PANEL_X = 880;                         // el panel de la derecha
constexpr double ZOOM_MAX = 32;
const char* const CAPAS[2] = {"En el suelo", "Encima de otras cosas"};
const char* const CAPAS_MIN[2] = {"en el suelo", "encima de otras cosas"};

// Lo que se puede quitar de golpe (como «Ordenar la isla» del Mac), con su nombre.
struct TipoConNombre {
    Tipo tipo;
    const char* nombre;
    const char* minusculas;
    uint16_t icono; // el objeto de su dibujo (los del Mac: Weed, Turip0, Branch...)
};
const TipoConNombre TIPOS[] = {{Hierbas, "Malas hierbas", "malas hierbas", 758}, {Flores, "Flores", "flores", 2883}, {Ramas, "Ramas", "ramas", 14644},
    {Conchas, "Conchas", "conchas", 1376}, {Hoyos, "Agujeros", "agujeros", 773}, {Vallas, "Vallas", "vallas", 3080},
    {Arbustos, "Arbustos", "arbustos", 2820}, {Arboles, "Árboles", "árboles", 2796}, {Cosas, "Muebles y objetos", "muebles y objetos", 14414},
    {Todo, "Todo", "cosas", 4645}};

uint16_t iconoTipo(Tipo t)
{
    for (const auto& k : TIPOS)
        if (k.tipo == t)
            return k.icono;
    return 0;
}

enum Modo { Mirar, Llevar, Marcar, Goma };

struct Estado {
    int capa = 0;
    int cx = 0, cy = 0;          // el cursor: casilla del mapa entero (siempre par: un cuadro)
    double zoom = 3, vx = 0, vy = 0;
    bool ajustado = false;
    int fotogramas = 0;
    Modo modo = Mirar;
    // Llevando una cosa a otro sitio: de dónde (casilla de la capa) y cuánto se separa del cursor.
    Objeto llevado;
    int desdeX = 0, desdeY = 0, dx = 0, dy = 0;
    // Marcando una zona: la primera esquina; y la zona marcada (casillas de la capa).
    bool esquina = false;
    int ex = 0, ey = 0;
    bool hayZona = false;
    Zona zona;
    // La goma.
    int goma = 1;                // cuadros de ancho
    Tipo gomaTipo = Flores;
    uint64_t gomaClave = 0;
    std::string gomaNombre;
    bool borrando = false;
    int borradas = 0;
    bool cuadricula = false;
    // Tocar y arrastrar.
    bool tocando = false, arrastrado = false;
    int t0x = 0, t0y = 0;
    double v0x = 0, v0y = 0;
    std::string aviso;
};

std::shared_ptr<Estado> g_estado;
std::vector<std::vector<uint8_t>> g_deshacer; // las dos capas antes de cada cambio
constexpr size_t MAX_DESHACER = 12;
int g_version = 0; // sube con cada cambio: lo que se cuenta en las pantallas se vuelve a contar
bool g_tocado = false; // se ha cambiado algo desde que se abrió el mapa

// Lo que está encima de otras cosas sin nada debajo (se cuenta una vez por cambio).
const std::vector<Punto>& sinApoyoAhora()
{
    static int version = -1;
    static const uint8_t* de = nullptr;
    static std::vector<Punto> v;
    if (version != g_version || de != partida::principal().data())
        v = sinApoyo(partida::principal()), version = g_version, de = partida::principal().data();
    return v;
}

std::vector<uint8_t>& principal() { return partida::principal(); }
Rejilla capaActiva(Estado& e) { return mapa::capa(principal(), e.capa); }

void recordar()
{
    g_version++;
    g_tocado = true;
    const uint8_t* p = principal().data() + CAMPO;
    g_deshacer.emplace_back(p, p + 2 * TAM_CAPA);
    if (g_deshacer.size() > MAX_DESHACER)
        g_deshacer.erase(g_deshacer.begin());
}

void olvidarUltimo()
{
    if (!g_deshacer.empty())
        g_deshacer.pop_back();
}

void deshacer()
{
    if (g_deshacer.empty())
    {
        avisar("No hay nada que deshacer.");
        return;
    }
    g_version++;
    std::copy(g_deshacer.back().begin(), g_deshacer.back().end(), principal().begin() + CAMPO);
    g_deshacer.pop_back();
    partida::cambiaPrincipal();
    cambiado("Deshecho");
}

void hecho(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

// ------------------------------------------------------------------------------------------------------- la vista
double zoomMin() { return std::max(0.5, std::min((double)AREA.w / MAPA_ANCHO, (double)AREA.h / MAPA_ALTO) * 0.9); }

void ajustar(Estado& e)
{
    const double vw = AREA.w / e.zoom, vh = AREA.h / e.zoom;
    e.vx = MAPA_ANCHO <= vw ? (MAPA_ANCHO - vw) / 2 : std::clamp(e.vx, 0.0, MAPA_ANCHO - vw);
    e.vy = MAPA_ALTO <= vh ? (MAPA_ALTO - vh) / 2 : std::clamp(e.vy, 0.0, MAPA_ALTO - vh);
}

void verZona(Estado& e, double x, double y, double w, double h)
{
    e.zoom = std::clamp(std::min(AREA.w / w, AREA.h / h), zoomMin(), ZOOM_MAX);
    e.vx = x + w / 2 - AREA.w / e.zoom / 2;
    e.vy = y + h / 2 - AREA.h / e.zoom / 2;
    ajustar(e);
}

void verIsla(Estado& e) { verZona(e, 26, 26, MAPA_ANCHO - 52, MAPA_ALTO - 52); }

void acercar(Estado& e, double f)
{
    const double mx = e.vx + AREA.w / e.zoom / 2, my = e.vy + AREA.h / e.zoom / 2;
    // Alrededor del cursor si se ve; si no, del centro.
    double ax = mx, ay = my;
    if (e.cx + 1 >= e.vx && e.cx + 1 <= e.vx + AREA.w / e.zoom && e.cy + 1 >= e.vy && e.cy + 1 <= e.vy + AREA.h / e.zoom)
        ax = e.cx + 1, ay = e.cy + 1;
    const double sx = (ax - e.vx) * e.zoom, sy = (ay - e.vy) * e.zoom;
    e.zoom = std::clamp(e.zoom * f, zoomMin(), ZOOM_MAX);
    e.vx = ax - sx / e.zoom;
    e.vy = ay - sy / e.zoom;
    ajustar(e);
}

// Que el cursor se vea, con un poco de margen.
void seguir(Estado& e)
{
    const double vw = AREA.w / e.zoom, vh = AREA.h / e.zoom, m = std::min(4.0, vw / 6);
    if (e.cx < e.vx + m)
        e.vx = e.cx - m;
    if (e.cx + 2 > e.vx + vw - m)
        e.vx = e.cx + 2 - vw + m;
    if (e.cy < e.vy + m)
        e.vy = e.cy - m;
    if (e.cy + 2 > e.vy + vh - m)
        e.vy = e.cy + 2 - vh + m;
    ajustar(e);
}

int pantallaX(const Estado& e, double x) { return (int)std::lround(AREA.x + (x - e.vx) * e.zoom); }
int pantallaY(const Estado& e, double y) { return (int)std::lround(AREA.y + (y - e.vy) * e.zoom); }
SDL_Rect pantalla(const Estado& e, double x, double y, double w, double h)
{
    const int a = pantallaX(e, x), b = pantallaY(e, y);
    return {a, b, pantallaX(e, x + w) - a, pantallaY(e, y + h) - b};
}

// ----------------------------------------------------------------------------------------------- lo que hay
const std::vector<Edificio>& losEdificios()
{
    static std::vector<Edificio> v;
    static const uint8_t* de = nullptr;
    if (de != principal().data() || v.empty())
    {
        std::vector<std::string> nombres;
        for (const auto& j : partida::jugadores)
            nombres.push_back(j.nombre);
        v = edificios(principal(), nombres);
        de = principal().data();
        auto le32 = [](const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); };
        ponerEdificios(v, le32(principal().data() + PLAZA_X), le32(principal().data() + PLAZA_Z));
    }
    return v;
}

const Edificio* edificioEn(double x, double y)
{
    for (const auto& b : losEdificios())
        if (!b.cruce() && b.contiene(x, y))
            return &b;
    return nullptr;
}

// La cosa del cursor en la capa que se edita: su raíz (casilla de la capa), o false.
bool cosaEn(Estado& e, int mx, int my, Objeto& o, int& rx, int& ry)
{
    Rejilla r = capaActiva(e);
    const int x = mx - ORIGEN_X, y = my - ORIGEN_Y;
    if (!r.dentro(x, y))
        return false;
    // Cualquiera de las cuatro casillas del cuadro.
    for (int k = 0; k < 4; k++)
        if (raiz(r, x + (k & 1), y + (k >> 1), o, rx, ry))
            return true;
    return false;
}

std::string tipoNombre(Tipo t, bool minusculas = false)
{
    for (const auto& k : TIPOS)
        if (k.tipo == t)
            return minusculas ? k.minusculas : k.nombre;
    return "";
}

// Vuelve al mapa desde una pantalla que está «nivel» pantallas por encima.
void alMapa(int nivel)
{
    for (int i = 0; i < nivel; i++)
        volver();
}

// ----------------------------------------------------------------------------------------------- pantallas
void abrirOrdenar(std::shared_ptr<Estado> e, bool enZona, int nivel);
void abrirClases(std::shared_ptr<Estado> e, bool enZona, int nivel);

// Lo que se hace con una clase de cosas (todas las que se llaman igual). «nivel»: a cuántas pantallas del mapa está.
void abrirClase(std::shared_ptr<Estado> e, Clase k, bool enZona, int nivel)
{
    const std::string n = nombre(k.ejemplo);
    auto cuenta = std::make_shared<std::pair<int, int>>(-1, 0);
    abrir([e, k, n, enZona, nivel, cuenta](Vista& v) {
        const Zona z = enZona ? e->zona : Zona{};
        if (cuenta->first != g_version)
            *cuenta = {g_version, (int)buscarNombre(capaActiva(*e), z, k.clave).size()};
        const int cuantas = cuenta->second;
        v.titulo = n;
        v.explicacion = "Hay " + numero(cuantas) + (enZona ? " en la zona marcada." : " en la isla (" + std::string(CAPAS_MIN[e->capa]) + ").");
        v.b.push_back({{140, 200, 1000, 90}, "Quitar todas (" + numero(cuantas) + ")", [e, k, n, z, cuantas] {
            confirmar("Quitar", "¿Quitar «" + n + "» (" + numero(cuantas) + ")" + (z.w ? " de la zona marcada" : " de toda la isla") + "? Se puede deshacer.", "Sí, quitar", [e, k, z] {
                recordar();
                Rejilla r = capaActiva(*e);
                const int q = quitarNombre(r, k.clave, z);
                volver();
                hecho("Quitadas " + numero(q));
            });
        }, cuantas > 0});
        v.b.push_back({{140, 310, 1000, 90}, "Goma solo para estas", [e, k, n, nivel] {
            e->gomaTipo = Todo;
            e->gomaClave = k.clave;
            e->gomaNombre = n;
            e->modo = Goma;
            alMapa(nivel);
        }});
        v.b.push_back({{440, ABAJO - 70, 400, 56}, "Volver", [] { volver(); }});
    });
}

// Cada cosa por separado, por su nombre (las que más hay primero).
void abrirClases(std::shared_ptr<Estado> e, bool enZona, int nivel)
{
    struct Cache {
        int version = -1;
        std::string busqueda;
        std::vector<Clase> v;
    };
    auto c = std::make_shared<Cache>();
    auto busca = std::make_shared<std::string>();
    auto lista_ = [e, c, enZona, busca]() -> const std::vector<Clase>& {
        if (c->version != g_version || c->busqueda != *busca)
        {
            // Las de esa capa y zona; con algo buscado, solo las que lo dicen en su nombre.
            std::vector<Clase> todas = clases(capaActiva(*e), enZona ? e->zona : Zona{});
            c->v.clear();
            const std::string q = acnh::normalizar(*busca);
            for (const auto& k : todas)
                if (q.empty() || acnh::normalizar(nombre(k.ejemplo)).find(q) != std::string::npos)
                    c->v.push_back(k);
            c->version = g_version, c->busqueda = *busca;
        }
        return c->v;
    };
    if (lista_().empty())
    {
        avisar(enZona ? "No hay nada en la zona marcada." : "No hay nada en esta capa.");
        return;
    }
    Lista l;
    l.titulo = enZona ? "Lo que hay en la zona" : "Lo que hay en la isla";
    l.explicacion = [lista_, busca] {
        return (busca->empty() ? "Cada cosa por su nombre (" : "Las que se llaman «" + *busca + "» (") + numero((uint32_t)lista_().size()) +
               "). Elige una para quitarlas todas o borrarlas con la goma. X para buscar.";
    };
    l.cuantas = [lista_] { return (int)lista_().size(); };
    l.porColumna = 7;
    l.fila = [e, lista_, enZona, nivel](int i) {
        const Clase k = lista_()[i];
        Fila f{nombre(k.ejemplo) + " ×" + numero(k.cuantas), [e, k, enZona, nivel] { abrirClase(e, k, enZona, nivel + 1); }};
        f.imagen = dibujoDe(k.ejemplo);
        if (!f.imagen)
            f.color = colorDe(k.ejemplo);
        return f;
    };
    auto buscar = [busca] {
        std::string q = *busca;
        if (pedirTexto("Buscar", "Escribe parte del nombre (vacío: todas)", q))
            *busca = q;
    };
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    abrir(lista(l));
}

// Ordenar: cada tipo con cuántas hay y botones para quitarlas (o regar, cambiar por otras, tapar).
void abrirOrdenar(std::shared_ptr<Estado> e, bool enZona, int nivel)
{
    struct Cache {
        int version = -1, capa = -1;
        std::vector<std::pair<Tipo, int>> filas;
    };
    auto c = std::make_shared<Cache>();
    abrir([e, enZona, nivel, c](Vista& v) {
        const Zona z = enZona ? e->zona : Zona{};
        const std::string donde = enZona ? "la zona marcada" : "toda la isla";
        v.titulo = enZona ? "Zona de " + std::to_string(z.w / 2) + " × " + std::to_string(z.h / 2) + " cuadros" : "Ordenar la isla";
        v.explicacion = std::string(enZona ? "Lo que hay en la zona marcada" : "Todo de golpe, en toda la isla") + " (" + CAPAS_MIN[e->capa] + "). Se puede deshacer.";
        if (c->version != g_version || c->capa != e->capa)
        {
            Rejilla r = capaActiva(*e);
            c->filas.clear();
            for (const auto& k : TIPOS)
            {
                if (k.tipo == Todo && !enZona)
                    continue;
                const int n = contar(r, k.tipo, z);
                if (n == 0 && k.tipo != Hierbas && k.tipo != Flores)
                    continue;
                c->filas.push_back({k.tipo, n});
            }
            c->version = g_version, c->capa = e->capa;
        }
        const auto& filas = c->filas;
        auto quitarTipo = [e, z, donde](Tipo t, int n) {
            confirmar("Quitar", "¿Quitar " + numero(n) + " " + tipoNombre(t, true) + " de " + donde + "? Se puede deshacer.", "Sí, quitar", [e, z, t] {
                recordar();
                Rejilla r = capaActiva(*e);
                const int q = quitarTodos(r, t, z);
                hecho("Quitadas " + numero(q));
            });
        };
        // Las flores en una fila entera (tienen más botones); lo demás de dos en dos.
        struct Etiqueta {
            SDL_Rect r;
            std::string nombre, cuantas;
            uint16_t icono;
        };
        auto etiquetas = std::make_shared<std::vector<Etiqueta>>();
        int k = 0;
        for (const auto& f : filas)
        {
            const Tipo t = f.first;
            const int n = f.second;
            const bool flores = t == Flores;
            const int col = flores ? 0 : k % 2, fila = flores ? 0 : 1 + k / 2;
            if (!flores)
                k++;
            const int x = 60 + col * 590, y = 140 + fila * 72;
            etiquetas->push_back({{x, y, 270, 60}, tipoNombre(t), numero(n), iconoTipo(t)});
            const std::string accion = t == Hoyos ? "Tapar" : "Quitar";
            if (flores)
            {
                v.b.push_back({{340, y, 280, 60}, "Regar", [e, z] {
                    recordar();
                    Rejilla r = capaActiva(*e);
                    const int q = regar(r, z);
                    if (!q)
                        olvidarUltimo();
                    hecho("Regadas " + numero(q) + " flores");
                }, n > 0});
                v.b.push_back({{630, y, 300, 60}, "Cambiar por…", [e, z, donde, n] {
                    elegirPlantas([e, z, donde, n](const std::vector<uint16_t>& ids, bool azar) {
                        std::string nombres;
                        for (auto id : ids)
                        {
                            Objeto o = nada();
                            o.id = id;
                            nombres += (nombres.empty() ? "" : ", ") + nombre(o);
                        }
                        confirmar("Cambiar flores", "¿Cambiar las flores de " + donde + " (" + numero(n) + ") por " + nombres + "? Se puede deshacer.", "Sí, cambiar", [e, z, ids, azar] {
                            recordar();
                            Rejilla r = capaActiva(*e);
                            const int q = cambiarTodos(r, Flores, ids, azar, z, (uint32_t)time(nullptr));
                            if (!q)
                                olvidarUltimo();
                            hecho("Cambiadas " + numero(q));
                        });
                    }, "cambiar las flores");
                }, n > 0});
                v.b.push_back({{940, y, 280, 60}, accion, [quitarTipo, t, n] { quitarTipo(t, n); }, n > 0});
            }
            else
                v.b.push_back({{x + 280, y, 290, 60}, accion, [quitarTipo, t, n] { quitarTipo(t, n); }, n > 0});
        }
        v.dibujo = [etiquetas] {
            for (const auto& t : *etiquetas)
            {
                redondo(t.r, 12, BLANCO);
                int x = t.r.x + 18;
                if (SDL_Texture* d = dibujos::de(t.icono))
                {
                    SDL_Rect r = {t.r.x + 10, t.r.y + 12, 36, 36};
                    SDL_RenderCopy(g_r, d, nullptr, &r);
                    x = t.r.x + 54;
                }
                const int libre = t.r.x + t.r.w - 24 - anchoTexto(t.cuantas) - x;
                texto(encajar(t.nombre, libre, g_pequena), x, t.r.y + 17, MARRON, g_pequena);
                texto(t.cuantas, t.r.x + t.r.w - 16, t.r.y + 12, VERDE_OSCURO, nullptr, Derecha);
            }
        };
        // Abajo: cada cosa por separado, deshacer, olvidar la zona y volver.
        std::vector<Boton> abajo;
        abajo.push_back({{}, "Por nombres…", [e, enZona, nivel] { abrirClases(e, enZona, nivel + 1); }});
        abajo.push_back({{}, "Deshacer" + (g_deshacer.empty() ? std::string() : " (" + std::to_string(g_deshacer.size()) + ")"),
            [] { deshacer(); }, !g_deshacer.empty()});
        if (enZona)
            abajo.push_back({{}, "Olvidar la zona", [e] {
                e->hayZona = false;
                volver();
            }});
        abajo.push_back({{}, "Volver", [] { volver(); }});
        const int ancho = (1160 - 16 * ((int)abajo.size() - 1)) / (int)abajo.size();
        for (size_t i = 0; i < abajo.size(); i++)
        {
            abajo[i].r = {60 + (int)i * (ancho + 16), ABAJO - 70, ancho, 56};
            v.b.push_back(abajo[i]);
        }
    });
}

// La goma: de qué tamaño y qué borra.
void abrirGoma(std::shared_ptr<Estado> e, int nivel)
{
    abrir([e, nivel](Vista& v) {
        v.titulo = "Goma";
        v.explicacion = "Mantén A y mueve el cursor por encima de lo que quieras quitar. Las cosas grandes se quitan enteras. Cada pasada se puede deshacer.";
        for (int i = 0; i < 3; i++)
        {
            const int n = i * 2 + 1;
            Boton b{{60 + i * 200, 190, 180, 70}, std::to_string(n) + "×" + std::to_string(n), [e, n] { e->goma = n; }};
            b.marcado = e->goma == n;
            v.b.push_back(b);
        }
        v.dibujo = [] { texto("Tamaño en cuadros", 680, 210, MARRON); };
        const int nt = sizeof(TIPOS) / sizeof(TIPOS[0]);
        int k = 0;
        if (e->gomaClave)
        {
            Boton b{{60, 290, 370, 60}, "Solo: " + e->gomaNombre, [] {}};
            b.marcado = true;
            v.b.push_back(b);
            k++;
        }
        for (int i = 0; i < nt; i++, k++)
        {
            const Tipo t = TIPOS[i].tipo;
            Boton b{{60 + (k % 3) * 390, 290 + (k / 3) * 70, 370, 60}, TIPOS[i].nombre, [e, t] {
                e->gomaTipo = t;
                e->gomaClave = 0;
            }};
            b.marcado = !e->gomaClave && e->gomaTipo == t;
            v.b.push_back(b);
        }
        v.b.push_back({{60, ABAJO - 70, 560, 56}, "Usar la goma", [e, nivel] {
            e->modo = Goma;
            alMapa(nivel);
        }});
        v.b.push_back({{660, ABAJO - 70, 560, 56}, "Volver", [] { volver(); }});
    });
}

// Herramientas (Y): zona, goma, ordenar, deshacer, cuadrícula.
void abrirHerramientas(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        v.titulo = "Herramientas del mapa";
        v.explicacion = std::string("Trabajando en: ") + CAPAS[e->capa] + ".";
        std::vector<Boton> bs;
        if (e->hayZona)
            bs.push_back({{}, "Lo que hay en la zona marcada…", [e] { abrirOrdenar(e, true, 2); }});
        bs.push_back({{}, "Marcar una zona", [e] {
            e->modo = Marcar;
            e->esquina = false;
            volver();
        }});
        bs.push_back({{}, "Goma…", [e] { abrirGoma(e, 2); }});
        bs.push_back({{}, "Ordenar toda la isla…", [e] { abrirOrdenar(e, false, 2); }});
        bs.push_back({{}, "Cada cosa por separado…", [e] { abrirClases(e, false, 2); }});
        bs.push_back({{}, std::string("Capa: ") + CAPAS[e->capa], [e] { e->capa = 1 - e->capa; }});
        bs.push_back({{}, std::string("Cuadrícula: ") + (e->cuadricula ? "sí" : "no"), [e] { e->cuadricula = !e->cuadricula; }});
        bs.push_back({{}, "Ver toda la isla", [e] {
            verIsla(*e);
            volver();
        }});
        bs.push_back({{}, "Deshacer" + (g_deshacer.empty() ? std::string() : " (" + std::to_string(g_deshacer.size()) + ")"), [] { deshacer(); }, !g_deshacer.empty()});
        const auto& solas = sinApoyoAhora();
        if (!solas.empty())
            bs.push_back({{}, "Encima de nada: " + numero((uint32_t)solas.size()), [e] {
                // Al primero, en la capa de encima (lo que no tiene nada debajo).
                const Punto p = sinApoyoAhora()[0];
                e->capa = 1;
                e->cx = (p.x + ORIGEN_X) & ~1, e->cy = (p.y + ORIGEN_Y) & ~1;
                e->zoom = std::max(e->zoom, 8.0);
                seguir(*e);
                volver();
                avisar("Esto está encima de otra cosa, pero debajo no hay nada: en el juego puede quedar flotando.");
            }});
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 150 + (int)(i / 2) * 84, 570, 70};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 70, 400, 56}, "Volver", [] { volver(); }});
    });
}

// Qué hay en el cursor (A): la cosa de la capa, o el edificio, o el cuadro vacío.
void abrirSitio(std::shared_ptr<Estado> e)
{
    Objeto o;
    int rx, ry;
    const bool hay = cosaEn(*e, e->cx, e->cy, o, rx, ry);
    if (!hay)
    {
        if (const Edificio* b = edificioEn(e->cx + 1, e->cy + 1))
        {
            const std::string n = b->casa() && b->nombre != "Parcela libre" ? "Casa de " + b->nombre : b->nombre;
            const uint16_t icono = iconoDeEdificio(b->tipo);
            abrir([n, icono](Vista& v) {
                v.titulo = n;
                v.explicacion = "Este sitio es de un edificio. Los edificios se cambian de sitio en el propio juego, en los Servicios a residentes.";
                v.dibujo = [n, icono] {
                    redondo({60, 170, 1160, 140}, 16, BLANCO);
                    redondo({84, 186, 108, 108}, 54, CREMA);
                    if (SDL_Texture* d = icono ? dibujos::de(icono) : nullptr)
                    {
                        SDL_Rect r = {104, 206, 68, 68};
                        SDL_RenderCopy(g_r, d, nullptr, &r);
                    }
                    texto(encajar(n, 980, g_grande), 216, 214, MARRON, g_grande);
                };
                v.b.push_back({{440, ABAJO - 70, 400, 56}, "Volver", [] { volver(); }});
            });
            return;
        }
        Rejilla r = capaActiva(*e);
        if (!r.dentro(e->cx - ORIGEN_X, e->cy - ORIGEN_Y))
        {
            avisar("Ahí no se puede poner nada: está fuera de la isla.");
            return;
        }
        rx = e->cx - ORIGEN_X, ry = e->cy - ORIGEN_Y;
    }
    auto pos = std::make_shared<std::pair<int, int>>(rx, ry);
    Sitio s;
    s.donde = CAPAS[e->capa];
    s.posicion = [pos] { return std::make_pair(pos->first + ORIGEN_X, pos->second + ORIGEN_Y); };
    s.plantas = e->capa == 0;
    s.objeto = [e, pos] {
        Rejilla r = capaActiva(*e);
        const Objeto o = r.leer(pos->first, pos->second);
        return esRaiz(o) ? o : nada();
    };
    s.aviso = [e] { return e->aviso; };
    e->aviso.clear();
    s.poner = [e, pos](const Objeto& n) {
        // Lo que no es planta ni mueble queda tirado en el suelo, como al soltarlo en el juego.
        Objeto c = n;
        if (e->capa == 0 && !esPlanta(c) && !esNada(c) && !girable(c) && c.sistema == 0)
            c.sistema |= 0x20;
        recordar();
        Rejilla r = capaActiva(*e);
        Objeto estorbo;
        int x, y;
        const Resultado res = mapa::poner(r, c, pos->first, pos->second, &estorbo, &x, &y);
        if (res != Bien)
        {
            olvidarUltimo();
            e->aviso = noCabe(res, estorbo);
            avisar(e->aviso, true);
            return false;
        }
        e->aviso.clear();
        *pos = {x, y};
        partida::cambiaPrincipal();
        return true;
    };
    s.quitar = [e, pos] {
        recordar();
        Rejilla r = capaActiva(*e);
        quitar(r, pos->first, pos->second);
        partida::cambiaPrincipal();
    };
    if (hay && girable(o))
        s.girar = [e, pos] {
            recordar();
            Rejilla r = capaActiva(*e);
            Objeto estorbo;
            const Resultado res = girar(r, pos->first, pos->second, &estorbo);
            if (res != Bien)
            {
                olvidarUltimo();
                e->aviso = noCabe(res, estorbo);
                avisar(e->aviso, true);
                return;
            }
            e->aviso.clear();
            hecho("Girado");
        };
    if (hay)
    {
        s.mover = [e, pos] {
            Rejilla r = capaActiva(*e);
            e->llevado = r.leer(pos->first, pos->second);
            e->desdeX = pos->first, e->desdeY = pos->second;
            e->cx = (pos->first + ORIGEN_X) & ~1, e->cy = (pos->second + ORIGEN_Y) & ~1;
            e->dx = pos->first + ORIGEN_X - e->cx, e->dy = pos->second + ORIGEN_Y - e->cy;
            e->modo = Llevar;
        };
        const uint64_t clave = claveNombre(o);
        const int iguales = (int)buscarNombre(capaActiva(*e), {}, clave).size();
        const std::string n = nombre(o);
        if (iguales >= 2)
        {
            s.mas.push_back({"Quitar todas iguales (" + numero(iguales) + ")", [e, clave, n, iguales] {
                confirmar("Quitar", "¿Quitar «" + n + "» (" + numero(iguales) + ") de toda la isla? Se puede deshacer.", "Sí, quitar", [e, clave] {
                    recordar();
                    Rejilla r = capaActiva(*e);
                    const int q = quitarNombre(r, clave);
                    volver();
                    hecho("Quitadas " + numero(q));
                });
            }});
            s.mas.push_back({"Goma solo para estas", [e, clave, n] {
                e->gomaTipo = Todo;
                e->gomaClave = clave;
                e->gomaNombre = n;
                e->modo = Goma;
                volver();
            }});
        }
    }
    abrir(panelSitio(s));
}

// ---------------------------------------------------------------------------------------------- el mapa
// ¿Cabe lo que se lleva con la esquina ahí? (lo que ocupa ella misma no cuenta)
bool cabeLlevado(Estado& e, int x, int y)
{
    Rejilla r = capaActiva(e);
    int w, h;
    tamano(e.llevado, w, h);
    if (!r.dentro(x, y) || x + w > r.ancho || y + h > r.alto)
        return false;
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h; j++)
        {
            Objeto o;
            int rx, ry;
            if (raiz(r, x + i, y + j, o, rx, ry) && !(rx == e.desdeX && ry == e.desdeY))
                return false;
        }
    return true;
}

// La zona de la goma en el cursor (casillas del mapa).
SDL_Rect pincel(const Estado& e)
{
    const int mitad = e.goma / 2 * 2;
    return {e.cx - mitad, e.cy - mitad, e.goma * 2, e.goma * 2};
}

void borrarAqui(Estado& e)
{
    const SDL_Rect p = pincel(e);
    Rejilla r = capaActiva(e);
    const int n = borrar(r, {p.x - ORIGEN_X, p.y - ORIGEN_Y, p.w, p.h}, e.gomaTipo, e.gomaClave);
    if (n)
    {
        e.borradas += n;
        partida::cambiaPrincipal();
    }
}

void terminarGoma(Estado& e)
{
    if (!e.borrando)
        return;
    e.borrando = false;
    if (e.borradas == 0)
        olvidarUltimo();
    else
        cambiado("Quitadas " + numero(e.borradas));
}

void pulsarA(std::shared_ptr<Estado> e)
{
    switch (e->modo)
    {
        case Mirar:
            e->hayZona = false;
            abrirSitio(e);
            break;
        case Llevar:
        {
            Rejilla r = capaActiva(*e);
            const int x = e->cx + e->dx - ORIGEN_X, y = e->cy + e->dy - ORIGEN_Y;
            recordar();
            Objeto estorbo;
            const Resultado res = mover(r, e->desdeX, e->desdeY, x, y, &estorbo);
            if (res != Bien)
            {
                olvidarUltimo();
                avisar(noCabe(res, estorbo), true);
                return;
            }
            e->modo = Mirar;
            hecho("Movido: " + nombre(e->llevado));
            break;
        }
        case Marcar:
            if (!e->esquina)
            {
                e->esquina = true;
                e->ex = e->cx, e->ey = e->cy;
                avisar("Ahora mueve el cursor a la otra esquina y pulsa A.");
            }
            else
            {
                const int x0 = std::max(0, std::min(e->ex, e->cx)), y0 = std::max(ORIGEN_Y, std::min(e->ey, e->cy));
                const int x1 = std::min(MAPA_ANCHO, std::max(e->ex, e->cx) + 2), y1 = std::min(ORIGEN_Y + ALTO, std::max(e->ey, e->cy) + 2);
                e->esquina = false;
                e->modo = Mirar;
                if (x1 <= x0 || y1 <= y0)
                {
                    avisar("Esa zona está fuera de la isla.", true);
                    return;
                }
                e->hayZona = true;
                e->zona = {x0 - ORIGEN_X, y0 - ORIGEN_Y, x1 - x0, y1 - y0};
                avisar("");
                abrirOrdenar(e, true, 1);
            }
            break;
        case Goma:
            break;
    }
}

void moverCursor(Estado& e, int dx, int dy)
{
    // Lejos se mueve de varios cuadros a la vez.
    const int paso = std::max(1, (int)std::lround(12 / (e.zoom * 2)));
    e.cx = std::clamp(e.cx + dx * paso * 2, 0, MAPA_ANCHO - 2);
    e.cy = std::clamp(e.cy + dy * paso * 2, 0, MAPA_ALTO - 2);
    seguir(e);
}

bool entrada(std::shared_ptr<Estado> e, u64 down, u64 held, int tx, int ty)
{
    Estado& s = *e;
    if (!lienzo().listo())
        return true;
    int dx = 0, dy = 0;
    if (down & HidNpadButton_AnyLeft) dx = -1;
    if (down & HidNpadButton_AnyRight) dx = 1;
    if (down & HidNpadButton_AnyUp) dy = -1;
    if (down & HidNpadButton_AnyDown) dy = 1;
    if (dx || dy)
        moverCursor(s, dx, dy);

    // Tocar: un toque elige el cuadro (y otro toque en el mismo, lo abre); arrastrar mueve el mapa.
    HidTouchScreenState ts = {};
    hidGetTouchScreenStates(&ts, 1);
    const bool toca = ts.count > 0;
    const int px = toca ? (int)ts.touches[0].x : 0, py = toca ? (int)ts.touches[0].y : 0;
    if (toca && !s.tocando)
    {
        if (px >= AREA.x && px < AREA.x + AREA.w && py >= AREA.y && py < AREA.y + AREA.h)
        {
            s.tocando = true;
            s.arrastrado = false;
            s.t0x = px, s.t0y = py, s.v0x = s.vx, s.v0y = s.vy;
        }
    }
    else if (toca && s.tocando)
    {
        if (std::abs(px - s.t0x) + std::abs(py - s.t0y) > 12)
            s.arrastrado = true;
        if (s.arrastrado)
        {
            s.vx = s.v0x - (px - s.t0x) / s.zoom;
            s.vy = s.v0y - (py - s.t0y) / s.zoom;
            ajustar(s);
        }
    }
    else if (!toca && s.tocando)
    {
        s.tocando = false;
        if (!s.arrastrado)
        {
            const int mx = ((int)std::floor(s.vx + (s.t0x - AREA.x) / s.zoom)) & ~1;
            int my = ((int)std::floor(s.vy + (s.t0y - AREA.y) / s.zoom)) & ~1;
            // Los árboles se dibujan más altos que su cuadro: tocar su copa también los elige.
            Objeto o;
            int rx, ry;
            if (!cosaEn(s, mx, my, o, rx, ry) && cosaEn(s, mx, my + 2, o, rx, ry) && clasePlanta(o.id) == 3)
                my += 2;
            const bool mismo = mx == s.cx && my == s.cy;
            s.cx = std::clamp(mx, 0, MAPA_ANCHO - 2), s.cy = std::clamp(my, 0, MAPA_ALTO - 2);
            if (mismo && s.modo != Goma)
                pulsarA(e);
        }
    }
    // Los botones del panel (solo al tocar).
    if (tx >= PANEL_X)
    {
        const int i = (ty - 420) / 56;
        if (ty >= 420 && i >= 0 && i < 4)
        {
            if (i == 0) pulsarA(e);
            if (i == 1) s.capa = 1 - s.capa;
            if (i == 2)
            {
                if (s.modo == Goma)
                    abrirGoma(e, 1);
                else
                    abrirHerramientas(e);
            }
            if (i == 3) deshacer();
        }
        if (ty >= 360 && ty < 410)
            acercar(s, tx < PANEL_X + 200 ? 1 / 1.5 : 1.5);
        return true;
    }

    if (s.modo == Goma)
    {
        if ((down & HidNpadButton_A) || ((held & HidNpadButton_A) && !s.borrando))
        {
            recordar();
            s.borrando = true;
            s.borradas = 0;
            borrarAqui(s);
        }
        else if ((held & HidNpadButton_A) && s.borrando && (dx || dy))
            borrarAqui(s);
        else if (!(held & HidNpadButton_A))
            terminarGoma(s);
        return false; // B, X, Y, L y R los mira la vista
    }
    if (down & HidNpadButton_A)
        pulsarA(e);
    return false;
}

void dibujarMapa(std::shared_ptr<Estado> e)
{
    Estado& s = *e;
    if (!lienzo().listo())
    {
        rect(AREA, {70, 170, 204, 255});
        texto("Dibujando la isla…", AREA.x + AREA.w / 2, AREA.y + AREA.h / 2, BLANCO, g_grande, Centro);
        if (s.fotogramas++ >= 1)
        {
            losEdificios();
            lienzo().preparar(principal());
        }
        return;
    }
    losEdificios();
    lienzo().preparar(principal());
    if (!s.ajustado)
    {
        // Empieza delante de la casa del jugador, con toda la isla a la vista.
        for (const auto& b : losEdificios())
            if (b.tipo == 1 + partida::yo)
                s.cx = ((int)b.x) & ~1, s.cy = ((int)(b.y + b.h / 2) + 4) & ~1;
        verIsla(s);
        s.ajustado = true;
    }
    lienzo().actualizar(principal(), s.capa);
    lienzo().dibujar(principal(), AREA, s.vx, s.vy, s.zoom);

    SDL_RenderSetClipRect(g_r, &AREA);
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_BLEND);
    if (s.cuadricula && s.zoom * 2 >= 5)
    {
        SDL_SetRenderDrawColor(g_r, 255, 255, 255, (uint8_t)std::clamp(s.zoom * 8, 30.0, 80.0));
        for (int x = ((int)s.vx) & ~1; x <= s.vx + AREA.w / s.zoom; x += 2)
            linea(pantallaX(s, x), AREA.y, pantallaX(s, x), AREA.y + AREA.h);
        for (int y = ((int)s.vy) & ~1; y <= s.vy + AREA.h / s.zoom; y += 2)
            linea(AREA.x, pantallaY(s, y), AREA.x + AREA.w, pantallaY(s, y));
    }
    // Los nombres de los edificios (las casas solo si caben).
    std::vector<SDL_Rect> puestos;
    for (int pasada = 0; pasada < 2; pasada++)
        for (const auto& b : losEdificios())
        {
            if (b.cruce() || b.nombre.empty() || (b.casa() != (pasada == 1)))
                continue;
            if (b.casa() && b.w * s.zoom < 46)
                continue;
            const int ax = pantallaX(s, b.x), ay = pantallaY(s, b.y + b.h / 2);
            if (ax < AREA.x - 80 || ax > AREA.x + AREA.w + 80 || ay < AREA.y - 40 || ay > AREA.y + AREA.h + 40)
                continue;
            const int w = anchoTexto(b.nombre, g_pequena) + 14;
            const SDL_Rect pill = {ax - w / 2, ay + 2, w, 28};
            bool choca = false;
            for (const auto& p : puestos)
                choca = choca || SDL_HasIntersection(&p, &pill);
            if (choca)
                continue;
            puestos.push_back(pill);
            redondo(pill, 14, {255, 251, 240, 235});
            texto(b.nombre, ax, ay + 16, {58, 51, 42, 255}, g_pequena, Centro);
        }
    // La zona marcada.
    auto marco = [](SDL_Rect r, SDL_Color c, int grosor) {
        SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
        for (int i = 0; i < grosor; i++)
        {
            SDL_Rect q = {r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i};
            borde(q);
        }
    };
    if (s.hayZona)
    {
        const SDL_Rect z = pantalla(s, s.zona.x + ORIGEN_X, s.zona.y + ORIGEN_Y, s.zona.w, s.zona.h);
        SDL_SetRenderDrawColor(g_r, 255, 255, 255, 0x30);
        SDL_RenderFillRect(g_r, &z);
        marco(z, BLANCO, 2);
    }
    if (s.modo == Marcar && s.esquina)
    {
        const int x0 = std::min(s.ex, s.cx), y0 = std::min(s.ey, s.cy);
        const SDL_Rect z = pantalla(s, x0, y0, std::abs(s.ex - s.cx) + 2, std::abs(s.ey - s.cy) + 2);
        SDL_SetRenderDrawColor(g_r, 255, 255, 255, 0x30);
        SDL_RenderFillRect(g_r, &z);
        marco(z, BLANCO, 2);
        texto(std::to_string(std::abs(s.ex - s.cx) / 2 + 1) + " × " + std::to_string(std::abs(s.ey - s.cy) / 2 + 1), z.x + 6, z.y + 4, BLANCO, g_pequena);
    }
    // El cursor: lo que hay, el edificio o el cuadro.
    if (s.modo == Llevar)
    {
        int w, h;
        tamano(s.llevado, w, h);
        const int x = s.cx + s.dx, y = s.cy + s.dy;
        const bool cabe = cabeLlevado(s, x - ORIGEN_X, y - ORIGEN_Y);
        const SDL_Rect r = pantalla(s, x, y, w, h);
        const SDL_Color c = cabe ? SDL_Color{0x2F, 0x9E, 0x62, 255} : SDL_Color{0xD0, 0x40, 0x40, 255};
        if (SDL_Texture* d = dibujoDe(s.llevado))
        {
            SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, 0x50);
            SDL_RenderFillRect(g_r, &r);
            SDL_SetTextureAlphaMod(d, 0xD8);
            const int l = std::min(r.w, r.h);
            SDL_Rect q = {r.x + (r.w - l) / 2, r.y + (r.h - l) / 2, l, l};
            SDL_RenderCopy(g_r, d, nullptr, &q);
            SDL_SetTextureAlphaMod(d, 255);
        }
        else
        {
            SDL_Color f = colorDe(s.llevado);
            f.a = 200;
            SDL_SetRenderDrawColor(g_r, f.r, f.g, f.b, f.a);
            SDL_RenderFillRect(g_r, &r);
        }
        marco(r, c, 3);
    }
    else if (s.modo == Goma)
    {
        const SDL_Rect p = pincel(s);
        const SDL_Rect r = pantalla(s, p.x, p.y, p.w, p.h);
        SDL_SetRenderDrawColor(g_r, 0xE0, 0x4A, 0x3C, 0x50);
        SDL_RenderFillRect(g_r, &r);
        marco(r, {0xE0, 0x4A, 0x3C, 255}, 2);
    }
    else
    {
        Objeto o;
        int rx, ry;
        SDL_Rect r;
        if (cosaEn(s, s.cx, s.cy, o, rx, ry))
        {
            int w, h;
            tamano(o, w, h);
            r = pantalla(s, rx + ORIGEN_X, ry + ORIGEN_Y, w, h);
        }
        else if (const Edificio* b = edificioEn(s.cx + 1, s.cy + 1))
            r = pantalla(s, b->x - b->w / 2, b->y - b->h / 2, b->w, b->h);
        else
            r = pantalla(s, s.cx, s.cy, 2, 2);
        // De lejos, un poco más grande para que se vea.
        if (r.w < 18)
            r = {r.x + r.w / 2 - 9, r.y + r.h / 2 - 9, 18, 18};
        marco({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, {0, 0, 0, 0x90}, 2);
        marco(r, BLANCO, 2);
        if (s.modo == Marcar && !s.esquina)
            marco({r.x + 3, r.y + 3, r.w - 6, r.h - 6}, AMARILLO, 2);
    }
    SDL_SetRenderDrawBlendMode(g_r, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(g_r, nullptr);
}

void dibujarPanel(std::shared_ptr<Estado> e)
{
    Estado& s = *e;
    rect({PANEL_X, 96, W - PANEL_X, ABAJO - 96}, CREMA);
    redondo({PANEL_X + 16, 108, W - PANEL_X - 32, 240}, 14, BLANCO);
    const int x = PANEL_X + 34, ancho = W - PANEL_X - 68;
    texto(CAPAS[s.capa], x, 120, VERDE_OSCURO, g_pequena);
    std::string titulo, detalle;
    SDL_Color color = CREMA_OSCURA;
    SDL_Texture* foto = nullptr;
    Objeto o;
    int rx, ry;
    if (s.modo == Llevar)
    {
        titulo = "Moviendo: " + nombre(s.llevado);
        detalle = "Lleva el cursor a su nuevo sitio y pulsa A. B para dejarlo donde estaba.";
        color = colorDe(s.llevado);
        foto = dibujoDe(s.llevado);
    }
    else if (s.modo == Marcar)
    {
        titulo = "Marcar una zona";
        detalle = s.esquina ? "Mueve el cursor a la otra esquina y pulsa A. B para dejarlo." : "Lleva el cursor a una esquina de la zona y pulsa A. B para dejarlo.";
    }
    else if (s.modo == Goma)
    {
        titulo = "Goma: " + (s.gomaClave ? "solo " + s.gomaNombre : tipoNombre(s.gomaTipo));
        detalle = "Mantén A y mueve el cursor por encima. Y para cambiar la goma, B para dejarla.";
        color = {0xE0, 0x4A, 0x3C, 255};
    }
    else if (cosaEn(s, s.cx, s.cy, o, rx, ry))
    {
        titulo = nombre(o);
        detalle = enterrado(o) ? "Enterrado. A para cambiarlo, girarlo, moverlo o quitarlo." : "A para cambiarlo, girarlo, moverlo o quitarlo.";
        color = colorDe(o);
        foto = dibujoDe(o);
    }
    else if (const Edificio* b = edificioEn(s.cx + 1, s.cy + 1))
    {
        titulo = b->casa() && b->nombre != "Parcela libre" ? "Casa de " + b->nombre : b->nombre;
        detalle = "Los edificios se cambian de sitio en el juego.";
        if (const uint16_t i = iconoDeEdificio(b->tipo))
            foto = dibujos::de(i);
    }
    else
    {
        titulo = "Sitio vacío";
        detalle = "A para poner algo aquí.";
    }
    redondo({x, 152, 44, 44}, 8, foto ? CREMA : color);
    if (foto)
    {
        SDL_Rect r = {x + 2, 154, 40, 40};
        SDL_RenderCopy(g_r, foto, nullptr, &r);
    }
    const int alto = std::max(44, texto(titulo, x + 56, 156, MARRON, nullptr, Izquierda, ancho - 56));
    texto(detalle, x, 152 + std::min(alto, 100) + 12, MARRON_SUAVE, g_pequena, Izquierda, ancho);
    // Botones para tocar.
    auto boton = [](SDL_Rect r, const std::string& k, const std::string& t, bool activo = true) {
        // As the buttons of NHSE for Mac: white with a grey edge, and the key in a dark circle.
        redondo(r, 10, {0xD1, 0xD1, 0xD6, 255});
        redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 8, activo ? BLANCO : GRIS);
        const int kw = k.empty() ? 0 : std::max(30, anchoTexto(k, g_pequena) + 16);
        if (!k.empty())
        {
            redondo({r.x + 8, r.y + (r.h - 30) / 2, kw, 30}, 15, activo ? MARRON : MARRON_SUAVE);
            texto(k, r.x + 8 + kw / 2, r.y + r.h / 2, BLANCO, g_pequena, Centro);
        }
        texto(encajar(t, r.w - kw - 26, g_pequena), k.empty() ? r.x + r.w / 2 : r.x + kw + 18, r.y + r.h / 2 - (k.empty() ? 0 : 14), activo ? MARRON : MARRON_SUAVE, g_pequena, k.empty() ? Centro : Izquierda);
    };
    boton({PANEL_X + 16, 360, 176, 46}, "L", "Alejar");
    boton({PANEL_X + 208, 360, 176, 46}, "R", "Acercar");
    boton({PANEL_X + 16, 420, W - PANEL_X - 32, 48}, "A", s.modo == Mirar ? "Qué hay aquí" : s.modo == Llevar ? "Dejarlo aquí" : s.modo == Marcar ? "Esquina" : "Borrar");
    boton({PANEL_X + 16, 476, W - PANEL_X - 32, 48}, "X", std::string("Capa: ") + (s.capa ? "encima" : "en el suelo"));
    boton({PANEL_X + 16, 532, W - PANEL_X - 32, 48}, "Y", s.modo == Goma ? "Cambiar la goma" : "Herramientas");
    boton({PANEL_X + 16, 588, W - PANEL_X - 32, 48}, "−", "Deshacer" + (g_deshacer.empty() ? std::string() : " (" + std::to_string(g_deshacer.size()) + ")"), !g_deshacer.empty());
}

Pantalla pantallaMapa()
{
    auto e = std::make_shared<Estado>();
    g_estado = e;
    g_deshacer.clear();
    g_tocado = false;
    // Empieza en la casa del jugador, si la hay.
    e->cx = MAPA_ANCHO / 2, e->cy = MAPA_ALTO / 2;
    return [e](Vista& v) {
        v.titulo = "Mapa de la isla";
        v.dibujo = [e] {
            dibujarMapa(e);
            dibujarPanel(e);
        };
        v.entrada = [e](u64 down, u64 held, int tx, int ty) { return entrada(e, down, held, tx, ty); };
        v.x = [e] {
            e->capa = 1 - e->capa;
            avisar(std::string("Ahora: ") + CAPAS[e->capa] + ".");
        };
        v.y = [e] {
            if (e->modo == Goma)
                abrirGoma(e, 1);
            else
                abrirHerramientas(e);
        };
        v.menos = [] { deshacer(); };
        v.paginar = [e](int d) { acercar(*e, d > 0 ? (d > 1 ? 4 : 1.5) : (d < -1 ? 0.25 : 1 / 1.5)); };
        v.atras = [e] {
            if (e->modo == Goma)
            {
                terminarGoma(*e);
                e->modo = Mirar;
            }
            else if (e->modo != Mirar)
            {
                e->modo = Mirar;
                e->esquina = false;
            }
            else if (e->hayZona)
                e->hayZona = false;
            else if (g_tocado && !sinApoyoAhora().empty())
                // Como el Mac antes de guardar el mapa.
                confirmar("Cosas encima de nada", "Hay algo puesto encima de otra cosa, pero debajo no hay nada (" + numero((uint32_t)sinApoyoAhora().size()) +
                    " casillas): en el juego puede quedar flotando. Se ve en Herramientas. ¿Salir del mapa igualmente?", "Sí, salir", [] { volver(); });
            else
                volver();
        };
        switch (e->modo)
        {
            case Llevar: v.teclas = {{"A", "Dejar aquí"}, {"L R", "Zoom"}, {"B", "Cancelar"}}; break;
            case Marcar: v.teclas = {{"A", "Esquina"}, {"L R", "Zoom"}, {"B", "Cancelar"}}; break;
            case Goma: v.teclas = {{"A", "Borrar"}, {"Y", "Goma"}, {"L R", "Zoom"}, {"B", "Dejar la goma"}}; break;
            default: v.teclas = {{"A", "Qué hay"}, {"X", "Capa"}, {"Y", "Herramientas"}, {"L R", "Zoom"}, {"B", "Volver"}}; break;
        }
    };
}

Registro r1({SeccionIsla, 10, [] { return std::string("Mapa de la isla"); },
    [] { return std::string("Ver la isla y cambiar lo que hay"); },
    [] { abrir(pantallaMapa()); }});

} // namespace
