#include "vecino.hpp"
#include "vecino_tablas.hpp"
#include <algorithm>
#include <cstring>

namespace acnh {

namespace {
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

Objeto leerObjeto(const uint8_t* p)
{
    Objeto o;
    o.id = le16(p);
    o.sistema = p[2];
    o.extra = p[3];
    o.cuenta = le16(p + 4);
    o.usos = le16(p + 6);
    return o;
}

void escribirObjeto(uint8_t* p, const Objeto& o)
{
    put16(p, o.id);
    p[2] = o.sistema;
    p[3] = o.extra;
    put16(p + 4, o.cuenta);
    put16(p + 6, o.usos);
}

const uint8_t* recuerdoPtr(const std::vector<uint8_t>& main, int hueco, int r) { return datosVecino(main, hueco) + V_RECUERDOS + r * TAM_RECUERDO; }
uint8_t* recuerdoPtr(std::vector<uint8_t>& main, int hueco, int r) { return datosVecino(main, hueco) + V_RECUERDOS + r * TAM_RECUERDO; }

// La isla (número y nombre) y el jugador (número y nombre): 24 bytes cada uno, en el recuerdo y en personal.dat.
constexpr int TAM_IDENTIDAD = 4 + 20;
constexpr uint32_t ID_JUGADOR_EN = 0x1C;

uint32_t posObjeto(int lista, int i) { return (lista == 0 ? V_ROPA : V_MUEBLES) + i * TAM_OBJETO_VECINO; }

// Cambia todas las veces que sale «antes» por «despues», como ReplaceOccurrences de NHSE.
void reemplazar(std::vector<uint8_t>& d, const std::vector<uint8_t>& antes, const uint8_t* despues)
{
    if (!memcmp(antes.data(), despues, antes.size()))
        return;
    auto it = d.begin();
    while (true)
    {
        it = std::search(it, d.end(), antes.begin(), antes.end());
        if (it == d.end())
            return;
        memcpy(&*it, despues, antes.size());
        it += antes.size();
    }
}
} // namespace

uint8_t* datosVecino(std::vector<uint8_t>& main, int hueco) { return main.data() + POS_VECINOS + hueco * TAM_VECINO; }
const uint8_t* datosVecino(const std::vector<uint8_t>& main, int hueco) { return main.data() + POS_VECINOS + hueco * TAM_VECINO; }

std::string leerTextoVecino(const uint8_t* p, int largo)
{
    std::string s;
    for (int i = 0; i < largo; i++)
    {
        uint32_t c = le16(p + 2 * i);
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < largo)
        {
            const uint32_t b = le16(p + 2 * (i + 1));
            if (b >= 0xDC00 && b < 0xE000)
                c = 0x10000 + ((c - 0xD800) << 10) + (b - 0xDC00), i++;
        }
        if (c < 0x80)
            s += (char)c;
        else if (c < 0x800)
            s += (char)(0xC0 | (c >> 6)), s += (char)(0x80 | (c & 0x3F));
        else if (c < 0x10000)
            s += (char)(0xE0 | (c >> 12)), s += (char)(0x80 | ((c >> 6) & 0x3F)), s += (char)(0x80 | (c & 0x3F));
        else
            s += (char)(0xF0 | (c >> 18)), s += (char)(0x80 | ((c >> 12) & 0x3F)), s += (char)(0x80 | ((c >> 6) & 0x3F)), s += (char)(0x80 | (c & 0x3F));
    }
    return s;
}

void escribirTextoVecino(uint8_t* p, const std::string& texto, int largo)
{
    // De UTF-8 a UTF-16; como NHSE, se corta a «largo» unidades y se rellena con ceros.
    std::vector<uint16_t> u;
    for (size_t i = 0; i < texto.size();)
    {
        const uint8_t c = texto[i];
        uint32_t cp;
        int n;
        if (c < 0x80)
            cp = c, n = 1;
        else if ((c & 0xE0) == 0xC0)
            cp = c & 0x1F, n = 2;
        else if ((c & 0xF0) == 0xE0)
            cp = c & 0x0F, n = 3;
        else
            cp = c & 0x07, n = 4;
        for (int k = 1; k < n && i + k < texto.size(); k++)
            cp = (cp << 6) | (texto[i + k] & 0x3F);
        i += n;
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            u.push_back((uint16_t)(0xD800 + (cp >> 10)));
            u.push_back((uint16_t)(0xDC00 + (cp & 0x3FF)));
        }
        else
            u.push_back((uint16_t)cp);
    }
    u.resize(largo, 0);
    for (int i = 0; i < largo; i++)
        put16(p + 2 * i, u[i]);
}

