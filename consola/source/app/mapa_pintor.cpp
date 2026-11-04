// El suelo de la isla pintado como el mapa del teléfono del juego, igual que IslandPainter de NHSE para Mac: mar más
// hondo lejos de la orilla, arena con el borde mojado, hierba más clara en cada nivel con la cara del acantilado y su
// sombra, ríos y estanques, cascadas, rocas y caminos del color de su clase. 16 puntos por cuadro (8 por casilla).
#include "mapa_pintor.hpp"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../core/mapa.hpp"
#include "../core/mapa_terreno.hpp"

namespace mapa {

using namespace acnh;

namespace {

constexpr int P = 16;                       // puntos por cuadro
constexpr int CUADROS_W = 144, CUADROS_H = 128;
constexpr int W = CUADROS_W * P, H = CUADROS_H * P;
constexpr int CELDAS_W = CUADROS_W * 4, CELDAS_H = CUADROS_H * 4;
constexpr uint8_t DINAMICA = 255;

enum K : uint8_t { Mar, Arena, Hierba, Agua, Cascada, CascadaLado, Cara, Roca, Piedra, Madera, Construido, Marea, Hielo, Arenisca };

struct Rgb {
    double r, g, b;
};
uint8_t byteDe(double v) { return (uint8_t)std::clamp(std::nearbyint(v), 0.0, 255.0); }
Rgb rgb(int r, int g, int b) { return {(double)r, (double)g, (double)b}; }
// Como el Mac: cada paso redondea a bytes.
Rgb redondo(Rgb c) { return {(double)byteDe(c.r), (double)byteDe(c.g), (double)byteDe(c.b)}; }
Rgb mul(Rgb c, double f) { return redondo({c.r * f, c.g * f, c.b * f}); }
Rgb mezcla(Rgb a, Rgb b, double t) { return redondo({a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}); }
uint32_t argb(Rgb c) { return 0xFF000000u | (byteDe(c.r) << 16) | (byteDe(c.g) << 8) | byteDe(c.b); }

const Rgb MAR_POCO = rgb(132, 216, 214), MAR_MEDIO = rgb(96, 197, 213), MAR_HONDO = rgb(70, 170, 204), ESPUMA = rgb(236, 250, 244);
const Rgb ARENA = rgb(244, 230, 176), ARENA_MOJADA = rgb(228, 208, 148);
const Rgb HIERBAS[5] = {rgb(112, 178, 82), rgb(132, 194, 96), rgb(153, 207, 113), rgb(172, 218, 130), rgb(188, 226, 146)};
const Rgb CARA_OSCURA = rgb(86, 112, 62), ORILLA = rgb(132, 210, 238), AGUA_HONDA = rgb(80, 172, 226), CASCADA = rgb(196, 238, 250);
const Rgb ROCA = rgb(164, 168, 174), PIEDRA = rgb(200, 196, 186), MADERA = rgb(198, 158, 108), EDIFICIO = rgb(226, 214, 190);
const Rgb MAREA = rgb(118, 204, 222), HIELO = rgb(222, 238, 248), ARENISCA = rgb(206, 182, 128);
const Rgb CAMINOS[9] = {rgb(0, 0, 0), rgb(196, 106, 72), rgb(142, 108, 76), rgb(208, 176, 126), rgb(186, 183, 172), rgb(216, 207, 188),
    rgb(214, 140, 94), rgb(238, 220, 170), rgb(186, 138, 94)};

int hash(int x, int y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (uint32_t)((int32_t)h ^ ((int32_t)h >> 13)) * 1274126177u;
    return (int)(((int32_t)h ^ ((int32_t)h >> 16)) & 0x7FFFFFFF);
}

// Ruido suave en -1..1 (celdas de 12 puntos mezcladas, y un poco de grano).
double ruido(int x, int y)
{
    const int celda = 12;
    const int gx = x / celda, gy = y / celda;
    double fx = (x % celda) / (double)celda, fy = (y % celda) / (double)celda;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto v = [](int a, int b) { return ((hash(a, b) & 1023) / 511.5) - 1; };
    const double arriba = v(gx, gy) + (v(gx + 1, gy) - v(gx, gy)) * fx;
    const double abajo = v(gx, gy + 1) + (v(gx + 1, gy + 1) - v(gx, gy + 1)) * fx;
    const double suave = arriba + (abajo - arriba) * fy;
    const double grano = ((hash(x >> 1, y >> 1) & 255) / 127.5) - 1;
    return suave * 0.8 + grano * 0.2;
}

struct Pieza {
    uint16_t modelo, angulo, camino, altura;
};

struct Terreno {
    const uint8_t* datos;
    const uint8_t* zonas;
    bool dentro(int x, int y) const { return x >= 0 && y >= 0 && x < TERRENO_ANCHO && y < TERRENO_ALTO; }
    Pieza pieza(int x, int y) const
    {
        const uint8_t* p = datos + ((size_t)x * TERRENO_ALTO + y) * 14;
        auto l = [p](int i) { return (uint16_t)(p[i] | (p[i + 1] << 8)); };
        return {l(0), l(4), l(6), l(12)};
    }
    uint16_t plantilla(int x, int y) const
    {
        const int ax = (16 + x) / 16, ay = (16 + y) / 16;
        const uint8_t* p = zonas + (9 * ay + ax) * 2;
        return (uint16_t)(p[0] | (p[1] << 8));
    }
};

int clase(uint16_t m) { return m < TERRENO_MODELOS ? TERRENO_CLASE[m] & 3 : 0; }
int camino(uint16_t m) { return m < TERRENO_MODELOS ? TERRENO_CLASE[m] >> 2 : 0; }
bool esAguaEn(uint16_t m, uint16_t angulo, int px, int py)
{
    if (m >= TERRENO_MODELOS || TERRENO_MASCARA[m] == 255)
        return false;
    const int a = angulo > 3 ? 4 : angulo;
    return (MASCARAS[TERRENO_MASCARA[m]][a * 32 + py * 2 + (px >> 3)] >> (px & 7)) & 1;
}

K deColision(uint8_t c)
{
    switch (c)
    {
        case 0: case 44: case 51: return Hierba;
        case 1: case 12: case 15: return Agua;
        case 3: case 45: return Piedra;
        case 41: case 42: case 70: return Roca;
        case 4: case 29: return Arena;
        case 5: case 46: case 7: return Mar;
        case 6: case 22: case 49: return Madera;
        case 8: case 10: return Construido;
        case 43: return Marea;
        case 47: return Arenisca;
        case 149: case 150: return Hielo;
        default: return Hierba;
    }
}

const std::vector<uint8_t>& bordes()
{
    static std::vector<uint8_t> b;
    if (b.empty())
    {
        b.resize(BORDES_TAM);
        uLongf tam = BORDES_TAM;
        if (uncompress(b.data(), &tam, BORDES_ZLIB, sizeof(BORDES_ZLIB)) != Z_OK)
            std::fill(b.begin(), b.end(), 5);
    }
    return b;
}

std::vector<uint8_t> celdasFijas(const Terreno& t)
{
    const auto& zonas = bordes();
    std::vector<uint8_t> celdas(CELDAS_W * CELDAS_H);
    for (int sy = 0; sy < CUADROS_H; sy++)
        for (int sx = 0; sx < CUADROS_W; sx++)
        {
            const int rx = sx - 16, ry = sy - 16;
            const bool dentro = t.dentro(rx, ry);
            const uint16_t zona = t.plantilla(rx, ry);
            const bool fija = zona != 0 && (size_t)(zona + 1) * 4096 <= zonas.size();
            const int ax = (sx % 16) * 4, ay = (sy % 16) * 4;
            for (int cy = 0; cy < 4; cy++)
                for (int cx = 0; cx < 4; cx++)
                {
                    const uint8_t col = fija ? zonas[(size_t)zona * 4096 + (ay + cy) * 64 + ax + cx] : (uint8_t)(dentro ? 7 : 5);
                    celdas[(sy * 4 + cy) * CELDAS_W + sx * 4 + cx] = col == 7 && dentro ? DINAMICA : (uint8_t)deColision(col);
                }
        }
    return celdas;
}

uint8_t celdaSuave(const std::vector<uint8_t>& c, int x, int y)
{
    const uint8_t propia = c[(y >> 2) * CELDAS_W + (x >> 2)];
    const double fx = ((x + 0.5) / 4) - 0.5, fy = ((y + 0.5) / 4) - 0.5;
    int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    const double tx = fx - x0, ty = fy - y0;
    x0 = std::clamp(x0, 0, CELDAS_W - 2);
    y0 = std::clamp(y0, 0, CELDAS_H - 2);
    const uint8_t a = c[y0 * CELDAS_W + x0], b = c[y0 * CELDAS_W + x0 + 1];
    const uint8_t cc = c[(y0 + 1) * CELDAS_W + x0], d = c[(y0 + 1) * CELDAS_W + x0 + 1];
    if (a == b && b == cc && cc == d)
        return a == DINAMICA ? propia : a;
    if (a == DINAMICA || b == DINAMICA || cc == DINAMICA || d == DINAMICA)
        return propia;
    const double wa = (1 - tx) * (1 - ty), wb = tx * (1 - ty), wc = (1 - tx) * ty, wd = tx * ty;
    uint8_t mejor = propia;
    double peso = -1;
    for (uint8_t k : {a, b, cc, d})
    {
        const double w = (a == k ? wa : 0) + (b == k ? wb : 0) + (cc == k ? wc : 0) + (d == k ? wd : 0);
        if (w > peso + 1e-9)
            mejor = k, peso = w;
    }
    return mejor;
}

bool cascadaVertical(const Terreno& t, int x, int y)
{
    auto mojado = [&](int dx, int dy) {
        if (!t.dentro(x + dx, y + dy))
            return false;
        const int c = clase(t.pieza(x + dx, y + dy).modelo);
        return c == 1 || c == 2;
    };
    return mojado(0, -1) || mojado(0, 1) || !(mojado(-1, 0) || mojado(1, 0));
}

void pintarCamino(const Terreno& t, int rx, int ry, int sx, int sy, uint8_t* camino_)
{
    const double r = 5.5;
    const uint8_t tipo = (uint8_t)camino(t.pieza(rx, ry).camino);
    auto hay = [&](int dx, int dy) { return t.dentro(rx + dx, ry + dy) && camino(t.pieza(rx + dx, ry + dy).camino) != 0; };
    const bool n = hay(0, -1), s = hay(0, 1), w = hay(-1, 0), e = hay(1, 0);
    auto fuera = [r](double x, double y, double cx, double cy) {
        const bool esquina = (cx < 8 ? x < cx : x > cx) && (cy < 8 ? y < cy : y > cy);
        return esquina && ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r);
    };
    for (int py = 0; py < P; py++)
        for (int px = 0; px < P; px++)
        {
            const double fx = px + 0.5, fy = py + 0.5;
            const bool corta = (!n && !w && fuera(fx, fy, r, r)) || (!n && !e && fuera(fx, fy, P - r, r)) ||
                (!s && !w && fuera(fx, fy, r, P - r)) || (!s && !e && fuera(fx, fy, P - r, P - r));
            if (!corta)
                camino_[(sy * P + py) * W + sx * P + px] = tipo;
        }
}

void clasificar(const Terreno& t, uint8_t* tipo, uint8_t* alto, uint8_t* camino_)
{
    const auto celdas = celdasFijas(t);
    for (int sy = 0; sy < CUADROS_H; sy++)
        for (int sx = 0; sx < CUADROS_W; sx++)
        {
            const int rx = sx - 16, ry = sy - 16;
            const bool hay = t.dentro(rx, ry);
            const Pieza pz = hay ? t.pieza(rx, ry) : Pieza{0, 0, 0, 0};
            const int cl = clase(pz.modelo);
            const bool vertical = hay && cl == 2 && cascadaVertical(t, rx, ry);
            const uint8_t e = (uint8_t)std::min<int>(pz.altura, 4);
            for (int py = 0; py < P; py++)
            {
                const int y = sy * P + py;
                for (int px = 0; px < P; px++)
                {
                    const int x = sx * P + px;
                    const int i = y * W + x;
                    const uint8_t celda = celdas[(y >> 2) * CELDAS_W + (x >> 2)];
                    if (celda == DINAMICA && hay)
                    {
                        K k = Hierba;
                        if (cl == 1)
                            k = esAguaEn(pz.modelo, pz.angulo, px, py) ? Agua : Hierba;
                        else if (cl == 2)
                            k = vertical ? Cascada : CascadaLado;
                        else if (cl == 3)
                            k = esAguaEn(pz.modelo, pz.angulo, px, py) ? Hierba : Cara;
                        tipo[i] = k;
                        alto[i] = e;
                    }
                    else
                        tipo[i] = celdaSuave(celdas, x, y);
                }
            }
            if (hay && camino(pz.camino) != 0)
                pintarCamino(t, rx, ry, sx, sy, camino_);
        }
}

// Distancia en puntos (hasta 255) al punto más cercano que cumple (chaflán 3-4 en dos pasadas).
template <class F>
std::vector<uint8_t> distancia(const uint8_t* tipo, F origen)
{
    constexpr uint16_t INF = 60000;
    std::vector<uint16_t> d(W * H);
    for (int i = 0; i < W * H; i++)
        d[i] = origen(tipo[i]) ? 0 : INF;
    auto mn = [](uint16_t v, int o) { return (uint16_t)std::min<int>(v, std::min<int>(o, INF)); };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            const int i = y * W + x;
            uint16_t v = d[i];
            if (!v)
                continue;
            if (x > 0) v = mn(v, d[i - 1] + 3);
            if (y > 0)
            {
                v = mn(v, d[i - W] + 3);
                if (x > 0) v = mn(v, d[i - W - 1] + 4);
                if (x + 1 < W) v = mn(v, d[i - W + 1] + 4);
            }
            d[i] = v;
        }
    for (int y = H - 1; y >= 0; y--)
        for (int x = W - 1; x >= 0; x--)
        {
            const int i = y * W + x;
            uint16_t v = d[i];
            if (!v)
                continue;
            if (x + 1 < W) v = mn(v, d[i + 1] + 3);
            if (y + 1 < H)
            {
                v = mn(v, d[i + W] + 3);
                if (x + 1 < W) v = mn(v, d[i + W + 1] + 4);
                if (x > 0) v = mn(v, d[i + W - 1] + 4);
            }
            d[i] = v;
        }
    std::vector<uint8_t> r(W * H);
    for (int i = 0; i < W * H; i++)
        r[i] = (uint8_t)std::min(255, d[i] / 3);
    return r;
}

