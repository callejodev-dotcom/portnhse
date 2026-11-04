// «Memoria en bruto» de la sección «Avanzado»: main.dat o el personal.dat de un jugador en hexadecimal, 256 bytes por
// página, con un cursor que se mueve con la cruceta; cambiar bytes con el teclado e ir a una posición.
#include <algorithm>
#include <memory>
#include <set>
#include "../core/avanzado.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

constexpr int COLUMNAS = 16, FILAS = 16, PAGINA = COLUMNAS * FILAS;
// A white card with the table (as the hex editor of the Mac): positions, the 16 bytes and the same as letters.
constexpr int X_TABLA = 60, W_TABLA = W - 120, Y_TABLA = 108;
constexpr int X_POS = 84, X_HEX = 206, ANCHO_HEX = 40, X_TEXTO = 892, ANCHO_TEXTO = 19;
constexpr int Y_CABECERA = Y_TABLA + 10, Y_REJILLA = Y_TABLA + 44, ALTO_FILA = 24;
constexpr int H_TABLA = Y_REJILLA + FILAS * ALTO_FILA + 10 - Y_TABLA;
constexpr int Y_INFO = Y_TABLA + H_TABLA + 12;

const SDL_Color FONDO_CABECERA = {0xF5, 0xF5, 0xF7, 255};
const SDL_Color RAYA = {0xFA, 0xFA, 0xFC, 255};
const SDL_Color FILA_CURSOR = {0xE8, 0xF1, 0xFF, 255};
const SDL_Color CERO = {0xAE, 0xAE, 0xB2, 255};      // «00», lighter: the rest stands out
const SDL_Color NARANJA = {0xE8, 0x6A, 0x10, 255};   // changed and not saved yet
const SDL_Color LINEA = {0xD1, 0xD1, 0xD6, 255};

int xHex(int c) { return X_HEX + c * ANCHO_HEX + (c >= 8 ? 12 : 0); }

struct Estado {
    int archivo = 0; // 0 = main.dat, 1 + j = personal.dat of player j
    uint32_t pos = 0;
    std::set<uint64_t> cambiados; // (archivo << 32) | posición
};

std::vector<uint8_t>& datos(const Estado& e) { return e.archivo == 0 ? partida::principal() : partida::personal(e.archivo - 1); }

std::string nombreArchivo(int a)
{
    return a == 0 ? std::string("main.dat (la isla)") : "personal.dat de " + partida::jugador(a - 1).nombre;
}

std::string dosCifras(uint8_t b)
{
    const char* h = "0123456789ABCDEF";
    return std::string{h[b >> 4], h[b & 15]};
}

void cambiar(std::shared_ptr<Estado> e)
{
    auto& d = datos(*e);
    std::string t = dosCifras(d[e->pos]);
    if (!pedirTexto("Bytes desde " + hex(e->pos, 6) + " (hexadecimal)", "Uno o varios, por ejemplo: 1F o 01 A0 FF", t, 96))
        return;
    std::vector<uint8_t> b;
    if (!leerHex(t, b))
    {
        avisar("Eso no son bytes en hexadecimal (cifras 0-9 y letras A-F, de dos en dos).", true);
        return;
    }
    const size_t n = std::min(b.size(), d.size() - e->pos);
    int distintos = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (d[e->pos + i] == b[i])
            continue;
        d[e->pos + i] = b[i];
        e->cambiados.insert(((uint64_t)e->archivo << 32) | (e->pos + i));
        distintos++;
    }
    if (distintos == 0)
    {
        avisar("Ya tenía esos bytes.");
        return;
    }
    if (e->archivo == 0)
        partida::cambiaPrincipal();
    else
        partida::cambiaPersonal(e->archivo - 1);
    cambiado(std::to_string(distintos) + (distintos == 1 ? " byte cambiado" : " bytes cambiados") + (n < b.size() ? " (el resto no cabe)" : ""));
}

void irA(std::shared_ptr<Estado> e)
{
    std::string t = hex(e->pos, 6);
    if (!pedirTexto("Ir a la posición (hexadecimal)", "Por ejemplo: 22ED00", t, 12))
        return;
    uint32_t p;
    if (!leerPosicion(t, p) || p >= datos(*e).size())
    {
        avisar("Esa posición no está en el archivo (va de 0 a " + hex((uint32_t)datos(*e).size() - 1) + ").", true);
        return;
    }
    e->pos = p;
}

void elegirArchivo(std::shared_ptr<Estado> e)
{
    std::vector<std::string> opciones;
    for (int a = 0; a <= (int)partida::jugadores.size(); a++)
        opciones.push_back(nombreArchivo(a));
    elegir("¿Qué archivo?", "Los datos sin cifrar de la partida.", opciones, e->archivo, [e](int a) {
        e->archivo = a;
        e->pos = 0;
    }, 1);
}

