#!/usr/bin/env python3
"""PNG/JPG -> KTX2 (UASTC + Zstd + mips), .ktx2 кладётся рядом с исходником.
Использование: convert_textures.py <файл|папка> ...
Классификация по имени: albedo=sRGB, normal=linear, mr=linear + запекание B/2 (как делал Engine.cpp)."""
import os, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image

EXTS = {".png", ".jpg", ".jpeg"}

def classify(stem):
    n = stem.lower()
    if any(k in n for k in ("occlusionroughnessmetallic", "roughness", "metallic")):
        return "mr"
    if "normal" in n:
        return "normal"
    if any(k in n for k in ("basecolor", "albedo", "diffuse", "color")):
        return "albedo"
    return None

def convert(src):
    kind = classify(src.stem)
    if kind is None:
        print(f"[skip] {src}: неизвестный тип")
        return None
    out = src.with_suffix(".ktx2")
    if out.exists() and out.stat().st_mtime >= src.stat().st_mtime:
        return None

    oetf = "srgb" if kind == "albedo" else "linear"
    tmp, inp = None, src
    if kind == "mr":
        r, g, b, a = Image.open(src).convert("RGBA").split()
        b = b.point(lambda v: v // 2)
        fd, tmp = tempfile.mkstemp(suffix=".png")
        os.close(fd)
        Image.merge("RGBA", (r, g, b, a)).save(tmp)
        inp = Path(tmp)
    try:
        subprocess.run(["toktx", "--t2", "--uastc", "2", "--zcmp", "19",
                        "--genmipmap", "--assign_oetf", oetf, str(out), str(inp)],
                       check=True)
    finally:
        if tmp:
            os.unlink(tmp)
    print(f"[ktx2] {kind:6s} {src.name}: {src.stat().st_size//1024} KB -> {out.stat().st_size//1024} KB")
    return src.stat().st_size, out.stat().st_size

def main():
    files = []
    for arg in sys.argv[1:]:
        p = Path(arg)
        files += [f for f in p.rglob("*") if f.suffix.lower() in EXTS] if p.is_dir() else [p]
    total_src = total_out = 0
    for f in files:
        res = convert(f)
        if res:
            total_src += res[0]; total_out += res[1]
    if total_src:
        print(f"Итого: {total_src//1024} KB -> {total_out//1024} KB")

if __name__ == "__main__":
    main()