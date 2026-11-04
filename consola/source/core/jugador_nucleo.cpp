// El núcleo de la sección Jugador (ver jugador.hpp). Se llama así para no chocar con app/jugador.cpp al compilar.
#include "jugador.hpp"
#include "jugador_correo.hpp"
#include "jugador_listas.hpp"
#include "vecinos.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace acnh {

namespace {
uint16_t l16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t l32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
void p16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
void p32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
bool bitEn(const uint8_t* p, int n) { return (p[n >> 3] >> (n & 7)) & 1; }
void ponerBitEn(uint8_t* p, int n, bool si)
{
    if (si)
        p[n >> 3] |= (uint8_t)(1 << (n & 7));
    else
        p[n >> 3] &= (uint8_t)~(1 << (n & 7));
}

// UTF-8 → UTF-16 (como string de C#).
std::vector<uint16_t> utf16(const std::string& s)
{
    std::vector<uint16_t> r;
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char c = s[i];
        uint32_t cp;
        int n;
        if (c < 0x80)
            cp = c, n = 1;
        else if ((c >> 5) == 6)
            cp = c & 0x1F, n = 2;
        else if ((c >> 4) == 14)
            cp = c & 0x0F, n = 3;
        else
            cp = c & 0x07, n = 4;
        for (int k = 1; k < n && i + k < s.size(); k++)
            cp = (cp << 6) | (s[i + k] & 0x3F);
        i += n;
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            r.push_back((uint16_t)(0xD800 + (cp >> 10)));
            r.push_back((uint16_t)(0xDC00 + (cp & 0x3FF)));
        }
        else
            r.push_back((uint16_t)cp);
    }
    return r;
}

// ¿Qué número tiene este récord en RECORDS?
constexpr int NUM_RECORDS = sizeof(RECORDS) / sizeof(RECORDS[0]);
int recordDe(int indice)
{
    for (int k = 0; k < NUM_RECORDS; k++)
        if (RECORDS[k].indice == indice)
            return k;
    return -1;
}
uint32_t umbral(const Record& r, int sello) { return sello < 5 ? r.umbral[sello] : 0; }
uint8_t* leidoSello(std::vector<uint8_t>& p, int indice, int sello) { return p.data() + RECORDS_POS + 0x800 + indice * 6 + sello; }
uint8_t* fechaSelloEn(std::vector<uint8_t>& p, int indice, int sello) { return p.data() + RECORDS_POS + 0x1400 + (indice * 6 + sello) * 4; }
uint8_t* visibleRecord(std::vector<uint8_t>& p, int indice) { return p.data() + RECORDS_POS + 0x4400 + indice; }
} // namespace

// ------------------------------------------------------------------------------------------------------- comunes
Objeto leerObjeto(const uint8_t* d)
{
    Objeto o;
    o.id = l16(d);
    o.sistema = d[2];
    o.extra = d[3];
    o.cuenta = l16(d + 4);
    o.usos = l16(d + 6);
    return o;
}

void escribirObjeto(uint8_t* d, const Objeto& o)
{
    p16(d, o.id);
    d[2] = o.sistema;
    d[3] = o.extra;
    p16(d + 4, o.cuenta);
    p16(d + 6, o.usos);
}

