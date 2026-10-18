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


# ═════════════════════════════════════════════════════════════════
# Villages : toits, vitres, mobilier, laine, cultures, portes
# ═════════════════════════════════════════════════════════════════
def tiles(base, dark, light, rows=4, n=16):
    """Tuiles / ardoises : rangées décalées, arrondi en bas de chaque tuile"""
    img = np.zeros((n, n, 3))
    h = n // rows
    for r in range(rows):
        off = (h // 2 + 1) * (r % 2)
        for y in range(r * h, r * h + h):
            for x in range(n):
                lx = (x + off) % 4
                ly = y - r * h
                c = np.array(base, float) + rng.uniform(-9, 9)
                if ly == h - 1:
                    c = np.array(dark, float)          # ombre sous la rangée suivante
                elif ly == 0:
                    c = np.array(light, float) + rng.uniform(-6, 6)
                elif lx == 0:
                    c = c * 0.82                      # joint entre deux tuiles
                img[y, x] = c
    return img

save("roof_tiles", tiles((176, 72, 48), (104, 38, 26), (204, 102, 70)))
save("slate", tiles((74, 82, 96), (40, 44, 54), (100, 108, 122)))

# Verre : cadre clair et reflets, centre transparent (découpe alpha)
glass = np.zeros((16, 16, 4))
edge = (214, 236, 246, 255)
glass[0, :] = glass[15, :] = edge
glass[:, 0] = glass[:, 15] = edge
for (y, x) in [(3, 4), (4, 3), (5, 2), (2, 5), (11, 11), (12, 10), (10, 12), (3, 12)]:
    glass[y, x] = (236, 248, 255, 255)
save("glass", glass)

def stained(color):
    img = np.zeros((16, 16, 4))
    lead = np.array((38, 36, 40, 255), float)
    for y in range(16):
        for x in range(16):
            # Losanges sertis de plomb
            dd = (x + y) % 8 == 0 or (x - y) % 8 == 0
            if x in (0, 15) or y in (0, 15) or dd:
                img[y, x] = lead
            else:
                c = np.array(color, float) * (0.82 + 0.3 * rng.uniform()) + rng.uniform(-10, 10, 3)
                img[y, x] = (*np.clip(c, 0, 255), 255)
    return img

save("stained_glass_red", stained((190, 40, 40)))
save("stained_glass_blue", stained((50, 80, 200)))
save("stained_glass_yellow", stained((230, 190, 50)))

# Bibliothèque : deux étagères de livres
oak16 = np.array(Image.open(os.path.join(OUT, "oak_planks.png")).convert("RGB").resize((16, 16), Image.NEAREST), float)
book = np.zeros((16, 16, 3))
book[:] = oak16
spines = [(130, 40, 36), (52, 86, 140), (60, 110, 54), (150, 120, 60), (96, 52, 110), (170, 150, 120)]
for top in (1, 9):
    book[top + 6] = np.array((92, 64, 34))
    x = 1
    while x < 15:
        w = 1 + int(rng.integers(0, 2))
        c = np.array(spines[int(rng.integers(0, len(spines)))], float)
        hgt = int(rng.integers(4, 7))
        for xx in range(x, min(15, x + w)):
            for y in range(top + 6 - hgt, top + 6):
                book[y, xx] = c * (0.85 + 0.25 * (xx == x)) + rng.uniform(-6, 6)
        x += w
book[0] = book[8] = book[15] = np.array((110, 78, 44))
save("bookshelf", book)

# Tonneau
bar = np.zeros((16, 16, 3))
for x in range(16):
    bar[:, x] = np.array((120, 82, 46)) * (0.86 + 0.14 * ((x // 3) % 2)) + rng.uniform(-6, 6)
    bar[:, x] -= (x % 3 == 0) * 18
for y in (2, 3, 12, 13):
    bar[y] = np.array((70, 70, 76)) + rng.uniform(-6, 6, (16, 3))
save("barrel_side", bar)
bt = fill((132, 92, 52)) + noise(amp=8)
yy, xx = np.mgrid[0:16, 0:16]
rr = np.sqrt((xx - 7.5) ** 2 + (yy - 7.5) ** 2)
bt[rr > 6.8] = np.array((72, 72, 78))
bt[(rr > 5.6) & (rr <= 6.8)] -= 22
for y in range(16):
    if y % 4 == 0:
        bt[y][rr[y] <= 5.6] -= 14
save("barrel_top", bt)

# Forge : pierre et grille rougeoyante, braises sur le dessus
fs = bricks((112, 108, 106), (70, 68, 68), rows=4, width=8, var=10, bevel=6)
for y in range(6, 12):
    for x in range(4, 12):
        if x in (4, 11) or y in (6, 11):
            fs[y, x] = (52, 48, 46)
        elif x % 2 == 1:
            fs[y, x] = (44, 40, 38)
        else:
            t = (y - 6) / 5.0
            fs[y, x] = np.array((255, 214, 120)) * (1 - t) + np.array((230, 90, 30)) * t
save("forge_side", fs)
ft = fill((40, 34, 32)) + noise(amp=6)
for _ in range(70):
    y, x = rng.integers(1, 15, 2)
    ft[y, x] = rng.choice([(255, 140, 40), (255, 200, 90), (220, 70, 20), (255, 236, 160)])
ft[[0, 15], :] = (96, 92, 90)
ft[:, [0, 15]] = (96, 92, 90)
save("forge_top", ft)

# Laines
def wool(color):
    w = fill(color) + noise(amp=7)
    for y in range(16):
        for x in range(16):
            if (x + 2 * y) % 5 == 0:
                w[y, x] *= 0.9
    return w

save("wool_white", wool((232, 232, 228)))
save("wool_red", wool((170, 44, 40)))
save("wool_yellow", wool((236, 196, 52)))
save("wool_blue", wool((54, 70, 168)))
save("wool_green", wool((88, 124, 40)))

# Cultures
def crop(leaf, root=None, rootRows=0):
    img = np.zeros((16, 16, 4))
    for x in (2, 5, 8, 11, 14):
        h = int(rng.integers(5, 9))
        for y in range(15, 15 - h, -1):
            dx = 0 if y > 12 else int(rng.integers(-1, 2))
            xx = min(15, max(0, x + dx))
            img[y, xx] = (*(np.array(leaf, float) + rng.uniform(-18, 18)), 255)
            if y < 12 and rng.uniform() < 0.5 and xx + 1 < 16:
                img[y, xx + 1] = (*(np.array(leaf, float) * 0.8), 255)
        if root:
            for y in range(16 - rootRows, 16):
                img[y, x] = (*root, 255)
                if x + 1 < 16:
                    img[y, x + 1] = (*(np.array(root) * 0.85), 255)
    return img

save("carrots", crop((70, 140, 40), (232, 120, 30), 2))
save("potatoes", crop((80, 150, 50), (196, 160, 90), 2))
save("beetroots", crop((60, 120, 50), (150, 30, 50), 3))

# Porte en chêne (bas plein, haut avec deux carreaux)
def door(top):
    img = np.zeros((16, 16, 4))
    for y in range(16):
        for x in range(16):
            c = np.array(oak16[y, x], float) * 0.92
            if x in (0, 15) or (not top and y == 15) or (top and y == 0):
                c = c * 0.7
            if (y % 5 == 2) and 1 <= x <= 14:
                c = c * 0.82
            img[y, x] = (*c, 255)
    if top:
        for y in range(3, 9):
            for x in list(range(3, 7)) + list(range(9, 13)):
                img[y, x] = (0, 0, 0, 0)
    else:
        img[8, 12] = (60, 60, 64, 255)
        img[8, 13] = (90, 90, 96, 255)
    return img

save("oak_door_bottom", door(False))
save("oak_door_top", door(True))

gold = fill((236, 196, 64)) + noise(amp=10)
gold[[0, 15], :] = (255, 236, 140)
gold[:, [0, 15]] = (200, 150, 40)
save("gold_block", gold)

ct = oak16.copy()
ct[[0, 15], :] *= 0.75
ct[:, [0, 15]] *= 0.75
for k in (5, 10):
    ct[k, 1:15] = (96, 66, 36)
    ct[1:15, k] = (96, 66, 36)
save("crafting_table_top", ct)
cs = oak16.copy()
cs[0:3] = (110, 76, 42)
for y in range(5, 12):
    cs[y, 3] = (130, 130, 136)
    cs[y, 11] = (96, 70, 40)
cs[5, 2:5] = (160, 160, 166)
cs[5:7, 10:13] = (150, 150, 156)
save("crafting_table_side", cs)
