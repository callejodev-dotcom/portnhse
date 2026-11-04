// La casa por fuera (ver casas_fuera.hpp).
#include "casas_fuera.hpp"
#include "cache_texturas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
#include <zlib.h>
#include "../core/acnh.hpp"
#include "../core/mapa_casas.hpp"
#include "ui.hpp"

namespace casas_fuera {

namespace {

// ------------------------------------------------------------------------------------------------ fachadas.bin
struct Entrada {
    std::string nombre;
    int w = 0, h = 0;
    uint8_t marcas = 0; // 1 tejado, 2 puerta, 4 puerta exacta
    SDL_Rect hueco = {0, 0, 0, 0};
    uint32_t pos = 0, tam = 0, posTejado = 0, tamTejado = 0;
};

struct Paquete {
    FILE* f = nullptr;
    std::vector<Entrada> indice;

    Paquete()
    {
        f = fopen("sdmc:/switch/NHSE/fachadas.bin", "rb");
        if (!f)
            return;
        uint8_t cab[16];
        if (fread(cab, 1, 16, f) != 16 || memcmp(cab, "FACH", 4) != 0)
        {
            cerrar();
            return;
        }
        const uint32_t n = cab[8] | (cab[9] << 8) | (cab[10] << 16) | ((uint32_t)cab[11] << 24);
        std::vector<uint8_t> d(70 * (size_t)n);
        if (fread(d.data(), 1, d.size(), f) != d.size())
        {
            cerrar();
            return;
        }
        auto u16 = [](const uint8_t* p) { return p[0] | (p[1] << 8); };
        auto u32 = [](const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); };
        for (uint32_t i = 0; i < n; i++)
        {
            const uint8_t* p = d.data() + 70 * i;
            Entrada e;
            e.nombre.assign((const char*)p, strnlen((const char*)p, 40));
            e.w = u16(p + 40), e.h = u16(p + 42), e.marcas = p[44];
            e.hueco = {u16(p + 46), u16(p + 48), u16(p + 50), u16(p + 52)};
            e.pos = u32(p + 54), e.tam = u32(p + 58), e.posTejado = u32(p + 62), e.tamTejado = u32(p + 66);
            indice.push_back(e);
        }
    }
    void cerrar()
    {
        if (f)
            fclose(f);
        f = nullptr;
        indice.clear();
    }
    const Entrada* buscar(const std::string& n) const
    {
        auto it = std::lower_bound(indice.begin(), indice.end(), n, [](const Entrada& e, const std::string& k) { return e.nombre < k; });
        return it != indice.end() && it->nombre == n ? &*it : nullptr;
    }
    std::vector<uint8_t> leer(uint32_t pos, uint32_t tam, size_t lleno) const
    {
        std::vector<uint8_t> z(tam), d(lleno);
        uLongf t = lleno;
        if (!f || fseek(f, pos, SEEK_SET) != 0 || fread(z.data(), 1, tam, f) != tam || uncompress(d.data(), &t, z.data(), tam) != Z_OK || t != lleno)
            return {};
        return d;
    }
    std::vector<uint8_t> puntos(const Entrada& e) const { return leer(e.pos, e.tam, (size_t)e.w * e.h * 4); }
    std::vector<uint8_t> tejado(const Entrada& e) const { return e.tamTejado ? leer(e.posTejado, e.tamTejado, (size_t)e.w * e.h) : std::vector<uint8_t>{}; }
};

Paquete& paquete()
{
    static Paquete p;
    return p;
}

// ------------------------------------------------------------------------------------------------ nombres
template <size_t N>
const char* buscarEn(const acnh::Aspecto (&lista)[N], uint16_t v)
{
    for (const auto& a : lista)
        if (a.valor == v)
            return a.nombre;
    return "";
}

std::string interno(Parte p, uint16_t v)
{
    return p == Tejado ? buscarEn(acnh::INTERNO_TEJADO, v) : p == Fachada ? buscarEn(acnh::INTERNO_FACHADA, v) : buscarEn(acnh::INTERNO_PUERTA, v);
}

std::string nombre(Parte p, uint16_t v)
{
    return p == Tejado ? buscarEn(acnh::NOMBRES_TEJADO, v) : p == Fachada ? buscarEn(acnh::NOMBRES_FACHADA, v) : buscarEn(acnh::NOMBRES_PUERTA, v);
}

// El dibujo de un aspecto (ExteriorPictures.Resolve): las puertas rectas usan el de la de arco, y los tejados de las casas
// pequeñas el de la casa terminada.
const Entrada* entrada(const std::string& n)
{
    const Paquete& pq = paquete();
    if (const Entrada* e = pq.buscar(n))
        return e;
    if (n.rfind("HouseDoor", 0) == 0 && !n.empty() && n.back() == 'S')
        if (const Entrada* e = pq.buscar(n.substr(0, n.size() - 1) + "R"))
            return e;
    if (n.rfind("HouseRoofPA0", 0) == 0 && n.size() > 12)
        if (const Entrada* e = pq.buscar("HouseRoofPA04" + n.substr(13)))
            return e;
    return nullptr;
}

SDL_Texture* textura(const std::vector<uint8_t>& px, int w, int h)
{
    const char* antes = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    const std::string guardado = antes ? antes : "";
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Texture* t = SDL_CreateTexture(ui::g_r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, guardado.empty() ? "0" : guardado.c_str());
    if (t)
    {
        SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return t;
}


// ------------------------------------------------------------------------------------------------ la fachada compuesta
struct Frente {
    SDL_Texture* t = nullptr;
    int w = 0, h = 0;
    bool hayHueco = false, exacto = false;
    SDL_Rect hueco = {0, 0, 0, 0};
};

// La fachada con el tejado elegido pintado encima, guardando su luz y su sombra (HouseComposer.PaintRoof).
const Frente* frente(const std::string& pared, const std::string& techo)
{
    static std::map<std::string, Frente> hechos;
    const std::string clave = pared + "|" + techo;
    auto it = hechos.find(clave);
    if (it != hechos.end())
        return it->second.t ? &it->second : nullptr;
    if (hechos.size() > 12)
    {
        for (auto& x : hechos)
            if (x.second.t)
                SDL_DestroyTexture(x.second.t);
        hechos.clear();
    }
    Frente& f = hechos[clave];
    const Entrada* e = entrada(pared);
    if (!e)
        return nullptr;
    std::vector<uint8_t> px = paquete().puntos(*e);
    if (px.empty())
        return nullptr;
    const std::vector<uint8_t> mascara = paquete().tejado(*e);
    const Entrada* et = entrada(techo);
    if (!mascara.empty() && et)
    {
        const std::vector<uint8_t> tp = paquete().puntos(*et);
        if (!tp.empty())
        {
            const int rw = et->w, rh = et->h;
            const int tx = (int)(0.24 * rw), ty = (int)(0.42 * rh), tw = std::max(1, (int)(0.16 * rw)), th = std::max(1, (int)(0.20 * rh));
            double suma = 0;
            int n = 0;
            for (size_t i = 0; i < mascara.size(); i++)
                if (mascara[i])
                    suma += (px[4 * i] + px[4 * i + 1] + px[4 * i + 2]) / 3.0, n++;
            if (n)
            {
                const double media = suma / n;
                for (size_t i = 0; i < mascara.size(); i++)
                {
                    if (!mascara[i])
                        continue;
                    const double luz = (px[4 * i] + px[4 * i + 1] + px[4 * i + 2]) / 3.0 / media;
                    const int x = (int)(i % e->w), y = (int)(i / e->w);
                    const uint8_t* t = tp.data() + 4 * ((size_t)(ty + y % th) * rw + tx + x % tw);
                    for (int k = 0; k < 3; k++)
                        px[4 * i + k] = (uint8_t)std::clamp(t[k] * luz, 0.0, 255.0);
                }
            }
        }
    }
    f.t = textura(px, e->w, e->h);
    f.w = e->w, f.h = e->h;
    f.hayHueco = (e->marcas & 2) != 0;
    f.exacto = (e->marcas & 4) != 0;
    f.hueco = e->hueco;
    return f.t ? &f : nullptr;
}

// La puerta: su dibujo y la parte que es puerta (el resto es aire alrededor).
struct DibujoPuerta {
    SDL_Texture* t = nullptr;
    SDL_Rect parte = {0, 0, 0, 0};
};

const DibujoPuerta* puerta(const std::string& n)
{
    static std::map<std::string, DibujoPuerta> hechas;
    auto it = hechas.find(n);
    if (it != hechas.end())
        return it->second.t ? &it->second : nullptr;
    DibujoPuerta& d = hechas[n];
    const Entrada* e = entrada(n);
    if (!e)
        return nullptr;
    const std::vector<uint8_t> px = paquete().puntos(*e);
    if (px.empty())
        return nullptr;
    int x0 = e->w, y0 = e->h, x1 = -1, y1 = -1;
    for (int y = 0; y < e->h; y++)
        for (int x = 0; x < e->w; x++)
            if (px[4 * ((size_t)y * e->w + x) + 3] >= 40)
                x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
    d.parte = x1 < 0 ? SDL_Rect{0, 0, e->w, e->h} : SDL_Rect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    d.t = textura(px, e->w, e->h);
    return d.t ? &d : nullptr;
}

// ------------------------------------------------------------------------------------------------ dibujar con SDL
SDL_Color rgb(uint32_t c, uint8_t a = 255) { return {(uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c, a}; }
SDL_Color sombra(SDL_Color c, double f)
{
    auto k = [f](int v) { return (uint8_t)std::clamp(v * f, 0.0, 255.0); };
    return {k(c.r), k(c.g), k(c.b), c.a};
}

void caja(double x, double y, double w, double h, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui::g_r, c.r, c.g, c.b, c.a);
    SDL_FRect r = {(float)x, (float)y, (float)w, (float)h};
    SDL_RenderFillRectF(ui::g_r, &r);
}

void poligono(const double* xs, const double* ys, int n, SDL_Color c)
{
    std::vector<SDL_Vertex> v;
    for (int i = 1; i + 1 < n; i++)
        for (int k : {0, i, i + 1})
            v.push_back({{(float)xs[k], (float)ys[k]}, c, {0, 0}});
    SDL_RenderGeometry(ui::g_r, nullptr, v.data(), (int)v.size(), nullptr, 0);
}

void elipse(double cx, double cy, double rx, double ry, SDL_Color c)
{
    constexpr int N = 40;
    double xs[N], ys[N];
    for (int i = 0; i < N; i++)
        xs[i] = cx + rx * std::cos(i * 6.2831853 / N), ys[i] = cy + ry * std::sin(i * 6.2831853 / N);
    poligono(xs, ys, N, c);
}

void linea(double xa, double ya, double xb, double yb, double g, SDL_Color c)
{
    const double dx = xb - xa, dy = yb - ya, l = std::sqrt(dx * dx + dy * dy);
    if (l <= 0)
        return;
    const double nx = -dy / l * g / 2, ny = dx / l * g / 2;
    const double xs[4] = {xa + nx, xb + nx, xb - nx, xa - nx}, ys[4] = {ya + ny, yb + ny, yb - ny, ya - ny};
    poligono(xs, ys, 4, c);
}

void marco(double x, double y, double w, double h, double g, SDL_Color c)
{
    caja(x, y, w, g, c);
    caja(x, y + h - g, w, g, c);
    caja(x, y, g, h, c);
    caja(x + w - g, y, g, h, c);
}

// El material de una fachada por su nombre interno: «HouseWallPB04BrickF» → «Brick» (HousePreview.Material).
std::string material(const std::string& n)
{
    if (n.rfind("HouseWall", 0) != 0)
        return "";
    std::string r = n.substr(9);
    if (r.size() > 4 && r[0] == 'P' && isdigit((unsigned char)r[2]))
        r = r.substr(4);
    else if (r.size() > 2 && r[0] == 'N')
        r = r.substr(2);
    if (r.size() > 1 && isupper((unsigned char)r.back()) && islower((unsigned char)r[r.size() - 2]))
        r.pop_back();
    return r;
}

// Las paredes en su material: ladrillos, troncos, tablas, piedras, entramado o estuco (HousePreview.DrawSiding).
void paredes(double x, double y, double w, double h, SDL_Color c, const std::string& mat, SDL_Color vigas, double k)
{
    caja(x, y, w, h, c);
    const SDL_Color oscuro = sombra(c, 0.72), claro = sombra(c, 1.12);
    if (mat == "Brick")
    {
        const double fh = 9 * k, fw = 20 * k;
        int fila = 0;
        for (double yy = y; yy < y + h; yy += fh, fila++)
        {
            caja(x, yy, w, 1.6 * k, sombra(c, 1.35));
            for (double xx = x + (fila & 1) * fw / 2; xx < x + w; xx += fw)
                caja(xx, yy, 1.6 * k, std::min(fh, y + h - yy), sombra(c, 1.35));
        }
    }
    else if (mat == "Log")
    {
        const double fh = 13 * k;
        for (double yy = y; yy < y + h; yy += fh)
        {
            caja(x, yy + 2 * k, w, std::min(fh * 0.35, y + h - yy - 2 * k), claro);
            if (yy + fh < y + h)
                caja(x, yy + fh - 0.6 * k, w, 1.2 * k, oscuro);
            elipse(x + 4 * k, yy + fh / 2, 5 * k, fh / 2, sombra(c, 1.2));
            elipse(x + w - 4 * k, yy + fh / 2, 5 * k, fh / 2, sombra(c, 1.2));
        }
    }
    else if (mat == "Woodsidng" || mat == "Woodsiding" || mat == "Wood" || mat == "Woodpanel")
    {
        for (double yy = y + 8 * k; yy < y + h; yy += 8 * k)
            caja(x, yy, w, 1.2 * k, oscuro);
    }
    else if (mat == "Stone")
    {
        const double fh = 12 * k, fw = 18 * k;
        int fila = 0;
        for (double yy = y; yy < y + h; yy += fh, fila++)
            for (double xx = x - (fila % 3) * 6 * k; xx < x + w; xx += fw)
            {
                const double x0 = std::max(x, xx + k), x1 = std::min(x + w, xx + fw - k), y1 = std::min(y + h, yy + fh - k);
                if (x1 > x0 && y1 > yy + k)
                {
                    caja(x0, yy + k, x1 - x0, y1 - yy - k, oscuro);
                    caja(x0 + k, yy + 2 * k, std::max(0.0, x1 - x0 - 2 * k), std::max(0.0, y1 - yy - 3 * k), sombra(c, 1.0 + ((int)(xx + yy) % 3) * 0.05));
                }
            }
    }
    else if (mat == "Japanese" || mat == "Oriental")
    {
        const SDL_Color madera = sombra(c, 0.55);
        caja(x, y + h * 0.55, w, h * 0.45, madera);
        for (double xx = x + 7 * k; xx < x + w; xx += 7 * k)
            caja(xx, y + h * 0.55, k, h * 0.45, sombra(c, 0.4));
        caja(x, y, 6 * k, h, madera);
        caja(x + w - 6 * k, y, 6 * k, h, madera);
    }
    else if (mat == "Woodframe" || mat == "Standard")
    {
        const double g = 4 * k;
        linea(x, y + h * 0.5, x + w, y + h * 0.5, g, vigas);
        for (double f : {0.02, 0.36, 0.64, 0.98})
            linea(x + w * f, y, x + w * f, y + h, g, vigas);
        linea(x + w * 0.02, y + h * 0.5, x + w * 0.36, y, g, vigas);
        linea(x + w * 0.98, y + h * 0.5, x + w * 0.64, y, g, vigas);
    }
    else
    {
        for (int i = 0; i < 18; i++)
            elipse(x + (i * 37 % 100) / 100.0 * w, y + (i * 53 % 100) / 100.0 * h, 5 * k, 3 * k, sombra(c, 0.94));
    }
}

// El color que dice un nombre («tejado pizarra rojo», «fachada estuco azul»), o el que se da.
SDL_Color colorDelNombre(const std::string& n, SDL_Color si_no)
{
    static const std::pair<const char*, uint32_t> COLORES[] = {{"azul", 0x5F88C8}, {"roj", 0xC2504A}, {"carmesi", 0xA83040},
        {"verde", 0x6EA05A}, {"amarill", 0xE3C050}, {"rosa", 0xE69AB4}, {"blanc", 0xF2EEE6}, {"negr", 0x4A454C}, {"morad", 0x8E6CC0},
        {"lila", 0xB09AD8}, {"naranja", 0xE0884C}, {"gris", 0x9A9AA0}, {"marron", 0x8B5E3C}, {"beis", 0xDCC8A4}, {"crema", 0xEFE2C2},
        {"ocre", 0xC89A48}, {"celeste", 0x94C8E8}, {"pardo", 0x9A7A58}, {"dorad", 0xD8B448}, {"chic", 0xB8A890}};
    const std::string k = acnh::normalizar(n);
    size_t mejor = std::string::npos;
    SDL_Color c = si_no;
    for (const auto& p : COLORES)
    {
        const size_t i = k.find(p.first);
        if (i != std::string::npos && (mejor == std::string::npos || i > mejor))
            mejor = i, c = rgb(p.second);
    }
    return c;
}

} // namespace

bool hayDibujos() { return paquete().f != nullptr; }

SDL_Color color(Parte p, uint16_t v)
{
    const SDL_Color si_no = p == Tejado ? rgb(0x9A6B4F) : p == Fachada ? rgb(0xE8E0D0) : rgb(0x9A6B4F);
    return colorDelNombre(nombre(p, v), si_no);
}

SDL_Texture* dibujo(Parte p, uint16_t v)
{
    static CacheTexturas<std::string> g(48);
    const std::string n = interno(p, v);
    bool hallada;
    if (SDL_Texture* ya = g.buscar(n, hallada); hallada)
        return ya;
    const Entrada* e = entrada(n);
    SDL_Texture* t = nullptr;
    if (e)
    {
        const std::vector<uint8_t> px = paquete().puntos(*e);
        if (!px.empty())
            t = textura(px, e->w, e->h);
    }
    return g.poner(n, t);
}

void dibujar(const SDL_Rect& r, uint16_t tejado, uint16_t fachada, uint16_t puertaV)
{
    SDL_Renderer* g = ui::g_r;
    SDL_RenderSetClipRect(g, &r);
    SDL_SetRenderDrawBlendMode(g, SDL_BLENDMODE_BLEND);
    const double X = r.x, Y = r.y, W = r.w, H = r.h, k = W / 320.0;
    // El cielo y el jardín.
    {
        const SDL_Color a = rgb(0x9FD6F2), b = rgb(0xD8F0FA);
        SDL_Vertex v[6] = {{{(float)X, (float)Y}, a, {0, 0}}, {{(float)(X + W), (float)Y}, a, {0, 0}}, {{(float)(X + W), (float)(Y + H)}, b, {0, 0}},
            {{(float)X, (float)Y}, a, {0, 0}}, {{(float)(X + W), (float)(Y + H)}, b, {0, 0}}, {{(float)X, (float)(Y + H)}, b, {0, 0}}};
        SDL_RenderGeometry(g, nullptr, v, 6, nullptr, 0);
    }
    const double suelo = Y + H * 0.8;
    caja(X, suelo, W, Y + H - suelo, rgb(0x8CCB6E));
    elipse(X + W * 0.12, suelo + 4 * k, W * 0.12, 10 * k, rgb(0x7ABD5D));
    elipse(X + W * 0.9, suelo + 6 * k, W * 0.14, 11 * k, rgb(0x7ABD5D));

    const std::string pared = interno(Fachada, fachada), techo = interno(Tejado, tejado), npuerta = interno(Puerta, puertaV);
    if (const Frente* f = frente(pared, techo))
    {
        // La fachada hecha con los dibujos del juego, sobre la hierba, con la puerta en su hueco (HousePreview.DrawFront).
        const double s = std::min(W * 0.8 / f->w, (suelo - Y - H * 0.06) / f->h);
        const double dw = f->w * s, dh = f->h * s, dx = X + W / 2 - dw / 2, dy = suelo + 3 * k - dh;
        elipse(dx + dw / 2, suelo + 3 * k, dw * 0.55, 7 * k, {0, 0, 0, 0x30});
        SDL_FRect d = {(float)dx, (float)dy, (float)dw, (float)dh};
        SDL_RenderCopyF(g, f->t, nullptr, &d);
        const DibujoPuerta* p = puerta(npuerta);
        if (f->hayHueco && p)
        {
            const SDL_Rect& h = f->hueco;
            const double arriba = dy + (h.y - (f->exacto ? 0 : h.h * 0.25)) * s;
            const double ax = dx + h.x * s, aw = h.w * s, ah = dy + dh - arriba;
            double pw = p->parte.w * ah / p->parte.h, ph = ah;
            if (pw > aw * 0.9)
                pw = aw * 0.9, ph = p->parte.h * aw * 0.9 / p->parte.w;
            SDL_FRect pd = {(float)(ax + aw / 2 - pw / 2), (float)(dy + dh - ph), (float)pw, (float)ph};
            SDL_RenderCopyF(g, p->t, &p->parte, &pd);
        }
        SDL_SetRenderDrawBlendMode(g, SDL_BLENDMODE_NONE);
        SDL_RenderSetClipRect(g, nullptr);
        return;
    }

    // Sin los dibujos: la casa dibujada (HousePreview), las paredes en su material y en el color que dice su nombre.
    const double bw = W * 0.56, bh = H * 0.38, bx = X + (W - bw) / 2, by = suelo - bh;
    elipse(bx + bw / 2, suelo + 2 * k, bw * 0.6, 7 * k, {0, 0, 0, 0x30});
    {
        const double xs[4] = {bx + bw / 2 - bw * 0.09, bx + bw / 2 + bw * 0.09, bx + bw / 2 + bw * 0.16, bx + bw / 2 - bw * 0.16};
        const double ys[4] = {suelo, suelo, Y + H, Y + H};
        poligono(xs, ys, 4, rgb(0xE6D3A6));
    }
    const std::string mat = material(pared);
    SDL_Color muro = color(Fachada, fachada);
    SDL_Color vigas = sombra(muro, 0.5);
    if ((mat == "Woodframe" || mat == "Standard") && muro.r + muro.g + muro.b < 300)
        vigas = muro, muro = rgb(0xF2EBDD); // entramado: el color es el de las vigas, la pared es clara
    const SDL_Color borde = sombra(muro, 0.45);
    SDL_Rect cuerpo = {(int)bx, (int)by, (int)bw, (int)bh};
    SDL_Rect corte;
    SDL_IntersectRect(&cuerpo, &r, &corte);
    SDL_RenderSetClipRect(g, &corte);
    paredes(bx, by, bw, bh, muro, mat, vigas, k);
    SDL_RenderSetClipRect(g, &r);
    marco(bx, by, bw, bh, 2 * k, borde);
    // La chimenea y el tejado encima.
    const SDL_Color teja = color(Tejado, tejado);
    const double picoX = bx + bw / 2, picoY = Y + H * 0.12;
    const double cx = bx + bw / 2 + bw * 0.22, cy = picoY + (by - picoY) * 0.25;
    caja(cx, cy, bw * 0.09, (by - picoY) * 0.55, sombra(muro, 0.8));
    marco(cx, cy, bw * 0.09, (by - picoY) * 0.55, 2 * k, borde);
    const double tx[3] = {bx - bw * 0.1, picoX, bx + bw * 1.1}, ty[3] = {by + 6 * k, picoY, by + 6 * k};
    const double ox[3] = {tx[0] - 2.5 * k, tx[1], tx[2] + 2.5 * k}, oy[3] = {ty[0] + 1.5 * k, ty[1] - 2.5 * k, ty[2] + 1.5 * k};
    poligono(ox, oy, 3, {0x30, 0x20, 0x18, 0xC0});
    poligono(tx, ty, 3, teja);
    // Unas tejas: rayas a lo largo del tejado.
    for (double f = 0.25; f < 1; f += 0.25)
        linea(picoX + (tx[0] - picoX) * f, picoY + (ty[0] - picoY) * f, picoX + (tx[2] - picoX) * f, picoY + (ty[2] - picoY) * f, 1.2 * k, sombra(teja, 0.8));
    caja(bx, by + 6 * k, bw, 6 * k, {0, 0, 0, 0x38});
    // Las ventanas.
    const double lado = bh * 0.32;
    for (double wx : {bx + bw * 0.2, bx + bw - bw * 0.2})
    {
        const double x0 = wx - lado / 2, y0 = by + bh * 0.24;
        caja(x0 - 3 * k, y0 - 3 * k, lado + 6 * k, lado + 6 * k, rgb(0xF7F3E8));
        marco(x0 - 3 * k, y0 - 3 * k, lado + 6 * k, lado + 6 * k, 1.5 * k, borde);
        const SDL_Color a = rgb(0xB9E3F7), b = rgb(0x7FBFE0);
        SDL_Vertex v[6] = {{{(float)x0, (float)y0}, a, {0, 0}}, {{(float)(x0 + lado), (float)y0}, a, {0, 0}}, {{(float)(x0 + lado), (float)(y0 + lado)}, b, {0, 0}},
            {{(float)x0, (float)y0}, a, {0, 0}}, {{(float)(x0 + lado), (float)(y0 + lado)}, b, {0, 0}}, {{(float)x0, (float)(y0 + lado)}, b, {0, 0}}};
        SDL_RenderGeometry(g, nullptr, v, 6, nullptr, 0);
        caja(x0 + lado / 2 - k, y0, 2 * k, lado, rgb(0xF7F3E8));
        caja(x0, y0 + lado / 2 - k, lado, 2 * k, rgb(0xF7F3E8));
    }
    // La puerta: su dibujo si está, o una de su color.
    if (const DibujoPuerta* p = puerta(npuerta))
    {
        const double lado2 = bh * 0.78;
        SDL_FRect d = {(float)(bx + bw / 2 - lado2 / 2), (float)(suelo - lado2 + lado2 * 0.02), (float)lado2, (float)lado2};
        SDL_RenderCopyF(g, p->t, nullptr, &d);
    }
    else
    {
        const SDL_Color c = color(Puerta, puertaV);
        const double pw = bw * 0.18, ph = bh * 0.55, px = bx + bw / 2 - pw / 2, py = suelo - ph;
        caja(px, py, pw, ph, c);
        marco(px, py, pw, ph, 2 * k, borde);
        elipse(px + pw * 0.78, py + ph * 0.55, 2.5 * k, 2.5 * k, rgb(0xE8C860));
    }
    SDL_SetRenderDrawBlendMode(g, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(g, nullptr);
}

} // namespace casas_fuera