std::vector<uint8_t> sombras(const uint8_t* tipo, const uint8_t* alto)
{
    std::vector<uint8_t> s(W * H);
    const int abajo = 7, derecha = 3;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            const int i = y * W + x;
            if (tipo[i] != Cara)
                continue;
            const uint8_t e = alto[i];
            for (int k = 1; k <= abajo && y + k < H; k++)
            {
                const int j = i + k * W;
                if (tipo[j] == Cara || alto[j] >= e)
                    break;
                const uint8_t v = (uint8_t)(255 * (abajo + 1 - k) / (abajo + 1));
                s[j] = std::max(s[j], v);
            }
            for (int k = 1; k <= derecha && x + k < W; k++)
            {
                const int j = i + k;
                if (tipo[j] == Cara || alto[j] >= e)
                    break;
                const uint8_t v = (uint8_t)(160 * (derecha + 1 - k) / (derecha + 1));
                s[j] = std::max(s[j], v);
            }
        }
    return s;
}

Rgb hierba(uint8_t e) { return HIERBAS[std::min<int>(e, 4)]; }
Rgb cara(uint8_t e) { return mezcla(hierba(e), CARA_OSCURA, 0.62); }

Rgb mar(uint8_t d, int x, int y)
{
    if (d <= 1)
        return ESPUMA;
    const double t = std::clamp((d - 2) / 70.0, 0.0, 1.0);
    Rgb c = t < 0.35 ? mezcla(MAR_POCO, MAR_MEDIO, t / 0.35) : mezcla(MAR_MEDIO, MAR_HONDO, (t - 0.35) / 0.65);
    if (d <= 4)
        c = mezcla(c, ESPUMA, (5 - d) * 0.12);
    return mul(c, 1 + ruido(x, y) * 0.012);
}

