// La casa de cada vecino: las paredes y el suelo de dentro, con un dibujo de la habitación (como RoomWindow de NHSE para
// Mac), y cómo se ve por fuera y lo que cuelga en la puerta (como VillagerHouseWindow), con su archivo en la tarjeta SD.
#include <algorithm>
#include <memory>
#include "../core/vecino.hpp"
#include "../core/vecino_tablas.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
constexpr int NUM_PAREDES = sizeof(PAREDES) / sizeof(PAREDES[0]);
constexpr int NUM_SUELOS = sizeof(SUELOS) / sizeof(SUELOS[0]);
constexpr int NUM_TEJADOS = sizeof(TEJADOS) / sizeof(TEJADOS[0]);
constexpr int NUM_FACHADAS = sizeof(FACHADAS) / sizeof(FACHADAS[0]);
constexpr int NUM_PUERTAS = sizeof(PUERTAS) / sizeof(PUERTAS[0]);

void cambio(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

std::string nombreParte(uint16_t id)
{
    if (id == NADA)
        return "Nada";
    for (const auto& p : PAREDES)
        if (p.id == id)
            return p.nombre;
    for (const auto& p : SUELOS)
        if (p.id == id)
            return p.nombre;
    return "Objeto nº " + std::to_string(id);
}

bool hayAcento(int hueco)
{
    const uint16_t a = parteCuarto(partida::principal(), hueco, CuartoAcento);
    return a != SIN_ACENTO && a != NADA;
}

std::string nombreAspecto(const AspectoCasa* lista, int n, uint16_t id)
{
    for (int i = 0; i < n; i++)
        if (lista[i].id == id)
            return lista[i].nombre;
    return "Otro (nº " + std::to_string(id) + ")";
}

int puertaDe(uint16_t id)
{
    for (int i = 0; i < NUM_PUERTAS; i++)
        if (PUERTAS[i].arco == id || PUERTAS[i].recta == id)
            return i;
    return -1;
}

std::string nombrePuerta(uint16_t id)
{
    const int k = puertaDe(id);
    return k < 0 ? "Otra (nº " + std::to_string(id) + ")" : PUERTAS[k].nombre;
}

// ------------------------------------------------------------------------------------------- el dibujo de la habitación
// Una caja vista de frente: la pared del fondo, las dos de los lados y el suelo.
constexpr int L = 60, R = 620, T = 150, B = 600; // fuera
constexpr int l = 180, r = 500, t = 200, b = 420; // la pared del fondo

int entre(int a, int c, int i, int n) { return a + (c - a) * i / n; }

void dibujarCuarto(int hueco, int elegido)
{
    const bool acento = hayAcento(hueco);
    const SDL_Color pared = {222, 205, 170, 255}, paredLado = {204, 186, 150, 255}, fondoAcento = {196, 214, 190, 255}, suelo = {170, 132, 96, 255};
    auto marca = [](SDL_Color c, bool si) { return si ? SDL_Color{(Uint8)std::min(255, c.r + 30), (Uint8)std::min(255, c.g + 30), (Uint8)std::max(0, c.b - 40), 255} : c; };
    // Paredes de los lados (columnas de un píxel) y suelo (filas).
    const SDL_Color lado = marca(paredLado, elegido == CuartoPared);
    for (int x = L; x < l; x++)
    {
        const int arriba = entre(T, t, x - L, l - L), abajo = entre(B, b, x - L, l - L);
        rect({x, arriba, 1, abajo - arriba}, lado);
        const int xd = R - 1 - (x - L);
        rect({xd, arriba, 1, abajo - arriba}, lado);
    }
    const SDL_Color s = marca(suelo, elegido == CuartoSuelo);
    for (int y = b; y < B; y++)
    {
        const int a = entre(l, L, y - b, B - b), c = entre(r, R, y - b, B - b);
        rect({a, y, c - a, 1}, s);
    }
    const SDL_Color fondo = acento ? marca(fondoAcento, elegido == CuartoAcento) : marca(pared, elegido == CuartoPared);
    rect({l, t, r - l, b - t}, fondo);
    // Las líneas de las esquinas.
    const SDL_Color linea = MARRON_SUAVE;
    rect({l, t, r - l, 2}, linea);
    rect({l, b, r - l, 2}, linea);
    rect({l, t, 2, b - t}, linea);
    rect({r, t, 2, b - t + 2}, linea);
    // Qué hay en cada parte.
    texto(acento ? "Pared de acento" : "Pared", (l + r) / 2, t + 30, MARRON_SUAVE, g_pequena, Centro);
    texto(encajar(nombreParte(parteCuarto(partida::principal(), hueco, acento ? CuartoAcento : CuartoPared)), r - l - 20, g_pequena), (l + r) / 2, (t + b) / 2, MARRON, g_pequena, Centro);
    texto("Suelo", (L + R) / 2, b + 40, CREMA, g_pequena, Centro);
    texto(encajar(nombreParte(parteCuarto(partida::principal(), hueco, CuartoSuelo)), R - L - 140), (L + R) / 2, (b + B) / 2 + 14, BLANCO, nullptr, Centro);
}

// ¿Qué parte se ha tocado? -1 si ninguna.
int parteTocada(int hueco, int tx, int ty)
{
    if (tx >= l && tx < r && ty >= t && ty < b)
        return hayAcento(hueco) ? CuartoAcento : CuartoPared;
    if (ty >= b && ty < B && tx >= entre(l, L, ty - b, B - b) && tx < entre(r, R, ty - b, B - b))
        return CuartoSuelo;
    if (tx >= L && tx < l && ty >= entre(T, t, tx - L, l - L) && ty < entre(B, b, tx - L, l - L))
        return CuartoPared;
    if (tx >= r && tx < R && ty >= entre(t, T, tx - r, R - r) && ty < entre(b, B, tx - r, R - r))
        return CuartoPared;
    return -1;
}

void elegirParte(int hueco, int parte)
{
    const bool esSuelo = parte == CuartoSuelo;
    const ParteHabitacion* lista = esSuelo ? SUELOS : PAREDES;
    const int n = esSuelo ? NUM_SUELOS : NUM_PAREDES;
    std::vector<Opcion> op;
    int actual = -1;
    const uint16_t ahora = parteCuarto(partida::principal(), hueco, parte);
    for (int i = 0; i < n; i++)
    {
        op.push_back({lista[i].nombre, lista[i].clave});
        if (lista[i].id == ahora)
            actual = i;
    }
    const char* titulos[3] = {"Elige otra pared de acento", "Elige otra pared", "Elige otro suelo"};
    elegirLargo(titulos[parte], esSuelo ? "Elige un suelo para ponerlo. X para buscar por nombre (madera, ladrillo…)."
                                        : "Elige un papel de pared para ponerlo. X para buscar por nombre (madera, ladrillo…).",
        op, actual, [hueco, parte, lista](int k) {
            ponerParteCuarto(partida::principal(), hueco, parte, lista[k].id);
            cambio(std::string("Puesto: ") + lista[k].nombre);
        });
}

// ------------------------------------------------------------------------------------------- archivo de la casa
void exportarCasaSd(int hueco)
{
    const int k = casaDe(partida::principal(), hueco);
    const std::string n = archivo(nombre(hueco), "nhvh2");
    if (k >= 0 && partida::escribirSd(std::string(CARPETA) + n, exportarCasa(partida::principal(), k)))
        avisar("Guardada en la tarjeta SD: /switch/NHSE/vecinos/" + n);
    else
        avisar("No he podido guardarla en la tarjeta SD.", true);
}

void importarCasaSd(int hueco)
{
    std::vector<std::string> nombres = partida::listarSd(CARPETA, ".nhvh2");
    for (const auto& n : partida::listarSd(CARPETA, ".nhvh"))
        nombres.push_back(n);
    if (nombres.empty())
    {
        avisar("No hay casas en la carpeta /switch/NHSE/vecinos de la tarjeta SD.", true);
        return;
    }
    std::vector<Opcion> op;
    for (const auto& n : nombres)
        op.push_back({n, normalizar(n)});
    elegirLargo("¿Qué casa pongo a " + nombre(hueco) + "?", "Las casas guardadas en /switch/NHSE/vecinos de la tarjeta SD. "
                                                         + nombre(hueco) + " sigue viviendo en ella.", op, -1, [hueco, nombres](int k) {
        std::vector<uint8_t> datos;
        const int casa = casaDe(partida::principal(), hueco);
        const std::string mal = casa < 0 ? "No tiene casa." : !partida::leerSd(std::string(CARPETA) + nombres[k], datos)
            ? "No he podido leer el archivo." : importarCasa(partida::principal(), casa, datos);
        if (!mal.empty())
            avisar("«" + nombres[k] + "»: " + mal, true);
        else
            cambio("Casa cambiada");
    });
}
} // namespace

