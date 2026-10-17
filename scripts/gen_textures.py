#!/usr/bin/env python3
"""Génère les textures procédurales (pixel art 16x16) des nouveaux blocs.

Usage : python3 scripts/gen_textures.py   (depuis la racine du projet)

Déterministe (graine fixe) : relancer le script redonne les mêmes images.
Les feuillages reçoivent des trous (alpha = 0) pour le rendu "cutout".
"""
import math
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "block")
S = 16


def clamp(v):
    return max(0, min(255, int(v)))


def jitter(rng, color, amount):
    d = rng.uniform(-amount, amount)
    return tuple(clamp(c + d * (0.6 + 0.4 * (i == 1))) for i, c in enumerate(color))


def save(img, name):
    img.save(os.path.join(OUT, name + ".png"))
    print("  wrote", name + ".png")


def noise_tile(seed, base, amount):
    rng = random.Random(seed)
    img = Image.new("RGBA", (S, S))
    for y in range(S):
        for x in range(S):
            img.putpixel((x, y), jitter(rng, base, amount) + (255,))
    return img


# ── Neige ───────────────────────────────────────────────────────
def snow():
    rng = random.Random(1)
    img = Image.new("RGBA", (S, S))
    for y in range(S):
        for x in range(S):
            v = rng.uniform(0, 1)
            c = (236, 242, 250) if v > 0.25 else (214, 225, 240)
            if v > 0.97:
                c = (255, 255, 255)
            img.putpixel((x, y), jitter(rng, c, 5) + (255,))
    save(img, "snow")


# ── Écorces ─────────────────────────────────────────────────────
def bark(name, base, dark, seed, birch=False):
    rng = random.Random(seed)
    img = Image.new("RGBA", (S, S))
    stripes = [rng.uniform(-1, 1) for _ in range(S)]
    for y in range(S):
        for x in range(S):
            c = base
            if birch:
                # Bouleau : blanc avec des marques noires horizontales
                c = jitter(rng, base, 10)
                img.putpixel((x, y), c + (255,))
                continue
            t = stripes[x] + 0.35 * math.sin(y * 0.9 + x * 2.1)
            c = dark if t < -0.25 else base
            img.putpixel((x, y), jitter(rng, c, 9) + (255,))
    if birch:
        for _ in range(7):
            y = rng.randrange(S)
            x0 = rng.randrange(S)
            w = rng.randint(2, 5)
            for x in range(x0, x0 + w):
                img.putpixel((x % S, y), jitter(rng, dark, 8) + (255,))
                if rng.random() < 0.4 and y + 1 < S:
                    img.putpixel((x % S, y + 1), jitter(rng, dark, 8) + (255,))
    save(img, name)


def log_top(name, wood, bark_col, seed):
    rng = random.Random(seed)
    img = Image.new("RGBA", (S, S))
    for y in range(S):
        for x in range(S):
            dx, dy = x - 7.5, y - 7.5
            r = math.sqrt(dx * dx + dy * dy)
            if max(abs(dx), abs(dy)) > 6.6:
                c = bark_col
            else:
                ring = 0.5 + 0.5 * math.sin(r * 2.2)
                c = tuple(clamp(w * (0.82 + 0.18 * ring)) for w in wood)
            img.putpixel((x, y), jitter(rng, c, 6) + (255,))
    save(img, name)


# ── Feuillages (avec trous) ─────────────────────────────────────
def leaves(name, palette, seed, hole=0.22):
    rng = random.Random(seed)
    img = Image.new("RGBA", (S, S))
    for y in range(S):
        for x in range(S):
            if rng.random() < hole:
                img.putpixel((x, y), (0, 0, 0, 0))
                continue
            c = palette[rng.randrange(len(palette))]
            img.putpixel((x, y), jitter(rng, c, 8) + (255,))
    save(img, name)


def punch_holes(name, seed, hole=0.2):
    """Ajoute des trous à une texture de feuillage existante (pixels sombres)."""
    path = os.path.join(OUT, name + ".png")
    img = Image.open(path).convert("RGBA")
    rng = random.Random(seed)
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            lum = 0.3 * r + 0.59 * g + 0.11 * b
            if a > 0 and (lum < 70 and rng.random() < 0.8 or rng.random() < hole * 0.4):
                img.putpixel((x, y), (0, 0, 0, 0))
    save(img, name)


# ── Plantes (sprites en croix) ──────────────────────────────────
def short_grass():
    rng = random.Random(7)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    for _ in range(11):
        x = rng.randrange(1, S - 1)
        height = rng.randint(6, 15)
        lean = rng.uniform(-0.35, 0.35)
        base = (88, 142, 52) if rng.random() < 0.5 else (70, 120, 42)
        for i in range(height):
            px = int(round(x + lean * i))
            py = S - 1 - i
            if 0 <= px < S:
                shade = 0.8 + 0.35 * (i / height)
                c = tuple(clamp(v * shade) for v in base)
                img.putpixel((px, py), jitter(rng, c, 6) + (255,))
    save(img, "short_grass")


def flower(name, petal, center, seed, tall=9):
    rng = random.Random(seed)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    stem = (62, 120, 40)
    cx = 7
    top = S - tall
    for y in range(top, S):
        img.putpixel((cx, y), jitter(rng, stem, 6) + (255,))
    # feuilles
    for dx, dy in ((-1, 3), (-2, 2), (1, 5), (2, 4)):
        img.putpixel((cx + dx, min(S - 1, top + 2 + dy)), jitter(rng, stem, 8) + (255,))
    # pétales
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            if abs(dx) + abs(dy) <= 3 and not (abs(dx) == 2 and abs(dy) == 2):
                img.putpixel((cx + dx, top - 1 + dy), jitter(rng, petal, 14) + (255,))
    img.putpixel((cx, top - 1), center + (255,))
    save(img, name)


def main():
    os.makedirs(OUT, exist_ok=True)
    print("Generating block textures in", os.path.normpath(OUT))
    snow()
    bark("spruce_log", (82, 58, 36), (56, 38, 24), 11)
    log_top("spruce_log_top", (150, 110, 70), (70, 50, 32), 12)
    bark("birch_log", (226, 222, 210), (40, 38, 36), 13, birch=True)
    log_top("birch_log_top", (200, 175, 120), (220, 216, 204), 14)
    bark("cherry_log", (78, 44, 46), (52, 28, 32), 15)
    log_top("cherry_log_top", (220, 160, 150), (78, 44, 46), 16)
    leaves("spruce_leaves", [(38, 78, 52), (30, 64, 44), (48, 92, 60)], 21, hole=0.18)
    leaves("birch_leaves", [(110, 160, 60), (92, 140, 50), (128, 176, 72)], 22)
    leaves("cherry_leaves", [(244, 170, 206), (232, 146, 190), (252, 196, 222), (220, 128, 176)], 23, hole=0.2)
    short_grass()
    flower("dandelion", (250, 214, 40), (240, 160, 20), 31)
    flower("poppy", (210, 30, 34), (40, 30, 20), 32)
    flower("cornflower", (80, 110, 230), (40, 50, 120), 33)
    flower("pink_tulip", (240, 150, 190), (250, 220, 230), 34, tall=10)
    punch_holes("oak_leaves", 41)


if __name__ == "__main__":
    main()
