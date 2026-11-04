// Sección «Isla»: el mapa de la isla, como el de NHSE para Mac (IslandMapWindow): la isla dibujada con sus edificios y
// todo lo que hay en el suelo, en un recuadro con sus controles flotando encima (la capa, acercar y alejar, la
// cuadrícula, y las herramientas: elegir, marcar zona y goma); a la derecha, un panel con lo que hay en el sitio
// elegido (su dibujo grande y qué se puede hacer con ello) u «Ordenar la isla» (cuántas hay de cada cosa y quitarlas).
// Un cursor que se mueve por cuadros (cruceta) o tocando, con acercar y alejar; ver qué hay en un sitio, quitarlo,
// girarlo, moverlo o cambiarlo por otra cosa; las dos capas; marcar una zona; la goma; y ordenar la isla (quitar o
// cambiar todas las cosas de un tipo o de un nombre, regar). Cada cambio se puede deshacer.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <memory>
#include "../core/mapa.hpp"
#include "dibujos.hpp"
#include "mapa_panel.hpp"
#include "mapa_pintor.hpp"
#include "mapa_vista.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace ui;
using namespace mapa;

namespace {

const SDL_Rect AREA = {60, 112, 760, 548};   // donde se ve el mapa
const SDL_Rect PANEL = {836, 112, 384, 548}; // el panel de la derecha
constexpr double ZOOM_MAX = 32;
// Las pantallas se dibujan un poco más pequeñas que la de la consola (a la derecha de la barra lateral): lo que se
// arrastra el dedo, en puntos de la pantalla de la app.
constexpr double ESCALA = (double)(W - 132) / W;
const char* const CAPAS[2] = {"En el suelo", "Encima de otras cosas"};
const char* const CAPAS_CORTO[2] = {"En el suelo", "Encima de otros"};
const char* const CAPAS_MIN[2] = {"en el suelo", "encima de otras cosas"};

// Lo que se puede quitar de golpe (como «Ordenar la isla» del Mac), con su nombre.
struct TipoConNombre {
    Tipo tipo;
    const char* nombre;
    const char* minusculas;
    uint16_t icono;
    const char* corto; // en los botones pequeños // el objeto de su dibujo (los del Mac: Weed, Turip0, Branch...)
};
const TipoConNombre TIPOS[] = {{Hierbas, "Malas hierbas", "malas hierbas", 758, "Hierbas"}, {Flores, "Flores", "flores", 2883, "Flores"},
    {Ramas, "Ramas", "ramas", 14644, "Ramas"}, {Conchas, "Conchas", "conchas", 1376, "Conchas"}, {Hoyos, "Agujeros", "agujeros", 773, "Agujeros"},
    {Vallas, "Vallas", "vallas", 3080, "Vallas"}, {Arbustos, "Arbustos", "arbustos", 2820, "Arbustos"}, {Arboles, "Árboles", "árboles", 2796, "Árboles"},
    {Cosas, "Muebles y objetos", "muebles y objetos", 14414, "Muebles"}, {Todo, "Todo", "cosas", 4645, "Todo"}};

uint16_t iconoTipo(Tipo t)
{
    for (const auto& k : TIPOS)
        if (k.tipo == t)
            return k.icono;
    return 0;
}

enum Modo { Mirar, Llevar, Marcar, Goma };

// Lo que se cuenta para «Ordenar la isla» (en toda la capa o en la zona), una vez por cambio.
struct Cuenta {
    int version = -1, capa = -1;
    bool enZona = false;
    Zona zona;
    std::vector<std::pair<Tipo, int>> filas;
    int clases = 0;
};

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
    // Tocar y arrastrar: dónde empezó (en puntos de la app y de la consola) y la vista entonces.
    bool tocando = false, arrastrado = false;
    int t0x = 0, t0y = 0, r0x = 0, r0y = 0;
    double v0x = 0, v0y = 0;
    std::string aviso;
    // Los botones del panel y de encima del mapa (se ponen al dibujar).
    Chips chips;
    std::vector<SDL_Rect> flotantes; // los recuadros encima del mapa (los nombres de los edificios no van debajo)
    Cuenta cuenta;
    // Cuántas iguales a la del cursor hay (se mira al mover el cursor).
    int igualesVersion = -1, igualesX = -1, igualesY = -1, igualesCapa = -1, iguales = 0;
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

std::string textoDeshacer() { return "Deshacer" + (g_deshacer.empty() ? std::string() : " (" + std::to_string(g_deshacer.size()) + ")"); }

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

std::string nombreEdificio(const Edificio& b) { return b.casa() && b.nombre != "Parcela libre" ? "Casa de " + b.nombre : b.nombre; }

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

std::string corto(Tipo t)
{
    for (const auto& k : TIPOS)
        if (k.tipo == t)
            return k.corto;
    return "";
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

// Cuántas hay de cada cosa en la capa (o en la zona marcada), contado una vez por cambio.
const Cuenta& contarTodo(Estado& e, bool enZona)
{
    Cuenta& c = e.cuenta;
    const Zona z = enZona ? e.zona : Zona{};
    if (c.version != g_version || c.capa != e.capa || c.enZona != enZona || c.zona.x != z.x || c.zona.y != z.y || c.zona.w != z.w || c.zona.h != z.h)
    {
        Rejilla r = capaActiva(e);
        c.filas.clear();
        for (const auto& k : TIPOS)
        {
            if (k.tipo == Todo && !enZona)
                continue;
            const int n = contar(r, k.tipo, z);
            if (n == 0 && k.tipo != Hierbas && k.tipo != Flores)
                continue;
            c.filas.push_back({k.tipo, n});
        }
        c.clases = (int)clases(r, z).size();
        c.version = g_version, c.capa = e.capa, c.enZona = enZona, c.zona = z;
    }
    return c;
}

// ----------------------------------------------------------------------------------------------- acciones
void quitarTipo(std::shared_ptr<Estado> e, Tipo t, int n, bool enZona)
{
    const Zona z = enZona ? e->zona : Zona{};
    const std::string donde = enZona ? "la zona marcada" : "toda la isla";
    confirmar("Quitar", "¿Quitar " + numero(n) + " " + tipoNombre(t, true) + " de " + donde + "? Se puede deshacer.", "Sí, quitar", [e, z, t] {
        recordar();
        Rejilla r = capaActiva(*e);
        const int q = quitarTodos(r, t, z);
        hecho("Quitadas " + numero(q));
    });
}

void regarZona(std::shared_ptr<Estado> e, bool enZona)
{
    recordar();
    Rejilla r = capaActiva(*e);
    const int q = regar(r, enZona ? e->zona : Zona{});
    if (!q)
        olvidarUltimo();
    hecho("Regadas " + numero(q) + " flores");
}

void cambiarFlores(std::shared_ptr<Estado> e, int n, bool enZona)
{
    const Zona z = enZona ? e->zona : Zona{};
    const std::string donde = enZona ? "la zona marcada" : "toda la isla";
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
}

void usarGoma(Estado& e, Tipo t, uint64_t clave = 0, const std::string& n = "")
{
    e.gomaTipo = t;
    e.gomaClave = clave;
    e.gomaNombre = n;
    e.modo = Goma;
    e.hayZona = false;
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
        v.explicacion = enZona ? "En la zona marcada." : std::string("En toda la isla, ") + CAPAS_MIN[e->capa] + ".";
        const Objeto ej = k.ejemplo;
        const int capa = e->capa;
        v.dibujo = [ej, n, cuantas, enZona, capa] {
            tarjeta({60, 112, 1160, 200});
            foto({84, 136, 152, 152}, dibujoDe(ej), colorDe(ej));
            texto(encajar(n, 940, g_titulo), 264, 140, MARRON, g_titulo);
            texto(numero(cuantas), 264, 196, AZUL, g_titulo);
            texto(enZona ? "en la zona marcada" : std::string("en la isla, ") + CAPAS_MIN[capa], 270 + anchoTexto(numero(cuantas), g_titulo), 210, MARRON_SUAVE);
        };
        Boton quitar{{60, 336, 570, 96}, "Quitar todas", [e, k, n, z, cuantas] {
            confirmar("Quitar", "¿Quitar «" + n + "» (" + numero(cuantas) + ")" + (z.w ? " de la zona marcada" : " de toda la isla") + "? Se puede deshacer.", "Sí, quitar", [e, k, z] {
                recordar();
                Rejilla r = capaActiva(*e);
                const int q = quitarNombre(r, k.clave, z);
                volver();
                hecho("Quitadas " + numero(q));
            });
        }, cuantas > 0};
        quitar.grande = true;
        quitar.detalle = "Las " + numero(cuantas) + (enZona ? ", solo de la zona" : ", de toda la isla");
        quitar.imagen = dibujos::menu("Can");
        v.b.push_back(quitar);
        Boton goma{{650, 336, 570, 96}, "Goma solo para estas", [e, k, n, nivel] {
            usarGoma(*e, Todo, k.clave, n);
            alMapa(nivel);
        }};
        goma.grande = true;
        goma.detalle = "Pasa por encima y quita solo estas";
        goma.imagen = dibujos::menu("Duster");
        v.b.push_back(goma);
        v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
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
    auto fila = [e, enZona](const Clase& k, int nivel) {
        Fila f{nombre(k.ejemplo), [e, k, enZona, nivel] { abrirClase(e, k, enZona, nivel); }};
        f.detalle = numero(k.cuantas) + (k.cuantas == 1 ? " en " : " en ") + (enZona ? "la zona" : "la isla");
        f.imagen = dibujoDe(k.ejemplo);
        if (!f.imagen)
            f.color = colorDe(k.ejemplo);
        return f;
    };
    Lista l;
    l.titulo = enZona ? "Lo que hay en la zona" : "Lo que hay en la isla";
    l.explicacion = [lista_, busca] {
        return (busca->empty() ? "Cada cosa por su nombre (" : "Las que se llaman «" + *busca + "» (") + numero((uint32_t)lista_().size()) +
               "). Elige una para quitarlas todas o borrarlas con la goma.";
    };
    l.cuantas = [lista_] { return (int)lista_().size(); };
    l.porColumna = 6;
    l.alto = 66;
    l.fila = [lista_, fila, nivel](int i) { return fila(lista_()[i], nivel + 1); };
    auto buscar = [e, enZona, busca, nivel, fila] {
        // En tiempo real, con su teclado.
        auto hallados = std::make_shared<std::vector<Clase>>();
        Buscador b;
        b.titulo = "Buscar en lo que hay";
        b.explicacion = "Escribe parte del nombre.";
        b.texto = *busca;
        b.filtrar = [e, enZona, hallados](const std::string& t) {
            hallados->clear();
            const std::string q = acnh::normalizar(t);
            for (const auto& k : clases(capaActiva(*e), enZona ? e->zona : Zona{}))
                if (q.empty() || acnh::normalizar(nombre(k.ejemplo)).find(q) != std::string::npos)
                    hallados->push_back(k);
            return (int)hallados->size();
        };
        b.fila = [hallados, fila, nivel](int i) { return fila((*hallados)[i], nivel + 2); };
        b.alSalir = [busca](const std::string& t) { *busca = t; };
        abrir(buscador(b));
    };
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    abrir(lista(l));
}

// Ordenar: cada tipo con cuántas hay y botones para quitarlas (o regar, cambiar por otras, tapar), como «Ordenar la
// isla» del Mac.
void abrirOrdenar(std::shared_ptr<Estado> e, bool enZona, int nivel)
{
    abrir([e, enZona, nivel](Vista& v) {
        const Zona z = enZona ? e->zona : Zona{};
        v.titulo = enZona ? "Zona de " + std::to_string(z.w / 2) + " × " + std::to_string(z.h / 2) + " cuadros" : "Ordenar la isla";
        v.explicacion = std::string(enZona ? "Lo que hay en la zona marcada" : "Todo de golpe, en toda la isla") + " (" + CAPAS_MIN[e->capa] + "). Se puede deshacer.";
        const Cuenta& c = contarTodo(*e, enZona);
        // Cada tipo en una tarjeta con su dibujo, cuántas hay y sus botones; de dos en dos.
        struct Etiqueta {
            SDL_Rect r;
            std::string nombre, cuantas;
            uint16_t icono;
            int libre; // ancho para el nombre
        };
        auto etiquetas = std::make_shared<std::vector<Etiqueta>>();
        const int n = (int)c.filas.size();
        const int filas = (n + 1) / 2;
        const int alto = std::min(84, (ABAJO - 80 - 112) / std::max(1, filas) - 12);
        for (int k = 0; k < n; k++)
        {
            const Tipo t = c.filas[k].first;
            const int cuantas = c.filas[k].second;
            const SDL_Rect r = {60 + (k % 2) * 590, 112 + (k / 2) * (alto + 12), 570, alto};
            const int by = r.y + (alto - 48) / 2;
            int bx = r.x + r.w - 14;
            auto boton = [&](int ancho, const std::string& texto, std::function<void()> f) {
                bx -= ancho;
                v.b.push_back({{bx, by, ancho, 48}, texto, std::move(f), cuantas > 0});
                v.b.back().pequena = true;
                bx -= 8;
            };
            boton(96, t == Hoyos ? "Tapar" : "Quitar", [e, t, cuantas, enZona] { quitarTipo(e, t, cuantas, enZona); });
            if (t == Flores)
            {
                boton(172, "Cambiar por…", [e, cuantas, enZona] { cambiarFlores(e, cuantas, enZona); });
                boton(84, "Regar", [e, enZona] { regarZona(e, enZona); });
            }
            const int lado = std::min(52, alto - 16);
            etiquetas->push_back({r, tipoNombre(t), numero(cuantas), iconoTipo(t), bx - (r.x + 24 + lado) - 4});
        }
        v.dibujo = [etiquetas] {
            for (const auto& t : *etiquetas)
            {
                tarjeta(t.r, 12);
                const int lado = std::min(52, t.r.h - 16);
                foto({t.r.x + 12, t.r.y + (t.r.h - lado) / 2, lado, lado}, dibujos::de(t.icono));
                const int x = t.r.x + 24 + lado;
                const int alto = TTF_FontHeight(g_negrita) + TTF_FontHeight(g_pequena) - 8;
                const int y = t.r.y + (t.r.h - alto) / 2;
                texto(encajar(t.nombre, t.libre, g_negrita), x, y, MARRON, g_negrita);
                texto(t.cuantas, x, y + TTF_FontHeight(g_negrita) - 8, MARRON_SUAVE, g_pequena);
            }
        };
        // Abajo: cada cosa por separado, deshacer, olvidar la zona y volver.
        std::vector<Boton> abajo;
        abajo.push_back({{}, "Cada cosa por separado (" + numero(c.clases) + ")", [e, enZona, nivel] { abrirClases(e, enZona, nivel + 1); }, c.clases > 0});
        abajo.push_back({{}, textoDeshacer(), [] { deshacer(); }, !g_deshacer.empty()});
        if (enZona)
            abajo.push_back({{}, "Olvidar la zona", [e] {
                e->hayZona = false;
                volver();
            }});
        abajo.push_back({{}, "Volver", [] { volver(); }});
        // Cada uno tan ancho como su texto; «Volver» a la derecha.
        int x = 60;
        for (size_t i = 0; i < abajo.size(); i++)
        {
            const int w = std::max(160, anchoTexto(abajo[i].texto) + 48);
            abajo[i].r = {i + 1 == abajo.size() ? W - 60 - w : x, ABAJO - 66, w, 56};
            x += w + 14;
            v.b.push_back(abajo[i]);
        }
        v.menos = [] { deshacer(); };
    });
}

// La goma: de qué tamaño y qué borra.
void abrirGoma(std::shared_ptr<Estado> e, int nivel)
{
    abrir([e, nivel](Vista& v) {
        v.titulo = "Goma";
        v.explicacion = "Mantén A (o el dedo) y pasa por encima de lo que quieras quitar. Cada pasada se puede deshacer.";
        const int filas = ((int)(sizeof(TIPOS) / sizeof(TIPOS[0])) + (e->gomaClave ? 1 : 0) + 3) / 4;
        v.dibujo = [filas] {
            tarjeta({60, 112, 1160, 132});
            texto("Tamaño", 84, 128, MARRON, g_negrita);
            texto("En cuadros de la isla.", 84, 166, MARRON_SUAVE, g_pequena);
            tarjeta({60, 256, 1160, 72 + filas * 66});
            texto("Qué borra", 84, 270, MARRON, g_negrita);
        };
        for (int i = 0; i < 3; i++)
        {
            const int n = i * 2 + 1;
            Boton b{{400 + i * 270, 128, 250, 100}, std::to_string(n) + " × " + std::to_string(n), [e, n] { e->goma = n; }};
            b.marcado = e->goma == n;
            b.detalle = n == 1 ? "Un cuadro" : std::to_string(n * n) + " cuadros";
            v.b.push_back(b);
        }
        std::vector<Boton> bs;
        if (e->gomaClave)
        {
            Boton b{{}, "Solo: " + e->gomaNombre, [] {}};
            b.marcado = true;
            bs.push_back(b);
        }
        for (const auto& t : TIPOS)
        {
            const Tipo tipo = t.tipo;
            Boton b{{}, t.nombre, [e, tipo] {
                e->gomaTipo = tipo;
                e->gomaClave = 0;
            }};
            b.marcado = !e->gomaClave && e->gomaTipo == tipo;
            b.imagen = dibujos::de(t.icono);
            bs.push_back(b);
        }
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {84 + (int)(i % 4) * 280, 316 + (int)(i / 4) * 66, 268, 56};
            bs[i].pequena = true;
            v.b.push_back(bs[i]);
        }
        Boton usar{{60, ABAJO - 66, 400, 56}, "Usar la goma", [e, nivel] {
            e->modo = Goma;
            e->hayZona = false;
            alMapa(nivel);
        }};
        usar.principal = true;
        v.b.push_back(usar);
        v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
    });
}

// Herramientas (Y): zona, goma, ordenar, deshacer, cuadrícula.
void abrirHerramientas(std::shared_ptr<Estado> e)
{
    abrir([e](Vista& v) {
        v.titulo = "Herramientas del mapa";
        v.explicacion = std::string("Trabajando en: ") + CAPAS_MIN[e->capa] + ".";
        std::vector<Boton> bs;
        auto tarjeta_ = [&](const std::string& t, const std::string& d, SDL_Texture* icono, std::function<void()> f, bool activo = true) {
            Boton b{{}, t, std::move(f), activo};
            b.detalle = d;
            b.grande = true;
            b.imagen = icono;
            bs.push_back(b);
        };
        tarjeta_("Ordenar la isla", "Quitar o cambiar todo lo de un tipo", dibujos::de(758), [e] { abrirOrdenar(e, false, 2); });
        tarjeta_("Cada cosa por separado", "Por su nombre, las que más hay primero", dibujos::menu("Book"), [e] { abrirClases(e, false, 2); });
        tarjeta_("Marcar una zona", "Para ordenar solo una parte de la isla", dibujos::menu("ToolPennantRcm"), [e] {
            e->modo = Marcar;
            e->esquina = false;
            volver();
        });
        tarjeta_("Goma", "Pasa por encima de lo que quieras quitar", dibujos::menu("Duster"), [e] { abrirGoma(e, 2); });
        tarjeta_(std::string("Capa: ") + CAPAS_CORTO[e->capa], std::string("Cambiar a: ") + CAPAS_MIN[1 - e->capa], dibujos::menu("Leaf"),
            [e] { e->capa = 1 - e->capa; });
        tarjeta_(std::string("Cuadrícula: ") + (e->cuadricula ? "sí" : "no"), "Las líneas de los cuadros al acercarse", dibujos::menu("Fence"),
            [e] { e->cuadricula = !e->cuadricula; });
        tarjeta_("Ver toda la isla", "Alejarse hasta verla entera", dibujos::menu("PltPalm"), [e] {
            verIsla(*e);
            volver();
        });
        tarjeta_(textoDeshacer(), g_deshacer.empty() ? "Aún no has cambiado nada" : "Vuelve atrás el último cambio", dibujos::menu("Timer"),
            [] { deshacer(); }, !g_deshacer.empty());
        const auto& solas = sinApoyoAhora();
        if (!solas.empty())
            tarjeta_("Encima de nada: " + numero((uint32_t)solas.size()), "Cosas que en el juego pueden quedar flotando", dibujos::menu("Balloon"), [e] {
                // Al primero, en la capa de encima (lo que no tiene nada debajo).
                const Punto p = sinApoyoAhora()[0];
                e->capa = 1;
                e->cx = (p.x + ORIGEN_X) & ~1, e->cy = (p.y + ORIGEN_Y) & ~1;
                e->zoom = std::max(e->zoom, 8.0);
                seguir(*e);
                volver();
                avisar("Esto está encima de otra cosa, pero debajo no hay nada: en el juego puede quedar flotando.");
            });
        const int filas = ((int)bs.size() + 1) / 2;
        const int alto = std::min(96, (ABAJO - 80 - 112) / filas - 12);
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 112 + (int)(i / 2) * (alto + 12), 570, alto};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
    });
}

