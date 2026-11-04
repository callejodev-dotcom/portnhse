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
#include "partida.hpp"
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
    std::string q;
    if (!pedirTexto("Un vecino", "Escribe parte de su nombre", q) || q.empty())
        return;
    const std::string nq = normalizar(q);
    std::vector<std::string> nombres, internos;
    for (const auto& v : LISTA_VECINOS)
        if (normalizar(v.nombre).find(nq) != std::string::npos)
            nombres.push_back(v.nombre), internos.push_back(v.interno);
    if (nombres.empty())
    {
        avisar("No hay ningún vecino que se llame «" + q + "».", true);
        return;
    }
    elegir("¿Qué vecino la manda?", "", nombres, -1, [slot, internos](int i) {
        const std::string& in = internos[i];
        const std::string especie = in.substr(0, in.size() - 2);
        const int n = sizeof(ESPECIES) / sizeof(ESPECIES[0]);
        for (int e = 0; e < n; e++)
            if (especie == ESPECIES[e])
            {
                ponerRemitente(b(), slot, 1, partida::personal(), e, atoi(in.c_str() + in.size() - 2));
                cambia("Ahora la manda " + remitente(b(), slot));
            }
    }, 3);
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
        v.titulo = "Carta de " + remitente(b(), slot);
        v.explicacion = "Más cosas de la carta.";
        const bool carta = esCarta(b(), slot);
        auto fila = [&](int i, int col, const std::string& t, const std::string& d, std::function<void()> f) {
            Boton bt{{60 + col * 590, 160 + i * 100, 570, 86}, t, std::move(f)};
            bt.detalle = d;
            v.b.push_back(bt);
        };
        fila(0, 0, carta ? "Tipo: carta" : "Tipo: paquete", "Carta o paquete", [slot] {
            elegir("Tipo", "", {"Paquete", "Carta"}, esCarta(b(), slot) ? 1 : 0, [slot](int i) {
                if ((i == 1) == esCarta(b(), slot))
                    return;
                ponerEsCarta(b(), slot, i == 1);
                cambia();
            });
        });
        if (carta)
            fila(1, 0, "Papel: " + nombreOpcion(PAPELES_CARTA, NUM_PAPELES, papelCarta(b(), slot)), "Papel de carta", [slot] {
                elegirOpcion("Papel de carta", PAPELES_CARTA, NUM_PAPELES, papelCarta(b(), slot), [slot](uint16_t c) {
                    ponerPapelCarta(b(), slot, c);
                    cambia();
                });
            });
        const Opcion* envs = carta ? ENVOLTORIOS : CAJAS;
        const int numEnvs = carta ? NUM_ENVOLTORIOS : NUM_CAJAS;
        fila(carta ? 2 : 1, 0, (carta ? "Envoltorio: " : "Caja: ") + nombreOpcion(envs, numEnvs, envoltorio(b(), slot)), carta ? "Envoltorio del regalo" : "Caja del paquete",
            [slot, envs, numEnvs, carta] {
                elegirOpcion(carta ? "Envoltorio del regalo" : "Caja", envs, numEnvs, envoltorio(b(), slot), [slot](uint16_t c) {
                    ponerEnvoltorio(b(), slot, (uint8_t)c);
                    cambia();
                });
            });
        fila(0, 1, "Destinatario: " + textoCarta(b(), slot, Destinatario), "A quién va", [slot] { cambiarTexto(slot, Destinatario, "Destinatario"); });
        fila(1, 1, "Isla: " + textoCarta(b(), slot, IslaDestino), "La isla del destinatario", [slot] { cambiarTexto(slot, IslaDestino, "Isla del destinatario"); });
        fila(2, 1, "Exportar esta carta", "A la SD, en /switch/NHSE/cartas", [slot] {
            if (exportar(slot, ahora()))
                avisar("Guardada en la SD: /switch/NHSE/cartas/" + nombreArchivo(slot, "…"));
            else
                avisar("No he podido guardarla en la SD.", true);
        });
        v.b.push_back({{440, ABAJO - 80, 400, 64}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------------- una carta
void unaCarta(int slot)
{
    abrir([slot](Vista& v) {
        v.titulo = "Carta de " + remitente(b(), slot);
        v.explicacion = esCarta(b(), slot) ? "Toca lo que quieras cambiar." : "Es un paquete. Toca lo que quieras cambiar.";
        auto boton = [&](SDL_Rect r, const std::string& t, const std::string& d, std::function<void()> f, bool marcado = false) {
            Boton bt{r, t, std::move(f), true, marcado};
            bt.detalle = d;
            v.b.push_back(bt);
        };
        boton({60, 150, 570, 80}, "De: " + remitente(b(), slot), "Cambiar quién la manda", [slot] { cambiarRemitente(slot); });
        boton({60, 240, 570, 80}, "Para: " + textoCarta(b(), slot, Para), "Lo de arriba de la carta", [slot] { cambiarTexto(slot, Para, "Para"); });
        boton({60, 330, 570, 80}, "Firma: " + textoCarta(b(), slot, Firma), "Lo de abajo de la carta", [slot] { cambiarTexto(slot, Firma, "Firma"); });
        const Objeto r = regalo(b(), slot);
        boton({60, 420, r.vacio() ? 570 : 410, 80}, r.vacio() ? "Sin regalo" : "Regalo: " + objetos::nombre(r), r.vacio() ? "Añadir un regalo" : "Elegir otro regalo", [slot] {
            objetos::buscar([slot](const Objeto& o) {
                // Como el Mac: una unidad (las recetas, con su número de receta).
                ponerRegalo(b(), slot, o.id, o.id == 0x16A2 ? o.cuenta : 0);
                cambia("Regalo: " + objetos::nombre(regalo(b(), slot)));
            }, "regalarlo");
        });
        if (!r.vacio())
            v.b.push_back({{480, 420, 150, 80}, "Quitar", [slot] {
                ponerRegalo(b(), slot, NADA, 0);
                cambia("Regalo quitado");
            }});

        // El mensaje, como se lee, y el botón para cambiarlo.
        v.dibujo = [slot] {
            redondo({660, 150, 560, 260}, 14, BLANCO);
            const std::string m = textoCarta(b(), slot, Mensaje);
            texto(m.empty() ? "(sin mensaje)" : m, 680, 164, m.empty() ? MARRON_SUAVE : MARRON, g_pequena, Izquierda, 520);
        };
        boton({660, 420, 560, 80}, "Cambiar el mensaje", std::to_string(largoTexto(textoCarta(b(), slot, Mensaje))) + " de " + std::to_string(LARGO_MENSAJE) + " letras",
            [slot] { abrir(mensaje(slot)); });

        const bool lei = leida(b(), slot), fav = favorita(b(), slot);
        boton({60, 515, 275, 70}, lei ? "Leída" : "Sin leer", "", [slot, lei] {
            ponerLeida(b(), slot, !lei);
            cambia(lei ? "Sin leer" : "Leída");
        }, lei);
        boton({355, 515, 275, 70}, fav ? "Favorita" : "No favorita", "", [slot, fav] {
            ponerFavorita(b(), slot, !fav);
            cambia(fav ? "Ya no es favorita" : "Favorita");
        }, fav);
        boton({660, 515, 270, 70}, textoFecha(fechaCarta(b(), slot)), "Fecha", [slot] {
            jugador::pedirFecha("Fecha de la carta", fechaCarta(b(), slot), true, [slot](Fecha f) {
                ponerFechaCarta(b(), slot, f);
                cambia();
            });
        });
        boton({950, 515, 270, 70}, "Más…", "", [slot] { masOpciones(slot); });
        boton({60, ABAJO - 72, 400, 60}, "Borrar esta carta", "", [slot] {
            confirmar("Borrar la carta", "¿Borrar la carta de " + remitente(b(), slot) + "?", "Borrar", [slot] {
                borrarCarta(b(), slot);
                volver(); // the letter is gone
                cambia("Carta borrada");
            });
        });
        v.b.push_back({{820, ABAJO - 72, 400, 60}, "Volver", [] { volver(); }});
    });
}

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
            unaCarta(slot);
        }
    }, 1);
}

