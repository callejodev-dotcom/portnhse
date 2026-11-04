// Los valores en bruto de la habitación de un vecino (GSaveRoomFloorWall) y de su casa (VillagerHouse2), como la tabla
// de «Opciones avanzadas» de RoomWindow y VillagerHouseWindow de NHSE para Mac: cada valor con su número, cambiar con el
// teclado. Para expertos: lo normal es cambiar estas cosas desde «Paredes y suelo» y «Casa».
#include <algorithm>
#include <cstdlib>
#include <memory>
#include "../core/vecino.hpp"
#include "../core/vecino_tablas.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
constexpr int FILAS = 7;

template <typename T, size_t N>
std::string nombreEn(const T (&lista)[N], int64_t id)
{
    for (const auto& x : lista)
        if (x.id == id)
            return x.nombre;
    return "";
}

// Lo que es ese número, si se sabe («Pared de ladrillo», «Rocío»...).
std::string significado(TipoCampo tipo, int64_t v)
{
    switch (tipo)
    {
    case CampoObjeto:
    {
        if (v == NADA)
            return "Nada";
        Objeto o;
        o.id = (uint16_t)v;
        return objetos::nombre(o);
    }
    case CampoFachada:
        return nombreEn(FACHADAS, v);
    case CampoTejado:
        return nombreEn(TEJADOS, v);
    case CampoPuerta:
        for (const auto& p : PUERTAS)
            if (p.arco == v || p.recta == v)
                return p.nombre + std::string(p.recta == v && p.recta != p.arco ? " (recta)" : "");
        return "";
    case CampoVecino:
        return v >= 0 && v < NUM_VECINOS ? nombre((int)v) : v == -1 ? "Nadie" : "";
    default:
        return "";
    }
}

// Lee un número escrito: con signo, o en hexadecimal con «0x». false si no lo es.
bool leerNumero(const std::string& t, int64_t& v)
{
    std::string s;
    for (char c : t)
        if (c != ' ' && c != '.')
            s += c;
    if (s.empty())
        return false;
    char* fin = nullptr;
    v = strtoll(s.c_str(), &fin, 0);
    return fin && *fin == 0;
}

void pantalla(const std::string& titulo, const std::string& explicacion, const CampoBruto* campos, int n, std::function<uint8_t*()> base)
{
    auto pagina = std::make_shared<int>(0);
    abrir([=](Vista& v) {
        uint8_t* b = base();
        if (!b)
        {
            volver();
            return;
        }
        const int paginas = (n + FILAS - 1) / FILAS;
        *pagina = std::clamp(*pagina, 0, paginas - 1);
        v.titulo = titulo;
        v.explicacion = explicacion + " (página " + std::to_string(*pagina + 1) + " de " + std::to_string(paginas) + ")";
        for (int f = 0; f < FILAS && *pagina * FILAS + f < n; f++)
        {
            const int k = *pagina * FILAS + f;
            const CampoBruto& c = campos[k];
            const int64_t valor = leerCampo(b, c);
            const std::string que = significado(c.tipo, valor);
            Boton x{{60, 140 + f * 64, 1160, 58}, c.nombre, [=] {
                std::string t = std::to_string(leerCampo(base(), c));
                const std::string rango = "de " + std::to_string(minimoCampo(c)) + " a " + std::to_string(maximoCampo(c));
                if (!pedirTexto(std::string(c.nombre) + " (" + rango + ")", "Un número (o en hexadecimal, con 0x delante)", t, 12))
                    return;
                int64_t nuevo;
                if (!leerNumero(t, nuevo) || nuevo < minimoCampo(c) || nuevo > maximoCampo(c))
                {
                    avisar("Tiene que ser un número " + rango + ".", true);
                    return;
                }
                ponerCampo(base(), c, nuevo);
                partida::cambiaPrincipal();
                cambiado(std::string(c.nombre) + ": " + std::to_string(nuevo));
            }};
            x.detalle = std::to_string(valor) + (que.empty() ? "" : " · " + que) + "  ·  " + c.interno;
            v.b.push_back(x);
        }
        const int y = ABAJO - 72;
        v.b.push_back({{60, y, 260, 60}, "Anteriores", [pagina] { (*pagina)--; }, *pagina > 0});
        v.b.push_back({{340, y, 260, 60}, "Siguientes", [pagina] { (*pagina)++; }, *pagina + 1 < paginas});
        v.b.push_back({{960, y, 260, 60}, "Volver", [] { volver(); }});
        v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + (d > 0 ? 1 : -1), 0, paginas - 1); };
        v.teclas = {{"A", "Cambiar"}, {"L R", "Página"}, {"B", "Volver"}};
    });
}
} // namespace

void brutoCuarto(int hueco)
{
    pantalla("Habitación de " + nombre(hueco) + ": en bruto", "Los números que guarda el juego de las paredes y el suelo. Para expertos",
        CAMPOS_CUARTO, NUM_CAMPOS_CUARTO, [hueco] { return datosVecino(partida::principal(), hueco) + V_HABITACION; });
}

void brutoCasa(int hueco)
{
    // La casa de ahora (aunque luego se cambie quién vive en ella).
    const int k = casaDe(partida::principal(), hueco);
    if (k < 0)
        return;
    pantalla("Casa de " + nombre(hueco) + ": en bruto", "Los números que guarda el juego de la casa. Para expertos", CAMPOS_CASA, NUM_CAMPOS_CASA,
        [k]() -> uint8_t* { return datosCasa(partida::principal(), k); });
}

} // namespace vecinos
