// Lo que un vecino recuerda de cada jugador, como VillagerMemoryEditor de NHSE para Mac: el nombre y la isla del jugador,
// cómo le llama, la amistad, los saludos que le ha enseñado y su fecha, las visitas, todas las marcas del recuerdo, y
// pasarlo a la tarjeta SD (.nhvpm, como el Mac) o traerlo de ella.
#include <algorithm>
#include <memory>
#include "../core/vecino.hpp"
#include "../core/vecino_marcas.hpp"
#include "partida.hpp"
#include "vecinos_ui.hpp"

using namespace acnh;
using namespace ui;

namespace vecinos {

namespace {
void cambio(const std::string& que)
{
    partida::cambiaPrincipal();
    cambiado(que);
}

constexpr int VISITAS_SUYAS = 8, VISITAS_TUYAS = 9; // marcas del recuerdo: veces que fue a casa del jugador y al revés

std::string dosCifras(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

std::string textoFecha(const FechaVecino& f)
{
    if (!fechaValida(f))
        return "Sin fecha";
    static const char* const MESES[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre",
        "octubre", "noviembre", "diciembre"};
    return std::to_string(f.dia) + " de " + MESES[f.mes - 1] + " de " + std::to_string(f.anio);
}

std::string nombreSaludo(int i) { return "Saludo " + std::to_string(i + 1); }

int saludosConTexto(int hueco, int r)
{
    int n = 0;
    for (int i = 0; i < NUM_SALUDOS; i++)
        n += !saludo(partida::principal(), hueco, r, i).empty();
    return n;
}

// «Carli (Barro)»
std::string quien(int hueco, int r)
{
    const Recuerdo x = recuerdo(partida::principal(), hueco, r);
    return x.jugador + " (" + x.isla + ")";
}

// ------------------------------------------------------------------------------------------- saludos
void saludos(int hueco, int r)
{
    abrir([hueco, r](Vista& v) {
        v.titulo = "Saludos de " + nombre(hueco) + " a " + recuerdo(partida::principal(), hueco, r).jugador;
        v.explicacion = "Lo que le dice al saludar (en el juego se lo enseñas hablando con el vecino). Se ven hasta 22 letras.";
        for (int i = 0; i < NUM_SALUDOS; i++)
        {
            const std::string s = saludo(partida::principal(), hueco, r, i);
            Boton b{{60 + (i / 6) * 590, 140 + (i % 6) * 72, 570, 64}, nombreSaludo(i) + ": " + (s.empty() ? "—" : "«" + s + "»"), [hueco, r, i, s] {
                std::string t = s;
                if (!pedirTexto(nombreSaludo(i) + " de " + nombre(hueco), "Lo que dice al saludar (vacío: nada)", t, 22))
                    return;
                ponerSaludo(partida::principal(), hueco, r, i, t);
                cambio(t.empty() ? "Sin saludo" : "Ahora saluda «" + t + "»");
            }};
            v.b.push_back(b);
        }
        v.b.push_back({{850, ABAJO - 76, 370, 64}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------- fecha
void fecha(int hueco, int r)
{
    abrir([hueco, r](Vista& v) {
        const FechaVecino f = fechaSaludo(partida::principal(), hueco, r);
        const bool hay = fechaValida(f);
        v.titulo = "Fecha del saludo";
        v.explicacion = "El día en que " + recuerdo(partida::principal(), hueco, r).jugador + " le enseñó el saludo a " + nombre(hueco) +
            ". Ahora: " + (hay ? textoFecha(f) : std::string("sin fecha")) + ".";
        // Al cambiar una parte sin fecha, se empieza por el día en que salió el juego.
        auto poner = [hueco, r](std::function<void(FechaVecino&)> cambiar) {
            FechaVecino n = fechaSaludo(partida::principal(), hueco, r);
            if (!fechaValida(n))
                n.anio = 2020, n.mes = 3, n.dia = 20;
            cambiar(n);
            while (n.dia > 1 && !fechaValida(n))
                n.dia--;
            ponerFechaSaludo(partida::principal(), hueco, r, n);
            cambio("Fecha: " + textoFecha(n));
        };
        Boton anio{{847, 150, 373, 110}, "Año", [poner, f, hay] {
            uint32_t a = hay ? f.anio : 2020;
            if (pedirNumero("Año (de 2000 a 2099)", a, 2099, 2000))
                poner([a](FechaVecino& n) { n.anio = (uint16_t)a; });
        }};
        anio.detalle = hay ? std::to_string(f.anio) : "—";
        Boton mes{{453, 150, 374, 110}, "Mes", [poner, f, hay] {
            uint32_t m = hay ? f.mes : 3;
            if (pedirNumero("Mes (de 1 a 12)", m, 12, 1))
                poner([m](FechaVecino& n) { n.mes = (uint8_t)m; });
        }};
        mes.detalle = hay ? dosCifras(f.mes) : "—";
        Boton dia{{60, 150, 373, 110}, "Día", [poner, f, hay] {
            uint32_t d = hay ? f.dia : 20;
            if (pedirNumero("Día (de 1 a 31)", d, 31, 1))
                poner([d](FechaVecino& n) { n.dia = (uint8_t)d; });
        }};
        dia.detalle = hay ? dosCifras(f.dia) : "—";
        v.b.push_back(dia);
        v.b.push_back(mes);
        v.b.push_back(anio);
        v.b.push_back({{60, 280, 570, 80}, "Sin fecha", [hueco, r] {
            ponerFechaSaludo(partida::principal(), hueco, r, FechaVecino{});
            cambio("Sin fecha");
        }, !f.vacia()});
        v.b.push_back({{850, ABAJO - 76, 370, 64}, "Volver", [] { volver(); }});
    });
}

// ------------------------------------------------------------------------------------------- todas las marcas
void marcasRecuerdo(int hueco, int r)
{
    ListaMarcas l;
    l.titulo = nombre(hueco) + " y " + recuerdo(partida::principal(), hueco, r).jugador;
    l.total = NUM_MARCAS_RECUERDO;
    l.tope = 255;
    for (const auto& m : MARCAS_RECUERDO)
    {
        l.indices.push_back(m.indice);
        l.textos.push_back(m.texto);
        l.internos.push_back(m.interno);
        l.maximos.push_back(m.maximo);
    }
    l.leer = [hueco, r](int i) { return (int)marcaRecuerdo(partida::principal(), hueco, r, i); };
    l.poner = [hueco, r](int i, int v) {
        ponerMarcaRecuerdo(partida::principal(), hueco, r, i, (uint8_t)v);
        partida::cambiaPrincipal();
    };
    listaMarcas(std::move(l));
}

// ------------------------------------------------------------------------------------------- archivos
void exportarRecuerdoSd(int hueco, int r)
{
    const std::string n = archivo(nombre(hueco) + " - " + recuerdo(partida::principal(), hueco, r).jugador, "nhvpm");
    if (partida::escribirSd(std::string(CARPETA) + n, exportarRecuerdo(partida::principal(), hueco, r)))
        avisar("Guardado en la tarjeta SD: /switch/NHSE/vecinos/" + n);
    else
        avisar("No he podido guardarlo en la tarjeta SD.", true);
}

void ponerRecuerdo(int hueco, int r, const std::vector<uint8_t>& datos, bool hacerSuyo)
{
    const std::string mal = importarRecuerdo(partida::principal(), hueco, r, datos, hacerSuyo);
    if (!mal.empty())
        avisar(mal, true);
    else
        cambio("Recuerdo cambiado");
}

void importarRecuerdoSd(int hueco, int r)
{
    const auto nombres = partida::listarSd(CARPETA, ".nhvpm");
    if (nombres.empty())
    {
        avisar("No hay recuerdos (.nhvpm) en la carpeta /switch/NHSE/vecinos de la tarjeta SD.", true);
        return;
    }
    std::vector<Opcion> op;
    for (const auto& n : nombres)
        op.push_back({n, normalizar(n)});
    elegirLargo("¿Qué recuerdo pongo?", "En lugar de lo que " + nombre(hueco) + " recuerda de " + quien(hueco, r) + ".", op, -1,
        [hueco, r, nombres](int k) {
            std::vector<uint8_t> datos;
            if (!partida::leerSd(std::string(CARPETA) + nombres[k], datos) || datos.size() != TAM_RECUERDO)
            {
                avisar("Ese archivo no es un recuerdo de un vecino.", true);
                return;
            }
            if (!recuerdoDeOtro(partida::principal(), hueco, r, datos))
            {
                ponerRecuerdo(hueco, r, datos, false);
                return;
            }
            // Como el Mac: ¿que pase a ser del jugador de ahora?
            const std::string otro = leerTextoVecino(datos.data() + 0x20, 10) + " (" + leerTextoVecino(datos.data() + 4, 10) + ")";
            abrir([hueco, r, datos, otro](Vista& vi) {
                vi.titulo = "Es de otro jugador";
                vi.explicacion = "Este recuerdo es de " + otro + ". ¿Quieres que pase a ser de " + quien(hueco, r) + "? Es lo normal.";
                vi.b.push_back({{140, 300, 480, 100}, "Sí, que sea de " + recuerdo(partida::principal(), hueco, r).jugador, [hueco, r, datos] {
                    volver();
                    ponerRecuerdo(hueco, r, datos, true);
                }});
                vi.b.push_back({{660, 300, 480, 100}, "No, dejarlo como está", [hueco, r, datos] {
                    volver();
                    ponerRecuerdo(hueco, r, datos, false);
                }});
                vi.b.push_back({{W / 2 - 200, ABAJO - 76, 400, 64}, "Cancelar", [] { volver(); }});
            });
        });
}

// ------------------------------------------------------------------------------------------- un recuerdo
void unRecuerdo(int hueco, int r)
{
    abrir([hueco, r](Vista& v) {
        const auto& m = partida::principal();
        const Recuerdo x = recuerdo(m, hueco, r);
        v.titulo = nombre(hueco) + " y " + x.jugador;
        v.explicacion = "Lo que " + nombre(hueco) + " recuerda de " + x.jugador + ", de " + x.isla + ".";
        std::vector<Boton> b;
        auto boton = [&](const std::string& t, const std::string& d, std::function<void()> f, bool activo = true) {
            Boton y{{}, t, std::move(f), activo};
            y.detalle = d;
            b.push_back(y);
        };
        boton("Nombre del jugador", x.jugador, [hueco, r, x] {
            std::string t = x.jugador;
            if (pedirTexto("Nombre del jugador", "Ojo: si no es igual que el del jugador, no le reconocerá", t, 10))
            {
                ponerJugadorRecuerdo(partida::principal(), hueco, r, t);
                cambio("Ahora recuerda a «" + t + "»");
            }
        });
        boton("Isla", x.isla, [hueco, r, x] {
            std::string t = x.isla;
            if (pedirTexto("Isla del jugador", "Ojo: si no es igual que la del jugador, no le reconocerá", t, 10))
            {
                ponerIslaRecuerdo(partida::principal(), hueco, r, t);
                cambio("Ahora es de «" + t + "»");
            }
        });
        boton("Cómo le llama", x.apodo.empty() ? "Por su nombre" : "«" + x.apodo + "»", [hueco, r, x] {
            std::string t = x.apodo;
            if (pedirTexto("Cómo llama a " + x.jugador, "Si lo dejas vacío, usará su nombre.", t, 10))
            {
                ponerApodo(partida::principal(), hueco, r, t);
                cambio(t.empty() ? "Le llamará por su nombre" : "Le llamará «" + t + "»");
            }
        });
        boton("Amistad", std::string(NIVELES[nivelAmistad(x.amistad)]) + " · " + std::to_string(x.amistad) + " puntos", [hueco, r] { corazones(hueco, r); });
        const std::string s = saludo(m, hueco, r, 0);
        boton("Saludos", s.empty() ? "No tiene" : "«" + s + "»" + (saludosConTexto(hueco, r) > 1 ? " y más" : ""), [hueco, r] { saludos(hueco, r); });
        boton("Fecha del saludo", textoFecha(fechaSaludo(m, hueco, r)), [hueco, r] { fecha(hueco, r); });
        auto visitas = [hueco, r](int marca, const std::string& que) {
            uint32_t n = marcaRecuerdo(partida::principal(), hueco, r, marca);
            if (pedirNumero(que + " (lo normal: de 0 a 7)", n, 255))
            {
                ponerMarcaRecuerdo(partida::principal(), hueco, r, marca, (uint8_t)n);
                cambio(que + ": " + std::to_string(n));
            }
        };
        boton("Visitas a su casa", "Le ha ido a ver " + std::to_string(marcaRecuerdo(m, hueco, r, VISITAS_TUYAS)) + " veces",
            [visitas, x] { visitas(VISITAS_TUYAS, "Veces que " + x.jugador + " ha ido a su casa"); });
        boton("Visitas a casa de " + x.jugador, "Ha ido " + std::to_string(marcaRecuerdo(m, hueco, r, VISITAS_SUYAS)) + " veces",
            [visitas, x, hueco] { visitas(VISITAS_SUYAS, "Veces que " + nombre(hueco) + " ha ido a casa de " + x.jugador); });
        boton("Todas las marcas", "Lo que han hecho juntos, una a una", [hueco, r] { marcasRecuerdo(hueco, r); });
        boton("Exportar a la SD", "Para llevarlo a otro vecino", [hueco, r] { exportarRecuerdoSd(hueco, r); });
        boton("Importar de la SD", "En lugar de este recuerdo", [hueco, r] { importarRecuerdoSd(hueco, r); });
        for (int i = 0; i < (int)b.size(); i++)
        {
            b[i].r = {50 + (i % 3) * 400, 140 + (i / 3) * 108, 380, 96};
            v.b.push_back(b[i]);
        }
        v.b.push_back({{850, ABAJO - 76, 380, 64}, "Volver", [] { volver(); }});
    });
}
} // namespace

// ------------------------------------------------------------------------------------------- los recuerdos
std::string resumenRecuerdos(int hueco)
{
    int n = 0;
    for (int r = 0; r < NUM_RECUERDOS; r++)
        n += recuerdoUsado(partida::principal(), hueco, r);
    return n == 0 ? "No conoce a nadie" : n == 1 ? "De 1 jugador" : "De " + std::to_string(n) + " jugadores";
}

void recuerdos(int hueco)
{
    abrir([hueco](Vista& v) {
        v.titulo = "Recuerdos de " + nombre(hueco);
        v.explicacion = "Lo que " + nombre(hueco) + " recuerda de cada jugador que conoce: saludos, apodo, visitas, fechas…";
        for (int r = 0; r < NUM_RECUERDOS; r++)
        {
            const bool usado = recuerdoUsado(partida::principal(), hueco, r);
            const Recuerdo x = recuerdo(partida::principal(), hueco, r);
            Boton b{{60 + (r % 2) * 590, 150 + (r / 2) * 100, 570, 88}, usado ? x.jugador + " (" + x.isla + ")" : "Sin recuerdo",
                [hueco, r] { unRecuerdo(hueco, r); }, usado};
            if (usado)
                b.detalle = std::string(NIVELES[nivelAmistad(x.amistad)]) + (x.apodo.empty() ? "" : " · Le llama «" + x.apodo + "»");
            v.b.push_back(b);
        }
        v.b.push_back({{850, ABAJO - 76, 370, 64}, "Volver", [] { volver(); }});
    });
}

} // namespace vecinos
