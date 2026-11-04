#include "ui.hpp"
#include "../core/acnh.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>

namespace ui {

SDL_Renderer* g_r;
TTF_Font* g_fuente;
TTF_Font* g_pequena;
TTF_Font* g_grande;

const SDL_Color CREMA = {0xF7, 0xF2, 0xE4, 255};
const SDL_Color CREMA_OSCURA = {0xEA, 0xE2, 0xCC, 255};
const SDL_Color VERDE = {0x3F, 0x9E, 0x5C, 255};
const SDL_Color VERDE_OSCURO = {0x2C, 0x6E, 0x40, 255};
const SDL_Color MARRON = {0x5A, 0x4A, 0x3A, 255};
const SDL_Color MARRON_SUAVE = {0x8C, 0x7C, 0x68, 255};
const SDL_Color BLANCO = {255, 255, 255, 255};
const SDL_Color GRIS = {0xCF, 0xC8, 0xB6, 255};
const SDL_Color ROJO = {0xC0, 0x45, 0x38, 255};
const SDL_Color AMARILLO = {0xF2, 0xC1, 0x4E, 255};
const SDL_Color AZUL = {0x3E, 0x7C, 0xB8, 255};

// ---------------------------------------------------------------------------------------------------------- dibujo
// Las letras ya dibujadas se guardan: dibujar texto cada vez es lento.
namespace {
std::map<std::tuple<std::string, TTF_Font*, uint32_t, int>, Letras> g_letras;
}

Letras letras(const std::string& s, TTF_Font* f, SDL_Color c, int ancho)
{
    const uint32_t color = (c.r << 24) | (c.g << 16) | (c.b << 8) | c.a;
    auto clave = std::make_tuple(s, f, color, ancho);
    auto it = g_letras.find(clave);
    if (it != g_letras.end())
        return it->second;
    if (g_letras.size() > 800)
    {
        for (auto& l : g_letras)
            SDL_DestroyTexture(l.second.t);
        g_letras.clear();
    }
    SDL_Surface* sf = ancho > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), c, ancho) : TTF_RenderUTF8_Blended(f, s.c_str(), c);
    Letras l{nullptr, 0, 0};
    if (sf)
    {
        l = {SDL_CreateTextureFromSurface(g_r, sf), sf->w, sf->h};
        SDL_FreeSurface(sf);
    }
    g_letras[clave] = l;
    return l;
}

void rect(const SDL_Rect& r, SDL_Color c)
{
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(g_r, &r);
}

void redondo(const SDL_Rect& r, int radio, SDL_Color c)
{
    radio = std::min({radio, r.w / 2, r.h / 2});
    SDL_SetRenderDrawColor(g_r, c.r, c.g, c.b, c.a);
    SDL_Rect centro = {r.x, r.y + radio, r.w, r.h - 2 * radio};
    SDL_RenderFillRect(g_r, &centro);
    for (int dy = 0; dy < radio; dy++)
    {
        const double d = radio - dy - 0.5;
        const int dx = radio - (int)std::lround(std::sqrt(std::max(0.0, (double)radio * radio - d * d)));
        SDL_RenderDrawLine(g_r, r.x + dx, r.y + dy, r.x + r.w - 1 - dx, r.y + dy);
        SDL_RenderDrawLine(g_r, r.x + dx, r.y + r.h - 1 - dy, r.x + r.w - 1 - dx, r.y + r.h - 1 - dy);
    }
}

int texto(const std::string& s, int x, int y, SDL_Color c, TTF_Font* f, Alinear a, int ancho)
{
    if (s.empty())
        return 0;
    Letras l = letras(s, f ? f : g_fuente, c, ancho);
    if (!l.t)
        return 0;
    SDL_Rect d = {a == Centro ? x - l.w / 2 : a == Derecha ? x - l.w : x, a == Centro ? y - l.h / 2 : y, l.w, l.h};
    SDL_RenderCopy(g_r, l.t, nullptr, &d);
    return l.h;
}

