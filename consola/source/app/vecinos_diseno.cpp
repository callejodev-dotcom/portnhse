// El diseño profesional que lleva un vecino (en la ropa o el paraguas), como el editor de diseños PRO que abre NHSE
// para Mac desde el vecino: verlo dibujado, ponerle uno de los diseños PRO de la partida o uno de la tarjeta SD (.nhpd,
// como el Mac y la sección «Diseños») y guardarlo en la SD, como archivo o como imagen. Usa el núcleo de los diseños
// (core/disenos.hpp).
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include "../core/disenos.hpp"
#include "../core/vecino.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
const std::string CARPETA_DISENOS = std::string(partida::CARPETA) + "disenos/";
const std::string CARPETA_PRO = CARPETA_DISENOS + "Diseños PRO/"; // la de «guardar todos» de la sección Diseños

void cambio(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

// Un dibujo de diseño PRO (64x64, las 4 hojas) como textura; se borra con el dueño.
struct Dibujo {
    std::vector<uint8_t> d;
    SDL_Texture* tex = nullptr;
    ~Dibujo()
    {
        if (tex && ui::g_r) // al salir, SDL ya se ha cerrado y se llevó las texturas
            SDL_DestroyTexture(tex);
    }
    // El dibujo de «nuevo», rehecho solo si ha cambiado.
    SDL_Texture* de(const std::vector<uint8_t>& nuevo)
    {
        if (tex && nuevo == d)
            return tex;
        if (tex)
            SDL_DestroyTexture(tex);
        d = nuevo;
        int w, h;
        const auto px = disenos::rgba(d, disenos::Pro, 1, w, h);
        tex = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (tex)
        {
            SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        return tex;
    }
};

// Cuadros claros y oscuros debajo, para que se vea lo transparente.
void cuadros(const SDL_Rect& r, int lado)
{
    rect(r, BLANCO);
    for (int y = 0; y < r.h; y += lado)
        for (int x = (y / lado) % 2 ? lado : 0; x < r.w; x += 2 * lado)
            rect({r.x + x, r.y + y, std::min(lado, r.w - x), std::min(lado, r.h - y)}, CREMA_OSCURA);
}

std::string nombreDiseno(const std::vector<uint8_t>& d)
{
    const std::string n = disenos::nombre(d);
    return n.empty() ? "(sin nombre)" : n;
}

bool existe(const std::string& ruta)
{
    struct stat st;
    return stat(ruta.c_str(), &st) == 0;
}

// Una ruta libre: «nombre.nhpd», si ya está «nombre (2).nhpd»...
std::string libre(const std::string& archivo)
{
    const size_t punto = archivo.rfind('.');
    const std::string base = archivo.substr(0, punto), ext = archivo.substr(punto);
    std::string r = CARPETA_DISENOS + archivo;
    for (int n = 2; existe(r); n++)
        r = CARPETA_DISENOS + base + " (" + std::to_string(n) + ")" + ext;
    return r;
}

std::string corta(const std::string& ruta) { return ruta.substr(strlen("sdmc:")); }

// ------------------------------------------------------------------------------------------- poner uno
void ponerDiseno(int hueco, std::vector<uint8_t> d, bool hacerSuyo)
{
    // Como «Importar» de PatternEditorPRO: hacerlo del jugador si se quiere, y el uso de un diseño PRO.
    if (hacerSuyo && !disenos::esDe(d, partida::personal()))
        disenos::hacerMio(d, partida::personal());
    disenos::arreglarUso(d, disenos::Pro);
    ponerDisenoVecino(partida::principal(), hueco, d);
    cambio("Ahora lleva «" + nombreDiseno(d) + "»");
}

void deLaPartida(int hueco)
{
    struct Estado {
        std::vector<std::unique_ptr<Dibujo>> dibujos;
        std::vector<std::vector<uint8_t>> d;
    };
    auto e = std::make_shared<Estado>();
    const int n = disenos::cuantos(disenos::Pro);
    for (int i = 0; i < n; i++)
    {
        e->d.push_back(disenos::leer(partida::principal(), disenos::Pro, i));
        e->dibujos.push_back(std::make_unique<Dibujo>());
    }
    Lista l;
    l.titulo = "Diseños PRO de la partida";
    l.explicacion = [hueco] { return "Elige el diseño que llevará " + nombre(hueco) + ". L y R: otra página."; };
    l.cuantas = [n] { return n; };
    l.porColumna = 6;
    l.columnas = 3;
    l.fila = [e, hueco](int i) {
        const bool vacio = disenos::vacio(e->d[i], disenos::Pro);
        Fila f{vacio ? "Vacío" : nombreDiseno(e->d[i]), [e, hueco, i] {
            volver();
            ponerDisenoVecino(partida::principal(), hueco, e->d[i]);
            cambio("Ahora lleva «" + nombreDiseno(e->d[i]) + "»");
        }, !vacio};
        f.detalle = "Nº " + std::to_string(i + 1);
        if (!vacio)
            f.imagen = e->dibujos[i]->de(e->d[i]);
        return f;
    };
    abrir(lista(l));
}

void deLaSd(int hueco)
{
    std::vector<std::string> rutas;
    for (const auto& carpeta : {CARPETA_DISENOS, CARPETA_PRO})
        for (const auto& n : partida::listarSd(carpeta, ".nhpd"))
            rutas.push_back(carpeta + n);
    if (rutas.empty())
    {
        avisar("No hay diseños PRO (.nhpd) en la carpeta /switch/NHSE/disenos de la tarjeta SD.", true);
        return;
    }
    std::vector<Opcion> op;
    for (const auto& r : rutas)
    {
        const std::string n = r.substr(r.rfind('/') + 1);
        op.push_back({n, normalizar(n)});
    }
    elegirLargo("¿Qué diseño lleva " + nombre(hueco) + "?", "Los diseños PRO de /switch/NHSE/disenos de la tarjeta SD.", op, -1, [hueco, rutas](int k) {
        std::vector<uint8_t> d;
        if (!partida::leerSd(rutas[k], d) || d.size() != TAM_DISENO_VECINO)
        {
            avisar("Ese archivo no es un diseño profesional (.nhpd).", true);
            return;
        }
        if (disenos::esDe(d, partida::personal()))
        {
            ponerDiseno(hueco, d, false);
            return;
        }
        // Como el Mac: ¿que pase a ser tuyo?
        const std::string quien = disenos::autor(d) + " (" + disenos::pueblo(d) + ")";
        abrir([hueco, d, quien](Vista& vi) {
            vi.titulo = "Lo hizo otro jugador";
            vi.explicacion = "Este diseño lo hizo " + quien + ". ¿Quieres que pase a ser de " + partida::jugador().nombre + "? Es lo normal.";
            vi.b.push_back({{140, 300, 480, 100}, "Sí, que sea mío", [hueco, d] {
                volver();
                ponerDiseno(hueco, d, true);
            }});
            vi.b.push_back({{660, 300, 480, 100}, "No, dejarlo como está", [hueco, d] {
                volver();
                ponerDiseno(hueco, d, false);
            }});
            vi.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Cancelar", [] { volver(); }});
        });
    });
}

// ------------------------------------------------------------------------------------------- guardar
void guardar(int hueco, bool imagen)
{
    const auto d = disenoVecino(partida::principal(), hueco);
    std::string archivo = disenos::nombreArchivo(d, disenos::Pro);
    if (imagen)
        archivo = archivo.substr(0, archivo.rfind('.')) + ".png";
    const std::string ruta = libre(archivo);
    if (partida::escribirSd(ruta, imagen ? disenos::png(d, disenos::Pro) : d))
        avisar((imagen ? "Imagen guardada en la SD: " : "Guardado en la SD: ") + corta(ruta));
    else
        avisar("No he podido escribir en la SD.", true);
}
} // namespace

std::string resumenDiseno(int hueco)
{
    const std::string n = disenos::nombre(disenoVecino(partida::principal(), hueco));
    return n.empty() ? "Sin nombre" : "«" + n + "»";
}

void diseno(int hueco)
{
    auto dibujo = std::make_shared<Dibujo>();
    abrir([hueco, dibujo](Vista& v) {
        const auto d = disenoVecino(partida::principal(), hueco);
        v.titulo = "Diseño de " + nombre(hueco);
        v.explicacion = "El diseño profesional que lleva " + nombre(hueco) + " (en la ropa o el paraguas).";
        SDL_Texture* tex = dibujo->de(d);
        const std::string n = nombreDiseno(d), autor = disenos::autor(d), isla = disenos::pueblo(d);
        // Sin nombre ni autor: el juego no le ha dado ninguno (todo a cero).
        const bool ninguno = disenos::nombre(d).empty() && autor.empty();
        constexpr int X = 84, Y = 132, LADO = 360, TX = 484;
        v.dibujo = [tex, d, n, autor, isla, ninguno] {
            panel({60, 112, 1160, 476}, "");
            cuadros({X, Y, LADO, LADO}, 24);
            if (ninguno)
            {
                texto("No lleva ninguno", X + LADO / 2, Y + LADO / 2, MARRON_SUAVE, nullptr, Centro);
                texto("No lleva ningún diseño", TX, 140, MARRON, g_titulo);
                texto("Ponle uno de los tuyos o uno de la SD.", TX, 196, MARRON_SUAVE);
                return;
            }
            if (tex)
            {
                const SDL_Rect r = {X, Y, LADO, LADO};
                SDL_RenderCopy(g_r, tex, nullptr, &r);
            }
            // Las 4 hojas: delante, detrás, manga derecha, manga izquierda.
            rect({X + LADO / 2 - 1, Y, 2, LADO}, CREMA_OSCURA);
            rect({X, Y + LADO / 2 - 1, LADO, 2}, CREMA_OSCURA);
            for (int c = 0; c < disenos::COLORES; c++)
            {
                uint8_t r, g, b;
                disenos::color(d, c, r, g, b);
                const SDL_Rect q = {X + c * 26, Y + LADO + 16, 22, 22};
                redondo({q.x - 2, q.y - 2, q.w + 4, q.h + 4}, 13, GRIS);
                redondo(q, 11, {r, g, b, 255});
            }
            texto(encajar(n, 1196 - TX, g_titulo), TX, 140, MARRON, g_titulo);
            texto(encajar("Hecho por " + autor + " (" + isla + ")", 1196 - TX), TX, 196, MARRON_SUAVE);
        };
        v.b.push_back({{TX, 264, 346, 96}, "Uno de la partida", [hueco] { deLaPartida(hueco); }});
        v.b.push_back({{TX + 366, 264, 346, 96}, "Uno de la SD", [hueco] { deLaSd(hueco); }});
        v.b.push_back({{TX, 376, 346, 96}, "Guardar en la SD", [hueco] { guardar(hueco, false); }});
        v.b.push_back({{TX + 366, 376, 346, 96}, "Guardar imagen", [hueco] { guardar(hueco, true); }});
        for (auto& b : v.b)
            b.grande = true;
        v.b[0].detalle = "Uno de tus diseños PRO";
        v.b[1].detalle = "Un archivo .nhpd";
        v.b[2].detalle = "Como archivo .nhpd";
        v.b[3].detalle = "Como imagen PNG";
        v.b.push_back({{960, ABAJO - 60, 260, 52}, "Volver", [] { volver(); }});
    });
}

} // namespace vecinos
