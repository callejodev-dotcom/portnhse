// Texturas guardadas (dibujos de objetos, caras...) que se quitan cuando hay demasiadas, pero nunca las que se han
// pedido en este mismo fotograma: un botón puede estar a punto de dibujarlas.
#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include <map>
#include <vector>
#include "ui.hpp"

template <typename Clave> class CacheTexturas
{
public:
    explicit CacheTexturas(size_t maximo) : maximo_(maximo) {}

    // nullptr también se guarda (no hay dibujo): «hallada» dice si estaba.
    SDL_Texture* buscar(const Clave& k, bool& hallada)
    {
        auto it = m_.find(k);
        hallada = it != m_.end();
        if (!hallada)
            return nullptr;
        it->second.usada = ui::fotograma();
        return it->second.t;
    }

    SDL_Texture* poner(const Clave& k, SDL_Texture* t)
    {
        if (m_.size() >= maximo_)
            hacerSitio();
        m_[k] = {t, ui::fotograma()};
        return t;
    }

private:
    struct Entrada {
        SDL_Texture* t;
        uint64_t usada;
    };
    std::map<Clave, Entrada> m_;
    size_t maximo_;

    void hacerSitio()
    {
        // The ones used longest ago go first, until a quarter is free (never the ones of this frame).
        std::vector<std::pair<uint64_t, Clave>> viejas;
        const uint64_t ahora = ui::fotograma();
        for (const auto& e : m_)
            if (e.second.usada < ahora)
                viejas.push_back({e.second.usada, e.first});
        std::sort(viejas.begin(), viejas.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        size_t quitar = std::min(viejas.size(), m_.size() - maximo_ * 3 / 4);
        for (size_t i = 0; i < quitar; i++)
        {
            auto it = m_.find(viejas[i].second);
            if (it->second.t)
                SDL_DestroyTexture(it->second.t);
            m_.erase(it);
        }
    }
};
