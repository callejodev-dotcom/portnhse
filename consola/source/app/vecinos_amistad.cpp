// La amistad de los vecinos, como FriendshipWindow de NHSE para Mac: de 0 a 255 puntos con cada jugador, que se ven
// como 7 corazones (los niveles del juego), y cómo llama el vecino a cada jugador.
#include <algorithm>
#include <cmath>
#include <map>
#include "../core/vecino.hpp"
#include "dibujos.hpp"
#include "partida.hpp"
#include "retratos.hpp"
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
constexpr int TAM = 84, PASO = 100, ARRIBA = 196;
int xCorazon(int k) { return W / 2 - (7 * PASO - (PASO - TAM)) / 2 + k * PASO; }
constexpr SDL_Rect BARRA = {140, 368, 1000, 24};
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

void filaCorazones(int x, int y, int tam, int paso, int nivel)
{
    SDL_Texture* t = dibujos::menu("Love");
    for (int k = 0; k < 7; k++)
    {
        if (!t)
        {
            corazon(x + k * paso, y, tam, k < nivel ? CORAZON : CREMA_OSCURA);
            continue;
        }
        const SDL_Rect d = {x + k * paso, y, tam, tam};
        SDL_SetTextureAlphaMod(t, k < nivel ? 255 : 60);
        SDL_RenderCopy(g_r, t, nullptr, &d);
    }
    if (t)
        SDL_SetTextureAlphaMod(t, 255);
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
        v.explicacion = "Toca un corazón o la barra, o sube y baja los puntos con L y R (ZL y ZR, de 10 en 10).";
        const std::string donde = deDonde(hueco, r);
        SDL_Texture* cara = retratos::deHueco(hueco);
        v.dibujo = [x, nivel, donde, cara] {
            panel({60, 112, 1160, 482}, x.jugador, nullptr, donde);
            if (cara)
            {
                const SDL_Rect d = {1220 - 20 - 96, 124, 96, 96};
                SDL_RenderCopy(g_r, cara, nullptr, &d);
            }
            filaCorazones(xCorazon(0), ARRIBA, TAM, PASO, nivel);
            texto(NIVELES[nivel], W / 2, ARRIBA + TAM + 30, MARRON, g_titulo, Centro);
            redondo(BARRA, BARRA.h / 2, CREMA_OSCURA);
            const int lleno = BARRA.w * x.amistad / 255;
            if (lleno > 0)
                redondo({BARRA.x, BARRA.y, std::max(lleno, BARRA.h), BARRA.h}, BARRA.h / 2, AZUL);
            // The knob, as the slider of the Mac.
            const int cx = BARRA.x + lleno;
            redondo({cx - 16, BARRA.y + BARRA.h / 2 - 16, 32, 32}, 16, AZUL);
            redondo({cx - 10, BARRA.y + BARRA.h / 2 - 10, 20, 20}, 10, BLANCO);
            texto(std::to_string(x.amistad) + " de 255 puntos", W / 2, BARRA.y + BARRA.h + 28, MARRON_SUAVE, g_pequena, Centro);
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
            if (tx >= BARRA.x - 10 && tx <= BARRA.x + BARRA.w + 10 && ty >= BARRA.y - 18 && ty <= BARRA.y + BARRA.h + 18)
            {
                poner((int)std::lround(255.0 * (tx - BARRA.x) / BARRA.w));
                return true;
            }
            return false;
        };
        v.paginar = [poner, hueco, r](int d) { poner(recuerdo(partida::principal(), hueco, r).amistad + d); };
        // Under the hearts: more and less, the points, the nickname and the maximum.
        const int y = 452;
        v.b.push_back({{84, y, 260, 56}, "Un corazón menos", [poner, nivel] { poner(puntosDeNivel(nivel - 1)); }, nivel > 0});
        v.b.push_back({{356, y, 260, 56}, "Un corazón más", [poner, nivel] { poner(puntosDeNivel(nivel + 1)); }, nivel < 7});
        v.b.push_back({{628, y, 280, 56}, "Escribir los puntos", [poner, x] {
            uint32_t p = x.amistad;
            if (pedirNumero("Puntos de amistad (de 0 a 255)", p, 255))
                poner((int)p);
        }});
        v.b.push_back({{920, y, 276, 56}, "Mejores amigos", [poner] { poner(255); }, x.amistad < 255});
        Boton apodo{{84, y + 68, 560, 56}, "Te llama: " + (x.apodo.empty() ? x.jugador : x.apodo), [hueco, r, x] {
            std::string t = x.apodo;
            if (pedirTexto("Cómo llama a " + x.jugador, "Si lo dejas vacío, usará tu nombre.", t, 10))
            {
                ponerApodo(partida::principal(), hueco, r, t);
                cambio(t.empty() ? "Te llamará por tu nombre" : "Te llamará «" + t + "»");
            }
        }};
        v.b.push_back(apodo);
        v.dibujo = [d = v.dibujo, x] {
            d();
            texto(x.apodo.empty() ? "Si no tiene apodo, usa el nombre." : "Un apodo que le ha puesto.", 660, 452 + 68 + 28 - TTF_FontHeight(g_pequena) / 2,
                MARRON_SUAVE, g_pequena);
        };
        v.b.push_back({{960, ABAJO - 60, 260, 52}, "Volver", [] { volver(); }});
        v.teclas = {{"A", "Elegir"}, {"L R", "Puntos"}, {"B", "Volver"}};
    }, 1);
}

