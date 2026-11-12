# Makes temporary art for the old woods until painted art replaces it: Project/Assets/Sprites/Objects/AncientOak.png and
# OldPine.png, the grown Oak and Pine sprites made larger (1.4x), darker and older: moss and lichen on them, vines hanging
# from the oak, with their .hymeta (handles kept). Pillow only.
#   py scripts/art/make_temp_oldwoods_art.py [--preview]   (--preview also writes scripts/art/out/oldwoods_preview.png)
import hashlib
import os
import random
import re
import sys
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OBJECTS = os.path.join(ROOT, "Project", "Assets", "Sprites", "Objects")

MOSS = (92, 132, 64)
MOSS_LIGHT = (132, 168, 88)
LICHEN = (170, 182, 150)
VINE = (52, 92, 46)


def older(source, scale, darken, seed, vines):
    """The source sprite scaled up (nearest), darker towards blue-green, speckled with moss (and lichen), with vines."""
    rng = random.Random(seed)
    width, height = round(source.width * scale), round(source.height * scale)
    image = source.resize((width, height), Image.NEAREST)
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            r, g, b = int(r * darken), int(g * darken * 1.02), int(b * darken * 1.08)
            dark = r + g + b < 260
            roll = rng.random()
            if dark and roll < 0.10:
                r, g, b = MOSS if roll < 0.06 else MOSS_LIGHT  # moss on the bark and in the shade
            elif roll < 0.025:
                r, g, b = LICHEN
            pixels[x, y] = (min(255, r), min(255, g), min(255, b), a)
    if vines:
        # Vines hang from the canopy's underside: where a column's leaves end, a few pixels drop down.
        for x in range(2, width - 2, 3):
            if rng.random() < 0.45:
                continue
            bottom = -1
            for y in range(height // 2, 0, -1):
                if pixels[x, y][3] != 0:
                    bottom = y
                    break
            if bottom < 0 or pixels[x, min(height - 1, bottom + 1)][3] != 0:
                continue
            for y in range(bottom + 1, min(height - 4, bottom + 2 + rng.randint(2, 6))):
                pixels[x, y] = VINE + (255,)
    return image


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/oldwoods/" + name).encode()).hexdigest()[:15], 16)


def write(image, name):
    path = os.path.join(OBJECTS, name + ".png")
    image.save(path)
    meta = path + ".hymeta"
    lines = [f"Handle: {handle_for(meta, name)}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest",
             "  Wrap: Clamp", "  SpriteMode: Single", "  PixelsPerUnit: 24"]
    with open(meta, "w", encoding="utf-8", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")
    print(f"{name}: {image.width} x {image.height}, handle {handle_for(meta, name)}")


def main():
    oak = older(Image.open(os.path.join(OBJECTS, "Oak.png")).convert("RGBA"), 1.4, 0.78, 7, True)
    pine = older(Image.open(os.path.join(OBJECTS, "Pine.png")).convert("RGBA"), 1.35, 0.74, 11, False)
    write(oak, "AncientOak")
    write(pine, "OldPine")
    if "--preview" in sys.argv:
        preview = Image.new("RGBA", (oak.width + pine.width + 24, max(oak.height, pine.height) + 8), (120, 150, 90, 255))
        preview.alpha_composite(oak, (4, 4))
        preview.alpha_composite(pine, (oak.width + 16, 4))
        out = os.path.join(ROOT, "scripts", "art", "out")
        os.makedirs(out, exist_ok=True)
        preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(os.path.join(out, "oldwoods_preview.png"))


if __name__ == "__main__":
    main()
