#include "objetos_ui.hpp"
#include "../core/objetos.hpp"
#include "dibujos.hpp"
#include <algorithm>
#include <memory>

using namespace acnh;
using namespace ui;

namespace objetos {

namespace {
constexpr int NUM_OBJETOS = sizeof(OBJETOS) / sizeof(OBJETOS[0]);

std::string nombreFosil(uint16_t id)
{
    for (int i = 0; i < NUM_OBJETOS; i++)
        if (OBJETOS[i].id == id)
            return OBJETOS[i].nombre;
    return "?";
}
} // namespace

std::string nombre(const Objeto& o)
{
    if (o.vacio())
        return "";
    const int k = objetoDe(o);
    if (k < 0)
        return "Objeto nº " + std::to_string(o.id);
    const auto& d = OBJETOS[k];
    std::string s = d.nombre;
    if (d.tipo == 2 && o.cuenta)
        s += " (" + nombreFosil(o.cuenta) + ")";
    if (d.remake >= 0)
    {
        const Remake& r = REMAKES[d.remake];
        for (int i = 0; i < r.numCuerpos; i++)
            if (VARIANTES[r.cuerpos + i].numero == cuerpo(o))
                s += std::string(" (") + VARIANTES[r.cuerpos + i].nombre + ")";
    }
    if (d.pila > 1)
        s += " ×" + std::to_string(o.cuenta + 1);
    if (envuelto(o))
        s += " (envuelto)";
    return s;
}

void buscar(std::function<void(const Objeto&)> elegido, const std::string& para)
{
    std::string q;
    if (!pedirTexto("Buscar objeto", "Escribe parte del nombre (por ejemplo, «pepita» o «receta escalera»)", q) || q.empty())
        return;
    auto hallados = std::make_shared<std::vector<int>>(buscarObjetos(q));
    if (hallados->empty())
    {
        avisar("No hay ningún objeto que se llame «" + q + "».", true);
        return;
    }
    Lista l;
    l.titulo = "«" + q + "»: " + (hallados->size() == 1 ? std::string("1 objeto") : numero((uint32_t)hallados->size()) + " objetos");
    l.explicacion = [para] { return para.empty() ? std::string("Elige el objeto.") : "Elige el objeto para " + para + "."; };
    l.cuantas = [hallados] { return (int)hallados->size(); };
    l.fila = [hallados, elegido](int k) {
        const int i = (*hallados)[k];
        std::string t = OBJETOS[i].nombre;
        if (OBJETOS[i].pila > 1)
            t += " (hasta " + std::to_string(OBJETOS[i].pila) + ")";
        Fila f{t, [elegido, i] {
            Objeto n;
            n.id = OBJETOS[i].id;
            n.cuenta = OBJETOS[i].pila > 1 ? OBJETOS[i].pila - 1 : OBJETOS[i].cuenta;
            volver();
            elegido(n);
        }};
        f.imagen = dibujos::de(OBJETOS[i].id);
        return f;
    };
    l.abajo.push_back([elegido, para] {
        return Fila{"Buscar otra cosa", [elegido, para] {
            volver();
            buscar(elegido, para);
        }};
    });
    abrir(lista(l));
}

Pantalla editor(Huecos h, int i)
{
    return [h, i](Vista& v) {
        const Objeto o = h.leer(i);
        const int k = objetoDe(o);
        v.titulo = h.nombre + ": hueco " + std::to_string(i + 1);
        v.explicacion = o.vacio() ? "Está vacío." : "Ahora: " + nombre(o);
        auto poner = [h, i](const Objeto& n) {
            h.escribir(i, n);
            cambiado("Puesto: " + nombre(n));
        };
        auto cambiar = [h, i](std::function<void(Objeto&)> f, const std::string& que = "Cambiado") {
            Objeto n = h.leer(i);
            f(n);
            h.escribir(i, n);
            cambiado(que);
        };
        auto buscarOtro = [poner, h, i] { buscar(poner, "el hueco " + std::to_string(i + 1)); };
        // A grid of 2 columns: what there is, and what can be changed of it.
        std::vector<Boton> opciones;
        opciones.push_back({{}, o.vacio() ? "Poner un objeto…" : "Cambiar por otro objeto…", buscarOtro});
        if (k >= 0)
        {
            const auto& d = OBJETOS[k];
            if (d.pila > 1)
            {
                opciones.push_back({{}, "Cantidad: " + std::to_string(o.cuenta + 1), [h, i, cambiar, k] {
                    uint32_t v2 = h.leer(i).cuenta + 1;
                    if (pedirNumero("¿Cuántos? (de 1 a " + std::to_string(OBJETOS[k].pila) + ")", v2, OBJETOS[k].pila, 1))
                        cambiar([v2](Objeto& n) { n.cuenta = (uint16_t)(v2 - 1); });
                }});
                opciones.push_back({{}, "Máximo (" + std::to_string(d.pila) + ")", [cambiar, k] {
                    cambiar([k](Objeto& n) { n.cuenta = OBJETOS[k].pila - 1; });
                }, o.cuenta + 1 < d.pila});
            }
            if (d.remake >= 0)
            {
                const Remake r = REMAKES[d.remake];
                auto nombres = [](int desde, int n) {
                    std::vector<std::string> v2;
                    for (int j = 0; j < n; j++)
                        v2.push_back(VARIANTES[desde + j].nombre);
                    return v2;
                };
                if (r.numCuerpos > 1)
                {
                    int actual = -1;
                    for (int j = 0; j < r.numCuerpos; j++)
                        if (VARIANTES[r.cuerpos + j].numero == cuerpo(o))
                            actual = j;
                    opciones.push_back({{}, "Color: " + (actual >= 0 ? std::string(VARIANTES[r.cuerpos + actual].nombre) : "?"), [=] {
                        elegir("Color", "Elige el color.", nombres(r.cuerpos, r.numCuerpos), actual, [=](int j) {
                            cambiar([&](Objeto& n) { ponerCuerpo(n, VARIANTES[r.cuerpos + j].numero); });
                        });
                    }});
                }
                if (r.numTelas > 1)
                {
                    int actual = -1;
                    for (int j = 0; j < r.numTelas; j++)
                        if (VARIANTES[r.telas + j].numero == tela(o))
                            actual = j;
                    opciones.push_back({{}, "Tela: " + (actual >= 0 ? std::string(VARIANTES[r.telas + actual].nombre) : "?"), [=] {
                        elegir("Tela", "Elige la tela.", nombres(r.telas, r.numTelas), actual, [=](int j) {
                            cambiar([&](Objeto& n) { ponerTela(n, VARIANTES[r.telas + j].numero); });
                        });
                    }});
                }
            }
            if (d.tipo == 2)
            {
                std::vector<std::string> fosiles;
                int actual = -1;
                for (int j = 0; j < (int)(sizeof(FOSILES) / sizeof(FOSILES[0])); j++)
                {
                    fosiles.push_back(nombreFosil(FOSILES[j]));
                    if (FOSILES[j] == o.cuenta)
                        actual = j;
                }
                opciones.push_back({{}, "Fósil: " + (actual >= 0 ? fosiles[actual] : std::string("sin elegir")), [=] {
                    elegir("¿Qué fósil es?", "Al identificarlo en el museo será este.", fosiles, actual, [=](int j) {
                        cambiar([&](Objeto& n) { n.cuenta = FOSILES[j]; });
                    });
                }});
            }
            std::vector<std::string> papeles(PAPELES, PAPELES + 16);
            opciones.push_back({{}, envuelto(o) ? std::string("Envuelto: ") + PAPELES[papel(o)] : "Envolver para regalo…", [=] {
                elegir("Papel de regalo", "Elige el color del papel.", papeles, envuelto(o) ? papel(o) : -1, [=](int j) {
                    cambiar([&](Objeto& n) { envolver(n, j); }, "Envuelto");
                }, 4);
            }});
            if (envuelto(o))
                opciones.push_back({{}, "Quitar el envoltorio", [=] { cambiar([](Objeto& n) { desenvolver(n); }, "Sin envoltorio"); }});
        }
        opciones.push_back({{}, "Vaciar el hueco", [h, i] {
            h.escribir(i, Objeto{});
            cambiado("Vaciado");
        }, !o.vacio()});
        const int cols = opciones.size() > 8 ? 3 : 2;
        const int ancho = (W - 120 - (cols - 1) * 20) / cols;
        for (int j = 0; j < (int)opciones.size(); j++)
        {
            opciones[j].r = {60 + (j % cols) * (ancho + 20), 250 + (j / cols) * 80, ancho, 68};
            v.b.push_back(opciones[j]);
        }
        v.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
        v.x = buscarOtro;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"B", "Volver"}};
        // What there is now, big, at the top.
        SDL_Texture* t = dibujos::de(o);
        const std::string ahora = o.vacio() ? "Vacío" : nombre(o);
        v.explicacion.clear();
        v.dibujo = [t, ahora] {
            redondo({60, 120, W - 120, 110}, 14, BLANCO);
            int x = 84;
            if (t)
            {
                SDL_Rect d = {x, 127, 96, 96};
                SDL_RenderCopy(g_r, t, nullptr, &d);
                x += 116;
            }
            texto(encajar(ahora, W - 60 - x - 24, g_grande), x, 175 - 24, MARRON, g_grande);
        };
    };
}