// ---------------------------------------------------------------------------------------------- personalidad y frase
const char* const PERSONALIDADES[8] = {"Perezoso", "Deportista", "Gruñón", "Presumido", "Normal", "Alegre", "Esnob", "Hermana mayor"};

int personalidad(const std::vector<uint8_t>& main, int hueco) { return datosVecino(main, hueco)[V_PERSONALIDAD]; }
void ponerPersonalidad(std::vector<uint8_t>& main, int hueco, int p) { datosVecino(main, hueco)[V_PERSONALIDAD] = (uint8_t)p; }
std::string frase(const std::vector<uint8_t>& main, int hueco) { return leerTextoVecino(datosVecino(main, hueco) + V_FRASE, LARGO_FRASE); }
void ponerFrase(std::vector<uint8_t>& main, int hueco, const std::string& texto) { escribirTextoVecino(datosVecino(main, hueco) + V_FRASE, texto, LARGO_FRASE); }

std::string fraseOriginal(const std::string& interno)
{
    const auto* fin = FRASES_VECINOS + sizeof(FRASES_VECINOS) / sizeof(FRASES_VECINOS[0]);
    const auto* f = std::lower_bound(FRASES_VECINOS, fin, interno, [](const FraseVecino& a, const std::string& b) { return strcmp(a.interno, b.c_str()) < 0; });
    return f != fin && interno == f->interno ? f->frase : "";
}

// ---------------------------------------------------------------------------------------------- mudanza
bool seMuda(const std::vector<uint8_t>& main, int hueco) { return (datosVecino(main, hueco)[V_MUDANZA] & 2) != 0; }

void ponerMudanza(std::vector<uint8_t>& main, int hueco, bool siONo)
{
    uint8_t* v = datosVecino(main, hueco);
    v[V_MUDANZA] = (uint8_t)((v[V_MUDANZA] & ~2) | (siONo ? 2 : 0));
    put16(v + V_BANDERAS + BANDERA_IRSE * 2, siONo ? 1 : 0);
}

int mudarTodos(std::vector<uint8_t>& main)
{
    int n = 0;
    for (int i = 0; i < NUM_VECINOS; i++)
        if (!vecinoEn(main, i).empty())
            ponerMudanza(main, i, true), n++;
    return n;
}

// ---------------------------------------------------------------------------------------------- amistad
Recuerdo recuerdo(const std::vector<uint8_t>& main, int hueco, int r)
{
    const uint8_t* m = recuerdoPtr(main, hueco, r);
    Recuerdo x;
    x.isla = leerTextoVecino(m + 4, 10);
    x.jugador = leerTextoVecino(m + 0x20, 10);
    x.apodo = leerTextoVecino(m + 0x138, 10);
    x.amistad = m[0x38 + 10];
    return x;
}

bool recuerdoUsado(const std::vector<uint8_t>& main, int hueco, int r)
{
    // Como FriendshipWindow: los que tienen nombre de jugador (no solo espacios).
    const uint8_t* m = recuerdoPtr(main, hueco, r) + 0x20;
    for (int k = 0; k < 10; k++)
    {
        const uint16_t c = le16(m + 2 * k);
        if (c == 0)
            break;
        if (c != ' ' && !(c >= 9 && c <= 13) && c != 0x85 && c != 0xA0 && c != 0x1680 && !(c >= 0x2000 && c <= 0x200A) && c != 0x2028 && c != 0x2029 && c != 0x202F && c != 0x205F && c != 0x3000)
            return true;
    }
    return false;
}

void ponerAmistad(std::vector<uint8_t>& main, int hueco, int r, uint8_t puntos) { recuerdoPtr(main, hueco, r)[0x38 + 10] = puntos; }
void ponerApodo(std::vector<uint8_t>& main, int hueco, int r, const std::string& apodo) { escribirTextoVecino(recuerdoPtr(main, hueco, r) + 0x138, apodo, 10); }

