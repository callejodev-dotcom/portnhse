#include "acnh.hpp"
#include "tablas.hpp"
#include "vecinos.hpp"
#include "objetos.hpp"
#include "listas.hpp"
#include <zlib.h>
#include <algorithm>
#include <cstring>

namespace acnh {

// ------------------------------------------------------------------------------------------------- cifrado (AES-CTR)
// De cbreezy210/ACNH-Save-Editor (MIT): la clave y el contador salen de la cabecera con un XorShift128.
namespace {
struct XorShift128 {
    uint32_t a, b, c, d;
    explicit XorShift128(uint32_t seed)
    {
        const uint32_t m = 0x6C078965;
        a = (m * (seed ^ (seed >> 30))) + 1;
        b = (m * (a ^ (a >> 30))) + 2;
        c = (m * (b ^ (b >> 30))) + 3;
        d = (m * (c ^ (c >> 30))) + 4;
    }
    uint32_t next()
    {
        uint32_t t = a;
        a = b;
        b = c;
        c = d;
        t ^= t << 11;
        t ^= t >> 8;
        return d = t ^ d ^ (d >> 19);
    }
    void next64()
    {
        next();
        next();
    }
};

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void put32(uint8_t* p, uint32_t v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}
void put16(uint8_t* p, uint16_t v)
{
    p[0] = v;
    p[1] = v >> 8;
}

void getParam(const uint8_t* importantes, int index, uint8_t* result)
{
    auto data = [&](uint32_t i) { return le32(importantes + 4 * i); };
    XorShift128 rand(data(data(index) & 0x7F));
    const uint32_t prms = data(data(index + 1) & 0x7F) & 0x7F;
    for (uint32_t i = 0; i < (prms & 0xF) + 1; i++)
        rand.next64();
    for (int i = 0; i < 16; i++)
        result[i] = (uint8_t)(rand.next() >> 24);
}

const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9,
    0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f,
    0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07,
    0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3,
    0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58,
    0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3,
    0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f,
    0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac,
    0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a,
    0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70,
    0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42,
    0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};
const uint8_t rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b)); }

void expandirClave(const uint8_t* key, uint8_t* rk)
{
    memcpy(rk, key, 16);
    for (int i = 4; i < 44; i++)
    {
        uint8_t t[4];
        memcpy(t, rk + (i - 1) * 4, 4);
        if (i % 4 == 0)
        {
            uint8_t q = t[0];
            t[0] = sbox[t[1]] ^ rcon[i / 4];
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[q];
        }
        for (int j = 0; j < 4; j++)
            rk[i * 4 + j] = rk[(i - 4) * 4 + j] ^ t[j];
    }
}

void desplazarFilas(uint8_t* s)
{
    uint8_t t;
    t = s[1], s[1] = s[5], s[5] = s[9], s[9] = s[13], s[13] = t;
    t = s[2], s[2] = s[10], s[10] = t, t = s[6], s[6] = s[14], s[14] = t;
    t = s[15], s[15] = s[11], s[11] = s[7], s[7] = s[3], s[3] = t;
}

void cifrarBloque(const uint8_t* rk, const uint8_t* in, uint8_t* out)
{
    uint8_t s[16];
    for (int i = 0; i < 16; i++)
        s[i] = in[i] ^ rk[i];
    for (int r = 1; r < 10; r++)
    {
        for (auto& b : s)
            b = sbox[b];
        desplazarFilas(s);
        for (int c = 0; c < 4; c++)
        {
            uint8_t a = s[c * 4], b = s[c * 4 + 1], d = s[c * 4 + 2], e = s[c * 4 + 3], x = a ^ b ^ d ^ e;
            s[c * 4] ^= x ^ xtime(a ^ b);
            s[c * 4 + 1] ^= x ^ xtime(b ^ d);
            s[c * 4 + 2] ^= x ^ xtime(d ^ e);
            s[c * 4 + 3] ^= x ^ xtime(e ^ a);
        }
        for (int i = 0; i < 16; i++)
            s[i] ^= rk[r * 16 + i];
    }
    for (auto& b : s)
        b = sbox[b];
    desplazarFilas(s);
    for (int i = 0; i < 16; i++)
        out[i] = s[i] ^ rk[160 + i];
}
} // namespace

