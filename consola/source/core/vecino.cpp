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

// ---------------------------------------------------------------------------------------------- recuerdos
namespace {
uint32_t posSaludo(int i) { return 0x14C + 0x60 * i; }
constexpr uint32_t FECHA_SALUDO = 0x56C;
constexpr uint32_t MARCAS_RECUERDO_EN = 0x38;
} // namespace

std::string saludo(const std::vector<uint8_t>& main, int hueco, int r, int i) { return leerTextoVecino(recuerdoPtr(main, hueco, r) + posSaludo(i), 22); }
void ponerSaludo(std::vector<uint8_t>& main, int hueco, int r, int i, const std::string& texto) { escribirTextoVecino(recuerdoPtr(main, hueco, r) + posSaludo(i), texto, 47); }
void ponerJugadorRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::string& nombre) { escribirTextoVecino(recuerdoPtr(main, hueco, r) + 0x20, nombre, 10); }
void ponerIslaRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::string& nombre) { escribirTextoVecino(recuerdoPtr(main, hueco, r) + 4, nombre, 10); }

FechaVecino fechaSaludo(const std::vector<uint8_t>& main, int hueco, int r)
{
    const uint8_t* p = recuerdoPtr(main, hueco, r) + FECHA_SALUDO;
    FechaVecino f;
    f.anio = le16(p), f.mes = p[2], f.dia = p[3];
    return f;
}

void ponerFechaSaludo(std::vector<uint8_t>& main, int hueco, int r, const FechaVecino& f)
{
    uint8_t* p = recuerdoPtr(main, hueco, r) + FECHA_SALUDO;
    put16(p, f.anio);
    p[2] = f.mes, p[3] = f.dia;
}

bool fechaValida(const FechaVecino& f)
{
    if (f.anio < 1753 || f.anio > 9999 || f.mes < 1 || f.mes > 12 || f.dia < 1)
        return false;
    static const int DIAS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool bisiesto = (f.anio % 4 == 0 && f.anio % 100 != 0) || f.anio % 400 == 0;
    return f.dia <= DIAS[f.mes - 1] + (f.mes == 2 && bisiesto);
}

uint8_t marcaRecuerdo(const std::vector<uint8_t>& main, int hueco, int r, int i) { return recuerdoPtr(main, hueco, r)[MARCAS_RECUERDO_EN + i]; }
void ponerMarcaRecuerdo(std::vector<uint8_t>& main, int hueco, int r, int i, uint8_t valor) { recuerdoPtr(main, hueco, r)[MARCAS_RECUERDO_EN + i] = valor; }

std::vector<uint8_t> exportarRecuerdo(const std::vector<uint8_t>& main, int hueco, int r)
{
    const uint8_t* m = recuerdoPtr(main, hueco, r);
    return std::vector<uint8_t>(m, m + TAM_RECUERDO);
}

bool recuerdoDeOtro(const std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& a)
{
    const uint8_t* m = recuerdoPtr(main, hueco, r);
    return a.size() == TAM_RECUERDO && (memcmp(a.data(), m, TAM_IDENTIDAD) != 0 || memcmp(a.data() + ID_JUGADOR_EN, m + ID_JUGADOR_EN, TAM_IDENTIDAD) != 0);
}

std::string importarRecuerdo(std::vector<uint8_t>& main, int hueco, int r, const std::vector<uint8_t>& archivo, bool hacerSuyo)
{
    if (archivo.size() != TAM_RECUERDO)
        return "Ese archivo no es un recuerdo de un vecino (tiene otro tamaño).";
    std::vector<uint8_t> a = archivo;
    uint8_t* m = recuerdoPtr(main, hueco, r);
    if (hacerSuyo && recuerdoDeOtro(main, hueco, r, a))
    {
        // ChangeOrigins sobre los mismos datos: la isla y luego el jugador (leído ya con la isla cambiada).
        const std::vector<uint8_t> isla(a.begin(), a.begin() + TAM_IDENTIDAD);
        reemplazar(a, isla, m);
        const std::vector<uint8_t> jugador(a.begin() + ID_JUGADOR_EN, a.begin() + ID_JUGADOR_EN + TAM_IDENTIDAD);
        reemplazar(a, jugador, m + ID_JUGADOR_EN);
    }
    memcpy(m, a.data(), TAM_RECUERDO);
    return "";
}