bool mismaIsla(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& personal)
{
    return !memcmp(recuerdoPtr(main, hueco, r), personal.data() + ID_JUGADOR, TAM_IDENTIDAD);
}

bool mismoJugador(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& personal)
{
    return !memcmp(recuerdoPtr(main, hueco, r) + ID_JUGADOR_EN, personal.data() + ID_JUGADOR + ID_JUGADOR_EN, TAM_IDENTIDAD);
}

int recuerdoDe(const std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& personal)
{
    for (int r = 0; r < NUM_RECUERDOS; r++)
        if (le16(recuerdoPtr(main, hueco, r) + 0x20) != 0 && mismoJugador(main, hueco, r, personal) && mismaIsla(main, hueco, r, personal))
            return r;
    return -1;
}

const uint8_t INICIO_NIVEL[8] = {0, 25, 30, 70, 100, 130, 160, 200};
const char* const NIVELES[8] = {"Está molesto contigo", "Os acabáis de conocer", "Conocidos", "Amigos", "Buenos amigos", "Muy buenos amigos", "Grandes amigos", "Mejores amigos"};

int nivelAmistad(uint8_t puntos)
{
    for (int n = 7; n > 0; n--)
        if (puntos >= INICIO_NIVEL[n])
            return n;
    return 0;
}

// ---------------------------------------------------------------------------------------------- ropa y muebles
Objeto objetoVecino(const std::vector<uint8_t>& main, int hueco, int lista, int i) { return leerObjeto(datosVecino(main, hueco) + posObjeto(lista, i)); }
void ponerObjetoVecino(std::vector<uint8_t>& main, int hueco, int lista, int i, const Objeto& o) { escribirObjeto(datosVecino(main, hueco) + posObjeto(lista, i), o); }

// ---------------------------------------------------------------------------------------------- paredes y suelo
uint16_t parteCuarto(const std::vector<uint8_t>& main, int hueco, int parte) { return le16(datosVecino(main, hueco) + V_HABITACION + parte * 8); }

void ponerParteCuarto(std::vector<uint8_t>& main, int hueco, int parte, uint16_t id)
{
    uint8_t* p = datosVecino(main, hueco) + V_HABITACION + parte * 8;
    if (le16(p) == id)
        return;
    memset(p, 0, 8);
    put16(p, id);
}

// ---------------------------------------------------------------------------------------------- la casa
int casaDe(const std::vector<uint8_t>& main, int hueco)
{
    for (int j = 0; j < NUM_VECINOS; j++)
        if ((int8_t)main[CASAS + j * TAM_CASA + 0x1C4] == hueco)
            return j;
    return -1;
}

uint8_t* datosCasa(std::vector<uint8_t>& main, int casa) { return main.data() + CASAS + casa * TAM_CASA; }
uint16_t parteCasa(const std::vector<uint8_t>& main, int casa, int parte) { return le16(main.data() + CASAS + casa * TAM_CASA + 8 + parte * 2); }

void ponerParteCasa(std::vector<uint8_t>& main, int casa, int parte, uint16_t id)
{
    uint8_t* c = datosCasa(main, casa);
    put16(c + 8 + parte * 2, id);
    put16(c + 0xE + parte * 2, id);
}

Objeto decoracionPuerta(const std::vector<uint8_t>& main, int casa) { return leerObjeto(main.data() + CASAS + casa * TAM_CASA + 0x1C8); }
void ponerDecoracionPuerta(std::vector<uint8_t>& main, int casa, const Objeto& o) { escribirObjeto(datosCasa(main, casa) + 0x1C8, o); }

// ---------------------------------------------------------------------------------------------- bricolaje
Bricolaje bricolaje(const std::vector<uint8_t>& main, int hueco)
{
    const uint8_t* p = datosVecino(main, hueco) + V_BRICOLAJE;
    Bricolaje b;
    b.hora = p[0], b.minuto = p[1], b.segundo = p[2];
    b.receta = le16(p + 3);
    return b;
}

void ponerBricolaje(std::vector<uint8_t>& main, int hueco, const Bricolaje& b)
{
    uint8_t* p = datosVecino(main, hueco) + V_BRICOLAJE;
    p[0] = b.hora, p[1] = b.minuto, p[2] = b.segundo;
    put16(p + 3, b.receta);
}