Rgb arena(uint8_t d, int x, int y)
{
    const Rgb c = d <= 6 ? mezcla(ARENA_MOJADA, ARENA, d / 6.0) : ARENA;
    return mul(c, 1 + ruido(x, y) * 0.03);
}

Rgb agua(uint8_t d, int x, int y) { return mul(mezcla(ORILLA, AGUA_HONDA, std::clamp((d - 1) / 9.0, 0.0, 1.0)), 1 + ruido(x, y) * 0.012); }

Rgb cascada(int x, int y, bool vertical)
{
    const int largo = vertical ? y : x, ancho = vertical ? x : y;
    const bool raya = ((ancho + (hash(ancho >> 2, largo >> 3) & 1)) % 4) == 0;
    const Rgb c = mezcla(ORILLA, CASCADA, 0.7);
    return raya ? mezcla(c, rgb(255, 255, 255), 0.75) : mul(c, 1 + ruido(x, y) * 0.03);
}

Rgb puntoCamino(int p, int x, int y)
{
    const Rgb c = CAMINOS[p];
    switch (p)
    {
        case 1:
        {
            const int fila = y / 4;
            const int col = (x + (fila & 1) * 4) / 8;
            const bool junta = (y % 4) == 0 || ((x + (fila & 1) * 4) % 8) == 0;
            return junta ? mul(c, 1.12) : mul(c, 1 + ((hash(col, fila) & 7) - 3.5) * 0.012);
        }
        case 4:
        {
            const int celda = hash(x / 5, y / 5);
            const bool hueco = (x % 5) == 0 || (y % 5) == 0;
            return hueco ? mul(c, 0.9) : mul(c, 1 + ((celda & 7) - 3.5) * 0.015);
        }
        case 6:
            return (x % 8) == 0 || (y % 8) == 0 ? mul(c, 1.1) : c;
        case 5:
        {
            const int dx = (x % 8) - 4, dy = y % 8;
            const int q = dx * dx + dy * dy;
            return q >= 14 && q <= 22 ? mul(c, 0.9) : c;
        }
        case 8:
            return (y % 4) == 0 ? mul(c, 0.86) : mul(c, 1 + ((hash(x / 6, y / 4) & 7) - 3.5) * 0.012);
        default:
            return mul(c, 1 + ruido(x, y) * 0.04);
    }
}

