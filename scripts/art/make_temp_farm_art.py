# Makes temporary farming art, drawn in code until painted art replaces it: the farmland terrain textures in
# Art/Terrain/Temp (farmland_01, farmland_02, farmland_wet_01: 32 x 32, wrapping; make_terrain_atlas.py puts them in
# row 7) and the crop objects in Project/Assets/Sprites/Farm (CropSown, CropGrowing, CropRipe, 32 px a tile) with their
# .hymeta (handles kept). Pillow only.
#   py scripts/art/make_temp_farm_art.py [--preview]   (--preview also writes scripts/art/out/farm_preview.png)
import hashlib
import os
import random
import re
import sys
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TERRAIN = os.path.join(ROOT, "Art", "Terrain", "Temp")
SPRITES = os.path.join(ROOT, "Project", "Assets", "Sprites", "Farm")

OUTLINE = (34, 24, 20, 255)
SOIL = [(74, 52, 34), (92, 64, 42), (110, 78, 50), (128, 92, 60)]       # dark trough to lit ridge
WET_SOIL = [(54, 38, 26), (68, 48, 32), (82, 58, 38), (96, 68, 44)]
SPROUT = (120, 172, 72, 255)
SPROUT_DARK = (84, 132, 56, 255)
STALK = (96, 150, 60, 255)
STALK_LIGHT = (140, 190, 84, 255)
STALK_DARK = (66, 112, 46, 255)
GRAIN = (226, 186, 88, 255)
GRAIN_LIGHT = (246, 216, 124, 255)
GRAIN_DARK = (184, 140, 58, 255)


def furrows(palette, seed, straw):
    """Rows of ridges and troughs, 4 texels apart, with a little grit; wraps both ways."""
    rng = random.Random(seed)
    image = Image.new("RGBA", (32, 32))
    for y in range(32):
        phase = y % 4  # 0 trough, 1 rising, 2 ridge, 3 falling
        shade = [0, 1, 3, 2][phase]
        for x in range(32):
            level = shade
            if rng.random() < 0.12:
                level = max(0, min(3, level + rng.choice((-1, 1))))
            image.putpixel((x, y), palette[level] + (255,))
    for _ in range(straw):
        x, y = rng.randrange(32), rng.randrange(8) * 4 + 2
        image.putpixel((x, y), (168, 140, 92, 255))
        image.putpixel(((x + 1) % 32, y), (150, 124, 80, 255))
    return image


def put(image, x, y, color):
    if 0 <= x < image.width and 0 <= y < image.height:
        image.putpixel((x, y), color)


def outline(image):
    pixels = image.load()
    edge = []
    for y in range(image.height):
        for x in range(image.width):
            if pixels[x, y][3] != 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < image.width and 0 <= ny < image.height and pixels[nx, ny][3] != 0:
                    edge.append((x, y))
                    break
    for x, y in edge:
        pixels[x, y] = OUTLINE
    return image


# The crop sits on its tile: plants in three rows across the tile's lower half (the tile's diamond spans the width).
ROWS = [(8, 22), (12, 25), (16, 28)]  # (first x, base y) per row


def plant_rows(draw_plant):
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for row, (x0, base) in enumerate(ROWS):
        for i in range(4 - (row == 2)):
            draw_plant(image, x0 + i * 5 - row, base)
    return image


def sown(image, x, base):
    put(image, x, base, SPROUT_DARK)
    put(image, x, base - 1, SPROUT)
    put(image, x + 1, base - 2, SPROUT)


def growing(image, x, base):
    for y in range(base - 6, base + 1):
        put(image, x, y, STALK if y > base - 4 else STALK_LIGHT)
    put(image, x - 1, base - 3, STALK_DARK)
    put(image, x + 1, base - 5, STALK_LIGHT)
    put(image, x - 1, base - 6, STALK)


def ripe(image, x, base):
    for y in range(base - 8, base + 1):
        put(image, x, y, GRAIN_DARK if y > base - 3 else GRAIN)
    for y in range(base - 12, base - 7):  # the ear
        put(image, x, y, GRAIN_LIGHT if (y + x) % 2 else GRAIN)
        put(image, x + 1, y, GRAIN if y % 2 else GRAIN_DARK)
    put(image, x - 1, base - 4, GRAIN_DARK)


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/farm/" + name).encode()).hexdigest()[:15], 16)


def write_sprite(image, name):
    os.makedirs(SPRITES, exist_ok=True)
    path = os.path.join(SPRITES, name + ".png")
    image.save(path)
    meta = path + ".hymeta"
    lines = [f"Handle: {handle_for(meta, name)}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest",
             "  Wrap: Clamp"]
    with open(meta, "w", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")
    print("wrote", path, handle_for(meta, name))


def main():
    os.makedirs(TERRAIN, exist_ok=True)
    textures = {"farmland_01": furrows(SOIL, 1, 3), "farmland_02": furrows(SOIL, 2, 8), "farmland_wet_01": furrows(WET_SOIL, 3, 2)}
    for name, image in textures.items():
        image.save(os.path.join(TERRAIN, name + ".png"))
        print("wrote", os.path.join(TERRAIN, name + ".png"))
    crops = {"CropSown": outline(plant_rows(sown)), "CropGrowing": outline(plant_rows(growing)), "CropRipe": outline(plant_rows(ripe))}
    for name, image in crops.items():
        write_sprite(image, name)
    if "--preview" in sys.argv:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
        os.makedirs(out, exist_ok=True)
        preview = Image.new("RGBA", (32 * 6, 32), (0, 0, 0, 0))
        for i, image in enumerate(textures.values()):
            preview.paste(image, (i * 32, 0))
        for i, image in enumerate(crops.values()):
            ground = textures["farmland_01"].copy()
            ground.alpha_composite(image)
            preview.paste(ground, (96 + i * 32, 0))
        preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(os.path.join(out, "farm_preview.png"))


if __name__ == "__main__":
    main()
