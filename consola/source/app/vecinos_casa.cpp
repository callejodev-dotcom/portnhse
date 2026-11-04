// La casa de cada vecino: las paredes y el suelo de dentro, con un dibujo de la habitación (como RoomWindow de NHSE para
// Mac), y cómo se ve por fuera y lo que cuelga en la puerta (como VillagerHouseWindow), con su archivo en la tarjeta SD.
#include <algorithm>
#include <cmath>
#include <memory>
#include "../core/vecino.hpp"
#include "../core/vecino_tablas.hpp"
#include "casas_fuera.hpp"
#include "dibujos.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "retratos.hpp"
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
// La habitación vista de frente, como en el juego: la pared del fondo, las de los lados y el suelo, cada una cubierta
// con el dibujo de su papel o su suelo (el de dibujos.bin, en baldosas; si no está, el color que dice su nombre), un
// zócalo, una ventana y el vecino en medio.
constexpr int L = 76, R = 604, T = 124, B = 586;   // fuera
constexpr int l = 186, r = 494, t = 168, b = 398;  // la pared del fondo
// Of each picture, only the part that is the pattern: the walls are a hanging sheet (rolled on the left, folded below),
// the floors a square.
struct Recorte { float u0, v0, u1, v1; };
constexpr Recorte DE_PARED = {0.42f, 0.16f, 0.80f, 0.54f}, DE_SUELO = {0.22f, 0.22f, 0.78f, 0.78f};

int entre(int a, int c, int i, int n) { return a + (c - a) * i / n; }

