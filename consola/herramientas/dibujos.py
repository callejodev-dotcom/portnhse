#!/usr/bin/env python3
"""Dibujos de los objetos para NHSE para Switch.

    python3 dibujos.py                 rehace romfs/iconos.bin (los iconos de menú de NHSE, van en la app) y
                                       romfs/menu.bin (los mismos por su nombre, para las tarjetas: «Bag», «Kabu»...)
    python3 dibujos.py --sd CARPETA    además hace dibujos.bin con los dibujos de cada objeto de CARPETA (la de NHSE
                                       para Mac: ~/Library/Application Support/NHSE/img), para copiar a la SD en
                                       /switch/NHSE/dibujos.bin. No se sube al repo: son imágenes del juego.

Formato (los dos): "DIBU", versión 1, lado, número; por dibujo, clave (objeto << 16 | variante), posición y tamaño,
ordenados por clave; luego cada dibujo comprimido con zlib: número de colores, los colores (RGBA) y un byte por
píxel (lado × lado). Necesita Pillow.
"""
import os
import struct
import sys
import zlib

from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
LADO = 48


def comprimir(ruta):
    im = Image.open(ruta).convert("RGBA").resize((LADO, LADO), Image.LANCZOS)
    q = im.quantize(64, method=Image.Quantize.FASTOCTREE)
    pal = q.getpalette(rawmode="RGBA")
    n = len(pal) // 4
    datos = bytes([n]) + bytes(pal[: n * 4]) + q.tobytes()
    return zlib.compress(datos, 9)


def escribir(ruta, entradas):
    """entradas: {clave: blob}; los blobs iguales se guardan una vez."""
    claves = sorted(entradas)
    blobs, donde = [], {}
    cabecera = 16 + 12 * len(claves)
    pos = cabecera
    indice = []
    for k in claves:
        b = entradas[k]
        if b not in donde:
            donde[b] = pos
            blobs.append(b)
            pos += len(b)
        indice.append(struct.pack("<III", k, donde[b], len(b)))
    with open(ruta, "wb") as f:
        f.write(b"DIBU" + struct.pack("<III", 1, LADO, len(claves)))
        f.write(b"".join(indice))
        f.write(b"".join(blobs))
    print(f"{ruta}: {len(claves)} dibujos, {len(blobs)} distintos, {pos / 1e6:.1f} MB")


def tabla():
    filas = []
    with open(os.path.join(AQUI, "dibujos.tsv"), encoding="utf-8") as f:
        for linea in f:
            if linea.startswith("#"):
                continue
            objeto, dibujo, icono = linea.rstrip("\n").split("\t")
            filas.append((int(objeto), int(dibujo), icono))
    return filas


def iconos():
    carpeta = os.path.join(AQUI, "../../NHSE/NHSE.Sprites/Resources")
    hoja = comprimir(os.path.join(carpeta, "Misc/leaf.png"))
    hechos = {}
    entradas = {}
    for objeto, _, icono in tabla():
        if icono not in hechos:
            ruta = os.path.join(carpeta, "MenuIcon", icono + ".png")
            hechos[icono] = comprimir(ruta) if os.path.exists(ruta) else hoja
        entradas[objeto << 16] = hechos[icono]
    escribir(os.path.join(AQUI, "../romfs/iconos.bin"), entradas)


def menu():
    """Los iconos de menú por su nombre (clave: CRC-32 del nombre), como los usa NHSE para Mac en sus tarjetas."""
    carpeta = os.path.join(AQUI, "../../NHSE/NHSE.Sprites/Resources")
    entradas = {}
    for sub in ("MenuIcon", "Misc"):
        for nombre in sorted(os.listdir(os.path.join(carpeta, sub))):
            base, ext = os.path.splitext(nombre)
            if ext == ".png":
                entradas[zlib.crc32(base.encode())] = comprimir(os.path.join(carpeta, sub, nombre))
    escribir(os.path.join(AQUI, "../romfs/menu.bin"), entradas)


def paquete(carpeta, salida):
    entradas = {}
    for nombre in sorted(os.listdir(carpeta)):
        base, ext = os.path.splitext(nombre)
        if ext != ".png" or "_" not in base:
            continue
        objeto, variante = base.split("_", 1)
        if not (objeto.isdigit() and variante.isdigit()) or int(variante) > 0xFFFF:
            continue
        entradas[(int(objeto) << 16) | int(variante)] = comprimir(os.path.join(carpeta, nombre))
    # Los objetos del suelo usan el dibujo del objeto que dan.
    for objeto, dibujo, _ in tabla():
        if objeto != dibujo and (dibujo << 16) in entradas:
            entradas[objeto << 16] = entradas[dibujo << 16]
    escribir(salida, entradas)


if __name__ == "__main__":
    iconos()
    menu()
    if len(sys.argv) >= 3 and sys.argv[1] == "--sd":
        paquete(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else os.path.join(AQUI, "dibujos.bin"))