int anchoTexto(const std::string& s, TTF_Font* f)
{
    int w = 0;
    TTF_SizeUTF8(f ? f : g_fuente, s.c_str(), &w, nullptr);
    return w;
}

std::string encajar(std::string s, int ancho, TTF_Font* f)
{
    f = f ? f : g_fuente;
    if (anchoTexto(s, f) <= ancho)
        return s;
    while (!s.empty())
    {
        size_t i = s.size() - 1;
        while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
            i--;
        s.erase(i);
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        if (anchoTexto(s + "…", f) <= ancho)
            break;
    }
    return s + "…";
}

void dibujar(const Boton& b, bool foco)
{
    SDL_Color fondo = !b.activo ? GRIS : b.marcado ? VERDE_OSCURO : b.grande ? BLANCO : VERDE;
    SDL_Color letra = !b.activo ? MARRON_SUAVE : b.grande && !b.marcado ? MARRON : BLANCO;
    if (foco)
        redondo({b.r.x - 5, b.r.y - 5, b.r.w + 10, b.r.h + 10}, 18, AMARILLO);
    if (b.grande && b.activo && !b.marcado)
        redondo({b.r.x, b.r.y + 4, b.r.w, b.r.h}, 14, CREMA_OSCURA);
    redondo(b.r, std::min(14, b.r.h / 3), fondo);
    int x0 = b.r.x, w0 = b.r.w;
    if (b.color.a > 0)
    {
        const int lado = std::min(b.r.h - 12, 40);
        redondo({b.r.x + 8, b.r.y + (b.r.h - lado) / 2, lado, lado}, 6, b.color);
        x0 += lado + 12;
        w0 -= lado + 12;
    }
    if (b.imagen)
    {
        const int lado = std::min(b.r.h - 4, 64);
        SDL_Rect d = {b.r.x + 8, b.r.y + (b.r.h - lado) / 2, lado, lado};
        SDL_RenderCopy(g_r, b.imagen, nullptr, &d);
        x0 += lado + 12;
        w0 -= lado + 12;
    }
    if (b.grande)
    {
        redondo({b.r.x, b.r.y, 10, b.r.h}, 5, b.marcado ? AMARILLO : b.activo ? VERDE : GRIS);
        const int x = b.r.x + 28;
        if (b.detalle.empty())
            texto(encajar(b.texto, b.r.w - 40), x, b.r.y + b.r.h / 2 - 17, letra);
        else
        {
            texto(encajar(b.texto, b.r.w - 40), x, b.r.y + b.r.h / 2 - 34, letra);
            texto(encajar(b.detalle, b.r.w - 40, g_pequena), x, b.r.y + b.r.h / 2 + 4, b.marcado ? CREMA : MARRON_SUAVE, g_pequena);
        }
        return;
    }
    const int margen = std::min(24, w0 / 8);
    // Small letters if the button is low or the text does not fit with the normal ones.
    TTF_Font* f = b.r.h < 40 || anchoTexto(b.texto) > w0 - margen ? g_pequena : g_fuente;
    if (b.detalle.empty())
        texto(encajar(b.texto, w0 - margen, f), x0 + w0 / 2, b.r.y + b.r.h / 2, letra, f, Centro);
    else
    {
        texto(encajar(b.texto, w0 - margen), x0 + w0 / 2, b.r.y + b.r.h / 2 - 14, letra, nullptr, Centro);
        texto(encajar(b.detalle, w0 - margen, g_pequena), x0 + w0 / 2, b.r.y + b.r.h / 2 + 18, letra, g_pequena, Centro);
    }
}

