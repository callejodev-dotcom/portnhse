#!/usr/bin/env python3
"""Retratos de los vecinos para NHSE para Switch.

    python3 retratos.py      rehace romfs/retratos.bin con la cara de cada vecino (las de NHSE.Sprites/Resources/Villagers,
                             que ya son públicas en NHSE); van en la app.

Mismo formato que dibujos.py («DIBU»), con LADO píxeles por lado y, como clave de cada retrato, el CRC-32 de su nombre
interno («kal09», «cat04»...). Necesita Pillow.
"""
import os
import zlib

import dibujos

AQUI = os.path.dirname(os.path.abspath(__file__))
LADO = 128


def retratos():
    carpeta = os.path.join(AQUI, "../../NHSE/NHSE.Sprites/Resources/Villagers")
    dibujos.LADO = LADO
    entradas = {}
    for nombre in sorted(os.listdir(carpeta)):
        base, ext = os.path.splitext(nombre)
        if ext != ".png":
            continue
        clave = zlib.crc32(base.encode("ascii"))
        if clave in entradas:
            raise SystemExit(f"Dos retratos con la misma clave: {base}")
        entradas[clave] = dibujos.comprimir(os.path.join(carpeta, nombre))
    dibujos.escribir(os.path.join(AQUI, "../romfs/retratos.bin"), entradas)


if __name__ == "__main__":
    retratos()