namespace {
// Huecos en casillas, como los bolsillos de NHSE para Mac: 10 × 4 por página y, a la derecha, el hueco elegido con lo
// que se puede hacer. A (o tocar otra vez) en una casilla abre todo lo del hueco. «indices» dice qué huecos se ven (todos,
// o solo los usados en el almacén); con «anadir», el botón «Añadir objeto».
Pantalla rejilla(Huecos h, std::function<std::vector<int>()> indices, std::function<std::string(int)> explicacion, std::function<void()> anadir)
{
    struct Estado {
        int pagina = 0;
        int elegida = -1;   // the square selected last frame (to open it when chosen again)
        std::vector<int> vistos;
    };
    auto e = std::make_shared<Estado>();
    return [h, indices, explicacion, anadir, e](Vista& v) {
        constexpr int COLS = 10, FILAS = 4, POR_PAGINA = COLS * FILAS, LADO = 70, PASO = 76;
        e->vistos = indices();
        const int total = (int)e->vistos.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = h.nombre;
        v.explicacion = explicacion(total) + (paginas > 1 ? " (página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ")" : "");
        const int primero = e->pagina * POR_PAGINA;
        const int enPagina = std::min(POR_PAGINA, total - primero);
        // The selected square is the one with the cursor (or the last one that had it).
        const int f = foco();
        if (f >= 0 && f < enPagina)
            e->elegida = f;
        if (e->elegida >= enPagina)
            e->elegida = enPagina - 1;
        for (int k = 0; k < enPagina; k++)
        {
            const int i = e->vistos[primero + k];
            const bool abierto = !h.abierto || h.abierto(i);
            const Objeto o = h.leer(i);
            Boton b{{60 + (k % COLS) * PASO, 132 + (k / COLS) * PASO, LADO, LADO}, abierto ? "" : "×", [h, i, e, k] {
                if (e->elegida == k)
                    abrir(editor(h, i));
                else
                    e->elegida = k;
            }, abierto};
            b.casilla = true;
            b.marcado = k == e->elegida;
            if (abierto && !o.vacio())
            {
                b.imagen = dibujos::de(o);
                b.color = dibujos::color(o.id);
                const int obj = objetoDe(o);
                if (obj >= 0 && OBJETOS[obj].pila > 1)
                    b.detalle = std::to_string(o.cuenta + 1);
            }
            v.b.push_back(b);
        }
        // Below the squares: pages, add.
        int y = 132 + FILAS * PASO + 10, x = 60;
        if (paginas > 1)
        {
            v.b.push_back({{x, y, 180, 56}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            v.b.push_back({{x + 196, y, 180, 56}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
            x += 392;
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        }
        if (anadir)
        {
            Boton b{{x, y, 250, 56}, "Añadir objeto…", anadir};
            v.b.push_back(b);
            v.x = anadir;
        }
        // On the right, the selected one, as the panel of the Mac.
        const int px = 60 + COLS * PASO + 20, pw = W - 60 - px;
        if (e->elegida >= 0 && e->elegida < enPagina)
        {
            const int i = e->vistos[primero + e->elegida];
            const Objeto o = h.leer(i);
            const bool abierto = !h.abierto || h.abierto(i);
            SDL_Texture* t = dibujos::de(o);
            const SDL_Color fondo = o.vacio() ? BLANCO : dibujos::color(o.id);
            const std::string nom = !abierto ? "Hueco sin ampliar" : o.vacio() ? "Hueco " + std::to_string(i + 1) + " vacío" : nombre(o);
            v.dibujo = [=] {
                redondo({px, 132, pw, 456}, 14, {0xD1, 0xD1, 0xD6, 255});
                redondo({px + 1, 133, pw - 2, 454}, 13, BLANCO);
                redondo({px + 20, 152, 96, 96}, 10, fondo);
                if (t)
                {
                    SDL_Rect d = {px + 24, 156, 88, 88};
                    SDL_RenderCopy(g_r, t, nullptr, &d);
                }
                texto(nom, px + 20, 262, MARRON, g_negrita, Izquierda, pw - 40);
            };
            auto buscarAqui = [h, i] {
                buscar([h, i](const Objeto& n) {
                    h.escribir(i, n);
                    cambiado("Puesto: " + nombre(n));
                }, "el hueco " + std::to_string(i + 1));
            };
            int by = 588 - 3 * 62;
            v.b.push_back({{px + 20, by, pw - 40, 54}, o.vacio() ? "Poner un objeto…" : "Cambiar por otro…", buscarAqui, abierto});
            v.b.push_back({{px + 20, by + 62, pw - 40, 54}, "Más opciones…", [h, i] { abrir(editor(h, i)); }, abierto && !o.vacio()});
            v.b.push_back({{px + 20, by + 124, pw - 40, 54}, "Vaciar", [h, i] {
                h.escribir(i, Objeto{});
                cambiado("Vaciado");
            }, abierto && !o.vacio()});
            if (!v.x)
                v.x = buscarAqui;
        }
        v.teclas = {{"A", "Elegir"}, {"X", anadir ? "Añadir" : "Buscar"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}
} // namespace

Pantalla todos(Huecos h, int)
{
    return rejilla(h, [h] {
        std::vector<int> r(h.cuantos());
        for (int i = 0; i < (int)r.size(); i++)
            r[i] = i;
        return r;
    }, [h](int total) {
        int usados = 0;
        for (int i = 0; i < total; i++)
            usados += !h.leer(i).vacio();
        return std::to_string(usados) + " objetos. Elige una casilla y luego qué hacer a la derecha; A otra vez abre todo.";
    }, nullptr);
}

Pantalla usados(Huecos h)
{
    auto anadir = [h] {
        int libre = -1;
        for (int i = 0; i < h.cuantos() && libre < 0; i++)
            if (h.leer(i).vacio() && (!h.abierto || h.abierto(i)))
                libre = i;
        if (libre < 0)
        {
            avisar("No queda ningún hueco libre.", true);
            return;
        }
        buscar([h, libre](const Objeto& n) {
            h.escribir(libre, n);
            cambiado("Añadido: " + nombre(n));
        });
    };
    return rejilla(h, [h] {
        std::vector<int> r;
        for (int i = 0; i < h.cuantos(); i++)
            if (!h.leer(i).vacio())
                r.push_back(i);
        return r;
    }, [h](int total) { return numero((uint32_t)total) + " de " + numero((uint32_t)h.cuantos()) + " huecos usados."; }, anadir);
}

} // namespace objetos