Rgb contorno(Rgb c, K k, uint8_t e, const uint8_t* tipo, const uint8_t* alto, int i, int x, int y)
{
    if (k == Mar || k == Construido || k == Marea || k == Hielo)
        return c;
    int vecinos[4], n = 0;
    if (x + 1 < W) vecinos[n++] = i + 1;
    if (x > 0) vecinos[n++] = i - 1;
    if (y + 1 < H) vecinos[n++] = i + W;
    if (y > 0) vecinos[n++] = i - W;
    bool orilla = false, bordeRoca = false, bordePiedra = false, labio = false, bordeHierba = false, pie = false, bordeArena = false;
    for (int a = 0; a < n; a++)
    {
        const K nk = (K)tipo[vecinos[a]];
        const uint8_t ne = alto[vecinos[a]];
        orilla |= nk != Agua && nk != Cascada && nk != CascadaLado;
        bordeRoca |= nk != Roca;
        bordePiedra |= nk != Piedra && nk != Roca;
        labio |= nk == Cara && ne == e;
        bordeHierba |= nk == Arena || nk == Agua;
        pie |= nk != Cara && ne < e;
        bordeArena |= nk == Hierba || nk == Roca || nk == Piedra;
    }
    if (k == Agua && orilla) return mul(c, 0.8);
    if (k == Roca && bordeRoca) return mul(c, 0.72);
    if (k == Piedra && bordePiedra) return mul(c, 0.86);
    if (k == Hierba && labio) return mul(c, 1.1);
    if (k == Hierba && bordeHierba) return mul(c, 0.9);
    if (k == Cara && pie) return mul(c, 0.78);
    if (k == Arena && bordeArena) return mul(c, 0.94);
    return c;
}

} // namespace

