#!/usr/bin/env python3
"""Export the committed master PNG to native icon containers. Requires Pillow only when regenerating."""
from pathlib import Path
import struct
from PIL import Image

icons = Path(__file__).resolve().parents[1] / "assets" / "icons"
with Image.open(icons / "usbtree.png") as source:
    image = source.convert("RGBA")

image.save(icons / "usbtree.ico", sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
image.resize((1024, 1024), Image.Resampling.LANCZOS).save(icons / "usbtree.icns")

# Explicit BMP v4 alpha masks avoid opaque corners in SDL and Windows image readers.
window = image.resize((256, 256), Image.Resampling.LANCZOS)
pixels = window.tobytes("raw", "BGRA", 0, -1)
dib = struct.pack("<IiiHHIIiiII", 108, 256, 256, 1, 32, 3, len(pixels), 2835, 2835, 0, 0)
dib += struct.pack("<IIIII", 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 0x73524742)
dib += bytes(48)
header = struct.pack("<2sIHHI", b"BM", 14 + len(dib) + len(pixels), 0, 0, 14 + len(dib))
(icons / "usbtree.bmp").write_bytes(header + dib + pixels)
with Image.open(icons / "usbtree.bmp") as decoded:
    if decoded.mode != "RGBA" or decoded.tobytes() != window.tobytes():
        raise RuntimeError("BMP conversion did not preserve pixels and transparency")
print("Exported usbtree.ico, usbtree.icns and usbtree.bmp")
