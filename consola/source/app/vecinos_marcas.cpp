// Las marcas sueltas de un vecino (256 números), como VillagerFlagEditor de NHSE para Mac, y la lista de marcas con
// nombre que usan también los recuerdos (vecinos_recuerdos.cpp): en páginas, con lo que significa cada una, buscar,
// ver solo las que no están a 0 o las cambiadas, y guardarlas y cargarlas de la tarjeta SD (.nhfl, como el Mac).
#include <algorithm>
#include <map>
#include <memory>
#include "../core/vecino.hpp"
#include "../core/vecino_marcas.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
const char* const FILTROS[4] = {"Con nombre", "Distintas de 0", "Cambiadas", "Todas"};
constexpr int FILAS = 6;

struct Estado {
    ListaMarcas l;
    std::map<int, int> conNombre;   // flag number -> position in the named lists
    std::vector<int> original;      // value of each flag when opened
    std::vector<std::string> claves; // for searching (named ones)
    std::string busqueda;
    int filtro = 0;
    std::vector<int> vistas;        // flag numbers shown
    int pagina = 0;

    void filtrar()
    {
        std::vector<std::string> palabras;
        std::string w;
        for (char c : normalizar(busqueda) + " ")
        {
            if (c != ' ')
                w += c;
            else if (!w.empty())
                palabras.push_back(w), w.clear();
        }
        vistas.clear();
        for (int k = 0; k < l.total; k++)
        {
            auto it = conNombre.find(k);
            if (filtro == 0 && it == conNombre.end())
                continue;
            if (!palabras.empty())
            {
                if (it == conNombre.end())
                    continue;
                bool ok = true;
                for (const auto& p : palabras)
                    ok = ok && claves[it->second].find(p) != std::string::npos;
                if (!ok)
                    continue;
            }
            const int v = l.leer(k);
            if ((filtro == 1 && v == 0) || (filtro == 2 && v == original[k]))
                continue;
            vistas.push_back(k);
        }
        pagina = 0;
    }
    int cambiadas() const
    {
        int n = 0;
        for (int k = 0; k < l.total; k++)
            n += l.leer(k) != original[k];
        return n;
    }
};

void cambio(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}
} // namespace

void listaMarcas(ListaMarcas lista)
{
    auto e = std::make_shared<Estado>();
    e->l = std::move(lista);
    for (int i = 0; i < (int)e->l.indices.size(); i++)
    {
        e->conNombre[e->l.indices[i]] = i;
        e->claves.push_back(normalizar(e->l.textos[i] + " " + e->l.internos[i]));
    }
    for (int k = 0; k < e->l.total; k++)
        e->original.push_back(e->l.leer(k));
    e->filtrar();

    auto buscar = [e] {
        std::string q = e->busqueda;
        if (!pedirTexto("Buscar", "Amistad, visitas, Navidad, cumpleaños… (vacío: todas)", q))
            return;
        e->busqueda = q;
        e->filtrar();
        enfocar(0);
    };
    auto filtro = [e] {
        e->filtro = (e->filtro + 1) % 4;
        e->filtrar();
        enfocar(0);
    };
    auto mas = [e] {
        std::vector<std::string> op = {"Deshacer todos los cambios"};
        for (const auto& m : e->l.mas)
            op.push_back(m.first);
        elegir("Más opciones", "Lo que se guarda en la SD se puede volver a cargar aquí o en NHSE para Mac.", op, -1, [e](int o) {
            if (o == 0)
            {
                for (int k = 0; k < e->l.total; k++)
                    if (e->l.leer(k) != e->original[k])
                        e->l.poner(k, e->original[k]);
                e->filtrar();
                avisar("Deshechos los cambios.");
                return;
            }
            e->l.mas[o - 1].second();
            e->filtrar();
        }, 1);
    };
    abrir([e, buscar, filtro, mas](Vista& v) {
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + FILAS - 1) / FILAS);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = e->l.titulo;
        std::string s = e->busqueda.empty() ? "" : "«" + e->busqueda + "» · ";
        const int cambiadas = e->cambiadas();
        v.explicacion = s + std::to_string(total) + (total == 1 ? " marca" : " marcas") + " · " + std::to_string(cambiadas) +
            (cambiadas == 1 ? " cambiada" : " cambiadas") + " · página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas);
        for (int f = 0; f < FILAS && e->pagina * FILAS + f < total; f++)
        {
            const int k = e->vistas[e->pagina * FILAS + f];
            auto it = e->conNombre.find(k);
            const bool nombre = it != e->conNombre.end();
            const int maximo = nombre ? e->l.maximos[it->second] : 0;
            const bool siNo = maximo == 1;
            const int valor = e->l.leer(k);
            std::string detalle = siNo && valor <= 1 ? (valor ? "Sí" : "No") : "Vale " + numero(valor);
            if (!siNo && maximo > 1)
                detalle += " (normalmente de 0 a " + numero(maximo) + ")";
            if (valor != e->original[k])
                detalle += " · cambiada";
            detalle += "  ·  nº " + std::to_string(k) + (nombre ? " · " + e->l.internos[it->second] : "");
            const std::string texto = nombre ? e->l.textos[it->second] : "Marca sin nombre";
            const uint32_t tope = e->l.tope;
            v.b.push_back({{60, 192 + f * 68, 1160, 60}, texto, [e, k, siNo, maximo, tope] {
                const int actual = e->l.leer(k);
                if (siNo && actual <= 1)
                {
                    e->l.poner(k, actual ? 0 : 1);
                    cambiado(actual ? "Ahora: no" : "Ahora: sí");
                    return;
                }
                uint32_t n = (uint32_t)actual;
                const std::string hasta = maximo > 1 ? " (lo normal: de 0 a " + numero(maximo) + ")" : "";
                if (!pedirNumero("Marca nº " + std::to_string(k) + ": nuevo valor" + hasta, n, tope))
                    return;
                e->l.poner(k, (int)n);
                cambiado("Ahora vale " + numero(n));
            }, true, valor != 0, detalle});
        }
        if (total == 0)
            v.dibujo = [] { texto("No hay ninguna con eso.", W / 2, 360, MARRON_SUAVE, nullptr, Centro); };
        else
        {
            // Lo que significa la elegida, entera, encima de las filas (en la fila se corta si es larga).
            const int f = foco();
            if (f >= 0 && f < (int)v.b.size())
            {
                const std::string completo = v.b[f].texto;
                v.dibujo = [completo] { texto(completo, 60, 136, MARRON, g_pequena, Izquierda, 1160); };
            }
        }
        struct Abajo {
            std::string texto;
            std::function<void()> accion;
            bool activo;
            bool marcado = false;
        };
        auto e2 = e;
        std::vector<Abajo> abajo = {
            {"Anteriores", [e2] { e2->pagina--; }, e->pagina > 0},
            {"Siguientes", [e2] { e2->pagina++; }, e->pagina + 1 < paginas},
            {"Buscar…", buscar, true},
            {FILTROS[e->filtro], filtro, true, e->filtro != 0},
            {"Más…", mas, true},
            {"Volver", [] { volver(); }, true},
        };
        const int ancho = (W - 120 - 5 * 14) / 6;
        for (int i = 0; i < 6; i++)
            v.b.push_back({{60 + i * (ancho + 14), ABAJO - 62, ancho, 52}, abajo[i].texto, abajo[i].accion, abajo[i].activo, abajo[i].marcado});
        v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        v.x = buscar;
        v.y = filtro;
        v.menos = mas;
        v.teclas = {{"A", "Cambiar"}, {"X", "Buscar"}, {"Y", "Ver"}, {"−", "Más"}, {"L R", "Página"}, {"B", "Volver"}};
    });
}

