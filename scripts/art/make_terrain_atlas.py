# Makes Project/Assets/Textures/TerrainAtlas.png: tile_blend.frag's atlas, one 16 x 16 pixel-art tile per terrain id in id
# order (0 sand, 1 dirt, 2 grass, 3 water, 4 snow). Every tile wraps, since the shader repeats it every tile. Pillow only.
#   py scripts/art/make_terrain_atlas.py [--preview]   (--preview also writes scripts/art/out/atlas_preview.png)
import os
import random
import sys
from PIL import Image

S = 16
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def fill(color):
    return [[color for _ in range(S)] for _ in range(S)]


def put(tile, x, y, color):
    tile[y % S][x % S] = color


def speckle(tile, rng, count, colors):
    for _ in range(count):
        put(tile, rng.randrange(S), rng.randrange(S), rng.choice(colors))


def sand(rng):
    tile = fill((222, 199, 132))
    speckle(tile, rng, 22, [(208, 184, 114)])
    speckle(tile, rng, 14, [(234, 214, 156)])
    for _ in range(4):  # small ripples
        x, y = rng.randrange(S), rng.randrange(S)
        for i in range(3):
            put(tile, x + i, y, (212, 188, 120))
        put(tile, x + 1, y - 1, (236, 218, 162))
    return tile


def dirt(rng):
    tile = fill((122, 86, 57))
    speckle(tile, rng, 30, [(104, 72, 47), (110, 77, 50)])
    speckle(tile, rng, 14, [(138, 100, 67)])
    for _ in range(5):  # pebbles: a light top over a dark shadow
        x, y = rng.randrange(S), rng.randrange(S)
        put(tile, x, y, (164, 134, 100))
        put(tile, x + 1, y, (150, 120, 88))
        put(tile, x, y + 1, (92, 63, 41))
        put(tile, x + 1, y + 1, (92, 63, 41))
    return tile


def grass(rng):
    tile = fill((80, 141, 62))
    speckle(tile, rng, 24, [(72, 130, 56)])
    speckle(tile, rng, 10, [(92, 154, 70)])
    for _ in range(14):  # blades: two dark pixels with a light tip
        x, y = rng.randrange(S), rng.randrange(S)
        put(tile, x, y, (62, 114, 50))
        put(tile, x, y + 1, (62, 114, 50))
        put(tile, x, y - 1, (106, 168, 78))
    return tile


def water(rng):
    tile = fill((52, 90, 162))
    speckle(tile, rng, 18, [(46, 81, 150)])
    rows = [1, 5, 9, 13]
    for row in rows:  # wave crests: a light dash over a darker trough
        x = rng.randrange(S)
        length = rng.choice([3, 4])
        for i in range(length):
            put(tile, x + i, row, (92, 136, 204))
            put(tile, x + i, row + 1, (44, 77, 144))
        put(tile, x + 1, row, (128, 168, 224))
    return tile


def snow(rng):
    tile = fill((233, 239, 246))
    speckle(tile, rng, 26, [(214, 225, 240)])
    speckle(tile, rng, 10, [(196, 210, 232)])
    for _ in range(5):  # sparkles
        put(tile, rng.randrange(S), rng.randrange(S), (255, 255, 255))
    return tile


TILES = [sand, dirt, grass, water, snow]  # in tile id order


def make_atlas(seed=11):
    atlas = Image.new("RGBA", (S * len(TILES), S))
    for index, make in enumerate(TILES):
        tile = make(random.Random(seed * 101 + index))
        for y in range(S):
            for x in range(S):
                atlas.putpixel((index * S + x, y), tile[y][x] + (255,))
    return atlas


def main():
    atlas = make_atlas()
    path = os.path.join(ROOT, "Project", "Assets", "Textures", "TerrainAtlas.png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    atlas.save(path)
    print("wrote", path)
    if "--preview" in sys.argv:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
        os.makedirs(out, exist_ok=True)
        atlas.resize((atlas.width * 8, atlas.height * 8), Image.NEAREST).save(os.path.join(out, "atlas_preview.png"))


if __name__ == "__main__":
    main()
