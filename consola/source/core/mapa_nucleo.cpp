// Rejillas de objetos y el mapa de la isla: lo mismo que GridItems e IslandMapWindow de NHSE para Mac (ver mapa.hpp).
#include "mapa.hpp"
#include <algorithm>
#include <map>
#include <unordered_map>
#include "mapa_objetos.hpp"
#include "mapa_terreno.hpp"
#include "mapa_casas.hpp"
#include "vecinos.hpp"

namespace mapa {

using namespace acnh;

namespace {

uint16_t lee16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void pon16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

constexpr int NUM_PLANTAS = sizeof(PLANTAS) / sizeof(PLANTAS[0]);

// Tamaño de un objeto sin girar (ItemInfo.GetItemSize): lo que no está en la tabla ocupa 2×2.
void tamanoBase(const Objeto& o, int& w, int& h)
{
    const uint16_t id = idVisible(o);
    if (enterrado(o) || tirado(o) || id >= MAPA_INFO_N)
    {
        w = h = 2;
        return;
    }
    w = MAPA_INFO[id] & 15;
    h = (MAPA_INFO[id] >> 4) & 15;
}

// Ancho y alto de lo que se quita o se marca como extensión, sin salir de la rejilla (LayerItem.GetTileWidthHeight).
void tamanoRecortado(const Rejilla& r, const Objeto& o, int x, int y, int& w, int& h)
{
    tamanoBase(o, w, h);
    if (giro(o) & 1)
        std::swap(w, h);
    if (x + w - 1 >= r.ancho)
        w = r.ancho - x;
    if (y + h - 1 >= r.alto)
        h = r.alto - y;
}

void borrarExtensiones(Rejilla& r, const Objeto& o, int x, int y)
{
    int w, h;
    tamanoRecortado(r, o, x, y, w, h);
    for (int ix = 0; ix < w; ix++)
        for (int iy = 0; iy < h; iy++)
            if (ix || iy)
                r.escribir(x + ix, y + iy, nada());
}

void ponerExtensiones(Rejilla& r, const Objeto& o, int x, int y)
{
    int w, h;
    tamanoRecortado(r, o, x, y, w, h);
    for (int ix = 0; ix < w; ix++)
        for (int iy = 0; iy < h; iy++)
        {
            if (!ix && !iy)
                continue;
            Objeto e;
            e.id = EXTENSION;
            e.cuenta = o.id;
            e.usos = (uint16_t)(ix | (iy << 8));
            r.escribir(x + ix, y + iy, e);
        }
}

// LayerItem.IsOccupied: 0 libre, 1 fuera, 2 choca.
int ocupado(const Rejilla& r, const Objeto& o, int x, int y)
{
    int w, h;
    tamanoBase(o, w, h);
    if (giro(o) & 1)
        std::swap(w, h);
    if (x + w - 1 >= r.ancho || y + h - 1 >= r.alto)
        return 1;
    for (int ix = 0; ix < w; ix++)
        for (int iy = 0; iy < h; iy++)
            if (!esNada(r.leer(x + ix, y + iy)))
                return 2;
    return 0;
}

Objeto buscarEstorbo(const Rejilla& r, int x, int y, int w, int h)
{
    for (int ix = 0; ix < w; ix++)
        for (int iy = 0; iy < h; iy++)
        {
            Objeto o;
            int rx, ry;
            if (raiz(r, x + ix, y + iy, o, rx, ry))
                return o;
        }
    return nada();
}

bool esConcha(uint16_t id)
{
    for (uint16_t c : MAPA_CONCHAS)
        if (c == id)
            return true;
    return false;
}

uint16_t grupoDe(uint16_t id)
{
    if (id < MAPA_INFO_N)
        return MAPA_GRUPO[id];
    const int p = plantaDe(id);
    return p >= 0 ? PLANTAS[p].grupo : 0;
}

int recetaObjeto(uint16_t receta)
{
    for (const auto& r : MAPA_RECETAS)
        if (r.receta == receta)
            return r.objeto;
    return -1;
}

// Una regla de nombre: el grupo del objeto (o su número si no lo hay) para lo que va detrás del « - ».
uint32_t parteNombre(uint16_t id)
{
    const uint16_t g = grupoDe(id);
    return g ? g : 0x10000u | id;
}

std::string nombreDeId(uint16_t id)
{
    const uint16_t g = grupoDe(id);
    if (g)
        return MAPA_NOMBRES[g];
    return "objeto nº " + std::to_string(id);
}

} // namespace

Objeto nada()
{
    Objeto o;
    o.id = NADA;
    return o;
}

Objeto Rejilla::leer(int x, int y) const
{
    const uint8_t* p = datos + ((size_t)x * alto + y) * 8;
    Objeto o;
    o.id = lee16(p);
    o.sistema = p[2];
    o.extra = p[3];
    o.cuenta = lee16(p + 4);
    o.usos = lee16(p + 6);
    return o;
}

void Rejilla::escribir(int x, int y, const Objeto& o)
{
    uint8_t* p = datos + ((size_t)x * alto + y) * 8;
    pon16(p, o.id);
    p[2] = o.sistema;
    p[3] = o.extra;
    pon16(p + 4, o.cuenta);
    pon16(p + 6, o.usos);
}

void tamano(const Objeto& o, int& w, int& h)
{
    tamanoBase(o, w, h);
    if (giro(o) & 1)
        std::swap(w, h);
}

bool girable(const Objeto& o)
{
    if (esNada(o) || esPlanta(o) || enterrado(o) || tirado(o))
        return false;
    const uint16_t id = idVisible(o);
    return id < MAPA_INFO_N && (MAPA_INFO[id] & 0x100);
}

int categoria(uint16_t id) { return id < MAPA_INFO_N ? (MAPA_INFO[id] >> 9) & 15 : 0; }

int plantaDe(uint16_t id)
{
    if (id < 60000)
        return -1;
    int a = 0, b = NUM_PLANTAS - 1;
    while (a <= b)
    {
        const int m = (a + b) / 2;
        if (PLANTAS[m].id == id)
            return m;
        if (PLANTAS[m].id < id)
            a = m + 1;
        else
            b = m - 1;
    }
    return -1;
}

int clasePlanta(uint16_t id)
{
    const int p = plantaDe(id);
    return p < 0 ? -1 : PLANTAS[p].clase;
}

uint64_t claveNombre(const Objeto& o)
{
    if (esNada(o))
        return 0;
    const uint16_t vis = idVisible(o);
    const int p = plantaDe(vis);
    if (p >= 0)
        return (uint64_t)PLANTAS[p].grupo << 32;
    if (esExtension(o))
        return (uint64_t)parteNombre(vis) << 32;
    const uint16_t id = o.id;
    const uint64_t base = (uint64_t)parteNombre(id) << 32;
    const int tipo = id < MAPA_INFO_N ? MAPA_INFO[id] >> 13 : 0;
    switch (tipo)
    {
        case 3: // flor con genes
            return (o.cuenta & 0xFF) ? base | 0x1000000u | (o.cuenta & 0xFF) : base;
        case 1: // receta o botella con mensaje: lo que se fabrica
        {
            const int hecho = recetaObjeto(o.cuenta);
            return base | (hecho >= 0 ? 0x2000000u | parteNombre((uint16_t)hecho) : 0x4000000u | o.cuenta);
        }
        case 2: // fósil sin identificar
            return base | 0x6000000u | parteNombre(o.cuenta);
        case 4: // árbol con lo que da
            return o.cuenta ? base | 0x8000000u | parteNombre(o.cuenta) : base;
    }
    return base;
}

std::string nombre(const Objeto& o)
{
    if (esNada(o))
        return "Sitio vacío";
    const uint16_t vis = idVisible(o);
    if (plantaDe(vis) >= 0 || esExtension(o))
        return nombreDeId(vis);
    const uint16_t id = o.id;
    std::string n = nombreDeId(id);
    const int tipo = id < MAPA_INFO_N ? MAPA_INFO[id] >> 13 : 0;
    if (tipo == 3 && (o.cuenta & 0xFF))
        n += " (con genes)";
    else if (tipo == 1)
    {
        const int hecho = recetaObjeto(o.cuenta);
        n += " - " + (hecho >= 0 ? nombreDeId((uint16_t)hecho) : std::to_string(o.cuenta));
    }
    else if (tipo == 2)
        n += " - " + nombreDeId(o.cuenta);
    else if (tipo == 4 && o.cuenta)
        n += " - " + nombreDeId(o.cuenta);
    return n;
}

// --------------------------------------------------------------------------------------------------------- rejillas
bool raiz(const Rejilla& r, int x, int y, Objeto& o, int& rx, int& ry)
{
    rx = x, ry = y;
    o = r.leerSeguro(x, y);
    if (esNada(o))
        return false;
    if (!esExtension(o))
        return true;
    const int px = x - extX(o), py = y - extY(o);
    if (r.dentro(px, py))
    {
        const Objeto padre = r.leer(px, py);
        if (esRaiz(padre) && padre.id == idExtension(o))
        {
            o = padre, rx = px, ry = py;
            return true;
        }
    }
    return true; // un trozo suelto: también se puede quitar
}

bool quitar(Rejilla& r, int x, int y)
{
    Objeto o;
    int rx, ry;
    if (!raiz(r, x, y, o, rx, ry))
        return false;
    if (esRaiz(o))
        borrarExtensiones(r, o, rx, ry);
    r.escribir(rx, ry, nada());
    return true;
}

Resultado poner(Rejilla& r, const Objeto& nuevo, int x, int y, Objeto* estorbo, int* enX, int* enY, bool ajustar)
{
    if (estorbo)
        *estorbo = nada();
    Objeto o;
    int rx, ry;
    if (raiz(r, x, y, o, rx, ry))
        x = rx, y = ry;
    if (ajustar && esPlanta(nuevo))
        x &= ~1, y &= ~1;
    if (enX)
        *enX = x, *enY = y;
    if (!r.dentro(x, y))
        return FueraDelMapa;

    // Una copia de la zona, para dejarla como estaba si no cabe.
    int w, h;
    tamano(nuevo, w, h);
    struct Copia {
        int x, y;
        Objeto o;
    };
    std::vector<Copia> copia;
    for (int ix = x - 12; ix < x + w + 12; ix++)
        for (int iy = y - 12; iy < y + h + 12; iy++)
            if (r.dentro(ix, iy))
                copia.push_back({ix, iy, r.leer(ix, iy)});

    quitar(r, x, y);
    const int permiso = ocupado(r, nuevo, x, y);
    if (permiso != 0)
    {
        if (permiso == 2 && estorbo)
            *estorbo = buscarEstorbo(r, x, y, w, h);
        for (const auto& c : copia)
            r.escribir(c.x, c.y, c.o);
        return permiso == 1 ? FueraDelMapa : NoCabe;
    }
    r.escribir(x, y, nuevo);
    if (esRaiz(nuevo))
        ponerExtensiones(r, nuevo, x, y);
    return Bien;
}

Resultado girar(Rejilla& r, int x, int y, Objeto* estorbo)
{
    if (estorbo)
        *estorbo = nada();
    Objeto o;
    int rx, ry;
    if (!raiz(r, x, y, o, rx, ry) || !esRaiz(o))
        return FueraDelMapa;
    o.sistema = (uint8_t)((o.sistema & ~3) | ((giro(o) + 1) & 3));
    return poner(r, o, rx, ry, estorbo, nullptr, nullptr, false);
}

Resultado mover(Rejilla& r, int x, int y, int aX, int aY, Objeto* estorbo)
{
    if (estorbo)
        *estorbo = nada();
    Objeto o;
    int rx, ry;
    if (!r.dentro(x, y) || !raiz(r, x, y, o, rx, ry) || !esRaiz(o))
        return FueraDelMapa;
    if (rx == aX && ry == aY)
        return Bien;
    quitar(r, rx, ry);
    Resultado res;
    Objeto otro;
    int ox, oy;
    if (!r.dentro(aX, aY))
        res = FueraDelMapa;
    else if (raiz(r, aX, aY, otro, ox, oy))
    {
        res = NoCabe;
        if (estorbo)
            *estorbo = otro;
    }
    else
        res = poner(r, o, aX, aY, estorbo, nullptr, nullptr, false);
    if (res != Bien)
        poner(r, o, rx, ry, nullptr, nullptr, nullptr, false);
    return res;
}

bool cabeLibre(const Rejilla& r, const Objeto& o, int x, int y)
{
    return r.dentro(x, y) && ocupado(r, o, x, y) == 0;
}

// ------------------------------------------------------------------------------------------------- ordenar la isla
bool esTipo(const Objeto& o, Tipo t)
{
    const uint16_t id = idVisible(o);
    const int c = clasePlanta(id);
    switch (t)
    {
        case Hierbas: return c == 1;
        case Flores: return c == 2;
        case Arboles: return c == 3;
        case Arbustos: return c == 4;
        case Vallas: return c == 5;
        case Hoyos: return c == 6;
        case Ramas: return id == 2500;
        case Conchas: return esConcha(id);
        case Cosas: return c < 0;
        default: return true;
    }
}

namespace {

Zona entera(const Rejilla& r, Zona z) { return z.w <= 0 ? Zona{0, 0, r.ancho, r.alto} : z; }

template <class F>
std::vector<Punto> encontrar(const Rejilla& r, Zona z, F cumple)
{
    z = entera(r, z);
    std::vector<Punto> res;
    for (int x = z.x; x < z.x + z.w; x++)
        for (int y = z.y; y < z.y + z.h; y++)
        {
            if (!r.dentro(x, y))
                continue;
            const Objeto o = r.leer(x, y);
            if (!esNada(o) && esRaiz(o) && cumple(o))
                res.push_back({x, y});
        }
    return res;
}

int quitarLos(Rejilla& r, const std::vector<Punto>& p)
{
    for (const auto& q : p)
        quitar(r, q.x, q.y);
    return (int)p.size();
}

} // namespace

std::vector<Punto> buscar(const Rejilla& r, Zona z, Tipo t)
{
    return encontrar(r, z, [t](const Objeto& o) { return esTipo(o, t); });
}

std::vector<Punto> buscarNombre(const Rejilla& r, Zona z, uint64_t clave)
{
    return encontrar(r, z, [clave](const Objeto& o) { return claveNombre(o) == clave; });
}

int contar(const Rejilla& r, Tipo t, Zona z) { return (int)buscar(r, z, t).size(); }
int quitarTodos(Rejilla& r, Tipo t, Zona z) { return quitarLos(r, buscar(r, z, t)); }
int quitarNombre(Rejilla& r, uint64_t clave, Zona z) { return quitarLos(r, buscarNombre(r, z, clave)); }

int cambiarTodos(Rejilla& r, Tipo t, const std::vector<uint16_t>& ids, bool alAzar, Zona z, uint32_t semilla)
{
    if (ids.empty())
        return 0;
    const auto p = encontrar(r, z, [&](const Objeto& o) { return esTipo(o, t) && (ids.size() > 1 || o.id != ids[0]); });
    uint32_t s = semilla ? semilla : 1;
    for (const auto& q : p)
    {
        size_t k;
        if (alAzar)
        {
            s ^= s << 13, s ^= s >> 17, s ^= s << 5;
            k = s % ids.size();
        }
        else
            k = (size_t)((q.x / 2) + (q.y / 2)) % ids.size();
        quitar(r, q.x, q.y);
        Objeto o = nada();
        o.id = ids[k];
        poner(r, o, q.x, q.y);
    }
    return (int)p.size();
}

int regar(Rejilla& r, Zona z)
{
    // LayerFieldItem.WaterAllFlowers: días regada 31 y regada (lo demás igual).
    z = entera(r, z);
    int n = 0;
    for (int x = z.x; x < z.x + z.w; x++)
        for (int y = z.y; y < z.y + z.h; y++)
        {
            if (!r.dentro(x, y))
                continue;
            Objeto o = r.leer(x, y);
            if (esNada(o) || !esRaiz(o) || clasePlanta(o.id) != 2)
                continue;
            // Watered: bits 0-4 de los bytes 5-6 (días) y el bit 15 (regada).
            uint16_t regada = (uint16_t)((o.cuenta >> 8) | ((o.usos & 0xFF) << 8));
            regada = (uint16_t)((regada & 0xFFE0) | 31);
            regada |= 0x8000;
            o.cuenta = (uint16_t)((o.cuenta & 0xFF) | ((regada & 0xFF) << 8));
            o.usos = (uint16_t)((o.usos & 0xFF00) | (regada >> 8));
            r.escribir(x, y, o);
            n++;
        }
    return n;
}

int borrar(Rejilla& r, Zona z, Tipo t, uint64_t clave)
{
    int n = 0;
    for (int x = z.x; x < z.x + z.w; x++)
        for (int y = z.y; y < z.y + z.h; y++)
        {
            Objeto o;
            int rx, ry;
            if (!r.dentro(x, y) || !raiz(r, x, y, o, rx, ry))
                continue;
            const bool toma = esRaiz(o) ? (clave ? claveNombre(o) == clave : esTipo(o, t)) : (t == Todo && !clave);
            if (!toma)
                continue;
            quitar(r, rx, ry);
            n++;
        }
    return n;
}

std::vector<Clase> clases(const Rejilla& r, Zona z)
{
    std::vector<Clase> res;
    std::unordered_map<uint64_t, size_t> donde;
    for (const auto& p : encontrar(r, z, [](const Objeto&) { return true; }))
    {
        const Objeto o = r.leer(p.x, p.y);
        const uint64_t c = claveNombre(o);
        auto it = donde.find(c);
        if (it == donde.end())
        {
            donde[c] = res.size();
            res.push_back({c, o, 1});
        }
        else
            res[it->second].cuantas++;
    }
    std::stable_sort(res.begin(), res.end(), [](const Clase& a, const Clase& b) {
        if (a.cuantas != b.cuantas)
            return a.cuantas > b.cuantas;
        return normalizar(nombre(a.ejemplo)) < normalizar(nombre(b.ejemplo));
    });
    return res;
}

// ------------------------------------------------------------------------------------------------------------- isla
Rejilla capa(std::vector<uint8_t>& main, int c)
{
    Rejilla r;
    r.datos = main.data() + CAMPO + (c ? TAM_CAPA : 0);
    r.ancho = ANCHO;
    r.alto = ALTO;
    return r;
}

std::vector<Punto> sinApoyo(std::vector<uint8_t>& main)
{
    const Rejilla abajo = capa(main, 0), arriba = capa(main, 1);
    std::vector<Punto> v;
    for (int x = 0; x < ANCHO; x++)
        for (int y = 0; y < ALTO; y++)
            if (!esNada(arriba.leer(x, y)) && esNada(abajo.leer(x, y)))
                v.push_back({x, y});
    return v;
}

std::vector<Edificio> edificios(const std::vector<uint8_t>& main, const std::vector<std::string>& jugadores)
{
    // Como IslandMapWindow.GetBuildings del Mac: nombre, color y tamaño en el mapa.
    static const uint32_t TEJADOS[8] = {0xD55A4C, 0x4D80C4, 0x5EA45A, 0xE3B03E, 0x8E6CC0, 0xE07E3C, 0x70717A, 0x8B5E3C};
    auto tejado = [](uint16_t roof) {
        const int n = sizeof(COLOR_TEJADO);
        return TEJADOS[roof < n ? COLOR_TEJADO[roof] : 7];
    };
    constexpr uint32_t CASAS_JUGADORES = 0x33CBE0, TAM_CASA_JUGADOR = 0x28A28;
    std::vector<Edificio> res;
    for (int i = 0; i < NUM_EDIFICIOS; i++)
    {
        const uint8_t* b = main.data() + EDIFICIOS + i * 0x14;
        const int tipo = lee16(b);
        if (tipo == 0)
            continue;
        Edificio e;
        e.tipo = tipo;
        e.x = lee16(b + 2);
        e.y = lee16(b + 4);
        e.indice = i;
        const int angulo = b[6];
        const uint16_t modelo = lee16(b + 8);
        bool diagonal = false;
        if (tipo >= 1 && tipo <= 8)
        {
            const int k = tipo - 1;
            e.nombre = k < (int)jugadores.size() ? jugadores[k] : "Parcela libre";
            e.color = tejado(lee16(main.data() + CASAS_JUGADORES + k * TAM_CASA_JUGADOR + 6));
            e.w = 10, e.h = 8;
        }
        else if (tipo >= 9 && tipo <= 18)
        {
            const int k = tipo - 9;
            const uint8_t* casa = main.data() + CASAS + k * TAM_CASA;
            const int8_t npc = (int8_t)casa[0x1C4];
            std::string interno = npc >= 0 && npc < NUM_VECINOS ? vecinoEn(main, npc) : "";
            e.nombre = "Parcela libre";
            for (const auto& v : LISTA_VECINOS)
                if (interno == v.interno)
                    e.nombre = v.nombre;
            e.color = tejado(lee16(casa + 0x0A));
            e.w = 9, e.h = 7;
        }
        else
        {
            struct Fijo {
                int tipo;
                const char* nombre;
                uint32_t color;
                double w, h;
            };
            static const Fijo FIJOS[] = {
                {20, "Servicios a residentes", 0x4EA278, 16, 11}, {23, "Servicios a residentes", 0x6CB46A, 12, 9},
                {19, "Comercio Nook", 0x3B8E8A, 14, 9}, {21, "Museo", 0x8C6E55, 14, 10}, {24, "Hermanas Manitas", 0xDF7C8E, 10, 8},
                {22, "Aeródromo", 0x4D80C4, 18, 10}, {25, "Zona de acampada", 0xE9C46A, 10, 8}, {29, "Estudio de fotos", 0x9C7BC9, 12, 8},
                {42, "Hotel", 0x3FA7B5, 12, 8}, {28, "Barco de Ladino", 0x6B5BA0, 12, 6},
            };
            e.nombre = "Edificio", e.color = 0xA08C70, e.w = 8, e.h = 6;
            for (const auto& f : FIJOS)
                if (f.tipo == tipo)
                    e.nombre = f.nombre, e.color = f.color, e.w = f.w, e.h = f.h;
            if (tipo == 26)
            {
                e.nombre = "Puente", e.color = 0xB98A5A, e.w = 10, e.h = 5;
                for (const auto& p : PUENTES)
                    if (p.tipo == modelo)
                        e.nombre = p.nombre, e.color = p.color, e.w = p.largo, diagonal = p.diagonal;
            }
            else if (tipo == 27)
            {
                e.nombre = "Cuesta", e.color = 0xB98A5A, e.w = 8, e.h = 4;
                for (const auto& p : CUESTAS)
                    if (p.tipo == modelo)
                        e.nombre = p.nombre, e.color = p.color;
            }
        }
        e.angulo = diagonal ? angulo * 90.0 - 45 : angulo * 90.0;
        res.push_back(e);
    }
    return res;
}

// ------------------------------------------------------------------------------------- puentes y cuestas
namespace {
uint8_t* edificio(std::vector<uint8_t>& main, int i) { return main.data() + EDIFICIOS + i * 0x14; }
const uint8_t* edificio(const std::vector<uint8_t>& main, int i) { return main.data() + EDIFICIOS + i * 0x14; }
void pon16(uint8_t* p, int v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
bool esCruce(const std::vector<uint8_t>& main, int i)
{
    if (i < 0 || i >= NUM_EDIFICIOS)
        return false;
    const int t = lee16(edificio(main, i));
    return t == PUENTE || t == CUESTA;
}
} // namespace

int ponerCruce(std::vector<uint8_t>& main, int tipo, uint16_t modelo, int x, int y, int giro)
{
    for (int i = 0; i < NUM_EDIFICIOS; i++)
    {
        uint8_t* b = edificio(main, i);
        if (lee16(b) != 0)
            continue;
        // Como Building de NHSE: todo a cero menos tipo, sitio, giro y modelo.
        std::fill(b, b + 0x14, 0);
        pon16(b, tipo);
        pon16(b + 2, x);
        pon16(b + 4, y);
        b[6] = (uint8_t)(giro & 3);
        pon16(b + 8, modelo);
        return i;
    }
    return -1;
}

bool cambiarCruce(std::vector<uint8_t>& main, int i, uint16_t modelo, int giro)
{
    if (!esCruce(main, i))
        return false;
    uint8_t* b = edificio(main, i);
    b[6] = (uint8_t)(giro & 3);
    pon16(b + 8, modelo);
    return true;
}

bool moverCruce(std::vector<uint8_t>& main, int i, int x, int y)
{
    if (!esCruce(main, i))
        return false;
    uint8_t* b = edificio(main, i);
    pon16(b + 2, x);
    pon16(b + 4, y);
    return true;
}

bool quitarCruce(std::vector<uint8_t>& main, int i)
{
    if (!esCruce(main, i))
        return false;
    uint8_t* b = edificio(main, i);
    std::fill(b, b + 0x14, 0); // Building.Clear
    return true;
}

int contarCruces(const std::vector<uint8_t>& main, int tipo)
{
    int n = 0;
    for (int i = 0; i < NUM_EDIFICIOS; i++)
        n += lee16(edificio(main, i)) == tipo;
    return n;
}

uint16_t modeloCruce(const std::vector<uint8_t>& main, int i) { return esCruce(main, i) ? lee16(edificio(main, i) + 8) : 0; }
int giroCruce(const std::vector<uint8_t>& main, int i) { return esCruce(main, i) ? edificio(main, i)[6] & 3 : 0; }

} // namespace mapa
