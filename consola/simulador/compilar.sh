#!/bin/sh
# Compila el simulador de NHSE para Switch en el Mac (hace falta: brew install sdl2 sdl2_ttf).
cd "$(dirname "$0")/.." || exit 1
exec clang++ -std=c++17 -O1 -Wall -Isimulador -o "${1:-/tmp/nhse-sim}" source/app/*.cpp source/core/*.cpp simulador/sim.cpp \
    -I/opt/homebrew/include $(sdl2-config --cflags --libs) -lSDL2_ttf -lz