// ------------------------------------------------------------------------------------------- las del vecino
std::string resumenMarcas(int hueco)
{
    int n = 0;
    for (int i = 0; i < NUM_MARCAS_VECINO; i++)
        n += marcaVecino(partida::principal(), hueco, i) != 0;
    return std::to_string(n) + (n == 1 ? " marca puesta" : " marcas puestas");
}

void marcas(int hueco)
{
    constexpr const char* CARPETA_MARCAS = "sdmc:/switch/NHSE/marcas/";
    ListaMarcas l;
    l.titulo = "Marcas de " + nombre(hueco);
    l.total = NUM_MARCAS_VECINO;
    l.tope = 65535;
    for (const auto& m : MARCAS_VECINO)
    {
        l.indices.push_back(m.indice);
        l.textos.push_back(m.texto);
        l.internos.push_back(m.interno);
        l.maximos.push_back(m.maximo);
    }
    l.leer = [hueco](int i) { return (int)marcaVecino(partida::principal(), hueco, i); };
    l.poner = [hueco](int i, int v) {
        ponerMarcaVecino(partida::principal(), hueco, i, (uint16_t)v);
        partida::cambiaPrincipal();
    };
    l.mas.push_back({"Guardar en la SD", [hueco] {
        const std::string n = archivo(nombre(hueco) + " - EventFlagVillager", "nhfl");
        const bool ok = partida::escribirSd(CARPETA_MARCAS + n, marcasVecinoEnBruto(partida::principal(), hueco));
        avisar(ok ? "Guardadas en /switch/NHSE/marcas/" + n : "No he podido escribir en la SD.", !ok);
    }});
    l.mas.push_back({"Cargar de la SD", [hueco] {
        const auto archivos = partida::listarSd(CARPETA_MARCAS, ".nhfl");
        if (archivos.empty())
        {
            avisar("No hay marcas guardadas en /switch/NHSE/marcas/.", true);
            return;
        }
        std::vector<Opcion> op;
        for (const auto& a : archivos)
            op.push_back({a, normalizar(a)});
        elegirLargo("Cargar marcas", "Se cambian todas las marcas de " + nombre(hueco) + " por las del archivo.", op, -1, [hueco, archivos](int i) {
            std::vector<uint8_t> d;
            if (!partida::leerSd(CARPETA_MARCAS + archivos[i], d))
                avisar("No he podido leer el archivo.", true);
            else if (!cargarMarcasVecino(partida::principal(), hueco, d))
                avisar("Ese archivo no es de marcas de un vecino (tamaño distinto).", true);
            else
                cambio("Cargadas");
        });
    }});
    listaMarcas(std::move(l));
}

} // namespace vecinos
