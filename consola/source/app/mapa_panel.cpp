// Lo que comparten el mapa de la isla y las casas (ver mapa_panel.hpp).
#include "mapa_panel.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include "../core/mapa_objetos.hpp"
#include "../core/objetos.hpp"
#include "mapa_pintor.hpp"

using namespace ui;

namespace mapa {

using namespace acnh;

namespace {

// El color que dice su nombre («pared flor delicada azul»), para papeles, suelos y alfombras; 0 si no dice ninguno.
uint32_t colorDelNombre(const std::string& n)
{
    static const std::pair<const char*, uint32_t> COLORES[] = {{"azul", 0x6F98D8}, {"roj", 0xC9504A}, {"verde", 0x7DB060},
        {"amarill", 0xE8CC58}, {"rosa", 0xEBA0B8}, {"blanc", 0xF2EEE6}, {"negr", 0x4A454C}, {"morad", 0x9A78C4},
        {"naranja", 0xE8954C}, {"gris", 0xA4A4A8}, {"marron", 0x8E6A4A}, {"beis", 0xDCC8A4}, {"beige", 0xDCC8A4},
        {"dorad", 0xD8B448}, {"platead", 0xC4C8CC}, {"madera", 0xB48A5C}, {"celeste", 0x9CCCE8}, {"turquesa", 0x58BCB8}};
    const std::string k = normalizar(n);
    size_t mejor = std::string::npos;
    uint32_t c = 0;
    for (const auto& p : COLORES)
    {
        const size_t i = k.find(p.first);
        if (i != std::string::npos && (mejor == std::string::npos || i > mejor))
            mejor = i, c = p.second;
    }
    return c;
}

} // namespace

SDL_Color colorDe(const Objeto& o)
{
    const uint16_t id = idVisible(o);
    const int p = plantaDe(id);
    uint32_t c = p >= 0 ? PLANTAS[p].color : colorCategoria(categoria(id));
    if (p < 0 && categoria(id) == 2)
    {
        const uint32_t n = colorDelNombre(nombre(o));
        if (n)
            c = n;
    }
    double f = 1;
    if (p < 0)
        f = 0.86 + ((id * 2654435761u) >> 27) / 100.0; // cada objeto con un tono algo distinto
    auto k = [f](uint32_t v) { return (uint8_t)std::min(255.0, v * f); };
    return {k((c >> 16) & 255), k((c >> 8) & 255), k(c & 255), 255};
}

std::string noCabe(Resultado r, const Objeto& estorbo, bool habitacion)
{
    if (r == NoCabe)
        return "Aquí no cabe: " + (esNada(estorbo) ? std::string("algo") : nombre(estorbo)) + " está en medio.";
    return habitacion ? "Aquí no cabe: se saldría de la habitación." : "Aquí no cabe: se saldría del borde.";
}

Pantalla panelSitio(Sitio s)
{
    return [s](Vista& v) {
        const Objeto o = s.objeto();
        const bool vacio = esNada(o);
        v.titulo = "Qué hay aquí";
        v.dibujo = [s, o, vacio] {
            redondo({60, 112, 1160, 140}, 16, BLANCO);
            const SDL_Color c = vacio ? CREMA_OSCURA : colorDe(o);
            SDL_Texture* foto = vacio ? nullptr : dibujoDe(o);
            redondo({84, 136, 92, 92}, 14, foto ? CREMA : c);
            if (foto)
            {
                SDL_Rect d = {92, 144, 76, 76};
                SDL_RenderCopy(g_r, foto, nullptr, &d);
            }
            int w, h;
            tamano(o, w, h);
            if (!vacio && !foto)
                texto(std::to_string(w / 2) + (w % 2 ? ",5" : "") + "×" + std::to_string(h / 2) + (h % 2 ? ",5" : ""), 130, 182, BLANCO, g_pequena, Centro);
            texto(encajar(vacio ? "Sitio vacío" : nombre(o), 1000, g_grande), 200, 128, MARRON, g_grande);
            std::string donde = s.donde;
            if (!vacio && enterrado(o))
                donde += " · Enterrado";
            if (vacio)
                donde += " · No hay nada: elige algo para poner aquí.";
            texto(encajar(donde, 1000, g_pequena), 200, 182, MARRON_SUAVE, g_pequena);
            const std::string aviso = s.aviso ? s.aviso() : "";
            if (!aviso.empty())
                texto(encajar(aviso, 1000, g_pequena), 200, 214, ROJO, g_pequena);
        };
        std::vector<Boton> bs;
        auto buscar = [s] {
            buscarObjeto([s](const Objeto& n) {
                if (s.poner(n))
                    cambiado("Puesto: " + nombre(n));
            }, "ponerlo aquí", s.soloCategoria);
        };
        auto planta = [s] {
            elegirPlanta([s](uint16_t id) {
                Objeto n = nada();
                n.id = id;
                if (s.poner(n))
                    cambiado("Puesto: " + nombre(n));
            }, "ponerla aquí");
        };
        if (!vacio)
        {
            bs.push_back({{}, "Quitar", [s] {
                const std::string n = nombre(s.objeto());
                s.quitar();
                volver();
                cambiado("Quitado: " + n);
            }});
            if (s.girar)
                bs.push_back({{}, "Girar", [s] { s.girar(); }});
            if (s.mover)
                bs.push_back({{}, "Mover a otro sitio", [s] {
                    volver();
                    s.mover();
                }});
            bs.push_back({{}, "Cambiar por un objeto…", buscar});
            if (s.plantas)
                bs.push_back({{}, "Cambiar por una planta…", planta});
        }
        else
        {
            bs.push_back({{}, "Poner un objeto…", buscar});
            if (s.plantas)
                bs.push_back({{}, "Poner una planta, valla o roca…", planta});
        }
        for (const auto& m : s.mas)
            bs.push_back({{}, m.first, m.second});
        bs.push_back({{}, "Opciones avanzadas…", [s] {
            abrir(editorBruto("Opciones avanzadas", s.objeto, s.poner, [s] {
                std::string d = s.donde;
                if (s.posicion)
                    d += " · casilla " + std::to_string(s.posicion().first) + ", " + std::to_string(s.posicion().second);
                return d;
            }));
        }});
        for (size_t i = 0; i < bs.size() && i < 8; i++)
        {
            bs[i].r = {60 + (int)(i % 2) * 590, 270 + (int)(i / 2) * 80, 570, 66};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 70, 400, 56}, "Volver", [] { volver(); }});
    };
}

