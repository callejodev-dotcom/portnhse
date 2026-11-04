// Sección «Vecinos»: los vecinos de la isla, como la página de vecinos de NHSE para Mac: arriba la cuadrícula con la cara
// de cada uno y debajo la ficha del elegido (personalidad, muletilla, amistad con sus corazones, paredes y suelo,
// bricolaje, mudanza, cambiarlo por otro, ropa, muebles, casa, y pasarlo a la tarjeta SD o traerlo de ella); y lo del
// editor clásico: recuerdos, marcas sueltas y su diseño. También: amistad de cada jugador, mejores amigos de todos, que
// se muden todos y guardar todos en la tarjeta SD.
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include "../core/objetos.hpp"
#include "../core/vecino.hpp"
#include "../core/vecinos.hpp"
#include "dibujos.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "retratos.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

std::string nombre(int hueco)
{
    const std::string interno = vecinoEn(partida::principal(), hueco);
    if (interno.empty())
        return "Parcela libre";
    for (const auto& v : LISTA_VECINOS)
        if (interno == v.interno)
            return v.nombre;
    return interno;
}

std::string archivo(const std::string& nombre, const std::string& extension)
{
    std::string r;
    for (char c : nombre)
        if (!strchr("\\/:*?\"<>|", c))
            r += c;
    return (r.empty() ? std::string("vecino") : r) + "." + extension;
}

} // namespace vecinos

using namespace vecinos;

