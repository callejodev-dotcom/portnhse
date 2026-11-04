// Piezas de la sección «Avanzado» que usan sus dos archivos (avanzado.cpp y avanzado_bruto.cpp).
#pragma once
#include <functional>
#include <string>

namespace avanzado {

// Abre una pantalla con «mensaje» y, cuando ya se ha visto, hace el trabajo (lento) y vuelve. El trabajo puede abrir
// otra pantalla o avisar.
void conEspera(const std::string& titulo, const std::string& mensaje, std::function<void()> trabajo);
std::string fecha();             // «2026-11-04 18.30.00», como las copias de seguridad
std::string carpetaJugador(int j); // «Villager0»

} // namespace avanzado