SDL_Color rgb(uint32_t c) { return {(Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255}; }

// El color que dice un nombre («Pared cerámica amarilla»), o el que se da.
SDL_Color colorDelNombre(const std::string& n, SDL_Color si_no)
{
    static const std::pair<const char*, uint32_t> COLORES[] = {{"azul", 0x7F9CD0}, {"roj", 0xC9605A}, {"verde", 0x86B072},
        {"amarill", 0xEBD27A}, {"rosa", 0xEBAFC2}, {"blanc", 0xF4F1EA}, {"negr", 0x55505A}, {"morad", 0x9C80C8}, {"lila", 0xBBA8DC},
        {"naranja", 0xE6A070}, {"gris", 0xA8A8AE}, {"marron", 0x9A6E4C}, {"beis", 0xDECDAA}, {"crema", 0xF0E4C6}, {"celeste", 0xA6D2EC},
        {"dorad", 0xD8B852}, {"madera", 0xB98A5C}, {"pino", 0xD6B07A}, {"roble", 0xA77B4F}, {"ladrillo", 0xB6644E}, {"cemento", 0xB4B2AC},
        {"marmol", 0xE8E6E2}, {"cesped", 0x8DBF6A}, {"hierba", 0x8DBF6A}, {"agua", 0x86C2E0}, {"arena", 0xE6D3A4}, {"turquesa", 0x6CC6C0}};
    const std::string k = normalizar(n);
    size_t mejor = std::string::npos;
    SDL_Color c = si_no;
    for (const auto& p : COLORES)
    {
        const size_t i = k.find(p.first);
        if (i != std::string::npos && (mejor == std::string::npos || i > mejor))
            mejor = i, c = rgb(p.second);
    }
    return c;
}

SDL_FPoint mezcla(SDL_FPoint a, SDL_FPoint c, float f) { return {a.x + (c.x - a.x) * f, a.y + (c.y - a.y) * f}; }

// Un cuadrilátero (arriba izquierda, arriba derecha, abajo derecha, abajo izquierda) cubierto con «tex» en nx × ny
// baldosas (o de su color si no hay dibujo), oscurecido con «luz». «fondo»: las filas se juntan hacia arriba (el suelo
// se aleja).
void mosaico(SDL_Texture* tex, const Recorte& rc, SDL_Color color, SDL_FPoint p00, SDL_FPoint p10, SDL_FPoint p11, SDL_FPoint p01, int nx,
    int ny, Uint8 luz, bool fondo = false)
{
    if (!tex)
    {
        const SDL_Color c = {(Uint8)(color.r * luz / 255), (Uint8)(color.g * luz / 255), (Uint8)(color.b * luz / 255), 255};
        poligono({p00, p10, p11, p01}, c);
        return;
    }
    std::vector<SDL_Vertex> v;
    const SDL_Color c = {luz, luz, luz, 255};
    auto fila = [&](int j) {
        const float f = (float)j / ny;
        return fondo ? f * f * 0.45f + f * 0.55f : f;
    };
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
        {
            const float x0 = (float)i / nx, x1 = (float)(i + 1) / nx, y0 = fila(j), y1 = fila(j + 1);
            auto punto = [&](float x, float y) { return mezcla(mezcla(p00, p10, x), mezcla(p01, p11, x), y); };
            const SDL_Vertex a = {punto(x0, y0), c, {rc.u0, rc.v0}}, bb = {punto(x1, y0), c, {rc.u1, rc.v0}},
                             cc = {punto(x1, y1), c, {rc.u1, rc.v1}}, d = {punto(x0, y1), c, {rc.u0, rc.v1}};
            v.insert(v.end(), {a, bb, cc, a, cc, d});
        }
    SDL_RenderGeometry(g_r, tex, v.data(), (int)v.size(), nullptr, 0);
}

// Un borde de 4 puntos alrededor de un polígono (para lo elegido).
void contorno(const std::vector<SDL_FPoint>& p, SDL_Color c)
{
    for (size_t i = 0; i < p.size(); i++)
    {
        const SDL_FPoint a = p[i], d = p[(i + 1) % p.size()];
        const float dx = d.x - a.x, dy = d.y - a.y, n = std::sqrt(dx * dx + dy * dy);
        if (n < 1)
            continue;
        const float ox = -dy / n * 2.5f, oy = dx / n * 2.5f;
        poligono({{a.x + ox, a.y + oy}, {d.x + ox, d.y + oy}, {d.x - ox, d.y - oy}, {a.x - ox, a.y - oy}}, c);
    }
}

// Una etiqueta blanca con un texto, centrada en x.
void etiqueta(const std::string& s, int x, int y)
{
    const std::string e = encajar(s, 280, g_pequena);
    const int w = anchoTexto(e, g_pequena) + 28, h = TTF_FontHeight(g_pequena) + 8;
    redondo({x - w / 2, y - h / 2 + 2, w, h}, h / 2, {0, 0, 0, 40});
    redondo({x - w / 2, y - h / 2, w, h}, h / 2, BLANCO);
    texto(e, x, y, MARRON, g_pequena, Centro);
}

void dibujarCuarto(int hueco, int elegido)
{
    auto& m = partida::principal();
    const bool acento = hayAcento(hueco);
    const uint16_t pared = parteCuarto(m, hueco, CuartoPared), suelo = parteCuarto(m, hueco, CuartoSuelo);
    const uint16_t fondo = acento ? parteCuarto(m, hueco, CuartoAcento) : pared;
    SDL_Texture *tPared = dibujos::de(pared), *tSuelo = dibujos::de(suelo), *tFondo = dibujos::de(fondo);
    const SDL_Color cPared = colorDelNombre(nombreParte(pared), rgb(0xE6D8B8)), cSuelo = colorDelNombre(nombreParte(suelo), rgb(0xB98A5C));
    const SDL_Color cFondo = acento ? colorDelNombre(nombreParte(fondo), rgb(0xD2E0CC)) : cPared;
    const SDL_FPoint TL = {L, T}, TR = {R, T}, BR = {R, B}, BL = {L, B}, tl = {l, t}, tr = {r, t}, br = {r, b}, bl = {l, b};

    // A frame, as the cards (the room inside).
    redondo({L - 4, T - 4, R - L + 8, B - T + 8}, 10, {0xD1, 0xD1, 0xD6, 255});
    // The ceiling (light) and the walls: the sides a bit darker than the back one, as the light of the game.
    poligono({TL, TR, tr, tl}, {246, 242, 234, 255});
    mosaico(tPared, DE_PARED, cPared, TL, tl, bl, BL, 2, 5, 205);
    mosaico(tPared, DE_PARED, cPared, tr, TR, BR, br, 2, 5, 205);
    mosaico(tFondo, DE_PARED, cFondo, tl, tr, br, bl, 5, 4, 245);
    mosaico(tSuelo, DE_SUELO, cSuelo, bl, br, BR, BL, 6, 4, 235, true);
    // The skirting board and the corners.
    const SDL_Color zocalo = {120, 92, 70, 255};
    poligono({{l, b - 8}, {r, b - 8}, {r, b}, {l, b}}, zocalo);
    poligono({{L, B - 22}, {l, b - 8}, {l, b}, {L, B}}, zocalo);
    poligono({{R, B - 22}, {r, b - 8}, {r, b}, {R, B}}, zocalo);
    const SDL_Color esquina = {0, 0, 0, 50};
    contorno({tl, tr, br, bl}, esquina);
    // A window on the back wall.
    const SDL_Rect ventana = {(l + r) / 2 - 56, t + 40, 112, 92};
    redondo({ventana.x - 6, ventana.y - 6, ventana.w + 12, ventana.h + 12}, 8, BLANCO);
    redondo(ventana, 4, {170, 214, 238, 255});
    rect({ventana.x, ventana.y + ventana.h / 2 - 2, ventana.w, 4}, BLANCO);
    rect({ventana.x + ventana.w / 2 - 2, ventana.y, 4, ventana.h}, BLANCO);
    poligono({{(float)ventana.x + 14, (float)ventana.y + ventana.h / 2 - 8}, {(float)ventana.x + 40, (float)ventana.y + 8}, {(float)ventana.x + 52, (float)ventana.y + 8},
        {(float)ventana.x + 26, (float)ventana.y + ventana.h / 2 - 8}}, {255, 255, 255, 110});
    // The villager, standing in the middle, with its shadow.
    if (SDL_Texture* cara = retratos::deHueco(hueco))
    {
        const int lado = 112, cx = (L + R) / 2, pie = B - 62;
        std::vector<SDL_FPoint> sombra;
        for (int k = 0; k < 24; k++)
            sombra.push_back({(float)(cx + 46 * std::cos(k * 6.2831853 / 24)), (float)(pie + 10 * std::sin(k * 6.2831853 / 24))});
        poligono(sombra, {0, 0, 0, 55});
        const SDL_Rect d = {cx - lado / 2, pie - lado + 4, lado, lado};
        SDL_RenderCopy(g_r, cara, nullptr, &d);
    }
    // What is chosen, with a blue edge.
    if (elegido == CuartoSuelo)
        contorno({bl, br, BR, BL}, AZUL);
    else if (elegido == CuartoPared && !acento)
        contorno({TL, TR, BR, BL}, AZUL), contorno({tl, tr, br, bl}, AZUL);
    else if (elegido == CuartoPared)
        contorno({TL, tl, bl, BL}, AZUL), contorno({tr, TR, BR, br}, AZUL);
    else if (elegido == CuartoAcento)
        contorno({tl, tr, br, bl}, AZUL);
    // Labels.
    etiqueta((acento ? "Acento: " : "") + nombreParte(fondo), (l + r) / 2, b - 40);
    etiqueta(nombreParte(suelo), (L + R) / 2, B - 24);
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

// Como la lista de la derecha de RoomWindow del Mac: cada pared o suelo con su dibujo, y un buscador que filtra mientras
// se escribe.
void elegirParte(int hueco, int parte)
{
    const bool esSuelo = parte == CuartoSuelo;
    const ParteHabitacion* lista = esSuelo ? SUELOS : PAREDES;
    const int n = esSuelo ? NUM_SUELOS : NUM_PAREDES;
    auto vistas = std::make_shared<std::vector<int>>();
    const char* titulos[3] = {"Elige otra pared de acento", "Elige otra pared", "Elige otro suelo"};
    Buscador bu;
    bu.titulo = titulos[parte];
    bu.explicacion = esSuelo ? "Escribe parte del nombre (madera, baldosas…) y toca uno para ponerlo." : "Escribe parte del nombre (madera, ladrillo…) y toca una para ponerla.";
    bu.filtrar = [vistas, lista, n](const std::string& q0) {
        const std::string q = normalizar(q0);
        vistas->clear();
        for (int i = 0; i < n; i++)
            if (std::string(lista[i].clave).find(q) != std::string::npos)
                vistas->push_back(i);
        return (int)vistas->size();
    };
    bu.fila = [vistas, lista, hueco, parte](int k) {
        const int i = (*vistas)[k];
        Fila f{lista[i].nombre, [hueco, parte, lista, i] {
            volver();
            ponerParteCuarto(partida::principal(), hueco, parte, lista[i].id);
            cambio(std::string("Puesto: ") + lista[i].nombre);
        }};
        f.marcado = parteCuarto(partida::principal(), hueco, parte) == lista[i].id;
        f.imagen = dibujos::de(lista[i].id);
        return f;
    };
    abrir(buscador(bu));
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

// Como RoomWindow del Mac: a la izquierda la habitación dibujada (se toca la pared o el suelo); a la derecha una tarjeta
// por cada parte, con su dibujo.
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
            const uint16_t id = parteCuarto(partida::principal(), hueco, p);
            const bool sin = p == CuartoAcento && !hayAcento(hueco);
            Boton b{{636, 120 + i * 112, 584, 100}, sin ? "No tiene" : nombreParte(id), [hueco, p] { elegirParte(hueco, p); }};
            b.grande = true;
            b.detalle = sin ? std::string("Pared de acento (otro papel en una pared)") : titulos[i];
            b.imagen = sin ? dibujos::menu("Wall") : dibujos::de(id);
            v.b.push_back(b);
        }
        v.b.push_back({{636, 470, 584, 56}, "Valores en bruto (expertos)…", [hueco] { brutoCuarto(hueco); }});
        v.b.push_back({{960, ABAJO - 60, 260, 52}, "Volver", [] { volver(); }});
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

namespace {
// Elegir un tejado, una fachada o una puerta: a la izquierda la casa con el que tiene el cursor (para verlo puesto antes
// de elegirlo), a la derecha la lista con su dibujo, en páginas.
struct ConDibujo {
    std::string nombre;
    SDL_Texture* (*dibujo)(casas_fuera::Parte, uint16_t); // nullptr: none
    casas_fuera::Parte parte;
    uint16_t id;
};
void elegirConDibujos(const std::string& titulo, int casa, std::vector<ConDibujo> opciones, int actual, std::function<void(int)> elegido)
{
    constexpr int COLS = 2, FILAS = 7, POR_PAGINA = COLS * FILAS;
    auto pagina = std::make_shared<int>(actual >= 0 ? actual / POR_PAGINA : 0);
    auto enfoque = std::make_shared<int>(actual >= 0 ? actual % POR_PAGINA : -1);
    auto todas = std::make_shared<std::vector<ConDibujo>>(std::move(opciones));
    abrir([=](Vista& v) {
        const int total = (int)todas->size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        *pagina = std::clamp(*pagina, 0, paginas - 1);
        v.titulo = titulo;
        v.explicacion = "A la izquierda se ve puesto el que tiene el cursor. " + std::to_string(total) + " para elegir" +
            (paginas > 1 ? " · página " + std::to_string(*pagina + 1) + " de " + std::to_string(paginas) + " (L y R)." : ".");
        const int f = foco();
        int mirado = actual;
        for (int k = 0; k < POR_PAGINA && *pagina * POR_PAGINA + k < total; k++)
        {
            const int i = *pagina * POR_PAGINA + k;
            const ConDibujo& o = (*todas)[i];
            Boton b{{580 + (k % COLS) * 324, 112 + (k / COLS) * 68, 316, 60}, o.nombre, [elegido, i] {
                volver();
                elegido(i);
            }, true, i == actual};
            b.imagen = o.dibujo ? o.dibujo(o.parte, o.id) : nullptr;
            if (!b.imagen)
                b.color = casas_fuera::color(o.parte, o.id);
            if (k == f)
                mirado = i;
            v.b.push_back(b);
        }
        if (*enfoque >= 0 && *enfoque < (int)v.b.size())
        {
            enfocar(*enfoque);
            *enfoque = -1;
        }
        auto& m = partida::principal();
        uint16_t partes[3] = {parteCasa(m, casa, CasaTejado), parteCasa(m, casa, CasaFachada), parteCasa(m, casa, CasaPuerta)};
        std::string nombreMirado;
        if (mirado >= 0)
        {
            const ConDibujo& o = (*todas)[mirado];
            partes[o.parte == casas_fuera::Tejado ? 0 : o.parte == casas_fuera::Fachada ? 1 : 2] = o.id;
            nombreMirado = o.nombre;
        }
        v.dibujo = [partes, nombreMirado] {
            panel({60, 112, 500, 476}, nombreMirado.empty() ? "Ahora" : nombreMirado);
            const SDL_Rect r = {78, 164, 464, 404};
            redondo({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 12, {0xD1, 0xD1, 0xD6, 255});
            casas_fuera::dibujar(r, partes[0], partes[1], partes[2]);
        };
        const int y = ABAJO - 60;
        v.b.push_back({{580, y, 200, 52}, "Anteriores", [pagina] { (*pagina)--; }, *pagina > 0});
        v.b.push_back({{790, y, 200, 52}, "Siguientes", [pagina] { (*pagina)++; }, *pagina + 1 < paginas});
        v.b.push_back({{1020, y, 200, 52}, "Volver", [] { volver(); }});
        v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + (d > 0 ? 1 : -1) * (std::abs(d) > 1 ? 5 : 1), 0, paginas - 1); };
        v.teclas = {{"A", "Elegir"}, {"L R", "Página"}, {"B", "Volver"}};
    });
}
} // namespace

// Como VillagerHouseWindow del Mac: la casa dibujada (con los dibujos del juego si está fachadas.bin) y, a la derecha, el
// tejado, la fachada y la puerta con su dibujo, la forma de la puerta y lo que cuelga en ella.
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
        const uint16_t tejado = parteCasa(m, k, CasaTejado), fachada = parteCasa(m, k, CasaFachada), puerta = parteCasa(m, k, CasaPuerta);
        const Objeto deco = decoracionPuerta(m, k);
        SDL_Texture* cara = retratos::deHueco(hueco);
        SDL_Texture* tDeco = deco.vacio() ? nullptr : dibujos::de(deco);
        v.dibujo = [tejado, fachada, puerta, cara, tDeco] {
            panel({60, 112, 556, 476}, "Por fuera");
            const SDL_Rect r = {80, 164, 516, 404};
            redondo({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 12, {0xD1, 0xD1, 0xD6, 255});
            casas_fuera::dibujar(r, tejado, fachada, puerta);
            if (cara)
            {
                const SDL_Rect d = {r.x + 10, r.y + r.h - 112, 104, 104};
                SDL_RenderCopy(g_r, cara, nullptr, &d);
            }
            if (tDeco)
            {
                // What hangs on the door, in a white circle at the corner.
                redondo({r.x + r.w - 86, r.y + 10, 76, 76}, 38, BLANCO);
                const SDL_Rect d = {r.x + r.w - 78, r.y + 18, 60, 60};
                SDL_RenderCopy(g_r, tDeco, nullptr, &d);
            }
        };
        auto aspecto = [k](const char* que, const AspectoCasa* lista, int n, int parte, casas_fuera::Parte dib) {
            std::vector<ConDibujo> op;
            int actual = -1;
            const uint16_t ahora = parteCasa(partida::principal(), k, parte);
            for (int i = 0; i < n; i++)
            {
                const uint16_t id = lista[i].id;
                op.push_back({lista[i].nombre, dib == casas_fuera::Tejado ? nullptr : casas_fuera::dibujo, dib, id});
                if (id == ahora)
                    actual = i;
            }
            elegirConDibujos(que, k, std::move(op), actual, [k, lista, parte](int i) {
                ponerParteCasa(partida::principal(), k, parte, lista[i].id);
                cambio(std::string("Puesto: ") + lista[i].nombre);
            });
        };
        auto tarjeta = [&](int y, const std::string& t, const std::string& d, SDL_Texture* img, std::function<void()> f, int w = 584) {
            Boton b{{636, y, w, 88}, t, std::move(f)};
            b.grande = true;
            b.detalle = d;
            b.imagen = img ? img : dibujos::menu("Porch");
            v.b.push_back(b);
        };
        tarjeta(112, nombreAspecto(TEJADOS, NUM_TEJADOS, tejado), "Tejado", nullptr,
            [aspecto] { aspecto("Tejado", TEJADOS, NUM_TEJADOS, CasaTejado, casas_fuera::Tejado); });
        // The roofs have no picture of their own: its colour.
        v.b.back().imagen = nullptr;
        v.b.back().color = casas_fuera::color(casas_fuera::Tejado, tejado);
        tarjeta(208, nombreAspecto(FACHADAS, NUM_FACHADAS, fachada), "Fachada", casas_fuera::dibujo(casas_fuera::Fachada, fachada),
            [aspecto] { aspecto("Fachada", FACHADAS, NUM_FACHADAS, CasaFachada, casas_fuera::Fachada); });
        const int pk = puertaDe(puerta);
        const bool recta = pk >= 0 && PUERTAS[pk].recta == puerta && PUERTAS[pk].recta != PUERTAS[pk].arco;
        const bool dosFormas = pk >= 0 && PUERTAS[pk].recta != PUERTAS[pk].arco;
        tarjeta(304, nombrePuerta(puerta), "Puerta", casas_fuera::dibujo(casas_fuera::Puerta, puerta), [k, pk, recta] {
            std::vector<ConDibujo> op;
            for (int i = 0; i < NUM_PUERTAS; i++)
            {
                const uint16_t id = PUERTAS[i].arco;
                op.push_back({PUERTAS[i].nombre, casas_fuera::dibujo, casas_fuera::Puerta, recta ? PUERTAS[i].recta : id});
            }
            elegirConDibujos("Puerta", k, std::move(op), pk, [k, recta](int i) {
                // It keeps the shape it had, if that door has it.
                ponerParteCasa(partida::principal(), k, CasaPuerta, recta ? PUERTAS[i].recta : PUERTAS[i].arco);
                cambio(std::string("Puesta: ") + PUERTAS[i].nombre);
            });
        });
        // The shape of the door, as the two buttons of the Mac.
        v.b.push_back({{636, 400, 287, 52}, "En arco", [k, pk] {
            ponerParteCasa(partida::principal(), k, CasaPuerta, PUERTAS[pk].arco);
            cambio("Puerta en arco");
        }, dosFormas, !recta});
        v.b.push_back({{933, 400, 287, 52}, "Recta", [k, pk] {
            ponerParteCasa(partida::principal(), k, CasaPuerta, PUERTAS[pk].recta);
            cambio("Puerta recta");
        }, dosFormas, recta});
        tarjeta(464, deco.vacio() ? "Nada" : objetos::nombre(deco), "En la puerta", tDeco ? tDeco : dibujos::menu("Present"), [k] {
            objetos::buscar([k](const Objeto& o) {
                ponerDecoracionPuerta(partida::principal(), k, o);
                cambio("En la puerta: " + objetos::nombre(o));
            }, "la puerta");
        }, 440);
        v.b.push_back({{1086, 464, 134, 88}, "Quitar", [k] {
            ponerDecoracionPuerta(partida::principal(), k, Objeto{});
            cambio("Ya no cuelga nada en la puerta");
        }, !deco.vacio()});
        const int y = ABAJO - 60;
        v.b.push_back({{60, y, 240, 52}, "Exportar casa…", [hueco] { exportarCasaSd(hueco); }});
        v.b.push_back({{312, y, 240, 52}, "Importar casa…", [hueco] { importarCasaSd(hueco); }});
        v.b.push_back({{564, y, 340, 52}, "Valores en bruto (expertos)…", [hueco] { brutoCasa(hueco); }});
        v.b.push_back({{960, y, 260, 52}, "Volver", [] { volver(); }});
    });
}

} // namespace vecinos
