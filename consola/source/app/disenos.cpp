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
// Los huecos de un tipo en una rejilla de dibujos (7 x 4 por página), con el elegido en grande a la derecha. Con
// «elegido», es para elegir un hueco (copiar a otro hueco).
constexpr int COLS = 7, FILAS = 4, POR_PAGINA = COLS * FILAS;
constexpr int TX = 40, TY = 146, TW = 104, IMG = 64, ETIQ = 30, PASO_X = 112, PASO_Y = 108;

SDL_Rect imagenDe(int k) { return {TX + (k % COLS) * PASO_X + (TW - IMG) / 2, TY + (k / COLS) * PASO_Y, IMG, IMG}; }
SDL_Rect etiquetaDe(int k) { return {TX + (k % COLS) * PASO_X, TY + (k / COLS) * PASO_Y + IMG + 4, TW, ETIQ}; }

Pantalla ficha(Tipo t, int i);

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
            v.explicacion = "Elige el hueco donde poner la copia del diseño " + std::to_string(excepto + 1) + ".";
        }
        else
        {
            v.titulo = TITULOS[t];
            v.explicacion = std::to_string(usados(t)) + " de " + std::to_string(total) + " huecos usados. Elige uno para verlo y cambiarlo.";
        }
        if (paginas > 1)
            v.explicacion += " (página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ")";
        for (int k = 0; k < enPagina; k++)
        {
            const int i = desde + k;
            const Hueco& h = hueco(t, i);
            v.b.push_back({etiquetaDe(k), nombreVisible(h), [abrirHueco, i] { abrirHueco(i); }, i != excepto});
        }
        const int f = foco();
        if (f >= 0 && f < enPagina)
            e->ultimo = desde + f;
        e->ultimo = std::clamp(e->ultimo, 0, total - 1);

        // Bottom: pages, and in the normal grid «save all» and «load a folder».
        std::vector<Boton> abajo;
        if (paginas > 1)
        {
            abajo.push_back({{}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            abajo.push_back({{}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
        }
        if (!elegido)
        {
            abajo.push_back({{}, "Guardar todos", [t] { guardarTodos(t); }, usados(t) > 0});
            abajo.push_back({{}, "Cargar carpeta", [t, e] { abrir(elegirCarpeta(t, e->ultimo)); }});
        }
        abajo.push_back({{}, elegido ? "Cancelar" : "Volver", [] { volver(); }});
        const int n = (int)abajo.size();
        const int ancho = std::min(260, (W - 120 - (n - 1) * 16) / n);
        for (int k = 0; k < n; k++)
        {
            abajo[k].r = {k == n - 1 ? W - 60 - ancho : 60 + k * (ancho + 16), ABAJO - 66, ancho, 56};
            v.b.push_back(abajo[k]);
        }
        if (paginas > 1)
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };

        v.dibujo = [t, e, desde, enPagina, f] {
            for (int k = 0; k < enPagina; k++)
            {
                const SDL_Rect r = imagenDe(k);
                if (k == f)
                    redondo({r.x - (TW - IMG) / 2 - 5, r.y - 5, TW + 10, IMG + ETIQ + 14}, 18, AMARILLO);
                if (hueco(t, desde + k).vacio)
                    redondo(r, 8, CREMA_OSCURA);
                else
                    dibujo(textura(t, desde + k), r, 8);
            }
            // The chosen one, large.
            const int i = e->ultimo;
            const Hueco& h = hueco(t, i);
            const int x = 880;
            texto(encajar(std::to_string(i + 1) + ". " + nombreVisible(h), 360), x, TY - 4, MARRON);
            if (h.vacio)
            {
                redondo({x, TY + 36, 288, 288}, 12, CREMA_OSCURA);
                texto("Hueco vacío", x + 144, TY + 180, MARRON_SUAVE, nullptr, Centro);
                return;
            }
            dibujo(textura(t, i), {x, TY + 36, 288, 288}, 18);
            paleta(h.d, x, TY + 338, 18);
            texto(quienLoHizo(h.d), x, TY + 372, MARRON_SUAVE, g_pequena, Izquierda, 360);
        };
        v.entrada = [enPagina, desde, abrirHueco, excepto](u64, u64, int tx, int ty) {
            for (int k = 0; k < enPagina; k++)
            {
                const SDL_Rect r = imagenDe(k);
                if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h)
                {
                    enfocar(k);
                    if (desde + k != excepto)
                        abrirHueco(desde + k);
                    return true;
                }
            }
            return false;
        };
        v.teclas = {{"A", elegido ? "Copiar aquí" : "Ver"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", elegido ? "Cancelar" : "Volver"});
    };
}

// --------------------------------------------------------------------------------------------------------- ficha
// Un diseño en grande con lo que se puede hacer con él. L y R pasan al anterior y al siguiente.
Pantalla ficha(Tipo t, int inicial)
{
    auto actual = std::make_shared<int>(inicial);
    return [t, actual](Vista& v) {
        const int i = *actual;
        const Hueco& h = hueco(t, i);
        const bool vacio = h.vacio, otro = !vacio && deOtro(h.d);
        v.titulo = cuantos(t) == 1 ? std::string(TITULOS[t]) : std::string(TITULOS[t]) + " · hueco " + std::to_string(i + 1) + " de " + std::to_string(cuantos(t));
        v.explicacion = vacio ? "Este hueco está vacío. Trae un diseño de un archivo para llenarlo." : quienLoHizo(h.d);
        if (otro)
            v.explicacion += " Lo hizo otra persona, así que el juego no te deja retocarlo. Hazlo tuyo para poder hacerlo.";

        v.dibujo = [t, i, vacio] {
            const SDL_Rect r = {60, 160, 384, 384};
            if (vacio)
            {
                redondo(r, 16, CREMA_OSCURA);
                texto("Vacío", r.x + r.w / 2, r.y + r.h / 2, MARRON_SUAVE, g_grande, Centro);
                return;
            }
            dibujo(textura(t, i), r, 24);
            texto("Colores", 480, 250, MARRON, g_pequena);
            paleta(hueco(t, i).d, 480, 282);
        };

        const int x1 = 480, x2 = 860, ancho = 360, alto = 66;
        Boton nb{{x1, 160, 740, 72}, "Nombre: " + nombreVisible(h), [t, i] {
            auto d = hueco(t, i).d;
            std::string n = nombre(d);
            if (!pedirTexto("Nombre del diseño", "Hasta 20 letras", n, 20))
                return;
            ponerNombre(d, n);
            poner(t, i, d);
            cambiado("Nombre cambiado");
        }, !vacio};
        nb.grande = true;
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
            {"Guardar en la SD", [t, i] { guardarArchivo(t, i); }, !vacio},
            {"Guardar como imagen", [t, i] { guardarImagen(t, i); }, !vacio},
            {"Cambiar por un archivo…", [t, i] { abrir(elegirArchivo(t, i)); }, true},
        };
        if (cuantos(t) > 1)
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
        for (size_t k = 0; k < acciones.size(); k++)
        {
            const int fila = (int)k / 2, col = (int)k % 2;
            v.b.push_back({{col ? x2 : x1, 340 + fila * (alto + 14), ancho, alto}, acciones[k].texto, acciones[k].hacer, acciones[k].activo});
        }
        v.b.push_back({{x2, ABAJO - 80, ancho, 64}, "Volver", [] { volver(); }});
        if (cuantos(t) > 1)
        {
            v.paginar = [t, actual](int d) { *actual = std::clamp(*actual + (d > 0 ? 1 : -1), 0, cuantos(t) - 1); };
            v.teclas = {{"A", "Elegir"}, {"L R", "Otro diseño"}, {"B", "Volver"}};
        }
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

Registro r1({SeccionDisenos, 10, [] { return std::string(TITULOS[Normales]); }, [] { return resumen(Normales); }, [] { abrirTipo(Normales); }});
Registro r2({SeccionDisenos, 20, [] { return std::string(TITULOS[Pro]); }, [] { return resumen(Pro); }, [] { abrirTipo(Pro); }});
Registro r3({SeccionDisenos, 30, [] { return std::string(TITULOS[Bandera]); }, [] { return resumen(Bandera); }, [] { abrirTipo(Bandera); }});
Registro r4({SeccionDisenos, 40, [] { return std::string(TITULOS[Sastreria]); }, [] { return resumen(Sastreria); }, [] { abrirTipo(Sastreria); }});

} // namespace
