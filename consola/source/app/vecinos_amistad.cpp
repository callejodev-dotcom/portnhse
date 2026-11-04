// La amistad de los vecinos, como FriendshipWindow de NHSE para Mac: de 0 a 255 puntos con cada jugador, que se ven
// como 7 corazones (los niveles del juego), y cómo llama el vecino a cada jugador.
#include <algorithm>
#include <cmath>
#include <map>
#include "../core/vecino.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
constexpr SDL_Color CORAZON = {226, 86, 110, 255};

void cambio(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

// «Jugador de tu isla» o «De visita desde …».
std::string deDonde(int hueco, int r)
{
    if (mismaIsla(partida::principal(), hueco, r, partida::personal(0)))
        return "Jugador de tu isla";
    return "De visita desde " + recuerdo(partida::principal(), hueco, r).isla;
}

// Dónde va cada corazón y la barra en la pantalla de los corazones.
constexpr int TAM = 96, PASO = 112, ARRIBA = 160;
int xCorazon(int k) { return W / 2 - (7 * PASO - (PASO - TAM)) / 2 + k * PASO; }
constexpr SDL_Rect BARRA = {140, 360, 1000, 28};
} // namespace

void corazon(int x, int y, int tam, SDL_Color c)
{
    // Dos círculos arriba y un pico abajo; las filas (uno o dos tramos) se calculan una vez por tamaño.
    struct Tramo { int a, b; };
    static std::map<int, std::vector<std::vector<Tramo>>> filas;
    auto& f = filas[tam];
    if (f.empty())
    {
        const double rr = tam * 0.27, cy = tam * 0.30, punta = tam * 0.92;
        const double cx[2] = {tam * 0.27, tam * 0.73};
        for (int fy = 0; fy < tam; fy++)
        {
            const double yy = fy + 0.5;
            std::vector<Tramo> t;
            if (yy < cy)
            {
                for (double c0 : cx)
                {
                    const double d = rr * rr - (yy - cy) * (yy - cy);
                    if (d > 0)
                        t.push_back({(int)std::lround(c0 - std::sqrt(d)), (int)std::lround(c0 + std::sqrt(d))});
                }
                if (t.size() == 2 && t[0].b >= t[1].a)
                    t = {{t[0].a, t[1].b}};
            }
            else if (yy < punta)
            {
                const double medio = (tam / 2.0) * (punta - yy) / (punta - cy);
                t.push_back({(int)std::lround(tam / 2.0 - medio), (int)std::lround(tam / 2.0 + medio)});
            }
            f.push_back(t);
        }
    }
    for (int fy = 0; fy < tam; fy++)
        for (const Tramo& t : f[fy])
            if (t.b > t.a)
                rect({x + t.a, y + fy, t.b - t.a, 1}, c);
}

std::string resumenAmistad(int hueco, int jugador)
{
    const int r = recuerdoDe(partida::principal(), hueco, partida::personal(jugador));
    const std::string quien = partida::jugadores[jugador].nombre;
    if (r < 0)
        return "Todavía no conoce a " + quien;
    return std::string(NIVELES[nivelAmistad(recuerdo(partida::principal(), hueco, r).amistad)]) + " con " + quien;
}

void corazones(int hueco, int r)
{
    abrir([hueco, r](Vista& v) {
        const Recuerdo x = recuerdo(partida::principal(), hueco, r);
        const int nivel = nivelAmistad(x.amistad);
        auto poner = [hueco, r](int puntos) {
            puntos = std::clamp(puntos, 0, 255);
            if (puntos == recuerdo(partida::principal(), hueco, r).amistad)
                return;
            ponerAmistad(partida::principal(), hueco, r, (uint8_t)puntos);
            cambio(std::string(NIVELES[nivelAmistad((uint8_t)puntos)]) + " (" + std::to_string(puntos) + " puntos)");
        };
        v.titulo = nombre(hueco) + " y " + x.jugador;
        v.explicacion = "Toca un corazón, o sube y baja los puntos con L y R (ZL y ZR, de 10 en 10). " + deDonde(hueco, r) + ".";
        v.dibujo = [x, nivel] {
            for (int k = 0; k < 7; k++)
                corazon(xCorazon(k), ARRIBA, TAM, k < nivel ? CORAZON : CREMA_OSCURA);
            texto(NIVELES[nivel], W / 2, ARRIBA + TAM + 34, MARRON, g_grande, Centro);
            redondo(BARRA, BARRA.h / 2, CREMA_OSCURA);
            const int lleno = BARRA.w * x.amistad / 255;
            if (lleno > 0)
                redondo({BARRA.x, BARRA.y, std::max(lleno, BARRA.h), BARRA.h}, BARRA.h / 2, CORAZON);
            texto(std::to_string(x.amistad) + " de 255 puntos", W / 2, BARRA.y + BARRA.h + 24, MARRON_SUAVE, g_pequena, Centro);
        };
        v.entrada = [poner](u64, u64, int tx, int ty) {
            if (tx < 0)
                return false;
            for (int k = 0; k < 7; k++)
                if (tx >= xCorazon(k) && tx < xCorazon(k) + TAM && ty >= ARRIBA && ty < ARRIBA + TAM)
                {
                    poner(puntosDeNivel(k + 1));
                    return true;
                }
            if (tx >= BARRA.x - 10 && tx <= BARRA.x + BARRA.w + 10 && ty >= BARRA.y - 16 && ty <= BARRA.y + BARRA.h + 16)
            {
                poner((int)std::lround(255.0 * (tx - BARRA.x) / BARRA.w));
                return true;
            }
            return false;
        };
        v.paginar = [poner, hueco, r](int d) { poner(recuerdo(partida::principal(), hueco, r).amistad + d); };
        // Debajo de los corazones: más y menos, los puntos, el apodo y el máximo.
        const int y = 440;
        v.b.push_back({{60, y, 270, 80}, "Un corazón menos", [poner, nivel] { poner(puntosDeNivel(nivel - 1)); }, nivel > 0});
        v.b.push_back({{350, y, 270, 80}, "Un corazón más", [poner, nivel] { poner(puntosDeNivel(nivel + 1)); }, nivel < 7});
        v.b.push_back({{640, y, 270, 80}, "Escribir los puntos", [poner, x] {
            uint32_t p = x.amistad;
            if (pedirNumero("Puntos de amistad (de 0 a 255)", p, 255))
                poner((int)p);
        }});
        v.b.push_back({{930, y, 290, 80}, "Mejores amigos", [poner] { poner(255); }, x.amistad < 255});
        Boton apodo{{60, y + 96, 560, 80}, "Te llama: " + (x.apodo.empty() ? x.jugador : x.apodo), [hueco, r, x] {
            std::string t = x.apodo;
            if (pedirTexto("Cómo llama a " + x.jugador, "Si lo dejas vacío, usará tu nombre.", t, 10))
            {
                ponerApodo(partida::principal(), hueco, r, t);
                cambio(t.empty() ? "Te llamará por tu nombre" : "Te llamará «" + t + "»");
            }
        }};
        apodo.detalle = x.apodo.empty() ? "Tu nombre (no tiene apodo)" : "Un apodo";
        v.b.push_back(apodo);
        v.b.push_back({{850, ABAJO - 76, 370, 64}, "Volver", [] { volver(); }});
        v.teclas = {{"A", "Elegir"}, {"L R", "Puntos"}, {"B", "Volver"}};
    }, 1);
}

