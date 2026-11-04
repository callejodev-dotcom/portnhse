// Sección «Vecinos»: cambiar quién vive en la isla y ser mejores amigos de todos.
#include <cctype>
#include <cstring>
#include <memory>
#include "../core/vecinos.hpp"
#include "partida.hpp"
#include "ui.hpp"

using namespace acnh;
using namespace ui;

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

std::string nombreVecino(const std::string& interno)
{
    for (const auto& v : LISTA_VECINOS)
        if (interno == v.interno)
            return v.nombre;
    return interno.empty() ? "(libre)" : interno;
}

char inicial(const char* nombre)
{
    const std::string n = normalizar(std::string(nombre, strnlen(nombre, 4)));
    return (char)toupper((unsigned char)n[0]);
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
        v.titulo = "¿Quién viene en lugar de " + nombreVecino(vecinoEn(partida::principal(), hueco)) + "?";
        v.explicacion = "Elige la letra abajo (o con L y R).";
        for (int k = 0; k < 18 && e->pagina * 18 + k < (int)deLetra.size(); k++)
        {
            const auto& vv = LISTA_VECINOS[deLetra[e->pagina * 18 + k]];
            bool yaEsta = false;
            for (int j = 0; j < NUM_VECINOS; j++)
                yaEsta |= vecinoEn(partida::principal(), j) == vv.interno;
            v.b.push_back({{60 + (k / 6) * 390, 140 + (k % 6) * 62, 370, 54}, vv.nombre, [hueco, interno = std::string(vv.interno), nombre = std::string(vv.nombre)] {
                const std::string antes = nombreVecino(vecinoEn(partida::principal(), hueco));
                const std::string mal = cambiarVecino(partida::principal(), hueco, interno, datosVecinos());
                if (mal.empty())
                    partida::cambiaPrincipal();
                volver();
                avisar(mal.empty() ? antes + " se va y llega " + nombre + ". Falta guardar." : mal, !mal.empty());
            }, !yaEsta});
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

void vecinos(Vista& v)
{
    v.titulo = "Vecinos";
    v.explicacion = "Elige el vecino que se va: el nuevo vivirá en su casa.";
    for (int i = 0; i < NUM_VECINOS; i++)
    {
        const std::string vv = vecinoEn(partida::principal(), i);
        v.b.push_back({{60 + (i / 5) * 590, 140 + (i % 5) * 76, 570, 66}, nombreVecino(vv), [i] { abrir(elegirVecino(i)); }, !vv.empty() && !datosVecinos().empty()});
    }
    v.b.push_back({{W / 2 - 200, ABAJO - 80, 400, 66}, "Volver", [] { volver(); }});
}

Registro r1({SeccionVecinos, 10, [] { return std::string("Cambiar vecinos"); },
    [] {
        int n = 0;
        for (int i = 0; i < NUM_VECINOS; i++)
            n += !vecinoEn(partida::principal(), i).empty();
        return std::to_string(n) + " viven en la isla";
    },
    [] { abrir(vecinos); }});

Registro r2({SeccionVecinos, 20, [] { return std::string("Mejores amigos de todos"); },
    [] { return std::string("Amistad al máximo con todos"); },
    [] {
        const int n = mejoresAmigos(partida::principal());
        if (n > 0)
            partida::cambiaPrincipal();
        avisar(n > 0 ? "Amistad al máximo con todos (" + std::to_string(n) + "). Falta guardar." : "Ya eran mejores amigos de todos.");
    }});

} // namespace