// El sitio del cursor como lo usan la pantalla de un sitio y el panel: la cosa de la capa, o el cuadro vacío. false si
// está fuera de la isla.
bool crearSitio(std::shared_ptr<Estado> e, Sitio& s, bool conIguales)
{
    Objeto o;
    int rx, ry;
    const bool hay = cosaEn(*e, e->cx, e->cy, o, rx, ry);
    if (!hay)
    {
        Rejilla r = capaActiva(*e);
        if (!r.dentro(e->cx - ORIGEN_X, e->cy - ORIGEN_Y))
            return false;
        rx = e->cx - ORIGEN_X, ry = e->cy - ORIGEN_Y;
    }
    auto pos = std::make_shared<std::pair<int, int>>(rx, ry);
    s.donde = CAPAS[e->capa];
    s.posicion = [pos] { return std::make_pair(pos->first + ORIGEN_X, pos->second + ORIGEN_Y); };
    s.plantas = e->capa == 0;
    s.objeto = [e, pos] {
        Rejilla r = capaActiva(*e);
        const Objeto o = r.leer(pos->first, pos->second);
        return esRaiz(o) ? o : nada();
    };
    s.aviso = [e] { return e->aviso; };
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
        if (conIguales)
        {
            const uint64_t clave = claveNombre(o);
            const int iguales = (int)buscarNombre(capaActiva(*e), {}, clave).size();
            const std::string n = nombre(o);
            if (iguales >= 2)
            {
                s.mas.push_back({"Quitar iguales\nLas " + numero(iguales) + " de toda la isla", [e, clave, n, iguales] {
                    confirmar("Quitar", "¿Quitar «" + n + "» (" + numero(iguales) + ") de toda la isla? Se puede deshacer.", "Sí, quitar", [e, clave] {
                        recordar();
                        Rejilla r = capaActiva(*e);
                        const int q = quitarNombre(r, clave);
                        volver();
                        hecho("Quitadas " + numero(q));
                    });
                }});
                s.mas.push_back({"Goma para estas\nAl pasar, quita solo estas", [e, clave, n] {
                    usarGoma(*e, Todo, clave, n);
                    volver();
                }});
            }
        }
    }
    return true;
}

