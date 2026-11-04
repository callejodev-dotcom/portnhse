#!/usr/bin/env python3
"""Los dibujos de las casas por fuera (tejados, fachadas y puertas), y los de los puentes y las cuestas, para NHSE para
Switch.

    python3 fachadas.py CARPETA [SALIDA]

CARPETA es la de los dibujos que descarga NHSE para Mac (~/Library/Application Support/NHSE/img/exterior). Hace
fachadas.bin (por defecto en esta carpeta), para copiar a la SD en /switch/NHSE/fachadas.bin. No se sube al repo: son
imágenes del juego.

Como HouseComposer del Mac: las fachadas de una misma familia («HouseWallPA04», «HouseWallPB04»...) están dibujadas
desde el mismo sitio, así que lo que es tejado (siempre gris claro) y el hueco de la puerta (siempre iluminado) se
encuentran comparando todos los dibujos de la familia. Eso se hace aquí, una vez; la consola solo pinta el tejado
elegido encima y pone la puerta.

Formato: "FACH", versión 1, número de dibujos; por dibujo (ordenados por nombre): nombre (40 bytes), ancho y alto
(16 bits), marcas (1 hay tejado, 2 hay puerta, 4 la puerta es exacta), un byte libre, el hueco de la puerta (x, y,
ancho, alto, 16 bits), y posición y tamaño de los puntos (RGBA, zlib) y del tejado (un byte por punto, zlib). Todo en
little endian. No necesita nada aparte de Python.
"""
import collections
import os
import struct
import sys
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))


