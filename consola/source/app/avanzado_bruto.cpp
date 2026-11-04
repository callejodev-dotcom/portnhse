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
constexpr int X_POS = 60, X_HEX = 200, ANCHO_HEX = 42, X_TEXTO = 905, ANCHO_TEXTO = 19;
constexpr int Y_CABECERA = 140, Y_REJILLA = 168, ALTO_FILA = 24;
constexpr int Y_INFO = Y_REJILLA + FILAS * ALTO_FILA + 8;

int xHex(int c) { return X_HEX + c * ANCHO_HEX + (c >= 8 ? 10 : 0); }

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
    return {60 + i * (ancho + 14), ABAJO - 62, ancho, 52};
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
            ". Solo si sabes lo que haces: un valor erróneo puede estropear la partida.";
        v.atras = [] { volver(); };
        v.dibujo = [e, inicio] {
            auto& d = datos(*e);
            for (int c = 0; c < COLUMNAS; c++)
                texto(dosCifras((uint8_t)c), xHex(c) + 14, Y_CABECERA, MARRON_SUAVE, g_pequena, Centro);
            for (int f = 0; f < FILAS; f++)
            {
                const uint32_t fila = inicio + f * COLUMNAS;
                if (fila >= d.size())
                    break;
                const int y = Y_REJILLA + f * ALTO_FILA;
                if (f % 2)
                    rect({X_POS - 8, y - 1, W - 2 * X_POS + 16, ALTO_FILA}, CREMA_OSCURA);
                texto(hex(fila, 6), X_POS, y, MARRON_SUAVE, g_pequena);
                for (int c = 0; c < COLUMNAS && fila + c < d.size(); c++)
                {
                    const uint32_t p = fila + c;
                    const bool cursor = p == e->pos;
                    const bool cambiado = e->cambiados.count(((uint64_t)e->archivo << 32) | p) > 0;
                    if (cursor)
                    {
                        redondo({xHex(c) - 4, y - 1, 36, ALTO_FILA}, 6, AMARILLO);
                        rect({X_TEXTO + c * ANCHO_TEXTO - 2, y - 1, ANCHO_TEXTO, ALTO_FILA}, AMARILLO);
                    }
                    texto(dosCifras(d[p]), xHex(c), y, cambiado ? ROJO : MARRON, g_pequena);
                    const char ch = d[p] >= 32 && d[p] < 127 ? (char)d[p] : '.';
                    texto(std::string(1, ch), X_TEXTO + c * ANCHO_TEXTO, y, cambiado ? ROJO : MARRON_SUAVE, g_pequena);
                }
            }
            // The value at the cursor, read as numbers (little endian, as the game keeps them).
            auto le = [&d](uint32_t p, int n) {
                uint32_t v = 0;
                for (int i = n - 1; i >= 0; i--)
                    v = (v << 8) | (p + i < d.size() ? d[p + i] : 0);
                return v;
            };
            const uint32_t p = e->pos;
            texto("En " + hex(p, 6) + ":  byte " + std::to_string(d[p]) + "  ·  16 bits " + std::to_string(le(p, 2)) + "  ·  32 bits " + std::to_string(le(p, 4)),
                X_POS, Y_INFO, MARRON, g_pequena);
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
    [] { abrir(bruto()); }});

} // namespace
