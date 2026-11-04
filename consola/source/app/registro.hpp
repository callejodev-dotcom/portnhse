// El registro de NHSE para Switch: lo que va pasando, en /switch/NHSE/registro.txt (el de la vez anterior queda en
// registro-anterior.txt). Si algo falla en la consola, ese archivo dice dónde. También apunta los cuelgues (con su
// dirección, para buscarla en NHSE.elf).
#pragma once
#include <string>

namespace registro {
void empezar(const char* version);
// Una línea (con la hora). Como printf.
void apuntar(const char* formato, ...) __attribute__((format(printf, 1, 2)));
void terminar();
} // namespace registro
