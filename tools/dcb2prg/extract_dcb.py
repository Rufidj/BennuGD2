#!/usr/bin/env python3
"""Saca el .dcb que va incrustado dentro de un ejecutable.

BennuGD y Fenix pueden pegar el .dcb detras de un "stub" (el interprete) para
repartir el juego como un solo .exe. Al final del fichero queda una firma con el
offset donde empieza el .dcb.

    python3 extract_dcb.py <ejecutable> [salida.dcb]

Hay DOS formas de la cola, segun con que se compilo:

    "DCB Stub\\x1a\\r\\n\\0" + offset de 4 bytes    (16 bytes en total)
    "DCB Stub\\x1a\\r\\n"    + ... + offset de 8    (20 bytes, BennuGD de 64)

y ademas un ejecutable puede llevar el .dcb sin cola ninguna. Se prueban las dos
colas y, si no hay, se busca la cabecera "dcb\\r\\n\\x1f" por el fichero. La
version que sale se comprueba contra las conocidas para no escribir basura.

Con esto se saco el .dcb de 5Leaps.exe.
"""
import os
import struct
import sys

FIRMA = b"DCB Stub\x1a\r\n"
MAGIA = b"dcb\r\n\x1f\x00\x00"
# Fenix llega a la 0x0500; BennuGD1 usa 0x0710 y BennuGD2 0x0800 y siguientes
VERSIONES = (0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0710, 0x0800, 0x0810)


def version_en(datos, off):
    """La version que declara la cabecera en ese offset, o None si no hay."""
    if off < 0 or off + 16 > len(datos) or datos[off:off + 8] != MAGIA:
        return None
    v32, = struct.unpack_from("<I", datos, off + 8)
    v64, = struct.unpack_from("<Q", datos, off + 8)
    for v in (v32, v64):
        if v in VERSIONES:
            return v
    return None


def buscar(datos):
    """(offset, version, como se encontro) del .dcb dentro del ejecutable."""
    n = len(datos)
    # 1. cola de 16 bytes con offset de 32 bits
    if n >= 16 and datos[n - 16:n - 4] == FIRMA + b"\0":
        off, = struct.unpack_from("<I", datos, n - 4)
        v = version_en(datos, off)
        if v:
            return off, v, "cola de 16 bytes (offset de 32 bits)"
    # 2. cola de 20 bytes con offset de 64 bits
    if n >= 20 and datos[n - 20:n - 9] == FIRMA:
        off, = struct.unpack_from("<q", datos, n - 8)
        v = version_en(datos, off)
        if v:
            return off, v, "cola de 20 bytes (offset de 64 bits)"
    # 3. sin cola: se busca la cabecera
    i = datos.find(MAGIA)
    while i >= 0:
        v = version_en(datos, i)
        if v:
            return i, v, "cabecera encontrada por busqueda"
        i = datos.find(MAGIA, i + 1)
    return None, None, None


def extraer(entrada, salida=None):
    if not os.path.isfile(entrada):
        print("no existe: %s" % entrada)
        return 2
    datos = open(entrada, "rb").read()
    off, ver, como = buscar(datos)
    if off is None:
        print("no encuentro ningun .dcb dentro de %s" % entrada)
        return 1
    if not salida:
        salida = os.path.splitext(entrada)[0] + ".dcb"
    # El .dcb llega hasta el final; la cola del stub, si la hay, sobra. Dejarla
    # no estorba (el parser va por offsets), pero se quita para que el fichero
    # sea exactamente el .dcb.
    fin = len(datos)
    for corte in (16, 20):
        if datos[fin - corte:fin - corte + 11] == FIRMA:
            fin -= corte
            break
    trozo = datos[off:fin]
    with open(salida, "wb") as fh:
        fh.write(trozo)
    print("   %s" % como)
    print("   empieza en : 0x%X (%d)" % (off, off))
    print("   version    : 0x%04X (%s)"
          % (ver, "Fenix" if ver < 0x0700 else
             "BennuGD1" if ver < 0x0800 else "BennuGD2"))
    print("   tamaño     : %d bytes" % len(trozo))
    print("   escrito en : %s" % salida)
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    return extraer(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)


if __name__ == "__main__":
    sys.exit(main())
