// Sección «Avanzado»: lo de las herramientas avanzadas de NHSE para Mac. Progreso de la isla y del jugador (marcas),
// clima, huecos abiertos (del editor clásico), comprobar la partida, exportar e importar la partida descifrada. La
// memoria en bruto va en avanzado_bruto.cpp.
#include <algorithm>
#include <ctime>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include "../core/avanzado.hpp"
#include "../core/avanzado_marcas.hpp"
#include "avanzado.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

namespace avanzado {

void conEspera(const std::string& titulo, const std::string& mensaje, std::function<void()> trabajo)
{
    // The screen is copied each frame: what changes goes in a shared state.
    struct Estado {
        bool dibujada = false;
        std::function<void()> trabajo;
    };
    auto e = std::make_shared<Estado>();
    e->trabajo = std::move(trabajo);
    abrir([titulo, mensaje, e](Vista& v) {
        v.titulo = titulo;
        v.atras = [] {};
        v.teclas = {{"…", "Un momento"}};
        v.dibujo = [mensaje] { texto(mensaje, W / 2, H / 2 - 40, MARRON, g_grande, Centro); };
        if (!e->dibujada)
        {
            e->dibujada = true; // first frame: only the message
            return;
        }
        if (e->trabajo)
        {
            auto t = std::move(e->trabajo);
            e->trabajo = nullptr;
            volver();
            t();
        }
    });
}

std::string fecha()
{
    time_t t = time(nullptr);
    char s[32];
    strftime(s, sizeof(s), "%Y-%m-%d %H.%M.%S", localtime(&t));
    return s;
}

std::string carpetaJugador(int j) { return partida::jugador(j).carpeta.substr(1); }

} // namespace avanzado

using namespace avanzado;

namespace {

// ------------------------------------------------------------------------------------------------ marcas de progreso
constexpr int NUM_ISLA = sizeof(MARCAS_ISLA) / sizeof(MARCAS_ISLA[0]);
constexpr int NUM_JUGADOR = sizeof(MARCAS_JUGADOR) / sizeof(MARCAS_JUGADOR[0]);
const char* const FILTROS[4] = {"Ver todas", "Activadas", "Sin activar", "Cambiadas"};
constexpr int FILAS = 6;

struct EstadoMarcas {
    bool isla = true;
    int jugador = 0;
    const MarcaConNombre* lista = nullptr;
    int total = 0;
    std::vector<int16_t> original; // as they were when opened (by position in «lista»)
    std::vector<std::string> claves;
    std::string busqueda;
    int filtro = 0;
    std::vector<int> vistas;
    int pagina = 0;