namespace {
// La barra de abajo: un aviso a la izquierda y la ayuda de los botones a la derecha.
void barra(const std::string& aviso, bool malo, bool suave, const Teclas& teclas)
{
    const bool fuerte = malo || (!aviso.empty() && !suave);
    rect({0, ABAJO, W, H - ABAJO}, malo ? ROJO : fuerte ? VERDE_OSCURO : CREMA_OSCURA);
    int x = W - 24;
    const SDL_Color letra = fuerte ? BLANCO : MARRON;
    for (auto it = teclas.rbegin(); it != teclas.rend(); ++it)
    {
        Letras l = letras(it->second, g_pequena, letra, 0);
        x -= l.w;
        texto(it->second, x, H - 24 - l.h / 2, letra, g_pequena);
        Letras k = letras(it->first, g_pequena, MARRON, 0);
        const int ancho = std::max(30, k.w + 16);
        x -= 8 + ancho;
        redondo({x, H - 39, ancho, 30}, 15, letra);
        texto(it->first, x + ancho / 2, H - 24, fuerte ? (malo ? ROJO : VERDE_OSCURO) : CREMA, g_pequena, Centro);
        x -= 26;
    }
    if (!aviso.empty())
        texto(encajar(aviso, x - 40, g_pequena), 24, H - 24 - letras("Ag", g_pequena, letra, 0).h / 2, letra, g_pequena);
}

// ---------------------------------------------------------------------------------------------- pantallas apiladas
struct Entrada {
    Pantalla p;
    int foco;
};
std::vector<Entrada> g_pila;
std::vector<std::function<void()>> g_pendiente; // changes to the stack, done after the action
bool g_salir = false;
std::string g_aviso;
bool g_avisoMalo = false;
std::string g_avisoFijo; // the one shown when there is none (set by the home screen)
uint64_t g_fotograma = 1;
} // namespace

void abrir(Pantalla p, int foco) { g_pendiente.push_back([p, foco] { g_pila.push_back({p, foco}); }); }
void volver()
{
    g_pendiente.push_back([] {
        if (g_pila.size() > 1)
            g_pila.pop_back();
    });
}
void reemplazar(Pantalla p, int foco)
{
    g_pendiente.push_back([p, foco] {
        if (!g_pila.empty())
            g_pila.back() = {p, foco};
    });
}
void alInicio()
{
    g_pendiente.push_back([] {
        if (!g_pila.empty())
            g_pila.resize(1);
    });
}
void enfocar(int f)
{
    g_pendiente.push_back([f] {
        if (!g_pila.empty())
            g_pila.back().foco = f;
    });
}
uint64_t fotograma() { return g_fotograma; }
int foco() { return g_pila.empty() ? 0 : g_pila.back().foco; }
void terminar() { g_salir = true; }
bool terminando() { return g_salir; }

void avisar(const std::string& s, bool malo) { g_aviso = s, g_avisoMalo = malo; }
void cambiado(const std::string& que) { avisar(que + ". Falta guardar."); }

// --------------------------------------------------------------------------------------------------------- teclado
bool pedirNumero(const std::string& titulo, uint32_t& valor, uint32_t maximo, uint32_t minimo)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetStringLenMax(&kbd, 10);
    swkbdConfigSetHeaderText(&kbd, titulo.c_str());
    char inicial[16];
    snprintf(inicial, sizeof(inicial), "%u", valor);
    swkbdConfigSetInitialText(&kbd, inicial);
    char out[16] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc) || !out[0])
        return false;
    unsigned long v = strtoul(out, nullptr, 10);
    valor = (uint32_t)std::clamp<unsigned long>(v, minimo, maximo);
    return true;
}

bool pedirTexto(const std::string& titulo, const std::string& pista, std::string& valor, int largo)
{
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return false;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, titulo.c_str());
    swkbdConfigSetGuideText(&kbd, pista.c_str());
    swkbdConfigSetStringLenMax(&kbd, largo);
    swkbdConfigSetInitialText(&kbd, valor.c_str());
    char out[512] = {};
    Result rc = swkbdShow(&kbd, out, sizeof(out));
    swkbdClose(&kbd);
    if (R_FAILED(rc))
        return false;
    valor = out;
    return true;
}

std::string numero(uint32_t n)
{
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3)
        s.insert(i, ".");
    return s;
}

