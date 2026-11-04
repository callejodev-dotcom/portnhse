// Lo que comparten el mapa de la isla y las casas (ver mapa_panel.hpp).
#include "mapa_panel.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include "../core/mapa_objetos.hpp"
#include "../core/objetos.hpp"
#include "dibujos.hpp"
#include "mapa_pintor.hpp"
#include "mapa_vista.hpp"

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
        v.explicacion = vacio ? "No hay nada: elige qué poner." : "Lo que se puede hacer con lo que hay en este sitio.";
        v.dibujo = [s, o, vacio] {
            // Como el panel del Mac: el dibujo grande, el nombre, dónde está y cuánto ocupa.
            tarjeta({60, 112, 1160, 196});
            foto({84, 136, 148, 148}, vacio ? dibujos::menu("Leaf") : dibujoDe(o), vacio ? SDL_Color{} : colorDe(o));
            int w, h;
            tamano(o, w, h);
            texto(encajar(vacio ? "Sitio vacío" : nombre(o), 940, g_titulo), 256, 140, MARRON, g_titulo);
            std::string donde = s.donde;
            if (!vacio && enterrado(o))
                donde += " · Enterrado";
            if (!vacio && (w > 2 || h > 2))
                donde += " · Ocupa " + std::to_string(std::max(1, w / 2)) + " × " + std::to_string(std::max(1, h / 2)) + " cuadros";
            texto(encajar(donde, 940, g_fuente), 256, 196, MARRON_SUAVE);
            const std::string aviso = s.aviso ? s.aviso() : "";
            if (!aviso.empty())
            {
                const int aw = std::min(940, anchoTexto(aviso, g_pequena) + 28);
                redondo({256, 240, aw, 40}, 10, {0xFB, 0xE4, 0xE0, 255});
                texto(encajar(aviso, aw - 28, g_pequena), 270, 260 - ui::alto(g_pequena) / 2, ROJO, g_pequena);
            }
        };
        std::vector<Boton> bs;
        auto tarjeta_ = [&](const std::string& t, const std::string& d, const char* icono, std::function<void()> f) {
            Boton b{{}, t, std::move(f)};
            b.detalle = d;
            b.grande = true;
            b.imagen = dibujos::menu(icono);
            bs.push_back(b);
        };
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
            tarjeta_("Quitar", "Deja el sitio vacío", "Can", [s] {
                const std::string n = nombre(s.objeto());
                s.quitar();
                volver();
                cambiado("Quitado: " + n);
            });
            if (s.girar)
            {
                const char* const MIRA[] = {"Mira hacia abajo", "Mira a la derecha", "Mira hacia arriba", "Mira a la izquierda"};
                tarjeta_("Girar", MIRA[giro(o)], "Windmill", [s] { s.girar(); });
            }
            if (s.mover)
                tarjeta_("Mover", "Llévalo a otro sitio", "OneroomCardboard", [s] {
                    volver();
                    s.mover();
                });
            tarjeta_("Otro objeto", "Búscalo por su nombre", "Present", buscar);
            if (s.plantas)
                tarjeta_("Una planta", "Cambiarlo por árbol, flor…", "PltCosmos0", planta);
        }
        else
        {
            tarjeta_("Poner un objeto", "Búscalo por su nombre", "Present", buscar);
            if (s.plantas)
                tarjeta_("Poner una planta", "Árboles, flores, rocas…", "Seedling", planta);
        }
        // Los de más: «título\ndetalle».
        for (const auto& m : s.mas)
        {
            const size_t k = m.first.find('\n');
            tarjeta_(m.first.substr(0, k), k == std::string::npos ? "" : m.first.substr(k + 1), m.first.rfind("Goma", 0) == 0 ? "Duster" : "Can", m.second);
        }
        tarjeta_("Avanzado", "Cada valor, uno a uno", "ToolChangeStick", [s] {
            abrir(editorBruto("Opciones avanzadas", s.objeto, s.poner, [s] {
                std::string d = s.donde;
                if (s.posicion)
                    d += " · casilla " + std::to_string(s.posicion().first) + ", " + std::to_string(s.posicion().second);
                return d;
            }));
        });
        for (size_t i = 0; i < bs.size() && i < 9; i++)
        {
            bs[i].r = {60 + (int)(i % 3) * 392, 324 + (int)(i / 3) * 90, 376, 80};
            v.b.push_back(bs[i]);
        }
        v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
    };
}

