// Sección «Diseños»: los diseños normales, los profesionales, el de la bandera y los de la sastrería, como la ventana de
// diseños de la app del Mac. Se ven dibujados con sus colores; se les cambia el nombre, se hacen tuyos, se guardan en la
// SD (.nhd y .nhpd, los mismos archivos que el Mac, y como imagen PNG) y se traen de la SD, uno o una carpeta entera.
// Además: copiar a otro hueco y vaciar un hueco.
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include "../core/acnh.hpp"
#include "../core/disenos.hpp"
#include "dibujos.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace disenos;
using namespace ui;

namespace {

const char* const TITULOS[NUM_TIPOS] = {"Mis diseños", "Diseños PRO", "Bandera de la isla", "Hermanas Manitas"};
const char* const CARPETAS[NUM_TIPOS] = {"Mis diseños", "Diseños PRO", "Bandera", "Hermanas Manitas"}; // «guardar todos»

std::string carpetaSd() { return std::string(partida::CARPETA) + "disenos/"; }
std::string corta(const std::string& ruta) // «disenos/...» para los avisos
{
    const std::string base = partida::CARPETA;
    return ruta.compare(0, base.size(), base) == 0 ? ruta.substr(base.size()) : ruta;
}

// ----------------------------------------------------------------------------------------------- lo leído y dibujado
// Los diseños leídos de la partida y sus dibujos, para no leerlos ni dibujarlos en cada fotograma. Se vuelven a leer
// cuando cambia algo (poner) o cuando la partida es otra (tras guardar, los datos se mueven).
struct Hueco {
    std::vector<uint8_t> d;
    bool vacio = false;
    SDL_Texture* tex = nullptr;
};
struct Cache {
    const uint8_t* datos = nullptr;
    uint32_t version = 0;
    bool leido[NUM_TIPOS] = {};
    std::vector<Hueco> h[NUM_TIPOS];
};
Cache g_cache;
uint32_t g_version = 1;

void olvidar()
{
    for (auto& v : g_cache.h)
    {
        for (auto& h : v)
            if (h.tex)
                SDL_DestroyTexture(h.tex);
        v.clear();
    }
    std::fill(std::begin(g_cache.leido), std::end(g_cache.leido), false);
}

std::vector<Hueco>& huecos(Tipo t)
{
    if (g_cache.datos != partida::principal().data() || g_cache.version != g_version)
    {
        olvidar();
        g_cache.datos = partida::principal().data();
        g_cache.version = g_version;
    }
    if (!g_cache.leido[t])
    {
        auto& v = g_cache.h[t];
        v.resize(cuantos(t));
        for (int i = 0; i < cuantos(t); i++)
        {
            v[i].d = leer(partida::principal(), t, i);
            v[i].vacio = vacio(v[i].d, t);
        }
        g_cache.leido[t] = true;
    }
    return g_cache.h[t];
}
const Hueco& hueco(Tipo t, int i) { return huecos(t)[i]; }

SDL_Texture* crearTextura(const std::vector<uint8_t>& d, Tipo t)
{
    int w, h;
    const auto px = rgba(d, t, 1, w, h);
    SDL_Texture* tex = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (tex)
    {
        SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    return tex;
}

SDL_Texture* textura(Tipo t, int i)
{
    Hueco& h = huecos(t)[i];
    if (!h.tex)
        h.tex = crearTextura(h.d, t);
    return h.tex;
}

// Cambia un diseño de la partida.
void poner(Tipo t, int i, const std::vector<uint8_t>& d)
{
    escribir(partida::principal(), t, i, d);
    partida::cambiaPrincipal();
    g_version++;
}

int usados(Tipo t)
{
    int n = 0;
    for (const auto& h : huecos(t))
        n += !h.vacio;
    return n;
}

std::string nombreVisible(const Hueco& h)
{
    if (h.vacio)
        return "Vacío";
    const std::string n = nombre(h.d);
    return n.empty() ? "(sin nombre)" : n;
}

bool deOtro(const std::vector<uint8_t>& d) { return !deJuego(d) && !esDe(d, partida::personal()); }

std::string quienLoHizo(const std::vector<uint8_t>& d)
{
    return deJuego(d) ? "Un diseño de ejemplo que trae el juego." : "Hecho por " + autor(d) + " en " + pueblo(d) + ".";
}

// El hueco sin usar del juego, para vaciar otro: el primero vacío de los normales (o de los PRO para los PRO).
bool blancoDe(Tipo t, std::vector<uint8_t>& d)
{
    const Tipo de = esPro(t) ? Pro : Normales;
    for (const auto& h : huecos(de))
        if (h.vacio)
        {
            d = h.d;
            return true;
        }
    return false;
}

// ----------------------------------------------------------------------------------------------------------- dibujo
// Cuadros claros y oscuros debajo del dibujo, para que se vea lo transparente.
void cuadros(const SDL_Rect& r, int lado)
{
    rect(r, BLANCO);
    SDL_SetRenderDrawColor(g_r, CREMA_OSCURA.r, CREMA_OSCURA.g, CREMA_OSCURA.b, 255);
    for (int y = 0; y < r.h; y += lado)
        for (int x = (y / lado) % 2 ? lado : 0; x < r.w; x += 2 * lado)
        {
            SDL_Rect c = {r.x + x, r.y + y, std::min(lado, r.w - x), std::min(lado, r.h - y)};
            SDL_RenderFillRect(g_r, &c);
        }
}

void dibujo(SDL_Texture* tex, const SDL_Rect& r, int lado = 16)
{
    cuadros(r, lado);
    if (tex)
        SDL_RenderCopy(g_r, tex, nullptr, &r);
}

// Los 15 colores en fila.
void paleta(const std::vector<uint8_t>& d, int x, int y, int lado = 30)
{
    for (int c = 0; c < COLORES; c++)
    {
        uint8_t r, g, b;
        color(d, c, r, g, b);
        const SDL_Rect q = {x + c * (lado + 6), y, lado, lado};
        redondo({q.x - 2, q.y - 2, q.w + 4, q.h + 4}, lado / 2 + 2, GRIS);
        redondo(q, lado / 2, {r, g, b, 255});
    }
}

// ------------------------------------------------------------------------------------------------------ archivos SD
bool esCarpeta(const std::string& ruta, dirent* e)
{
#ifdef DT_DIR
    if (e->d_type == DT_DIR)
        return true;
    if (e->d_type != DT_UNKNOWN)
        return false;
#endif
    struct stat st;
    return stat(ruta.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string minusculas(std::string s)
{
    for (auto& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

bool acabaEn(const std::string& n, const std::string& ext)
{
    return n.size() > ext.size() && minusculas(n.substr(n.size() - ext.size())) == ext;
}

// Los archivos de una carpeta con esa terminación, por orden (sin mirar mayúsculas, como el Mac).
std::vector<std::string> archivosDe(const std::string& carpeta, const std::string& ext)
{
    std::vector<std::string> r;
    if (DIR* dir = opendir(carpeta.c_str()))
    {
        while (dirent* e = readdir(dir))
        {
            const std::string n = e->d_name;
            if (n[0] != '.' && acabaEn(n, ext) && !esCarpeta(carpeta + n, e))
                r.push_back(n);
        }
        closedir(dir);
    }
    std::sort(r.begin(), r.end(), [](const std::string& a, const std::string& b) { return minusculas(a) < minusculas(b); });
    return r;
}

// La carpeta de diseños de la SD y las que tenga dentro («» es la de diseños).
std::vector<std::string> subcarpetas()
{
    std::vector<std::string> r = {""};
    if (DIR* dir = opendir(carpetaSd().c_str()))
    {
        while (dirent* e = readdir(dir))
        {
            const std::string n = e->d_name;
            if (n[0] != '.' && esCarpeta(carpetaSd() + n, e))
                r.push_back(n + "/");
        }
        closedir(dir);
    }
    std::sort(r.begin() + 1, r.end(), [](const std::string& a, const std::string& b) { return minusculas(a) < minusculas(b); });
    return r;
}

bool existe(const std::string& ruta)
{
    struct stat st;
    return stat(ruta.c_str(), &st) == 0;
}

// Una ruta libre: «nombre.nhd», si ya está «nombre (2).nhd»...
std::string libre(const std::string& carpeta, const std::string& archivo)
{
    const size_t punto = archivo.rfind('.');
    const std::string base = archivo.substr(0, punto), ext = archivo.substr(punto);
    std::string r = carpeta + archivo;
    for (int n = 2; existe(r); n++)
        r = carpeta + base + " (" + std::to_string(n) + ")" + ext;
    return r;
}

void guardarArchivo(Tipo t, int i)
{
    const auto& d = hueco(t, i).d;
    const std::string ruta = libre(carpetaSd(), nombreArchivo(d, t));
    if (partida::escribirSd(ruta, d))
        avisar("Guardado en la SD: " + corta(ruta));
    else
        avisar("No he podido escribir en la SD.", true);
}

void guardarImagen(Tipo t, int i)
{
    const auto& d = hueco(t, i).d;
    std::string archivo = nombreArchivo(d, t);
    archivo = archivo.substr(0, archivo.rfind('.')) + ".png";
    const std::string ruta = libre(carpetaSd(), archivo);
    if (partida::escribirSd(ruta, png(d, t)))
        avisar("Imagen guardada en la SD: " + corta(ruta));
    else
        avisar("No he podido escribir en la SD.", true);
}

void guardarTodos(Tipo t)
{
    const std::string carpeta = carpetaSd() + CARPETAS[t] + "/";
    int n = 0;
    for (int i = 0; i < cuantos(t); i++)
    {
        const Hueco& h = hueco(t, i);
        if (h.vacio)
            continue;
        if (!partida::escribirSd(carpeta + nombreArchivo(h.d, t, i + 1), h.d))
        {
            avisar("No he podido escribir en la SD.", true);
            return;
        }
        n++;
    }
    avisar("Se han guardado " + std::to_string(n) + " diseños en la SD: " + corta(carpeta.substr(0, carpeta.size() - 1)));
}

// ----------------------------------------------------------------------------------------------- traer de la SD
// Diseños traídos de la SD, antes de ponerlos, con sus dibujos (se borran con la pantalla que los usa).
struct Traidos {
    std::vector<std::vector<uint8_t>> ds;
    std::vector<SDL_Texture*> tex;
    std::string de; // the file or folder
    ~Traidos()
    {
        for (auto* t : tex)
            if (t)
                SDL_DestroyTexture(t);
    }
};

// Pone los diseños traídos en el hueco «i» y los siguientes (como Import del Mac), tras preguntar si se hacen tuyos.
Pantalla traer(Tipo t, int i, std::shared_ptr<Traidos> e, int volverTras)
{
    e->tex.resize(e->ds.size(), nullptr);
    return [t, i, e, volverTras](Vista& v) {
        const int n = (int)e->ds.size();
        int otros = 0;
        for (const auto& d : e->ds)
            otros += deOtro(d);
        auto hacer = [t, i, e, volverTras, n](bool mio) {
            for (int k = 0; k < n; k++)
            {
                auto d = e->ds[k];
                if (mio && deOtro(d))
                    hacerMio(d, partida::personal());
                arreglarUso(d, t);
                poner(t, i + k, d);
            }
            for (int k = 0; k < volverTras; k++)
                volver();
            cambiado(n == 1 ? "Diseño traído" : "Se han traído " + std::to_string(n) + " diseños");
        };
        v.titulo = n == 1 ? "Traer un diseño" : "Traer " + std::to_string(n) + " diseños";
        const std::string donde = cuantos(t) == 1 ? "" : n == 1 ? " Va al hueco " + std::to_string(i + 1) + "." :
            " Van a los huecos " + std::to_string(i + 1) + " a " + std::to_string(i + n) + ".";
        if (n == 1)
        {
            const auto& d = e->ds[0];
            v.explicacion = "«" + (nombre(d).empty() ? std::string("(sin nombre)") : nombre(d)) + "», de " + corta(e->de) + "." + donde;
            if (otros)
                v.explicacion += " Este diseño lo hizo " + autor(d) + ". ¿Quieres que pase a ser tuyo? Así podrás retocarlo en el juego.";
        }
        else
        {
            v.explicacion = "De " + corta(e->de) + "." + donde;
            if (otros)
                v.explicacion += " Algunos los hizo otra persona. ¿Quieres que pasen a ser tuyos? Así podrás retocarlos en el juego.";
        }
        v.dibujo = [t, e, n] {
            for (int k = 0; k < n; k++)
                if (!e->tex[k])
                    e->tex[k] = crearTextura(e->ds[k], t);
            if (n == 1)
            {
                dibujo(e->tex[0], {60, 190, 384, 384}, 24);
                paleta(e->ds[0], 480, 200);
                texto(quienLoHizo(e->ds[0]), 480, 250, MARRON, g_pequena, Izquierda, 740);
                return;
            }
            // Up to 20 small pictures.
            for (int k = 0; k < n && k < 20; k++)
                dibujo(e->tex[k], {60 + (k % 10) * 116, 200 + (k / 10) * 116, 100, 100}, 10);
            if (n > 20)
                texto("… y " + std::to_string(n - 20) + " más", 60, 440, MARRON_SUAVE, g_pequena);
        };
        const int x = n == 1 ? 480 : 140, ancho = n == 1 ? 740 : 1000;
        const int y = n == 1 ? 330 : 470;
        if (otros)
        {
            v.b.push_back({{x, y, ancho / 2 - 10, 80}, n == 1 ? "Sí, hacerlo mío" : "Sí, hacerlos míos", [hacer] { hacer(true); }});
            v.b.push_back({{x + ancho / 2 + 10, y, ancho / 2 - 10, 80}, "No, dejarlo como está", [hacer] { hacer(false); }});
        }
        else
            v.b.push_back({{x, y, ancho, 80}, n == 1 ? "Ponerlo" : "Ponerlos", [hacer] { hacer(false); }});
        v.b.push_back({{x, y + 100, ancho, 70}, "Cancelar", [] { volver(); }});
    };
}

// Elegir un archivo de la SD para el hueco «i».
Pantalla elegirArchivo(Tipo t, int i)
{
    struct Estado {
        std::vector<std::string> rutas;
    };
    auto e = std::make_shared<Estado>();
    for (const auto& c : subcarpetas())
        for (const auto& a : archivosDe(carpetaSd() + c, extension(t)))
            e->rutas.push_back(c + a);
    Lista l;
    l.titulo = "Elige un archivo";
    l.explicacion = [t, e] {
        return e->rutas.empty() ? std::string("No hay diseños ") + extension(t) + " en la SD. Cópialos a la carpeta switch/NHSE/disenos (o a una carpeta dentro de ella) desde el ordenador." :
            std::string("Archivos ") + extension(t) + " de la carpeta switch/NHSE/disenos de la SD.";
    };
    l.cuantas = [e] { return (int)e->rutas.size(); };
    l.porColumna = 7;
    l.fila = [t, i, e](int k) {
        const std::string r = e->rutas[k];
        const size_t barra = r.rfind('/');
        Fila f{barra == std::string::npos ? r : r.substr(barra + 1), [t, i, r] {
            std::vector<uint8_t> d;
            if (!partida::leerSd(carpetaSd() + r, d) || (int)d.size() != tamano(t))
            {
                avisar("«" + r + "» no es un archivo " + extension(t) + ".", true);
                return;
            }
            auto tr = std::make_shared<Traidos>();
            tr->ds.push_back(std::move(d));
            tr->de = "disenos/" + r;
            abrir(traer(t, i, tr, 2));
        }};
        if (barra != std::string::npos)
            f.detalle = "En " + r.substr(0, barra);
        return f;
    };
    return lista(l);
}

// Elegir una carpeta de la SD: todos sus archivos van al hueco «i» y los siguientes.
Pantalla elegirCarpeta(Tipo t, int i)
{
    struct Estado {
        std::vector<std::pair<std::string, int>> carpetas; // with how many files
    };
    auto e = std::make_shared<Estado>();
    for (const auto& c : subcarpetas())
    {
        const int n = (int)archivosDe(carpetaSd() + c, extension(t)).size();
        if (n > 0)
            e->carpetas.push_back({c, n});
    }
    Lista l;
    l.titulo = "Cargar desde una carpeta";
    l.explicacion = [t, i, e] {
        return e->carpetas.empty() ? std::string("No hay diseños ") + extension(t) + " en la SD. Cópialos a la carpeta switch/NHSE/disenos (o a una carpeta dentro de ella) desde el ordenador." :
            "Sus diseños van al hueco " + std::to_string(i + 1) + " y los siguientes.";
    };
    l.cuantas = [e] { return (int)e->carpetas.size(); };
    l.columnas = 1;
    l.porColumna = 6;
    l.fila = [t, i, e](int k) {
        const auto c = e->carpetas[k];
        Fila f{c.first.empty() ? "disenos" : "disenos/" + c.first.substr(0, c.first.size() - 1), [t, i, c] {
            auto tr = std::make_shared<Traidos>();
            for (const auto& a : archivosDe(carpetaSd() + c.first, extension(t)))
            {
                if ((int)tr->ds.size() >= cuantos(t) - i)
                    break;
                std::vector<uint8_t> d;
                if (!partida::leerSd(carpetaSd() + c.first + a, d) || (int)d.size() != tamano(t))
                {
                    avisar("«" + a + "» no es un archivo " + extension(t) + ".", true);
                    return;
                }
                tr->ds.push_back(std::move(d));
            }
            tr->de = "disenos/" + c.first;
            if (!tr->de.empty() && tr->de.back() == '/')
                tr->de.pop_back();
            abrir(traer(t, i, tr, 2));
        }};
        f.detalle = std::to_string(c.second) + (c.second == 1 ? " diseño" : " diseños");
        return f;
    };
    return lista(l);
}

// ------------------------------------------------------------------------------------------------------- rejilla
// Como la ventana de diseños del Mac: a la izquierda los huecos en una cuadrícula de dibujos (5 x 4 por página), cada
// uno con su nombre debajo; a la derecha, en un recuadro, el elegido en grande con su nombre, quién lo hizo, sus colores
// y lo que se puede hacer con él. Con «elegido», es para elegir un hueco (copiar a otro hueco).
constexpr int COLS = 5, FILAS = 4, POR_PAGINA = COLS * FILAS;
constexpr int GX = 60, GY = 140, CW = 136, CH = 130, CAJA = 96;
constexpr int PX = 770, PW = 450, PY = 140, PH = 466; // the panel on the right
constexpr int ABAJO_Y = ABAJO - 56, ABAJO_H = 46;      // the buttons along the bottom

const SDL_Color VERDE_FONDO = {0xD6, 0xEE, 0xDD, 255}; // the chosen one, as the Mac
const SDL_Color LINEA = {0xD1, 0xD1, 0xD6, 255};       // edges
const SDL_Color APAGADO_ = {0xAE, 0xAE, 0xB2, 255};    // «Vacío»

SDL_Rect celda(int k) { return {GX + (k % COLS) * CW, GY + (k / COLS) * CH, CW - 8, CH - 2}; }
SDL_Rect cajaDe(int k)
{
    const SDL_Rect c = celda(k);
    return {c.x + (c.w - CAJA) / 2, c.y + 4, CAJA, CAJA};
}

// Las cosas del recuadro de la derecha (se dibujan en «dibujo»; los botones van encima).
void dibujarElegido(Tipo t, int i, const std::string& encabezado)
{
    panel({PX, PY, PW, PH}, "");
    const Hueco& h = hueco(t, i);
    const SDL_Rect img = {PX + 20, PY + 20, 180, 180};
    const int x2 = img.x + img.w + 22, ancho2 = PX + PW - 20 - x2;
    texto(encajar(encabezado, ancho2, g_pequena), x2, PY + 18, MARRON_SUAVE, g_pequena);
    if (h.vacio)
    {
        redondo(img, 12, LINEA);
        redondo({img.x + 2, img.y + 2, img.w - 4, img.h - 4}, 10, CREMA);
        texto("Vacío", img.x + img.w / 2, img.y + img.h / 2, APAGADO_, nullptr, Centro);
        texto("Hueco vacío", PX + 20, PY + 214, MARRON, g_negrita);
        texto("Trae un diseño de la SD para llenarlo.", PX + 20, PY + 252, MARRON_SUAVE, g_pequena, Izquierda, PW - 40);
        return;
    }
    redondo({img.x - 2, img.y - 2, img.w + 4, img.h + 4}, 6, LINEA);
    dibujo(textura(t, i), img, 15);
    texto("Colores", x2, PY + 52, MARRON, g_pequena);
    for (int c = 0; c < COLORES; c++)
    {
        uint8_t r, g, b;
        color(h.d, c, r, g, b);
        const SDL_Rect q = {x2 + (c % 5) * 36, PY + 86 + (c / 5) * 36, 28, 28};
        redondo({q.x - 2, q.y - 2, q.w + 4, q.h + 4}, 16, LINEA);
        redondo(q, 14, {r, g, b, 255});
    }
    texto(encajar(nombreVisible(h), PW - 40, g_negrita), PX + 20, PY + 212, MARRON, g_negrita);
    std::string quien = quienLoHizo(h.d);
    texto(quien, PX + 20, PY + 250, MARRON_SUAVE, g_pequena, Izquierda, PW - 40);
}

Pantalla ficha(Tipo t, int i);
void masOpciones(Tipo t, int i);

Pantalla rejilla(Tipo t, int inicial, std::function<void(int)> elegido = nullptr, int excepto = -1)
{
    struct Estado {
        int pagina = 0;
        int ultimo = 0; // the slot shown on the right
    };
    auto e = std::make_shared<Estado>();
    e->pagina = inicial / POR_PAGINA;
    e->ultimo = inicial;
    return [t, e, elegido, excepto](Vista& v) {
        const int total = cuantos(t);
        const int paginas = (total + POR_PAGINA - 1) / POR_PAGINA;
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        const int desde = e->pagina * POR_PAGINA, hasta = std::min(total, desde + POR_PAGINA);
        const int enPagina = hasta - desde;
        auto abrirHueco = [t, elegido](int i) {
            if (elegido)
                elegido(i);
            else
                abrir(ficha(t, i));
        };
        if (elegido)
        {
            v.titulo = "Copiar a otro hueco";
            v.explicacion = "Elige el hueco donde poner la copia de «" + nombreVisible(hueco(t, excepto)) + "».";
        }
        else
        {
            v.titulo = TITULOS[t];
            v.explicacion = std::to_string(usados(t)) + " de " + std::to_string(total) + " huecos usados. Elige un diseño para verlo en grande.";
        }
        if (paginas > 1)
            v.explicacion += " (página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ")";
        const int f = foco();
        if (f >= 0 && f < enPagina)
            e->ultimo = desde + f;
        e->ultimo = std::clamp(e->ultimo, 0, total - 1);
        if (e->ultimo < desde || e->ultimo >= hasta)
            e->ultimo = desde;

        // The grid: a square with the picture (or «Vacío»); the name goes below, in «dibujo».
        for (int k = 0; k < enPagina; k++)
        {
            const int i = desde + k;
            const Hueco& h = hueco(t, i);
            Boton b{cajaDe(k), h.vacio ? "Vacío" : nombreVisible(h), [abrirHueco, i] { abrirHueco(i); }, i != excepto};
            b.casilla = true;
            b.marcado = i == e->ultimo && !h.vacio;
            if (h.vacio)
                b.color = CREMA;
            else
                b.imagen = textura(t, i);
            v.b.push_back(b);
        }

        // The panel on the right: what can be done with the chosen one.
        const int i = e->ultimo;
        const Hueco& h = hueco(t, i);
        const bool vacio = h.vacio, otro = !vacio && deOtro(h.d);
        if (!elegido)
        {
            const int y0 = PY + 312, bw = (PW - 40 - 10) / 2, bh = 44;
            auto boton = [&](int fila, int col, const std::string& s, std::function<void()> a, bool activo) {
                Boton b{{PX + 20 + col * (bw + 10), y0 + fila * (bh + 8), bw, bh}, s, std::move(a), activo};
                b.pequena = true;
                v.b.push_back(b);
            };
            boton(0, 0, "Cambiar nombre", [t, i] {
                auto d = hueco(t, i).d;
                std::string n = nombre(d);
                if (!pedirTexto("Nombre del diseño", "Hasta 20 letras", n, 20))
                    return;
                ponerNombre(d, n);
                poner(t, i, d);
                cambiado("Nombre cambiado");
            }, !vacio);
            boton(0, 1, "Hacerlo mío", [t, i] {
                auto d = hueco(t, i).d;
                hacerMio(d, partida::personal());
                poner(t, i, d);
                cambiado("Ahora es tuyo");
            }, otro);
            boton(1, 0, "Guardar imagen", [t, i] { guardarImagen(t, i); }, !vacio);
            boton(1, 1, "Guardar en la SD", [t, i] { guardarArchivo(t, i); }, !vacio);
            boton(2, 0, "Traer de la SD…", [t, i] { abrir(elegirArchivo(t, i)); }, true);
            boton(2, 1, "Más…", [t, i] { masOpciones(t, i); }, true);
        }

        // Along the bottom: pages and «Volver».
        if (paginas > 1)
        {
            Boton ant{{PX, ABAJO_Y, 155, ABAJO_H}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0};
            Boton sig{{PX + 165, ABAJO_Y, 155, ABAJO_H}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas};
            ant.pequena = sig.pequena = true;
            v.b.push_back(ant);
            v.b.push_back(sig);
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        }
        Boton vb{{PX + PW - 120, ABAJO_Y, 120, ABAJO_H}, elegido ? "Cancelar" : "Volver", [] { volver(); }};
        vb.pequena = true;
        v.b.push_back(vb);

        const std::string pie = "Hueco " + std::to_string(i + 1) + " de " + std::to_string(total);
        const bool copiando = elegido != nullptr;
        v.dibujo = [t, e, desde, enPagina, pie, copiando] {
            for (int k = 0; k < enPagina; k++)
            {
                const SDL_Rect c = celda(k);
                const int i = desde + k;
                const Hueco& h = hueco(t, i);
                if (i == e->ultimo)
                    redondo(c, 14, VERDE_FONDO);
                if (!h.vacio)
                    texto(encajar(nombreVisible(h), c.w - 6, g_pequena), c.x + c.w / 2, c.y + CAJA + 6 + TTF_FontHeight(g_pequena) / 2, MARRON, g_pequena, Centro);
            }
            dibujarElegido(t, e->ultimo, pie);
            if (copiando)
                texto("Pulsa A en un hueco para poner allí la copia. Si ya tiene un diseño, se te pregunta antes.", PX + 20, PY + 330, MARRON_SUAVE,
                    g_pequena, Izquierda, PW - 40);
        };
        v.teclas = {{"A", elegido ? "Copiar aquí" : "Ver en grande"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", elegido ? "Cancelar" : "Volver"});
    };
}

// Lo que no cabe en el recuadro: copiar, vaciar, y guardar o traer todos de golpe.
void masOpciones(Tipo t, int i)
{
    const bool vacio = hueco(t, i).vacio;
    std::vector<uint8_t> limpio;
    const bool hayBlanco = blancoDe(t, limpio);
    std::vector<std::pair<std::string, std::function<void()>>> op;
    if (!vacio && cuantos(t) > 1)
        op.push_back({"Copiar a otro hueco…", [t, i] {
            abrir(rejilla(t, i, [t, i](int j) {
                auto copiar = [t, i, j] {
                    auto d = hueco(t, i).d;
                    arreglarUso(d, t);
                    poner(t, j, d);
                    volver();
                    cambiado("Copiado al hueco " + std::to_string(j + 1));
                };
                if (hueco(t, j).vacio)
                    copiar();
                else
                    confirmar("Copiar a otro hueco", "En el hueco " + std::to_string(j + 1) + " está «" + nombreVisible(hueco(t, j)) + "». ¿Cambiarlo por la copia?", "Cambiarlo", copiar);
            }, i), i % POR_PAGINA);
        }});
    if (!vacio && hayBlanco)
        op.push_back({"Vaciar el hueco", [t, i] {
            confirmar("Vaciar el hueco", "¿Quitar «" + nombreVisible(hueco(t, i)) + "»? El hueco se queda vacío, como uno sin usar del juego.", "Vaciar", [t, i] {
                std::vector<uint8_t> d;
                if (!blancoDe(t, d))
                    return;
                poner(t, i, d);
                cambiado("Hueco vaciado");
            });
        }});
    if (cuantos(t) > 1)
    {
        if (usados(t) > 0)
            op.push_back({"Guardar todos en la SD", [t] { guardarTodos(t); }});
        op.push_back({"Cargar desde una carpeta…", [t, i] { abrir(elegirCarpeta(t, i)); }});
    }
    std::vector<std::string> textos;
    for (const auto& o : op)
        textos.push_back(o.first);
    const std::string n = vacio ? "el hueco " + std::to_string(i + 1) : "«" + nombreVisible(hueco(t, i)) + "»";
    elegir("Más opciones", "Para " + n + ". «Cargar desde una carpeta» pone sus diseños en este hueco y los siguientes.", textos, -1,
        [op](int k) { op[k].second(); }, 1);
}

// --------------------------------------------------------------------------------------------------------- ficha
// Un diseño en grande con lo que se puede hacer con él (y la bandera, que es uno solo, como su ventana del Mac). L y R
// pasan al anterior y al siguiente.
Pantalla ficha(Tipo t, int inicial)
{
    auto actual = std::make_shared<int>(inicial);
    return [t, actual](Vista& v) {
        const int i = *actual;
        const Hueco& h = hueco(t, i);
        const bool vacio = h.vacio, otro = !vacio && deOtro(h.d);
        const bool varios = cuantos(t) > 1;
        v.titulo = TITULOS[t];
        v.explicacion = varios ? "Hueco " + std::to_string(i + 1) + " de " + std::to_string(cuantos(t)) + ". Con L y R pasas al anterior y al siguiente." :
            "Puedes cambiarle el nombre, guardarlo como imagen o cambiarlo por uno de la SD.";

        // Left, the picture; right, a panel with the name, who made it, the colours and the actions.
        constexpr int IX = 60, IY = 140, IL = 440;
        constexpr int RX = IX + IL + 20, RW = W - 60 - RX, RY = 140;
        std::string quien = vacio ? "Este hueco está vacío. Trae un diseño de la SD para llenarlo." : quienLoHizo(h.d);
        if (otro)
            quien += " Lo hizo otra persona, así que el juego no te deja retocarlo. Hazlo tuyo para poder hacerlo.";
        const int altoQuien = TTF_FontHeight(g_pequena) * (otro ? 2 : 1);
        const int yColores = RY + 148 + altoQuien;
        v.dibujo = [t, i, vacio, quien, yColores] {
            panel({IX, IY, IL, IL}, "");
            const SDL_Rect r = {IX + 20, IY + 20, IL - 40, IL - 40};
            if (vacio)
            {
                redondo(r, 12, CREMA);
                texto("Vacío", r.x + r.w / 2, r.y + r.h / 2, APAGADO_, g_grande, Centro);
            }
            else
            {
                redondo({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 6, LINEA);
                dibujo(textura(t, i), r, 25);
            }
            panel({RX, RY, RW, ABAJO - 72 - RY}, "");
            texto("Nombre", RX + 20, RY + 16, MARRON, g_pequena);
            texto(quien, RX + 20, RY + 122, MARRON_SUAVE, g_pequena, Izquierda, RW - 40);
            if (vacio)
                return;
            texto("Colores", RX + 20, yColores, MARRON, g_pequena);
            const auto& d = hueco(t, i).d;
            for (int c = 0; c < COLORES; c++)
            {
                uint8_t cr, cg, cb;
                color(d, c, cr, cg, cb);
                const SDL_Rect q = {RX + 20 + c * 42, yColores + 32, 32, 32};
                redondo({q.x - 2, q.y - 2, q.w + 4, q.h + 4}, 18, LINEA);
                redondo(q, 16, {cr, cg, cb, 255});
            }
        };

        // The name, as the text field of the Mac: touch it to change it.
        Boton nb{{RX + 20, RY + 46, RW - 40, 66}, nombreVisible(h), [t, i] {
            auto d = hueco(t, i).d;
            std::string n = nombre(d);
            if (!pedirTexto("Nombre del diseño", "Hasta 20 letras", n, 20))
                return;
            ponerNombre(d, n);
            poner(t, i, d);
            cambiado("Nombre cambiado");
        }, !vacio};
        nb.grande = true;
        nb.detalle = vacio ? "" : "Toca para cambiarlo";
        nb.imagen = dibujos::menu("MyDesignPro");
        v.b.push_back(nb);

        std::vector<uint8_t> limpio;
        const bool hayBlanco = blancoDe(t, limpio);
        struct Accion {
            std::string texto;
            std::function<void()> hacer;
            bool activo;
        };
        std::vector<Accion> acciones = {
            {"Hacerlo mío", [t, i] {
                auto d = hueco(t, i).d;
                hacerMio(d, partida::personal());
                poner(t, i, d);
                cambiado("Ahora es tuyo");
            }, otro},
            {"Guardar como imagen", [t, i] { guardarImagen(t, i); }, !vacio},
            {"Guardar en la SD", [t, i] { guardarArchivo(t, i); }, !vacio},
            {"Traer de la SD…", [t, i] { abrir(elegirArchivo(t, i)); }, true},
        };
        if (varios)
            acciones.push_back({"Copiar a otro hueco…", [t, i] {
                abrir(rejilla(t, i, [t, i](int j) {
                    auto copiar = [t, i, j] {
                        auto d = hueco(t, i).d;
                        arreglarUso(d, t);
                        poner(t, j, d);
                        volver();
                        cambiado("Copiado al hueco " + std::to_string(j + 1));
                    };
                    if (hueco(t, j).vacio)
                        copiar();
                    else
                        confirmar("Copiar a otro hueco", "En el hueco " + std::to_string(j + 1) + " está «" + nombreVisible(hueco(t, j)) + "». ¿Cambiarlo por la copia?", "Cambiarlo", copiar);
                }, i), i % POR_PAGINA);
            }, !vacio});
        acciones.push_back({"Vaciar el hueco", [t, i] {
            confirmar("Vaciar el hueco", "¿Quitar «" + nombreVisible(hueco(t, i)) + "»? El hueco se queda vacío, como uno sin usar del juego.", "Vaciar", [t, i] {
                std::vector<uint8_t> d;
                if (!blancoDe(t, d))
                    return;
                poner(t, i, d);
                cambiado("Hueco vaciado");
            });
        }, !vacio && hayBlanco});
        const int bw = (RW - 40 - 12) / 2, bh = 48;
        const int y0 = yColores + 84;
        for (size_t k = 0; k < acciones.size(); k++)
        {
            const int fila = (int)k / 2, col = (int)k % 2;
            v.b.push_back({{RX + 20 + col * (bw + 12), y0 + fila * (bh + 10), bw, bh}, acciones[k].texto, acciones[k].hacer, acciones[k].activo});
        }
        if (varios)
        {
            v.b.push_back({{IX, ABAJO_Y, 200, ABAJO_H}, "Anterior", [actual] { (*actual)--; }, i > 0});
            v.b.push_back({{IX + 212, ABAJO_Y, 200, ABAJO_H}, "Siguiente", [actual] { (*actual)++; }, i + 1 < cuantos(t)});
            v.paginar = [t, actual](int d) { *actual = std::clamp(*actual + (d > 0 ? 1 : -1), 0, cuantos(t) - 1); };
            v.teclas = {{"A", "Elegir"}, {"L R", "Otro diseño"}, {"B", "Volver"}};
        }
        v.b.push_back({{W - 60 - 220, ABAJO_Y, 220, ABAJO_H}, "Volver", [] { volver(); }});
    };
}

// ------------------------------------------------------------------------------------------------------ tarjetas
std::string resumen(Tipo t)
{
    if (cuantos(t) == 1)
        return "«" + nombreVisible(hueco(t, 0)) + "»";
    return std::to_string(usados(t)) + " de " + std::to_string(cuantos(t)) + " huecos usados";
}

void abrirTipo(Tipo t)
{
    if (cuantos(t) == 1)
    {
        abrir(ficha(t, 0));
        return;
    }
    int primero = 0;
    for (int i = 0; i < cuantos(t); i++)
        if (!hueco(t, i).vacio)
        {
            primero = i;
            break;
        }
    abrir(rejilla(t, primero), primero % POR_PAGINA);
}

Registro r1({SeccionDisenos, 10, [] { return std::string(TITULOS[Normales]); }, [] { return resumen(Normales); }, [] { abrirTipo(Normales); }, nullptr, "MyDesignPro"});
Registro r2({SeccionDisenos, 20, [] { return std::string(TITULOS[Pro]); }, [] { return resumen(Pro); }, [] { abrirTipo(Pro); }, nullptr, "MyDesignPro"});
Registro r3({SeccionDisenos, 30, [] { return std::string(TITULOS[Bandera]); }, [] { return resumen(Bandera); }, [] { abrirTipo(Bandera); }, nullptr, "Flowershower"});
Registro r4({SeccionDisenos, 40, [] { return std::string(TITULOS[Sastreria]); }, [] { return resumen(Sastreria); }, [] { abrirTipo(Sastreria); }, nullptr, "TailorTicket"});

} // namespace
