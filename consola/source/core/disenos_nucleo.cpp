#include "disenos.hpp"
#include "acnh.hpp"
#include <zlib.h>
#include <algorithm>
#include <cstring>

namespace disenos {

namespace {
constexpr int NOMBRE = 0x10;     // 20 UTF-16 letters
constexpr int ORIGEN = 0x38;     // town id (4) + town name (20 bytes); then the player id (4) + name (20) at +0x1C
constexpr int USO = 0x70;
constexpr int PALETA = 0x78;
constexpr int PIXELES = PALETA + COLORES * 3; // 0xA5
constexpr int HOJA = LADO * LADO / 2;           // bytes per sheet (two pixels per byte)

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
void pon32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
void pon32be(std::vector<uint8_t>& o, uint32_t v)
{
    for (int i = 3; i >= 0; i--)
        o.push_back((uint8_t)(v >> (8 * i)));
}

std::string utf16(const uint8_t* p, int max)
{
    std::string s;
    for (int i = 0; i < max; i++)
    {
        uint32_t c = p[2 * i] | (p[2 * i + 1] << 8);
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < max)
        {
            const uint32_t c2 = p[2 * i + 2] | (p[2 * i + 3] << 8);
            if (c2 >= 0xDC00 && c2 < 0xE000)
                c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00), i++;
        }
        if (c < 0x80)
            s += (char)c;
        else if (c < 0x800)
            s += (char)(0xC0 | (c >> 6)), s += (char)(0x80 | (c & 0x3F));
        else if (c < 0x10000)
            s += (char)(0xE0 | (c >> 12)), s += (char)(0x80 | ((c >> 6) & 0x3F)), s += (char)(0x80 | (c & 0x3F));
        else
            s += (char)(0xF0 | (c >> 18)), s += (char)(0x80 | ((c >> 12) & 0x3F)), s += (char)(0x80 | ((c >> 6) & 0x3F)),
                s += (char)(0x80 | (c & 0x3F));
    }
    return s;
}

std::vector<uint16_t> aUtf16(const std::string& s)
{
    std::vector<uint16_t> r;
    for (size_t i = 0; i < s.size();)
    {
        const uint8_t c = s[i];
        uint32_t u;
        int n;
        if (c < 0x80)
            u = c, n = 1;
        else if ((c & 0xE0) == 0xC0)
            u = c & 0x1F, n = 2;
        else if ((c & 0xF0) == 0xE0)
            u = c & 0x0F, n = 3;
        else
            u = c & 0x07, n = 4;
        for (int k = 1; k < n && i + k < s.size(); k++)
            u = (u << 6) | (s[i + k] & 0x3F);
        i += n;
        if (u >= 0x10000)
        {
            u -= 0x10000;
            r.push_back((uint16_t)(0xD800 + (u >> 10)));
            r.push_back((uint16_t)(0xDC00 + (u & 0x3FF)));
        }
        else
            r.push_back((uint16_t)u);
    }
    return r;
}

// SHA-1 (para reconocer el hueco sin usar del juego, como la app del Mac).
std::vector<uint8_t> sha1(const uint8_t* datos, size_t n)
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<uint8_t> m(datos, datos + n);
    m.push_back(0x80);
    while (m.size() % 64 != 56)
        m.push_back(0);
    const uint64_t bits = (uint64_t)n * 8;
    for (int i = 7; i >= 0; i--)
        m.push_back((uint8_t)(bits >> (8 * i)));
    auto rot = [](uint32_t x, int k) { return (x << k) | (x >> (32 - k)); };
    for (size_t b = 0; b < m.size(); b += 64)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (m[b + 4 * i] << 24) | (m[b + 4 * i + 1] << 16) | (m[b + 4 * i + 2] << 8) | m[b + 4 * i + 3];
        for (int i = 16; i < 80; i++)
            w[i] = rot(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++)
        {
            uint32_t f, k;
            if (i < 20)
                f = (bb & c) | (~bb & d), k = 0x5A827999;
            else if (i < 40)
                f = bb ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60)
                f = (bb & c) | (bb & d) | (c & d), k = 0x8F1BBCDC;
            else
                f = bb ^ c ^ d, k = 0xCA62C1D6;
            const uint32_t t = rot(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rot(bb, 30), bb = a, a = t;
        }
        h[0] += a, h[1] += bb, h[2] += c, h[3] += d, h[4] += e;
    }
    std::vector<uint8_t> r;
    for (uint32_t x : h)
        pon32be(r, x);
    return r;
}