void buscarObjeto(std::function<void(const Objeto&)> elegido, const std::string& para, int cat)
{
    std::string q;
    const char* pista = cat == 2 ? "Escribe parte del nombre (por ejemplo, «papel» o «suelo»)" : "Escribe parte del nombre (por ejemplo, «silla» o «manzana»)";
    if (!pedirTexto("Buscar objeto", pista, q) || q.empty())
        return;
    auto hallados = std::make_shared<std::vector<int>>();
    for (int i : buscarObjetos(q))
        if (cat < 0 || categoria(OBJETOS[i].id) == cat)
            hallados->push_back(i);
    if (hallados->empty())
    {
        avisar("No hay ningún objeto que se llame «" + q + "»" + (cat == 2 ? " entre los papeles, suelos y alfombras." : "."), true);
        return;
    }
    Lista l;
    l.titulo = "«" + q + "»: " + (hallados->size() == 1 ? std::string("1 objeto") : numero((uint32_t)hallados->size()) + " objetos");
    l.explicacion = [para] { return para.empty() ? std::string("Elige el objeto.") : "Elige el objeto para " + para + "."; };
    l.cuantas = [hallados] { return (int)hallados->size(); };
    l.fila = [hallados, elegido](int k) {
        const int i = (*hallados)[k];
        Fila f{OBJETOS[i].nombre, [elegido, i] {
            Objeto n;
            n.id = OBJETOS[i].id;
            n.cuenta = OBJETOS[i].cuenta;
            volver();
            elegido(n);
        }};
        Objeto n = nada();
        n.id = OBJETOS[i].id;
        f.imagen = dibujoDe(n);
        if (!f.imagen)
            f.color = colorDe(n);
        return f;
    };
    l.abajo.push_back([elegido, para, cat] {
        return Fila{"Buscar otra cosa", [elegido, para, cat] {
            volver();
            buscarObjeto(elegido, para, cat);
        }};
    });
    abrir(lista(l));
}