std::vector<uint32_t> pintarSuelo(const std::vector<uint8_t>& main)
{
    Terreno t{main.data() + TERRENO, main.data() + ZONAS};
    std::vector<uint8_t> tipo(W * H), alto(W * H), cam(W * H);
    clasificar(t, tipo.data(), alto.data(), cam.data());
    const auto dMar = distancia(tipo.data(), [](uint8_t k) { return k != Mar && k != Marea; });
    const auto dCosta = distancia(tipo.data(), [](uint8_t k) { return k == Mar; });
    const auto dAgua = distancia(tipo.data(), [](uint8_t k) { return k != Agua; });
    const auto sombra = sombras(tipo.data(), alto.data());

    std::vector<uint32_t> res(W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            const int i = y * W + x;
            const K k = (K)tipo[i];
            const uint8_t e = alto[i];
            Rgb c;
            const int p = cam[i];
            if (p)
            {
                c = puntoCamino(p, x, y);
                const bool distinto = (x + 1 < W && cam[i + 1] != p) || (x > 0 && cam[i - 1] != p) || (y + 1 < H && cam[i + W] != p) || (y > 0 && cam[i - W] != p);
                if (distinto)
                    c = mul(c, 0.84);
            }
            else
            {
                switch (k)
                {
                    case Mar: c = mar(dMar[i], x, y); break;
                    case Arena: c = arena(dCosta[i], x, y); break;
                    case Hierba: c = mul(hierba(e), 1 + ruido(x, y) * 0.035); break;
                    case Agua: c = agua(dAgua[i], x, y); break;
                    case Cascada: c = cascada(x, y, true); break;
                    case CascadaLado: c = cascada(x, y, false); break;
                    case Cara: c = mul(cara(e), 1 + ruido(x, y) * 0.04); break;
                    case Roca: c = mul(ROCA, 1 + ruido(x, y) * 0.05); break;
                    case Piedra: c = mul(PIEDRA, 1 + ruido(x, y) * 0.04); break;
                    case Madera: c = mul(MADERA, (y & 3) == 0 ? 0.88 : 1); break;
                    case Construido: c = EDIFICIO; break;
                    case Marea: c = MAREA; break;
                    case Hielo: c = HIELO; break;
                    default: c = mul(ARENISCA, 1 + ruido(x, y) * 0.05); break;
                }
                c = contorno(c, k, e, tipo.data(), alto.data(), i, x, y);
            }
            if (sombra[i])
                c = mul(c, 1 - sombra[i] / 255.0 * 0.22);
            res[i] = argb(c);
        }
    return res;
}

uint32_t colorMar() { return argb(MAR_HONDO); }

} // namespace mapa