    std::vector<uint8_t>& datos() { return isla ? partida::principal() : partida::personal(jugador); }
    int16_t valor(int k) { return leerMarca(datos(), isla, lista[k].indice); }
    void poner(int k, int16_t v)
    {
        escribirMarca(datos(), isla, lista[k].indice, v);
        cambia();
    }
    void cambia()
    {
        if (isla)
            partida::cambiaPrincipal();
        else
            partida::cambiaPersonal(jugador);
    }
    void filtrar()
    {
        std::vector<std::string> palabras;
        std::string w;
        for (char c : normalizar(busqueda) + " ")
        {
            if (c != ' ')
                w += c;
            else if (!w.empty())
                palabras.push_back(w), w.clear();
        }
        vistas.clear();
        for (int k = 0; k < total; k++)
        {
            bool ok = true;
            for (const auto& p : palabras)
                ok = ok && claves[k].find(p) != std::string::npos;
            const int16_t v = valor(k);
            ok = ok && (filtro == 0 || (filtro == 1 && v != 0) || (filtro == 2 && v == 0) || (filtro == 3 && v != original[k]));
            if (ok)
                vistas.push_back(k);
        }
        pagina = 0;
    }
    int cambiadas()
    {
        int n = 0;
        for (int k = 0; k < total; k++)
            n += valor(k) != original[k];
        return n;
    }
};

std::string nombreArchivoMarcas(const EstadoMarcas& e)
{
    return fecha() + (e.isla ? " EventFlagLand" : " EventFlagPlayer " + partida::jugador(e.jugador).nombre) + ".nhfl";
}

constexpr const char* CARPETA_MARCAS = "sdmc:/switch/NHSE/marcas/";

void masMarcas(std::shared_ptr<EstadoMarcas> e)
{
    elegir("Más opciones", "Lo que se guarda en la SD se puede volver a cargar aquí o en NHSE para Mac (archivo .nhfl).",
        {"Deshacer todos los cambios", "Guardar en la SD", "Cargar de la SD"}, -1, [e](int o) {
            if (o == 0)
            {
                for (int k = 0; k < e->total; k++)
                    if (e->valor(k) != e->original[k])
                        e->poner(k, e->original[k]);
                e->filtrar();
                avisar("Deshechos los cambios.");
            }
            else if (o == 1)
            {
                const std::string nombre = nombreArchivoMarcas(*e);
                const bool ok = partida::escribirSd(CARPETA_MARCAS + nombre, marcasEnBruto(e->datos(), e->isla));
                avisar(ok ? "Guardadas en /switch/NHSE/marcas/" + nombre : "No he podido escribir en la SD.", !ok);
            }
            else
            {
                const auto archivos = partida::listarSd(CARPETA_MARCAS, ".nhfl");
                if (archivos.empty())
                {
                    avisar("No hay marcas guardadas en /switch/NHSE/marcas/.", true);
                    return;
                }
                elegir("Cargar marcas", "Se cambian todas las marcas por las del archivo.", archivos, -1, [e, archivos](int i) {
                    std::vector<uint8_t> bruto;
                    if (!partida::leerSd(CARPETA_MARCAS + archivos[i], bruto))
                    {
                        avisar("No he podido leer el archivo.", true);
                        return;
                    }
                    if (!cargarMarcas(e->datos(), e->isla, bruto))
                    {
                        avisar("Ese archivo no es de marcas " + std::string(e->isla ? "de la isla" : "de un jugador") + " (tamaño distinto).", true);
                        return;
                    }
                    e->cambia();
                    e->filtrar();
                    cambiado("Cargadas");
                }, 1);
            }
        }, 1);
}

Pantalla marcas(bool isla)
{
    auto e = std::make_shared<EstadoMarcas>();
    e->isla = isla;
    e->jugador = partida::yo;
    e->lista = isla ? MARCAS_ISLA : MARCAS_JUGADOR;
    e->total = isla ? NUM_ISLA : NUM_JUGADOR;
    for (int k = 0; k < e->total; k++)
    {
        e->original.push_back(e->valor(k));
        e->claves.push_back(normalizar(std::string(e->lista[k].texto) + " " + e->lista[k].interno));
    }
    e->filtrar();

    auto buscar = [e] {
        std::string q = e->busqueda;
        if (!pedirTexto("Buscar", "Museo, Tom Nook, Totakeke… (vacío: todas)", q))
            return;
        e->busqueda = q;
        e->filtrar();
        enfocar(0);
    };
    auto filtro = [e] {
        e->filtro = (e->filtro + 1) % 4;
        e->filtrar();
        enfocar(0);
    };
    return [e, buscar, filtro](Vista& v) {
        const int total = (int)e->vistas.size();
        const int paginas = std::max(1, (total + FILAS - 1) / FILAS);
        e->pagina = std::clamp(e->pagina, 0, paginas - 1);
        v.titulo = e->isla ? "Progreso de la isla" : "Progreso de " + partida::jugador(e->jugador).nombre;
        std::string s = e->busqueda.empty() ? "" : "«" + e->busqueda + "» ";
        if (e->filtro)
            s += std::string("(") + FILTROS[e->filtro] + ") ";
        const int cambiadas = e->cambiadas();
        v.explicacion = (s.empty() ? "" : s + "· ") + numero(total) + " de " + numero(e->total) + " · " + std::to_string(cambiadas) +
            (cambiadas == 1 ? " cambiada" : " cambiadas") + " · página " + std::to_string(e->pagina + 1) + " de " + std::to_string(paginas);
        for (int f = 0; f < FILAS && e->pagina * FILAS + f < total; f++)
        {
            const int k = e->vistas[e->pagina * FILAS + f];
            const MarcaConNombre& m = e->lista[k];
            const int16_t valor = e->valor(k);
            const bool siNo = m.maximo == 1;
            const int maximo = m.maximo > 1 ? m.maximo : 32767;
            std::string detalle = siNo ? (valor ? "Sí" : "No") : "Vale " + std::to_string(valor);
            if (valor != e->original[k])
                detalle += " (cambiada)";
            detalle += "  ·  nº " + std::to_string(m.indice) + " · " + m.interno;
            Boton b{{60, 192 + f * 68, 1160, 60}, m.texto, [e, k, siNo, maximo] {
                const int16_t actual = e->valor(k);
                if (siNo)
                {
                    e->poner(k, actual ? 0 : 1);
                    cambiado(actual ? "Desactivada" : "Activada");
                    return;
                }
                uint32_t n = actual < 0 ? 0 : (uint32_t)actual;
                if (!pedirNumero("Marca nº " + std::to_string(e->lista[k].indice) + ": nuevo valor (0 a " + numero(maximo) + ")", n, maximo))
                    return;
                e->poner(k, (int16_t)n);
                cambiado();
            }, true, valor != 0, detalle};
            v.b.push_back(b);
        }
        if (total == 0)
            v.dibujo = [] { texto("No hay ninguna con eso.", W / 2, 360, MARRON_SUAVE, nullptr, Centro); };
        else
        {
            // The whole explanation of the chosen one, above the rows (the row cuts it if it is long).
            const int f = foco();
            if (f >= 0 && f < (int)v.b.size())
            {
                const std::string completo = v.b[f].texto;
                v.dibujo = [completo] { texto(completo, 60, 136, MARRON, g_pequena, Izquierda, 1160); };
            }
        }
        struct Abajo {
            std::string texto;
            std::function<void()> accion;
            bool activo;
            bool marcado = false;
        };
        auto e2 = e;
        std::vector<Abajo> abajo = {
            {"Anteriores", [e2] { e2->pagina--; }, e->pagina > 0},
            {"Siguientes", [e2] { e2->pagina++; }, e->pagina + 1 < paginas},
            {"Buscar…", buscar, true},
            {FILTROS[e->filtro], filtro, true, e->filtro != 0},
            {"Más…", [e2] { masMarcas(e2); }, true},
            {"Volver", [] { volver(); }, true},
        };
        const int ancho = (W - 120 - 5 * 14) / 6;
        for (int i = 0; i < 6; i++)
            v.b.push_back({{60 + i * (ancho + 14), ABAJO - 62, ancho, 52}, abajo[i].texto, abajo[i].accion, abajo[i].activo, abajo[i].marcado});
        v.paginar = [e, paginas](int d) { e->pagina = std::clamp(e->pagina + d, 0, paginas - 1); };
        v.x = buscar;
        v.y = filtro;
        v.menos = [e] { masMarcas(e); };
        v.teclas = {{"A", "Cambiar"}, {"X", "Buscar"}, {"Y", "Ver"}, {"−", "Más"}, {"L R", "Página"}, {"B", "Volver"}};
    };
}

int activadas(bool isla)
{
    int n = 0;
    const auto& d = isla ? partida::principal() : partida::personal();
    const int total = isla ? NUM_ISLA : NUM_JUGADOR;
    for (int k = 0; k < total; k++)
        n += leerMarca(d, isla, (isla ? MARCAS_ISLA : MARCAS_JUGADOR)[k].indice) != 0;
    return n;
}

const char* const AVISO_MARCAS = "Ojo: se pueden repetir o saltar eventos.";

// --------------------------------------------------------------------------------------------------------- clima
void clima(Vista& v)
{
    v.titulo = "Clima";
    v.explicacion = "Las semillas deciden el tiempo de cada día. Cámbialas solo si sabes lo que haces.";
    auto& m = partida::principal();
    auto semilla = [](uint32_t pos, const std::string& titulo) {
        uint32_t n = leer32(partida::principal(), pos);
        if (!pedirNumero(titulo, n, 0xFFFFFFFFu))
            return;
        escribir32(partida::principal(), pos, n);
        partida::cambiaPrincipal();
        cambiado();
    };
    Boton b1{{60, 160, 1160, 100}, "Semilla del clima: " + std::to_string(leer32(m, SEMILLA_CLIMA)),
        [semilla] { semilla(SEMILLA_CLIMA, "Semilla del clima de la isla"); }};
    b1.detalle = "El tiempo de cada día en la isla";
    Boton b2{{60, 280, 1160, 100}, "Islas misteriosas: semilla " + std::to_string(leer32(m, SEMILLA_TOUR)),
        [semilla] { semilla(SEMILLA_TOUR, "Semilla del clima de las islas misteriosas"); }};
    b2.detalle = "El tiempo de las islas que visitas con un billete Nook";
    const bool sur = m[HEMISFERIO_TOUR] != 0;
    Boton b3{{60, 400, 1160, 100}, std::string("Islas misteriosas: hemisferio ") + (sur ? "sur" : "norte"), [] {
        auto& d = partida::principal();
        elegir("Hemisferio de las islas misteriosas", "Como en el editor clásico de NHSE.", {"Norte", "Sur"}, d[HEMISFERIO_TOUR] ? 1 : 0, [](int i) {
            auto& d2 = partida::principal();
            if (d2[HEMISFERIO_TOUR] == i)
                return;
            d2[HEMISFERIO_TOUR] = (uint8_t)i;
            partida::cambiaPrincipal();
            cambiado();
        });
    }};
    b3.detalle = "Las estaciones de las islas que visitas";
    v.b.push_back(b1);
    v.b.push_back(b2);
    v.b.push_back(b3);
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
}

// --------------------------------------------------------------------------------------------- huecos abiertos
void huecos(Vista& v)
{
    v.titulo = "Huecos abiertos de " + partida::jugador().nombre;
    v.explicacion = "Cuántos huecos tiene abiertos, como en el editor clásico de NHSE. Lo normal: 20 y 20 en los bolsillos.";
    const auto& p = partida::personal();
    struct H {
        uint32_t pos;
        const char* nombre;
        const char* titulo;
        uint32_t maximo;
    };
    const H hs[3] = {{HUECOS_BOLSILLOS1, "Bolsillos, primera parte", "Huecos abiertos en la primera parte de los bolsillos (0 a 20)", 20},
        {HUECOS_BOLSILLOS2, "Bolsillos, segunda parte", "Huecos abiertos en la segunda parte de los bolsillos (0 a 20)", 20},
        {HUECOS_ALMACEN, "Almacén", "Huecos abiertos en el almacén (0 a 9.000)", ALMACEN_MAX}};
    for (int i = 0; i < 3; i++)
    {
        const H h = hs[i];
        Boton b{{60, 160 + i * 120, 1160, 100}, std::string(h.nombre) + ": " + numero(leer32(p, h.pos)) + " de " + numero(h.maximo), [h] {
            uint32_t n = leer32(partida::personal(), h.pos);
            if (!pedirNumero(h.titulo, n, h.maximo))
                return;
            escribir32(partida::personal(), h.pos, n);
            partida::cambiaPersonal();
            cambiado();
        }};
        v.b.push_back(b);
    }
    v.b.push_back({{440, ABAJO - 90, 400, 70}, "Volver", [] { volver(); }});
}

// ---------------------------------------------------------------------------------------------- comprobar
struct Resultado {
    std::string nombre;
    std::string detalle;
    int estado; // 0 bien, 1 mal, 2 con cambios sin guardar, 3 no se puede comprobar
    std::string fallos;
};

std::vector<Resultado> comprobarTodo()
{
    std::vector<Resultado> r;
    // Si algo no cuadra en un archivo con cambios sin guardar, es normal: sus comprobaciones se rehacen al guardar.
    auto una = [&r](const std::string& nombre, const std::string& archivo, const std::vector<uint8_t>& d, bool cambiado) {
        Resultado x{nombre, "", 0, ""};
        const Comprobado c = comprobarArchivo(archivo, d);
        if (c.total >= 0 && !c.mal.empty() && cambiado)
        {
            x.estado = 2;
            x.detalle = "Con cambios: se rehace al guardar";
        }
        else if (c.total < 0)
        {
            x.estado = 3;
            x.detalle = "No es de la versión 3.0";
        }
        else if (c.mal.empty())
            x.detalle = c.total == 1 ? "Bien" : "Bien (" + std::to_string(c.total) + " comprobaciones)";
        else
        {
            x.estado = 1;
            x.detalle = std::to_string(c.mal.size()) + " de " + std::to_string(c.total) + " mal";
            for (const auto& m : c.mal)
                x.fallos += (x.fallos.empty() ? "" : ", ") + hex(m.pos) + ": (" + hex(m.desde) + "-" + hex(m.hasta) + ")";
        }
        r.push_back(x);
    };
    // De main.dat no se sabe aparte si tiene cambios: vale con que haya alguno.
    una("main.dat (la isla)", "main.dat", partida::principal(), partida::hayCambios());
    for (int j = 0; j < (int)partida::jugadores.size(); j++)
    {
        const auto& jug = partida::jugadores[j];
        for (const char* n : ARCHIVOS_JUGADOR)
        {
            const std::string archivo = n;
            const std::string nombre = archivo + " de " + jug.nombre;
            if (archivo == "personal.dat")
            {
                una(nombre, archivo, jug.a.datos, jug.cambiado);
                continue;
            }
            const std::string base = jug.carpeta + "/" + archivo.substr(0, archivo.size() - 4);
            Archivo a;
            if (!partida::leerArchivo(base + ".dat", a.datos) || !partida::leerArchivo(base + "Header.dat", a.cabecera))
                continue; // not every player has every file (wherearen.dat)
            cifrar(a.cabecera, a.datos);
            una(nombre, archivo, a.datos, false);
        }
    }
    return r;
}

Pantalla comprobacion(std::shared_ptr<std::vector<Resultado>> res)
{
    Lista l;
    l.titulo = "Comprobar la partida";
    l.explicacion = [res] {
        int mal = 0, pendientes = 0;
        for (const auto& x : *res)
            mal += x.estado == 1, pendientes += x.estado == 2;
        if (mal)
            return "¡Ojo! " + std::to_string(mal) + (mal == 1 ? " archivo tiene" : " archivos tienen") + " comprobaciones que no cuadran. Toca uno para ver cuáles.";
        if (pendientes)
            return std::string("Nada está mal. Lo que tiene cambios sin guardar se comprueba al guardar.");
        return "Todo bien: las comprobaciones de los " + std::to_string(res->size()) + " archivos cuadran.";
    };
    l.cuantas = [res] { return (int)res->size(); };
    l.columnas = 2;
    l.porColumna = 7;
    l.fila = [res](int i) {
        const Resultado& x = (*res)[i];
        Fila f{x.nombre, [x] {
            if (x.estado == 1)
                avisar(x.nombre + ": " + x.fallos, true);
            else if (x.estado == 2)
                avisar("Tiene cambios sin guardar: al guardar se rehacen sus comprobaciones y se vuelve a comprobar.");
            else
                avisar(x.nombre + ": " + x.detalle + ".");
        }};
        f.detalle = x.detalle;
        f.color = x.estado == 0 ? VERDE_OSCURO : x.estado == 1 ? ROJO : x.estado == 2 ? AMARILLO : GRIS;
        return f;
    };
    l.abajo.push_back([res] {
        return Fila{"Volver a comprobar", [res] { conEspera("Comprobar la partida", "Comprobando la partida…", [res] { *res = comprobarTodo(); }); }};
    });
    return lista(l);
}

// ------------------------------------------------------------------------------------- exportar e importar descifrada
constexpr const char* CARPETA_DESCIFRADA = "sdmc:/switch/NHSE/descifrada/";

void exportar()
{
    conEspera("Exportar descifrada", "Guardando la partida descifrada…", [] {
        const std::string nombre = fecha();
        const std::string dest = std::string(CARPETA_DESCIFRADA) + nombre + "/";
        bool ok = partida::escribirSd(dest + "main.dat", paraExportar(partida::principal()));
        int n = ok;
        for (int j = 0; j < (int)partida::jugadores.size() && ok; j++)
        {
            const auto& jug = partida::jugadores[j];
            for (const char* a : ARCHIVOS_JUGADOR)
            {
                const std::string archivo = a;
                std::vector<uint8_t> d;
                if (archivo == "personal.dat")
                    d = paraExportar(jug.a.datos);
                else
                {
                    const std::string base = jug.carpeta + "/" + archivo.substr(0, archivo.size() - 4);
                    std::vector<uint8_t> cabecera;
                    if (!partida::leerArchivo(base + ".dat", d) || !partida::leerArchivo(base + "Header.dat", cabecera))
                        continue;
                    cifrar(cabecera, d);
                }
                ok = ok && partida::escribirSd(dest + carpetaJugador(j) + "/" + archivo, d);
                n += ok;
            }
        }
        if (ok)
            avisar("Guardados " + std::to_string(n) + " archivos en /switch/NHSE/descifrada/" + nombre + "/");
        else
            avisar("No he podido escribir en la SD.", true);
    });
}

std::vector<std::string> carpetasDescifradas()
{
    std::vector<std::string> r;
    DIR* d = opendir(CARPETA_DESCIFRADA);
    if (!d)
        return r;
    while (dirent* e = readdir(d))
    {
        const std::string n = e->d_name;
        struct stat st;
        if (n != "." && n != ".." && stat((CARPETA_DESCIFRADA + n + "/main.dat").c_str(), &st) == 0)
            r.push_back(n);
    }
    closedir(d);
    std::sort(r.rbegin(), r.rend()); // the newest first
    return r;
}

void cargarDe(const std::string& carpeta)
{
    conEspera("Importar descifrada", "Cargando la partida descifrada…", [carpeta] {
        const std::string orig = std::string(CARPETA_DESCIFRADA) + carpeta + "/";
        std::vector<uint8_t> leido;
        std::string cargado, mal;
        auto anadir = [](std::string& s, const std::string& que) { s += (s.empty() ? "" : ", ") + que; };
        if (partida::leerSd(orig + "main.dat", leido))
        {
            if (cargarDescifrado(partida::principal(), leido))
                partida::cambiaPrincipal(), anadir(cargado, "la isla");
            else
                anadir(mal, "main.dat");
        }
        for (int j = 0; j < (int)partida::jugadores.size(); j++)
        {
            if (!partida::leerSd(orig + carpetaJugador(j) + "/personal.dat", leido))
                continue;
            if (cargarDescifrado(partida::personal(j), leido))
            {
                partida::cambiaPersonal(j);
                partida::jugadores[j].nombre = nombreJugador(partida::personal(j));
                anadir(cargado, partida::jugadores[j].nombre);
            }
            else
                anadir(mal, "personal.dat de " + partida::jugadores[j].nombre);
        }
        // Los demás archivos de cada jugador (buzón, foto, perfil, wherearen), si están y tienen su tamaño.
        for (int j = 0; j < (int)partida::jugadores.size(); j++)
            for (const char* a : ARCHIVOS_JUGADOR)
            {
                const std::string archivo = a;
                if (archivo == "personal.dat" || !partida::leerSd(orig + carpetaJugador(j) + "/" + archivo, leido))
                    continue;
                const std::string ruta = partida::jugadores[j].carpeta + "/" + archivo;
                acnh::Archivo* o = partida::otro(ruta);
                if (o && cargarDescifrado(o->datos, leido))
                    partida::cambiaOtro(ruta), anadir(cargado, archivo + " de " + partida::jugadores[j].nombre);
                else
                    anadir(mal, archivo + " de " + partida::jugadores[j].nombre);
            }
        if (!partida::jugadores.empty())
            partida::isla = nombreIsla(partida::personal(0));
        if (cargado.empty())
            avisar(mal.empty() ? "No hay nada que cargar en esa carpeta." : "No se carga nada: " + mal + " no son de la versión 3.0.", true);
        else
            avisar("Cargado: " + cargado + (mal.empty() ? "" : " (no " + mal + ": otro tamaño)") + ". Falta guardar.", !mal.empty());
    });
}

void importar()
{
    const auto carpetas = carpetasDescifradas();
    if (carpetas.empty())
    {
        avisar("No hay ninguna partida descifrada en /switch/NHSE/descifrada/.", true);
        return;
    }
    elegir("Importar descifrada", "Elige la carpeta (de /switch/NHSE/descifrada/). Se cargan main.dat y los archivos de cada jugador.",
        carpetas, -1, [carpetas](int i) {
            const std::string c = carpetas[i];
            confirmar("Importar descifrada", "¿Cambiar la partida por la de «" + c + "»? No se escribe en la consola hasta que pulses «Guardar».",
                "Cargar", [c] { cargarDe(c); });
        }, 1);
}

// ------------------------------------------------------------------------------------------------------- tarjetas
Registro r1({SeccionAvanzado, 10, [] { return std::string("Progreso de la isla"); },
    [] { return numero(activadas(true)) + " de " + numero(NUM_ISLA) + " activadas"; },
    [] {
        abrir(marcas(true));
        avisar(AVISO_MARCAS);
    }});

Registro r2({SeccionAvanzado, 20, [] { return "Progreso de " + partida::jugador().nombre; },
    [] { return numero(activadas(false)) + " de " + numero(NUM_JUGADOR) + " activadas"; },
    [] {
        abrir(marcas(false));
        avisar(AVISO_MARCAS);
    }});

Registro r3({SeccionAvanzado, 30, [] { return std::string("Clima"); },
    [] { return std::string("Semillas del tiempo y hemisferio"); },
    [] { abrir(clima); }});

Registro r4({SeccionAvanzado, 40, [] { return std::string("Huecos abiertos"); },
    [] {
        const auto& p = partida::personal();
        return "Bolsillos " + std::to_string(leer32(p, HUECOS_BOLSILLOS1)) + "+" + std::to_string(leer32(p, HUECOS_BOLSILLOS2)) + ", almacén " +
            numero(leer32(p, HUECOS_ALMACEN));
    },
    [] { abrir(huecos); }});

Registro r5({SeccionAvanzado, 50, [] { return std::string("Comprobar la partida"); },
    [] { return std::string("Que cada archivo esté sano"); },
    [] {
        auto res = std::make_shared<std::vector<Resultado>>();
        conEspera("Comprobar la partida", "Comprobando la partida…", [res] {
            *res = comprobarTodo();
            abrir(comprobacion(res));
        });
    }});

Registro r6({SeccionAvanzado, 60, [] { return std::string("Exportar descifrada"); },
    [] { return std::string("Guardarla sin cifrar en la SD"); },
    [] { exportar(); }});

Registro r7({SeccionAvanzado, 70, [] { return std::string("Importar descifrada"); },
    [] { return std::string("Cargar una descifrada de la SD"); },
    [] { importar(); }});

} // namespace
