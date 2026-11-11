# Makes temporary terrain art for raised ground and rivers in Art/Terrain/Temp, from the Hearthlands palette and the
# textures in Art/Terrain, until PixelLab versions replace them. Every piece wraps horizontally; tiles also wrap vertically.
#   py scripts/art/make_temp_terrain_art.py [--preview]   (--preview also writes Art/Terrain/Temp/preview.png)
#
#   cliff_<type>_01.png  32 x 32  side face of a raised step: lit top, strata, dark foot (the shader stretches it per step)
#   lip_<type>_01.png    32 x 8   transparent overhang along a step's top edge (grass blades, snow crust)
#   foam_01..04.png      32 x 8   transparent shore foam along the water's edge, 4 frames for a slow loop
#   water_shallow_01.png 32 x 32  lighter water near the shore; water_deep_01.png darker water away from it
#   bank_wet_01.png      32 x 32  darkened, damp ground beside water
#   rock_01.png, rock_02.png, rock_cracked_01.png  32 x 32  rock ground (the dirt textures in stone greys)
import os
import random
import sys
from PIL import Image

W = 32
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SOURCE = os.path.join(ROOT, "Art", "Terrain")
OUT = os.path.join(SOURCE, "Temp")


def hexes(text):
    return [tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in text.split()]


GRASS = hexes("2F5E2A 3E7A33 508D3E 66A64A 86C25C")
DIRT = hexes("4E3424 62432D 7A5639 94704C AD8A62")
SAND = hexes("B89A5E CDB073 DEC784 EBD9A4")
WATER = hexes("22406F 2D4F8A 345AA2 4A78C2 8DB4E6")
MUD = hexes("3A2A1F 4A3628 564030 6B5442")
SNOW = hexes("B9C8DC D6E1EE E9EFF6 FFFFFF")
STONE = hexes("5E5E62 7E7E84 A2A2A8")
OUTLINE = (42, 30, 22)


def new(height, color=None):
    image = Image.new("RGBA", (W, height), (0, 0, 0, 0) if color is None else color + (255,))
    return image


def wave(rng, height, octaves=3):
    # A height per column that wraps around the 32 columns: sums of whole-period sines with random phases.
    import math
    phases = [rng.random() * 6.283 for _ in range(octaves)]
    return [sum(math.sin((x / W) * 6.283 * (o + 1) + phases[o]) / (o + 1) for o in range(octaves)) * height for x in range(W)]