// ---------------------------------------------------------------------------------------------- marcas sueltas
uint16_t marcaVecino(const std::vector<uint8_t>& main, int hueco, int i) { return le16(datosVecino(main, hueco) + V_BANDERAS + 2 * i); }
void ponerMarcaVecino(std::vector<uint8_t>& main, int hueco, int i, uint16_t valor) { put16(datosVecino(main, hueco) + V_BANDERAS + 2 * i, valor); }

std::vector<uint8_t> marcasVecinoEnBruto(const std::vector<uint8_t>& main, int hueco)
{
    const uint8_t* p = datosVecino(main, hueco) + V_BANDERAS;
    return std::vector<uint8_t>(p, p + 2 * NUM_MARCAS_VECINO);
}

bool cargarMarcasVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& a)
{
    if (a.size() != 2 * NUM_MARCAS_VECINO)
        return false;
    memcpy(datosVecino(main, hueco) + V_BANDERAS, a.data(), a.size());
    return true;
}

// ---------------------------------------------------------------------------------------------- su diseño
std::vector<uint8_t> disenoVecino(const std::vector<uint8_t>& main, int hueco)
{
    const uint8_t* p = datosVecino(main, hueco) + V_DISENO;
    return std::vector<uint8_t>(p, p + TAM_DISENO_VECINO);
}

void ponerDisenoVecino(std::vector<uint8_t>& main, int hueco, const std::vector<uint8_t>& d)
{
    if (d.size() == TAM_DISENO_VECINO)
        memcpy(datosVecino(main, hueco) + V_DISENO, d.data(), TAM_DISENO_VECINO);
}

// ---------------------------------------------------------------------------------------------- valores en bruto
const CampoBruto CAMPOS_CUARTO[] = {
    {"Pared de acento: objeto", "AccentWallItem.UniqueID", 0x00, 2, false, CampoObjeto},
    {"Pared de acento: parámetro del sistema", "AccentWallItem.SystemParam", 0x02, 1, false, CampoNumero},
    {"Pared de acento: parámetro extra", "AccentWallItem.AdditionalParam", 0x03, 1, false, CampoNumero},
    {"Pared de acento: parámetro libre", "AccentWallItem.FreeParam", 0x04, 4, true, CampoNumero},
    {"Pared: objeto", "WallItem.UniqueID", 0x08, 2, false, CampoObjeto},
    {"Pared: parámetro del sistema", "WallItem.SystemParam", 0x0A, 1, false, CampoNumero},
    {"Pared: parámetro extra", "WallItem.AdditionalParam", 0x0B, 1, false, CampoNumero},
    {"Pared: parámetro libre", "WallItem.FreeParam", 0x0C, 4, true, CampoNumero},
    {"Suelo: objeto", "FloorItem.UniqueID", 0x10, 2, false, CampoObjeto},
    {"Suelo: parámetro del sistema", "FloorItem.SystemParam", 0x12, 1, false, CampoNumero},
    {"Suelo: parámetro extra", "FloorItem.AdditionalParam", 0x13, 1, false, CampoNumero},
    {"Suelo: parámetro libre", "FloorItem.FreeParam", 0x14, 4, true, CampoNumero},
    {"Diseño de la pared de acento", "AccentWallMyDesignID", 0x18, 2, false, CampoNumero},
    {"Diseño de la pared", "WallMyDesignID", 0x1A, 2, false, CampoNumero},
    {"Diseño del suelo", "FloorMyDesignID", 0x1C, 2, false, CampoNumero},
    {"Hacia dónde mira la pared de acento", "AccentWallDirection", 0x1E, 1, false, CampoNumero},
    {"Marcas de información", "InfoBit", 0x1F, 1, false, CampoNumero},
    {"Hacia dónde mira el suelo", "FloorDirection", 0x20, 1, false, CampoNumero},
};
const int NUM_CAMPOS_CUARTO = sizeof(CAMPOS_CUARTO) / sizeof(CAMPOS_CUARTO[0]);