Pantalla correo()
{
    auto cartas = std::make_shared<std::vector<int>>();
    Lista l;
    l.titulo = "Buzón de " + partida::jugador().nombre;
    l.explicacion = [cartas] {
        *cartas = cartasUsadas(b());
        if (cartas->empty())
            return std::string("El buzón está vacío. Pulsa «Escribir una carta» para añadir una.");
        return numero((uint32_t)cartas->size()) + (cartas->size() == 1 ? " carta" : " cartas") + ". Elige una para leerla o cambiarla.";
    };
    l.cuantas = [cartas] { return (int)cartas->size(); };
    l.columnas = 1;
    l.porColumna = 6;
    l.fila = [cartas](int i) {
        const int slot = (*cartas)[i];
        const bool lei = leida(b(), slot);
        Fila f{(lei ? "" : "● ") + remitente(b(), slot), [slot] { unaCarta(slot); }};
        std::string d = textoFecha(fechaCarta(b(), slot));
        if (!lei)
            d += " · sin leer";
        if (favorita(b(), slot))
            d += " · favorita";
        if (!regalo(b(), slot).vacio())
            d += " · con regalo";
        f.detalle = d + " · " + resumen(slot);
        return f;
    };
    l.abajo.push_back([] {
        return Fila{"Escribir una carta", [] {
            const int slot = nuevaCarta(b(), partida::principal(), partida::personal(), jugador::hoy());
            if (slot < 0)
            {
                avisar("El buzón está lleno. Borra antes alguna carta.", true);
                return;
            }
            cambia("Carta nueva");
            unaCarta(slot);
        }};
    });
    l.abajo.push_back([] { return Fila{"Importar…", importar}; });
    l.abajo.push_back([cartas] {
        return Fila{"Exportar todas", [cartas] {
            const std::string cuando = ahora();
            int n = 0;
            for (int slot : cartasUsadas(b()))
                n += exportar(slot, cuando);
            avisar(numero(n) + " cartas guardadas en la SD (/switch/NHSE/cartas).");
        }, !cartas->empty()};
    });
    return lista(l);
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
            abrir(correo());
    },
    [] { return buzon() != nullptr; }});

} // namespace