def cliff(rng, ramp, stones=None, roots=False):
    # ramp: dark to light. Light at the top, strata bands, scattered stones, a dark foot.
    image = new(W)
    dark, mid, light = ramp[0], ramp[len(ramp) // 2], ramp[-1]
    bands = [wave(rng, 1.5) for _ in range(4)]
    for y in range(W):
        for x in range(W):
            shade = 1 if y < 6 else 0 if y > 24 else (1 if rng.random() < 0.55 else 0)
            color = ramp[min(len(ramp) - 1, 1 + shade)] if len(ramp) > 2 else mid
            if y >= 28:
                color = dark
            image.putpixel((x, y), color + (255,))
    for index, band in enumerate(bands):  # strata: a dark line with a light line above it
        row = 7 + index * 5
        for x in range(W):
            y = int(round(row + band[x]))
            if 0 < y < 27:
                image.putpixel((x, y), dark + (255,))
                if rng.random() < 0.6:
                    image.putpixel((x, y - 1), light + (255,))
    for _ in range(26):  # grain
        x, y = rng.randrange(W), rng.randrange(2, 27)
        image.putpixel((x, y), rng.choice(ramp[:3]) + (255,))
    if stones:
        for _ in range(5):
            x, y = rng.randrange(W), rng.randrange(8, 24)
            for dx in range(3):
                image.putpixel(((x + dx) % W, y), stones[1] + (255,))
                image.putpixel(((x + dx) % W, y + 1), stones[0] + (255,))
            image.putpixel(((x + 1) % W, y - 1), stones[2] + (255,))
    if roots:
        for _ in range(5):  # thin roots hanging from the top
            x = rng.randrange(W)
            for y in range(rng.randrange(4, 10)):
                x = (x + rng.choice([0, 0, 1, -1])) % W
                image.putpixel((x, y), OUTLINE + (255,))
    for x in range(W):  # the top edge catches the light
        image.putpixel((x, 0), light + (255,))
    return image


def lip(rng, ramp, drip):
    # A transparent strip: solid along the top, hanging tufts below, a dark outline under the hang.
    image = new(8)
    hang = wave(rng, drip * 0.5)
    for x in range(W):
        depth = int(round(2 + drip * 0.5 + hang[x] + (rng.random() < 0.3)))
        depth = max(2, min(7, depth))
        for y in range(depth):
            color = ramp[-2] if y == 0 else ramp[-3] if y < depth - 1 else ramp[0]
            if y > 0 and rng.random() < 0.2:
                color = ramp[-1]
            image.putpixel((x, y), color + (255,))
        if depth < 8:
            image.putpixel((x, depth), OUTLINE + (200,))
    return image


def foam(frame):
    # Bubbles thinning out from the shore (the top row) into the water, shifted per frame so 4 frames loop.
    rng = random.Random(70)
    image = new(8)
    line = wave(random.Random(71), 1.0)
    for x in range(W):
        edge = int(round(1 + line[(x + frame * 2) % W]))
        for y in range(max(0, edge)):
            image.putpixel((x, y), SNOW[2] + (255,))
    for _ in range(46):
        x, y = rng.randrange(W), int(rng.random() ** 1.7 * 8)
        x = (x + frame * (1 + y // 3)) % W
        image.putpixel((x, y), (SNOW[3] if y < 3 else WATER[4]) + (255 if y < 5 else 170,))
    return image


def remap(name, shift, ramps):
    # Moves every colour of a source tile `shift` steps along whichever ramp holds it.
    image = Image.open(os.path.join(SOURCE, name)).convert("RGBA")
    out = image.copy()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = image.getpixel((x, y))
            for ramp in ramps:
                if (r, g, b) in ramp:
                    i = max(0, min(len(ramp) - 1, ramp.index((r, g, b)) + shift))
                    out.putpixel((x, y), ramp[i] + (a,))
                    break
    return out


def to_stone(name):
    # The dirt textures' browns as stone greys: the main colour mid grey, darker browns dark, lighter ones light.
    image = Image.open(os.path.join(SOURCE, name)).convert("RGBA")
    light = (194, 194, 200)
    table = {MUD[0]: STONE[0], MUD[1]: STONE[1], DIRT[0]: STONE[1], MUD[2]: STONE[2], DIRT[1]: STONE[2], MUD[3]: STONE[2],
             DIRT[3]: STONE[2], STONE[0]: STONE[2], STONE[1]: light}
    out = image.copy()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = image.getpixel((x, y))
            out.putpixel((x, y), table.get((r, g, b), (r, g, b)) + (a,))
    return out


def sparkle(image, rng, count, colors):
    for _ in range(count):
        image.putpixel((rng.randrange(W), rng.randrange(W)), rng.choice(colors) + (255,))
    return image


def make():
    pieces = {
        "cliff_dirt_01": cliff(random.Random(1), DIRT, STONE, roots=True),
        "cliff_stone_01": cliff(random.Random(2), STONE + [(194, 194, 200)]),
        "cliff_sand_01": cliff(random.Random(3), SAND),
        "cliff_snow_01": cliff(random.Random(4), SNOW, STONE),
        "cliff_mud_01": cliff(random.Random(5), MUD, STONE),
        "lip_grass_01": lip(random.Random(6), GRASS, 3),
        "lip_snow_01": lip(random.Random(7), SNOW, 2),
        "lip_sand_01": lip(random.Random(8), SAND, 1),
        "water_shallow_01": sparkle(remap("water_01.png", 1, [WATER]), random.Random(9), 6, [WATER[4]]),
        "water_deep_01": remap("water_01.png", -1, [WATER]),
        "bank_wet_01": sparkle(remap("dirt_01.png", -1, [MUD, DIRT, STONE]), random.Random(10), 8, [MUD[3]]),
        "rock_01": to_stone("dirt_01.png"),
        "rock_02": to_stone("dirt_02.png"),
        "rock_cracked_01": to_stone("dirt_cracked_01.png"),
    }
    for frame in range(4):
        pieces["foam_%02d" % (frame + 1)] = foam(frame)
    os.makedirs(OUT, exist_ok=True)
    for name, image in pieces.items():
        image.save(os.path.join(OUT, name + ".png"))
    return pieces


def tile(image, columns, rows):
    out = Image.new("RGBA", (image.width * columns, image.height * rows))
    for row in range(rows):
        for column in range(columns):
            out.alpha_composite(image, (column * image.width, row * image.height))
    return out


def preview(pieces):
    # A flat mock-up, top to bottom: a grass plateau, its lip and 1.5 tiles of dirt cliff, dirt ground, a wet bank,
    # foam, shallow water, deep water. 6 tiles wide, 3x scale.
    source = lambda name: Image.open(os.path.join(SOURCE, name)).convert("RGBA")
    columns = 6
    strip = [tile(source("grass_plain_01.png"), columns, 2), tile(pieces["cliff_dirt_01"], columns, 1),
             tile(pieces["cliff_dirt_01"].crop((0, 16, W, W)), columns, 1), tile(source("dirt_01.png"), columns, 1),
             tile(pieces["bank_wet_01"].crop((0, 0, W, 16)), columns, 1), tile(pieces["water_shallow_01"], columns, 1),
             tile(pieces["water_deep_01"], columns, 1)]
    height = sum(s.height for s in strip)
    out = Image.new("RGBA", (W * columns, height))
    y = 0
    offsets = []
    for s in strip:
        offsets.append(y)
        out.alpha_composite(s, (0, y))
        y += s.height
    out.alpha_composite(tile(pieces["lip_grass_01"], columns, 1), (0, offsets[1]))
    out.alpha_composite(tile(pieces["foam_01"], columns, 1), (0, offsets[5]))
    out = out.resize((out.width * 3, out.height * 3), Image.NEAREST)
    out.save(os.path.join(OUT, "preview.png"))


def main():
    pieces = make()
    print("wrote", len(pieces), "pieces to", OUT)
    if "--preview" in sys.argv:
        preview(pieces)


if __name__ == "__main__":
    main()