namespace {

constexpr int NUM_LISTA = sizeof(PLANTAS_LISTA) / sizeof(PLANTAS_LISTA[0]);

struct Plantas {
    std::string busqueda;
    std::vector<int> vistas;
    std::vector<uint16_t> elegidas;
    bool alAzar = false;
    void filtrar()
    {
        vistas.clear();
        std::vector<std::string> palabras;
        std::string w;
        for (char c : normalizar(busqueda) + " ")
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
        for (int i = 0; i < NUM_LISTA; i++)
        {
            bool todas = true;
            for (const auto& p : palabras)
                todas = todas && std::strstr(PLANTAS_LISTA[i].clave, p.c_str());
            if (todas)
                vistas.push_back(i);
        }
    }
};

// El dibujo de una planta en su fila, o su color si no hay.
void ponerDibujo(Fila& f, uint16_t id)
{
    Objeto o = nada();
    o.id = id;
    f.imagen = dibujoDe(o);
    if (!f.imagen)
        f.color = colorDe(o);
}

Lista listaPlantas(std::shared_ptr<Plantas> e, const std::string& titulo, const std::string& para)
{
    Lista l;
    l.titulo = titulo;
    l.explicacion = [e, para] {
        std::string t = e->busqueda.empty() ? "Árboles, arbustos, flores, vallas, rocas..." : "Las que se llaman «" + e->busqueda + "».";
        return t + " " + para + " X para buscar.";
    };
    l.cuantas = [e] { return (int)e->vistas.size(); };
    l.porColumna = 7;
    auto buscar = [e] {
        std::string q = e->busqueda;
        if (!pedirTexto("Buscar planta", "Escribe parte del nombre (vacío: todas)", q))
            return;
        e->busqueda = q;
        e->filtrar();
    };
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Elegir"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return l;
}

} // namespace

void elegirPlanta(std::function<void(uint16_t)> elegida, const std::string& para)
{
    auto e = std::make_shared<Plantas>();
    e->filtrar();
    Lista l = listaPlantas(e, "Plantas, vallas y rocas", "Elige una para " + para + ".");
    l.fila = [e, elegida](int k) {
        const auto& p = PLANTAS_LISTA[e->vistas[k]];
        Fila f{p.nombre, [elegida, id = p.id] {
            volver();
            elegida(id);
        }};
        ponerDibujo(f, p.id);
        return f;
    };
    abrir(lista(l));
}

void elegirPlantas(std::function<void(const std::vector<uint16_t>&, bool)> elegidas, const std::string& para)
{
    auto e = std::make_shared<Plantas>();
    e->filtrar();
    Lista l = listaPlantas(e, "Cambiar por…", "Marca una o varias para " + para + " (con varias, se mezclan).");
    l.fila = [e](int k) {
        const auto& p = PLANTAS_LISTA[e->vistas[k]];
        const bool marcada = std::find(e->elegidas.begin(), e->elegidas.end(), p.id) != e->elegidas.end();
        Fila f{std::string(marcada ? "● " : "") + p.nombre, [e, id = p.id] {
            auto it = std::find(e->elegidas.begin(), e->elegidas.end(), id);
            if (it == e->elegidas.end())
                e->elegidas.push_back(id);
            else
                e->elegidas.erase(it);
        }, true, marcada};
        ponerDibujo(f, p.id);
        return f;
    };
    l.abajo.push_back([e] { return Fila{e->alAzar ? "Al azar" : "Alternando", [e] { e->alAzar = !e->alAzar; }}; });
    l.abajo.push_back([e, elegidas] {
        return Fila{e->elegidas.empty() ? "Cambiar" : "Cambiar (" + std::to_string(e->elegidas.size()) + ")", [e, elegidas] {
            volver();
            elegidas(e->elegidas, e->alAzar);
        }, !e->elegidas.empty()};
    });
    const auto extra = l.extra;
    l.extra = [extra](Vista& v) {
        extra(v);
        v.teclas[0].second = "Marcar";
    };
    abrir(lista(l));
}