// ------------------------------------------------------------------------------------------- pantallas que se repiten
Pantalla lista(Lista l)
{
    auto pagina = std::make_shared<int>(0);
    return [l, pagina](Vista& v) {
        const int total = l.cuantas();
        const int porPagina = l.columnas * l.porColumna;
        const int paginas = std::max(1, (total + porPagina - 1) / porPagina);
        int& p = *pagina;
        p = std::clamp(p, 0, paginas - 1);
        v.titulo = l.titulo;
        v.explicacion = l.explicacion ? l.explicacion() : "";
        if (paginas > 1)
            v.explicacion += (v.explicacion.empty() ? "" : " ") + std::string("(página ") + std::to_string(p + 1) + " de " + std::to_string(paginas) + ")";
        // Rows between y=140 and the bottom buttons.
        const int arriba = 140, abajoFilas = ABAJO - 72;
        const int hueco = 8;
        const int cabe = (abajoFilas - arriba) / l.porColumna - hueco;
        const int alto = std::min(l.alto > 0 ? l.alto : 70, cabe);
        const int ancho = (W - 120 - (l.columnas - 1) * 20) / l.columnas;
        for (int k = 0; k < porPagina && p * porPagina + k < total; k++)
        {
            Fila f = l.fila(p * porPagina + k);
            Boton bt{{60 + (k / l.porColumna) * (ancho + 20), arriba + (k % l.porColumna) * (alto + hueco), ancho, alto}, f.texto, f.accion, f.activo, f.marcado, f.detalle};
            bt.color = f.color;
            bt.imagen = f.imagen;
            v.b.push_back(bt);
        }
        std::vector<Fila> abajo;
        if (paginas > 1)
        {
            abajo.push_back({"Anteriores", [pagina] { (*pagina)--; }, p > 0});
            abajo.push_back({"Siguientes", [pagina] { (*pagina)++; }, p + 1 < paginas});
        }
        for (const auto& a : l.abajo)
            abajo.push_back(a());
        abajo.push_back({"Volver", [] { volver(); }});
        const int n = (int)abajo.size();
        const int anchoAbajo = std::min(300, (W - 120 - (n - 1) * 16) / n);
        for (int i = 0; i < n; i++)
        {
            // «Volver» on the right; the rest from the left.
            const int x = i == n - 1 ? W - 60 - anchoAbajo : 60 + i * (anchoAbajo + 16);
            v.b.push_back({{x, ABAJO - 66, anchoAbajo, 56}, abajo[i].texto, abajo[i].accion, abajo[i].activo, abajo[i].marcado});
        }
        if (paginas > 1)
            v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + d, 0, paginas - 1); };
        if (l.extra)
            l.extra(v);
    };
}

void elegir(const std::string& titulo, const std::string& explicacion, const std::vector<std::string>& opciones, int actual,
    std::function<void(int)> elegido, int columnas)
{
    Lista l;
    l.titulo = titulo;
    l.explicacion = [explicacion] { return explicacion; };
    l.cuantas = [opciones] { return (int)opciones.size(); };
    l.fila = [opciones, actual, elegido](int i) {
        return Fila{opciones[i], [elegido, i] {
            volver();
            elegido(i);
        }, true, i == actual};
    };
    l.columnas = columnas;
    const int porPagina = l.columnas * l.porColumna;
    abrir(lista(l), actual >= 0 && actual < (int)opciones.size() ? actual % porPagina : 0);
}

void confirmar(const std::string& titulo, const std::string& pregunta, const std::string& si, std::function<void()> accion)
{
    abrir([=](Vista& v) {
        v.titulo = titulo;
        v.explicacion = pregunta;
        v.b.push_back({{140, 330, 480, 100}, si, [accion] {
            volver();
            accion();
        }});
        v.b.push_back({{660, 330, 480, 100}, "No", [] { volver(); }});
    }, 1);
}

