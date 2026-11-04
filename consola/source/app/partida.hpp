// La partida abierta: main.dat y los personal.dat de los jugadores, descifrados; qué ha cambiado; abrir y guardar.
#pragma once
#include <switch.h>
#include <cstdint>
#include <string>
#include <vector>
#include "../core/acnh.hpp"

namespace partida {

struct Jugador {
    std::string carpeta; // «/Villager0»
    std::string nombre;
    acnh::Archivo a;
    bool cambiado = false;
};

extern std::vector<Jugador> jugadores;
extern int yo; // the player being edited
extern std::string isla;
extern std::string copia; // the backup folder on the SD («sdmc:/switch/NHSE/copias/...»)

std::vector<uint8_t>& principal();       // main.dat
std::vector<uint8_t>& personal(int j = -1); // personal.dat (of «yo» by default)
Jugador& jugador(int j = -1);
void cambiaPrincipal();
void cambiaPersonal(int j = -1);
bool hayCambios();

// Otros archivos de la partida (los que no son main.dat ni personal.dat), por ejemplo las fotos o landname.dat: leer y
// escribir tal cual. Escribir solo se guarda con guardar().
bool leerArchivo(const std::string& ruta, std::vector<uint8_t>& datos);
std::vector<std::string> archivos(const std::string& carpeta); // names of the files in a folder of the save

std::string abrir();   // empty if it went well, or why not
std::string guardar(); // empty if it went well, or why not
void cerrar();

// Guardar otro archivo de la SD (exportar) o leerlo (importar). Rutas «sdmc:/...».
bool escribirSd(const std::string& ruta, const std::vector<uint8_t>& datos);
bool leerSd(const std::string& ruta, std::vector<uint8_t>& datos);
std::vector<std::string> listarSd(const std::string& carpeta, const std::string& extension); // names, sorted
constexpr const char* CARPETA = "sdmc:/switch/NHSE/";

} // namespace partida
