// La isla (sin el mapa ni las casas): ver isla.hpp. Se llama isla_nucleo.cpp y no isla.cpp porque el Makefile de la
// Switch junta los .o de source/app y source/core en una carpeta y ya hay un source/app/isla.cpp.
#include "isla.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace acnh {

namespace {

uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
void pon16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void pon32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

// GSaveDate: año (2 bytes), mes, día.
FechaIsla leerFecha(const uint8_t* p) { return {le16(p), p[2], p[3]}; }
void ponerFecha(uint8_t* p, const FechaIsla& f)
{
    pon16(p, (uint16_t)f.anio);
    p[2] = (uint8_t)f.mes;
    p[3] = (uint8_t)f.dia;
}

// Texto UTF-16 de la partida (hasta el primer 0) a UTF-8 y al revés.
std::string deUtf16(const uint8_t* p, int max)
{
    std::string s;
    for (int i = 0; i < max; i++)
    {
        uint32_t c = le16(p + i * 2);
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < max)
        {
            const uint32_t d = le16(p + (i + 1) * 2);
            if (d >= 0xDC00 && d < 0xE000)
                c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00), i++;
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

std::vector<uint16_t> aUtf16(const std::string& s)
{
    std::vector<uint16_t> r;
    for (size_t i = 0; i < s.size();)
    {
        const uint8_t b = (uint8_t)s[i];
        uint32_t c;
        int n;
        if (b < 0x80) c = b, n = 1;
        else if ((b & 0xE0) == 0xC0) c = b & 0x1F, n = 2;
        else if ((b & 0xF0) == 0xE0) c = b & 0x0F, n = 3;
        else c = b & 0x07, n = 4;
        for (int k = 1; k < n && i + k < s.size(); k++)
            c = (c << 6) | ((uint8_t)s[i + k] & 0x3F);
        i += n;
        if (c >= 0x10000)
        {
            c -= 0x10000;
            r.push_back((uint16_t)(0xD800 + (c >> 10)));
            r.push_back((uint16_t)(0xDC00 + (c & 0x3FF)));
        }
        else
            r.push_back((uint16_t)c);
    }
    return r;
}

// Escribe un texto en un campo de «tam» letras: como mucho tam-1 y un 0; lo demás a cero (como lo deja NHSE).
void ponerUtf16(uint8_t* p, int tam, const std::string& s)
{
    std::vector<uint16_t> u = aUtf16(s);
    if ((int)u.size() > tam - 1)
        u.resize(tam - 1);
    std::fill(p, p + tam * 2, 0);
    for (size_t i = 0; i < u.size(); i++)
        pon16(p + i * 2, u[i]);
}

const char* const MESES_CORTOS[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sept", "oct", "nov", "dic"};

} // namespace

FechaIsla hoyIsla()
{
    const time_t t = time(nullptr);
    const tm* l = localtime(&t);
    return {l->tm_year + 1900, l->tm_mon + 1, l->tm_mday};
}

std::string textoFechaIsla(const FechaIsla& f)
{
    if (f.vacia())
        return "";
    if (f.mes < 1 || f.mes > 12)
        return std::to_string(f.dia) + "/" + std::to_string(f.mes) + "/" + std::to_string(f.anio);
    return std::to_string(f.dia) + " " + MESES_CORTOS[f.mes - 1] + " " + std::to_string(f.anio);
}

// ---------------------------------------------------------------------------------------- hemisferio y aeródromo
const char* const AEROPUERTOS[NUM_AEROPUERTOS] = {"Azul", "Amarillo", "Naranja", "Verde"};

int hemisferio(const std::vector<uint8_t>& m) { return m[ISLA_HEMISFERIO]; }
void ponerHemisferio(std::vector<uint8_t>& m, int h) { m[ISLA_HEMISFERIO] = (uint8_t)h; }
int aeropuerto(const std::vector<uint8_t>& m) { return m[ISLA_AEROPUERTO]; }
void ponerAeropuerto(std::vector<uint8_t>& m, int c) { m[ISLA_AEROPUERTO] = (uint8_t)c; }

// ------------------------------------------------------------------------------------------------- fruta y flores
const char* const FLORES_ISLA[NUM_FLORES_ISLA] = {"Anémonas", "Jacintos", "Tulipanes", "Rosas", "Cosmos", "Lirios", "Crisantemos", "Violas"};

namespace {
int indiceFruta(uint16_t id)
{
    for (int i = 0; i < 5; i++)
        if (FRUTAS_ISLA[i].id == id)
            return i;
    return -1;
}
} // namespace

std::string nombreFruta(uint16_t id)
{
    const int i = indiceFruta(id);
    return i >= 0 ? FRUTAS_ISLA[i].nombre : "Objeto " + std::to_string(id);
}

FrutasFlores leerFrutasFlores(const std::vector<uint8_t>& m, const std::vector<uint8_t>& yo)
{
    FrutasFlores f;
    f.fruta = le16(m.data() + ISLA_FRUTA);
    if (indiceFruta(f.fruta) < 0)
        f.fruta = le16(yo.data() + PERSONAL_FRUTA);
    f.frutaHermana = le16(m.data() + ISLA_FRUTA_HERMANA);
    f.flor = m[ISLA_FLOR];
    f.florHermana = m[ISLA_FLOR_HERMANA];
    return f;
}

void guardarFrutasFlores(std::vector<uint8_t>& m, std::vector<std::vector<uint8_t>*> personales, int yo, const FrutasFlores& f)
{
    const uint16_t anterior = le16(m.data() + ISLA_FRUTA);
    for (int i = 0; i < (int)personales.size(); i++)
    {
        uint8_t* p = personales[i]->data() + PERSONAL_FRUTA;
        if (i == yo || le16(p) == anterior)
            pon16(p, f.fruta);
    }
    pon16(m.data() + ISLA_FRUTA, f.fruta);
    pon16(m.data() + ISLA_FRUTA_HERMANA, f.frutaHermana);
    const int k = indiceFruta(f.fruta);
    for (int i = 0; i < 5; i++)
        m[ISLA_BANDERAS_FRUTA + i] = i == k;
    m[ISLA_FLOR] = f.flor;
    m[ISLA_FLOR_HERMANA] = f.florHermana;
}

// ---------------------------------------------------------------------------------------------- zona de acampada
namespace {
constexpr int NUM_ESPECIES = 36; // con «non» (nadie) al final
const char* const ESPECIES_CAMPING[NUM_ESPECIES] = {"ant", "bea", "brd", "bul", "cat", "cbr", "chn", "cow", "crd", "der",
    "dog", "duk", "elp", "flg", "goa", "gor", "ham", "hip", "hrs", "kal", "kgr", "lon", "mnk", "mus", "ocp", "ost", "pbr",
    "pgn", "pig", "rbt", "rhn", "shp", "squ", "tig", "wol", "non"};
constexpr uint8_t NADIE = NUM_ESPECIES - 1;
} // namespace

int estadoCamping(const std::vector<uint8_t>& m) { return m[ISLA_CAMPING]; }

std::string campista(const std::vector<uint8_t>& m)
{
    const uint8_t variante = m[ISLA_CAMPING_VECINO], especie = m[ISLA_CAMPING_VECINO + 1];
    if (estadoCamping(m) != 3 || especie >= NADIE)
        return "";
    char s[16];
    snprintf(s, sizeof(s), "%s%02d", ESPECIES_CAMPING[especie], variante);
    return s;
}

FechaIsla fechaCamping(const std::vector<uint8_t>& m) { return leerFecha(m.data() + ISLA_CAMPING_FECHA); }

bool guardarCamping(std::vector<uint8_t>& m, const std::string& interno, const FechaIsla& dia)
{
    if (!hayCamping(m))
        return false;
    if (interno.empty())
    {
        m[ISLA_CAMPING] = 2;
        m[ISLA_CAMPING_VECINO] = 0;
        m[ISLA_CAMPING_VECINO + 1] = NADIE;
        return true;
    }
    if (interno.size() != 5)
        return false;
    int especie = -1;
    for (int i = 0; i < NADIE; i++)
        if (interno.compare(0, 3, ESPECIES_CAMPING[i]) == 0)
            especie = i;
    if (especie < 0 || !isdigit((unsigned char)interno[3]) || !isdigit((unsigned char)interno[4]))
        return false;
    m[ISLA_CAMPING] = 3;
    m[ISLA_CAMPING_VECINO + 1] = (uint8_t)especie;
    m[ISLA_CAMPING_VECINO] = (uint8_t)std::stoi(interno.substr(3));
    ponerFecha(m.data() + ISLA_CAMPING_FECHA, dia);
    return true;
}

// ---------------------------------------------------------------------------------------------------- visitantes
// GSaveVisitorNpc: quién viene (7 × 4 bytes), dos posiciones, conciertos normales (7) y de cumpleaños (7), 2 bytes,
// el plan de la semana (7 × 4), 14 marcas, 2 bytes y dos números.
namespace {
constexpr uint32_t VIS_QUIEN = 0x00, VIS_POSICIONES = 0x1C, VIS_NORMAL = 0x34, VIS_CUMPLE = 0x3B, VIS_PLAN = 0x44,
    VIS_MARCAS = 0x60, VIS_DIAS = 0x70;

// NHSE lee y vuelve a escribir todo: las marcas (sí/no) quedan en 0 o 1.
void normalizarVisitantes(std::vector<uint8_t>& m)
{
    uint8_t* p = m.data() + ISLA_VISITANTES;
    for (uint32_t i = 0; i < 7; i++)
    {
        p[VIS_NORMAL + i] = p[VIS_NORMAL + i] != 0;
        p[VIS_CUMPLE + i] = p[VIS_CUMPLE + i] != 0;
    }
    for (uint32_t i = 0; i < 14; i++)
        p[VIS_MARCAS + i] = p[VIS_MARCAS + i] != 0;
}
} // namespace

int visitante(const std::vector<uint8_t>& m, int dia) { return (int)le32(m.data() + ISLA_VISITANTES + VIS_QUIEN + dia * 4); }

void ponerVisitante(std::vector<uint8_t>& m, int dia, int quien)
{
    pon32(m.data() + ISLA_VISITANTES + VIS_QUIEN + dia * 4, (uint32_t)quien);
    pon32(m.data() + ISLA_VISITANTES + VIS_PLAN + dia * 4, (uint32_t)quien);
    normalizarVisitantes(m);
}

bool concierto(const std::vector<uint8_t>& m, int dia, bool cumple)
{
    return m[ISLA_VISITANTES + (cumple ? VIS_CUMPLE : VIS_NORMAL) + dia] != 0;
}

void ponerConcierto(std::vector<uint8_t>& m, int dia, bool cumple, bool si)
{
    m[ISLA_VISITANTES + (cumple ? VIS_CUMPLE : VIS_NORMAL) + dia] = si;
    normalizarVisitantes(m);
}

void ponerSoloVisitante(std::vector<uint8_t>& m, int dia, int quien)
{
    pon32(m.data() + ISLA_VISITANTES + VIS_QUIEN + dia * 4, (uint32_t)quien);
    normalizarVisitantes(m);
}

int planVisitante(const std::vector<uint8_t>& m, int dia) { return (int)le32(m.data() + ISLA_VISITANTES + VIS_PLAN + dia * 4); }

void ponerPlanVisitante(std::vector<uint8_t>& m, int dia, int quien)
{
    pon32(m.data() + ISLA_VISITANTES + VIS_PLAN + dia * 4, (uint32_t)quien);
    normalizarVisitantes(m);
}

PosicionIsla posicionVisitantes(const std::vector<uint8_t>& m, int cual)
{
    PosicionIsla p;
    float* v[3] = {&p.x, &p.y, &p.z};
    for (int i = 0; i < 3; i++)
    {
        const uint32_t b = le32(m.data() + ISLA_VISITANTES + VIS_POSICIONES + cual * 12 + i * 4);
        memcpy(v[i], &b, 4);
    }
    return p;
}

void ponerPosicionVisitantes(std::vector<uint8_t>& m, int cual, const PosicionIsla& p)
{
    const float v[3] = {p.x, p.y, p.z};
    for (int i = 0; i < 3; i++)
    {
        uint32_t b;
        memcpy(&b, &v[i], 4);
        pon32(m.data() + ISLA_VISITANTES + VIS_POSICIONES + cual * 12 + i * 4, b);
    }
    normalizarVisitantes(m);
}

bool marcaVisitantes(const std::vector<uint8_t>& m, int i) { return m[ISLA_VISITANTES + VIS_MARCAS + i] != 0; }

void ponerMarcaVisitantes(std::vector<uint8_t>& m, int i, bool si)
{
    m[ISLA_VISITANTES + VIS_MARCAS + i] = si;
    normalizarVisitantes(m);
}

int32_t diaVisitante(const std::vector<uint8_t>& m, int cual) { return (int32_t)le32(m.data() + ISLA_VISITANTES + VIS_DIAS + cual * 4); }

void ponerDiaVisitante(std::vector<uint8_t>& m, int cual, int32_t valor)
{
    pon32(m.data() + ISLA_VISITANTES + VIS_DIAS + cual * 4, (uint32_t)valor);
    normalizarVisitantes(m);
}

// -------------------------------------------------------------------------------------------------------- nabos
const char* const TENDENCIAS[NUM_TENDENCIAS] = {"Fluctuante", "Pico grande", "Decreciente", "Pico pequeño"};
const char* const TENDENCIAS_EXPLICACION[NUM_TENDENCIAS] = {
    "Sube y baja toda la semana; a veces, algo por encima de lo que pagaste.",
    "Baja unos días y luego se dispara: hasta seis veces lo que pagaste.",
    "Baja durante toda la semana. Mejor venderlos en otra isla.",
    "Baja y luego sube un poco: hasta unas dos veces lo que pagaste.",
};

// TurnipStonk: compra, 14 precios, tendencia (+0x3C) y comienzo del pico (+0x40).
uint32_t tendenciaNabos(const std::vector<uint8_t>& m) { return le32(m.data() + ISLA_NABOS + 0x3C); }
void ponerTendenciaNabos(std::vector<uint8_t>& m, uint32_t t) { pon32(m.data() + ISLA_NABOS + 0x3C, t); }
uint32_t picoNabos(const std::vector<uint8_t>& m) { return le32(m.data() + ISLA_NABOS + 0x40); }
void ponerPicoNabos(std::vector<uint8_t>& m, uint32_t medioDia) { pon32(m.data() + ISLA_NABOS + 0x40, medioDia); }

// --------------------------------------------------------------------------------------------------------- museo
// Museum: nivel (4 bytes), 1024 fechas (4 bytes), 1024 objetos (8 bytes) y 1024 jugadores (1 byte).
namespace {
constexpr uint32_t MUS_FECHAS = 0x4, MUS_OBJETOS = 0x1004, MUS_JUGADORES = 0x3004;
constexpr uint16_t SIN_OBJETO = 0xFFFE;

uint16_t objetoMuseo(const std::vector<uint8_t>& m, int i) { return le16(m.data() + ISLA_MUSEO + MUS_OBJETOS + i * 8); }
} // namespace

std::vector<Donacion> donaciones(const std::vector<uint8_t>& m)
{
    // Qué pieza es cada objeto (una vez): así se mira cada hueco del museo una sola vez.
    static const std::vector<int16_t> pieza = [] {
        std::vector<int16_t> v(65536, -1);
        for (int k = NUM_PIEZAS - 1; k >= 0; k--)
            v[PIEZAS_MUSEO[k].id] = (int16_t)k;
        return v;
    }();
    std::vector<Donacion> r(NUM_PIEZAS);
    // La primera vez que sale cada objeto, como MuseumWindow.
    for (int h = 0; h < HUECOS_MUSEO; h++)
    {
        const int k = pieza[objetoMuseo(m, h)];
        if (k < 0 || r[k].donada())
            continue;
        r[k].hueco = h;
        r[k].fecha = leerFecha(m.data() + ISLA_MUSEO + MUS_FECHAS + h * 4);
        r[k].jugador = m[ISLA_MUSEO + MUS_JUGADORES + h];
    }
    return r;
}

bool donar(std::vector<uint8_t>& m, uint16_t id, int jugador, const FechaIsla& dia)
{
    int libre = -1;
    for (int h = 0; h < HUECOS_MUSEO; h++)
    {
        const uint16_t o = objetoMuseo(m, h);
        if (o == id)
            return false;
        if (o == SIN_OBJETO && libre < 0)
            libre = h;
    }
    if (libre < 0)
        return false;
    uint8_t* o = m.data() + ISLA_MUSEO + MUS_OBJETOS + libre * 8;
    std::fill(o, o + 8, 0);
    pon16(o, id);
    ponerFecha(m.data() + ISLA_MUSEO + MUS_FECHAS + libre * 4, dia);
    m[ISLA_MUSEO + MUS_JUGADORES + libre] = (uint8_t)jugador;
    return true;
}

bool retirar(std::vector<uint8_t>& m, uint16_t id)
{
    bool alguna = false;
    uint8_t* base = m.data() + ISLA_MUSEO;
    for (int h = 0; h < HUECOS_MUSEO;)
    {
        if (objetoMuseo(m, h) != id)
        {
            h++;
            continue;
        }
        alguna = true;
        const int ultimo = HUECOS_MUSEO - 1;
        std::copy(base + MUS_FECHAS + (h + 1) * 4, base + MUS_FECHAS + HUECOS_MUSEO * 4, base + MUS_FECHAS + h * 4);
        std::copy(base + MUS_OBJETOS + (h + 1) * 8, base + MUS_OBJETOS + HUECOS_MUSEO * 8, base + MUS_OBJETOS + h * 8);
        std::copy(base + MUS_JUGADORES + h + 1, base + MUS_JUGADORES + HUECOS_MUSEO, base + MUS_JUGADORES + h);
        uint8_t* o = base + MUS_OBJETOS + ultimo * 8;
        std::fill(o, o + 8, 0);
        pon16(o, SIN_OBJETO);
        std::fill(base + MUS_FECHAS + ultimo * 4, base + MUS_FECHAS + ultimo * 4 + 4, 0);
        base[MUS_JUGADORES + ultimo] = 0xFF; // los huecos vacíos no tienen jugador
    }
    return alguna;
}

std::vector<EntradaMuseo> entradasMuseo(const std::vector<uint8_t>& m)
{
    std::vector<EntradaMuseo> r;
    for (int h = 0; h < HUECOS_MUSEO; h++)
    {
        const uint16_t o = objetoMuseo(m, h);
        if (o == SIN_OBJETO)
            continue;
        EntradaMuseo e;
        e.hueco = h;
        e.id = o;
        e.fecha = leerFecha(m.data() + ISLA_MUSEO + MUS_FECHAS + h * 4);
        e.jugador = m[ISLA_MUSEO + MUS_JUGADORES + h];
        r.push_back(e);
    }
    return r;
}

bool fechaIslaValida(const FechaIsla& f)
{
    if (f.anio < 1 || f.anio > 9999 || f.mes < 1 || f.mes > 12 || f.dia < 1)
        return false;
    static const int dias[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool bisiesto = (f.anio % 4 == 0 && f.anio % 100 != 0) || f.anio % 400 == 0;
    return f.dia <= dias[f.mes - 1] + (f.mes == 2 && bisiesto);
}

void ponerFechaDonacion(std::vector<uint8_t>& m, int hueco, const FechaIsla& f)
{
    ponerFecha(m.data() + ISLA_MUSEO + MUS_FECHAS + hueco * 4, f.vacia() ? FechaIsla{} : f);
}

void ponerDonante(std::vector<uint8_t>& m, int hueco, int jugador) { m[ISLA_MUSEO + MUS_JUGADORES + hueco] = (uint8_t)jugador; }

void ponerPiezaDonacion(std::vector<uint8_t>& m, int hueco, uint16_t id) { pon16(m.data() + ISLA_MUSEO + MUS_OBJETOS + hueco * 8, id); }

void ordenarMuseoPorFecha(std::vector<uint8_t>& m)
{
    // Como MuseumEditor.SortAll: primero las que tienen fecha, por fecha; luego las demás; sin cambiar el orden de las
    // que empatan (también los huecos vacíos).
    uint8_t* base = m.data() + ISLA_MUSEO;
    std::vector<int> orden(HUECOS_MUSEO);
    for (int i = 0; i < HUECOS_MUSEO; i++)
        orden[i] = i;
    auto clave = [&](int i) {
        const FechaIsla f = leerFecha(base + MUS_FECHAS + i * 4);
        return f.vacia() ? (int64_t)1 << 40 : (int64_t)f.anio * 10000 + f.mes * 100 + f.dia;
    };
    std::stable_sort(orden.begin(), orden.end(), [&](int a, int b) { return clave(a) < clave(b); });
    const std::vector<uint8_t> antes(base, base + ISLA_TAM_MUSEO);
    for (int k = 0; k < HUECOS_MUSEO; k++)
    {
        const int i = orden[k];
        std::copy(&antes[MUS_FECHAS + i * 4], &antes[MUS_FECHAS + i * 4] + 4, base + MUS_FECHAS + k * 4);
        std::copy(&antes[MUS_OBJETOS + i * 8], &antes[MUS_OBJETOS + i * 8] + 8, base + MUS_OBJETOS + k * 8);
        base[MUS_JUGADORES + k] = antes[MUS_JUGADORES + i];
    }
}

std::vector<uint8_t> exportarMuseo(const std::vector<uint8_t>& m)
{
    return std::vector<uint8_t>(m.begin() + ISLA_MUSEO, m.begin() + ISLA_MUSEO + ISLA_TAM_MUSEO);
}

bool importarMuseo(std::vector<uint8_t>& m, const std::vector<uint8_t>& datos)
{
    if (datos.size() != ISLA_TAM_MUSEO)
        return false;
    std::copy(datos.begin(), datos.end(), m.begin() + ISLA_MUSEO);
    return true;
}

// -------------------------------------------------------------------------------------------- tablón de anuncios
// GSaveBBS (0x77D0 bytes, desde +8 del tablón): fecha, texto (240 letras), firma (48), letra a mano (con el número de
// trazos al final), jugador (su nombre en +0x1C+4), dibujo y número de nota.
namespace {
constexpr uint32_t NOTA_TAM = 0x77D0, NOTA_TEXTO = 0x4, NOTA_FIRMA = 0x1E4, NOTA_TRAZOS = 0x244 + 0x7534,
    NOTA_JUGADOR = 0x777C + 0x1C + 4, NOTA_DIBUJO = 0x77B6, NOTA_NUMERO = 0x77C8;

uint8_t* nota(std::vector<uint8_t>& m, int hueco) { return m.data() + ISLA_TABLON + 8 + hueco * NOTA_TAM; }
const uint8_t* nota(const std::vector<uint8_t>& m, int hueco) { return m.data() + ISLA_TABLON + 8 + hueco * NOTA_TAM; }
} // namespace

std::vector<Nota> notas(const std::vector<uint8_t>& m)
{
    struct Con { Nota n; uint32_t numero; };
    std::vector<Con> v;
    for (int i = 0; i < NUM_NOTAS; i++)
    {
        const uint8_t* p = nota(m, i);
        Nota n;
        n.hueco = i;
        n.texto = deUtf16(p + NOTA_TEXTO, 240);
        const uint32_t numero = le32(p + NOTA_NUMERO);
        if (numero == 0 && n.texto.empty())
            continue;
        n.fecha = leerFecha(p);
        n.firma = deUtf16(p + NOTA_FIRMA, 48);
        const std::string jugador = deUtf16(p + NOTA_JUGADOR, 10);
        n.autor = !n.firma.empty() ? n.firma : !jugador.empty() ? jugador : "Servicios a residentes";
        n.aMano = le32(p + NOTA_TRAZOS) != 0 && n.texto.empty();
        n.dibujo = le16(p + NOTA_DIBUJO);
        v.push_back({n, numero});
    }
    // De la más nueva a la más vieja; con la misma fecha, la de número mayor primero.
    auto clave = [](const FechaIsla& f) { return f.vacia() ? -1 : f.anio * 10000 + f.mes * 100 + f.dia; };
    std::stable_sort(v.begin(), v.end(), [&](const Con& a, const Con& b) {
        if (clave(a.n.fecha) != clave(b.n.fecha))
            return clave(a.n.fecha) > clave(b.n.fecha);
        return a.numero > b.numero;
    });
    std::vector<Nota> r;
    for (auto& c : v)
        r.push_back(c.n);
    return r;
}

void escribirNota(std::vector<uint8_t>& m, int hueco, const std::string& texto)
{
    ponerUtf16(nota(m, hueco) + NOTA_TEXTO, LARGO_NOTA + 1, texto);
}

void escribirFirma(std::vector<uint8_t>& m, int hueco, const std::string& firma)
{
    ponerUtf16(nota(m, hueco) + NOTA_FIRMA, LARGO_FIRMA_NOTA + 1, firma);
}

int letras16(const std::string& texto) { return (int)aUtf16(texto).size(); }

} // namespace acnh