void mover(Estado& e, long d)
{
    const long tam = (long)datos(e).size();
    e.pos = (uint32_t)std::clamp<long>((long)e.pos + d, 0, tam - 1);
}

// The buttons along the bottom, drawn and touched by hand (the cross moves the cursor, not the focus).
struct BotonAbajo {
    const char* texto;
    std::function<void(std::shared_ptr<Estado>)> accion;
};
const std::vector<BotonAbajo>& botones()
{
    static const std::vector<BotonAbajo> b = {
        {"Cambiar…", cambiar},
        {"Ir a…", irA},
        {"Archivo…", elegirArchivo},
        {"Anterior", [](std::shared_ptr<Estado> e) { mover(*e, -PAGINA); }},
        {"Siguiente", [](std::shared_ptr<Estado> e) { mover(*e, PAGINA); }},
        {"Volver", [](std::shared_ptr<Estado>) { volver(); }},
    };
    return b;
}
SDL_Rect rectBoton(int i)
{
    const int ancho = (W - 120 - 5 * 14) / 6;
    return {60 + i * (ancho + 14), ABAJO - 56, ancho, 46};
}

Pantalla bruto()
{
    auto e = std::make_shared<Estado>();
    return [e](Vista& v) {
        auto& d = datos(*e);
        if (e->pos >= d.size())
            e->pos = 0;
        const uint32_t inicio = e->pos / PAGINA * PAGINA;
        v.titulo = "Memoria en bruto";
        v.explicacion = nombreArchivo(e->archivo) + " · página " + numero(inicio / PAGINA + 1) + " de " + numero((uint32_t)((d.size() + PAGINA - 1) / PAGINA)) +
            ". Un valor erróneo puede estropear la partida.";
        v.atras = [] { volver(); };
        v.dibujo = [e, inicio] {
            auto& d = datos(*e);
            const int fh = ui::alto(g_pequena);
            const int filaCursor = (int)((e->pos - inicio) / COLUMNAS), colCursor = (int)(e->pos % COLUMNAS);
            panel({X_TABLA, Y_TABLA, W_TABLA, H_TABLA}, "");
            // The header: the column numbers, the one of the cursor in blue.
            redondo({X_TABLA + 2, Y_TABLA + 2, W_TABLA - 4, Y_REJILLA - Y_TABLA - 6}, 12, FONDO_CABECERA);
            rect({X_TABLA + 2, Y_REJILLA - 16, W_TABLA - 4, 12}, FONDO_CABECERA);
            rect({X_TABLA + 2, Y_REJILLA - 4, W_TABLA - 4, 2}, LINEA);
            texto("Posición", X_POS, Y_CABECERA, MARRON_SUAVE, g_pequena);
            for (int c = 0; c < COLUMNAS; c++)
                texto(dosCifras((uint8_t)c), xHex(c) + 16, Y_CABECERA + fh / 2, c == colCursor ? AZUL : MARRON_SUAVE, g_pequena, Centro);
            texto("Texto", X_TEXTO, Y_CABECERA, MARRON_SUAVE, g_pequena);
            const int yFin = Y_REJILLA + FILAS * ALTO_FILA;
            rect({X_HEX - 16, Y_REJILLA - 4, 2, yFin - Y_REJILLA + 6}, LINEA);
            rect({X_TEXTO - 18, Y_REJILLA - 4, 2, yFin - Y_REJILLA + 6}, LINEA);
            for (int f = 0; f < FILAS; f++)
            {
                const uint32_t fila = inicio + f * COLUMNAS;
                if (fila >= d.size())
                    break;
                const int y = Y_REJILLA + f * ALTO_FILA;
                if (f == filaCursor)
                    rect({X_TABLA + 4, y, W_TABLA - 8, ALTO_FILA}, FILA_CURSOR);
                else if (f % 2)
                    rect({X_TABLA + 4, y, W_TABLA - 8, ALTO_FILA}, RAYA);
                const int ty = y + (ALTO_FILA - fh) / 2;
                texto(hex(fila, 6), X_POS, ty, f == filaCursor ? AZUL : MARRON_SUAVE, g_pequena);
                for (int c = 0; c < COLUMNAS && fila + c < d.size(); c++)
                {
                    const uint32_t p = fila + c;
                    const bool cursor = p == e->pos;
                    const bool cambiado = e->cambiados.count(((uint64_t)e->archivo << 32) | p) > 0;
                    const char ch = d[p] >= 32 && d[p] < 127 ? (char)d[p] : '.';
                    SDL_Color col = cambiado ? NARANJA : d[p] == 0 ? CERO : MARRON;
                    if (cursor)
                    {
                        redondo({xHex(c) - 2, y + 1, 36, ALTO_FILA - 2}, 6, cambiado ? NARANJA : AZUL);
                        redondo({X_TEXTO + c * ANCHO_TEXTO - 2, y + 1, ANCHO_TEXTO, ALTO_FILA - 2}, 4, cambiado ? NARANJA : AZUL);
                        col = BLANCO;
                    }
                    texto(dosCifras(d[p]), xHex(c) + 16, ty + fh / 2, col, g_pequena, Centro);
                    texto(std::string(1, ch), X_TEXTO + c * ANCHO_TEXTO + ANCHO_TEXTO / 2 - 2, ty + fh / 2, cursor ? BLANCO : cambiado ? NARANJA : ch == '.' ? CERO : MARRON_SUAVE,
                        g_pequena, Centro);
                }
            }
            // The value at the cursor, read as numbers (little endian, as the game keeps them), in little labels.
            auto le = [&d](uint32_t p, int n) {
                uint32_t v = 0;
                for (int i = n - 1; i >= 0; i--)
                    v = (v << 8) | (p + i < d.size() ? d[p + i] : 0);
                return v;
            };
            const uint32_t p = e->pos;
            const std::pair<std::string, std::string> datosCursor[] = {{"Posición", hex(p, 6)}, {"Byte", std::to_string(d[p])},
                {"16 bits", std::to_string(le(p, 2))}, {"32 bits", std::to_string(le(p, 4))}};
            int x = X_TABLA;
            for (const auto& dc : datosCursor)
            {
                const int w1 = anchoTexto(dc.first, g_pequena), w2 = anchoTexto(dc.second, g_negrita);
                const int w = w1 + w2 + 40;
                redondo({x, Y_INFO, w, 40}, 20, CREMA_OSCURA);
                texto(dc.first, x + 14, Y_INFO + (40 - fh) / 2, MARRON_SUAVE, g_pequena);
                texto(dc.second, x + 26 + w1, Y_INFO + (40 - ui::alto(g_negrita)) / 2, MARRON, g_negrita);
                x += w + 12;
            }
            if (!e->cambiados.empty())
                texto("En naranja, lo cambiado", W - 60, Y_INFO + (40 - fh) / 2, NARANJA, g_pequena, Derecha);
            for (size_t i = 0; i < botones().size(); i++)
                dibujar({rectBoton((int)i), botones()[i].texto, nullptr}, false);
        };
        v.entrada = [e](u64 down, u64, int tx, int ty) {
            if (tx >= 0)
            {
                for (size_t i = 0; i < botones().size(); i++)
                {
                    const SDL_Rect r = rectBoton((int)i);
                    if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h)
                    {
                        avisar("");
                        botones()[i].accion(e);
                        return true;
                    }
                }
                const int f = (ty - Y_REJILLA) / ALTO_FILA;
                int c = -1;
                for (int k = 0; k < COLUMNAS; k++)
                    if (tx >= xHex(k) - 4 && tx < xHex(k) + 36)
                        c = k;
                if (ty >= Y_REJILLA && f < FILAS && c >= 0)
                {
                    const uint32_t p = e->pos / PAGINA * PAGINA + f * COLUMNAS + c;
                    if (p < datos(*e).size())
                        e->pos = p;
                    return true;
                }
                return false;
            }
            if (down & HidNpadButton_AnyUp)
                mover(*e, -COLUMNAS);
            else if (down & HidNpadButton_AnyDown)
                mover(*e, COLUMNAS);
            else if (down & HidNpadButton_AnyLeft)
                mover(*e, -1);
            else if (down & HidNpadButton_AnyRight)
                mover(*e, 1);
            else if (down & HidNpadButton_L)
                mover(*e, -PAGINA);
            else if (down & HidNpadButton_R)
                mover(*e, PAGINA);
            else if (down & HidNpadButton_ZL)
                mover(*e, -PAGINA * 64);
            else if (down & HidNpadButton_ZR)
                mover(*e, PAGINA * 64);
            else if (down & HidNpadButton_A)
                avisar(""), cambiar(e);
            else if (down & HidNpadButton_X)
                avisar(""), irA(e);
            else if (down & HidNpadButton_Y)
                avisar(""), elegirArchivo(e);
            else
                return false;
            return true;
        };
        v.teclas = {{"A", "Cambiar"}, {"X", "Ir a"}, {"Y", "Archivo"}, {"L R", "Página"}, {"ZL ZR", "64 páginas"}, {"B", "Volver"}};
    };
}

Registro r1({SeccionAvanzado, 80, [] { return std::string("Memoria en bruto"); },
    [] { return std::string("Los bytes de la partida"); },
    [] { abrir(bruto()); }, nullptr, "ToolChangeStick"});

} // namespace
