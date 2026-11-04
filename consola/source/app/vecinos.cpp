// Sección «Vecinos»: los vecinos de la isla y la ficha de cada uno (cambiarlo por otro, personalidad, muletilla,
// amistad, paredes y suelo, ropa, muebles, casa, mudanza, bricolaje, y pasarlo a la tarjeta SD o traerlo de ella), como
// la página de vecinos de NHSE para Mac, con la cara de cada uno; y lo del editor clásico: recuerdos, marcas sueltas y su
// diseño. También: amistad de cada jugador, mejores amigos de todos, que se muden todos y guardar todos en la tarjeta SD.
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include "../core/objetos.hpp"
#include "../core/vecino.hpp"
#include "../core/vecinos.hpp"
#include "objetos_ui.hpp"
#include "partida.hpp"
#include "retratos.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

std::string nombre(int hueco)
{
    const std::string interno = vecinoEn(partida::principal(), hueco);
    if (interno.empty())
        return "Parcela libre";
    for (const auto& v : LISTA_VECINOS)
        if (interno == v.interno)
            return v.nombre;
    return interno;
}

std::string archivo(const std::string& nombre, const std::string& extension)
{
    std::string r;
    for (char c : nombre)
        if (!strchr("\\/:*?\"<>|", c))
            r += c;
    return (r.empty() ? std::string("vecino") : r) + "." + extension;
}

} // namespace vecinos

using namespace vecinos;