// Como FriendshipWindow del Mac: arriba cómo funciona, y una tarjeta por jugador con sus corazones (se tocan), el nivel,
// los puntos y cómo le llama.
void amistad(int hueco)
{
    auto pagina = std::make_shared<int>(0);
    abrir([hueco, pagina](Vista& v) {
        constexpr int POR_PAGINA = 2, TARJETA_Y = 238, TARJETA_H = 172, TARJETA_PASO = 184;
        constexpr int CTAM = 50, CPASO = 58; // the hearts
        v.titulo = "Amistad con " + nombre(hueco);
        v.explicacion = "Cómo se lleva " + nombre(hueco) + " con cada jugador. Toca un corazón o usa − y +.";
        std::vector<int> usados;
        for (int r = 0; r < NUM_RECUERDOS; r++)
            if (recuerdoUsado(partida::principal(), hueco, r))
                usados.push_back(r);
        const int paginas = std::max(1, ((int)usados.size() + POR_PAGINA - 1) / POR_PAGINA);
        *pagina = std::clamp(*pagina, 0, paginas - 1);
        struct Tarjeta {
            SDL_Rect r;
            std::string jugador, donde, nivel, puntos;
            int corazones;
        };
        std::vector<Tarjeta> tarjetas;
        std::vector<std::pair<int, SDL_Rect>> zonas; // memory, hearts
        for (int k = 0; k < POR_PAGINA && *pagina * POR_PAGINA + k < (int)usados.size(); k++)
        {
            const int r = usados[*pagina * POR_PAGINA + k];
            const Recuerdo x = recuerdo(partida::principal(), hueco, r);
            const int nivel = nivelAmistad(x.amistad);
            const SDL_Rect rr = {60, TARJETA_Y + k * TARJETA_PASO, 1160, TARJETA_H};
            tarjetas.push_back({rr, x.jugador, deDonde(hueco, r), NIVELES[nivel], std::to_string(x.amistad) + " de 255 puntos", nivel});
            zonas.push_back({r, {rr.x + 24, rr.y + 92, 7 * CPASO, CTAM}});
            auto poner = [hueco, r](int puntos) {
                puntos = std::clamp(puntos, 0, 255);
                ponerAmistad(partida::principal(), hueco, r, (uint8_t)puntos);
                cambio(std::string(NIVELES[nivelAmistad((uint8_t)puntos)]) + " (" + std::to_string(puntos) + " puntos)");
            };
            Boton apodo{{rr.x + rr.w - 24 - 330, rr.y + 18, 330, 52}, "Te llama: " + (x.apodo.empty() ? x.jugador : x.apodo), [hueco, r, x] {
                std::string t = x.apodo;
                if (pedirTexto("Cómo llama a " + x.jugador, "Si lo dejas vacío, usará tu nombre.", t, 10))
                {
                    ponerApodo(partida::principal(), hueco, r, t);
                    cambio(t.empty() ? "Le llamará por su nombre" : "Le llamará «" + t + "»");
                }
            }};
            v.b.push_back(apodo);
            const int by = rr.y + 92, bx = rr.x + rr.w - 24 - 330;
            v.b.push_back({{bx, by, 96, 56}, "−", [poner, nivel] { poner(puntosDeNivel(nivel - 1)); }, nivel > 0});
            v.b.push_back({{bx + 106, by, 96, 56}, "+", [poner, nivel] { poner(puntosDeNivel(nivel + 1)); }, nivel < 7});
            v.b.push_back({{bx + 212, by, 118, 56}, "Más…", [hueco, r] { corazones(hueco, r); }});
        }
        const bool hayPaginas = paginas > 1;
        if (hayPaginas)
            v.explicacion = "Cómo se lleva " + nombre(hueco) + " con cada jugador (página " + std::to_string(*pagina + 1) + " de " +
                std::to_string(paginas) + ", L y R).";
        v.dibujo = [tarjetas] {
            panel({60, 112, 1160, 114}, "Cómo funciona la amistad", "Love",
                "De 0 a 255 puntos con cada jugador, que se ven como 7 corazones. Suben al hablar con él cada día y con regalos; "
                "bajan si le pegas con la red.");
            for (const auto& t : tarjetas)
            {
                panel(t.r, t.jugador, nullptr, t.donde);
                filaCorazones(t.r.x + 24, t.r.y + 92, CTAM, CPASO, t.corazones);
                const int x = t.r.x + 24 + 7 * CPASO + 16;
                texto(t.nivel, x, t.r.y + 86, MARRON, g_negrita);
                texto(t.puntos, x, t.r.y + 120, MARRON_SUAVE, g_pequena);
            }
            if (tarjetas.empty())
                texto("Todavía no conoce a ningún jugador.", W / 2, 360, MARRON_SUAVE, nullptr, Centro);
        };
        v.entrada = [hueco, zonas](u64, u64, int tx, int ty) {
            if (tx < 0)
                return false;
            for (const auto& z : zonas)
                if (tx >= z.second.x && tx < z.second.x + z.second.w && ty >= z.second.y && ty < z.second.y + z.second.h)
                {
                    const int k = (tx - z.second.x) / CPASO;
                    ponerAmistad(partida::principal(), hueco, z.first, puntosDeNivel(k + 1));
                    cambio(std::string(NIVELES[k + 1]));
                    return true;
                }
            return false;
        };
        bool falta = false;
        for (int r : usados)
            falta |= recuerdo(partida::principal(), hueco, r).amistad < 255;
        const int y = ABAJO - 60;
        v.b.push_back({{60, y, 360, 52}, "Mejores amigos de todos", [hueco, usados] {
            for (int r : usados)
                ponerAmistad(partida::principal(), hueco, r, 255);
            cambio("Mejores amigos de todos");
        }, falta});
        v.b.push_back({{432, y, 300, 52}, "Recuerdos (expertos)…", [hueco] { recuerdos(hueco); }});
        v.b.push_back({{960, y, 260, 52}, "Volver", [] { volver(); }});
        if (hayPaginas)
        {
            v.paginar = [pagina, paginas](int d) { *pagina = std::clamp(*pagina + (d > 0 ? 1 : -1), 0, paginas - 1); };
            v.teclas = {{"A", "Elegir"}, {"L R", "Otros jugadores"}, {"B", "Volver"}};
        }
    });
}

