#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# Fichier : gen_structure_textures.py
# Rôle : Outil de développement — génère les textures (pixel art 16x16, ou
# 64x64 quand elles dérivent d'une texture existante) des blocs de
# structures : briques, grès, planches, lanternes, torches, foin, blé...
# Déterministe (graine fixe). Usage : python3 scripts/gen_structure_textures.py
# -----------------------------------------------------------------------------
import os

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "assets", "textures", "block")
rng = np.random.default_rng(1234)


def save(name, img):
    img = np.clip(img, 0, 255).astype(np.uint8)
    if img.shape[2] == 3:
        a = np.full(img.shape[:2] + (1,), 255, np.uint8)
        img = np.concatenate([img, a], axis=2)
    Image.fromarray(img, "RGBA").save(os.path.join(OUT, name + ".png"))
    print("wrote", name)


def load(name):
    return np.array(Image.open(os.path.join(OUT, name + ".png")).convert("RGBA")).astype(float)


def noise(n=16, amp=1.0):
    return rng.uniform(-amp, amp, (n, n, 1))


def fill(color, n=16):
    return np.ones((n, n, 3)) * np.array(color, float)


def lum(img):
    return img[..., 0] * 0.299 + img[..., 1] * 0.587 + img[..., 2] * 0.114


def recolor(img, dark, light):
    """Remappe la luminance d'une texture sur un dégradé dark -> light"""
    l = lum(img[..., :3])
    t = (l - l.min()) / max(1e-3, l.max() - l.min())
    out = np.array(dark, float) + t[..., None] * (np.array(light, float) - np.array(dark, float))
    return np.concatenate([out, img[..., 3:4]], axis=2)