// Qué hay en el cursor (A): la cosa de la capa, o el edificio, o el cuadro vacío.
void abrirSitio(std::shared_ptr<Estado> e)
{
    Objeto o;
    int rx, ry;
    if (!cosaEn(*e, e->cx, e->cy, o, rx, ry))
        if (const Edificio* b = edificioEn(e->cx + 1, e->cy + 1))
        {
            // Como el Mac: su dibujo en un círculo, su nombre y que se cambia en el juego.
            const std::string n = nombreEdificio(*b);
            const uint16_t icono = iconoDeEdificio(b->tipo);
            abrir([n, icono](Vista& v) {
                v.titulo = n;
                v.dibujo = [icono] {
                    tarjeta({60, 112, 1160, 200});
                    foto({84, 136, 152, 152}, icono ? dibujos::de(icono) : nullptr, {}, true);
                    texto("Este sitio es de un edificio.", 264, 150, MARRON, g_negrita);
                    texto("Los edificios se cambian de sitio en el propio juego, en los Servicios a residentes.", 264, 192, MARRON_SUAVE, g_pequena,
                        Izquierda, 920);
                };
                v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
            });
            return;
        }
    Sitio s;
    e->aviso.clear();
    if (!crearSitio(e, s, true))
    {
        avisar("Ahí no se puede poner nada: está fuera de la isla.");
        return;
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

void empezarGoma(Estado& e)
{
    recordar();
    e.borrando = true;
    e.borradas = 0;
    borrarAqui(e);
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

void dejarModo(Estado& e)
{
    if (e.modo == Goma)
        terminarGoma(e);
    e.modo = Mirar;
    e.esquina = false;
}

// La zona entre la esquina y el cursor.
void terminarZona(Estado& e)
{
    const int x0 = std::max(0, std::min(e.ex, e.cx)), y0 = std::max(ORIGEN_Y, std::min(e.ey, e.cy));
    const int x1 = std::min(MAPA_ANCHO, std::max(e.ex, e.cx) + 2), y1 = std::min(ORIGEN_Y + ALTO, std::max(e.ey, e.cy) + 2);
    e.esquina = false;
    e.modo = Mirar;
    if (x1 <= x0 || y1 <= y0)
    {
        avisar("Esa zona está fuera de la isla.", true);
        return;
    }
    e.hayZona = true;
    e.zona = {x0 - ORIGEN_X, y0 - ORIGEN_Y, x1 - x0, y1 - y0};
    avisar("Zona marcada: a la derecha, lo que hay en ella.");
}

void dejarLlevado(Estado& e)
{
    Rejilla r = capaActiva(e);
    const int x = e.cx + e.dx - ORIGEN_X, y = e.cy + e.dy - ORIGEN_Y;
    recordar();
    Objeto estorbo;
    const Resultado res = mover(r, e.desdeX, e.desdeY, x, y, &estorbo);
    if (res != Bien)
    {
        olvidarUltimo();
        avisar(noCabe(res, estorbo), true);
        return;
    }
    e.modo = Mirar;
    hecho("Movido: " + nombre(e.llevado));
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
            dejarLlevado(*e);
            break;
        case Marcar:
            if (!e->esquina)
            {
                e->esquina = true;
                e->ex = e->cx, e->ey = e->cy;
            }
            else
                terminarZona(*e);
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

// El cuadro del mapa en un punto de la pantalla.
void cuadroEn(const Estado& s, int px, int py, int& mx, int& my)
{
    mx = std::clamp(((int)std::floor(s.vx + (px - AREA.x) / s.zoom)) & ~1, 0, MAPA_ANCHO - 2);
    my = std::clamp(((int)std::floor(s.vy + (py - AREA.y) / s.zoom)) & ~1, 0, MAPA_ALTO - 2);
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

    // Tocar: los botones del panel y de encima del mapa; en el mapa, un toque elige el cuadro (y otro toque en el
    // mismo, lo abre) y arrastrar mueve el mapa (con la goma, borra; marcando, marca la zona).
    HidTouchScreenState ts = {};
    hidGetTouchScreenStates(&ts, 1);
    const bool toca = ts.count > 0;
    const int rx = toca ? (int)ts.touches[0].x : 0, ry = toca ? (int)ts.touches[0].y : 0;
    if (tx >= 0)
    {
        if (s.chips.tocar(tx, ty))
            return true;
        if (tx >= AREA.x && tx < AREA.x + AREA.w && ty >= AREA.y && ty < AREA.y + AREA.h)
        {
            s.tocando = true;
            s.arrastrado = false;
            s.t0x = tx, s.t0y = ty, s.r0x = rx, s.r0y = ry, s.v0x = s.vx, s.v0y = s.vy;
            if (s.modo == Goma || s.modo == Marcar)
            {
                cuadroEn(s, tx, ty, s.cx, s.cy);
                if (s.modo == Goma)
                    empezarGoma(s);
                else
                    s.esquina = true, s.ex = s.cx, s.ey = s.cy;
            }
        }
        return true;
    }
    if (toca && s.tocando)
    {
        const int px = s.t0x + (int)std::lround((rx - s.r0x) / ESCALA), py = s.t0y + (int)std::lround((ry - s.r0y) / ESCALA);
        if (std::abs(px - s.t0x) + std::abs(py - s.t0y) > 12)
            s.arrastrado = true;
        if (s.modo == Goma || s.modo == Marcar)
        {
            const int ax = s.cx, ay = s.cy;
            cuadroEn(s, std::clamp(px, AREA.x, AREA.x + AREA.w - 1), std::clamp(py, AREA.y, AREA.y + AREA.h - 1), s.cx, s.cy);
            if (s.modo == Goma && s.borrando && (ax != s.cx || ay != s.cy))
                borrarAqui(s);
        }
        else if (s.arrastrado)
        {
            s.vx = s.v0x - (px - s.t0x) / s.zoom;
            s.vy = s.v0y - (py - s.t0y) / s.zoom;
            ajustar(s);
        }
        return true;
    }
    if (!toca && s.tocando)
    {
        s.tocando = false;
        if (s.modo == Goma)
            terminarGoma(s);
        else if (s.modo == Marcar)
        {
            if (s.arrastrado)
                terminarZona(s);
            // Un toque sin arrastrar deja la primera esquina: el siguiente toque o A pone la otra.
        }
        else if (!s.arrastrado)
        {
            int mx, my;
            cuadroEn(s, s.t0x, s.t0y, mx, my);
            // Los árboles se dibujan más altos que su cuadro: tocar su copa también los elige.
            Objeto o;
            int ox, oy;
            if (!cosaEn(s, mx, my, o, ox, oy) && cosaEn(s, mx, my + 2, o, ox, oy) && clasePlanta(o.id) == 3)
                my += 2;
            const bool mismo = mx == s.cx && my == s.cy;
            s.cx = mx, s.cy = my;
            if (mismo)
                pulsarA(e);
        }
        return true;
    }

    if (s.modo == Goma)
    {
        if ((down & HidNpadButton_A) || ((held & HidNpadButton_A) && !s.borrando))
            empezarGoma(s);
        else if ((held & HidNpadButton_A) && s.borrando && (dx || dy))
            borrarAqui(s);
        else if (!(held & HidNpadButton_A))
            terminarGoma(s);
        return (down & HidNpadButton_A) || dx || dy; // B, X, Y, L y R los mira la vista
    }
    if (down & HidNpadButton_A)
    {
        pulsarA(e);
        return true;
    }
    return dx || dy;
}

// --------------------------------------------------------------------------------------------- dibujar el mapa
void dibujarMapa(std::shared_ptr<Estado> e)
{
    Estado& s = *e;
    // El recuadro del mapa, con las esquinas redondeadas como el Mac.
    redondo({AREA.x - 2, AREA.y - 2, AREA.w + 4, AREA.h + 4}, 16, BORDE_MAC);
    if (!lienzo().listo())
    {
        rect(AREA, {0x6F, 0xC3, 0xD8, 255});
        esquinas(AREA, 14, BORDE_MAC);
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
    rect(AREA, {0x6F, 0xC3, 0xD8, 255});
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
    // Los nombres de los edificios en etiquetas blancas (las casas solo si caben), dentro del mapa.
    std::vector<SDL_Rect> puestos;
    for (int pasada = 0; pasada < 2; pasada++)
        for (const auto& b : losEdificios())
        {
            if (b.cruce() || b.nombre.empty() || (b.casa() != (pasada == 1)))
                continue;
            if (b.casa() && b.w * s.zoom < 46)
                continue;
            const int ax = pantallaX(s, b.x), ay = pantallaY(s, b.y + b.h / 2);
            const int w = anchoTexto(b.nombre, g_pequena) + 18;
            const SDL_Rect pill = {ax - w / 2, ay + 2, w, 28};
            if (pill.x < AREA.x + 4 || pill.y < AREA.y + 4 || pill.x + pill.w > AREA.x + AREA.w - 4 || pill.y + pill.h > AREA.y + AREA.h - 4)
                continue;
            bool choca = false;
            for (const auto& p : s.flotantes)
                choca = choca || SDL_HasIntersection(&p, &pill);
            for (const auto& p : puestos)
                choca = choca || SDL_HasIntersection(&p, &pill);
            if (choca)
                continue;
            puestos.push_back(pill);
            redondo({pill.x, pill.y + 1, pill.w, pill.h}, 10, {0, 0, 0, 0x28});
            redondo(pill, 10, {255, 255, 255, 240});
            texto(b.nombre, ax, ay + 16, MARRON, g_pequena, Centro);
        }
    // La zona marcada.
    if (s.hayZona)
    {
        const SDL_Rect z = pantalla(s, s.zona.x + ORIGEN_X, s.zona.y + ORIGEN_Y, s.zona.w, s.zona.h);
        rect(z, {255, 255, 255, 0x38});
        marco(z, BLANCO, 2);
    }
    if (s.modo == Marcar && s.esquina)
    {
        const int x0 = std::min(s.ex, s.cx), y0 = std::min(s.ey, s.cy);
        const SDL_Rect z = pantalla(s, x0, y0, std::abs(s.ex - s.cx) + 2, std::abs(s.ey - s.cy) + 2);
        rect(z, {255, 255, 255, 0x38});
        marco(z, BLANCO, 2);
        const std::string t = std::to_string(std::abs(s.ex - s.cx) / 2 + 1) + " × " + std::to_string(std::abs(s.ey - s.cy) / 2 + 1);
        const int tw = anchoTexto(t, g_pequena) + 14;
        redondo({z.x + 4, z.y + 4, tw, 28}, 8, {0, 0, 0, 0x90});
        texto(t, z.x + 11, z.y + 18 - TTF_FontHeight(g_pequena) / 2, BLANCO, g_pequena);
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
            rect(r, {c.r, c.g, c.b, 0x50});
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
            rect(r, f);
        }
        marco(r, c, 3);
    }
    else if (s.modo == Goma)
    {
        const SDL_Rect p = pincel(s);
        const SDL_Rect r = pantalla(s, p.x, p.y, p.w, p.h);
        rect(r, {0xE0, 0x4A, 0x3C, 0x50});
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
    esquinas(AREA, 14, BORDE_MAC);
}

// Los controles que flotan encima del mapa, como en el Mac: la capa arriba a la izquierda, acercar y alejar arriba a
// la derecha, las herramientas abajo a la izquierda y deshacer abajo a la derecha.
void dibujarControles(std::shared_ptr<Estado> e)
{
    Estado& s = *e;
    Chips& ch = s.chips;
    s.flotantes.clear();
    if (!lienzo().listo())
        return;
    // La capa.
    {
        const SDL_Rect r = {AREA.x + 12, AREA.y + 12, 380, 60};
        s.flotantes.push_back(r);
        flotante(r);
        segmentado(ch, {r.x + 8, r.y + 8, r.w - 54, 44}, {CAPAS_CORTO[0], CAPAS_CORTO[1]}, s.capa, [e](int i) {
            e->capa = i;
            e->hayZona = false;
        });
        tecla("X", r.x + r.w - 23, r.y + r.h / 2);
    }
    // Acercar, alejar, toda la isla y la cuadrícula.
    {
        const SDL_Rect r = {AREA.x + AREA.w - 12 - 318, AREA.y + 12, 318, 60};
        s.flotantes.push_back(r);
        flotante(r);
        int x = r.x + 8;
        auto boton = [&](int ancho, const std::string& t, std::function<void()> f, bool marcado = false, SDL_Texture* icono = nullptr) {
            Chip c{{x, r.y + 8, ancho, 44}, t, std::move(f)};
            c.marcado = marcado;
            c.icono = icono;
            ch.poner(c);
            x += ancho + 6;
        };
        boton(44, "−", [e] { acercar(*e, 1 / 1.5); });
        boton(44, "+", [e] { acercar(*e, 1.5); });
        boton(50, "", [e] { verIsla(*e); }, false, dibujos::menu("PltPalm"));
        boton(r.x + r.w - 8 - x, "Cuadrícula", [e] { e->cuadricula = !e->cuadricula; }, s.cuadricula);
    }
    // Las herramientas: elegir, marcar zona y goma.
    {
        const char* ayuda = s.modo == Goma ? "Pasa por encima de lo que quieras quitar."
                            : s.modo == Marcar ? "Arrastra el dedo, o A en cada esquina."
                            : s.modo == Llevar ? "Llévalo a su nuevo sitio y pulsa A."
                                               : "Toca un sitio para ver qué hay.";
        const SDL_Rect r = {AREA.x + 12, AREA.y + AREA.h - 12 - 96, 470, 96};
        s.flotantes.push_back(r);
        flotante(r);
        const int elegida = s.modo == Marcar ? 1 : s.modo == Goma ? 2 : 0;
        segmentado(ch, {r.x + 8, r.y + 8, r.w - 16, 44}, {"Elegir", "Marcar zona", "Goma"}, elegida, [e](int i) {
            dejarModo(*e);
            if (i == 1)
                e->modo = Marcar;
            else if (i == 2)
                e->modo = Goma, e->hayZona = false;
        }, {dibujos::menu("Net"), dibujos::menu("ToolPennantRcm"), dibujos::menu("Duster")});
        texto(encajar(ayuda, r.w - 28, g_pequena), r.x + 14, r.y + 60, MARRON_SUAVE, g_pequena);
    }
    // Deshacer.
    if (!g_deshacer.empty())
    {
        const std::string t = textoDeshacer();
        const int w = anchoTexto(t, g_pequena) + 74;
        const SDL_Rect r = {AREA.x + AREA.w - 12 - w - 16, AREA.y + AREA.h - 12 - 60, w + 16, 60};
        s.flotantes.push_back(r);
        flotante(r);
        Chip c{{r.x + 8, r.y + 8, w, 44}, t, [] { deshacer(); }};
        c.tecla = "−";
        ch.poner(c);
    }
}

// --------------------------------------------------------------------------------------------- el panel
// «Ordenar la isla» (o la zona) en el panel, como el Mac: un título con «Más…», cada tipo con su dibujo, cuántas hay
// y sus botones, y abajo «Cada cosa por separado» (y «extra», si hay).
void tablaOrdenar(std::shared_ptr<Estado> e, bool enZona, int x, int y, int ancho, int hasta, const std::string& titulo, const Chip* extra = nullptr)
{
    Chips& ch = e->chips;
    const Cuenta& c = contarTodo(*e, enZona);
    texto(encajar(titulo, ancho - 140, g_negrita), x, y + 22 - TTF_FontHeight(g_negrita) / 2, MARRON, g_negrita);
    const int mw = enZona ? 130 : 100;
    Chip mas{{x + ancho - mw, y, mw, 44}, "Más…", [e, enZona] { abrirOrdenar(e, enZona, 1); }};
    if (enZona)
        mas.tecla = "Y";
    ch.poner(mas);
    y += 54;
    const int n = (int)c.filas.size();
    const int alto = std::clamp((hasta - y - 52) / std::max(1, n), 44, 50);
    for (int k = 0; k < n; k++)
    {
        const Tipo t = c.filas[k].first;
        const int cuantas = c.filas[k].second;
        const int fy = y + k * alto;
        if (fy + 44 > hasta - 52)
            break;
        if (SDL_Texture* d = dibujos::de(iconoTipo(t)))
        {
            SDL_Rect r = {x, fy + (44 - 32) / 2, 32, 32};
            SDL_RenderCopy(g_r, d, nullptr, &r);
        }
        // Los botones a la derecha; el número antes; el nombre en lo que quede.
        int bx = x + ancho;
        auto boton = [&](int w, const std::string& txt, std::function<void()> f) {
            bx -= w;
            Chip b{{bx, fy, w, 44}, txt, std::move(f)};
            b.activo = cuantas > 0;
            ch.poner(b);
            bx -= 6;
        };
        boton(84, t == Hoyos ? "Tapar" : "Quitar", [e, t, cuantas, enZona] { quitarTipo(e, t, cuantas, enZona); });
        if (t == Flores)
            boton(76, "Regar", [e, enZona] { regarZona(e, enZona); });
        const std::string num = numero(cuantas);
        texto(num, bx - 4, fy + 22 - TTF_FontHeight(g_negrita) / 2, MARRON, g_negrita, Derecha);
        const int libre = bx - 14 - anchoTexto(num, g_negrita) - (x + 42);
        const std::string n = anchoTexto(tipoNombre(t), g_pequena) <= libre ? tipoNombre(t) : corto(t);
        texto(encajar(n, libre, g_pequena), x + 42, fy + 22 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
    }
    const int fy = hasta - 44;
    const std::string porNombres = extra ? "Por nombres" : "Cada cosa por separado (" + numero(c.clases) + ")";
    const int w = extra ? (ancho - 8) / 2 : ancho;
    Chip clases_{{x, fy, w, 44}, porNombres, [e, enZona] { abrirClases(e, enZona, 1); }};
    clases_.activo = c.clases > 0;
    ch.poner(clases_);
    if (extra)
    {
        Chip x2 = *extra;
        x2.r = {x + w + 8, fy, ancho - w - 8, 44};
        ch.poner(x2);
    }
}

void dibujarPanel(std::shared_ptr<Estado> e)
{
    Estado& s = *e;
    Chips& ch = s.chips;
    tarjeta(PANEL);
    const int x = PANEL.x + 20, ancho = PANEL.w - 40, abajo = PANEL.y + PANEL.h - 16;
    int y = PANEL.y + 18;
    if (!lienzo().listo())
        return;
    // Lo de arriba: lo elegido con su dibujo grande, su nombre y dónde está.
    auto cabecera = [&](SDL_Texture* t, SDL_Color color, const std::string& titulo, const std::string& debajo, bool redonda = false) {
        foto({x, y, 96, 96}, t, color, redonda);
        const int tx = x + 112, tw = ancho - 112;
        const int h = texto(titulo, tx, y + 4, MARRON, g_negrita, Izquierda, tw);
        texto(encajar(debajo, tw, g_pequena), tx, y + 4 + std::min(h, 70) - 4, MARRON_SUAVE, g_pequena);
        y += 96 + 16;
    };
    auto fila = [&](std::vector<Chip> cs) {
        const int n = (int)cs.size();
        const int w = (ancho - 8 * (n - 1)) / n;
        for (int i = 0; i < n; i++)
        {
            cs[i].r = {x + i * (w + 8), y, i == n - 1 ? x + ancho - (x + i * (w + 8)) : w, 44};
            ch.poner(cs[i]);
        }
        y += 52;
    };
    if (s.modo == Llevar)
    {
        cabecera(dibujoDe(s.llevado), colorDe(s.llevado), nombre(s.llevado), "Moviéndolo");
        y = encabezado("", "Lleva el cursor a su nuevo sitio (o tócalo) y pulsa A. Verde: cabe; rojo: algo estorba.", x, y, ancho) + 16;
        Chip dejar{{}, "Dejar aquí", [e] { dejarLlevado(*e); }};
        dejar.principal = true;
        dejar.tecla = "A";
        Chip cancelar{{}, "Cancelar", [e] { e->modo = Mirar; }};
        cancelar.tecla = "B";
        fila({dejar, cancelar});
        return;
    }
    if (s.modo == Marcar)
    {
        cabecera(dibujos::menu("ToolPennantRcm"), {}, "Marcar una zona", "Solo una parte");
        y = encabezado("", s.esquina ? "Ahora la otra esquina: mueve el cursor y pulsa A (o toca)." : "Arrastra el dedo sobre el mapa, o pon el cursor en una esquina y pulsa A.", x, y, ancho) + 16;
        if (s.esquina)
        {
            Chip c{{}, "Aquí", [e] { terminarZona(*e); }};
            c.principal = true;
            c.tecla = "A";
            Chip no{{}, "Cancelar", [e] { dejarModo(*e); }};
            no.tecla = "B";
            fila({c, no});
        }
        else
        {
            Chip no{{}, "Cancelar", [e] { dejarModo(*e); }};
            no.tecla = "B";
            fila({no});
        }
        return;
    }
    if (s.modo == Goma)
    {
        cabecera(dibujos::menu("Duster"), {}, "Goma", s.gomaClave ? "Solo: " + s.gomaNombre : "Borra: " + tipoNombre(s.gomaTipo, true));
        texto("Tamaño", x, y + 22 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
        std::vector<std::string> tam = {"1", "3", "5"};
        segmentado(ch, {x + 90, y, ancho - 90, 44}, tam, s.goma / 2, [e](int i) { e->goma = i * 2 + 1; });
        y += 60;
        texto("Qué borra", x, y, MARRON, g_pequena);
        y += 32;
        // Cada tipo con su dibujo, de dos en dos (y «solo estas», si se eligió).
        std::vector<Chip> cs;
        if (s.gomaClave)
        {
            Chip c{{}, "Solo: " + s.gomaNombre, [] {}};
            c.marcado = true;
            cs.push_back(c);
        }
        for (const auto& t : TIPOS)
        {
            const Tipo tipo = t.tipo;
            Chip c{{}, t.corto, [e, tipo] {
                e->gomaTipo = tipo;
                e->gomaClave = 0;
            }};
            c.marcado = !s.gomaClave && s.gomaTipo == tipo;
            c.icono = dibujos::de(t.icono);
            cs.push_back(c);
        }
        const int w = (ancho - 8) / 2;
        for (size_t i = 0; i < cs.size(); i++)
        {
            const int fy = y + (int)(i / 2) * 50;
            if (fy + 44 > abajo - 52)
                break;
            cs[i].r = {x + (int)(i % 2) * (w + 8), fy, w, 44};
            ch.poner(cs[i]);
        }
        Chip dejar{{x, abajo - 44, ancho, 44}, "Dejar la goma", [e] { dejarModo(*e); }};
        dejar.tecla = "B";
        ch.poner(dejar);
        return;
    }
    // Elegir: la zona marcada, la cosa del cursor, el edificio, o el sitio vacío con «Ordenar la isla».
    if (s.hayZona)
    {
        Chip olvidar{{}, "Olvidar", [e] { e->hayZona = false; }};
        olvidar.tecla = "B";
        tablaOrdenar(e, true, x, y, ancho, abajo, "Zona de " + std::to_string(s.zona.w / 2) + " × " + std::to_string(s.zona.h / 2), &olvidar);
        return;
    }
    Objeto o;
    int rx, ry;
    if (cosaEn(s, s.cx, s.cy, o, rx, ry))
    {
        int w, h;
        tamano(o, w, h);
        std::string donde = CAPAS[s.capa];
        if (enterrado(o))
            donde += " · enterrado";
        cabecera(dibujoDe(o), colorDe(o), nombre(o), donde);
        auto sitio = [e](std::function<void(Sitio&)> f) {
            return [e, f] {
                Sitio st;
                e->aviso.clear();
                if (crearSitio(e, st, false))
                    f(st);
            };
        };
        std::vector<Chip> a;
        a.push_back({{}, "Quitar", sitio([](Sitio& st) {
            const std::string n = nombre(st.objeto());
            st.quitar();
            cambiado("Quitado: " + n);
        })});
        if (girable(o))
            a.push_back({{}, "Girar", sitio([](Sitio& st) {
                if (st.girar)
                    st.girar();
            })});
        a.push_back({{}, "Mover", sitio([](Sitio& st) { st.mover(); })});
        fila(a);
        Chip cambiar{{}, "Cambiar por…", sitio([](Sitio& st) {
            buscarObjeto([st](const Objeto& n) {
                if (st.poner(n))
                    cambiado("Puesto: " + nombre(n));
            }, "ponerlo en su lugar");
        })};
        Chip mas{{}, "Más", [e] { abrirSitio(e); }};
        mas.tecla = "A";
        fila({cambiar, mas});
        // Las que se llaman igual (se cuentan al mover el cursor).
        if (s.igualesVersion != g_version || s.igualesX != rx || s.igualesY != ry || s.igualesCapa != s.capa)
        {
            s.iguales = (int)buscarNombre(capaActiva(s), {}, claveNombre(o)).size();
            s.igualesVersion = g_version, s.igualesX = rx, s.igualesY = ry, s.igualesCapa = s.capa;
        }
        if (s.iguales >= 2)
        {
            y += 8;
            texto("Hay " + numero(s.iguales) + " iguales en la isla", x, y, MARRON, g_pequena);
            y += 34;
            const uint64_t clave = claveNombre(o);
            const std::string n = nombre(o);
            const int iguales = s.iguales;
            fila({Chip{{}, "Quitar las " + numero(iguales), [e, clave, n, iguales] {
                confirmar("Quitar", "¿Quitar «" + n + "» (" + numero(iguales) + ") de toda la isla? Se puede deshacer.", "Sí, quitar", [e, clave] {
                    recordar();
                    Rejilla r = capaActiva(*e);
                    const int q = quitarNombre(r, clave);
                    hecho("Quitadas " + numero(q));
                });
            }}});
            Chip goma{{}, "Goma solo para estas", [e, clave, n] { usarGoma(*e, Todo, clave, n); }};
            goma.icono = dibujos::menu("Duster");
            fila({goma});
        }
        y += 8;
        if (w > 2 || h > 2)
            texto("Ocupa " + std::to_string(std::max(1, w / 2)) + " × " + std::to_string(std::max(1, h / 2)) + " cuadros.", x, y, MARRON_SUAVE, g_pequena);
        return;
    }
    if (const Edificio* b = edificioEn(s.cx + 1, s.cy + 1))
    {
        const uint16_t i = iconoDeEdificio(b->tipo);
        foto({x + (ancho - 120) / 2, y, 120, 120}, i ? dibujos::de(i) : nullptr, {}, true);
        y += 136;
        encabezado(nombreEdificio(*b), "Este sitio es de un edificio. Los edificios se cambian de sitio en el propio juego, en los Servicios a residentes.", x, y,
            ancho);
        return;
    }
    // Un sitio vacío: lo que se puede poner, y ordenar la isla como el Mac (si no cabe todo, solo ordenar).
    const int filas = (int)contarTodo(s, false).filas.size();
    if (abajo - y - (54 + filas * 46 + 52) >= 66)
    {
        Sitio st;
        const bool dentro = crearSitio(e, st, false);
        texto(dentro ? "Sitio vacío" : "Fuera de la isla", x, y + 22 - TTF_FontHeight(g_negrita) / 2, MARRON, g_negrita);
        if (dentro)
        {
            Chip poner{{x + ancho - 170, y, 170, 44}, "Poner algo", [e] { abrirSitio(e); }};
            poner.tecla = "A";
            ch.poner(poner);
        }
        y += 56;
        rect({x, y, ancho, 2}, CREMA_OSCURA);
        y += 12;
    }
    tablaOrdenar(e, false, x, y, ancho, abajo, "Ordenar la isla");
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
        v.explicacion = "Toca un sitio para ver qué hay y cambiarlo; arrastra para moverte. Con la cruceta, el cursor; L y R alejan y acercan.";
        v.dibujo = [e] {
            e->chips.limpiar();
            dibujarMapa(e);
            dibujarControles(e);
            dibujarPanel(e);
            e->chips.dibujar();
        };
        v.entrada = [e](u64 down, u64 held, int tx, int ty) { return entrada(e, down, held, tx, ty); };
        v.x = [e] {
            e->capa = 1 - e->capa;
            e->hayZona = false;
            avisar(std::string("Ahora: ") + CAPAS[e->capa] + ".");
        };
        v.y = [e] {
            if (e->modo == Goma)
                abrirGoma(e, 1);
            else if (e->hayZona)
                abrirOrdenar(e, true, 1);
            else
                abrirHerramientas(e);
        };
        v.menos = [] { deshacer(); };
        v.paginar = [e](int d) { acercar(*e, d > 0 ? (d > 1 ? 4 : 1.5) : (d < -1 ? 0.25 : 1 / 1.5)); };
        v.atras = [e] {
            if (e->modo != Mirar)
                dejarModo(*e);
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
            default:
                if (e->hayZona)
                    v.teclas = {{"A", "Qué hay"}, {"Y", "La zona"}, {"L R", "Zoom"}, {"B", "Olvidar la zona"}};
                else
                    v.teclas = {{"A", "Qué hay"}, {"X", "Capa"}, {"Y", "Herramientas"}, {"L R", "Zoom"}, {"B", "Volver"}};
                break;
        }
    };
}

Registro r1({SeccionIsla, 10, [] { return std::string("Mapa de la isla"); },
    [] { return std::string("Ver la isla y cambiar lo que hay"); },
    [] { abrir(pantallaMapa()); }});

} // namespace