def leer_png(ruta):
    """(ancho, alto, puntos RGBA en bytes) de un PNG de 8 bits por canal sin entrelazar."""
    with open(ruta, "rb") as f:
        datos = f.read()
    if datos[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(ruta + ": no es un PNG")
    i = 8
    idat = b""
    paleta, transparencia = None, None
    while i < len(datos):
        largo, tipo = struct.unpack(">I4s", datos[i:i + 8])
        trozo = datos[i + 8:i + 8 + largo]
        if tipo == b"IHDR":
            w, h, bits, color, _, _, entrelazado = struct.unpack(">IIBBBBB", trozo)
        elif tipo == b"PLTE":
            paleta = trozo
        elif tipo == b"tRNS":
            transparencia = trozo
        elif tipo == b"IDAT":
            idat += trozo
        i += 12 + largo
    if bits != 8 or entrelazado:
        raise ValueError(ruta + ": formato no admitido")
    canales = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    crudo = zlib.decompress(idat)
    paso = w * canales
    filas = []
    anterior = bytearray(paso)
    pos = 0
    for _ in range(h):
        filtro = crudo[pos]
        fila = bytearray(crudo[pos + 1:pos + 1 + paso])
        pos += 1 + paso
        for x in range(paso):
            a = fila[x - canales] if x >= canales else 0
            b = anterior[x]
            c = anterior[x - canales] if x >= canales else 0
            if filtro == 1:
                fila[x] = (fila[x] + a) & 255
            elif filtro == 2:
                fila[x] = (fila[x] + b) & 255
            elif filtro == 3:
                fila[x] = (fila[x] + ((a + b) >> 1)) & 255
            elif filtro == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                fila[x] = (fila[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        filas.append(bytes(fila))
        anterior = fila
    puntos = bytearray()
    for fila in filas:
        if color == 6:
            puntos += fila
        elif color == 2:
            for x in range(w):
                puntos += fila[3 * x:3 * x + 3] + b"\xff"
        elif color == 0:
            for v in fila:
                puntos += bytes((v, v, v, 255))
        elif color == 4:
            for x in range(w):
                v, a = fila[2 * x], fila[2 * x + 1]
                puntos += bytes((v, v, v, a))
        else:
            for k in fila:
                a = transparencia[k] if transparencia and k < len(transparencia) else 255
                puntos += paleta[3 * k:3 * k + 3] + bytes((a,))
    return w, h, bytes(puntos)


def familia(nombre):
    """ "HouseWallPB04BrickF" → "HouseWallPB04", "HouseWallNDStuccoD" → "HouseWallND" (HouseComposer.Family). """
    if len(nombre) > 13 and nombre[9] == "P":
        return nombre[:13]
    return nombre[:11] if len(nombre) > 11 else nombre


def grupos(mascara, w, h):
    """Grupos de puntos que se tocan: (puntos, x0, y0, x1, y1 (sin incluir), y medio)."""
    visto = bytearray(len(mascara))
    res = []
    for inicio in range(len(mascara)):
        if not mascara[inicio] or visto[inicio]:
            continue
        cola = collections.deque([inicio])
        visto[inicio] = 1
        puntos = []
        x0, y0, x1, y1, suma = w, h, 0, 0, 0
        while cola:
            i = cola.popleft()
            x, y = i % w, i // w
            puntos.append(i)
            suma += y
            x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
            for n in (i - 1, i + 1, i - w, i + w):
                if n < 0 or n >= len(mascara) or visto[n] or not mascara[n] or abs(n % w - x) > 1:
                    continue
                visto[n] = 1
                cola.append(n)
        res.append((puntos, x0, y0, x1 + 1, y1 + 1, suma / len(puntos)))
    return res


def mascaras_familia(miembros, w, h):
    """El tejado y el hueco de la puerta de una familia (HouseComposer.GetMasks); None si hay menos de cuatro."""
    if len(miembros) < 4:
        return None
    igual = bytearray(w * h)
    for i in range(w * h):
        lo = [255, 255, 255]
        hi = [0, 0, 0]
        opaco = True
        for px in miembros:
            r, g, b, a = px[4 * i:4 * i + 4]
            if a < 200:
                opaco = False
                break
            lo = [min(lo[0], r), min(lo[1], g), min(lo[2], b)]
            hi = [max(hi[0], r), max(hi[1], g), max(hi[2], b)]
        igual[i] = 1 if opaco and max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) <= 14 else 0
    partes = grupos(igual, w, h)
    candidatas = [p for p in partes if p[4] >= h * 0.9 and p[2] > h * 0.3]
    puerta = max(candidatas, key=lambda p: len(p[0])) if candidatas else None
    hueco = (puerta[1], puerta[2], puerta[3], puerta[4]) if puerta else None
    # Una línea que cambia entre dibujos puede partir el hueco en dos: se juntan las partes justo encima.
    crecio = hueco is not None
    while crecio:
        crecio = False
        for p in partes:
            bx0, by0, bx1, by1 = p[1], p[2], p[3], p[4]
            dx0, dy0, dx1, dy1 = hueco
            if p is puerta or by0 >= dy0 or by1 < dy0 - 10 or by0 < h * 0.3:
                continue
            if min(bx1, dx1) - max(bx0, dx0) < (dx1 - dx0) * 0.7:
                continue
            nuevo = (min(bx0, dx0), min(by0, dy0), max(bx1, dx1), max(by1, dy1))
            if nuevo != hueco:
                hueco = nuevo
                crecio = True
    tejado = bytearray(w * h)
    for p in partes:
        if p is puerta or len(p[0]) < 150 or p[5] > h * 0.6:
            continue
        if hueco and hueco[0] <= p[1] and hueco[1] <= p[2] and hueco[2] >= p[3] and hueco[3] >= p[4]:
            continue
        for i in p[0]:
            tejado[i] = 1
    return tejado, hueco, False


def iluminado(r, g, b, a):
    return a > 200 and r > 140 and g > 95 and r - b > 45 and b < 170


def solo(px, w, h):
    """Una fachada sola (las casas pequeñas): solo el hueco de la puerta, por su luz (HouseComposer.Single)."""
    luz = bytearray(w * h)
    for y in range(h // 3, h):
        for x in range(w):
            i = y * w + x
            luz[i] = 1 if iluminado(*px[4 * i:4 * i + 4]) else 0
    partes = [p for p in grupos(luz, w, h) if len(p[0]) > 40]
    puerta = max(partes, key=lambda p: len(p[0])) if partes else None
    return None, (puerta[1], puerta[2], puerta[3], puerta[4]) if puerta else None, True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    carpeta = sys.argv[1]
    salida = sys.argv[2] if len(sys.argv) > 2 else os.path.join(AQUI, "fachadas.bin")
    dibujos = {}
    for nombre in sorted(os.listdir(carpeta)):
        if nombre.startswith(("House", "Bridge", "Slope")) and nombre.endswith(".png"):
            dibujos[nombre[:-4]] = leer_png(os.path.join(carpeta, nombre))
    # Las máscaras de cada familia de fachadas, con el tamaño que más tienen.
    familias = collections.defaultdict(list)
    for n in dibujos:
        if n.startswith("HouseWall"):
            familias[familia(n)].append(n)
    hechas = {}
    for f, nombres in familias.items():
        tam = collections.Counter((dibujos[n][0], dibujos[n][1]) for n in nombres).most_common(1)[0][0]
        miembros = [dibujos[n][2] for n in nombres if (dibujos[n][0], dibujos[n][1]) == tam]
        hechas[f] = (tam, mascaras_familia(miembros, *tam))
        print(f"{f}: {len(nombres)} fachadas, {'con' if hechas[f][1] else 'sin'} tejado")
    entradas, blobs = [], []
    pos = 16 + (40 + struct.calcsize("<HHBBHHHHIIII")) * len(dibujos)  # 70 bytes por dibujo
    for n in sorted(dibujos):
        w, h, px = dibujos[n]
        tejado, hueco, exacta = None, None, False
        if n.startswith("HouseWall"):
            tam, m = hechas[familia(n)]
            tejado, hueco, exacta = m if m and tam == (w, h) else solo(px, w, h)
        zp = zlib.compress(px, 9)
        zt = zlib.compress(bytes(tejado), 9) if tejado else b""
        marcas = (1 if tejado else 0) | (2 if hueco else 0) | (4 if exacta else 0)
        hx, hy, hx1, hy1 = hueco if hueco else (0, 0, 0, 0)
        entradas.append(n.encode("ascii").ljust(40, b"\0") + struct.pack("<HHBBHHHHIIII", w, h, marcas, 0, hx, hy, hx1 - hx, hy1 - hy,
                                                                       pos, len(zp), pos + len(zp) if zt else 0, len(zt)))
        blobs += [zp, zt]
        pos += len(zp) + len(zt)
    with open(salida, "wb") as f:
        f.write(b"FACH" + struct.pack("<III", 1, len(entradas), 0))
        f.write(b"".join(entradas))
        f.write(b"".join(blobs))
    print(f"{salida}: {len(entradas)} dibujos, {pos / 1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