// ---------------------------------------------------------------------------------------------- archivos
std::vector<uint8_t> exportarVecino(const std::vector<uint8_t>& main, int hueco)
{
    const uint8_t* v = datosVecino(main, hueco);
    return std::vector<uint8_t>(v, v + TAM_VECINO);
}

std::vector<uint8_t> vecinoDeArchivo(const std::vector<uint8_t>& a)
{
    if (a.size() == TAM_VECINO)
        return a;
    if (a.size() != TAM_VECINO1)
        return {};
    // VillagerConverter.Convert12: en la 1.5 cada uno de los 160 «GSaveLightMemory» creció 0xC bytes.
    std::vector<uint8_t> v(TAM_VECINO);
    memcpy(v.data(), a.data(), 0x2F84);
    for (int i = 0; i < 160; i++)
        memcpy(v.data() + 0x2F84 + 0x158 * i, a.data() + 0x2F84 + 0x14C * i, 0x14C);
    memcpy(v.data() + 0x10684, a.data() + 0xFF04, a.size() - 0xFF04);
    return v;
}

bool deOtraIsla(const std::vector<uint8_t>& v, const std::vector<uint8_t>& personal)
{
    return memcmp(v.data() + V_RECUERDOS, personal.data() + ID_JUGADOR, TAM_IDENTIDAD) != 0
        || memcmp(v.data() + V_RECUERDOS + ID_JUGADOR_EN, personal.data() + ID_JUGADOR + ID_JUGADOR_EN, TAM_IDENTIDAD) != 0;
}

std::string islaDe(const std::vector<uint8_t>& v) { return leerTextoVecino(v.data() + V_RECUERDOS + 4, 10); }

void hacerDeLaIsla(std::vector<uint8_t>& v, const std::vector<uint8_t>& personal)
{
    // Como IVillagerOrigin.ChangeOrigins: primero la isla y luego el jugador del primer recuerdo, en todo el vecino.
    const std::vector<uint8_t> isla(v.begin() + V_RECUERDOS, v.begin() + V_RECUERDOS + TAM_IDENTIDAD);
    const std::vector<uint8_t> jugador(v.begin() + V_RECUERDOS + ID_JUGADOR_EN, v.begin() + V_RECUERDOS + ID_JUGADOR_EN + TAM_IDENTIDAD);
    reemplazar(v, isla, personal.data() + ID_JUGADOR);
    reemplazar(v, jugador, personal.data() + ID_JUGADOR + ID_JUGADOR_EN);
}

std::string importarVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& archivo, bool hacerSuyo, const std::vector<uint8_t>& personal)
{
    std::vector<uint8_t> v = vecinoDeArchivo(archivo);
    if (v.empty())
        return "Ese archivo no es un vecino de Animal Crossing: New Horizons.";
    if (hacerSuyo && deOtraIsla(v, personal))
        hacerDeLaIsla(v, personal);
    memcpy(datosVecino(main, hueco), v.data(), TAM_VECINO);
    return "";
}

std::vector<uint8_t> exportarCasa(const std::vector<uint8_t>& main, int casa)
{
    const uint8_t* c = main.data() + CASAS + casa * TAM_CASA;
    return std::vector<uint8_t>(c, c + TAM_CASA);
}

std::string importarCasa(std::vector<uint8_t>& main, int casa, const std::vector<uint8_t>& a)
{
    std::vector<uint8_t> c;
    if (a.size() == TAM_CASA)
        c = a;
    else if (a.size() == TAM_CASA1)
    {
        // VillagerHouse1.Upgrade: los 236 muebles vacíos y el final de siempre.
        static const uint8_t FINAL[] = {
            0x4B, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x9B, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x40,
            0x10, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        c.assign(TAM_CASA, 0);
        memcpy(c.data(), a.data(), a.size());
        for (int i = 0; i < 236; i++)
            put16(c.data() + 0x1D8 + i * 0xC, NADA);
        memcpy(c.data() + 0x1270, FINAL, sizeof(FINAL));
    }
    else
        return "Ese archivo no es una casa de vecino.";
    uint8_t* d = datosCasa(main, casa);
    c[0x1C4] = d[0x1C4]; // quien vive en ella sigue igual
    memcpy(d, c.data(), TAM_CASA);
    return "";
}

} // namespace acnh
