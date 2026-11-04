// Sección «Jugador»: el buzón (postbox.dat del jugador), como MailWindow de NHSE para Mac: la lista de cartas, leer y
// cambiar una (quién la manda, para, mensaje, firma, regalo, leída, favorita, fecha, tipo, papel, envoltorio y
// destinatario), escribir una nueva, borrarla, y exportar o importar cartas (.nhpb, en la SD).
#include <algorithm>
#include <ctime>
#include <memory>
#include "../core/jugador.hpp"
#include "../core/jugador_correo.hpp"
#include "../core/vecinos.hpp"
#include "jugador_comun.hpp"
#include "objetos_ui.hpp"
#include "dibujos.hpp"
#include "partida.hpp"
#include "retratos.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

std::string rutaBuzon(int j = -1) { return partida::jugador(j).carpeta + "/postbox.dat"; }

// El buzón del jugador elegido, o nullptr si no se puede leer (se intenta una vez por jugador).
std::vector<uint8_t>* buzon()
{
    static std::vector<std::string> fallidos;
    const std::string r = rutaBuzon();
    if (std::find(fallidos.begin(), fallidos.end(), r) != fallidos.end())
        return nullptr;
    Archivo* a = partida::otro(r);
    if (!a || a->datos.size() != TAM_BUZON)
    {
        fallidos.push_back(r);
        return nullptr;
    }
    return &a->datos;
}

std::vector<uint8_t>& b() { return *buzon(); }

void cambia(const std::string& que = "Cambiado")
{
    partida::cambiaOtro(rutaBuzon());
    cambiado(que);
}

std::string carpetaCartas() { return std::string(partida::CARPETA) + "cartas/"; }

// El nombre del archivo de una carta exportada, como el Mac: «Carla_Post_Slot12_20240531120000.nhpb».
std::string nombreArchivo(int slot, const std::string& cuando)
{
    std::string n = partida::jugador().nombre;
    for (char& c : n)
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    return n + "_Post_Slot" + std::to_string(slot) + "_" + cuando + ".nhpb";
}

std::string ahora()
{
    const time_t t = time(nullptr);
    char s[32];
    strftime(s, sizeof(s), "%Y%m%d%H%M%S", localtime(&t));
    return s;
}

bool exportar(int slot, const std::string& cuando)
{
    return partida::escribirSd(carpetaCartas() + nombreArchivo(slot, cuando), exportarCarta(b(), slot));
}

// La primera línea del mensaje (o el «para» si no hay mensaje).
std::string resumen(int slot)
{
    std::string m = textoCarta(b(), slot, Mensaje);
    std::replace(m.begin(), m.end(), '\n', ' ');
    const size_t i = m.find_first_not_of(' ');
    return i == std::string::npos ? textoCarta(b(), slot, Para) : m.substr(i);
}

// ------------------------------------------------------------------------------------------------ quién la manda
void elegirVecino(int slot)
{
    // A search with its keyboard: the villagers that match, each with its face.
    auto encontrados = std::make_shared<std::vector<int>>();
    Buscador bu;
    bu.titulo = "¿Qué vecino la manda?";
    bu.explicacion = "Escribe parte de su nombre y toca su cara.";
    bu.filtrar = [encontrados](const std::string& t) {
        encontrados->clear();
        const std::string q = normalizar(t);
        if (q.empty())
            return 0;
        const int n = sizeof(LISTA_VECINOS) / sizeof(LISTA_VECINOS[0]);
        for (int i = 0; i < n; i++)
            if (normalizar(LISTA_VECINOS[i].nombre).find(q) != std::string::npos)
                encontrados->push_back(i);
        return (int)encontrados->size();
    };
    bu.fila = [slot, encontrados](int k) {
        const auto& v = LISTA_VECINOS[(*encontrados)[k]];
        const std::string in = v.interno;
        Fila f{v.nombre, [slot, in] {
            volver();
            const std::string especie = in.substr(0, in.size() - 2);
            const int n = sizeof(ESPECIES) / sizeof(ESPECIES[0]);
            for (int e = 0; e < n; e++)
                if (especie == ESPECIES[e])
                {
                    ponerRemitente(b(), slot, 1, partida::personal(), e, atoi(in.c_str() + in.size() - 2));
                    cambia("Ahora la manda " + remitente(b(), slot));
                }
        }};
        f.imagen = retratos::de(in);
        return f;
    };
    abrir(buscador(bu));
}

