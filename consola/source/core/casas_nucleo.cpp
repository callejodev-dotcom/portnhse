// Las casas de los jugadores, como HouseWindow de NHSE para Mac (ver casas.hpp).
#include "casas.hpp"
#include <algorithm>

namespace casas {

using namespace mapa;

namespace {
uint16_t lee16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void pon16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
uint8_t* habitacion(std::vector<uint8_t>& main, int c, int h) { return casa(main, c) + HABITACIONES + h * TAM_HABITACION; }
} // namespace

uint8_t* casa(std::vector<uint8_t>& main, int c) { return main.data() + CASAS + c * TAM_CASA; }
const uint8_t* casa(const std::vector<uint8_t>& main, int c) { return main.data() + CASAS + c * TAM_CASA; }

uint32_t nivel(const std::vector<uint8_t>& main, int c)
{
    const uint8_t* p = casa(main, c);
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

void ponerNivel(std::vector<uint8_t>& main, int c, uint32_t n)
{
    uint8_t* p = casa(main, c);
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (8 * i));
}

// Fachada en 0x04, tejado en 0x06, puerta en 0x08; los pedidos 6 bytes después.
static uint32_t posAspecto(int parte) { return parte == 0 ? 0x06 : parte == 1 ? 0x04 : 0x08; }

uint16_t aspecto(const std::vector<uint8_t>& main, int c, int parte) { return lee16(casa(main, c) + posAspecto(parte)); }

void ponerAspecto(std::vector<uint8_t>& main, int c, int parte, uint16_t valor)
{
    pon16(casa(main, c) + posAspecto(parte), valor);
    pon16(casa(main, c) + posAspecto(parte) + 6, valor);
}

Rejilla capa(std::vector<uint8_t>& main, int c, int h, int superficie)
{
    Rejilla r;
    r.datos = habitacion(main, c, h) + superficie * TAM_CAPA;
    r.ancho = r.alto = LADO;
    return r;
}

Objeto pared(const std::vector<uint8_t>& main, int c, int h, bool suelo)
{
    const uint8_t* p = casa(main, c) + HABITACIONES + h * TAM_HABITACION + PAREDES + (suelo ? 0x10 : 0x08);
    Objeto o;
    o.id = lee16(p);
    o.sistema = p[2];
    o.extra = p[3];
    o.cuenta = lee16(p + 4);
    o.usos = lee16(p + 6);
    return o;
}

void ponerPared(std::vector<uint8_t>& main, int c, int h, bool suelo, const Objeto& o)
{
    uint8_t* p = habitacion(main, c, h) + PAREDES + (suelo ? 0x10 : 0x08);
    pon16(p, o.id);
    p[2] = o.sistema;
    p[3] = o.extra;
    pon16(p + 4, o.cuenta);
    pon16(p + 6, o.usos);
}

std::vector<int> habitaciones(std::vector<uint8_t>& main, int c)
{
    const int construidas = std::clamp((int)nivel(main, c) - 2, 1, NUM_HABITACIONES);
    std::vector<int> res;
    for (int h = 0; h < NUM_HABITACIONES; h++)
    {
        bool algo = false;
        const uint8_t* p = habitacion(main, c, h);
        for (uint32_t i = 0; i < 8 * LADO * LADO && !algo; i++)
            algo = lee16(p + i * 8) != NADA;
        if (h < construidas || algo)
            res.push_back(h);
    }
    return res;
}

Zona sueloDe(uint32_t n, int h)
{
    if (h == 0)
        return n >= 3 ? Zona{2, 2, 16, 16} : Zona{4, 4, 12, 12};
    if (h == 4 || h == 5)
        return Zona{0, 4, 20, 12};
    return Zona{4, 4, 12, 12};
}

Zona limites(uint32_t n, int h, int superficie)
{
    const Zona s = sueloDe(n, h);
    if (superficie >= ParedFondo && superficie <= ParedDerecha)
    {
        const bool lado = superficie == ParedIzquierda || superficie == ParedDerecha;
        return lado ? Zona{s.y, 0, s.h, FILAS_PARED} : Zona{s.x, 0, s.w, FILAS_PARED};
    }
    return s;
}

bool cabe(const Zona& l, const Objeto& o, int x, int y)
{
    int w, h;
    tamano(o, w, h);
    return x >= l.x && y >= l.y && x + w <= l.x + l.w && y + h <= l.y + l.h;
}

Resultado poner(Rejilla& r, const Zona& l, const Objeto& o, int x, int y, Objeto* estorbo, int* enX, int* enY)
{
    if (estorbo)
        *estorbo = nada();
    Objeto ya;
    int rx, ry;
    if (raiz(r, x, y, ya, rx, ry))
        x = rx, y = ry;
    if (enX)
        *enX = x, *enY = y;
    if (!cabe(l, o, x, y))
        return FueraDelMapa;
    return mapa::poner(r, o, x, y, estorbo, enX, enY, false);
}

Resultado girar(Rejilla& r, const Zona& l, int x, int y, Objeto* estorbo)
{
    if (estorbo)
        *estorbo = nada();
    Objeto o = r.leer(x, y);
    o.sistema = (uint8_t)((o.sistema & ~3) | ((giro(o) + 1) & 3));
    if (!cabe(l, o, x, y))
        return FueraDelMapa;
    return mapa::girar(r, x, y, estorbo);
}

Resultado mover(Rejilla& r, const Zona& l, int x, int y, int aX, int aY, Objeto* estorbo)
{
    if (estorbo)
        *estorbo = nada();
    Objeto o;
    int rx, ry;
    if (!raiz(r, x, y, o, rx, ry) || !esRaiz(o))
        return FueraDelMapa;
    if (rx == aX && ry == aY)
        return Bien;
    if (!cabe(l, o, aX, aY))
        return FueraDelMapa;
    return mapa::mover(r, rx, ry, aX, aY, estorbo);
}

Resultado ponerNuevo(Rejilla& r, const Zona& l, const Objeto& o, int x, int y, Objeto* estorbo)
{
    if (estorbo)
        *estorbo = nada();
    Objeto otro;
    int ox, oy;
    if (!cabe(l, o, x, y))
        return FueraDelMapa;
    if (raiz(r, x, y, otro, ox, oy))
    {
        if (estorbo)
            *estorbo = otro;
        return NoCabe;
    }
    return mapa::poner(r, o, x, y, estorbo, nullptr, nullptr, false);
}

// ------------------------------------------------------------------------------------------- opciones avanzadas
int16_t suceso(const std::vector<uint8_t>& main, int c, int i) { return (int16_t)lee16(casa(main, c) + SUCESOS + 2 * i); }

void ponerSuceso(std::vector<uint8_t>& main, int c, int i, int16_t v) { pon16(casa(main, c) + SUCESOS + 2 * i, (uint16_t)v); }

std::vector<uint8_t> sucesos(const std::vector<uint8_t>& main, int c)
{
    const uint8_t* p = casa(main, c) + SUCESOS;
    return std::vector<uint8_t>(p, p + 2 * NUM_SUCESOS);
}

bool ponerSucesos(std::vector<uint8_t>& main, int c, const std::vector<uint8_t>& datos)
{
    if (datos.size() != 2 * NUM_SUCESOS)
        return false;
    std::copy(datos.begin(), datos.end(), casa(main, c) + SUCESOS);
    return true;
}

std::vector<uint8_t> exportar(const std::vector<uint8_t>& main, int c)
{
    const uint8_t* p = casa(main, c);
    return std::vector<uint8_t>(p, p + TAM_CASA);
}

std::vector<uint8_t> exportarHabitacion(const std::vector<uint8_t>& main, int c, int h)
{
    const uint8_t* p = casa(main, c) + HABITACIONES + h * TAM_HABITACION;
    return std::vector<uint8_t>(p, p + TAM_HABITACION);
}

namespace {
// Una habitación de antes de la 2.0 a la de ahora (PlayerRoom1.Upgrade: lo nuevo, a cero).
std::vector<uint8_t> habitacionNueva(const uint8_t* p)
{
    std::vector<uint8_t> r(TAM_HABITACION, 0);
    std::copy(p, p + TAM_HABITACION1, r.begin());
    return r;
}
} // namespace

std::string importar(std::vector<uint8_t>& main, int c, const std::vector<uint8_t>& datos)
{
    std::vector<uint8_t> d;
    if (datos.size() == TAM_CASA)
        d = datos;
    else if (datos.size() == TAM_CASA1)
    {
        // PlayerHouse1.Upgrade: la cabecera, cada habitación agrandada y lo de después de las habitaciones.
        d.assign(TAM_CASA, 0);
        std::copy(datos.begin(), datos.begin() + HABITACIONES, d.begin());
        for (int h = 0; h < NUM_HABITACIONES; h++)
        {
            const auto r = habitacionNueva(datos.data() + HABITACIONES + h * TAM_HABITACION1);
            std::copy(r.begin(), r.end(), d.begin() + HABITACIONES + h * TAM_HABITACION);
        }
        std::copy(datos.begin() + 0x263D0, datos.begin() + 0x263D0 + 0x30, d.begin() + 0x289F8);
    }
    else
        return "No es una casa de jugador (mide " + std::to_string(datos.size()) + " bytes).";
    // Como NHSE: «h.NPC1 = current.NPC1» a través de IPlayerHouse, que en las casas de ahora sigue siendo el NPC1 de
    // PlayerHouse1 (el byte 0x263D0, dentro de la última habitación).
    uint8_t* p = casa(main, c);
    d[0x263D0] = p[0x263D0];
    std::copy(d.begin(), d.end(), p);
    return "";
}

std::string importarHabitacion(std::vector<uint8_t>& main, int c, int h, const std::vector<uint8_t>& datos)
{
    std::vector<uint8_t> d;
    if (datos.size() == TAM_HABITACION)
        d = datos;
    else if (datos.size() == TAM_HABITACION1)
        d = habitacionNueva(datos.data());
    else
        return "No es una habitación (mide " + std::to_string(datos.size()) + " bytes).";
    std::copy(d.begin(), d.end(), habitacion(main, c, h));
    return "";
}

namespace {
constexpr uint32_t NPC1 = 0x289F8, NPC2 = 0x289F9, BANDERA = 0x28A04;
const uint32_t OBJETOS_CASA[3] = {0x289FC, 0x28A08, 0x28A10}; // DoorDecoItemName, PostItemName, OrderPostItemName
const uint32_t POS_VALOR[] = {0, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x18, NPC1, NPC2, BANDERA};
} // namespace

int64_t valor(const std::vector<uint8_t>& main, int c, Valor v)
{
    const uint8_t* p = casa(main, c) + POS_VALOR[v];
    switch (v)
    {
        case ValorNivel: return nivel(main, c);
        case ValorPrestamo:
        case ValorOtro: return acnh::leerNumero(p);
        case ValorNpc1:
        case ValorNpc2: return (int8_t)p[0];
        case ValorBandera: return p[0] != 0;
        default: return lee16(p);
    }
}

void ponerValor(std::vector<uint8_t>& main, int c, Valor v, int64_t n)
{
    uint8_t* p = casa(main, c) + POS_VALOR[v];
    switch (v)
    {
        case ValorNivel: ponerNivel(main, c, (uint32_t)n); break;
        case ValorPrestamo:
        case ValorOtro: acnh::escribirNumero(p, (uint32_t)n); break;
        case ValorNpc1:
        case ValorNpc2: p[0] = (uint8_t)(int8_t)n; break;
        case ValorBandera: p[0] = n ? 1 : 0; break;
        default: pon16(p, (uint16_t)n); break;
    }
}

Objeto objetoCasa(const std::vector<uint8_t>& main, int c, int cual)
{
    const uint8_t* p = casa(main, c) + OBJETOS_CASA[cual];
    Objeto o;
    o.id = lee16(p);
    o.sistema = p[2];
    o.extra = p[3];
    o.cuenta = lee16(p + 4);
    o.usos = lee16(p + 6);
    return o;
}

void ponerObjetoCasa(std::vector<uint8_t>& main, int c, int cual, const Objeto& o)
{
    uint8_t* p = casa(main, c) + OBJETOS_CASA[cual];
    pon16(p, o.id);
    p[2] = o.sistema;
    p[3] = o.extra;
    pon16(p + 4, o.cuenta);
    pon16(p + 6, o.usos);
}

} // namespace casas
