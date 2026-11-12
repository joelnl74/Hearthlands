# Makes temporary water-life art, drawn in code until painted art replaces it: Project/Assets/Sprites/Animals/ FishShapes
# (16 x 16 cells, a row per fish: its shape in the water, leaping side on, a ripple ring; AnimalsView's order) and the water
# birds Kingfisher, Heron, Osprey, Gull, Mallard, Dipper: the pack's bird sheets recoloured (32 x 32 cells, rows fly left,
# away, toward, right, then the same on the ground), with their .hymeta (handles kept). Pillow only.
#   py scripts/art/make_temp_water_life_art.py [--preview]   (--preview also writes scripts/art/out/water_life_preview.png)
import hashlib
import math
import os
import re
import sys
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Project", "Assets", "Sprites", "Animals")
CELL = 16

# Fish: name, body length and depth (pixels), back, side, belly colours, spots (colour or None), bars (colour or None)
FISH = [
    ("minnow", 7, 2.2, (70, 82, 70), (160, 170, 160), (220, 224, 214), None, None),
    ("trout", 11, 3.0, (64, 78, 48), (118, 132, 84), (226, 200, 168), (196, 64, 52), None),
    ("pike", 14, 2.8, (44, 66, 36), (92, 120, 62), (214, 214, 170), None, (190, 196, 120)),
    ("salmon", 13, 3.2, (60, 74, 92), (176, 184, 196), (236, 196, 190), (40, 44, 56), None),
    ("eel", 15, 1.4, (46, 38, 26), (88, 74, 44), (150, 132, 84), None, None),
    ("carp", 12, 3.8, (112, 82, 32), (184, 142, 58), (232, 206, 130), None, None),
]

# Water birds: sheet name, the pack sheet it is drawn from, its colours from dark to light (by the source's brightness)
BIRDS = [
    ("Kingfisher", "Bluejay", [(18, 30, 64), (22, 86, 156), (40, 156, 208), (228, 116, 46), (250, 196, 128)]),
    ("Heron", "Egret", [(38, 42, 52), (78, 86, 100), (110, 118, 132), (138, 146, 158), (182, 188, 198)]),
    ("Osprey", "Eagle", [(36, 26, 22), (86, 62, 46), (128, 98, 74), (222, 216, 204), (250, 250, 246)]),
    ("Gull", "Egret", [(58, 60, 68), (136, 142, 152), (172, 178, 188), (224, 226, 230), (250, 250, 250)]),
    ("Mallard", "Robin", [(18, 52, 32), (36, 112, 62), (118, 88, 58), (168, 148, 118), (222, 216, 198)]),
    ("Dipper", "Sparrow", [(24, 20, 18), (58, 44, 36), (94, 72, 58), (148, 120, 100), (246, 242, 234)]),
]
BIRD_ROWS = ["fly_left", "fly_away", "fly_toward", "fly_right", "ground_left", "ground_away", "ground_toward", "ground_right"]