void elegirPersonaje(int slot)
{
    std::vector<std::string> nombres;
    std::vector<int> numeros;
    for (const auto& p : PERSONAJES)
        if (p.menu)
            nombres.push_back(p.nombre), numeros.push_back(p.numero);
    elegir("¿Qué personaje la manda?", "", nombres, -1, [slot, numeros](int i) {
        ponerRemitente(b(), slot, 2, partida::personal(), numeros[i]);
        cambia("Ahora la manda " + remitente(b(), slot));
    }, 3);
}

void cambiarRemitente(int slot)
{
    const std::vector<std::string> opciones = {"Un vecino…", "Un personaje especial…", partida::jugador().nombre + " (tú)"};
    elegir("¿Quién manda la carta?", "", opciones, -1, [slot](int i) {
        if (i == 0)
            elegirVecino(slot);
        else if (i == 1)
            elegirPersonaje(slot);
        else
        {
            ponerRemitente(b(), slot, 0, partida::personal());
            cambia("Ahora la mandas tú");
        }
    }, 1);
}

// ---------------------------------------------------------------------------------------------- textos y mensaje
void cambiarTexto(int slot, TextoCarta t, const std::string& titulo)
{
    std::string s = textoCarta(b(), slot, t);
    if (!pedirTexto(titulo, "Hasta " + std::to_string(largoMaximo(t)) + " letras", s, largoMaximo(t)))
        return;
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    ponerTextoCarta(b(), slot, t, s);
    cambia();
}

std::vector<std::string> lineas(const std::string& s)
{
    std::vector<std::string> r;
    if (s.empty())
        return r;
    size_t i = 0;
    for (size_t j; (j = s.find('\n', i)) != std::string::npos; i = j + 1)
        r.push_back(s.substr(i, j - i));
    r.push_back(s.substr(i));
    return r;
}

std::string unir(const std::vector<std::string>& l)
{
    std::string s;
    for (size_t i = 0; i < l.size(); i++)
        s += (i ? "\n" : "") + l[i];
    return s;
}

// Cambia una línea del mensaje (o añade una nueva, con linea = -1). Vacía: se quita.
void cambiarLinea(int slot, int linea)
{
    std::vector<std::string> l = lineas(textoCarta(b(), slot, Mensaje));
    std::string s = linea >= 0 ? l[linea] : "";
    const int quedan = LARGO_MENSAJE - largoTexto(unir(l)) + (linea >= 0 ? largoTexto(s) : -1);
    if (quedan <= 0)
    {
        avisar("El mensaje ya está lleno (" + std::to_string(LARGO_MENSAJE) + " letras).", true);
        return;
    }
    if (!pedirTexto(linea >= 0 ? "Línea " + std::to_string(linea + 1) : "Línea nueva", "Caben " + std::to_string(quedan) + " letras más. Vacía: se quita", s, quedan))
        return;
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    if (linea >= 0 && s.empty())
        l.erase(l.begin() + linea);
    else if (linea >= 0)
        l[linea] = s;
    else if (!s.empty())
        l.push_back(s);
    else
        return;
    const std::string m = unir(l);
    if (largoTexto(m) > LARGO_MENSAJE)
    {
        avisar("No cabe: el mensaje tiene como mucho " + std::to_string(LARGO_MENSAJE) + " letras.", true);
        return;
    }
    ponerTextoCarta(b(), slot, Mensaje, m);
    cambia();
}