def bricks(base, mortar, rows, width, var=14, bevel=10, n=16, jitter=0.0):
    img = np.zeros((n, n, 3))
    h = n // rows
    for r in range(rows):
        off = (width // 2) * (r % 2)
        for bx in range(-1, n // width + 1):
            x0 = bx * width + off
            tone = np.array(base, float) * (1.0 + rng.uniform(-jitter, jitter)) + rng.uniform(-var, var)
            for y in range(r * h, r * h + h):
                for x in range(max(0, x0), min(n, x0 + width)):
                    c = tone + rng.uniform(-var * 0.6, var * 0.6)
                    ly, lx = y - r * h, x - x0
                    if ly == h - 1 or lx == width - 1:
                        c = np.array(mortar, float) + rng.uniform(-6, 6)
                    elif ly == 0 or lx == 0:
                        c = c + bevel
                    elif ly == h - 2 or lx == width - 2:
                        c = c - bevel * 0.6
                    img[y, x] = c
    return img


def moss(img, amount, seed_mask=None):
    n = img.shape[0]
    m = rng.uniform(0, 1, (n, n))
    # Taches plus denses en haut et le long des joints sombres
    l = lum(img)
    m += (l < l.mean() - 20) * 0.35
    m += np.linspace(0.25, -0.15, n)[:, None]
    out = img.copy()
    mask = m > 1.0 - amount
    green = np.array([84, 112, 46], float)
    for y, x in zip(*np.nonzero(mask)):
        out[y, x] = green + rng.uniform(-14, 14, 3) + np.array([0, rng.uniform(0, 16), 0])
    return out


# ── Pierre taillée ───────────────────────────────────────────────
sb = bricks((122, 122, 124), (78, 78, 80), rows=2, width=16)
save("stone_bricks", sb)
save("mossy_stone_bricks", moss(sb, 0.34))
cob = load("cobblestone")
save("mossy_cobblestone", np.concatenate([moss(cob[..., :3], 0.38), cob[..., 3:4]], axis=2))
save("bricks", bricks((150, 72, 56), (168, 160, 150), rows=4, width=8, var=12, bevel=8, jitter=0.08))

# ── Grès ─────────────────────────────────────────────────────────
sand = (219, 206, 152)
top = fill(sand) + noise(amp=5)
save("sandstone_top", top)
bot = fill((210, 196, 142)) + noise(amp=9)
save("sandstone_bottom", bot)
side = fill(sand) + noise(amp=4)
side[0:4] = np.array((226, 214, 162)) + noise(amp=3)[:4]
side[4] = np.array((192, 176, 122))
for y in range(5, 12):
    side[y] = np.array(sand) - (y % 3 == 0) * 10 + rng.uniform(-7, 7, (16, 3))
side[12] = np.array((196, 180, 126))
side[13:16] = np.array((205, 190, 136)) + noise(amp=10)[:3]
save("sandstone", side)
ch = fill(sand) + noise(amp=4)
ch[0:2] = ch[14:16] = np.array((228, 216, 166))
ch[1] = ch[14] = np.array((196, 180, 126))
glyph = [
    "................",
    "................",
    "................",
    "....########....",
    "...#........#...",
    "...#.##..##.#...",
    "...#.##..##.#...",
    "...#........#...",
    "...#...##...#...",
    "...#..####..#...",
    "...#..#..#..#...",
    "....########....",
    "................",
    "................",
    "................",
    "................",
]
for y, row in enumerate(glyph):
    for x, c in enumerate(row):
        if c == "#":
            ch[y, x] = np.array((176, 158, 104)) + rng.uniform(-5, 5)
save("chiseled_sandstone", ch)

# ── Bois ─────────────────────────────────────────────────────────
oak = load("oak_planks")
save("spruce_planks", recolor(oak, (70, 48, 28), (134, 100, 62)))
save("cherry_planks", recolor(oak, (166, 102, 100), (236, 196, 186)))
ver = fill((182, 52, 32)) + noise(amp=6)
for x in range(16):
    ver[:, x] += rng.uniform(-8, 8)
ver[0] += 14
ver[15] -= 18
save("vermilion_planks", ver)
pl = fill((226, 222, 210)) + noise(amp=5)
pl += (rng.uniform(0, 1, (16, 16, 1)) > 0.93) * -16
save("white_plaster", pl)

# ── Lumières ─────────────────────────────────────────────────────
lan = np.zeros((16, 16, 3))
yy, xx = np.mgrid[0:16, 0:16]
d = np.sqrt((xx - 7.5) ** 2 + (yy - 7.5) ** 2) / 8.0
glow_c = np.array((255, 236, 170), float)
glow_e = np.array((236, 140, 52), float)
lan[:] = glow_c * (1 - d[..., None]) + glow_e * d[..., None]
lan += noise(amp=6)
frame = np.array((44, 40, 42), float)
lan[[0, 15], :] = frame
lan[:, [0, 15]] = frame
lan[[5, 10], 1:15] = frame + 18
lan[1:15, [7, 8]] *= 0.9
for (y, x) in [(1, 1), (1, 14), (14, 1), (14, 14)]:
    lan[y, x] = (90, 84, 80)
save("lantern", lan)

torch = np.zeros((16, 16, 4))
for y in range(6, 16):
    for x in (7, 8):
        torch[y, x] = (*((112, 80, 44) if x == 7 else (86, 60, 34)), 255)
for (y, x, c) in [(3, 7, (255, 200, 80)), (3, 8, (255, 170, 60)), (4, 6, (255, 150, 40)),
                  (4, 7, (255, 246, 190)), (4, 8, (255, 236, 150)), (4, 9, (255, 150, 40)),
                  (5, 6, (240, 120, 30)), (5, 7, (255, 230, 130)), (5, 8, (255, 210, 100)),
                  (5, 9, (240, 120, 30)), (2, 8, (255, 140, 40))]:
    torch[y, x] = (*c, 255)
save("torch", torch)

# ── Ferme ────────────────────────────────────────────────────────
hay = fill((206, 172, 44)) + noise(amp=10)
for x in range(16):
    hay[:, x] += (x % 3 == 0) * -16 + rng.uniform(-6, 6)
for y in (3, 4, 11, 12):
    hay[y] = np.array((138, 92, 30)) + rng.uniform(-8, 8, (16, 3))
save("hay_block_side", hay)
ht = fill((214, 180, 52)) + noise(amp=16)
ht[[0, 15], :] -= 30
ht[:, [0, 15]] -= 30
save("hay_block_top", ht)

farm = fill((92, 60, 36)) + noise(amp=7)
for y in range(16):
    if y % 4 == 3:
        farm[y] -= 24
    elif y % 4 == 0:
        farm[y] += 10
save("farmland", farm)

wheat = np.zeros((16, 16, 4))
for x in (1, 4, 6, 9, 11, 14):
    h = int(rng.integers(9, 13))
    for y in range(15, 15 - h, -1):
        t = (15 - y) / h
        c = np.array((128, 150, 52)) * (1 - t) + np.array((204, 176, 70)) * t
        wheat[y, x] = (*c, 255)
    for y in range(15 - h - 1, 15 - h + 4):
        if 0 <= y < 16:
            for dx in (-1, 1):
                if (y + dx) % 2 == 0 and 0 <= x + dx < 16:
                    wheat[y, x + dx] = (*(np.array((214, 186, 84)) + rng.uniform(-14, 14)), 255)
            wheat[y, x] = (*(np.array((196, 160, 64)) + rng.uniform(-10, 10)), 255)
save("wheat", wheat)

# ── Chemins ──────────────────────────────────────────────────────
path = fill((150, 122, 74)) + noise(amp=7)
path += (rng.uniform(0, 1, (16, 16, 1)) > 0.85) * 16
path += (rng.uniform(0, 1, (16, 16, 1)) > 0.88) * -18
save("dirt_path_top", path)
dirt = load("dirt_side")[..., :3]
pside = dirt.copy()
band = np.kron(path[:3], np.ones((4, 4, 1)))
pside[:12] = band
pside[12:14] = pside[12:14] * 0.8
save("dirt_path_side", pside)

# ── Décor ────────────────────────────────────────────────────────
web = np.zeros((16, 16, 4))
cx, cy = 7.5, 7.5
for a in np.linspace(0, 2 * np.pi, 8, endpoint=False):
    for r in np.linspace(0, 10, 40):
        x, y = int(round(cx + np.cos(a) * r)), int(round(cy + np.sin(a) * r))
        if 0 <= x < 16 and 0 <= y < 16:
            web[y, x] = (232, 232, 236, 255)
for rad in (2.5, 5.0, 7.5):
    for a in np.linspace(0, 2 * np.pi, 48):
        x, y = int(round(cx + np.cos(a) * rad)), int(round(cy + np.sin(a) * rad))
        if 0 <= x < 16 and 0 <= y < 16 and rng.uniform() > 0.15:
            web[y, x] = (214, 214, 220, 255)
save("cobweb", web)

bush = np.zeros((16, 16, 4))
def branch(x, y, dx, n):
    for i in range(n):
        if 0 <= x < 16 and 0 <= y < 16:
            bush[y, x] = (*(np.array((122, 84, 44)) + rng.uniform(-12, 12)), 255)
        y -= 1
        if rng.uniform() < 0.55:
            x += dx
branch(7, 15, 0, 6)
branch(7, 11, -1, 6)
branch(8, 12, 1, 7)
branch(6, 9, -1, 4)
branch(9, 8, 1, 5)
branch(8, 10, 0, 6)
save("dead_bush", bush)

cac = fill((86, 136, 48)) + noise(amp=6)
for x in (3, 8, 12):
    cac[:, x] -= 22
cac[:, [0, 15]] -= 30
for _ in range(14):
    y, x = rng.integers(0, 16), rng.choice([2, 4, 7, 9, 11, 13])
    cac[y, x] = (214, 214, 170)
save("cactus_side", cac)
ct = fill((98, 150, 56)) + noise(amp=6)
ct[[0, 15], :] -= 30
ct[:, [0, 15]] -= 30
ct[6:10, 6:10] += 16
save("cactus_top", ct)

pump = fill((222, 128, 30)) + noise(amp=7)
for x in (0, 4, 8, 12):
    pump[:, x] -= 40
pump[:, [3, 7, 11, 15]] -= 12
save("pumpkin_side", pump)
pt = fill((214, 124, 30)) + noise(amp=7)
for k in range(16):
    pt[k, 7] -= 28
    pt[7, k] -= 28
pt[6:10, 6:10] = np.array((96, 76, 36)) + noise(amp=8)[:4, :4]
save("pumpkin_top", pt)
jack = pump.copy()
face = [
    "................",
    "................",
    "................",
    "...##......##...",
    "..####....####..",
    "..####....####..",
    "................",
    ".......##.......",
    "................",
    "..############..",
    "..#.########.#..",
    "...##########...",
    "....###..###....",
    "................",
    "................",
    "................",
]
for y, row in enumerate(face):
    for x, c in enumerate(row):
        if c == "#":
            jack[y, x] = np.array((255, 226, 120)) + rng.uniform(-10, 10)
save("jack_o_lantern", jack)
