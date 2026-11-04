#include "jugador_comun.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <memory>

using namespace acnh;
using namespace ui;

namespace jugador {

const SDL_Color BORDE_MAC = {0xD1, 0xD1, 0xD6, 255};
const SDL_Color GRIS_CLARO = {0xE5, 0xE5, 0xEA, 255};

Fecha hoy()
{
    const time_t t = time(nullptr);
    const tm* l = localtime(&t);
    Fecha f;
    f.anio = (uint16_t)(l->tm_year + 1900);
    f.mes = (uint8_t)(l->tm_mon + 1);
    f.dia = (uint8_t)l->tm_mday;
    return f;
}

std::string diaYMes(int dia, int mes)
{
    if (mes < 1 || mes > 12 || dia < 1)
        return "Sin elegir";
    return std::to_string(dia) + " de " + MESES[mes - 1];
}

void pedirFecha(const std::string& titulo, Fecha actual, bool sinFecha, std::function<void(Fecha)> elegida)
{
    auto f = std::make_shared<Fecha>(actual.valida() ? actual : hoy());
    abrir([titulo, f, sinFecha, elegida](Vista& v) {
        v.titulo = titulo;
        v.explicacion = "Toca el día, el mes o el año para cambiarlo.";
        Fecha& x = *f;
        x.dia = (uint8_t)std::min<int>(x.dia, diasDelMes(x.mes, x.anio));
        v.dibujo = [] {
            panel({60, 112, 1160, 270}, "Fecha", "Timer", "");
        };
        auto cifra = [&](int col, const std::string& que, const std::string& valor, std::function<void()> f) {
            Boton b{{84 + col * 376, 196, 360, 120}, que, std::move(f)};
            b.grande = true;
            b.valor = valor;
            b.detalle = "Toca para cambiarlo";
            v.b.push_back(b);
        };
        cifra(0, "Día", std::to_string(x.dia), [f] {
            uint32_t n = f->dia;
            if (pedirNumero("Día", n, diasDelMes(f->mes, f->anio), 1))
                f->dia = (uint8_t)n;
        });
        cifra(1, "Mes", MESES[x.mes - 1], [f] {
            std::vector<std::string> meses(MESES, MESES + 12);
            elegir("Mes", "", meses, f->mes - 1, [f](int i) { f->mes = (uint8_t)(i + 1); }, 3);
        });
        cifra(2, "Año", std::to_string(x.anio), [f] {
            uint32_t n = f->anio;
            if (pedirNumero("Año", n, 2099, 2000))
                f->anio = (uint16_t)n;
        });
        v.b.push_back({{60, 400, 280, 60}, "Hoy", [f] { *f = hoy(); }});
        if (sinFecha)
            v.b.push_back({{356, 400, 280, 60}, "Sin fecha", [elegida] {
                volver();
                elegida(Fecha{});
            }});
        Boton aceptar{{760, ABAJO - 66, 260, 56}, "Aceptar", [f, elegida] {
            const Fecha r = *f;
            volver();
            elegida(r);
        }};
        aceptar.principal = true;
        v.b.push_back(aceptar);
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------------- piezas de dibujo
namespace {

// Un dibujo hecho punto a punto: «pinta(x, y)» da el color (con su transparencia) de cada punto, de 0 a 1.
SDL_Texture* dibujar(int lado, std::function<SDL_Color(float, float)> pinta)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, lado, lado, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s)
        return nullptr;
    for (int y = 0; y < lado; y++)
    {
        uint32_t* fila = (uint32_t*)((uint8_t*)s->pixels + y * s->pitch);
        for (int x = 0; x < lado; x++)
        {
            const SDL_Color c = pinta(x + 0.5f, y + 0.5f);
            fila[x] = (uint32_t)c.a << 24 | (uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b;
        }
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_r, s);
    SDL_FreeSurface(s);
    if (t)
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

// Cuánto de un punto queda dentro de una forma, por su distancia al borde (negativa dentro): bordes suaves.
float cubre(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

float rectanguloRedondo(float x, float y, float x0, float y0, float x1, float y1, float r)
{
    const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    const float qx = std::fabs(x - cx) - hx, qy = std::fabs(y - cy) - hy;
    return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}

float segmento(float x, float y, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float t = std::clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
    return std::hypot(x - ax - t * dx, y - ay - t * dy);
}

// Pone «c» (con transparencia «a») encima de «fondo».
SDL_Color encima(SDL_Color fondo, SDL_Color c, float a)
{
    if (a <= 0)
        return fondo;
    const float fa = fondo.a / 255.0f, ca = a * c.a / 255.0f;
    const float out = ca + fa * (1 - ca);
    if (out <= 0)
        return {0, 0, 0, 0};
    auto mezcla = [&](uint8_t f, uint8_t k) { return (uint8_t)std::lround((k * ca + f * fa * (1 - ca)) / out); };
    return {mezcla(fondo.r, c.r), mezcla(fondo.g, c.g), mezcla(fondo.b, c.b), (uint8_t)std::lround(out * 255)};
}

} // namespace

SDL_Texture* casilla(bool marcada)
{
    static SDL_Texture* t[2] = {nullptr, nullptr};
    if (!t[marcada])
    {
        constexpr int L = 64;
        t[marcada] = dibujar(L, [marcada](float x, float y) {
            const float d = rectanguloRedondo(x, y, 8, 8, L - 8, L - 8, 9);
            SDL_Color c = {0, 0, 0, 0};
            if (marcada)
            {
                c = encima(c, AZUL, cubre(d));
                const float m = std::min(segmento(x, y, 19, 33, 28, 42), segmento(x, y, 28, 42, 45, 22)) - 3.2f;
                c = encima(c, BLANCO, cubre(m) * cubre(d));
            }
            else
            {
                c = encima(c, {0x8E, 0x8E, 0x93, 255}, cubre(d));
                c = encima(c, BLANCO, cubre(d + 3));
            }
            return c;
        });
    }
    return t[marcada];
}

Boton interruptor(const SDL_Rect& r, const std::string& etiqueta, bool marcada, std::function<void()> accion)
{
    Boton b{r, etiqueta, std::move(accion)};
    b.imagen = casilla(marcada);
    b.pequena = true;
    return b;
}

SDL_Texture* sellos(int conseguidos, int total)
{
    static std::map<std::pair<int, int>, SDL_Texture*> hechos;
    total = std::clamp(total, 1, 12);
    conseguidos = std::clamp(conseguidos, 0, total);
    SDL_Texture*& t = hechos[{conseguidos, total}];
    if (!t)
    {
        constexpr int L = 64;
        constexpr float PI = 3.14159265f;
        t = dibujar(L, [conseguidos, total](float x, float y) {
            const float dx = x - L / 2.0f, dy = y - L / 2.0f, r = std::hypot(dx, dy);
            // From the top, clockwise.
            float a = std::atan2(dx, -dy);
            if (a < 0)
                a += 2 * PI;
            const float trozo = 2 * PI / total;
            const int i = std::min(total - 1, (int)(a / trozo));
            // A small gap between pieces (in points, at that radius).
            const float borde = std::min(a - i * trozo, (i + 1) * trozo - a) * r;
            const float dentro = std::max(r - 29.0f, 17.0f - r); // ring between 17 and 29
            float k = cubre(dentro);
            if (total > 1)
                k *= std::clamp(borde - 1.5f, 0.0f, 1.0f);
            const SDL_Color c = i < conseguidos ? VERDE : GRIS_CLARO;
            SDL_Color out = encima({0, 0, 0, 0}, c, k);
            // In the middle, a green dot when it is complete.
            if (conseguidos == total)
                out = encima(out, VERDE, cubre(r - 9));
            return out;
        });
    }
    return t;
}

void recuadro(const SDL_Rect& r)
{
    redondo(r, 14, BORDE_MAC);
    redondo({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 12, BLANCO);
}

void elipse(int cx, int cy, int rx, int ry, SDL_Color c)
{
    std::vector<SDL_FPoint> p;
    for (int i = 0; i < 72; i++)
    {
        const float a = i * 2 * 3.14159265f / 72;
        p.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
    poligono(p, c);
}

void imagen(SDL_Texture* t, int x, int y, int lado)
{
    if (!t)
        return;
    SDL_Rect d = {x, y, lado, lado};
    SDL_RenderCopy(g_r, t, nullptr, &d);
}

// ------------------------------------------------------------------------------- una lista con casillas, como el Mac
Pantalla listaCasillas(ListaCasillas l)
{
    struct Estado {
        std::vector<std::string> claves; // the names to search, lower case without accents
        std::string busqueda;
        int filtro = 0;
        int pagina = 0;
        std::vector<int> vistas;
        bool sucio = true;
        uint64_t visto = 0; // the last frame it was drawn
    };
    auto e = std::make_shared<Estado>();
    e->claves.reserve(l.total);
    for (int i = 0; i < l.total; i++)
        e->claves.push_back(normalizar(l.nombre(i)));
    auto lp = std::make_shared<ListaCasillas>(std::move(l));
    return [lp, e](Vista& v) {
        const ListaCasillas& l = *lp;
        // Coming back from another screen (or from a question), what is marked may have changed.
        if (fotograma() != e->visto + 1)
            e->sucio = true;
        e->visto = fotograma();
        if (e->sucio)
        {
            // Which rows are shown, with the search and the filter (only when they change).
            e->sucio = false;
            e->vistas.clear();
            std::vector<std::string> palabras;
            std::string w;
            for (char c : normalizar(e->busqueda) + " ")
                if (c == ' ')
                {
                    if (!w.empty())
                        palabras.push_back(w);
                    w.clear();
                }
                else
                    w += c;
            const Filtro* f = l.filtros.empty() ? nullptr : &l.filtros[e->filtro];
            for (int i = 0; i < l.total; i++)
            {
                bool si = !f || !f->pasa || f->pasa(i);
                for (const auto& p : palabras)
                    si = si && e->claves[i].find(p) != std::string::npos;
                if (si)
                    e->vistas.push_back(i);
            }
        }
        v.titulo = l.titulo;
        // The header: what there is (bold), the buttons, the search box and the filters.
        const bool cabecera = l.resumen || !l.arriba.empty();
        const int yArriba = 104, yBuscar = cabecera ? 166 : 112, yLista = yBuscar + 58;
        const int fondoCabecera = yBuscar + 50;
        // The rows.
        const int ALTO = 50;
        const int porColumna = std::max(1, (ABAJO - 72 - yLista - 10) / ALTO);
        const int porPagina = porColumna * l.columnas;
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + porPagina - 1) / porPagina);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.explicacion = l.explicacion;
        if (paginas > 1)
            v.explicacion += (v.explicacion.empty() ? "P" : " · P") + std::string("ágina ") + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ".";
        const int anchoCol = (1160 - (l.columnas - 1) * 16) / l.columnas;
        const std::string resumen = l.resumen ? l.resumen() : "";
        const int desde = e->pagina * porPagina, hasta = std::min(total, desde + porPagina);
        // Where each row's name ends (the boxes go after it).
        int anchoCasillas = 0;
        for (const auto& c : l.casillas)
            anchoCasillas += c.ancho + 10;
        // The header buttons go right to left: where they start, the summary ends.
        int xArriba = 1204;
        for (const auto& a : l.arriba)
            xArriba -= std::max(160, anchoTexto(a().texto, g_pequena) + 44) + 12;
        v.dibujo = [lp, e, cabecera, fondoCabecera, yLista, porColumna, anchoCol, resumen, desde, hasta, anchoCasillas, xArriba] {
            const ListaCasillas& l = *lp;
            recuadro({60, yArriba, 1160, fondoCabecera - yArriba});
            if (cabecera)
                texto(encajar(resumen, xArriba - 100, g_titulo), 84, yArriba + 8, MARRON, g_titulo);
            const int filas = std::min(porColumna, hasta - desde);
            for (int c = 0; c < l.columnas && c * porColumna < hasta - desde; c++)
            {
                const int x = 60 + c * (anchoCol + 16);
                const int enEsta = std::min(porColumna, hasta - desde - c * porColumna);
                recuadro({x, yLista, anchoCol, (c == 0 ? filas : enEsta) * ALTO + 10});
            }
            if (desde == hasta)
            {
                recuadro({60, yLista, 1160, 120});
                texto(e->busqueda.empty() ? "No hay ninguna." : "No hay ninguna que coincida con «" + e->busqueda + "».", 640, yLista + 60, MARRON_SUAVE, nullptr, Centro);
            }
            for (int k = desde; k < hasta; k++)
            {
                const int i = e->vistas[k], pos = k - desde;
                const int x = 60 + (pos / porColumna) * (anchoCol + 16), y = yLista + 5 + (pos % porColumna) * ALTO;
                if (pos % porColumna)
                {
                    SDL_SetRenderDrawColor(g_r, GRIS_CLARO.r, GRIS_CLARO.g, GRIS_CLARO.b, 255);
                    linea(x + 16, y, x + anchoCol - 16, y);
                }
                int xt = x + 18;
                if (l.imagen)
                {
                    imagen(l.imagen(i), x + 14, y + 4, 42);
                    xt = x + 68;
                }
                const int ancho = x + anchoCol - anchoCasillas - 12 - xt;
                const std::string nota = l.nota ? l.nota(i) : "";
                if (nota.empty())
                {
                    TTF_Font* f = l.columnas > 1 ? g_pequena : g_fuente;
                    texto(encajar(l.nombre(i), ancho, f), xt, y + (ALTO - ui::alto(f)) / 2, MARRON, f);
                }
                else
                {
                    texto(encajar(l.nombre(i), ancho, g_pequena), xt, y + 1, MARRON, g_pequena);
                    texto(encajar(nota, ancho, g_pequena), xt, y + 23, MARRON_SUAVE, g_pequena);
                }
            }
        };
        // The boxes of each row.
        for (int k = desde; k < hasta; k++)
        {
            const int i = e->vistas[k], pos = k - desde;
            const int x = 60 + (pos / porColumna) * (anchoCol + 16), y = yLista + 5 + (pos % porColumna) * ALTO;
            // Left to right (so the cursor starts on the first one), ending at the right edge of the row.
            int xc = x + anchoCol - 10 - anchoCasillas + 10;
            for (int c = 0; c < (int)l.casillas.size(); c++)
            {
                const Casilla& ca = l.casillas[c];
                if (!ca.visible || ca.visible(i))
                {
                    const bool si = ca.marcada(i);
                    v.b.push_back(interruptor({xc, y + 3, ca.ancho, ALTO - 6}, ca.etiqueta, si, [lp, c, i, si] { lp->casillas[c].marcar(i, !si); }));
                }
                xc += ca.ancho + 10;
            }
        }
        // The buttons on the top right: the first one blue.
        if (cabecera)
        {
            int x = 1204;
            for (int k = (int)l.arriba.size() - 1; k >= 0; k--)
            {
                Fila f = l.arriba[k]();
                const int w = std::max(160, anchoTexto(f.texto, g_pequena) + 44);
                x -= w;
                auto accion = f.accion;
                Boton b{{x, yArriba + 10, w, 46}, f.texto, [e, accion] {
                    accion();
                    e->sucio = true;
                }, f.activo};
                b.pequena = true;
                b.principal = k == 0 && f.activo;
                v.b.push_back(b);
                x -= 12;
            }
        }
        // The search box (a button: it opens the search with its keyboard) and the filters.
        auto buscar = [lp, e] {
            auto vistas = std::make_shared<std::vector<int>>();
            Buscador bu;
            bu.titulo = lp->titulo + ": buscar";
            bu.explicacion = lp->casillas.empty() ? "" : "Toca una para cambiar «" + lp->casillas[0].etiqueta + "».";
            bu.texto = e->busqueda;
            bu.filtrar = [lp, e, vistas](const std::string& t) {
                vistas->clear();
                const std::string q = normalizar(t);
                for (int i = 0; i < lp->total; i++)
                    if (q.empty() || e->claves[i].find(q) != std::string::npos)
                        vistas->push_back(i);
                return (int)vistas->size();
            };
            bu.fila = [lp, vistas](int k) {
                const int i = (*vistas)[k];
                const Casilla& c = lp->casillas[0];
                const bool si = c.marcada(i);
                Fila f{lp->nombre(i), [lp, i, si] { lp->casillas[0].marcar(i, !si); }, true, si};
                f.imagen = lp->imagen ? lp->imagen(i) : casilla(si);
                if (lp->imagen)
                    f.detalle = (si ? "Sí: " : "No: ") + c.etiqueta;
                return f;
            };
            bu.alSalir = [e](const std::string& t) {
                e->busqueda = t;
                e->pagina = 0;
                e->sucio = true;
            };
            abrir(buscador(bu), 11);
        };
        {
            const int w = l.filtros.empty() ? 1128 : 440;
            Boton b{{76, yBuscar, w, 46}, e->busqueda.empty() ? l.pista : "Buscando «" + e->busqueda + "»", buscar};
            b.pequena = true;
            v.b.push_back(b);
            int x = 76 + w + 16;
            for (int k = 0; k < (int)l.filtros.size(); k++)
            {
                const int fw = std::max(120, anchoTexto(l.filtros[k].nombre, g_pequena) + 40);
                Boton f{{x, yBuscar, fw, 46}, l.filtros[k].nombre, [e, k] {
                    e->filtro = k;
                    e->pagina = 0;
                    e->sucio = true;
                }, true, e->filtro == k};
                f.pequena = true;
                v.b.push_back(f);
                x += fw + 10;
            }
        }
        // At the bottom: pages, the extra buttons and «Volver».
        std::vector<Fila> abajo;
        if (paginas > 1)
        {
            abajo.push_back({"Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            abajo.push_back({"Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
        }
        for (const auto& a : l.abajo)
        {
            Fila f = a();
            auto accion = f.accion;
            f.accion = [e, accion] {
                accion();
                e->sucio = true;
            };
            abajo.push_back(f);
        }
        int x = 60;
        for (const auto& f : abajo)
        {
            const int w = std::max(150, anchoTexto(f.texto, g_pequena) + 44);
            Boton b{{x, ABAJO - 66, w, 56}, f.texto, f.accion, f.activo};
            b.pequena = true;
            v.b.push_back(b);
            x += w + 12;
        }
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
        if (paginas > 1)
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        v.x = buscar;
        v.teclas = {{"A", "Marcar"}, {"X", "Buscar"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}

} // namespace jugador