// ------------------------------------------------------------------------------------------- elegir de una lista larga
void elegirLargo(const std::string& titulo, const std::string& explicacion, std::vector<Opcion> opciones, int actual,
    std::function<void(int)> elegido)
{
    constexpr int COLUMNAS = 3, FILAS = 7, POR_PAGINA = COLUMNAS * FILAS;
    struct Estado {
        std::string buscado;
        std::vector<int> vistas; // indices in opciones
        int pagina = 0;
        int enfocar = -1;
    };
    auto e = std::make_shared<Estado>();
    auto todas = std::make_shared<std::vector<Opcion>>(std::move(opciones));
    auto filtrar = [e, todas] {
        e->vistas.clear();
        const std::string q = normalizar(e->buscado);
        for (int i = 0; i < (int)todas->size(); i++)
        {
            bool vale = true;
            size_t desde = 0;
            while (vale && desde < q.size())
            {
                size_t fin = q.find(' ', desde);
                if (fin == std::string::npos)
                    fin = q.size();
                if (fin > desde)
                    vale = (*todas)[i].clave.find(q.substr(desde, fin - desde)) != std::string::npos;
                desde = fin + 1;
            }
            if (vale)
                e->vistas.push_back(i);
        }
        e->pagina = 0;
    };
    filtrar();
    if (actual >= 0)
        e->pagina = actual / POR_PAGINA, e->enfocar = actual % POR_PAGINA;
    const bool buscable = !todas->empty() && !(*todas)[0].clave.empty();
    auto buscar = [e, filtrar] {
        std::string q = e->buscado;
        if (pedirTexto("Buscar", "Escribe parte del nombre", q))
        {
            e->buscado = q;
            filtrar();
            if (e->vistas.empty())
                avisar("No hay nada que se llame «" + q + "».", true);
        }
    };
    abrir([=](Vista& v) {
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = titulo;
        v.explicacion = (e->buscado.empty() ? explicacion : "Con «" + e->buscado + "»: " + std::to_string(total) + ".")
            + (paginas > 1 ? " (página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ")" : "");
        for (int k = 0; k < POR_PAGINA && e->pagina * POR_PAGINA + k < total; k++)
        {
            const int i = e->vistas[e->pagina * POR_PAGINA + k];
            v.b.push_back({{60 + (k % COLUMNAS) * 390, 150 + (k / COLUMNAS) * 62, 370, 54}, (*todas)[i].nombre, [elegido, i] {
                volver();
                elegido(i);
            }, true, i == actual});
        }
        if (e->enfocar >= 0 && e->enfocar < (int)v.b.size())
        {
            enfocar(e->enfocar);
            e->enfocar = -1;
        }
        const int y = ABAJO - 72;
        v.b.push_back({{60, y, 240, 60}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
        v.b.push_back({{316, y, 240, 60}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
        if (buscable)
        {
            v.b.push_back({{572, y, 300, 60}, e->buscado.empty() ? "Buscar…" : "Ver todos", [e, buscar, filtrar] {
                if (e->buscado.empty())
                    buscar();
                else
                    e->buscado.clear(), filtrar();
            }});
            v.x = buscar;
        }
        v.b.push_back({{960, y, 260, 60}, "Volver", [] { volver(); }});
        v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + (d > 0 ? 1 : -1) * (std::abs(d) > 1 ? 5 : 1), 0, paginas - 1); };
        if (buscable)
            v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    });
}

// ------------------------------------------------------------------------------------------- paredes y suelo
std::string resumenCuarto(int hueco)
{
    return nombreParte(parteCuarto(partida::principal(), hueco, CuartoPared)) + " · " + nombreParte(parteCuarto(partida::principal(), hueco, CuartoSuelo));
}

void cuarto(int hueco)
{
    abrir([hueco](Vista& v) {
        v.titulo = "Paredes y suelo de " + nombre(hueco);
        v.explicacion = "Toca la pared o el suelo (o elige a la derecha) y escoge el nuevo.";
        const int f = foco();
        const int elegido = f == 0 ? CuartoPared : f == 1 ? CuartoSuelo : f == 2 ? CuartoAcento : -1;
        v.dibujo = [hueco, elegido] { dibujarCuarto(hueco, elegido); };
        v.entrada = [hueco](u64, u64, int tx, int ty) {
            const int p = parteTocada(hueco, tx, ty);
            if (p < 0)
                return false;
            elegirParte(hueco, p);
            return true;
        };
        const char* titulos[3] = {"Pared", "Suelo", "Pared de acento"};
        const int partes[3] = {CuartoPared, CuartoSuelo, CuartoAcento};
        for (int i = 0; i < 3; i++)
        {
            const int p = partes[i];
            Boton b{{660, 160 + i * 120, 560, 100}, titulos[i], [hueco, p] { elegirParte(hueco, p); }};
            b.detalle = p == CuartoAcento && !hayAcento(hueco) ? "No tiene (una de las paredes, con otro papel)" : nombreParte(parteCuarto(partida::principal(), hueco, p));
            v.b.push_back(b);
        }
        v.b.push_back({{660, ABAJO - 76, 560, 64}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------- la casa por fuera
std::string resumenCasa(int hueco)
{
    const int k = casaDe(partida::principal(), hueco);
    if (k < 0)
        return "No tiene casa";
    return nombreAspecto(FACHADAS, NUM_FACHADAS, parteCasa(partida::principal(), k, CasaFachada)) + " · "
        + nombreAspecto(TEJADOS, NUM_TEJADOS, parteCasa(partida::principal(), k, CasaTejado));
}

void casa(int hueco)
{
    abrir([hueco](Vista& v) {
        const int k = casaDe(partida::principal(), hueco);
        if (k < 0)
        {
            volver();
            avisar(nombre(hueco) + " todavía no tiene casa en la isla.", true);
            return;
        }
        auto& m = partida::principal();
        v.titulo = "Casa de " + nombre(hueco);
        v.explicacion = "Cómo se ve por fuera la casa de " + nombre(hueco) + " y lo que cuelga en su puerta.";
        auto aspecto = [k](const char* que, const AspectoCasa* lista, int n, int parte) {
            std::vector<Opcion> op;
            int actual = -1;
            const uint16_t ahora = parteCasa(partida::principal(), k, parte);
            for (int i = 0; i < n; i++)
            {
                op.push_back({lista[i].nombre, ""});
                if (lista[i].id == ahora)
                    actual = i;
            }
            elegirLargo(que, "Elige cómo será.", op, actual, [k, lista, parte](int i) {
                ponerParteCasa(partida::principal(), k, parte, lista[i].id);
                cambio(std::string("Puesto: ") + lista[i].nombre);
            });
        };
        std::vector<Boton> b;
        auto boton = [&](const std::string& t, const std::string& d, std::function<void()> f, bool activo = true) {
            Boton x{{}, t, std::move(f), activo};
            x.detalle = d;
            b.push_back(x);
        };
        boton("Tejado", nombreAspecto(TEJADOS, NUM_TEJADOS, parteCasa(m, k, CasaTejado)), [aspecto] { aspecto("Tejado", TEJADOS, NUM_TEJADOS, CasaTejado); });
        boton("Fachada", nombreAspecto(FACHADAS, NUM_FACHADAS, parteCasa(m, k, CasaFachada)), [aspecto] { aspecto("Fachada", FACHADAS, NUM_FACHADAS, CasaFachada); });
        const uint16_t puerta = parteCasa(m, k, CasaPuerta);
        const int pk = puertaDe(puerta);
        const bool recta = pk >= 0 && PUERTAS[pk].recta == puerta && PUERTAS[pk].recta != PUERTAS[pk].arco;
        boton("Puerta", nombrePuerta(puerta), [k, pk, recta] {
            std::vector<Opcion> op;
            for (int i = 0; i < NUM_PUERTAS; i++)
                op.push_back({PUERTAS[i].nombre, ""});
            elegirLargo("Puerta", "Elige la puerta (la forma, en arco o recta, se elige aparte).", op, pk, [k, recta](int i) {
                // Se queda con la forma que tenía, si esa puerta la tiene.
                ponerParteCasa(partida::principal(), k, CasaPuerta, recta ? PUERTAS[i].recta : PUERTAS[i].arco);
                cambio(std::string("Puesta: ") + PUERTAS[i].nombre);
            });
        });
        boton("Forma de la puerta", recta ? "Recta (pulsa para en arco)" : "En arco (pulsa para recta)", [k, pk, recta] {
            ponerParteCasa(partida::principal(), k, CasaPuerta, recta ? PUERTAS[pk].arco : PUERTAS[pk].recta);
            cambio(recta ? "Puerta en arco" : "Puerta recta");
        }, pk >= 0 && PUERTAS[pk].recta != PUERTAS[pk].arco);
        const Objeto deco = decoracionPuerta(m, k);
        boton("Decoración de la puerta", deco.vacio() ? "No hay nada en la puerta" : objetos::nombre(deco), [k] {
            objetos::buscar([k](const Objeto& o) {
                ponerDecoracionPuerta(partida::principal(), k, o);
                cambio("En la puerta: " + objetos::nombre(o));
            }, "la puerta");
        });
        boton("Quitar la decoración", "Que no cuelgue nada", [k] {
            ponerDecoracionPuerta(partida::principal(), k, Objeto{});
            cambio("Ya no cuelga nada en la puerta");
        }, !deco.vacio());
        boton("Exportar la casa", "A la tarjeta SD", [hueco] { exportarCasaSd(hueco); });
        boton("Importar una casa", "De la tarjeta SD", [hueco] { importarCasaSd(hueco); });
        for (int i = 0; i < (int)b.size(); i++)
        {
            b[i].r = {60 + (i % 2) * 590, 140 + (i / 2) * 106, 570, 94};
            v.b.push_back(b[i]);
        }
        v.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
    });
}

} // namespace vecinos