void cifrar(const std::vector<uint8_t>& cabecera, std::vector<uint8_t>& datos)
{
    uint8_t key[16], ctr[16], rk[176], ek[16];
    getParam(cabecera.data() + 0x100, 0, key);
    getParam(cabecera.data() + 0x100, 2, ctr);
    expandirClave(key, rk);
    for (size_t off = 0; off < datos.size(); off += 16)
    {
        cifrarBloque(rk, ctr, ek);
        const size_t n = std::min<size_t>(16, datos.size() - off);
        for (size_t i = 0; i < n; i++)
            datos[off + i] ^= ek[i];
        for (int i = 15; i >= 0; i--)
            if (++ctr[i] != 0)
                break;
    }
}

// ---------------------------------------------------------------------------------------------- comprobaciones (Murmur3)
// Las regiones de la 3.0.0 de NHSE (FileHashRevision.REV_300): el valor va en la posición y cubre los «largo» bytes
// que siguen a él.
namespace {
struct Region {
    uint32_t pos, largo;
};
const Region REGIONES_MAIN[] = {
    {0x000110, 0x1e339c}, {0x1e34b0, 0x3d089c}, {0x5b3e60, 0x037acc}, {0x5eb930, 0x03ce5c}, {0x6288a0, 0x037acc},
    {0x660370, 0x03ce5c}, {0x69d2e0, 0x037acc}, {0x6d4db0, 0x03ce5c}, {0x711d20, 0x037acc}, {0x7497f0, 0x03ce5c},
    {0x786760, 0x037acc}, {0x7be230, 0x03ce5c}, {0x7fb1a0, 0x037acc}, {0x832c70, 0x03ce5c}, {0x86fbe0, 0x037acc},
    {0x8a76b0, 0x03ce5c}, {0x8e4620, 0x037acc}, {0x91c0f0, 0x03ce5c}, {0x958f50, 0x057f3c},
};
const Region REGIONES_PERSONAL[] = {{0x00110, 0x37acc}, {0x37be0, 0x3ce5c}};
const Region REGIONES_BUZON[] = {{0x100, 0x4732c}};

uint32_t murmur3(const uint8_t* data, size_t len)
{
    uint32_t c = 0;
    for (size_t i = 0; i + 4 <= len; i += 4)
    {
        uint32_t v = le32(data + i);
        v = (v * 0x16A88000) | ((v * 0xCC9E2D51) >> 17);
        v *= 0x1B873593;
        c ^= v;
        c = (c >> 19) | (c << 13);
        c = (c * 5) + 0xE6546B64;
    }
    c ^= (uint32_t)len;
    c ^= c >> 16;
    c *= 0x85EBCA6B;
    c ^= c >> 13;
    c *= 0xC2B2AE35;
    c ^= c >> 16;
    return c;
}

bool regiones(size_t tam, const Region*& r, size_t& n)
{
    if (tam == TAM_MAIN)
        r = REGIONES_MAIN, n = sizeof(REGIONES_MAIN) / sizeof(Region);
    else if (tam == TAM_PERSONAL)
        r = REGIONES_PERSONAL, n = sizeof(REGIONES_PERSONAL) / sizeof(Region);
    else if (tam == TAM_BUZON)
        r = REGIONES_BUZON, n = 1;
    else
        return false;
    return true;
}
} // namespace

bool arreglarComprobaciones(std::vector<uint8_t>& datos)
{
    const Region* r;
    size_t n;
    if (!regiones(datos.size(), r, n))
        return false;
    for (size_t i = 0; i < n; i++)
        put32(datos.data() + r[i].pos, murmur3(datos.data() + r[i].pos + 4, r[i].largo));
    return true;
}