void amistad(int hueco)
{
    abrir([hueco](Vista& v) {
        v.titulo = "Amistad con " + nombre(hueco);
        v.explicacion = "Cada vecino tiene de 0 a 255 puntos de amistad con cada jugador (7 corazones). Sube al hablar con él "
                        "cada día y con regalos; baja si le pegas con la red.";
        std::vector<int> usados;
        for (int r = 0; r < NUM_RECUERDOS; r++)
            if (recuerdoUsado(partida::principal(), hueco, r))
                usados.push_back(r);
        for (int k = 0; k < (int)usados.size(); k++)
        {
            const int r = usados[k];
            const Recuerdo x = recuerdo(partida::principal(), hueco, r);
            Boton b{{60 + (k % 2) * 590, 170 + (k / 2) * 100, 570, 88}, x.jugador + ": " + NIVELES[nivelAmistad(x.amistad)], [hueco, r] { corazones(hueco, r); }};
            b.detalle = std::to_string(x.amistad) + " de 255 puntos · " + deDonde(hueco, r) + (x.apodo.empty() ? "" : " · Le llama «" + x.apodo + "»");
            v.b.push_back(b);
        }
        if (usados.empty())
            v.explicacion = nombre(hueco) + " todavía no conoce a ningún jugador.";
        bool falta = false;
        for (int r : usados)
            falta |= recuerdo(partida::principal(), hueco, r).amistad < 255;
        v.b.push_back({{60, ABAJO - 76, 500, 64}, "Mejores amigos de todos", [hueco, usados] {
            for (int r : usados)
                ponerAmistad(partida::principal(), hueco, r, 255);
            cambio("Mejores amigos de todos");
        }, falta});
        v.b.push_back({{850, ABAJO - 76, 370, 64}, "Volver", [] { volver(); }});
    });
}

void amistadDeJugador(int jugador)
{
    auto j = std::make_shared<int>(jugador);
    abrir([j](Vista& v) {
        const int yo = *j;
        const std::string quien = partida::jugadores[yo].nombre;
        v.titulo = "Amistad con " + quien;
        v.explicacion = partida::jugadores.size() > 1 ? "Cómo se lleva cada vecino con " + quien + ". Otro jugador con L y R."
                                                      : "Cómo se lleva cada vecino con " + quien + ".";
        for (int i = 0; i < NUM_VECINOS; i++)
        {
            const bool vacio = vecinoEn(partida::principal(), i).empty();
            const int r = vacio ? -1 : recuerdoDe(partida::principal(), i, partida::personal(yo));
            Boton b{{60 + (i / 5) * 590, 140 + (i % 5) * 86, 570, 76}, nombre(i), [i, r] { corazones(i, r); }, r >= 0};
            if (!vacio)
            {
                const uint8_t p = r >= 0 ? recuerdo(partida::principal(), i, r).amistad : 0;
                b.detalle = r >= 0 ? std::string(NIVELES[nivelAmistad(p)]) + " · " + std::to_string(p) + " de 255" : "Todavía no conoce a " + quien;
            }
            v.b.push_back(b);
        }
        v.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
        if (partida::jugadores.size() > 1)
        {
            v.paginar = [j](int d) {
                const int n = (int)partida::jugadores.size();
                *j = ((*j + d) % n + n) % n;
            };
            v.teclas = {{"A", "Elegir"}, {"L R", "Otro jugador"}, {"B", "Volver"}};
        }
    });
}

} // namespace vecinos
