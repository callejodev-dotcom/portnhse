// Sección «Isla»: el museo, como el de NHSE para Mac. Las salas (peces, bichos, criaturas marinas, fósiles y arte) como
// pestañas, y en cada una sus piezas con su dibujo, donde se marca lo que ya está expuesto, con cuándo y quién lo donó;
// «Donar todos» y «Donar todo…» (lo que falta en todas).
// En «Fechas y más…», lo del editor clásico del Mac: cambiar el día, quién donó y la pieza de cada donación, ordenar
// la lista por fecha, y guardar el museo en la SD o traerlo (el mismo .nhm que NHSE).
#include <algorithm>
#include <cstring>
#include <ctime>
#include <memory>
#include "../core/isla.hpp"
#include "dibujos.hpp"
#include "isla_comun.hpp"
#include "jugador_comun.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace {

// El dibujo de una pieza: los animales con su icono (en la SD son su acuario), lo demás con su dibujo.
SDL_Texture* dibujoPieza(int i)
{
    const auto& p = PIEZAS_MUSEO[i];
    SDL_Texture* t = p.sala < 3 ? isla::iconoObjeto(p.id) : nullptr;
    return t ? t : dibujos::de(p.id);
}

SDL_Texture* dibujoDe(uint16_t id)
{
    for (int i = 0; i < NUM_PIEZAS; i++)
        if (PIEZAS_MUSEO[i].id == id)
            return dibujoPieza(i);
    return dibujos::de(id);
}

// Lo que ya está donado: se mira una vez por fotograma (son 1024 huecos), no por fila.
std::vector<Donacion> donadas() { return donaciones(partida::principal()); }

int cuantasDonadas(const std::vector<Donacion>& d, int sala)
{
    int n = 0;
    for (int i = INICIO_SALAS[sala]; i < INICIO_SALAS[sala + 1]; i++)
        n += d[i].donada();
    return n;
}

std::string nombreJugador(int j)
{
    return j >= 0 && j < (int)partida::jugadores.size() ? partida::jugadores[j].nombre : std::string();
}

// «Donado el 3 may 2026 por Carli», como MuseumWindow.
std::string detalle(const Donacion& d)
{
    if (!d.donada())
        return "Aún sin donar";
    const std::string quien = nombreJugador(d.jugador);
    if (d.fecha.vacia())
        return quien.empty() ? "Donado" : "Donado por " + quien;
    return "Donado el " + textoFechaIsla(d.fecha) + (quien.empty() ? "" : " por " + quien);
}

// Dona (hoy, a nombre del jugador que se edita) o retira una pieza.
void marcar(int i, bool si)
{
    auto& m = partida::principal();
    const auto& p = PIEZAS_MUSEO[i];
    const bool hecho = si ? donar(m, p.id, partida::yo, hoyIsla()) : retirar(m, p.id);
    if (!hecho)
    {
        avisar(si ? "No queda sitio en el museo" : "No estaba en el museo", true);
        return;
    }
    partida::cambiaPrincipal();
    cambiado(std::string(si ? "Donado: " : "Retirado: ") + p.nombre);
}

// Dona lo que falta de esas piezas, en su orden. Devuelve cuántas.
int donarTodas(int desde, int hasta)
{
    const auto d = donadas();
    const FechaIsla f = hoyIsla();
    int n = 0;
    for (int i = desde; i < hasta; i++)
        if (!d[i].donada())
            n += donar(partida::principal(), PIEZAS_MUSEO[i].id, partida::yo, f);
    if (n)
        partida::cambiaPrincipal();
    return n;
}

const char* const QUE_SALA[NUM_SALAS] = {"peces donados", "bichos donados", "criaturas marinas donadas", "fósiles donados", "obras de arte donadas"};
// ------------------------------------------------------------------------------------------------ fechas y más
std::string nombrePieza(uint16_t id)
{
    for (const auto& p : PIEZAS_MUSEO)
        if (p.id == id)
            return p.nombre;
    Objeto o;
    o.id = id;
    const std::string n = objetos::nombre(o);
    return n.empty() ? "Objeto " + std::to_string(id) : n;
}