int comprobacionesMal(const std::vector<uint8_t>& datos)
{
    const Region* r;
    size_t n;
    if (!regiones(datos.size(), r, n))
        return -1;
    int mal = 0;
    for (size_t i = 0; i < n; i++)
        mal += le32(datos.data() + r[i].pos) != murmur3(datos.data() + r[i].pos + 4, r[i].largo);
    return mal;
}

// ------------------------------------------------------------------------------------------------- números cifrados
uint32_t leerNumero(const uint8_t* p)
{
    const uint32_t enc = le32(p);
    const uint16_t adjust = le16(p + 4);
    const uint8_t shift = (p[6] + 3) & 31;
    const uint32_t rot = (enc >> shift) | (enc << ((32 - shift) & 31));
    return rot + 0x80E32B11u - adjust;
}

void escribirNumero(uint8_t* p, uint32_t valor)
{
    const uint16_t adjust = le16(p + 4);
    const uint8_t shift = (p[6] + 3) & 31;
    const uint32_t v = valor + adjust - 0x80E32B11u;
    const uint32_t enc = (v << shift) | (v >> ((32 - shift) & 31));
    put32(p, enc);
    p[7] = (uint8_t)(enc + (enc >> 16) + (enc >> 24) + (enc >> 8) - 0x2D);
}

// ------------------------------------------------------------------------------------------------------- el suelo
bool esFlor(uint16_t id) { return std::find(std::begin(FLORES), std::end(FLORES), id) != std::end(FLORES); }
bool esMalaHierba(uint16_t id)
{
    return std::find(std::begin(MALAS_HIERBAS), std::end(MALAS_HIERBAS), id) != std::end(MALAS_HIERBAS);
}

namespace {
uint8_t* casilla(std::vector<uint8_t>& main, int x, int y) { return main.data() + CAMPO + 8 * (x * ALTO + y); }

// Lo que ocupa una planta: la casilla de arriba a la izquierda y las 3 que la apuntan («extensiones»).
template <typename F> void plantas(std::vector<uint8_t>& main, F f)
{
    for (int x = 0; x < ANCHO; x++)
        for (int y = 0; y < ALTO; y++)
        {
            uint8_t* t = casilla(main, x, y);
            const uint16_t id = le16(t);
            if (id < EXTENSION)
                f(x, y, id);
        }
}

// Las casillas de una cosa de 2×2 que apuntan a la de (x, y) y llevan ese objeto.
template <typename F> void extensiones(std::vector<uint8_t>& main, int x, int y, uint16_t id, F f)
{
    for (int dx = 0; dx < 2; dx++)
        for (int dy = 0; dy < 2; dy++)
        {
            if ((dx == 0 && dy == 0) || x + dx >= ANCHO || y + dy >= ALTO)
                continue;
            uint8_t* t = casilla(main, x + dx, y + dy);
            if (le16(t) == EXTENSION && le16(t + 4) == id && t[6] == dx && t[7] == dy)
                f(t);
        }
}
} // namespace

Recuento contar(const std::vector<uint8_t>& constMain)
{
    auto& main = const_cast<std::vector<uint8_t>&>(constMain);
    Recuento r;
    plantas(main, [&](int, int, uint16_t id) {
        r.flores += esFlor(id);
        r.malasHierbas += esMalaHierba(id);
    });
    return r;
}

int cambiarFlores(std::vector<uint8_t>& main, const std::vector<uint16_t>& nuevas, bool alAzar, uint32_t semilla)
{
    if (nuevas.empty())
        return 0;
    XorShift128 azar(semilla);
    int n = 0;
    plantas(main, [&](int x, int y, uint16_t id) {
        if (!esFlor(id))
            return;
        const uint16_t nueva = alAzar ? nuevas[azar.next() % nuevas.size()] : nuevas[((x / 2) + (y / 2)) % nuevas.size()];
        if (nueva == id && nuevas.size() == 1)
            return;
        // Como NHSE: la flor nueva sin más datos, y sus extensiones apuntándola.
        uint8_t* t = casilla(main, x, y);
        memset(t, 0, 8);
        put16(t, nueva);
        extensiones(main, x, y, id, [&](uint8_t* e) {
            put16(e + 4, nueva);
            e[2] = e[3] = 0;
        });
        n++;
    });
    return n;
}