std::string leerTexto(const uint8_t* p, int max)
{
    std::string s;
    for (int i = 0; i < max; i++)
    {
        uint32_t c = l16(p + 2 * i);
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < max)
        {
            const uint32_t b = l16(p + 2 * (i + 1));
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

void escribirTexto(uint8_t* p, const std::string& s, int max)
{
    const std::vector<uint16_t> u = utf16(s);
    for (int i = 0; i < max; i++)
        p16(p + 2 * i, i < (int)u.size() ? u[i] : 0);
}

int largoTexto(const std::string& s) { return (int)utf16(s).size(); }

bool Fecha::valida() const { return mes >= 1 && mes <= 12 && dia >= 1 && anio >= 1 && dia <= diasDelMes(mes, anio); }

Fecha leerFecha(const uint8_t* p) { return Fecha{l16(p), p[2], p[3]}; }

void escribirFecha(uint8_t* p, const Fecha& f)
{
    p16(p, f.anio);
    p[2] = f.mes;
    p[3] = f.dia;
}

int diasDelMes(int mes, int anio)
{
    static const int dias[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mes == 2)
        return anio == 0 || (anio % 4 == 0 && (anio % 100 != 0 || anio % 400 == 0)) ? 29 : 28;
    return dias[(mes - 1) % 12];
}

Fecha masUnDia(Fecha f)
{
    if (++f.dia > diasDelMes(f.mes, f.anio))
    {
        f.dia = 1;
        if (++f.mes > 12)
            f.mes = 1, f.anio++;
    }
    return f;
}

const char* const MESES[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

std::string textoFecha(const Fecha& f)
{
    if (!f.valida())
        return "Sin fecha";
    return std::to_string(f.dia) + " " + std::string(MESES[f.mes - 1]).substr(0, 3) + " " + std::to_string(f.anio);
}

// ---------------------------------------------------------------------------------------- almacén y papelera
int almacenAbiertos(const std::vector<uint8_t>& p)
{
    const uint32_t n = l32(p.data() + ALMACEN + ALMACEN_MAX * 8);
    return (int)std::clamp<uint32_t>(n, 50, ALMACEN_MAX);
}

Objeto leerAlmacen(const std::vector<uint8_t>& p, int hueco) { return leerObjeto(p.data() + ALMACEN + hueco * 8); }
void escribirAlmacen(std::vector<uint8_t>& p, int hueco, const Objeto& o) { escribirObjeto(p.data() + ALMACEN + hueco * 8, o); }
Objeto leerPapelera(const std::vector<uint8_t>& m, int hueco) { return leerObjeto(m.data() + PAPELERA + hueco * 8); }
void escribirPapelera(std::vector<uint8_t>& m, int hueco, const Objeto& o) { escribirObjeto(m.data() + PAPELERA + hueco * 8, o); }

bool hayHotel(const std::vector<uint8_t>& p) { return l32(p.data() + BILLETES_HOTEL) != 0 || l32(p.data() + BILLETES_HOTEL + 4) != 0; }

// ------------------------------------------------------------------------------------------------------- recetas
bool receta(const std::vector<uint8_t>& p, int i, ListaReceta l) { return bitEn(p.data() + RECETAS_POS + l * 0x100, i); }
void ponerReceta(std::vector<uint8_t>& p, int i, ListaReceta l, bool si) { ponerBitEn(p.data() + RECETAS_POS + l * 0x100, i, si); }

void aprenderReceta(std::vector<uint8_t>& p, int i, bool si)
{
    if (receta(p, i, Sabida) == si)
        return;
    ponerReceta(p, i, Sabida, si);
    ponerReceta(p, i, Nueva, si);
    if (!si)
    {
        ponerReceta(p, i, Fabricada, false);
        ponerReceta(p, i, Favorita, false);
    }
}

int fabricarSabidas(std::vector<uint8_t>& p)
{
    int n = 0;
    for (int i = 0; i < 0x800; i++)
    {
        if (!receta(p, i, Sabida))
            continue;
        n += !receta(p, i, Fabricada);
        ponerReceta(p, i, Fabricada, true);
        ponerReceta(p, i, Nueva, false);
    }
    return n;
}

void olvidarRecetas(std::vector<uint8_t>& p) { std::fill(p.begin() + RECETAS_POS, p.begin() + RECETAS_POS + 0x400, 0); }

// ------------------------------------------------------------------------------------------------------ catálogo
bool recibido(const std::vector<uint8_t>& p, uint16_t id) { return bitEn(p.data() + CATALOGO_POS, id); }

void recibir(std::vector<uint8_t>& p, int k, bool si)
{
    const CatalogoConNombre& c = CATALOGO_NOMBRES[k];
    ponerBitEn(p.data() + CATALOGO_POS, c.id, si);
    for (int i = 0; i < 8; i++)
        if (c.remake >= 0 && (c.colores >> i) & 1)
            ponerBitEn(p.data() + CATALOGO_FIN, c.remake * 8 + i, si);
}

// ---------------------------------------------------------------------------------------------------- reacciones
bool sabeReaccion(const std::vector<uint8_t>& p, uint8_t r)
{
    const uint8_t* l = p.data() + REACCIONES_POS;
    return std::find(l, l + 256, r) != l + 256;
}

bool aprenderReaccion(std::vector<uint8_t>& p, uint8_t r)
{
    // Como TryAddReaction de NHSE: en el primer hueco libre.
    if (sabeReaccion(p, r))
        return true;
    uint8_t* l = p.data() + REACCIONES_POS;
    uint8_t* libre = std::find(l, l + 256, 0);
    if (libre == l + 256)
        return false;
    *libre = r;
    return true;
}

void olvidarReaccion(std::vector<uint8_t>& p, uint8_t r)
{
    // Como ReactionsWindow.Forget: las de detrás suben (con su «nueva»), y sale de la rueda.
    uint8_t* lista = p.data() + REACCIONES_POS;
    uint8_t* nuevas = lista + 256 + NUM_RUEDA;
    int w = 0;
    for (int i = 0; i < 256; i++)
    {
        if (lista[i] == r)
            continue;
        lista[w] = lista[i];
        nuevas[w] = nuevas[i];
        w++;
    }
    for (; w < 256; w++)
        lista[w] = 0, nuevas[w] = 0;
    for (int i = 0; i < NUM_RUEDA; i++)
        if (lista[256 + i] == r)
            lista[256 + i] = 0;
}

uint8_t rueda(const std::vector<uint8_t>& p, int hueco) { return p[REACCIONES_POS + 256 + hueco]; }
void ponerRueda(std::vector<uint8_t>& p, int hueco, uint8_t r) { p[REACCIONES_POS + 256 + hueco] = r; }

const char* nombreReaccion(uint8_t r)
{
    for (const auto& x : REACCIONES_NOMBRES)
        if (x.valor == r)
            return x.nombre;
    return nullptr;
}

// --------------------------------------------------------------------------------------------- récords (millas)
uint32_t cuentaRecord(const std::vector<uint8_t>& p, int indice) { return l32(p.data() + RECORDS_POS + indice * 4); }
void ponerCuentaRecord(std::vector<uint8_t>& p, int indice, uint32_t n) { p32(p.data() + RECORDS_POS + indice * 4, n); }
Fecha fechaSello(const std::vector<uint8_t>& p, int indice, int sello) { return leerFecha(p.data() + RECORDS_POS + 0x1400 + (indice * 6 + sello) * 4); }
void ponerFechaSello(std::vector<uint8_t>& p, int indice, int sello, const Fecha& f) { escribirFecha(fechaSelloEn(p, indice, sello), f); }

bool selloConseguido(const std::vector<uint8_t>& p, int k, int sello)
{
    const Record& r = RECORDS[k];
    const uint32_t n = cuentaRecord(p, r.indice), u = umbral(r, sello);
    return u != 0 ? n >= u : ((n >> sello) & 1) != 0;
}

int sellosConseguidos(const std::vector<uint8_t>& p, int k)
{
    int n = 0;
    for (int i = 0; i < RECORDS[k].sellos; i++)
        n += selloConseguido(p, k, i);
    return n;
}

bool recordPorPasos(int k) { return RECORDS[k].umbral[0] == 0; }

void ponerPaso(std::vector<uint8_t>& p, int k, int sello, bool si)
{
    const int i = RECORDS[k].indice;
    const uint32_t b = 1u << sello;
    ponerCuentaRecord(p, i, si ? cuentaRecord(p, i) | b : cuentaRecord(p, i) & ~b);
}

namespace {
// GiveAll(index, t) de AchievementList.
void darRecord(std::vector<uint8_t>& p, int indice, Fecha t)
{
    const int k = recordDe(indice);
    if (k < 0)
    {
        borrarRecord(p, indice);
        return;
    }
    const Record& r = RECORDS[k];
    uint32_t max = 0;
    for (uint32_t u : r.umbral)
        max = std::max(max, u);
    if (max != 0)
        ponerCuentaRecord(p, indice, std::max(cuentaRecord(p, indice), max));
    *visibleRecord(p, indice) = 1;
    for (int i = 0; i < r.sellos; i++)
    {
        if (*leidoSello(p, indice, i))
            continue;
        if (!fechaSello(p, indice, i).vacia())
            continue;
        ponerFechaSello(p, indice, i, t);
        *leidoSello(p, indice, i) = 1;
        t = masUnDia(t);
    }
}

// CompleteSteps de RecordRow: todos los pasos de un récord por pasos.
void todosLosPasos(std::vector<uint8_t>& p, int k)
{
    if (recordPorPasos(k))
        ponerCuentaRecord(p, RECORDS[k].indice, cuentaRecord(p, RECORDS[k].indice) | ((1u << RECORDS[k].sellos) - 1));
}
} // namespace

void completarRecord(std::vector<uint8_t>& p, int k, Fecha hoy)
{
    darRecord(p, RECORDS[k].indice, hoy);
    todosLosPasos(p, k);
}

void completarRecords(std::vector<uint8_t>& p, Fecha hoy)
{
    for (int i = 0; i < NUM_RECORDS_MAX; i++)
        darRecord(p, i, hoy);
    for (int k = 0; k < NUM_RECORDS; k++)
        todosLosPasos(p, k);
}

void borrarRecord(std::vector<uint8_t>& p, int indice)
{
    std::fill_n(leidoSello(p, indice, 0), 6, 0);
    std::fill_n(fechaSelloEn(p, indice, 0), 24, 0);
    *visibleRecord(p, indice) = 0;
    ponerCuentaRecord(p, indice, 0);
}

void borrarRecords(std::vector<uint8_t>& p)
{
    for (int i = 0; i < NUM_RECORDS_MAX; i++)
        borrarRecord(p, i);
}

int recordsCompletos(const std::vector<uint8_t>& p)
{
    int n = 0;
    for (int k = 0; k < NUM_RECORDS; k++)
        n += sellosConseguidos(p, k) == RECORDS[k].sellos;
    return n;
}

// --------------------------------------------------------------------------------------------- perfil (pasaporte)
Perfil leerPerfil(const std::vector<uint8_t>& p)
{
    Perfil r;
    r.mes = p[CUMPLE];
    r.dia = p[CUMPLE + 1];
    r.fruta = l16(p.data() + FRUTA_PERFIL);
    r.registro = leerFecha(p.data() + REGISTRO_PERFIL);
    r.fundador = p[FUNDADOR_PERFIL] != 0;
    return r;
}

void escribirPerfil(std::vector<uint8_t>& p, const Perfil& r)
{
    if (r.mes >= 1 && r.mes <= 12 && r.dia >= 1)
    {
        p[CUMPLE] = p[CUMPLE_PERFIL] = r.mes;
        p[CUMPLE + 1] = p[CUMPLE_PERFIL + 1] = r.dia;
    }
    p16(p.data() + FRUTA_PERFIL, r.fruta);
    // El Mac guarda la fecha que enseña: sin fecha si no es un día de verdad.
    escribirFecha(p.data() + REGISTRO_PERFIL, r.registro.valida() ? r.registro : Fecha{});
    p[FUNDADOR_PERFIL] = r.fundador ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------- buzón (postbox.dat)
// Posiciones dentro de una carta (Mail de NHSE).
namespace {
constexpr int LEIDA = 0x0, FAVORITA = 0x1, ISLA_JUGADOR = 0x4, NOMBRE_ISLA_JUGADOR = 0x8, ESPECIE = 0x4, VARIANTE = 0x5,
              ISLA_PERSONAJE = 0x8, NOMBRE_ISLA_PERSONAJE = 0xC, ID_REMITENTE = 0x20, NOMBRE_REMITENTE = 0x24, TIPO = 0x5C,
              ISLA_DESTINO = 0x60, NOMBRE_ISLA_DESTINO = 0x64, ID_DESTINO = 0x7C, NOMBRE_DESTINO = 0x80, FECHA = 0xBC,
              REGALO = 0xC0, ENVOLTORIO = 0xC3, TIPO1 = 0xC8, TIPO2 = 0xCC, PARA = 0xCE, MENSAJE = 0x12E, FIRMA = 0x30E,
              PAPEL = 0x36E, ORDEN = 0x374, IDIOMA = 0x390;

std::vector<uint8_t> cartaVacia()
{
    std::vector<uint8_t> c(TAM_CARTA, 0);
    c[REGALO] = 0xFE, c[REGALO + 1] = 0xFF;
    c[ORDEN - 4] = 0xFE, c[ORDEN - 3] = 0xFF;
    return c;
}

void siguientePosicion(std::vector<uint8_t>& b, int slot)
{
    const uint32_t n = l32(b.data() + ULTIMA_CARTA);
    carta(b, slot)[ORDEN] = (uint8_t)n;
    p32(b.data() + ULTIMA_CARTA, n + 1);
}

std::string nombreVecino(int especie, int variante)
{
    const int especies = sizeof(ESPECIES) / sizeof(ESPECIES[0]);
    if (especie >= especies)
        return "";
    char interno[16];
    snprintf(interno, sizeof(interno), "%s%02d", ESPECIES[especie], variante);
    for (const auto& v : LISTA_VECINOS)
        if (!strcmp(v.interno, interno))
            return v.nombre;
    return interno;
}

uint32_t idDe(const std::vector<uint8_t>& personal, int mas) { return l32(personal.data() + ID_JUGADOR + mas); }
} // namespace

std::vector<int> cartasUsadas(const std::vector<uint8_t>& b)
{
    std::vector<int> r;
    for (int i = 0; i < NUM_CARTAS - 1; i++)
    {
        const uint8_t s = carta(b, i)[LEIDA];
        if (s == 3 || s == 4)
            r.push_back(i);
    }
    // La más nueva primero; con la misma fecha, la de número de orden mayor.
    auto dia = [&b](int s) {
        const Fecha f = fechaCarta(b, s);
        return f.valida() ? (f.anio << 9) | (f.mes << 5) | f.dia : 0;
    };
    std::stable_sort(r.begin(), r.end(), [&](int x, int y) {
        const int dx = dia(x), dy = dia(y);
        if (dx != dy)
            return dx > dy;
        return carta(b, x)[ORDEN] > carta(b, y)[ORDEN];
    });
    return r;
}

int cartaLibre(const std::vector<uint8_t>& b)
{
    for (int i = 0; i < NUM_CARTAS; i++)
        if (carta(b, i)[LEIDA] == 0)
            return i;
    return -1;
}

int tipoRemitente(const std::vector<uint8_t>& b, int slot) { return carta(b, slot)[TIPO]; }

std::string remitente(const std::vector<uint8_t>& b, int slot)
{
    const uint8_t* c = carta(b, slot);
    switch (c[TIPO])
    {
    case 0: {
        const std::string n = leerTexto(c + NOMBRE_REMITENTE, 10);
        return n.find_first_not_of(" \t\n") == std::string::npos ? "Un jugador" : n;
    }
    case 1:
        return nombreVecino(c[ESPECIE], c[VARIANTE]);
    default:
        for (const auto& p : PERSONAJES)
            if (p.numero == c[ESPECIE])
                return p.nombre;
        return "Alguien";
    }
}

void ponerRemitente(std::vector<uint8_t>& b, int slot, int tipo, const std::vector<uint8_t>& personal, int especie, int variante)
{
    uint8_t* c = carta(b, slot);
    const bool lei = leida(b, slot), fav = favorita(b, slot);
    std::fill_n(c, 0x60, 0);
    ponerLeida(b, slot, lei);
    ponerFavorita(b, slot, fav);
    c[TIPO] = (uint8_t)tipo;
    const uint8_t* id = personal.data() + ID_JUGADOR;
    switch (tipo)
    {
    case 0:
        p32(c + ISLA_JUGADOR, idDe(personal, 0));
        escribirTexto(c + NOMBRE_ISLA_JUGADOR, leerTexto(id + 4, 10), 10);
        p32(c + ID_REMITENTE, idDe(personal, 0x1C));
        escribirTexto(c + NOMBRE_REMITENTE, leerTexto(id + 0x20, 10), 10);
        break;
    case 1:
        c[ESPECIE] = (uint8_t)especie;
        c[VARIANTE] = (uint8_t)variante;
        p32(c + ISLA_PERSONAJE, idDe(personal, 0));
        escribirTexto(c + NOMBRE_ISLA_PERSONAJE, leerTexto(id + 4, 10), 20); // NHSE writes 20 letters here
        p32(c + ID_REMITENTE, 2);
        break;
    default:
        c[ESPECIE] = (uint8_t)especie;
        c[VARIANTE] = (uint8_t)variante;
        p32(c + ID_REMITENTE, 0);
        break;
    }
    // El idioma de la carta (solo las de jugadores): el de la app, castellano.
    if (tipo == 0)
        escribirTexto(c + IDIOMA, "es-ES", 5);
    else
        std::fill_n(c + IDIOMA, 5, 0);
}

bool leida(const std::vector<uint8_t>& b, int slot) { return carta(b, slot)[LEIDA] != 3; }
void ponerLeida(std::vector<uint8_t>& b, int slot, bool si) { carta(b, slot)[LEIDA] = si ? 4 : 3; }
bool favorita(const std::vector<uint8_t>& b, int slot) { return carta(b, slot)[FAVORITA] != 0; }
void ponerFavorita(std::vector<uint8_t>& b, int slot, bool si) { carta(b, slot)[FAVORITA] = si ? 1 : 0; }
Fecha fechaCarta(const std::vector<uint8_t>& b, int slot) { return leerFecha(carta(b, slot) + FECHA); }
void ponerFechaCarta(std::vector<uint8_t>& b, int slot, const Fecha& f) { escribirFecha(carta(b, slot) + FECHA, f); }

namespace {
int posTexto(TextoCarta t)
{
    switch (t)
    {
    case Para: return PARA;
    case Mensaje: return MENSAJE;
    case Firma: return FIRMA;
    case Destinatario: return NOMBRE_DESTINO;
    default: return NOMBRE_ISLA_DESTINO;
    }
}
} // namespace

int largoMaximo(TextoCarta t)
{
    switch (t)
    {
    case Para: return LARGO_PARA;
    case Mensaje: return LARGO_MENSAJE;
    case Firma: return LARGO_FIRMA;
    default: return 10;
    }
}

std::string textoCarta(const std::vector<uint8_t>& b, int slot, TextoCarta t) { return leerTexto(carta(b, slot) + posTexto(t), largoMaximo(t)); }

void ponerTextoCarta(std::vector<uint8_t>& b, int slot, TextoCarta t, const std::string& s)
{
    escribirTexto(carta(b, slot) + posTexto(t), s, largoMaximo(t));
}

Objeto regalo(const std::vector<uint8_t>& b, int slot) { return leerObjeto(carta(b, slot) + REGALO); }

void ponerRegalo(std::vector<uint8_t>& b, int slot, uint16_t id, uint16_t cuenta)
{
    uint8_t* c = carta(b, slot);
    const uint8_t envuelto = c[ENVOLTORIO];
    Objeto o;
    o.id = id;
    if (id != NADA)
        o.cuenta = cuenta;
    escribirObjeto(c + REGALO, o);
    c[ENVOLTORIO] = id == NADA ? 0 : envuelto;
}

bool esCarta(const std::vector<uint8_t>& b, int slot) { return carta(b, slot)[TIPO1] == 0 && carta(b, slot)[TIPO2] == 1; }

void ponerEsCarta(std::vector<uint8_t>& b, int slot, bool si)
{
    uint8_t* c = carta(b, slot);
    c[TIPO1] = si ? 0 : 1;
    c[TIPO2] = si ? 1 : 0;
    c[ENVOLTORIO] = 0;
    if (!si)
        p16(c + PAPEL, 0);
}

uint16_t papelCarta(const std::vector<uint8_t>& b, int slot) { return l16(carta(b, slot) + PAPEL); }
void ponerPapelCarta(std::vector<uint8_t>& b, int slot, uint16_t codigo) { p16(carta(b, slot) + PAPEL, codigo); }
uint8_t envoltorio(const std::vector<uint8_t>& b, int slot) { return carta(b, slot)[ENVOLTORIO]; }
void ponerEnvoltorio(std::vector<uint8_t>& b, int slot, uint8_t codigo) { carta(b, slot)[ENVOLTORIO] = codigo; }

int nuevaCarta(std::vector<uint8_t>& b, const std::vector<uint8_t>& main, const std::vector<uint8_t>& personal, Fecha hoy)
{
    const int slot = cartaLibre(b);
    if (slot < 0)
        return -1;
    const std::vector<uint8_t> vacia = cartaVacia();
    std::copy(vacia.begin(), vacia.end(), carta(b, slot));
    siguientePosicion(b, slot);

    // El primer vecino de la isla que conocemos; si no hay, Tom Nook.
    int vecino = -1;
    for (int i = 0; i < NUM_VECINOS && vecino < 0; i++)
    {
        const std::string interno = vecinoEn(main, i);
        for (const auto& v : LISTA_VECINOS)
            if (!interno.empty() && interno == v.interno)
                vecino = i;
    }
    if (vecino >= 0)
    {
        const uint8_t* v = main.data() + POS_VECINOS + vecino * TAM_VECINO;
        ponerRemitente(b, slot, 1, personal, v[0], v[1]);
    }
    else
        ponerRemitente(b, slot, 2, personal, 22);
    ponerLeida(b, slot, false);

    uint8_t* c = carta(b, slot);
    const uint8_t* id = personal.data() + ID_JUGADOR;
    const std::string jugador = leerTexto(id + 0x20, 10);
    escribirTexto(c + NOMBRE_DESTINO, jugador, 10);
    p32(c + ID_DESTINO, idDe(personal, 0x1C));
    escribirTexto(c + NOMBRE_ISLA_DESTINO, leerTexto(id + 4, 10), 10);
    p32(c + ISLA_DESTINO, idDe(personal, 0));
    ponerEsCarta(b, slot, true);
    c[ENVOLTORIO] = 0; // (SetMailType of NHSE doesn't touch it: it is still 0 here)
    p16(c + PAPEL, 0);
    ponerTextoCarta(b, slot, Para, jugador);
    ponerTextoCarta(b, slot, Firma, remitente(b, slot));
    ponerFechaCarta(b, slot, hoy);
    return slot;
}

void borrarCarta(std::vector<uint8_t>& b, int slot)
{
    const std::vector<uint8_t> vacia = cartaVacia();
    std::copy(vacia.begin(), vacia.end(), carta(b, slot));
}

int importarCarta(std::vector<uint8_t>& b, const std::vector<uint8_t>& datos)
{
    if (datos.size() != TAM_CARTA)
        return -2;
    const int slot = cartaLibre(b);
    if (slot < 0)
        return -1;
    std::copy(datos.begin(), datos.end(), carta(b, slot));
    siguientePosicion(b, slot);
    return slot;
}

std::vector<uint8_t> exportarCarta(const std::vector<uint8_t>& b, int slot)
{
    const uint8_t* c = carta(b, slot);
    return std::vector<uint8_t>(c, c + TAM_CARTA);
}

} // namespace acnh