Pantalla marcas(Marcas m)
{
    struct Estado {
        std::string busqueda;
        std::vector<int> vistas; // the indexes shown
    };
    auto e = std::make_shared<Estado>();
    auto filtrar = [m, e] {
        e->vistas.clear();
        std::vector<std::string> palabras;
        std::string w;
        for (char c : acnh::normalizar(e->busqueda) + " ")
        {
            if (c == ' ')
            {
                if (!w.empty())
                    palabras.push_back(w);
                w.clear();
            }
            else
                w += c;
        }
        for (int i = 0; i < m.total; i++)
        {
            if (palabras.empty())
            {
                e->vistas.push_back(i);
                continue;
            }
            const std::string clave = m.clave ? m.clave(i) : acnh::normalizar(m.nombre(i));
            bool todas = true;
            for (const auto& p : palabras)
                todas = todas && clave.find(p) != std::string::npos;
            if (todas)
                e->vistas.push_back(i);
        }
    };
    filtrar();
    auto buscar = [e, filtrar] {
        std::string q = e->busqueda;
        if (!pedirTexto("Buscar", "Escribe parte del nombre (vacío: todas)", q))
            return;
        e->busqueda = q;
        filtrar();
    };
    Lista l;
    l.titulo = m.titulo;
    l.explicacion = [m, e] {
        int marcadas = 0;
        for (int i : e->vistas)
            marcadas += m.marcada(i);
        std::string s = e->busqueda.empty() ? "" : "«" + e->busqueda + "»: ";
        return s + numero(marcadas) + " de " + numero((uint32_t)e->vistas.size()) + " " + m.queEs + " marcadas.";
    };
    l.cuantas = [e] { return (int)e->vistas.size(); };
    l.fila = [m, e](int k) {
        const int i = e->vistas[k];
        const bool si = m.marcada(i);
        return Fila{(si ? "● " : "") + m.nombre(i), [m, i, si] { m.marcar(i, !si); }, true, si};
    };
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.abajo.push_back([m, e] {
        return Fila{"Marcar todas", [m, e] {
            int n = 0;
            for (int i : e->vistas)
                if (!m.marcada(i))
                    m.marcar(i, true), n++;
            cambiado(std::to_string(n) + " marcadas");
        }};
    });
    l.abajo.push_back([m, e] {
        return Fila{"Desmarcar todas", [m, e] {
            const auto vistas = e->vistas;
            confirmar(m.titulo, "¿Desmarcar las " + numero((uint32_t)vistas.size()) + " " + m.queEs + " que se ven?", "Desmarcar", [m, vistas] {
                for (int i : vistas)
                    if (m.marcada(i))
                        m.marcar(i, false);
                cambiado("Desmarcadas");
            });
        }};
    });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Marcar"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

// ----------------------------------------------------------------------------------------------------------- bucle
std::vector<Tarjeta>& tarjetas()
{
    static std::vector<Tarjeta> t;
    return t;
}

void bucle(Pantalla inicio)
{
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    g_pila = {{inicio, 0}};

    u64 repiteTecla = 0;
    u32 repiteDesde = 0;
    bool touchAntes = false;

    while (!g_salir && appletMainLoop())
    {
        g_fotograma++;
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        const u64 held = padGetButtons(&pad);
        const u64 flechas = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
        if (down & flechas)
            repiteTecla = down & flechas, repiteDesde = SDL_GetTicks() + 350;
        else if ((held & repiteTecla) && SDL_GetTicks() >= repiteDesde)
            down |= repiteTecla & held, repiteDesde = SDL_GetTicks() + 80;
        HidTouchScreenState ts = {};
        hidGetTouchScreenStates(&ts, 1);
        const bool toque = ts.count > 0 && !touchAntes;
        touchAntes = ts.count > 0;
        const int tx = toque ? (int)ts.touches[0].x : -1, ty = toque ? (int)ts.touches[0].y : -1;

        Vista v;
        Entrada actual = g_pila.back();
        actual.p(v);
        int& foco = g_pila.back().foco;
        if (foco >= (int)v.b.size() || foco < 0)
            foco = 0;

        auto accion = [&](const std::function<void()>& f) {
            g_aviso.clear();
            f();
        };
        bool usada = v.entrada && v.entrada(down, held, tx, ty);
        if (!usada)
        {
            auto mover = [&](int dx, int dy) {
                if (v.b.empty())
                    return;
                const SDL_Rect& a = v.b[foco].r;
                const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
                int mejor = -1;
                long mejorPeso = 0;
                for (int i = 0; i < (int)v.b.size(); i++)
                {
                    if (i == foco)
                        continue;
                    const SDL_Rect& r = v.b[i].r;
                    const int bx = r.x + r.w / 2, by = r.y + r.h / 2;
                    const long lejos = dx ? (long)(bx - ax) * dx : (long)(by - ay) * dy;
                    if (lejos <= 4)
                        continue;
                    long cruce = dx ? std::abs(by - ay) : std::abs(bx - ax);
                    const bool solapan = dx ? (r.y < a.y + a.h && a.y < r.y + r.h) : (r.x < a.x + a.w && a.x < r.x + r.w);
                    if (solapan)
                        cruce /= 4;
                    const long peso = lejos + 3 * cruce;
                    if (mejor < 0 || peso < mejorPeso)
                        mejor = i, mejorPeso = peso;
                }
                if (mejor >= 0)
                    foco = mejor;
            };
            if (down & HidNpadButton_AnyUp)
                mover(0, -1);
            else if (down & HidNpadButton_AnyDown)
                mover(0, 1);
            else if (down & HidNpadButton_AnyLeft)
                mover(-1, 0);
            else if (down & HidNpadButton_AnyRight)
                mover(1, 0);

            int pulsado = -1;
            if (down & HidNpadButton_A)
                pulsado = foco;
            for (size_t i = 0; i < v.b.size(); i++)
                if (tx >= v.b[i].r.x && tx < v.b[i].r.x + v.b[i].r.w && ty >= v.b[i].r.y && ty < v.b[i].r.y + v.b[i].r.h)
                    pulsado = foco = (int)i;
            if (pulsado >= 0 && pulsado < (int)v.b.size() && v.b[pulsado].activo && v.b[pulsado].accion)
                accion(v.b[pulsado].accion);
            else if (down & HidNpadButton_B)
                accion(v.atras ? v.atras : [] { volver(); });
            else if ((down & HidNpadButton_X) && v.x)
                accion(v.x);
            else if ((down & HidNpadButton_Y) && v.y)
                accion(v.y);
            else if ((down & HidNpadButton_Minus) && v.menos)
                accion(v.menos);
            else if (v.paginar && (down & (HidNpadButton_L | HidNpadButton_R)))
                v.paginar(down & HidNpadButton_L ? -1 : 1);
            else if (v.paginar && (down & (HidNpadButton_ZL | HidNpadButton_ZR)))
                v.paginar(down & HidNpadButton_ZL ? -10 : 10);
            else if ((down & HidNpadButton_Plus) && g_pila.size() == 1 && v.atras)
                accion(v.atras);
        }

        if (!g_pendiente.empty() || g_salir)
        {
            // The stack changed: the next frame shows the new screen.
            auto p = std::move(g_pendiente);
            g_pendiente.clear();
            for (auto& f : p)
                f();
            continue;
        }

        rect({0, 0, W, H}, CREMA);
        rect({0, 0, W, 96}, VERDE);
        texto("NHSE", 40, 22, BLANCO, g_grande);
        texto(encajar(v.titulo, 900), 1240, 29, BLANCO, nullptr, Derecha);
        texto(v.explicacion, 60, 104, MARRON, g_pequena, Izquierda, 1160);
        if (v.dibujo)
            v.dibujo();
        for (size_t i = 0; i < v.b.size(); i++)
            dibujar(v.b[i], (int)i == foco);
        Teclas teclas = v.teclas;
        if (teclas.empty())
        {
            teclas.push_back({"A", "Elegir"});
            if (v.paginar)
                teclas.push_back({"L R", "Página"});
            teclas.push_back({"B", "Volver"});
        }
        if (!g_aviso.empty())
            barra(g_aviso, g_avisoMalo, false, teclas);
        else
            barra(g_pila.size() == 1 ? g_avisoFijo : "", false, true, teclas);
        SDL_RenderPresent(g_r);
    }
    for (auto& l : g_letras)
        SDL_DestroyTexture(l.second.t);
    g_letras.clear();
}

// The home screen sets the message shown when there is none.
void avisoFijo(const std::string& s) { g_avisoFijo = s; }

} // namespace ui