void buscarObjeto(std::function<void(const Objeto&)> elegido, const std::string& para, int cat)
{
    // En tiempo real, con su teclado. Para paredes y suelos, sin escribir nada salen todos.
    auto hallados = std::make_shared<std::vector<int>>();
    Buscador b;
    b.titulo = cat == 2 ? "Papel, suelo o alfombra" : "Buscar objeto";
    b.explicacion = std::string(cat == 2 ? "Escribe parte del nombre (por ejemplo, «madera» o «azul»)" : "Escribe parte del nombre (por ejemplo, «silla» o «manzana»)") +
                    (para.empty() ? "." : " para " + para + ".");
    b.filtrar = [hallados, cat](const std::string& t) {
        hallados->clear();
        if (t.empty())
        {
            if (cat >= 0)
                for (int i = 0; i < (int)(sizeof(OBJETOS) / sizeof(OBJETOS[0])); i++)
                    if (categoria(OBJETOS[i].id) == cat)
                        hallados->push_back(i);
            return (int)hallados->size();
        }
        for (int i : buscarObjetos(t))
            if (cat < 0 || categoria(OBJETOS[i].id) == cat)
                hallados->push_back(i);
        return (int)hallados->size();
    };
    b.fila = [hallados, elegido](int k) {
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
    abrir(buscador(b));
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

// La lista de plantas, con buscar en tiempo real (X). «filaDe(i, niveles)»: la fila de PLANTAS_LISTA[i]; «niveles», a
// cuántas pantallas está la lista (1 desde ella, 2 desde el buscador).
Lista listaPlantas(std::shared_ptr<Plantas> e, const std::string& titulo, const std::string& para, std::function<Fila(int, int)> filaDe)
{
    Lista l;
    l.titulo = titulo;
    l.explicacion = [e, para] {
        std::string t = e->busqueda.empty() ? "Árboles, arbustos, flores, vallas, rocas..." : "Las que se llaman «" + e->busqueda + "».";
        return t + " " + para;
    };
    l.cuantas = [e] { return (int)e->vistas.size(); };
    l.porColumna = 7;
    l.fila = [e, filaDe](int k) { return filaDe(e->vistas[k], 1); };
    auto buscar = [e, titulo, filaDe] {
        Buscador b;
        b.titulo = "Buscar planta";
        b.explicacion = "Escribe parte del nombre (por ejemplo, «rosa» o «manzano»).";
        b.texto = e->busqueda;
        b.filtrar = [e](const std::string& t) {
            e->busqueda = t;
            e->filtrar();
            return (int)e->vistas.size();
        };
        b.fila = [e, filaDe](int k) { return filaDe(e->vistas[k], 2); };
        b.alSalir = [e](const std::string& t) {
            e->busqueda = t;
            e->filtrar();
        };
        abrir(buscador(b));
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
    Lista l = listaPlantas(e, "Plantas, vallas y rocas", "Elige una para " + para + ".", [elegida](int i, int niveles) {
        const auto& p = PLANTAS_LISTA[i];
        Fila f{p.nombre, [elegida, id = p.id, niveles] {
            for (int k = 0; k < niveles; k++)
                volver();
            elegida(id);
        }};
        ponerDibujo(f, p.id);
        return f;
    });
    abrir(lista(l));
}

void elegirPlantas(std::function<void(const std::vector<uint16_t>&, bool)> elegidas, const std::string& para)
{
    auto e = std::make_shared<Plantas>();
    e->filtrar();
    Lista l = listaPlantas(e, "Cambiar por…", "Marca una o varias para " + para + " (con varias, se mezclan).", [e](int i, int) {
        const auto& p = PLANTAS_LISTA[i];
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
    });
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
            tarjeta({60, 120, 1160, 116});
            foto({76, 132, 92, 92}, esNada(o) ? nullptr : dibujoDe(o), esNada(o) ? CREMA_OSCURA : colorDe(o));
            texto(encajar(esNada(o) ? std::string("Nada") : nombre(o), 1000, g_negrita), 188, 144, MARRON, g_negrita);
            texto(encajar(donde ? donde() : "", 1000, g_pequena), 188, 186, MARRON_SUAVE, g_pequena);
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
        v.b.push_back({{W - 60 - 300, ABAJO - 66, 300, 56}, "Volver", [] { volver(); }});
    };
}

} // namespace mapa