// ReplaceOccurrences de NHSE: cambia cada aparición de «buscar» por «poner», de izquierda a derecha.
void reemplazar(std::vector<uint8_t>& d, const std::vector<uint8_t>& buscar, const uint8_t* poner)
{
    if (std::equal(buscar.begin(), buscar.end(), poner))
        return;
    auto it = d.begin();
    while (true)
    {
        it = std::search(it, d.end(), buscar.begin(), buscar.end());
        if (it == d.end())
            return;
        std::copy(poner, poner + buscar.size(), it);
        it += buscar.size();
    }
}
} // namespace

int cuantos(Tipo t) { return t == Normales || t == Pro ? 100 : t == Bandera ? 1 : 8; }
bool esPro(Tipo t) { return t == Pro || t == Sastreria; }
int tamano(Tipo t) { return esPro(t) ? TAM_PRO : TAM_NORMAL; }
int hojas(Tipo t) { return esPro(t) ? 4 : 1; }
const char* extension(Tipo t) { return esPro(t) ? ".nhpd" : ".nhd"; }

uint32_t posicion(Tipo t, int i)
{
    switch (t)
    {
    case Normales: return NORMALES + i * TAM_NORMAL;
    case Pro: return PROS + i * TAM_PRO;
    case Bandera: return BANDERA;
    default: return SASTRERIA + i * TAM_PRO;
    }
}

std::vector<uint8_t> leer(const std::vector<uint8_t>& main, Tipo t, int i)
{
    const uint32_t p = posicion(t, i);
    return std::vector<uint8_t>(main.begin() + p, main.begin() + p + tamano(t));
}

void escribir(std::vector<uint8_t>& main, Tipo t, int i, const std::vector<uint8_t>& d)
{
    std::copy(d.begin(), d.begin() + tamano(t), main.begin() + posicion(t, i));
    if (t == Normales || t == Pro)
        std::fill_n(main.begin() + (t == Normales ? EDITADOS : EDITADOS_PRO), cuantos(t), 0);
}

std::string nombre(const std::vector<uint8_t>& d) { return utf16(d.data() + NOMBRE, 20); }

void ponerNombre(std::vector<uint8_t>& d, const std::string& utf8)
{
    auto u = aUtf16(utf8);
    u.resize(20, 0); // StringUtil.GetBytes: cut to 20, or padded with zeros
    for (int i = 0; i < 20; i++)
        d[NOMBRE + 2 * i] = (uint8_t)u[i], d[NOMBRE + 2 * i + 1] = (uint8_t)(u[i] >> 8);
}

std::string pueblo(const std::vector<uint8_t>& d) { return utf16(d.data() + ORIGEN + 4, 10); }
std::string autor(const std::vector<uint8_t>& d) { return utf16(d.data() + ORIGEN + 0x20, 10); }
uint32_t idAutor(const std::vector<uint8_t>& d) { return le32(d.data() + ORIGEN + 0x1C); }
uint32_t uso(const std::vector<uint8_t>& d) { return le32(d.data() + USO); }
bool deJuego(const std::vector<uint8_t>& d) { return idAutor(d) == 0xFFFFFFFF; }

bool vacio(const std::vector<uint8_t>& d, Tipo t)
{
    if (!deJuego(d))
        return false;
    // Start of the SHA-1 of the colours and pixels of the game's blank slot (DesignSlot.BlankHash of the Mac).
    static const uint8_t normal[6] = {0xF6, 0xB0, 0x81, 0xA9, 0x47, 0xA1};
    static const uint8_t pro[6] = {0x04, 0x47, 0x13, 0x15, 0x69, 0xD5};
    const auto h = sha1(d.data() + PALETA, d.size() - PALETA);
    return std::equal(h.begin(), h.begin() + 6, esPro(t) ? pro : normal);
}

void color(const std::vector<uint8_t>& d, int i, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = d[PALETA + 3 * i], g = d[PALETA + 3 * i + 1], b = d[PALETA + 3 * i + 2];
}

int pixel(const std::vector<uint8_t>& d, int hoja, int x, int y)
{
    const int i = y * LADO + x;
    const uint8_t v = d[PIXELES + hoja * HOJA + i / 2];
    return (i & 1) == 0 ? (v & 0x0F) : (v >> 4);
}

bool esDe(const std::vector<uint8_t>& d, const std::vector<uint8_t>& personal)
{
    const uint8_t* yo = personal.data() + acnh::ID_JUGADOR;
    return std::equal(yo, yo + 24, d.data() + ORIGEN) && std::equal(yo + 0x1C, yo + 0x1C + 24, d.data() + ORIGEN + 0x1C);
}

void hacerMio(std::vector<uint8_t>& d, const std::vector<uint8_t>& personal)
{
    const uint8_t* yo = personal.data() + acnh::ID_JUGADOR;
    const std::vector<uint8_t> isla(d.begin() + ORIGEN, d.begin() + ORIGEN + 24);
    reemplazar(d, isla, yo);
    const std::vector<uint8_t> jugador(d.begin() + ORIGEN + 0x1C, d.begin() + ORIGEN + 0x1C + 24);
    reemplazar(d, jugador, yo + 0x1C);
}

