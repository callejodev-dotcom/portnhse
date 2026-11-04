// Sección «Isla»: el tablón de anuncios de la plaza, como el de NHSE para Mac. Las notas, de la más nueva a la más
// vieja; en cada una se cambia lo que dice (línea a línea, con el teclado de la consola) y la firma. Las escritas a mano
// solo se ven en el juego. NHSE tampoco añade ni quita notas.
#include <algorithm>
#include <memory>
#include "../core/isla.hpp"
#include "dibujos.hpp"
#include "isla_comun.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

std::vector<std::string> lineas(const std::string& t)
{
    std::vector<std::string> r;
    std::string l;
    for (char c : t)
    {
        if (c == '\n')
            r.push_back(l), l.clear();
        else
            l += c;
    }
    r.push_back(l);
    if (r.size() == 1 && r[0].empty())
        r.clear();
    return r;
}

std::string juntar(const std::vector<std::string>& v)
{
    std::string t;
    for (size_t i = 0; i < v.size(); i++)
        t += (i ? "\n" : "") + v[i];
    return t;
}

bool leerNota(int hueco, Nota& n)
{
    for (auto& x : notas(partida::principal()))
        if (x.hueco == hueco)
        {
            n = x;
            return true;
        }
    return false;
}

std::string titulo(const Nota& n) { return n.autor + (n.fecha.vacia() ? "" : " · " + textoFechaIsla(n.fecha)); }

void guardarLineas(int hueco, const std::vector<std::string>& v, const std::string& que)
{
    escribirNota(partida::principal(), hueco, juntar(v));
    partida::cambiaPrincipal();
    cambiado(que);
}

Pantalla nota(int hueco)
{
    struct Estado {
        Nota n;
        std::vector<std::string> l;
    };
    auto e = std::make_shared<Estado>();
    Lista li;
    li.titulo = "Nota";
    li.cuantas = [e, hueco] {
        leerNota(hueco, e->n);
        e->l = e->n.aMano ? std::vector<std::string>() : lineas(e->n.texto);
        return (int)e->l.size();
    };
    li.explicacion = [e] {
        if (e->n.aMano)
            return "Escrita a mano por " + e->n.autor + ": la letra solo se puede ver en el juego.";
        const int quedan = LARGO_NOTA - letras16(e->n.texto);
        return titulo(e->n) + ". Toca una línea para cambiarla (si la dejas vacía, se quita). Quedan " + std::to_string(quedan) + " letras.";
    };
    li.fila = [e, hueco](int i) {
        const std::string linea = e->l[i];
        return Fila{linea.empty() ? std::string("(línea vacía)") : linea, [e, hueco, i] {
            std::vector<std::string> v = e->l;
            std::string t = v[i];
            const int cabe = LARGO_NOTA - (letras16(e->n.texto) - letras16(t));
            if (!pedirTexto("Línea " + std::to_string(i + 1), "Vacía: se quita la línea", t, std::max(1, cabe)))
                return;
            if (t == v[i])
                return;
            if (t.empty())
                v.erase(v.begin() + i);
            else
                v[i] = t;
            guardarLineas(hueco, v, t.empty() ? "Línea quitada" : "Cambiado");
        }};
    };
    li.columnas = 1;
    li.porColumna = 7;
    li.abajo.push_back([e, hueco] {
        const int cabe = LARGO_NOTA - letras16(e->n.texto) - (e->n.texto.empty() ? 0 : 1);
        return Fila{"Añadir línea", [e, hueco, cabe] {
            std::string t;
            if (!pedirTexto("Línea nueva", "Lo que se añade al final de la nota", t, cabe) || t.empty())
                return;
            std::vector<std::string> v = e->l;
            v.push_back(t);
            guardarLineas(hueco, v, "Línea añadida");
        }, !e->n.aMano && cabe > 0};
    });
    li.abajo.push_back([e, hueco] {
        // Como NHSE para Mac: la firma solo se cambia en las notas que la tienen.
        const bool hay = !e->n.aMano && !e->n.firma.empty();
        return Fila{hay ? "Firma: " + e->n.firma : std::string("Sin firma"), [e, hueco] {
            std::string t = e->n.firma;
            if (!pedirTexto("Firma", "Quién firma la nota", t, LARGO_FIRMA_NOTA) || t == e->n.firma)
                return;
            escribirFirma(partida::principal(), hueco, t);
            partida::cambiaPrincipal();
            cambiado("Firma cambiada");
        }, hay};
    });
    li.extra = [](Vista& v) { v.teclas = {{"A", "Cambiar"}, {"L R", "Página"}, {"B", "Volver"}}; };
    return lista(li);
}