namespace {

constexpr int NUM_LISTA = sizeof(LISTA_VECINOS) / sizeof(LISTA_VECINOS[0]);

const std::vector<uint8_t>& datosVecinos()
{
    static const std::vector<uint8_t> datos = [] {
        std::vector<uint8_t> d;
        FILE* f = fopen("romfs:/vecinos.bin", "rb");
        if (!f)
            return d;
        fseek(f, 0, SEEK_END);
        d.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        if (fread(d.data(), 1, d.size(), f) != d.size())
            d.clear();
        fclose(f);
        return d;
    }();
    return datos;
}

char inicial(const char* nombre)
{
    const std::string n = normalizar(std::string(nombre, strnlen(nombre, 4)));
    return (char)toupper((unsigned char)n[0]);
}

void cambio(const std::string& que = "Cambiado")
{
    partida::cambiaPrincipal();
    cambiado(que);
}

// ¿Quién viene en lugar del vecino del hueco? Por letras (abajo, o con L y R).
Pantalla elegirVecino(int hueco)
{
    struct Estado {
        char letra = 'A';
        int pagina = 0;
        int focoLetra = -1;
    };
    auto e = std::make_shared<Estado>();
    return [hueco, e](Vista& v) {
        std::vector<int> deLetra;
        for (int i = 0; i < NUM_LISTA; i++)
            if (inicial(LISTA_VECINOS[i].nombre) == e->letra)
                deLetra.push_back(i);
        const int paginas = std::max<int>(1, (deLetra.size() + 17) / 18);
        e->pagina = std::min(e->pagina, paginas - 1);
        v.titulo = "¿Quién viene en lugar de " + nombre(hueco) + "?";
        v.explicacion = "Elige la letra abajo (o con L y R). El nuevo vecino se queda con la misma casa.";
        for (int k = 0; k < 18 && e->pagina * 18 + k < (int)deLetra.size(); k++)
        {
            const auto& vv = LISTA_VECINOS[deLetra[e->pagina * 18 + k]];
            bool yaEsta = false;
            for (int j = 0; j < NUM_VECINOS; j++)
                yaEsta |= vecinoEn(partida::principal(), j) == vv.interno;
            Boton b{{60 + (k / 6) * 390, 140 + (k % 6) * 62, 370, 54}, vv.nombre, [hueco, interno = std::string(vv.interno), nombre = std::string(vv.nombre)] {
                const std::string antes = vecinos::nombre(hueco);
                const std::string mal = cambiarVecino(partida::principal(), hueco, interno, datosVecinos());
                if (mal.empty())
                    partida::cambiaPrincipal();
                volver();
                avisar(mal.empty() ? antes + " se va y llega " + nombre + ". Falta guardar." : mal, !mal.empty());
            }, !yaEsta};
            b.imagen = retratos::de(vv.interno);
            v.b.push_back(b);
        }
        const int primeraLetra = (int)v.b.size();
        for (int l = 0; l < 26; l++)
            v.b.push_back({{40 + l * 46, 525, 42, 50}, std::string(1, (char)('A' + l)), [e, l] {
                e->letra = (char)('A' + l), e->pagina = 0, e->focoLetra = l;
            }, true, 'A' + l == e->letra});
        if (e->focoLetra >= 0)
        {
            enfocar(primeraLetra + e->focoLetra);
            e->focoLetra = -1;
        }
        v.b.push_back({{60, 600, 260, 60}, "Anteriores", [e] { e->pagina--; }, e->pagina > 0});
        v.b.push_back({{340, 600, 260, 60}, "Siguientes", [e] { e->pagina++; }, e->pagina + 1 < paginas});
        v.b.push_back({{960, 600, 260, 60}, "Volver", [] { volver(); }});
        v.paginar = [e](int d) { e->letra = (char)('A' + ((e->letra - 'A' + 26 + d % 26) % 26)), e->pagina = 0; };
        v.teclas = {{"A", "Elegir"}, {"L R", "Otra letra"}, {"B", "Volver"}};
    };
}

// ------------------------------------------------------------------------------------------- ropa y muebles
objetos::Huecos guardado(int hueco, int lista)
{
    objetos::Huecos h;
    h.nombre = (lista == 0 ? "Ropa de " : "Muebles de ") + nombre(hueco);
    h.cuantos = [lista] { return lista == 0 ? NUM_ROPA : NUM_MUEBLES; };
    h.leer = [hueco, lista](int i) { return objetoVecino(partida::principal(), hueco, lista, i); };
    h.escribir = [hueco, lista](int i, const Objeto& o) {
        ponerObjetoVecino(partida::principal(), hueco, lista, i, o);
        partida::cambiaPrincipal();
    };
    return h;
}

int usados(int hueco, int lista)
{
    int n = 0;
    for (int i = 0; i < (lista == 0 ? NUM_ROPA : NUM_MUEBLES); i++)
        n += !objetoVecino(partida::principal(), hueco, lista, i).vacio();
    return n;
}

// ------------------------------------------------------------------------------------------- bricolaje
// El nombre de una receta por su número (los objetos «Receta: …» llevan el número en la cantidad).
std::string nombreReceta(uint16_t receta)
{
    static const std::map<uint16_t, int> indice = [] {
        std::map<uint16_t, int> m;
        for (int i = 0; i < (int)(sizeof(OBJETOS) / sizeof(OBJETOS[0])); i++)
            if (OBJETOS[i].tipo == 1)
                m[OBJETOS[i].cuenta] = i;
        return m;
    }();
    auto it = indice.find(receta);
    if (it == indice.end())
        return "Receta nº " + std::to_string(receta);
    std::string n = OBJETOS[it->second].nombre;
    return n.rfind("Receta: ", 0) == 0 ? n.substr(8) : n;
}

std::string dosCifras(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

std::string resumenBricolaje(int hueco)
{
    const Bricolaje b = bricolaje(partida::principal(), hueco);
    if (!b.haciendo())
        return "No está haciendo nada";
    return "Hace " + nombreReceta(b.receta) + " hasta las " + dosCifras(b.hora) + ":" + dosCifras(b.minuto);
}

void buscarReceta(int hueco)
{
    std::string q;
    if (!pedirTexto("Buscar receta", "Escribe parte del nombre (por ejemplo, «escalera»)", q) || q.empty())
        return;
    std::vector<Opcion> op;
    std::vector<uint16_t> numeros;
    for (int i : buscarObjetos(q))
        if (OBJETOS[i].tipo == 1)
        {
            op.push_back({nombreReceta(OBJETOS[i].cuenta), ""});
            numeros.push_back(OBJETOS[i].cuenta);
        }
    if (op.empty())
    {
        avisar("No hay ninguna receta que se llame «" + q + "».", true);
        return;
    }
    elegirLargo("Recetas con «" + q + "»", "Elige lo que está haciendo " + nombre(hueco) + ".", op, -1, [hueco, numeros](int k) {
        Bricolaje b = bricolaje(partida::principal(), hueco);
        b.receta = numeros[k];
        ponerBricolaje(partida::principal(), hueco, b);
        cambio("Ahora hace " + nombreReceta(numeros[k]));
    });
}

void pantallaBricolaje(Vista& v, int hueco)
{
    const Bricolaje b = bricolaje(partida::principal(), hueco);
    v.titulo = "Bricolaje de " + nombre(hueco);
    v.explicacion = "A veces los vecinos están en casa haciendo algo de bricolaje: qué hacen y a qué hora terminan.";
    auto cambiar = [hueco](std::function<void(Bricolaje&)> f, const std::string& que) {
        Bricolaje n = bricolaje(partida::principal(), hueco);
        f(n);
        ponerBricolaje(partida::principal(), hueco, n);
        cambio(que);
    };
    Boton haciendo{{60, 160, 570, 100}, "Haciendo algo", [b, cambiar] {
        // Como el editor clásico: sin hacer nada, la hora es 255:255; al empezar, las 00:00:00.
        if (b.haciendo())
            cambiar([](Bricolaje& n) { n.hora = n.minuto = 255; }, "Ya no está haciendo nada");
        else
            cambiar([](Bricolaje& n) { n.hora = n.minuto = n.segundo = 0; }, "Está haciendo algo");
    }, true, b.haciendo()};
    haciendo.detalle = b.haciendo() ? "Sí (pulsa para que no)" : "No (pulsa para que sí)";
    v.b.push_back(haciendo);
    Boton receta{{650, 160, 570, 100}, "Qué hace", [hueco] { buscarReceta(hueco); }};
    receta.detalle = nombreReceta(b.receta);
    v.b.push_back(receta);
    Boton hora{{60, 290, 570, 100}, "Hora a la que termina", [b, cambiar] {
        uint32_t h = b.hora;
        if (pedirNumero("¿A qué hora termina? (de 0 a 23)", h, 23))
            cambiar([h](Bricolaje& n) { n.hora = (uint8_t)h; }, "Termina a las " + dosCifras(h) + ":" + dosCifras(b.minuto));
    }, b.haciendo()};
    hora.detalle = b.haciendo() ? dosCifras(b.hora) + ":" + dosCifras(b.minuto) : "—";
    v.b.push_back(hora);
    Boton minuto{{650, 290, 570, 100}, "Minuto en que termina", [b, cambiar] {
        uint32_t m = b.minuto;
        if (pedirNumero("¿En qué minuto termina? (de 0 a 59)", m, 59))
            cambiar([m](Bricolaje& n) { n.minuto = (uint8_t)m; }, "Termina a las " + dosCifras(b.hora) + ":" + dosCifras(m));
    }, b.haciendo()};
    minuto.detalle = hora.detalle;
    v.b.push_back(minuto);
    v.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
}

// ------------------------------------------------------------------------------------------- archivos
void exportar(int hueco)
{
    const std::string n = archivo(nombre(hueco), "nhv2");
    if (partida::escribirSd(std::string(CARPETA) + n, exportarVecino(partida::principal(), hueco)))
        avisar("Guardado en la tarjeta SD: /switch/NHSE/vecinos/" + n);
    else
        avisar("No he podido guardarlo en la tarjeta SD.", true);
}

void ponerDeArchivo(int hueco, const std::vector<uint8_t>& datos, bool hacerSuyo)
{
    const std::string mal = importarVecino(partida::principal(), hueco, datos, hacerSuyo, partida::personal(0));
    if (!mal.empty())
    {
        avisar(mal, true);
        return;
    }
    partida::cambiaPrincipal();
    cambiado("Se ha importado a " + nombre(hueco));
}

void importar(int hueco)
{
    std::vector<std::string> nombres = partida::listarSd(CARPETA, ".nhv2");
    for (const auto& n : partida::listarSd(CARPETA, ".nhv"))
        nombres.push_back(n);
    if (nombres.empty())
    {
        avisar("No hay vecinos en la carpeta /switch/NHSE/vecinos de la tarjeta SD.", true);
        return;
    }
    std::vector<Opcion> op;
    for (const auto& n : nombres)
        op.push_back({n, normalizar(n)});
    elegirLargo("¿Qué vecino viene en lugar de " + nombre(hueco) + "?",
        "Los vecinos guardados en /switch/NHSE/vecinos de la tarjeta SD. La casa sigue siendo la misma.", op, -1, [hueco, nombres](int k) {
            std::vector<uint8_t> datos;
            if (!partida::leerSd(std::string(CARPETA) + nombres[k], datos) || vecinoDeArchivo(datos).empty())
            {
                avisar("Este archivo no es un vecino de Animal Crossing: New Horizons.", true);
                return;
            }
            const std::vector<uint8_t> v = vecinoDeArchivo(datos);
            if (!deOtraIsla(v, partida::personal(0)))
            {
                ponerDeArchivo(hueco, datos, false);
                return;
            }
            // Como el Mac: ¿que pase a ser de esta isla? Es lo normal.
            const std::string isla = islaDe(v);
            abrir([hueco, datos, isla](Vista& vi) {
                vi.titulo = "Viene de otra isla";
                vi.explicacion = "Este vecino viene de otra isla (" + isla + "). ¿Quieres que pase a ser de la tuya, para que te "
                                 "recuerde a ti y a tu isla? Es lo normal.";
                vi.b.push_back({{140, 300, 480, 100}, "Sí, que sea de mi isla", [hueco, datos] {
                    volver();
                    ponerDeArchivo(hueco, datos, true);
                }});
                vi.b.push_back({{660, 300, 480, 100}, "No, dejarlo como está", [hueco, datos] {
                    volver();
                    ponerDeArchivo(hueco, datos, false);
                }});
                vi.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Cancelar", [] { volver(); }});
            });
        });
}

// ------------------------------------------------------------------------------------------- la ficha
void ficha(Vista& v, int hueco)
{
    auto& m = partida::principal();
    if (vecinoEn(m, hueco).empty())
    {
        volver();
        return;
    }
    const std::string n = nombre(hueco);
    const std::string interno = vecinoEn(m, hueco);
    const int p = personalidad(m, hueco);
    const std::string fr = frase(m, hueco), original = fraseOriginal(interno);
    v.titulo = n;
    v.explicacion = seMuda(m, hueco) ? n + " se va a mudar: hará las maletas y se irá de la isla." : "Elige qué cambiar de " + n + ".";

    std::vector<Boton> b;
    auto boton = [&](const std::string& t, const std::string& d, std::function<void()> f, bool activo = true, bool marcado = false) {
        Boton x{{}, t, std::move(f), activo, marcado};
        x.detalle = d;
        b.push_back(x);
    };
    boton("Cambiar por otro", "Otro vecino en su casa", [hueco] { abrir(elegirVecino(hueco)); }, !datosVecinos().empty());
    boton("Personalidad", p < 8 ? PERSONALIDADES[p] : "?", [hueco, p] {
        elegir("Personalidad de " + nombre(hueco), "Cambia cómo habla y cómo se porta.", std::vector<std::string>(PERSONALIDADES, PERSONALIDADES + 8), p, [hueco](int k) {
            ponerPersonalidad(partida::principal(), hueco, k);
            cambio(std::string("Ahora es ") + PERSONALIDADES[k]);
        });
    });
    boton("Muletilla", fr.empty() ? "La suya: «" + original + "»" : "«" + fr + "»", [hueco, fr] {
        std::string t = fr;
        if (pedirTexto("Muletilla de " + nombre(hueco), "Lo que dice al final de las frases", t, LARGO_FRASE))
        {
            ponerFrase(partida::principal(), hueco, t);
            cambio("Ahora dice «" + t + "»");
        }
    });
    boton("Muletilla original", "«" + original + "»", [hueco, original] {
        ponerFrase(partida::principal(), hueco, original);
        cambio("Vuelve a decir «" + original + "»");
    }, !original.empty() && fr != original);
    boton("Amistad", resumenAmistad(hueco, partida::yo), [hueco] { amistad(hueco); });
    boton("Paredes y suelo", resumenCuarto(hueco), [hueco] { cuarto(hueco); });
    boton("Ropa", std::to_string(usados(hueco, 0)) + " de " + std::to_string(NUM_ROPA) + " prendas guardadas", [hueco] { abrir(objetos::todos(guardado(hueco, 0), 8)); });
    boton("Muebles", std::to_string(usados(hueco, 1)) + " de " + std::to_string(NUM_MUEBLES) + " muebles guardados", [hueco] { abrir(objetos::todos(guardado(hueco, 1), 8)); });
    boton("Casa", resumenCasa(hueco), [hueco] { casa(hueco); }, casaDe(m, hueco) >= 0);
    const bool muda = seMuda(m, hueco);
    boton("Se va a mudar", muda ? "Sí: hará las maletas" : "No", [hueco, muda] {
        ponerMudanza(partida::principal(), hueco, !muda);
        cambio(!muda ? nombre(hueco) + " se va a mudar" : nombre(hueco) + " se queda");
    }, true, muda);
    boton("Bricolaje", resumenBricolaje(hueco), [hueco] { abrir([hueco](Vista& vi) { pantallaBricolaje(vi, hueco); }); });
    boton("Recuerdos", resumenRecuerdos(hueco), [hueco] { recuerdos(hueco); });
    boton("Marcas", resumenMarcas(hueco), [hueco] { marcas(hueco); });
    boton("Su diseño", resumenDiseno(hueco), [hueco] { diseno(hueco); });
    boton("Exportar a la SD", "Para llevarlo a otra isla", [hueco] { exportar(hueco); });
    for (int i = 0; i < (int)b.size(); i++)
    {
        b[i].r = {290 + (i % 3) * 315, 140 + (i / 3) * 88, 300, 80};
        v.b.push_back(b[i]);
    }
    // A la izquierda, su cara, su nombre y su personalidad.
    SDL_Texture* cara = retratos::de(interno);
    const std::string especie = p < 8 ? PERSONALIDADES[p] : "";
    v.dibujo = [cara, n, especie, muda] {
        const SDL_Rect tarjeta = {50, 140, 220, 432};
        redondo({tarjeta.x, tarjeta.y + 4, tarjeta.w, tarjeta.h}, 14, CREMA_OSCURA);
        redondo(tarjeta, 14, BLANCO);
        if (cara)
        {
            const SDL_Rect r = {60, 160, 200, 200};
            SDL_RenderCopy(g_r, cara, nullptr, &r);
        }
        texto(encajar(n, 200, g_grande), 160, 400, MARRON, g_grande, Centro);
        texto(especie, 160, 448, MARRON_SUAVE, nullptr, Centro);
        if (muda)
            texto("Se va a mudar", 160, 490, ROJO, g_pequena, Centro);
    };
    v.b.push_back({{50, ABAJO - 76, 380, 64}, "Importar de la SD…", [hueco] { importar(hueco); }});
    v.b.push_back({{850, ABAJO - 76, 380, 64}, "Volver", [] { volver(); }});
    v.paginar = [hueco](int d) {
        // L y R: el vecino anterior o el siguiente.
        for (int k = 1; k < NUM_VECINOS; k++)
        {
            const int otro = (hueco + d * k + NUM_VECINOS * 10) % NUM_VECINOS;
            if (!vecinoEn(partida::principal(), otro).empty())
            {
                reemplazar([otro](Vista& vi) { ficha(vi, otro); }, foco());
                return;
            }
        }
    };
    v.teclas = {{"A", "Elegir"}, {"L R", "Otro vecino"}, {"B", "Volver"}};
}

void lista(Vista& v)
{
    v.titulo = "Tus vecinos";
    v.explicacion = "Elige un vecino para ver y cambiar todo lo suyo.";
    for (int i = 0; i < NUM_VECINOS; i++)
    {
        const bool vacio = vecinoEn(partida::principal(), i).empty();
        // Como NHSE para Mac: tarjetas con la cara grande, 5 por fila.
        Boton b{{60 + (i % 5) * 236, 136 + (i / 5) * 222, 220, 206}, nombre(i), [i] { abrir([i](Vista& vi) { ficha(vi, i); }); }, !vacio};
        b.vertical = true;
        if (!vacio)
        {
            b.imagen = retratos::deHueco(i);
            const int p = personalidad(partida::principal(), i);
            b.detalle = std::string(p < 8 ? PERSONALIDADES[p] : "?") + (seMuda(partida::principal(), i) ? " · Se va a mudar" : "");
        }
        v.b.push_back(b);
    }
    v.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Volver", [] { volver(); }});
}

int cuantos()
{
    int n = 0;
    for (int i = 0; i < NUM_VECINOS; i++)
        n += !vecinoEn(partida::principal(), i).empty();
    return n;
}

Registro r1({SeccionVecinos, 10, [] { return std::string("Tus vecinos"); },
    [] { return std::to_string(cuantos()) + " viven en la isla"; },
    [] { abrir(lista); }});

Registro r2({SeccionVecinos, 20, [] { return std::string("Mejores amigos de todos"); },
    [] { return std::string("Amistad al máximo con todos"); },
    [] {
        const int n = mejoresAmigos(partida::principal());
        if (n > 0)
            partida::cambiaPrincipal();
        avisar(n > 0 ? "Amistad al máximo con todos (" + std::to_string(n) + "). Falta guardar." : "Ya eran mejores amigos de todos.");
    }});

Registro r3({SeccionVecinos, 30, [] { return std::string("Amistad"); },
    [] { return "Cada vecino con " + partida::jugador().nombre; },
    [] { amistadDeJugador(partida::yo); }});

Registro r4({SeccionVecinos, 40, [] { return std::string("Que se muden todos"); },
    [] {
        int n = 0;
        for (int i = 0; i < NUM_VECINOS; i++)
            n += !vecinoEn(partida::principal(), i).empty() && seMuda(partida::principal(), i);
        return n ? std::to_string(n) + " se van a mudar" : std::string("Hacen las maletas y se van");
    },
    [] {
        confirmar("Que se muden todos", "Todos tus vecinos harán las maletas y se irán de la isla. ¿Seguro?", "Sí, que se muden", [] {
            const int n = mudarTodos(partida::principal());
            partida::cambiaPrincipal();
            avisar("Hecho: " + std::to_string(n) + " vecinos se van a mudar. Guarda para que pase en el juego.");
        });
    }});

Registro r5({SeccionVecinos, 50, [] { return std::string("Guardar todos en la SD"); },
    [] { return std::string("Copia de cada uno en la tarjeta"); },
    [] {
        int n = 0, mal = 0;
        for (int i = 0; i < NUM_VECINOS; i++)
            if (!vecinoEn(partida::principal(), i).empty())
                (partida::escribirSd(std::string(CARPETA) + archivo(nombre(i), "nhv2"), exportarVecino(partida::principal(), i)) ? n : mal)++;
        avisar(mal ? "No he podido guardar " + std::to_string(mal) + " vecinos en la tarjeta SD." : "Se han guardado " + std::to_string(n) + " vecinos en /switch/NHSE/vecinos.", mal > 0);
    }});

} // namespace