// Quién donó: el nombre del jugador, «nadie» (255) o su número si no es de esta isla.
std::string donante(int j)
{
    const std::string n = nombreJugador(j);
    if (!n.empty())
        return n;
    return j == 255 ? std::string("nadie") : "jugador " + std::to_string(j);
}

std::string cuandoYQuien(const EntradaMuseo& e)
{
    return (e.fecha.vacia() ? std::string("Sin fecha") : textoFechaIsla(e.fecha)) + " · " + donante(e.jugador);
}

bool buscarEntrada(int hueco, EntradaMuseo& r)
{
    for (const auto& e : entradasMuseo(partida::principal()))
        if (e.hueco == hueco)
        {
            r = e;
            return true;
        }
    return false;
}

void cambiaMuseo(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

void cambiarFecha(int hueco, FechaIsla actual)
{
    Fecha f;
    f.anio = (uint16_t)actual.anio;
    f.mes = (uint8_t)actual.mes;
    f.dia = (uint8_t)actual.dia;
    jugador::pedirFecha("Día de la donación", f, true, [hueco](Fecha n) {
        const FechaIsla x{n.anio, n.mes, n.dia};
        if (!x.vacia() && !fechaIslaValida(x))
        {
            avisar("Ese día no existe", true);
            return;
        }
        ponerFechaDonacion(partida::principal(), hueco, x);
        cambiaMuseo(x.vacia() ? std::string("Sin fecha") : "Donado el " + textoFechaIsla(x));
    });
}

void cambiarDonante(int hueco, int actual)
{
    std::vector<std::string> op;
    for (const auto& j : partida::jugadores)
        op.push_back(j.nombre);
    const int n = (int)op.size();
    op.push_back("Nadie");
    op.push_back(actual < n || actual == 255 ? std::string("Otro número…") : "Otro número: " + std::to_string(actual) + "…");
    const int marcada = actual < n ? actual : actual == 255 ? n : n + 1;
    elegir("¿Quién la donó?", "El jugador que sale en el museo como quien la donó.", op, marcada, [hueco, n, actual](int i) {
        uint32_t j = i < n ? (uint32_t)i : i == n ? 255u : (uint32_t)actual;
        if (i == n + 1 && !pedirNumero("Número de jugador (0-255)", j, 255))
            return;
        ponerDonante(partida::principal(), hueco, (int)j);
        cambiaMuseo("Donado por " + donante((int)j));
    });
}

// Una donación de la lista: su día, quién la donó y qué pieza es.
void entrada(int hueco, Vista& v)
{
    EntradaMuseo e;
    if (!buscarEntrada(hueco, e))
    {
        volver();
        return;
    }
    v.titulo = "Donación";
    v.explicacion = "El día, quién la donó y qué pieza es.";
    const std::string nombre = nombrePieza(e.id);
    SDL_Texture* dibujo = dibujoDe(e.id);
    v.dibujo = [nombre, dibujo, hueco] {
        isla::recuadro({60, 136, 1160, 100}, nombre, dibujo, "Número " + std::to_string(hueco + 1) + " de la lista del museo.");
    };
    Boton dia{{60, 252, 570, 96}, e.fecha.vacia() ? std::string("Sin fecha") : textoFechaIsla(e.fecha), [hueco, e] { cambiarFecha(hueco, e.fecha); }};
    dia.detalle = "El día que se donó. Toca para cambiarlo.";
    dia.grande = true;
    dia.imagen = dibujos::menu("Timer");
    v.b.push_back(dia);
    Boton hoy{{650, 252, 570, 96}, "Poner hoy", [hueco] {
        const FechaIsla f = hoyIsla();
        ponerFechaDonacion(partida::principal(), hueco, f);
        cambiaMuseo("Donado el " + textoFechaIsla(f));
    }};
    hoy.detalle = textoFechaIsla(hoyIsla());
    hoy.grande = true;
    hoy.imagen = dibujos::menu("Timer");
    v.b.push_back(hoy);
    Boton quien{{60, 360, 570, 96}, "Donó: " + donante(e.jugador), [hueco, e] { cambiarDonante(hueco, e.jugador); }};
    quien.detalle = "Quién sale como que la donó.";
    quien.grande = true;
    quien.imagen = dibujos::menu("Onepiece");
    v.b.push_back(quien);
    Boton pieza{{650, 360, 570, 96}, "Cambiar la pieza…", [hueco] {
        objetos::buscar([hueco](const Objeto& o) {
            ponerPiezaDonacion(partida::principal(), hueco, o.id);
            cambiaMuseo("Ahora es: " + nombrePieza(o.id));
        }, "esta donación");
    }};
    pieza.detalle = "Otra pieza en su lugar, con el mismo día.";
    pieza.imagen = dibujo;
    pieza.grande = true;
    v.b.push_back(pieza);
    Boton quitar{{60, 468, 570, 96}, "Retirar del museo", [id = e.id] {
        confirmar("Retirar", "¿Retirar " + nombrePieza(id) + " del museo? Las donaciones de detrás suben un puesto.", "Retirar", [id] {
            retirar(partida::principal(), id);
            volver();
            cambiaMuseo("Retirado: " + nombrePieza(id));
        });
    }};
    quitar.detalle = "Como desmarcarla en su sala.";
    quitar.grande = true;
    quitar.imagen = dibujos::menu("Can");
    v.b.push_back(quitar);
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// Todas las donaciones, en el orden de la lista del museo.
Pantalla listaEntradas()
{
    struct Estado {
        std::string busqueda;
        std::vector<EntradaMuseo> vistas;
    };
    auto e = std::make_shared<Estado>();
    // Una fila: la pieza con su dibujo, y cuándo y quién la donó.
    auto fila = [](const EntradaMuseo& x) {
        Fila f{nombrePieza(x.id), [h = x.hueco] { abrir([h](Vista& v) { entrada(h, v); }); }};
        f.detalle = cuandoYQuien(x);
        f.imagen = dibujoDe(x.id);
        return f;
    };
    auto buscar = [e, fila] {
        auto encontradas = std::make_shared<std::vector<EntradaMuseo>>();
        Buscador b;
        b.titulo = "Buscar una donación";
        b.explicacion = "Escribe parte del nombre de la pieza y toca la que quieras cambiar.";
        b.texto = e->busqueda;
        b.filtrar = [encontradas](const std::string& t) {
            encontradas->clear();
            const std::string q = normalizar(t);
            for (const auto& x : entradasMuseo(partida::principal()))
                if (q.empty() || normalizar(nombrePieza(x.id)).find(q) != std::string::npos)
                    encontradas->push_back(x);
            return (int)encontradas->size();
        };
        b.fila = [encontradas, fila](int k) { return fila((*encontradas)[k]); };
        b.alSalir = [e](const std::string& t) { e->busqueda = t; };
        abrir(buscador(b));
    };
    Lista l;
    l.titulo = "Fechas y donantes";
    l.cuantas = [e] {
        e->vistas.clear();
        const std::string q = normalizar(e->busqueda);
        for (const auto& x : entradasMuseo(partida::principal()))
            if (q.empty() || normalizar(nombrePieza(x.id)).find(q) != std::string::npos)
                e->vistas.push_back(x);
        return (int)e->vistas.size();
    };
    l.explicacion = [e] {
        if (e->vistas.empty())
            return e->busqueda.empty() ? std::string("No hay nada en el museo.") : "«" + e->busqueda + "»: nada.";
        return (e->busqueda.empty() ? std::string() : "«" + e->busqueda + "»: ") + std::to_string(e->vistas.size()) +
               " donaciones, en el orden del museo. Toca una para cambiarla.";
    };
    l.fila = [e, fila](int k) { return fila(e->vistas[k]); };
    l.porColumna = 6;
    l.abajo.push_back([buscar] { return Fila{"Buscar…", buscar}; });
    l.extra = [buscar](Vista& v) {
        v.x = buscar;
        v.teclas = {{"A", "Cambiar"}, {"X", "Buscar"}, {"L R", "Página"}, {"B", "Volver"}};
    };
    return lista(l);
}

std::string carpetaMuseo() { return std::string(partida::CARPETA) + "museo/"; }

void exportarMuseoSd()
{
    std::string isla;
    for (char c : partida::isla)
        isla += strchr("\\/:*?\"<>|", c) ? '_' : c;
    char cuando[32];
    const time_t t = time(nullptr);
    strftime(cuando, sizeof(cuando), "%Y%m%d%H%M%S", localtime(&t));
    const std::string n = "museo_" + (isla.empty() ? std::string("isla") : isla) + "_" + cuando + ".nhm";
    if (partida::escribirSd(carpetaMuseo() + n, exportarMuseo(partida::principal())))
        avisar("Guardado en la tarjeta SD: /switch/NHSE/museo/" + n);
    else
        avisar("No he podido guardarlo en la tarjeta SD.", true);
}

void importarMuseoSd()
{
    const std::vector<std::string> nombres = partida::listarSd(carpetaMuseo(), ".nhm");
    if (nombres.empty())
    {
        avisar("No hay museos en la carpeta /switch/NHSE/museo de la tarjeta SD.", true);
        return;
    }
    Lista l;
    l.titulo = "Traer un museo";
    l.explicacion = [] { return std::string("Los museos guardados en /switch/NHSE/museo de la tarjeta SD (.nhm). Toca uno para traerlo."); };
    l.cuantas = [nombres] { return (int)nombres.size(); };
    l.fila = [nombres](int i) {
        const std::string n = nombres[i];
        return Fila{n, [n] {
            confirmar("Traer un museo", "¿Cambiar todo el museo por «" + n + "»? Las donaciones de ahora se pierden.", "Cambiar", [n] {
                std::vector<uint8_t> d;
                if (!partida::leerSd(carpetaMuseo() + n, d))
                {
                    avisar("No he podido leer el archivo.", true);
                    return;
                }
                if (!importarMuseo(partida::principal(), d))
                {
                    avisar("Este archivo no es un museo (tiene " + numero((uint32_t)d.size()) + " bytes y un museo tiene " + numero(ISLA_TAM_MUSEO) + ").", true);
                    return;
                }
                volver();
                cambiaMuseo("Museo traído de la SD");
            });
        }};
    };
    l.columnas = 1;
    l.porColumna = 7;
    abrir(lista(l));
}

void museoMas(Vista& v)
{
    v.titulo = "Museo: fechas y más";
    v.explicacion = "Lo mismo que el editor clásico del museo de NHSE.";
    const int n = (int)entradasMuseo(partida::principal()).size();
    struct Opcion4 { std::string t, d; std::function<void()> a; bool activa; };
    const Opcion4 op[4] = {
        {"Fechas y quién donó cada pieza", std::to_string(n) + " donaciones. Cambia el día, quién la donó o la pieza.", [] { abrir(listaEntradas()); }, n > 0},
        {"Ordenar por fecha", "Pone la lista de donaciones de la más vieja a la más nueva (las que no tienen fecha, al final).", [] {
            confirmar("Ordenar por fecha", "¿Ordenar las donaciones del museo por su fecha?", "Ordenar", [] {
                ordenarMuseoPorFecha(partida::principal());
                cambiaMuseo("Museo ordenado por fecha");
            });
        }, n > 1},
        {"Guardar el museo en la tarjeta SD", "En /switch/NHSE/museo, en el formato de NHSE (.nhm).", exportarMuseoSd, true},
        {"Traer un museo de la tarjeta SD", "Cambia todo el museo por uno guardado antes.", importarMuseoSd, true},
    };
    for (int i = 0; i < 4; i++)
    {
        Boton b{{60, 140 + i * 104, 1160, 92}, op[i].t, op[i].a, op[i].activa};
        b.detalle = op[i].d;
        b.grande = true;
        v.b.push_back(b);
    }
    v.b.push_back({{W - 60 - 240, ABAJO - 66, 240, 56}, "Volver", [] { volver(); }});
}

// Como MuseumWindow: arriba las salas como pestañas (con su icono y cuántas hay), lo donado de la sala con su barra,
// ver todo / sin donar / donado, buscar, y las piezas con su dibujo y quién y cuándo las donó.
const char* const ICONO_SALA[NUM_SALAS] = {"FishTai", "InsMonshirocho", "Tako", "Fossil", "Haniwa"};
const char* const FILTROS[3] = {"Todos", "Sin donar", "Donados"};

Pantalla museo()
{
    struct Estado {
        int sala = 0, filtro = 0, pagina = 0;
        std::string busqueda;
        std::vector<std::string> claves; // the names without accents, for searching
    };
    auto e = std::make_shared<Estado>();
    for (const auto& p : PIEZAS_MUSEO)
        e->claves.push_back(normalizar(p.nombre));
    auto buscar = [e] {
        auto encontradas = std::make_shared<std::vector<int>>();
        Buscador b;
        b.titulo = "Buscar en " + std::string(SALAS_MUSEO[e->sala]);
        b.explicacion = "Toca una pieza para donarla o retirarla. Al volver, la sala enseña solo lo buscado.";
        b.texto = e->busqueda;
        b.filtrar = [e, encontradas](const std::string& t) {
            encontradas->clear();
            const std::string q = normalizar(t);
            for (int i = INICIO_SALAS[e->sala]; i < INICIO_SALAS[e->sala + 1]; i++)
                if (q.empty() || e->claves[i].find(q) != std::string::npos)
                    encontradas->push_back(i);
            return (int)encontradas->size();
        };
        b.fila = [encontradas](int k) {
            const int i = (*encontradas)[k];
            const auto d = donadas();
            const bool si = d[i].donada();
            Fila f{PIEZAS_MUSEO[i].nombre, [i, si] { marcar(i, !si); }, true, si, si ? "Donado" : "Sin donar"};
            f.imagen = dibujoPieza(i);
            return f;
        };
        b.alSalir = [e](const std::string& t) {
            e->busqueda = t;
            e->pagina = 0;
        };
        abrir(buscador(b));
    };
    auto otraSala = [e](int s) {
        e->sala = (s + NUM_SALAS) % NUM_SALAS;
        e->pagina = 0;
        e->busqueda.clear();
    };
    return [e, buscar, otraSala](Vista& v) {
        const auto d = donadas();
        const int s = e->sala;
        const int hay = cuantasDonadas(d, s), total = INICIO_SALAS[s + 1] - INICIO_SALAS[s];
        int faltan = 0;
        for (const auto& x : d)
            faltan += !x.donada();
        // Lo que se ve de la sala.
        std::vector<int> vistas;
        const std::string q = normalizar(e->busqueda);
        for (int i = INICIO_SALAS[s]; i < INICIO_SALAS[s + 1]; i++)
        {
            const bool si = d[i].donada();
            if ((e->filtro == 1 && si) || (e->filtro == 2 && !si))
                continue;
            if (!q.empty() && e->claves[i].find(q) == std::string::npos)
                continue;
            vistas.push_back(i);
        }
        constexpr int COLS = 2, FILAS = 5, POR_PAGINA = COLS * FILAS;
        const int paginas = std::max(1, ((int)vistas.size() + POR_PAGINA - 1) / POR_PAGINA);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);

        v.titulo = "Museo";
        v.explicacion = "Marca lo que ya está expuesto. Si desmarcas una pieza, se retira del museo.";
        if (paginas > 1)
            v.explicacion += " Página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas) + ".";
        // «64 de 80 peces donados»; si no cabe, «28 de 40 donadas» (la sala ya se ve en su pestaña).
        std::string resumen = std::to_string(hay) + " de " + std::to_string(total) + " " + QUE_SALA[s];
        if (anchoTexto(resumen, g_negrita) > 430)
            resumen = std::to_string(hay) + " de " + std::to_string(total) + (s == 2 || s == 4 ? " donadas" : " donados");
        const double parte = total ? (double)hay / total : 0;
        const bool nada = vistas.empty();
        v.dibujo = [resumen, parte, nada] {
            texto(encajar(resumen, 430, g_negrita), 60, 202, MARRON, g_negrita);
            isla::progreso({62, 240, 400, 10}, parte);
            if (nada)
                texto("No hay nada aquí.", W / 2, 400, MARRON_SUAVE, nullptr, Centro);
        };

        // Las salas, como pestañas: tan anchas como su nombre, y lo que sobra repartido.
        int anchos[NUM_SALAS], suma = 0;
        for (int k = 0; k < NUM_SALAS; k++)
            suma += anchos[k] = 90 + anchoTexto(SALAS_MUSEO[k], g_pequena);
        const int sobra = (1160 - 12 * (NUM_SALAS - 1) - suma) / NUM_SALAS;
        int x = 60;
        for (int k = 0; k < NUM_SALAS; k++)
        {
            const int h = cuantasDonadas(d, k), t = INICIO_SALAS[k + 1] - INICIO_SALAS[k];
            Boton b{{x, 136, anchos[k] + sobra, 56}, SALAS_MUSEO[k], [otraSala, k] { otraSala(k); }, true, k == s};
            b.detalle = std::to_string(h) + " de " + std::to_string(t);
            b.imagen = dibujos::menu(ICONO_SALA[k]);
            v.b.push_back(b);
            x += anchos[k] + sobra + 12;
        }
        // Ver, buscar y donar la sala.
        x = 500;
        for (int k = 0; k < 3; k++)
        {
            const int w = k == 1 ? 140 : 120;
            Boton b{{x, 204, w, 46}, FILTROS[k], [e, k] {
                e->filtro = k;
                e->pagina = 0;
            }, true, e->filtro == k};
            b.pequena = true;
            v.b.push_back(b);
            x += w + 8;
        }
        Boton bu{{x + 8, 204, 1220 - 170 - 12 - x - 8, 46}, e->busqueda.empty() ? std::string("Buscar…") : "«" + e->busqueda + "»", buscar, true, !e->busqueda.empty()};
        bu.pequena = true;
        v.b.push_back(bu);
        Boton todos{{1220 - 170, 204, 170, 46}, "Donar todos", [s] {
            const int n = donarTodas(INICIO_SALAS[s], INICIO_SALAS[s + 1]);
            cambiado(std::to_string(n) + " donados");
        }, hay < total};
        todos.pequena = true;
        v.b.push_back(todos);

        // Las piezas.
        for (int k = 0; k < POR_PAGINA && e->pagina * POR_PAGINA + k < (int)vistas.size(); k++)
        {
            const int i = vistas[e->pagina * POR_PAGINA + k];
            const bool si = d[i].donada();
            Boton b{{60 + (k / FILAS) * 590, 264 + (k % FILAS) * 66, 570, 58}, PIEZAS_MUSEO[i].nombre, [i, si] { marcar(i, !si); }, true, si};
            b.detalle = detalle(d[i]);
            b.imagen = dibujoPieza(i);
            if (!b.imagen)
                b.imagen = dibujos::menu(ICONO_SALA[s]);
            v.b.push_back(b);
        }

        // Abajo.
        x = 60;
        if (paginas > 1)
        {
            v.b.push_back({{x, ABAJO - 66, 190, 56}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
            v.b.push_back({{x + 204, ABAJO - 66, 190, 56}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
            x += 408;
            v.paginar = [e, paginas](int dd) { e->pagina = std::clamp(e->pagina + dd, 0, paginas - 1); };
        }
        v.b.push_back({{x, ABAJO - 66, 260, 56}, "Donar todo…", [faltan] {
            confirmar("Museo", "¿Donar las " + std::to_string(faltan) + " piezas que faltan en todas las salas? Las dona hoy " + partida::jugador().nombre + ".", "Donar", [] {
                const int n = donarTodas(0, NUM_PIEZAS);
                cambiado(std::to_string(n) + " piezas donadas");
            });
        }, faltan > 0});
        v.b.push_back({{x + 274, ABAJO - 66, 240, 56}, "Fechas y más…", [] { abrir(museoMas); }});
        v.b.push_back({{W - 60 - 200, ABAJO - 66, 200, 56}, "Volver", [] { volver(); }});
        v.x = buscar;
        v.y = [e] {
            e->filtro = (e->filtro + 1) % 3;
            e->pagina = 0;
        };
        v.menos = [otraSala, s] { otraSala(s + 1); };
        v.teclas = {{"A", "Marcar"}, {"X", "Buscar"}, {"Y", "Ver"}, {"−", "Otra sala"}};
        if (paginas > 1)
            v.teclas.push_back({"L R", "Página"});
        v.teclas.push_back({"B", "Volver"});
    };
}

Registro r({SeccionIsla, 50, [] { return std::string("Museo"); },
    [] {
        const auto d = donadas();
        int n = 0;
        for (const auto& x : d)
            n += x.donada();
        return std::to_string(n) + " de " + std::to_string(NUM_PIEZAS) + " piezas donadas";
    },
    [] { abrir(museo()); }});

} // namespace