void arreglarUso(std::vector<uint8_t>& d, Tipo t)
{
    const uint32_t u = uso(d);
    if (esPro(t))
    {
        if (u != 0xEE01 && u != 0xEE05) // 01 = pro, 05 = sin usar
            pon32(d.data() + USO, 0xEE01);
        return;
    }
    if (u == 0xEE00 || u == 0xEE02) // 00 = opaco, 02 = con transparencia
        return;
    bool transparente = false;
    for (int y = 0; y < LADO && !transparente; y++)
        for (int x = 0; x < LADO && !transparente; x++)
            transparente = pixel(d, 0, x, y) == COLORES;
    pon32(d.data() + USO, transparente ? 0xEE02 : 0xEE00);
}

bool blanco(const std::vector<uint8_t>& main, Tipo t, std::vector<uint8_t>& d)
{
    const Tipo de = esPro(t) ? Pro : Normales;
    for (int i = 0; i < cuantos(de); i++)
    {
        d = leer(main, de, i);
        if (vacio(d, de))
            return true;
    }
    d.clear();
    return false;
}

std::vector<uint8_t> rgba(const std::vector<uint8_t>& d, Tipo t, int escala, int& ancho, int& alto)
{
    const int lado = esPro(t) ? 2 * LADO : LADO;
    ancho = alto = lado * escala;
    std::vector<uint8_t> o((size_t)ancho * alto * 4, 0);
    for (int h = 0; h < hojas(t); h++)
    {
        const int ox = (h % 2) * LADO, oy = (h / 2) * LADO;
        for (int y = 0; y < LADO; y++)
            for (int x = 0; x < LADO; x++)
            {
                const int c = pixel(d, h, x, y);
                if (c >= COLORES)
                    continue;
                const uint8_t* p = d.data() + PALETA + 3 * c;
                for (int dy = 0; dy < escala; dy++)
                {
                    uint8_t* q = o.data() + (((size_t)(oy + y) * escala + dy) * ancho + (size_t)(ox + x) * escala) * 4;
                    for (int dx = 0; dx < escala; dx++, q += 4)
                        q[0] = p[0], q[1] = p[1], q[2] = p[2], q[3] = 255;
                }
            }
    }
    return o;
}

std::vector<uint8_t> png(const std::vector<uint8_t>& d, Tipo t)
{
    int w, h;
    const auto px = rgba(d, t, esPro(t) ? 4 : 8, w, h);
    std::vector<uint8_t> filas;
    filas.reserve((size_t)h * (w * 4 + 1));
    for (int y = 0; y < h; y++)
    {
        filas.push_back(0); // no filter
        filas.insert(filas.end(), px.begin() + (size_t)y * w * 4, px.begin() + (size_t)(y + 1) * w * 4);
    }
    uLongf largo = compressBound(filas.size());
    std::vector<uint8_t> z(largo);
    compress2(z.data(), &largo, filas.data(), filas.size(), 9);
    z.resize(largo);

    std::vector<uint8_t> o = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    auto trozo = [&o](const char* tipo, const std::vector<uint8_t>& datos) {
        pon32be(o, (uint32_t)datos.size());
        const size_t inicio = o.size();
        o.insert(o.end(), tipo, tipo + 4);
        o.insert(o.end(), datos.begin(), datos.end());
        pon32be(o, (uint32_t)crc32(0, o.data() + inicio, (uInt)(o.size() - inicio)));
    };
    std::vector<uint8_t> cab;
    pon32be(cab, (uint32_t)w);
    pon32be(cab, (uint32_t)h);
    cab.insert(cab.end(), {8, 6, 0, 0, 0}); // 8 bits, RGBA
    trozo("IHDR", cab);
    trozo("IDAT", z);
    trozo("IEND", {});
    return o;
}

std::string nombreArchivo(const std::vector<uint8_t>& d, Tipo t, int numero)
{
    std::string n = nombre(d);
    std::string limpio;
    for (char c : n)
        if (!strchr("\\/:*?\"<>|", c) && (unsigned char)c >= 0x20)
            limpio += c;
    while (!limpio.empty() && (limpio.back() == ' ' || limpio.back() == '.'))
        limpio.pop_back();
    if (limpio.empty())
        limpio = "(sin nombre)";
    if (numero >= 0)
    {
        char p[8];
        snprintf(p, sizeof(p), "%02d - ", numero);
        limpio = p + limpio;
    }
    return limpio + extension(t);
}

} // namespace disenos