Pantalla mensaje(int slot)
{
    Lista l;
    l.titulo = "Mensaje de la carta";
    l.explicacion = [slot] {
        return "Toca una línea para cambiarla (si la dejas vacía, se quita). " + std::to_string(largoTexto(textoCarta(b(), slot, Mensaje))) + " de " +
            std::to_string(LARGO_MENSAJE) + " letras.";
    };
    l.cuantas = [slot] { return (int)lineas(textoCarta(b(), slot, Mensaje)).size(); };
    l.fila = [slot](int i) {
        const std::string s = lineas(textoCarta(b(), slot, Mensaje))[i];
        return Fila{s.empty() ? "(línea vacía)" : s, [slot, i] { cambiarLinea(slot, i); }};
    };
    l.columnas = 1;
    l.porColumna = 8;
    l.alto = 50;
    l.abajo.push_back([slot] { return Fila{"Añadir una línea", [slot] { cambiarLinea(slot, -1); }}; });
    l.abajo.push_back([slot] {
        return Fila{"Borrar el mensaje", [slot] {
            confirmar("Borrar el mensaje", "¿Borrar todo el mensaje de la carta?", "Borrar", [slot] {
                ponerTextoCarta(b(), slot, Mensaje, "");
                cambia("Mensaje borrado");
            });
        }};
    });
    return lista(l);
}

// --------------------------------------------------------------------------------------------- más opciones
std::string nombreOpcion(const Opcion* o, int n, uint16_t codigo)
{
    for (int i = 0; i < n; i++)
        if (o[i].codigo == codigo)
            return o[i].nombre;
    return "Otro (" + std::to_string(codigo) + ")";
}

void elegirOpcion(const std::string& titulo, const Opcion* o, int n, uint16_t actual, std::function<void(uint16_t)> elegida)
{
    std::vector<std::string> nombres;
    int k = -1;
    for (int i = 0; i < n; i++)
    {
        nombres.push_back(o[i].nombre);
        if (o[i].codigo == actual)
            k = i;
    }
    std::vector<uint16_t> codigos;
    for (int i = 0; i < n; i++)
        codigos.push_back(o[i].codigo);
    elegir(titulo, "", nombres, k, [codigos, elegida](int i) { elegida(codigos[i]); }, 3);
}

constexpr int NUM_PAPELES = sizeof(PAPELES_CARTA) / sizeof(PAPELES_CARTA[0]);
constexpr int NUM_ENVOLTORIOS = sizeof(ENVOLTORIOS) / sizeof(ENVOLTORIOS[0]);
constexpr int NUM_CAJAS = sizeof(CAJAS) / sizeof(CAJAS[0]);