// El dibujo de cada nota, como BulletinWindow.
const char* iconoNota(const Nota& n)
{
    if (n.aMano)
        return "LostQuestMemo";
    return n.dibujo == 1 ? "BdayCupcake" : n.dibujo == 2 ? "Fishingrod" : n.dibujo == 3 ? "Net" : "Megaphone";
}

// Como BulletinWindow: cada nota en su tarjeta (quién y cuándo, lo que dice y la firma), dos por página.
Pantalla tablon()
{
    struct Estado {
        std::vector<Nota> notas;
        int pagina = 0;
    };
    auto e = std::make_shared<Estado>();
    return [e](Vista& v) {
        e->notas = notas(partida::principal());
        constexpr int POR_PAGINA = 2, ANCHO = 570, PASO = 590, ARRIBA = 136, ALTO = 460;
        const int total = (int)e->notas.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = "Tablón de anuncios";
        v.explicacion = total == 0 ? std::string("No hay notas en el tablón.")
                                   : std::string("Las notas de la plaza, de la más nueva a la más vieja. Puedes cambiar lo que dicen.");
        if (paginas > 1)
            v.explicacion += " Página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ".";
        std::vector<Nota> vistas;
        for (int k = 0; k < POR_PAGINA && e->pagina * POR_PAGINA + k < total; k++)
            vistas.push_back(e->notas[e->pagina * POR_PAGINA + k]);
        v.dibujo = [vistas] {
            for (size_t k = 0; k < vistas.size(); k++)
            {
                const Nota& n = vistas[k];
                const int x = 60 + (int)k * PASO;
                const int y = isla::recuadro({x, ARRIBA, ANCHO, ALTO}, n.autor, dibujos::menu(iconoNota(n)),
                    n.fecha.vacia() ? std::string("Sin fecha") : textoFechaIsla(n.fecha));
                // El papel: lo que dice la nota, hasta donde quepa.
                const SDL_Rect papel = {x + 16, y, ANCHO - 32, ARRIBA + ALTO - 82 - y};
                redondo(papel, 8, {0xD1, 0xD1, 0xD6, 255});
                redondo({papel.x + 2, papel.y + 2, papel.w - 4, papel.h - 4}, 6, {0xFF, 0xFD, 0xF4, 255});
                const bool vacia = n.texto.empty();
                const std::string t = n.aMano ? "Una nota escrita a mano por " + n.autor + ". La letra solo se puede ver en el juego."
                                    : vacia ? std::string("(sin texto)") : n.texto;
                Letras l = letras(t, g_pequena, n.aMano || vacia ? MARRON_SUAVE : MARRON, papel.w - 28);
                if (l.t)
                {
                    const int cabe = papel.h - 24;
                    SDL_Rect de = {0, 0, l.w, std::min(l.h, cabe)};
                    SDL_Rect a = {papel.x + 14, papel.y + 12, l.w, de.h};
                    SDL_RenderCopy(g_r, l.t, &de, &a);
                    if (l.h > cabe)
                        texto("…", papel.x + papel.w - 12, papel.y + papel.h - 30, MARRON_SUAVE, g_pequena, Derecha);
                }
                if (!n.aMano)
                    texto(encajar(n.firma.empty() ? std::string("Sin firma") : "Firma: " + n.firma, ANCHO - 250, g_pequena), x + 24,
                        ARRIBA + ALTO - 48 - ui::alto(g_pequena) / 2, MARRON_SUAVE, g_pequena);
            }
        };
        for (size_t k = 0; k < vistas.size(); k++)
        {
            const Nota& n = vistas[k];
            const int x = 60 + (int)k * PASO;
            Boton b{{x + ANCHO - 16 - 200, ARRIBA + ALTO - 70, 200, 54}, n.aMano ? "Solo en el juego" : "Cambiar…",
                [h = n.hueco] { abrir(nota(h)); }, !n.aMano};
            v.b.push_back(b);
        }
        if (paginas > 1)
        {
            v.b.push_back({{60, ABAJO - 66, 220, 56}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            v.b.push_back({{294, ABAJO - 66, 220, 56}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        }
        v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
        v.teclas = {{"A", "Cambiar"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}

std::string resumen()
{
    const auto v = notas(partida::principal());
    if (v.empty())
        return "Sin notas";
    return std::to_string(v.size()) + " notas · la última del " + textoFechaIsla(v[0].fecha);
}

Registro r({SeccionIsla, 80, [] { return std::string("Tablón de anuncios"); }, resumen, [] { abrir(tablon()); }});

} // namespace