const CampoBruto CAMPOS_CASA[] = {
    {"Nivel de la casa", "HouseLevel", 0x00, 4, false, CampoNumero},
    {"Estado de la casa", "HouseStatus", 0x04, 4, false, CampoNumero},
    {"Fachada", "WallUniqueID", 0x08, 2, false, CampoFachada},
    {"Tejado", "RoofUniqueID", 0x0A, 2, false, CampoTejado},
    {"Puerta", "DoorUniqueID", 0x0C, 2, false, CampoPuerta},
    {"Fachada pedida para mañana", "OrderWallUniqueID", 0x0E, 2, false, CampoFachada},
    {"Tejado pedido para mañana", "OrderRoofUniqueID", 0x10, 2, false, CampoTejado},
    {"Puerta pedida para mañana", "OrderDoorUniqueID", 0x12, 2, false, CampoPuerta},
    {"En la puerta: objeto", "DoorDecoItemName.ItemId", 0x1C8, 2, false, CampoObjeto},
    {"En la puerta: parámetro del sistema", "DoorDecoItemName.SystemParam", 0x1CA, 1, false, CampoNumero},
    {"En la puerta: parámetro extra", "DoorDecoItemName.AdditionalParam", 0x1CB, 1, false, CampoNumero},
    {"En la puerta: cantidad", "DoorDecoItemName.Count", 0x1CC, 2, false, CampoNumero},
    {"En la puerta: usos", "DoorDecoItemName.UseCount", 0x1CE, 2, false, CampoNumero},
    {"Quién vive en ella (vecino)", "NPC1", 0x1C4, 1, true, CampoVecino},
    {"Segundo vecino", "NPC2", 0x1C5, 1, true, CampoVecino},
    {"Jugador que la construyó", "BuildPlayer", 0x1D0, 1, true, CampoNumero},
    {"Objeto extra", "ExtraItem.ItemId", 0x12DC, 2, false, CampoObjeto},
    {"Objeto extra: parámetro del sistema", "ExtraItem.SystemParam", 0x12DE, 1, false, CampoNumero},
    {"Objeto extra: parámetro extra", "ExtraItem.AdditionalParam", 0x12DF, 1, false, CampoNumero},
    {"Objeto extra: cantidad", "ExtraItem.Count", 0x12E0, 2, false, CampoNumero},
    {"Objeto extra: usos", "ExtraItem.UseCount", 0x12E2, 2, false, CampoNumero},
    {"Marca", "Flag", 0x12E4, 1, false, CampoNumero},
};
const int NUM_CAMPOS_CASA = sizeof(CAMPOS_CASA) / sizeof(CAMPOS_CASA[0]);

int64_t leerCampo(const uint8_t* base, const CampoBruto& c)
{
    const uint8_t* p = base + c.pos;
    uint32_t u = 0;
    for (int i = 0; i < c.tam; i++)
        u |= (uint32_t)p[i] << (8 * i);
    if (!c.signo)
        return u;
    return c.tam == 1 ? (int64_t)(int8_t)u : c.tam == 2 ? (int64_t)(int16_t)u : (int64_t)(int32_t)u;
}

void ponerCampo(uint8_t* base, const CampoBruto& c, int64_t valor)
{
    const uint32_t u = (uint32_t)valor;
    for (int i = 0; i < c.tam; i++)
        base[c.pos + i] = (uint8_t)(u >> (8 * i));
}

int64_t minimoCampo(const CampoBruto& c) { return c.signo ? -(int64_t(1) << (8 * c.tam - 1)) : 0; }
int64_t maximoCampo(const CampoBruto& c) { return c.signo ? (int64_t(1) << (8 * c.tam - 1)) - 1 : (int64_t(1) << (8 * c.tam)) - 1; }

} // namespace acnh