def blend(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def body_half(t, depth):
    """Half the body's depth at t (0 the tail's root, 1 the snout): fullest just behind the head."""
    if t < 0.0 or t > 1.0:
        return -1.0
    return depth * 0.5 * (math.sin(math.pi * t) ** 0.6) * (0.55 + 0.45 * min(1.0, t * 1.6))


def fish_shape(length, depth, back, side, spots, bars, alpha):
    """Seen from above, pointing right: a dark back fading to the side colour, a forked tail."""
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    pixels = image.load()
    cy = CELL / 2.0
    root = (CELL - length) / 2.0 + 2.0  # where the tail joins
    span = length - 2.0
    for y in range(CELL):
        for x in range(CELL):
            t = (x + 0.5 - root) / span
            half = body_half(t, depth * 0.9)
            d = abs(y + 0.5 - cy)
            if half < 0.0 or d > half + 0.35:
                continue
            colour = blend(back, side, min(1.0, d / max(half + 0.35, 0.5)))
            if bars and int(x - root) % 3 == 0 and 0.2 < t < 0.85:
                colour = blend(colour, bars, 0.35)
            if spots and (x * 7 + y * 3) % 5 == 0 and 0.2 < t < 0.8:
                colour = blend(colour, spots, 0.5)
            pixels[x, y] = colour + (alpha,)
    tail(pixels, int(root) + 1, cy, back, alpha)
    return image


def tail(pixels, x0, cy, colour, alpha):
    """A forked tail spreading back from (x0, cy)."""
    for i in range(3):
        for sign in (-1, 1):
            x = x0 - i
            y = int(math.floor(cy + sign * (0.5 + i * 0.8)))
            if 0 <= x < CELL and 0 <= y < CELL:
                pixels[x, y] = colour + (alpha,)
        if 0 <= x0 - i < CELL and i == 0:
            pixels[x0, int(cy)] = colour + (alpha,)
            pixels[x0, int(cy) - 1] = colour + (alpha,)


def fish_leap(length, depth, back, side, belly, spots, bars):
    """Side on, pointing right, as it leaps: back on top, light belly, a tail fin, an eye."""
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    pixels = image.load()
    cy = CELL / 2.0
    root = (CELL - length) / 2.0 + 2.0
    span = length - 2.0
    for y in range(CELL):
        for x in range(CELL):
            t = (x + 0.5 - root) / span
            half = body_half(t, depth * 1.25)
            dy = y + 0.5 - cy
            if half < 0.0 or abs(dy) > half + 0.35:
                continue
            share = (dy + half) / max(2 * half, 0.5)  # 0 on top, 1 at the belly
            colour = blend(back, side, min(1.0, share * 1.6)) if share < 0.6 else blend(side, belly, (share - 0.6) / 0.4)
            if bars and int(x - root) % 3 == 0 and 0.15 < t < 0.85 and share < 0.7:
                colour = blend(colour, bars, 0.4)
            if spots and (x * 5 + y * 3) % 4 == 0 and 0.15 < t < 0.8 and share < 0.6:
                colour = blend(colour, spots, 0.6)
            pixels[x, y] = colour + (255,)
    tail(pixels, int(root) + 1, cy, back, 255)
    eye = int(root + span * 0.85)
    if 0 <= eye < CELL:
        pixels[eye, int(cy - 0.5)] = (16, 16, 20, 255)
    return image


def ripple():
    """A thin ring on the water, brighter on its near side."""
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    pixels = image.load()
    c = CELL / 2.0 - 0.5
    for y in range(CELL):
        for x in range(CELL):
            d = math.hypot(x - c, (y - c) * 1.0)
            if abs(d - 6.0) < 0.6:
                near = 0.6 + 0.4 * (y / CELL)
                pixels[x, y] = (236, 244, 250, int(200 * near))
            elif abs(d - 3.5) < 0.5:
                pixels[x, y] = (236, 244, 250, 90)
    return image


def fish_sheet():
    sheet = Image.new("RGBA", (CELL * 3, CELL * len(FISH)), (0, 0, 0, 0))
    ring = ripple()
    for row, (name, length, depth, back, side, belly, spots, bars) in enumerate(FISH):
        sheet.alpha_composite(fish_shape(length, depth, back, side, spots, bars, 210), (0, row * CELL))
        sheet.alpha_composite(fish_leap(length, depth, back, side, belly, spots, bars), (CELL, row * CELL))
        sheet.alpha_composite(ring, (CELL * 2, row * CELL))
    return sheet


def recolour(source, ramp):
    """The source sheet with each opaque pixel's colour taken from the ramp by how bright it is among the sheet's pixels."""
    image = source.convert("RGBA").copy()
    pixels = image.load()
    seen = set()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a >= 128:
                seen.add(int(0.3 * r + 0.59 * g + 0.11 * b))
    levels = sorted(seen)
    if not levels:
        return image
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a < 128:
                continue
            level = int(0.3 * r + 0.59 * g + 0.11 * b)
            rank = levels.index(level) / max(len(levels) - 1, 1)
            position = rank * (len(ramp) - 1)
            low = int(position)
            high = min(low + 1, len(ramp) - 1)
            pixels[x, y] = blend(ramp[low], ramp[high], position - low) + (255,)
    return image


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/animals/" + name).encode()).hexdigest()[:15], 16)


def write(image, name, cell, columns, rows, names):
    path = os.path.join(OUT, name + ".png")
    image.save(path)
    meta = path + ".hymeta"
    handle = handle_for(meta, name)
    lines = [f"Handle: {handle}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest", "  Wrap: Clamp",
             "  SpriteMode: Multiple", "  PixelsPerUnit: 32", "  Slicing:", "    Mode: Grid", f"    CellSize: [{cell}, {cell}]",
             "    Offset: [0, 0]", "    Spacing: [0, 0]", f"    Columns: {columns}", f"    Rows: {rows}",
             "    RowNames: [" + ", ".join(names) + "]", "    DefaultPivot: [0.5, 0.5]", "  Sprites: []"]
    with open(meta, "w", encoding="utf-8", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")
    print(f"{name}: {image.width} x {image.height}, handle {handle}")
    return image


def main():
    os.makedirs(OUT, exist_ok=True)
    made = [write(fish_sheet(), "FishShapes", CELL, 3, len(FISH), [f[0] for f in FISH])]
    for name, base, ramp in BIRDS:
        source = Image.open(os.path.join(OUT, base + ".png"))
        made.append(write(recolour(source, ramp), name, 32, source.width // 32, source.height // 32, BIRD_ROWS))
    if "--preview" in sys.argv:
        width = sum(image.width for image in made) + 8 * len(made)
        preview = Image.new("RGBA", (width, max(image.height for image in made)), (60, 110, 150, 255))
        x = 0
        for image in made:
            preview.alpha_composite(image, (x, 0))
            x += image.width + 8
        out = os.path.join(ROOT, "scripts", "art", "out")
        os.makedirs(out, exist_ok=True)
        preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(os.path.join(out, "water_life_preview.png"))


if __name__ == "__main__":
    main()