void masOpciones(int slot)
{
    abrir([slot](Vista& v) {
        v.titulo = "Opciones avanzadas";
        v.explicacion = "De la carta de " + remitente(b(), slot) + ": el papel, el envoltorio y a quién va.";
        const bool carta = esCarta(b(), slot);
        v.dibujo = [] {
            panel({60, 112, 570, 400}, "La carta", "Post", "");
            panel({650, 112, 570, 400}, "A quién va", "Onepiece", "");
        };
        auto fila = [&](int col, int i, const char* icono, const std::string& que, const std::string& valor, std::function<void()> f) {
            Boton bt{{84 + col * 590, 196 + i * 104, 522, 92}, valor, std::move(f)};
            bt.grande = true;
            bt.imagen = dibujos::menu(icono);
            bt.detalle = que;
            v.b.push_back(bt);
        };
        fila(0, 0, carta ? "Post" : "Cardboard", "Tipo", carta ? "Carta" : "Paquete", [slot] {
            elegir("Tipo", "", {"Paquete", "Carta"}, esCarta(b(), slot) ? 1 : 0, [slot](int i) {
                if ((i == 1) == esCarta(b(), slot))
                    return;
                ponerEsCarta(b(), slot, i == 1);
                cambia();
            });
        });
        if (carta)
            fila(0, 1, "Post", "Papel de carta", nombreOpcion(PAPELES_CARTA, NUM_PAPELES, papelCarta(b(), slot)), [slot] {
                elegirOpcion("Papel de carta", PAPELES_CARTA, NUM_PAPELES, papelCarta(b(), slot), [slot](uint16_t c) {
                    ponerPapelCarta(b(), slot, c);
                    cambia();
                });
            });
        const Opcion* envs = carta ? ENVOLTORIOS : CAJAS;
        const int numEnvs = carta ? NUM_ENVOLTORIOS : NUM_CAJAS;
        fila(0, carta ? 2 : 1, "Present", carta ? "Envoltorio del regalo" : "Caja del paquete", nombreOpcion(envs, numEnvs, envoltorio(b(), slot)),
            [slot, envs, numEnvs, carta] {
                elegirOpcion(carta ? "Envoltorio del regalo" : "Caja", envs, numEnvs, envoltorio(b(), slot), [slot](uint16_t c) {
                    ponerEnvoltorio(b(), slot, (uint8_t)c);
                    cambia();
                });
            });
        fila(1, 0, "Onepiece", "Destinatario", textoCarta(b(), slot, Destinatario), [slot] { cambiarTexto(slot, Destinatario, "Destinatario"); });
        fila(1, 1, "PltPalm", "La isla del destinatario", textoCarta(b(), slot, IslaDestino), [slot] { cambiarTexto(slot, IslaDestino, "Isla del destinatario"); });
        Boton ex{{60, ABAJO - 66, 280, 56}, "Exportar esta carta", [slot] {
            if (exportar(slot, ahora()))
                avisar("Guardada en la SD: /switch/NHSE/cartas/" + nombreArchivo(slot, "…"));
            else
                avisar("No he podido guardarla en la SD.", true);
        }};
        ex.pequena = true;
        v.b.push_back(ex);
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------------- caras
// La cara de quien manda la carta, como el Mac: la del vecino, la del personaje (o el icono de su servicio), o una hoja.
SDL_Texture* cara(int slot)
{
    const uint8_t* c = carta(b(), slot);
    const int tipo = tipoRemitente(b(), slot);
    const uint8_t especie = c[0x4], variante = c[0x5];
    if (tipo == 1)
    {
        constexpr int N = sizeof(ESPECIES) / sizeof(ESPECIES[0]);
        if (especie < N)
        {
            char in[16];
            snprintf(in, sizeof(in), "%s%02d", ESPECIES[especie], variante);
            if (SDL_Texture* t = retratos::de(in))
                return t;
        }
    }
    else if (tipo == 2)
    {
        // Los dibujos de SenderNPCData de NHSE: un retrato o un icono de menú.
        static const std::pair<uint8_t, const char*> DIBUJOS[] = {{22, "rco"}, {25, "rco"}, {40, "bey"}, {41, "chy"}, {42, "pyn"},
            {43, "fox"}, {44, "alp"}, {45, "gulB"}, {46, "tap"}, {47, "pkn"}, {48, "xct"}, {50, "bpt"}, {51, "slo"}, {52, "spn"},
            {53, "man"}, {54, "OneroomCardboard"}, {55, "otg"}, {56, "mnc"}, {57, "kpg"}, {60, "gul"}, {61, "gstA"}, {62, "hgc"},
            {63, "tkkA"}, {64, "boc"}, {65, "SnowCrystal"}, {66, "bey"}, {67, "chy"}, {68, "mol"}, {80, "ott"}, {81, "Leaf"},
            {82, "MoneyBag010"}, {84, "Cardboard"}, {85, "PlaneTicket"}, {86, "Post"}, {87, "dod"}, {88, "owl"}, {89, "Leaf"}};
        for (const auto& [n, d] : DIBUJOS)
            if (n == especie)
            {
                if (SDL_Texture* t = retratos::de(d))
                    return t;
                if (SDL_Texture* t = dibujos::menu(d))
                    return t;
            }
    }
    return dibujos::menu("Leaf");
}

// La carta elegida (el hueco), la misma al volver al buzón.
int g_elegida = -1;

// ------------------------------------------------------------------------------------------------- el buzón
void importar()
{
    const std::vector<std::string> archivos = partida::listarSd(carpetaCartas(), ".nhpb");
    if (archivos.empty())
    {
        avisar("No hay cartas en la SD (/switch/NHSE/cartas, archivos .nhpb).", true);
        return;
    }
    elegir("Importar una carta", "Las cartas de /switch/NHSE/cartas en la SD.", archivos, -1, [archivos](int i) {
        std::vector<uint8_t> d;
        if (!partida::leerSd(carpetaCartas() + archivos[i], d))
        {
            avisar("No he podido leerla.", true);
            return;
        }
        const int slot = importarCarta(b(), d);
        if (slot == -2)
            avisar("Ese archivo no es una carta exportada con NHSE.", true);
        else if (slot < 0)
            avisar("El buzón está lleno. Borra antes alguna carta.", true);
        else
        {
            cambia("Carta importada");
            g_elegida = slot;
        }
    }, 1);
}

// El buzón como el Mac: a la izquierda las cartas (la cara, quién, el principio del mensaje y la fecha), a la derecha
// la elegida, con todo lo que se puede cambiar.
Pantalla correo()
{
    struct Estado {
        int pagina = 0;
        int vista = -2; // the chosen letter the last frame (if it changes, its page is shown)
    };
    auto e = std::make_shared<Estado>();
    return [e](Vista& v) {
        const std::vector<int> cartas = cartasUsadas(b());
        v.titulo = "Buzón";
        constexpr int POR_PAGINA = 6, ALTO = 66;
        const int total = (int)cartas.size();
        const int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
        auto donde = std::find(cartas.begin(), cartas.end(), g_elegida);
        if (donde == cartas.end())
        {
            // The chosen one is gone (or none yet): the first one of the page.
            const int primera = std::clamp(e->pagina, 0, paginas - 1) * POR_PAGINA;
            g_elegida = total ? cartas[std::min(primera, total - 1)] : -1;
            donde = std::find(cartas.begin(), cartas.end(), g_elegida);
        }
        else if (g_elegida != e->vista)
            e->pagina = (int)(donde - cartas.begin()) / POR_PAGINA;
        e->vista = g_elegida;
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.explicacion = total == 0 ? "El buzón está vacío. Pulsa «Escribir una carta» para añadir una."
                                   : numero((uint32_t)total) + (total == 1 ? " carta" : " cartas") + ". Elige una para leerla o cambiarla.";
        const int slot = g_elegida;

        // On the left, the letters: the face, who, the start of the message; the date on the right.
        struct Dato {
            std::string fecha, anio;
            bool leida, favorita, regalo;
        };
        std::vector<Dato> datos;
        const int desde = e->pagina * POR_PAGINA, hasta = std::min(total, desde + POR_PAGINA);
        for (int k = desde; k < hasta; k++)
        {
            const int s = cartas[k];
            Boton bt{{60, 108 + (k - desde) * (ALTO + 6), 488, ALTO}, remitente(b(), s), [s] { g_elegida = s; }, true, s == slot};
            bt.grande = true;
            bt.imagen = cara(s);
            bt.detalle = resumen(s);
            v.b.push_back(bt);
            const Fecha f = fechaCarta(b(), s);
            std::string t = textoFecha(f), anio;
            const size_t i = t.rfind(' ');
            if (f.valida() && i != std::string::npos)
                anio = t.substr(i + 1), t = t.substr(0, i);
            datos.push_back({t, anio, leida(b(), s), favorita(b(), s), !regalo(b(), s).vacio()});
        }
        const std::string pagina = paginas > 1 ? std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) : "";
        if (paginas > 1)
        {
            Boton an{{60, 544, 170, 48}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0};
            Boton si{{470, 544, 170, 48}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas};
            an.pequena = si.pequena = true;
            v.b.push_back(an);
            v.b.push_back(si);
            v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        }

        // On the right, the chosen letter.
        std::string elMensaje, cuenta;
        if (slot >= 0)
        {
            elMensaje = textoCarta(b(), slot, Mensaje);
            cuenta = std::to_string(largoTexto(elMensaje)) + " / " + std::to_string(LARGO_MENSAJE);
            Boton de{{676, 112, 310, 56}, "De " + remitente(b(), slot), [slot] { cambiarRemitente(slot); }};
            de.imagen = cara(slot);
            de.detalle = "Cambiar remitente";
            v.b.push_back(de);
            Boton fe{{994, 112, 210, 56}, textoFecha(fechaCarta(b(), slot)), [slot] {
                jugador::pedirFecha("Fecha de la carta", fechaCarta(b(), slot), true, [slot](Fecha f) {
                    ponerFechaCarta(b(), slot, f);
                    cambia();
                });
            }};
            fe.imagen = dibujos::menu("Timer");
            fe.pequena = true;
            v.b.push_back(fe);
            Boton para{{676, 174, 528, 44}, "Para: " + textoCarta(b(), slot, Para), [slot] { cambiarTexto(slot, Para, "Para"); }};
            para.pequena = true;
            v.b.push_back(para);
            Boton firma{{676, 420, 528, 44}, "Firma: " + textoCarta(b(), slot, Firma), [slot] { cambiarTexto(slot, Firma, "Firma"); }};
            firma.pequena = true;
            v.b.push_back(firma);
            const Objeto r = regalo(b(), slot);
            Boton reg{{676, 470, r.vacio() ? 528 : 384, 50}, r.vacio() ? "Sin regalo" : objetos::nombre(r), [slot] {
                objetos::buscar([slot](const Objeto& o) {
                    // Como el Mac: una unidad (las recetas, con su número de receta).
                    ponerRegalo(b(), slot, o.id, o.id == 0x16A2 ? o.cuenta : 0);
                    cambia("Regalo: " + objetos::nombre(regalo(b(), slot)));
                }, "regalarlo");
            }};
            reg.imagen = r.vacio() ? dibujos::menu("Present") : dibujos::de(r);
            reg.detalle = r.vacio() ? "Toca para añadir un regalo" : "El regalo · toca para elegir otro";
            v.b.push_back(reg);
            if (!r.vacio())
            {
                Boton q{{1068, 470, 136, 50}, "Quitar", [slot] {
                    ponerRegalo(b(), slot, NADA, 0);
                    cambia("Regalo quitado");
                }};
                q.pequena = true;
                v.b.push_back(q);
            }
            const bool lei = leida(b(), slot), fav = favorita(b(), slot);
            v.b.push_back(jugador::interruptor({676, 526, 130, 46}, "Leída", lei, [slot, lei] {
                ponerLeida(b(), slot, !lei);
                cambia(lei ? "Sin leer" : "Leída");
            }));
            v.b.push_back(jugador::interruptor({814, 526, 160, 46}, "Favorita", fav, [slot, fav] {
                ponerFavorita(b(), slot, !fav);
                cambia(fav ? "Ya no es favorita" : "Favorita");
            }));
            Boton m{{982, 526, 222, 46}, "Cambiar el mensaje", [slot] { abrir(mensaje(slot)); }};
            m.pequena = true;
            v.b.push_back(m);
        }
        const bool paquete = slot >= 0 && !esCarta(b(), slot);
        v.dibujo = [datos, pagina, elMensaje, cuenta, slot, paquete] {
            for (size_t k = 0; k < datos.size(); k++)
            {
                const auto& f = datos[k];
                const int y = 108 + (int)k * 72;
                texto(f.fecha, 640, y + 2, MARRON_SUAVE, g_pequena, Derecha);
                texto(f.anio, 640, y + 22, MARRON_SUAVE, g_pequena, Derecha);
                // Little signs: not read (a blue dot), favourite, with a present.
                int x = 640;
                if (f.regalo)
                {
                    x -= 22;
                    jugador::imagen(dibujos::menu("Present"), x, y + 46, 20);
                }
                if (f.favorita)
                {
                    x -= 22;
                    jugador::imagen(dibujos::menu("StarPiece"), x, y + 46, 20);
                }
                if (!f.leida)
                {
                    x -= 16;
                    redondo({x, y + 50, 12, 12}, 6, AZUL);
                }
            }
            if (!pagina.empty())
                texto(pagina, 350, 568, MARRON_SUAVE, g_pequena, Centro);
            jugador::recuadro({660, 104, 560, 478});
            if (slot < 0)
            {
                texto("No hay ninguna carta.", 940, 340, MARRON_SUAVE, nullptr, Centro);
                return;
            }
            // The message, as it is read.
            redondo({676, 224, 528, 190}, 8, jugador::BORDE_MAC);
            redondo({678, 226, 524, 186}, 7, BLANCO);
            texto(elMensaje.empty() ? "(sin mensaje)" : elMensaje, 692, 230, elMensaje.empty() ? MARRON_SUAVE : MARRON, g_pequena, Izquierda, 410);
            texto(cuenta, 1192, 230, MARRON_SUAVE, g_pequena, Derecha);
            if (paquete)
                texto("Paquete", 1192, 376, MARRON_SUAVE, g_pequena, Derecha);
        };

        // At the bottom: write, import, export, back.
        int x = 60;
        auto abajo = [&](const std::string& t, std::function<void()> f, bool activo, bool principal) {
            const int w = anchoTexto(t, g_pequena) + 36;
            Boton bt{{x, ABAJO - 66, w, 56}, t, std::move(f), activo};
            bt.pequena = true;
            bt.principal = principal;
            v.b.push_back(bt);
            x += w + 10;
        };
        abajo("Escribir una carta", [] {
            const int s = nuevaCarta(b(), partida::principal(), partida::personal(), jugador::hoy());
            if (s < 0)
            {
                avisar("El buzón está lleno. Borra antes alguna carta.", true);
                return;
            }
            cambia("Carta nueva");
            g_elegida = s;
        }, true, true);
        abajo("Importar…", importar, true, false);
        abajo("Exportar todas", [] {
            const std::string cuando = ahora();
            int n = 0;
            for (int s : cartasUsadas(b()))
                n += exportar(s, cuando);
            avisar(numero(n) + " cartas guardadas en la SD (/switch/NHSE/cartas).");
        }, total > 0, false);
        // And for the chosen letter: more options and delete it.
        abajo("Más opciones…", [slot] { masOpciones(slot); }, slot >= 0, false);
        abajo("Borrar carta", [slot] {
            confirmar("Borrar la carta", "¿Borrar la carta de " + remitente(b(), slot) + "?", "Borrar", [slot] {
                borrarCarta(b(), slot);
                cambia("Carta borrada");
            });
        }, slot >= 0, false);
        v.b.push_back({{1036, ABAJO - 66, 184, 56}, "Volver", [] { volver(); }});
        v.teclas = {{"A", "Elegir"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}

Registro r1({SeccionJugador, 70, [] { return std::string("Correo"); },
    [] {
        if (!buzon())
            return std::string("No he podido leer el buzón");
        const size_t n = cartasUsadas(b()).size();
        return n == 0 ? std::string("El buzón está vacío") : numero((uint32_t)n) + (n == 1 ? " carta" : " cartas");
    },
    [] {
        if (!buzon())
            avisar("No he podido leer el buzón de " + partida::jugador().nombre + ".", true);
        else
        {
            g_elegida = -1;
            abrir(correo());
        }
    },
    [] { return buzon() != nullptr; }});

} // namespace