Pantalla editorBruto(const std::string& titulo, std::function<Objeto()> leer, std::function<bool(const Objeto&)> poner,
    std::function<std::string()> donde)
{
    return [=](Vista& v) {
        const Objeto o = leer();
        v.titulo = titulo;
        v.explicacion = "Cada valor del objeto por separado, como en el editor de objetos de NHSE. Para quien sabe lo que toca.";
        v.dibujo = [o, donde] {
            redondo({60, 140, 1160, 96}, 16, BLANCO);
            SDL_Texture* foto = dibujoDe(o);
            redondo({76, 150, 76, 76}, 12, foto ? CREMA : esNada(o) ? CREMA_OSCURA : colorDe(o));
            if (foto)
            {
                SDL_Rect d = {82, 156, 64, 64};
                SDL_RenderCopy(g_r, foto, nullptr, &d);
            }
            texto(encajar(esNada(o) ? std::string("Nada") : nombre(o), 1020), 172, 152, MARRON);
            texto(encajar(donde ? donde() : "", 1020, g_pequena), 172, 194, MARRON_SUAVE, g_pequena);
        };
        auto cambiar = [leer, poner](std::function<void(Objeto&)> f, const std::string& que) {
            Objeto n = leer();
            f(n);
            if (poner(n))
                cambiado(que);
        };
        auto pedir = [cambiar](const std::string& t, uint32_t ahora, uint32_t maximo, std::function<void(Objeto&, uint32_t)> f) {
            uint32_t n = ahora;
            if (pedirNumero(t, n, maximo))
                cambiar([f, n](Objeto& o) { f(o, n); }, "Cambiado");
        };
        std::vector<Boton> bs;
        auto fila = [&](const std::string& t, const std::string& d, std::function<void()> f) {
            Boton b{{}, t, std::move(f)};
            b.detalle = d;
            bs.push_back(b);
        };
        fila("Objeto", esNada(o) ? "Nada · buscar otro" : "nº " + std::to_string(o.id) + " · buscar otro", [cambiar] {
            buscarObjeto([cambiar](const Objeto& n) { cambiar([n](Objeto& x) { x.id = n.id; }, "Puesto: " + nombre(n)); }, "este sitio");
        });
        fila("Número del objeto", std::to_string(o.id), [pedir, o] { pedir("Número del objeto (65534: nada)", o.id, 65535, [](Objeto& x, uint32_t n) { x.id = (uint16_t)n; }); });
        fila("Cantidad", std::to_string(o.cuenta), [pedir, o] { pedir("Cantidad (como la guarda el juego)", o.cuenta, 65535, [](Objeto& x, uint32_t n) { x.cuenta = (uint16_t)n; }); });
        fila("Usos", std::to_string(o.usos), [pedir, o] { pedir("Usos", o.usos, 65535, [](Objeto& x, uint32_t n) { x.usos = (uint16_t)n; }); });
        const char* const GIROS[] = {"Mira hacia abajo", "Mira a la derecha", "Mira hacia arriba", "Mira a la izquierda"};
        fila("Giro", GIROS[giro(o)], [cambiar] { cambiar([](Objeto& x) { x.sistema = (uint8_t)((x.sistema & ~3) | ((x.sistema + 1) & 3)); }, "Girado"); });
        fila("Enterrado", enterrado(o) ? "Sí" : "No", [cambiar] { cambiar([](Objeto& x) { x.sistema ^= 0x04; }, "Cambiado"); });
        fila("Tirado en el suelo", tirado(o) ? "Sí" : "No", [cambiar] { cambiar([](Objeto& x) { x.sistema ^= 0x20; }, "Cambiado"); });
        fila("Bits del sistema", std::to_string(o.sistema), [pedir, o] { pedir("Bits del sistema (0 a 255)", o.sistema, 255, [](Objeto& x, uint32_t n) { x.sistema = (uint8_t)n; }); });
        fila("Extra", std::to_string(o.extra), [pedir, o] { pedir("Extra: envoltorio y otros (0 a 255)", o.extra, 255, [](Objeto& x, uint32_t n) { x.extra = (uint8_t)n; }); });
        fila("Dejarlo vacío", "Sin nada", [poner] {
            if (poner(nada()))
                cambiado("Vaciado");
        });
        for (size_t i = 0; i < bs.size(); i++)
        {
            bs[i].r = {60 + (int)(i % 3) * 393, 256 + (int)(i / 3) * 84, 373, 74};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{440, ABAJO - 66, 400, 56}, "Volver", [] { volver(); }});
    };
}

} // namespace mapa