namespace {

constexpr int NUM_LISTA = sizeof(LISTA_VECINOS) / sizeof(LISTA_VECINOS[0]);
constexpr int NUM_OBJETOS = sizeof(OBJETOS) / sizeof(OBJETOS[0]);

// El vecino que se ve en la ficha (se recuerda al volver a la página).
int g_elegido = -1;

const std::vector<uint8_t>& datosVecinos()
{
    static const std::vector<uint8_t> datos = [] {
        std::vector<uint8_t> d;
        FILE* f = fopen("romfs:/vecinos.bin", "rb");
        if (!f)
            return d;
        fseek(f, 0, SEEK_END);
        d.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        if (fread(d.data(), 1, d.size(), f) != d.size())
            d.clear();
        fclose(f);
        return d;
    }();
    return datos;
}

void cambio(const std::string& que = "Cambiado")
{
    partida::cambiaPrincipal();
    cambiado(que);
}

bool vive(int hueco) { return !vecinoEn(partida::principal(), hueco).empty(); }

// ------------------------------------------------------------------------------------------- elegir vecino nuevo
// Como VillagerPicker del Mac: las caras de todos, y arriba un buscador que filtra mientras se escribe (con su propio
// teclado en pantalla).
Pantalla elegirVecino(int hueco)
{
    struct Estado {
        std::string texto;
        std::string filtrado = "\x01"; // the text of the last search (none yet)
        std::vector<int> vistas;
        int pagina = 0;
        bool primera = true;
    };
    static const std::vector<std::string> claves = [] {
        std::vector<std::string> c;
        for (const auto& v : LISTA_VECINOS)
            c.push_back(normalizar(v.nombre));
        return c;
    }();
    constexpr int COLS = 7, FILAS = 2, POR_PAGINA = COLS * FILAS;
    auto e = std::make_shared<Estado>();
    return [hueco, e](Vista& v) {
        const auto& m = partida::principal();
        const std::string actual = vecinoEn(m, hueco);
        if (e->filtrado != e->texto)
        {
            e->vistas.clear();
            const std::string q = normalizar(e->texto);
            for (int i = 0; i < NUM_LISTA; i++)
                if (q.empty() || claves[i].find(q) != std::string::npos)
                    e->vistas.push_back(i);
            e->filtrado = e->texto;
            e->pagina = 0;
            // The first time, on the page of the one there is now (as the Mac, that shows it chosen).
            if (e->primera)
                for (int k = 0; k < (int)e->vistas.size(); k++)
                    if (actual == LISTA_VECINOS[e->vistas[k]].interno)
                        e->pagina = k / POR_PAGINA;
            e->primera = false;
        }
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = "¿Quién viene en lugar de " + nombre(hueco) + "?";
        v.explicacion = "Escribe parte del nombre y elige su cara. El nuevo vecino se queda con la misma casa.";

        const std::string escrito = e->texto;
        const std::string cuantos = total == 0 ? "Nadie se llama así"
            : numero((uint32_t)total) + (total == 1 ? " vecino" : " vecinos") +
                (paginas > 1 ? " · página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) : "");
        v.dibujo = [escrito, cuantos] {
            redondo({60, 112, W - 120, 50}, 12, AZUL);
            redondo({62, 114, W - 124, 46}, 10, BLANCO);
            const int y = 137 - TTF_FontHeight(g_fuente) / 2;
            if (escrito.empty())
                texto("Buscar por nombre", 80, y, MARRON_SUAVE);
            else
                texto(escrito, 80, y, MARRON);
            if ((SDL_GetTicks() / 500) % 2 == 0)
                rect({80 + (escrito.empty() ? 0 : anchoTexto(escrito)) + 2, 121, 3, 32}, AZUL);
            texto(cuantos, W - 80, 137 - TTF_FontHeight(g_pequena) / 2, MARRON_SUAVE, g_pequena, Derecha);
        };

        // The faces.
        constexpr int ANCHO = 159, ALTO = 118, HUECO = 7;
        for (int k = 0; k < POR_PAGINA && e->pagina * POR_PAGINA + k < total; k++)
        {
            const auto& vv = LISTA_VECINOS[e->vistas[e->pagina * POR_PAGINA + k]];
            bool yaEsta = false;
            for (int j = 0; j < NUM_VECINOS; j++)
                yaEsta |= vecinoEn(m, j) == vv.interno;
            Boton b{{60 + (k % COLS) * (ANCHO + HUECO), 172 + (k / COLS) * (ALTO + HUECO), ANCHO, ALTO}, vv.nombre,
                [hueco, interno = std::string(vv.interno), nombre = std::string(vv.nombre)] {
                    const std::string antes = vecinos::nombre(hueco);
                    const std::string mal = cambiarVecino(partida::principal(), hueco, interno, datosVecinos());
                    if (mal.empty())
                        partida::cambiaPrincipal();
                    volver();
                    avisar(mal.empty() ? antes + " se va y llega " + nombre + ". Falta guardar." : mal, !mal.empty());
                }, !yaEsta, actual == vv.interno};
            b.vertical = true;
            b.imagen = retratos::de(vv.interno);
            v.b.push_back(b);
        }

        // The keyboard: the letters of the names.
        static const char* const TECLAS[3] = {"qwertyuiop", "asdfghjklñ", "zxcvbnm"};
        const int arriba = 172 + FILAS * (ALTO + HUECO) + 4;
        for (int f = 0; f < 3; f++)
        {
            const std::string fila = TECLAS[f];
            int col = 0;
            for (size_t i = 0; i < fila.size(); col++)
            {
                const size_t n = ((unsigned char)fila[i] & 0xE0) == 0xC0 ? 2 : 1;
                const std::string letra = fila.substr(i, n);
                i += n;
                v.b.push_back({{60 + col * 117, arriba + f * 50, 109, 44}, letra, [e, letra] { e->texto += letra; }});
            }
        }
        auto borrar = [e] {
            if (e->texto.empty())
                return;
            size_t i = e->texto.size() - 1;
            while (i > 0 && ((unsigned char)e->texto[i] & 0xC0) == 0x80)
                i--;
            e->texto.erase(i);
        };
        v.b.push_back({{60 + 7 * 117, arriba + 100, 226, 44}, "Borrar", borrar, !e->texto.empty()});
        v.b.push_back({{60 + 9 * 117, arriba + 100, 109, 44}, "Espacio", [e] { e->texto += " "; }});
        const int y = arriba + 150 + 6;
        v.b.push_back({{60, y, 220, 52}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
        v.b.push_back({{292, y, 220, 52}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
        v.b.push_back({{524, y, 260, 52}, "Teclado…", [e] {
            std::string t = e->texto;
            if (pedirTexto("Buscar vecino", "Escribe parte del nombre", t, 20))
                e->texto = t;
        }});
        v.b.push_back({{1000, y, 220, 52}, "Volver", [] { volver(); }});
        v.x = borrar;
        v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + (d > 0 ? 1 : -1), 0, paginas - 1); };
        v.teclas = {{"A", "Elegir"}, {"X", "Borrar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
}

// ------------------------------------------------------------------------------------------- ropa y muebles
objetos::Huecos guardado(int hueco, int lista)
{
    objetos::Huecos h;
    h.nombre = (lista == 0 ? "Ropa de " : "Muebles de ") + nombre(hueco);
    h.cuantos = [lista] { return lista == 0 ? NUM_ROPA : NUM_MUEBLES; };
    h.leer = [hueco, lista](int i) { return objetoVecino(partida::principal(), hueco, lista, i); };
    h.escribir = [hueco, lista](int i, const Objeto& o) {
        ponerObjetoVecino(partida::principal(), hueco, lista, i, o);
        partida::cambiaPrincipal();
    };
    return h;
}

// ------------------------------------------------------------------------------------------- bricolaje
// Las recetas: su número (los objetos «Receta: …» lo llevan en la cantidad), su nombre y el objeto que sale.
struct Receta {
    uint16_t numero;
    std::string nombre, clave;
    int objeto; // index in OBJETOS of what is made, or -1
    int papel;  // index in OBJETOS of the recipe
};

const std::vector<Receta>& recetas()
{
    static const std::vector<Receta> r = [] {
        std::map<std::string, int> porClave;
        for (int i = 0; i < NUM_OBJETOS; i++)
            if (OBJETOS[i].tipo == 0)
                porClave.emplace(OBJETOS[i].clave, i);
        std::vector<Receta> v;
        for (int i = 0; i < NUM_OBJETOS; i++)
            if (OBJETOS[i].tipo == 1)
            {
                std::string n = OBJETOS[i].nombre, c = OBJETOS[i].clave;
                if (n.rfind("Receta: ", 0) == 0)
                    n = n.substr(8), c = c.substr(std::min<size_t>(8, c.size()));
                // «(Sin variaciones)» says nothing to Carla.
                for (const char* sin : {" (Sin variaciones)", " (sin variaciones)"})
                    if (n.size() > strlen(sin) && n.compare(n.size() - strlen(sin), std::string::npos, sin) == 0)
                        n.erase(n.size() - strlen(sin));
                // «abrigo de paja (Sin variaciones)»: the object is «abrigo de paja».
                auto it = porClave.find(c);
                if (it == porClave.end() && c.find(" (") != std::string::npos)
                    it = porClave.find(c.substr(0, c.find(" (")));
                v.push_back({OBJETOS[i].cuenta, n, c, it == porClave.end() ? -1 : it->second, i});
            }
        return v;
    }();
    return r;
}

const Receta* receta(uint16_t numero)
{
    static const std::map<uint16_t, int> indice = [] {
        std::map<uint16_t, int> m;
        for (int i = 0; i < (int)recetas().size(); i++)
            m.emplace(recetas()[i].numero, i);
        return m;
    }();
    auto it = indice.find(numero);
    return it == indice.end() ? nullptr : &recetas()[it->second];
}

std::string nombreReceta(uint16_t numero)
{
    if (numero == 0xFFFF)
        return "Nada";
    const Receta* r = receta(numero);
    return r ? r->nombre : "Receta nº " + std::to_string(numero);
}

// El dibujo de lo que sale de la receta (o el de la receta, si no se sabe).
SDL_Texture* dibujoReceta(uint16_t numero)
{
    const Receta* r = receta(numero);
    if (!r)
        return dibujos::menu("DIYWoodNormal");
    if (r->objeto >= 0)
        if (SDL_Texture* t = dibujos::de(OBJETOS[r->objeto].id))
            return t;
    return dibujos::de(OBJETOS[r->papel].id);
}

std::string dosCifras(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

std::string resumenBricolaje(int hueco)
{
    const Bricolaje b = bricolaje(partida::principal(), hueco);
    if (!b.haciendo() || b.receta == 0xFFFF)
        return "No está haciendo nada";
    return "Hace " + nombreReceta(b.receta) + ". Termina a las " + dosCifras(b.hora) + ":" + dosCifras(b.minuto) + ".";
}

void ponerBricolajeCon(int hueco, std::function<void(Bricolaje&)> f, const std::string& que)
{
    Bricolaje n = bricolaje(partida::principal(), hueco);
    f(n);
    ponerBricolaje(partida::principal(), hueco, n);
    cambio(que);
}

void buscarReceta(int hueco)
{
    auto vistas = std::make_shared<std::vector<int>>();
    Buscador b;
    b.titulo = "¿Qué está haciendo " + nombre(hueco) + "?";
    b.explicacion = "Escribe parte del nombre de la receta (por ejemplo, «escalera») y elígela.";
    b.filtrar = [vistas](const std::string& t) {
        vistas->clear();
        const std::string q = normalizar(t);
        for (int i = 0; i < (int)recetas().size(); i++)
            if (recetas()[i].clave.find(q) != std::string::npos)
                vistas->push_back(i);
        return (int)vistas->size();
    };
    b.fila = [vistas, hueco](int k) {
        const Receta& r = recetas()[(*vistas)[k]];
        const uint16_t n = r.numero;
        Fila f{r.nombre, [hueco, n] {
            volver();
            ponerBricolajeCon(hueco, [n](Bricolaje& x) { x.receta = n; }, "Ahora hace " + nombreReceta(n));
        }};
        f.marcado = bricolaje(partida::principal(), hueco).receta == n;
        f.imagen = dibujoReceta(n);
        return f;
    };
    abrir(buscador(b));
}

// Como el editor de bricolaje del Mac (¿está haciendo algo?, hasta qué hora y qué receta), en una tarjeta.
void pantallaBricolaje(Vista& v, int hueco)
{
    const Bricolaje b = bricolaje(partida::principal(), hueco);
    const bool haciendo = b.haciendo();
    v.titulo = "Bricolaje de " + nombre(hueco);
    v.explicacion = "A veces los vecinos están en casa haciendo algo de bricolaje: qué hacen y a qué hora terminan.";
    SDL_Texture* cara = retratos::deHueco(hueco);
    const std::string resumen = resumenBricolaje(hueco);
    v.dibujo = [cara, resumen] {
        const SDL_Rect r = {60, 112, 1160, 476};
        panel(r, "¿Qué está haciendo?", "DIYWoodNormal", resumen);
        if (cara)
        {
            const SDL_Rect d = {1220 - 24 - 130, 112 + 16, 130, 130};
            SDL_RenderCopy(g_r, cara, nullptr, &d);
        }
        texto("Termina a las", 84, 432, MARRON_SUAVE, g_pequena);
    };
    Boton si{{84, 206, 420, 70}, "Está haciendo algo", [hueco, haciendo] {
        // Como el editor clásico: sin hacer nada, la hora es 255:255; al empezar, las 00:00:00.
        if (haciendo)
            ponerBricolajeCon(hueco, [](Bricolaje& n) { n.hora = n.minuto = 255; }, "Ya no está haciendo nada");
        else
            ponerBricolajeCon(hueco, [](Bricolaje& n) { n.hora = n.minuto = n.segundo = 0; }, "Está haciendo algo");
    }, true, haciendo};
    si.detalle = haciendo ? "Sí (pulsa para que no)" : "No (pulsa para que sí)";
    v.b.push_back(si);
    Boton rec{{84, 290, 1112, 120}, nombreReceta(b.receta), [hueco] { buscarReceta(hueco); }};
    rec.grande = true;
    rec.imagen = b.receta == 0xFFFF ? dibujos::menu("DIYWoodNormal") : dibujoReceta(b.receta);
    rec.detalle = "La receta que está haciendo · pulsa para elegir otra";
    v.b.push_back(rec);
    Boton hora{{84, 462, 300, 104}, "Hora", [hueco, b] {
        uint32_t h = b.hora < 24 ? b.hora : 0;
        if (pedirNumero("¿A qué hora termina? (de 0 a 23)", h, 23))
            ponerBricolajeCon(hueco, [h](Bricolaje& n) { n.hora = (uint8_t)h; }, "Termina a las " + dosCifras(h) + ":" + dosCifras(b.minuto));
    }, haciendo};
    hora.grande = true;
    hora.valor = haciendo ? dosCifras(b.hora) : "—";
    hora.detalle = "de 0 a 23";
    v.b.push_back(hora);
    Boton minuto{{400, 462, 300, 104}, "Minuto", [hueco, b] {
        uint32_t m = b.minuto < 60 ? b.minuto : 0;
        if (pedirNumero("¿En qué minuto termina? (de 0 a 59)", m, 59))
            ponerBricolajeCon(hueco, [m](Bricolaje& n) { n.minuto = (uint8_t)m; }, "Termina a las " + dosCifras(b.hora) + ":" + dosCifras(m));
    }, haciendo};
    minuto.grande = true;
    minuto.valor = haciendo ? dosCifras(b.minuto) : "—";
    minuto.detalle = "de 0 a 59";
    v.b.push_back(minuto);
    v.b.push_back({{960, ABAJO - 64, 260, 56}, "Volver", [] { volver(); }});
}

// ------------------------------------------------------------------------------------------- archivos
void exportar(int hueco)
{
    const std::string n = archivo(nombre(hueco), "nhv2");
    if (partida::escribirSd(std::string(CARPETA) + n, exportarVecino(partida::principal(), hueco)))
        avisar("Guardado en la tarjeta SD: /switch/NHSE/vecinos/" + n);
    else
        avisar("No he podido guardarlo en la tarjeta SD.", true);
}

void ponerDeArchivo(int hueco, const std::vector<uint8_t>& datos, bool hacerSuyo)
{
    const std::string mal = importarVecino(partida::principal(), hueco, datos, hacerSuyo, partida::personal(0));
    if (!mal.empty())
    {
        avisar(mal, true);
        return;
    }
    partida::cambiaPrincipal();
    cambiado("Se ha importado a " + nombre(hueco));
}

void importar(int hueco)
{
    std::vector<std::string> nombres = partida::listarSd(CARPETA, ".nhv2");
    for (const auto& n : partida::listarSd(CARPETA, ".nhv"))
        nombres.push_back(n);
    if (nombres.empty())
    {
        avisar("No hay vecinos en la carpeta /switch/NHSE/vecinos de la tarjeta SD.", true);
        return;
    }
    std::vector<Opcion> op;
    for (const auto& n : nombres)
        op.push_back({n, normalizar(n)});
    elegirLargo("¿Qué vecino viene en lugar de " + nombre(hueco) + "?",
        "Los vecinos guardados en /switch/NHSE/vecinos de la tarjeta SD. La casa sigue siendo la misma.", op, -1, [hueco, nombres](int k) {
            std::vector<uint8_t> datos;
            if (!partida::leerSd(std::string(CARPETA) + nombres[k], datos) || vecinoDeArchivo(datos).empty())
            {
                avisar("Este archivo no es un vecino de Animal Crossing: New Horizons.", true);
                return;
            }
            const std::vector<uint8_t> v = vecinoDeArchivo(datos);
            if (!deOtraIsla(v, partida::personal(0)))
            {
                ponerDeArchivo(hueco, datos, false);
                return;
            }
            // Como el Mac: ¿que pase a ser de esta isla? Es lo normal.
            const std::string isla = islaDe(v);
            abrir([hueco, datos, isla](Vista& vi) {
                vi.titulo = "Viene de otra isla";
                vi.explicacion = "Este vecino viene de otra isla (" + isla + "). ¿Quieres que pase a ser de la tuya, para que te "
                                 "recuerde a ti y a tu isla? Es lo normal.";
                vi.b.push_back({{140, 300, 480, 100}, "Sí, que sea de mi isla", [hueco, datos] {
                    volver();
                    ponerDeArchivo(hueco, datos, true);
                }});
                vi.b.push_back({{660, 300, 480, 100}, "No, dejarlo como está", [hueco, datos] {
                    volver();
                    ponerDeArchivo(hueco, datos, false);
                }});
                vi.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Cancelar", [] { volver(); }});
            });
        });
}

// ------------------------------------------------------------------------------------------- la página
// Como VillagersPage del Mac: arriba los diez huecos con la cara de cada vecino; debajo, la ficha del elegido (el que
// tiene el cursor, o el último que se tocó), con cada dato a la izquierda y lo que lo cambia a la derecha.
constexpr int CUADRO_Y = 112, CUADRO_W = 220, CUADRO_H = 128, CUADRO_PASO_X = 235, CUADRO_PASO_Y = 136;
constexpr int FICHA_Y = CUADRO_Y + 2 * CUADRO_PASO_Y, FICHA_H = 214; // 384 .. 598
constexpr int ETIQUETA_X = 228, VALOR_X = 384, FIN_X = 1010, EDITAR_W = 104, DERECHA_X = 1024, DERECHA_W = 180;
constexpr int FILA_Y = FICHA_Y + 10, FILA_PASO = 50, FILA_H = 44;

int filaY(int f) { return FILA_Y + f * FILA_PASO; }

// «Pared cerámica amarilla» → «cerámica amarilla» (en la ficha ya lo dicen la etiqueta y el dibujo).
std::string sinPrefijo(const std::string& n)
{
    for (const char* p : {"Pared ", "Suelo "})
        if (n.rfind(p, 0) == 0 && n.size() > strlen(p))
            return n.substr(strlen(p));
    return n;
}

void pagina(Vista& v)
{
    auto& m = partida::principal();
    v.titulo = "Tus vecinos";
    v.explicacion = "Elige un vecino para ver y cambiar todo lo suyo. L y R: el anterior o el siguiente.";
    // Which one the ficha shows: the one with the cursor, if it is on a face; if not, the last one chosen.
    const int f = foco();
    if (f >= 0 && f < NUM_VECINOS && vive(f))
        g_elegido = f;
    if (g_elegido < 0 || g_elegido >= NUM_VECINOS || !vive(g_elegido))
    {
        g_elegido = -1;
        for (int i = 0; i < NUM_VECINOS && g_elegido < 0; i++)
            if (vive(i))
                g_elegido = i;
    }
    const int hueco = g_elegido;
    for (int i = 0; i < NUM_VECINOS; i++)
    {
        const bool vacio = !vive(i);
        Boton b{{60 + (i % 5) * CUADRO_PASO_X, CUADRO_Y + (i / 5) * CUADRO_PASO_Y, CUADRO_W, CUADRO_H}, nombre(i), [i] {
            g_elegido = i;
            enfocar(NUM_VECINOS);
        }, !vacio, i == hueco};
        b.vertical = true;
        if (!vacio)
            b.imagen = retratos::deHueco(i);
        v.b.push_back(b);
    }
    v.paginar = [](int d) {
        for (int k = 1; k <= NUM_VECINOS; k++)
        {
            const int otro = ((g_elegido + d * k) % NUM_VECINOS + NUM_VECINOS * 10) % NUM_VECINOS;
            if (vive(otro))
            {
                // With the cursor on a face, it goes with it.
                if (foco() >= 0 && foco() < NUM_VECINOS)
                    enfocar(otro);
                g_elegido = otro;
                return;
            }
        }
    };
    v.teclas = {{"A", "Elegir"}, {"L R", "Otro vecino"}, {"B", "Volver"}};
    if (hueco < 0)
    {
        v.dibujo = [] {
            panel({60, FICHA_Y, 1160, FICHA_H}, "No vive nadie en la isla", "Leaf", "Cuando llegue alguien, aparecerá aquí.");
        };
        v.b.push_back({{1000, ABAJO - 60, 220, 52}, "Volver", [] { volver(); }});
        return;
    }

    // ---- the ficha
    const std::string n = nombre(hueco);
    const std::string interno = vecinoEn(m, hueco);
    const int p = personalidad(m, hueco);
    const std::string fr = frase(m, hueco), original = fraseOriginal(interno);
    const bool muda = seMuda(m, hueco);
    const int r = recuerdoDe(m, hueco, partida::personal());
    const int nivel = r >= 0 ? nivelAmistad(recuerdo(m, hueco, r).amistad) : 0;
    const std::string amistadTexto = r >= 0 ? NIVELES[nivel] : "Todavía no te conoce";
    const std::string conQuien = r >= 0 ? "con " + partida::jugador().nombre : "";
    const uint16_t pared = parteCuarto(m, hueco, CuartoPared), suelo = parteCuarto(m, hueco, CuartoSuelo);
    const Bricolaje bri = bricolaje(m, hueco);
    const bool haciendo = bri.haciendo() && bri.receta != 0xFFFF;
    struct Ficha {
        SDL_Texture *cara, *pared, *suelo, *receta;
        std::string nombre, personalidad, amistad, conQuien, pared_t, suelo_t, bricolaje;
        int nivel;
        bool conoce, muda;
    };
    const Ficha fi{retratos::de(interno), dibujos::de(pared), dibujos::de(suelo), haciendo ? dibujoReceta(bri.receta) : dibujos::menu("DIYWoodNormal"),
        n, p < 8 ? PERSONALIDADES[p] : "", amistadTexto, conQuien, sinPrefijo(nombreParte(pared)), sinPrefijo(nombreParte(suelo)),
        haciendo ? nombreReceta(bri.receta) + ", hasta las " + dosCifras(bri.hora) + ":" + dosCifras(bri.minuto) : "No está haciendo nada",
        nivel, r >= 0, muda};
    v.dibujo = [fi] {
        panel({60, FICHA_Y, 1160, FICHA_H}, "");
        // On the left, the face big and the name.
        if (fi.cara)
        {
            const SDL_Rect d = {73, FICHA_Y + 10, 130, 130};
            SDL_RenderCopy(g_r, fi.cara, nullptr, &d);
        }
        texto(encajar(fi.nombre, 150, g_negrita), 138, FICHA_Y + 160, MARRON, g_negrita, Centro);
        if (fi.muda)
            texto("Se va a mudar", 138, FICHA_Y + 190, ROJO, g_pequena, Centro);
        // The labels, as the Mac.
        const char* const ETIQUETAS[4] = {"Personalidad", "Amistad", "Paredes y suelo", "Bricolaje"};
        for (int k = 0; k < 4; k++)
            texto(ETIQUETAS[k], ETIQUETA_X, filaY(k) + FILA_H / 2 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
        texto("Muletilla", 598, filaY(0) + FILA_H / 2 - TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena);
        const int ch = TTF_FontHeight(g_pequena) / 2;
        // Friendship: the hearts, the level and with whom.
        int y = filaY(1) + FILA_H / 2;
        int x = VALOR_X;
        if (fi.conoce)
        {
            filaCorazones(x, y - 13, 26, 29, fi.nivel);
            x += 7 * 29 + 8;
        }
        texto(fi.amistad, x, y - TTF_FontHeight(g_negrita) / 2, MARRON, g_negrita);
        x += anchoTexto(fi.amistad, g_negrita) + 10;
        if (!fi.conQuien.empty())
            if (anchoTexto(fi.conQuien, g_pequena) <= FIN_X - EDITAR_W - 10 - x)
                texto(fi.conQuien, x, y - ch, MARRON_SUAVE, g_pequena);
        // Walls and floor, each with its picture.
        y = filaY(2) + FILA_H / 2;
        auto conDibujo = [&](SDL_Texture* t, const std::string& s, int x0, int ancho) {
            if (t)
            {
                const SDL_Rect d = {x0, y - 18, 36, 36};
                SDL_RenderCopy(g_r, t, nullptr, &d);
            }
            texto(encajar(s, ancho - 46, g_pequena), x0 + 42, y - ch, MARRON, g_pequena);
        };
        // The room each one needs (the rest, shared), so both names fit if they can.
        const int libre = FIN_X - EDITAR_W - 10 - VALOR_X;
        const int a1 = anchoTexto(fi.pared_t, g_pequena) + 50, a2 = anchoTexto(fi.suelo_t, g_pequena) + 50;
        const int primero = a1 + a2 <= libre ? a1 + std::min(24, libre - a1 - a2) : std::max(libre / 3, libre * a1 / (a1 + a2));
        conDibujo(fi.pared, fi.pared_t, VALOR_X, primero);
        conDibujo(fi.suelo, fi.suelo_t, VALOR_X + primero, libre - primero);
        // DIY.
        y = filaY(3) + FILA_H / 2;
        conDibujo(fi.receta, fi.bricolaje, VALOR_X, FIN_X - EDITAR_W - 10 - VALOR_X);
    };

    auto boton = [&](SDL_Rect rr, const std::string& t, std::function<void()> f, bool activo = true, bool marcado = false) {
        Boton b{rr, t, std::move(f), activo, marcado};
        v.b.push_back(b);
        return (int)v.b.size() - 1;
    };
    // Row 1: personality and catchphrase.
    boton({VALOR_X, filaY(0), 200, FILA_H}, p < 8 ? PERSONALIDADES[p] : "?", [hueco, p] {
        elegir("Personalidad de " + nombre(hueco), "Cambia cómo habla y cómo se porta.", std::vector<std::string>(PERSONALIDADES, PERSONALIDADES + 8), p, [hueco](int k) {
            ponerPersonalidad(partida::principal(), hueco, k);
            cambio(std::string("Ahora es ") + PERSONALIDADES[k]);
        });
    });
    boton({690, filaY(0), 180, FILA_H}, fr.empty() ? original : fr, [hueco, fr] {
        std::string t = fr;
        if (pedirTexto("Muletilla de " + nombre(hueco), "Lo que dice al final de las frases", t, LARGO_FRASE))
        {
            ponerFrase(partida::principal(), hueco, t);
            cambio("Ahora dice «" + t + "»");
        }
    });
    boton({878, filaY(0), FIN_X - 878, FILA_H}, "Original", [hueco, original] {
        ponerFrase(partida::principal(), hueco, original);
        cambio("Vuelve a decir «" + original + "»");
    }, !original.empty() && fr != original);
    // Rows 2 to 4: friendship, walls and floor, DIY; each with «Editar».
    boton({FIN_X - EDITAR_W, filaY(1), EDITAR_W, FILA_H}, "Editar", [hueco] { amistad(hueco); });
    boton({FIN_X - EDITAR_W, filaY(2), EDITAR_W, FILA_H}, "Editar", [hueco] { cuarto(hueco); });
    boton({FIN_X - EDITAR_W, filaY(3), EDITAR_W, FILA_H}, "Editar", [hueco] { abrir([hueco](Vista& vi) { pantallaBricolaje(vi, hueco); }); });
    // On the right: what the classic editor has, and moving out.
    boton({DERECHA_X, filaY(0), DERECHA_W, FILA_H}, "Recuerdos", [hueco] { recuerdos(hueco); });
    boton({DERECHA_X, filaY(1), DERECHA_W, FILA_H}, "Marcas", [hueco] { marcas(hueco); });
    boton({DERECHA_X, filaY(2), DERECHA_W, FILA_H}, "Su diseño", [hueco] { diseno(hueco); });
    boton({DERECHA_X, filaY(3), DERECHA_W, FILA_H}, "Se va a mudar", [hueco, muda] {
        ponerMudanza(partida::principal(), hueco, !muda);
        cambio(!muda ? nombre(hueco) + " se va a mudar: hará las maletas" : nombre(hueco) + " se queda");
    }, true, muda);

    // Below, as the buttons of the Mac: change it for another one (blue), clothes, furniture, house, files.
    const int y = FICHA_Y + FICHA_H + 12, h = 52;
    Boton cambiar{{60, y, 290, h}, "Cambiar por otro vecino…", [hueco] { abrir(elegirVecino(hueco)); }, !datosVecinos().empty()};
    cambiar.principal = true;
    v.b.push_back(cambiar);
    boton({362, y, 124, h}, "Ropa", [hueco] { abrir(objetos::todos(guardado(hueco, 0), 8)); });
    boton({496, y, 140, h}, "Muebles", [hueco] { abrir(objetos::todos(guardado(hueco, 1), 8)); });
    boton({646, y, 116, h}, "Casa", [hueco] { casa(hueco); }, casaDe(m, hueco) >= 0);
    boton({772, y, 144, h}, "Exportar…", [hueco] { exportar(hueco); });
    boton({926, y, 144, h}, "Importar…", [hueco] { importar(hueco); });
    boton({1080, y, 140, h}, "Volver", [] { volver(); });
}

int cuantos()
{
    int n = 0;
    for (int i = 0; i < NUM_VECINOS; i++)
        n += !vecinoEn(partida::principal(), i).empty();
    return n;
}

Registro r1({SeccionVecinos, 10, [] { return std::string("Tus vecinos"); },
    [] { return std::to_string(cuantos()) + " viven en la isla"; },
    [] { abrir(pagina); }});

Registro r2({SeccionVecinos, 20, [] { return std::string("Mejores amigos de todos"); },
    [] { return std::string("Amistad al máximo con todos"); },
    [] {
        const int n = mejoresAmigos(partida::principal());
        if (n > 0)
            partida::cambiaPrincipal();
        avisar(n > 0 ? "Amistad al máximo con todos (" + std::to_string(n) + "). Falta guardar." : "Ya eran mejores amigos de todos.");
    }});

Registro r3({SeccionVecinos, 30, [] { return std::string("Amistad"); },
    [] { return "Cada vecino con " + partida::jugador().nombre; },
    [] { amistadDeJugador(partida::yo); }});

Registro r4({SeccionVecinos, 40, [] { return std::string("Que se muden todos"); },
    [] {
        int n = 0;
        for (int i = 0; i < NUM_VECINOS; i++)
            n += !vecinoEn(partida::principal(), i).empty() && seMuda(partida::principal(), i);
        return n ? std::to_string(n) + " se van a mudar" : std::string("Hacen las maletas y se van");
    },
    [] {
        confirmar("Que se muden todos", "Todos tus vecinos harán las maletas y se irán de la isla. ¿Seguro?", "Sí, que se muden", [] {
            const int n = mudarTodos(partida::principal());
            partida::cambiaPrincipal();
            avisar("Hecho: " + std::to_string(n) + " vecinos se van a mudar. Guarda para que pase en el juego.");
        });
    }});

Registro r5({SeccionVecinos, 50, [] { return std::string("Guardar todos en la SD"); },
    [] { return std::string("Copia de cada uno en la tarjeta"); },
    [] {
        int n = 0, mal = 0;
        for (int i = 0; i < NUM_VECINOS; i++)
            if (!vecinoEn(partida::principal(), i).empty())
                (partida::escribirSd(std::string(CARPETA) + archivo(nombre(i), "nhv2"), exportarVecino(partida::principal(), i)) ? n : mal)++;
        avisar(mal ? "No he podido guardar " + std::to_string(mal) + " vecinos en la tarjeta SD." : "Se han guardado " + std::to_string(n) + " vecinos en /switch/NHSE/vecinos.", mal > 0);
    }});

} // namespace