// La amistad de cada vecino con un jugador, como una tabla: su cara y su nombre, los corazones, el nivel y − y +.
void amistadDeJugador(int jugador)
{
    auto j = std::make_shared<int>(jugador);
    abrir([j](Vista& v) {
        const int yo = *j;
        const std::string quien = partida::jugadores[yo].nombre;
        constexpr int ARR = 112, PASO_F = 50, ALTO_F = 44, CT = 30, CP = 34;
        v.titulo = "Amistad con " + quien;
        v.explicacion = partida::jugadores.size() > 1 ? "Cómo se lleva cada vecino con " + quien + ". Otro jugador con L y R."
                                                      : "Cómo se lleva cada vecino con " + quien + ".";
        struct Fila {
            int y, nivel;
            bool conoce;
            std::string texto;
        };
        std::vector<Fila> filas;
        for (int i = 0; i < NUM_VECINOS; i++)
        {
            const bool vacio = vecinoEn(partida::principal(), i).empty();
            const int y = ARR + i * PASO_F;
            const int r = vacio ? -1 : recuerdoDe(partida::principal(), i, partida::personal(yo));
            Boton b{{60, y, 300, ALTO_F}, nombre(i), [i, r] { corazones(i, r); }, r >= 0};
            if (!vacio)
                b.imagen = retratos::deHueco(i);
            v.b.push_back(b);
            if (vacio)
                continue;
            const uint8_t p = r >= 0 ? recuerdo(partida::principal(), i, r).amistad : 0;
            const int nivel = nivelAmistad(p);
            filas.push_back({y, nivel, r >= 0,
                r >= 0 ? std::string(NIVELES[nivel]) + " · " + std::to_string(p) + " de 255" : "Todavía no conoce a " + quien});
            if (r < 0)
                continue;
            auto poner = [i, r](int puntos) {
                puntos = std::clamp(puntos, 0, 255);
                ponerAmistad(partida::principal(), i, r, (uint8_t)puntos);
                cambio(nombre(i) + ": " + NIVELES[nivelAmistad((uint8_t)puntos)]);
            };
            v.b.push_back({{1000, y, 100, ALTO_F}, "−", [poner, nivel] { poner(puntosDeNivel(nivel - 1)); }, nivel > 0});
            v.b.push_back({{1112, y, 108, ALTO_F}, "+", [poner, nivel] { poner(puntosDeNivel(nivel + 1)); }, nivel < 7});
        }
        v.dibujo = [filas] {
            for (const auto& f : filas)
            {
                const int cy = f.y + ALTO_F / 2;
                int x = 380;
                if (f.conoce)
                {
                    filaCorazones(x, cy - CT / 2, CT, CP, f.nivel);
                    x += 7 * CP + 12;
                }
                texto(encajar(f.texto, 990 - x, g_pequena), x, cy - TTF_FontHeight(g_pequena) / 2, f.conoce ? MARRON : MARRON_SUAVE, g_pequena);
            }
        };
        v.b.push_back({{1000, ABAJO - 56, 220, 50}, "Volver", [] { volver(); }});
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