int quitarMalasHierbas(std::vector<uint8_t>& main)
{
    int n = 0;
    plantas(main, [&](int x, int y, uint16_t id) {
        if (!esMalaHierba(id))
            return;
        extensiones(main, x, y, id, [&](uint8_t* e) {
            memset(e, 0, 8);
            put16(e, NADA);
        });
        uint8_t* t = casilla(main, x, y);
        memset(t, 0, 8);
        put16(t, NADA);
        n++;
    });
    return n;
}

// ---------------------------------------------------------------------------------------------------------- vecinos
std::string vecinoEn(const std::vector<uint8_t>& main, int hueco)
{
    const uint8_t* v = main.data() + POS_VECINOS + hueco * TAM_VECINO;
    const int especies = sizeof(ESPECIES) / sizeof(ESPECIES[0]);
    if (v[0] >= especies || !strcmp(ESPECIES[v[0]], "non") || !ESPECIES[v[0]][0])
        return "";
    char nombre[16];
    snprintf(nombre, sizeof(nombre), "%s%02d", ESPECIES[v[0]], v[1]);
    return nombre;
}

std::string cambiarVecino(std::vector<uint8_t>& main, int hueco, const std::string& interno, const std::vector<uint8_t>& bin)
{
    if (hueco < 0 || hueco >= NUM_VECINOS || bin.size() < 8 || memcmp(bin.data(), "VECI", 4))
        return "Faltan los datos de los vecinos.";
    int casa = -1;
    for (int j = 0; j < NUM_VECINOS && casa < 0; j++)
        if ((int8_t)main[CASAS + j * TAM_CASA + 0x1C4] == hueco)
            casa = j;
    if (casa < 0)
        return "Ese vecino no tiene casa.";

    const uint32_t n = le32(bin.data() + 4);
    std::vector<uint8_t> nuevo(TAM_VECINO + TAM_CASA);
    bool hallado = false;
    for (uint32_t i = 0; i < n && !hallado; i++)
    {
        const uint8_t* e = bin.data() + 8 + i * 16;
        if (strncmp((const char*)e, interno.c_str(), 8) != 0)
            continue;
        uLongf tam = nuevo.size();
        if (uncompress(nuevo.data(), &tam, bin.data() + le32(e + 8), le32(e + 12)) != Z_OK || tam != nuevo.size())
            return "Los datos de ese vecino están mal.";
        hallado = true;
    }
    if (!hallado)
        return "No tengo los datos de ese vecino.";

    // Como NHSE (VillagerSwap): el recuerdo del jugador del vecino anterior, salvo su identificador (los primeros
    // 0x38 bytes), y la casa nueva con quién vive en ella y quién la construyó.
    uint8_t* v = main.data() + POS_VECINOS + hueco * TAM_VECINO;
    uint8_t* c = main.data() + CASAS + casa * TAM_CASA;
    memcpy(nuevo.data() + 4 + 0x38, v + 4 + 0x38, 0x5F0 - 0x38);
    uint8_t* nc = nuevo.data() + TAM_VECINO;
    nc[0x1C4] = c[0x1C4];
    nc[0x1C5] = c[0x1C5];
    nc[0x1D0] = c[0x1D0];
    memcpy(v, nuevo.data(), TAM_VECINO);
    memcpy(c, nc, TAM_CASA);
    return "";
}

