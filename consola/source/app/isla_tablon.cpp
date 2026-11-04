// Sección «Isla»: el tablón de anuncios de la plaza, como el de NHSE para Mac. Las notas, de la más nueva a la más
// vieja; en cada una se cambia lo que dice (línea a línea, con el teclado de la consola) y la firma. Las escritas a mano
// solo se ven en el juego. NHSE tampoco añade ni quita notas.
#include <memory>
#include "../core/isla.hpp"
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
            if (!pedirTexto("Firma", "Quién firma la nota", t, LARGO_FIRMA) || t == e->n.firma)
                return;
            escribirFirma(partida::principal(), hueco, t);
            partida::cambiaPrincipal();
            cambiado("Firma cambiada");
        }, hay};
    });
    li.extra = [](Vista& v) { v.teclas = {{"A", "Cambiar"}, {"L R", "Página"}, {"B", "Volver"}}; };
    return lista(li);
}

Pantalla tablon()
{
    auto e = std::make_shared<std::vector<Nota>>();
    Lista l;
    l.titulo = "Tablón de anuncios";
    l.cuantas = [e] {
        *e = notas(partida::principal());
        return (int)e->size();
    };
    l.explicacion = [e] {
        return e->empty() ? std::string("No hay notas en el tablón.")
                          : std::string("Las notas de la plaza, de la más nueva a la más vieja. Toca una para cambiarla.");
    };
    l.fila = [e](int i) {
        const Nota& n = (*e)[i];
        const auto ls = lineas(n.texto);
        Fila f{titulo(n), [h = n.hueco] { abrir(nota(h)); }};
        f.detalle = n.aMano ? std::string("Escrita a mano (solo se ve en el juego)") : ls.empty() ? std::string("(sin texto)") : ls[0];
        return f;
    };
    l.columnas = 1;
    l.porColumna = 6;
    return lista(l);
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
