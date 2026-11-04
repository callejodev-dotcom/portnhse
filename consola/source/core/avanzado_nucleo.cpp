#include "avanzado.hpp"
#include "acnh.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace acnh {

namespace {
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

uint32_t posMarca(bool isla, int i) { return (isla ? MARCAS_ISLA_POS : MARCAS_JUGADOR_POS) + i * 2; }
int numMarcas(bool isla) { return isla ? NUM_MARCAS_ISLA : NUM_MARCAS_JUGADOR; }
} // namespace

int16_t leerMarca(const std::vector<uint8_t>& d, bool isla, int i)
{
    const uint32_t p = posMarca(isla, i);
    return (int16_t)(d[p] | (d[p + 1] << 8));
}

void escribirMarca(std::vector<uint8_t>& d, bool isla, int i, int16_t valor)
{
    const uint32_t p = posMarca(isla, i);
    d[p] = (uint8_t)(valor & 0xFF);
    d[p + 1] = (uint8_t)((uint16_t)valor >> 8);
}

std::vector<uint8_t> marcasEnBruto(const std::vector<uint8_t>& d, bool isla)
{
    const uint32_t p = posMarca(isla, 0);
    return std::vector<uint8_t>(d.begin() + p, d.begin() + p + numMarcas(isla) * 2);
}

bool cargarMarcas(std::vector<uint8_t>& d, bool isla, const std::vector<uint8_t>& bruto)
{
    if (bruto.size() != (size_t)numMarcas(isla) * 2)
        return false;
    memcpy(d.data() + posMarca(isla, 0), bruto.data(), bruto.size());
    return true;
}

uint32_t leer32(const std::vector<uint8_t>& d, uint32_t pos) { return le32(d.data() + pos); }

void escribir32(std::vector<uint8_t>& d, uint32_t pos, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        d[pos + i] = (uint8_t)(v >> (8 * i));
}

// ---------------------------------------------------------------------------------------------- comprobaciones
// FileHashRevision.REV_300 de NHSE: el valor va en «pos» y cubre los «largo» bytes que siguen a él.
namespace {
struct Region {
    uint32_t pos, largo;
};
struct Tipo {
    const char* nombre;
    size_t tam;
    std::vector<Region> regiones;
};
const std::vector<Tipo>& tipos()
{
    static const std::vector<Tipo> t = {
        {"main.dat", TAM_MAIN, {
            {0x000110, 0x1e339c}, {0x1e34b0, 0x3d089c}, {0x5b3e60, 0x037acc}, {0x5eb930, 0x03ce5c}, {0x6288a0, 0x037acc},
            {0x660370, 0x03ce5c}, {0x69d2e0, 0x037acc}, {0x6d4db0, 0x03ce5c}, {0x711d20, 0x037acc}, {0x7497f0, 0x03ce5c},
            {0x786760, 0x037acc}, {0x7be230, 0x03ce5c}, {0x7fb1a0, 0x037acc}, {0x832c70, 0x03ce5c}, {0x86fbe0, 0x037acc},
            {0x8a76b0, 0x03ce5c}, {0x8e4620, 0x037acc}, {0x91c0f0, 0x03ce5c}, {0x958f50, 0x057f3c}}},
        {"personal.dat", TAM_PERSONAL, {{0x00110, 0x37acc}, {0x37be0, 0x3ce5c}}},
        {"postbox.dat", 0x47430, {{0x100, 0x4732c}}},
        {"photo_studio_island.dat", 0x2F650, {{0x100, 0x2f54c}}},
        {"profile.dat", 0x69560, {{0x100, 0x6945c}}},
        {"wherearen.dat", 0xB8A4E0, {{0x100, 0xB8A3DC}}},
    };
    return t;
}

const Tipo* tipo(const std::string& nombre)
{
    for (const auto& t : tipos())
        if (nombre == t.nombre)
            return &t;
    return nullptr;
}

// Murmur3 como lo usa el juego (igual que en acnh.cpp).
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
} // namespace

size_t tamanoArchivo(const std::string& nombre)
{
    const Tipo* t = tipo(nombre);
    return t ? t->tam : 0;
}

Comprobado comprobarArchivo(const std::string& nombre, const std::vector<uint8_t>& d)
{
    Comprobado r;
    const Tipo* t = tipo(nombre);
    if (!t || d.size() != t->tam)
        return r;
    r.total = (int)t->regiones.size();
    for (const auto& g : t->regiones)
        if (le32(d.data() + g.pos) != murmur3(d.data() + g.pos + 4, g.largo))
            r.mal.push_back({g.pos, g.pos + 4, g.pos + 4 + g.largo});
    return r;
}

std::string hex(uint32_t n, int cifras)
{
    char s[16];
    snprintf(s, sizeof(s), "0x%0*X", cifras, n);
    return s;
}

// ------------------------------------------------------------------------------------------- partida descifrada
const char* const ARCHIVOS_JUGADOR[5] = {"personal.dat", "photo_studio_island.dat", "postbox.dat", "profile.dat", "wherearen.dat"};

std::vector<uint8_t> paraExportar(const std::vector<uint8_t>& datos)
{
    std::vector<uint8_t> r = datos;
    arreglarComprobaciones(r);
    return r;
}

bool cargarDescifrado(std::vector<uint8_t>& datos, const std::vector<uint8_t>& leido)
{
    if (leido.size() != datos.size() || leido.empty())
        return false;
    datos = leido;
    return true;
}

namespace {
int cifra(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// Quita espacios y el «0x» del principio de cada grupo.
std::string limpiar(const std::string& texto)
{
    std::string r;
    size_t i = 0;
    while (i < texto.size())
    {
        if (isspace((unsigned char)texto[i]) || texto[i] == ',')
        {
            i++;
            continue;
        }
        if (texto[i] == '0' && i + 1 < texto.size() && (texto[i + 1] == 'x' || texto[i + 1] == 'X'))
        {
            i += 2;
            continue;
        }
        r += texto[i++];
    }
    return r;
}
} // namespace

bool leerHex(const std::string& texto, std::vector<uint8_t>& bytes)
{
    // Cada grupo separado por espacios es un byte si tiene 1 o 2 cifras; si es más largo, de dos en dos.
    bytes.clear();
    std::string grupo;
    auto cerrar = [&]() {
        std::string g = limpiar(grupo);
        grupo.clear();
        if (g.empty())
            return true;
        if (g.size() % 2)
            g = "0" + g;
        for (size_t i = 0; i < g.size(); i += 2)
        {
            const int a = cifra(g[i]), b = cifra(g[i + 1]);
            if (a < 0 || b < 0)
                return false;
            bytes.push_back((uint8_t)(a * 16 + b));
        }
        return true;
    };
    for (char c : texto + " ")
    {
        if (isspace((unsigned char)c) || c == ',')
        {
            if (!cerrar())
                return false;
        }
        else
            grupo += c;
    }
    return !bytes.empty();
}

bool leerPosicion(const std::string& texto, uint32_t& pos)
{
    const std::string g = limpiar(texto);
    if (g.empty() || g.size() > 8)
        return false;
    uint32_t v = 0;
    for (char c : g)
    {
        const int k = cifra(c);
        if (k < 0)
            return false;
        v = v * 16 + k;
    }
    pos = v;
    return true;
}

} // namespace acnh