int mejoresAmigos(std::vector<uint8_t>& main)
{
    int n = 0;
    for (int i = 0; i < NUM_VECINOS; i++)
    {
        if (vecinoEn(main, i).empty())
            continue;
        for (int j = 0; j < 8; j++)
        {
            uint8_t* m = main.data() + POS_VECINOS + i * TAM_VECINO + 4 + j * 0x5F0;
            // Como NHSE: los recuerdos con nombre de jugador (los demás están vacíos).
            bool conNombre = false;
            for (int k = 0; k < 10; k++)
            {
                const uint16_t c = le16(m + 0x20 + 2 * k);
                if (c == 0)
                    break;
                if (c != ' ' && c != '\t' && c != 0x3000)
                    conNombre = true;
            }
            if (conNombre && m[0x38 + 10] != 255)
                m[0x38 + 10] = 255, n++;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------------------------------------- jugador
namespace {
std::string utf16(const uint8_t* p, int max)
{
    std::string s;
    for (int i = 0; i < max; i++)
    {
        uint16_t c = le16(p + 2 * i);
        if (c == 0)
            break;
        if (c < 0x80)
            s += (char)c;
        else if (c < 0x800)
            s += (char)(0xC0 | (c >> 6)), s += (char)(0x80 | (c & 0x3F));
        else
            s += (char)(0xE0 | (c >> 12)), s += (char)(0x80 | ((c >> 6) & 0x3F)), s += (char)(0x80 | (c & 0x3F));
    }
    return s;
}

uint32_t posBolsillo(int hueco) { return hueco < 20 ? BOLSILLOS2 + 8 * hueco : BOLSILLOS1 + 8 * (hueco - 20); }

bool bit(const std::vector<uint8_t>& d, uint32_t pos, int n) { return (d[pos + (n >> 3)] >> (n & 7)) & 1; }
void ponerBit(std::vector<uint8_t>& d, uint32_t pos, int n) { d[pos + (n >> 3)] |= (uint8_t)(1 << (n & 7)); }
} // namespace

std::string nombreIsla(const std::vector<uint8_t>& p) { return utf16(p.data() + ID_JUGADOR + 4, 10); }
std::string nombreJugador(const std::vector<uint8_t>& p) { return utf16(p.data() + ID_JUGADOR + 0x20, 10); }

int bolsillosAbiertos(const std::vector<uint8_t>& p, int parte)
{
    const uint32_t n = le32(p.data() + (parte == 0 ? BOLSILLOS2 : BOLSILLOS1) + 20 * 8);
    return (int)std::min<uint32_t>(n, 20);
}

bool bolsilloAbierto(const std::vector<uint8_t>& p, int hueco)
{
    return hueco >= 0 && hueco < NUM_BOLSILLOS && hueco % 20 < bolsillosAbiertos(p, hueco / 20);
}

Objeto leerBolsillo(const std::vector<uint8_t>& p, int hueco)
{
    const uint8_t* d = p.data() + posBolsillo(hueco);
    Objeto o;
    o.id = le16(d);
    o.sistema = d[2];
    o.extra = d[3];
    o.cuenta = le16(d + 4);
    o.usos = le16(d + 6);
    return o;
}

void escribirBolsillo(std::vector<uint8_t>& p, int hueco, const Objeto& o)
{
    uint8_t* d = p.data() + posBolsillo(hueco);
    put16(d, o.id);
    d[2] = o.sistema;
    d[3] = o.extra;
    put16(d + 4, o.cuenta);
    put16(d + 6, o.usos);
}

const char* const PAPELES[16] = {"Amarillo", "Rosa", "Naranja", "Verde claro", "Verde", "Menta", "Azul claro", "Morado",
    "Azul marino", "Azul", "Blanco", "Rojo", "Dorado", "Marrón", "Gris", "Negro"};

std::string normalizar(const std::string& t)
{
    std::string s;
    for (size_t i = 0; i < t.size(); i++)
    {
        const unsigned char c = t[i];
        if (c == 0xC3 && i + 1 < t.size() && ((unsigned char)t[i + 1] & 0xC0) == 0x80)
        {
            // Las letras de Latin-1 (U+00C0 a U+00FF) sin tilde, diéresis ni virgulilla, y en minúscula, como NHSE.
            static const char base[] = "aaaaaa?ceeeeiiii?nooooo??uuuuy??aaaaaa?ceeeeiiii?nooooo??uuuuy?y";
            const unsigned char d = (unsigned char)t[++i] - 0x80;
            if (base[d] != '?')
                s += base[d];
            else
                s += (char)0xC3, s += (char)(d < 32 ? t[i] + 0x20 : t[i]);
            continue;
        }
        s += (char)tolower(c);
    }
    return s;
}

std::vector<int> buscarObjetos(const std::string& texto)
{
    std::vector<std::string> palabras;
    std::string q = normalizar(texto), w;
    for (char c : q + " ")
    {
        if (c == ' ')
        {
            if (!w.empty())
                palabras.push_back(w);
            w.clear();
        }
        else
            w += c;
    }
    std::vector<int> empiezan, contienen;
    if (palabras.empty())
        return empiezan;
    const int total = sizeof(OBJETOS) / sizeof(OBJETOS[0]);
    for (int i = 0; i < total; i++)
    {
        const char* clave = OBJETOS[i].clave;
        bool todas = true;
        for (const auto& p : palabras)
            todas = todas && strstr(clave, p.c_str()) != nullptr;
        if (!todas)
            continue;
        (strncmp(clave, palabras[0].c_str(), palabras[0].size()) == 0 ? empiezan : contienen).push_back(i);
    }
    empiezan.insert(empiezan.end(), contienen.begin(), contienen.end());
    return empiezan;
}

int objetoDe(const Objeto& o)
{
    const int total = sizeof(OBJETOS) / sizeof(OBJETOS[0]);
    for (int i = 0; i < total; i++)
        if (OBJETOS[i].id == o.id && (o.id != 0x16A2 || OBJETOS[i].cuenta == o.cuenta))
            return i;
    return -1;
}

int aprenderRecetas(std::vector<uint8_t>& p)
{
    // Como RecipeBook.GiveAll de NHSE: sabida y «nueva» si no se sabía.
    int n = 0;
    for (uint16_t r : RECETAS)
        if (!bit(p, RECETAS_POS, r))
            ponerBit(p, RECETAS_POS, r), ponerBit(p, RECETAS_POS + 0x200, r), n++;
    return n;
}

int completarCatalogo(std::vector<uint8_t>& p)
{
    int n = 0;
    for (uint16_t id : CATALOGO)
        if (!bit(p, CATALOGO_POS, id))
            ponerBit(p, CATALOGO_POS, id), n++;
    for (uint16_t c : CATALOGO_COLORES)
        ponerBit(p, CATALOGO_FIN, c);
    return n;
}

int aprenderReacciones(std::vector<uint8_t>& p)
{
    // Como AddMissingReactions de NHSE: cada una que falte, en el primer hueco libre.
    uint8_t* lista = p.data() + REACCIONES_POS;
    int n = 0;
    for (uint8_t r : REACCIONES)
    {
        if (std::find(lista, lista + 256, r) != lista + 256)
            continue;
        uint8_t* libre = std::find(lista, lista + 256, 0);
        if (libre == lista + 256)
            break;
        *libre = r;
        n++;
    }
    return n;
}

Progreso recetas(const std::vector<uint8_t>& p)
{
    Progreso r;
    for (uint16_t i : RECETAS)
        r.tiene += bit(p, RECETAS_POS, i), r.total++;
    return r;
}

Progreso catalogo(const std::vector<uint8_t>& p)
{
    Progreso r;
    for (uint16_t i : CATALOGO)
        r.tiene += bit(p, CATALOGO_POS, i), r.total++;
    return r;
}

Progreso reacciones(const std::vector<uint8_t>& p)
{
    Progreso r;
    const uint8_t* lista = p.data() + REACCIONES_POS;
    for (uint8_t i : REACCIONES)
        r.tiene += std::find(lista, lista + 256, i) != lista + 256, r.total++;
    return r;
}

// ------------------------------------------------------------------------------------------------------------ nabos
Nabos leerNabos(const std::vector<uint8_t>& main)
{
    Nabos n;
    n.compra = le32(main.data() + NABOS);
    for (int i = 0; i < 14; i++)
        n.venta[i] = le32(main.data() + NABOS + 4 + 4 * i);
    return n;
}

void escribirNabos(std::vector<uint8_t>& main, const Nabos& n)
{
    put32(main.data() + NABOS, n.compra);
    for (int i = 0; i < 14; i++)
        put32(main.data() + NABOS + 4 + 4 * i, n.venta[i]);
}

} // namespace acnh
