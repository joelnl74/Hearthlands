# Makes temporary building art, drawn in code until painted art replaces it: the food stores, hunting lodges and the barn
# of the building system (Buildings.hytable) in Project/Assets/Sprites/Buildings (32 px a tile) with their .hymeta (handles
# kept). Granary, HuntingLodge, Barn, FishingLodge (humans), LarderBower, HuntersBlind (elves, also wood elves), SeedHollow
# (wood elves), ColdCellar, StoneLodge, FishersHall (dwarves). Pillow only; the palette and helpers are
# make_temp_settler_art.py's.
#   py scripts/art/make_temp_building_art.py [--preview]   (--preview also writes scripts/art/out/buildings_preview.png)
import hashlib
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_temp_settler_art import (BARK, DOOR, FLAME, FLAME_HOT, GLOW, GLOW_DIM, LEAF, LEAF_DARK, LEAF_LIGHT, MOSS, ORE,  # noqa: E402
                                   ROCK, ROCK_DARK, ROCK_LIGHT, STONE, STONE_DARK, STONE_LIGHT, TIMBER, TIMBER_DARK, outline, put,
                                   rect)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Project", "Assets", "Sprites", "Buildings")

THATCH, THATCH_DARK, THATCH_LIGHT = (196, 160, 82, 255), (150, 118, 56, 255), (222, 190, 112, 255)
SACK, SACK_DARK = (214, 196, 150, 255), (170, 150, 108, 255)
HIDE, HIDE_DARK = (176, 132, 92, 255), (132, 96, 64, 255)
ANTLER = (232, 220, 190, 255)
BARN, BARN_DARK, BARN_LIGHT = (150, 70, 52, 255), (112, 50, 40, 255), (178, 92, 66, 255)
FRUIT, FRUIT_LIGHT = (210, 72, 66, 255), (240, 120, 96, 255)
ICE, ICE_LIGHT = (170, 214, 236, 255), (220, 242, 252, 255)
NET, NET_DARK = (200, 190, 160, 255), (150, 140, 116, 255)
FISH, FISH_LIGHT = (150, 170, 184, 255), (200, 214, 222, 255)
WATER, WATER_LIGHT = (70, 120, 160, 255), (120, 170, 200, 255)


def roof(image, top, bottom, centre, half_top, half_bottom, light, mid, dark):
    """A pitched roof from `top` to `bottom`, lit on the left, shaded on the right."""
    for y in range(top, bottom + 1):
        t = (y - top) / max(1, bottom - top)
        half = round(half_top + (half_bottom - half_top) * t)
        for x in range(centre - half, centre + half + 1):
            put(image, x, y, light if x < centre - half // 3 else (dark if x > centre + half // 2 else mid))


def granary():
    """The humans' food store: a timber store on stone feet (dry from the ground), sacks at the door."""
    image = Image.new("RGBA", (32, 40), (0, 0, 0, 0))
    for x in (8, 15, 22):  # the stone feet
        rect(image, x, 34, x + 2, 37, STONE_DARK)
    rect(image, 6, 20, 26, 33, TIMBER)
    for x in range(7, 26, 3):  # upright planks
        rect(image, x, 20, x, 33, TIMBER_DARK)
    rect(image, 23, 20, 26, 33, TIMBER_DARK)
    roof(image, 6, 21, 16, 2, 13, THATCH_LIGHT, THATCH, THATCH_DARK)
    rect(image, 13, 25, 18, 33, DOOR)
    for x in (4, 20):  # sacks
        rect(image, x, 35, x + 4, 38, SACK)
        rect(image, x + 3, 35, x + 4, 38, SACK_DARK)
        put(image, x + 2, 34, SACK_DARK)
    return outline(image)


def hunting_lodge():
    """The humans' hunting lodge: a low log lodge, antlers over the door, a hide on a drying rack."""
    image = Image.new("RGBA", (36, 36), (0, 0, 0, 0))
    rect(image, 6, 20, 26, 33, TIMBER)
    for y in range(21, 34, 3):
        rect(image, 6, y, 26, y, TIMBER_DARK)
    rect(image, 23, 20, 26, 33, TIMBER_DARK)
    roof(image, 10, 21, 16, 4, 13, THATCH_LIGHT, THATCH, THATCH_DARK)
    rect(image, 13, 26, 18, 33, DOOR)
    for x, step in ((12, -1), (19, 1)):  # antlers
        put(image, x, 23, ANTLER)
        put(image, x + step, 22, ANTLER)
        put(image, x + 2 * step, 21, ANTLER)
        put(image, x + step, 20, ANTLER)
    rect(image, 28, 22, 28, 33, BARK)  # the rack and its hide
    rect(image, 34, 22, 34, 33, BARK)
    rect(image, 28, 22, 34, 22, BARK)
    rect(image, 29, 23, 33, 30, HIDE)
    rect(image, 32, 23, 33, 30, HIDE_DARK)
    return outline(image)


def barn():
    """The humans' barn: a big plank barn with a wide double door and a hay loft."""
    image = Image.new("RGBA", (40, 40), (0, 0, 0, 0))
    rect(image, 5, 18, 34, 37, BARN)
    for x in range(6, 34, 3):
        rect(image, x, 18, x, 37, BARN_DARK)
    rect(image, 29, 18, 34, 37, BARN_DARK)
    roof(image, 4, 19, 20, 3, 17, (160, 150, 140, 255), (128, 118, 110, 255), (96, 88, 84, 255))
    rect(image, 14, 26, 25, 37, TIMBER_DARK)  # the doors
    rect(image, 15, 27, 19, 37, TIMBER)
    rect(image, 20, 27, 24, 37, TIMBER)
    for i in range(5):  # their cross braces
        put(image, 15 + i, 27 + i * 2, TIMBER_DARK)
        put(image, 24 - i, 27 + i * 2, TIMBER_DARK)
    rect(image, 18, 14, 21, 18, THATCH)  # hay in the loft door
    return outline(image)


def larder_bower():
    """The elves' food store: a woven dome of boughs with baskets of fruit at its mouth."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(10, 30):
        t = (y - 10) / 19
        half = round(4 + 10 * min(1.0, t * 1.6))
        for x in range(16 - half, 16 + half + 1):
            weave = (x + y) % 4 == 0
            put(image, x, y, LEAF_LIGHT if x < 12 else (LEAF_DARK if x > 21 or weave else LEAF))
    rect(image, 13, 22, 19, 29, BARK)  # the mouth
    for x in (5, 22):  # baskets
        rect(image, x, 26, x + 4, 29, TIMBER)
        put(image, x + 1, 25, FRUIT)
        put(image, x + 2, 25, FRUIT_LIGHT)
        put(image, x + 3, 25, FRUIT)
    return outline(image)


def hunters_blind():
    """The elves' hunting place: a screen of woven leaves on poles, a bow and quiver hung on it."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for x in (6, 25):  # poles
        rect(image, x, 8, x, 29, BARK)
    for y in range(11, 27):
        for x in range(7, 25):
            put(image, x, y, LEAF_LIGHT if x < 11 else (LEAF_DARK if (x + y) % 5 == 0 or x > 21 else LEAF))
    for y in range(13, 24):  # the bow
        put(image, 12 + abs(y - 18) // 3, y, TIMBER_DARK)
    rect(image, 18, 14, 19, 22, HIDE)  # the quiver
    put(image, 18, 13, ANTLER)
    put(image, 19, 12, ANTLER)
    return outline(image)


def seed_hollow():
    """The wood elves' food store: a hollow old stump with glowing seeds and nuts in its heart."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(12, 30):
        half = 9 + (y - 12) // 6
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, BARK if x < 22 else (70, 52, 36, 255))
    for x in range(8, 25, 3):  # bark ridges
        rect(image, x, 14, x, 29, (70, 52, 36, 255))
    rect(image, 7, 11, 25, 12, MOSS)
    rect(image, 12, 18, 20, 27, (40, 30, 22, 255))  # the hollow
    for x, y in ((13, 25), (15, 24), (17, 25), (19, 24), (14, 22), (18, 22)):
        put(image, x, y, GLOW if (x + y) % 2 else GLOW_DIM)
    return outline(image)


def cold_cellar():
    """The dwarves' food store: a stone door into a turfed mound, frost on its lintel."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(10, 30):
        half = min(15, 6 + (y - 10))
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, ROCK_LIGHT if x < 11 else (ROCK_DARK if x > 22 else ROCK))
    for x in range(6, 27):  # turf on top
        put(image, x, 10 + max(0, abs(x - 16) - 6), MOSS)
    rect(image, 11, 17, 21, 29, STONE)
    rect(image, 11, 17, 21, 18, ICE_LIGHT)
    rect(image, 13, 20, 19, 29, STONE_DARK)
    rect(image, 15, 22, 17, 29, DOOR)
    put(image, 12, 19, ICE)
    put(image, 20, 19, ICE)
    return outline(image)


def stone_lodge():
    """The dwarves' hunting lodge: a squat stone hut with a turf roof, antlers and a brazier."""
    image = Image.new("RGBA", (36, 34), (0, 0, 0, 0))
    rect(image, 6, 17, 27, 31, STONE)
    for y in range(18, 32, 3):
        for x in range(6 + (y % 2) * 2, 28, 5):
            put(image, x, y, STONE_DARK)
    rect(image, 24, 17, 27, 31, STONE_DARK)
    roof(image, 9, 17, 16, 5, 13, (110, 150, 80, 255), MOSS, (70, 100, 56, 255))
    rect(image, 13, 23, 18, 31, DOOR)
    for x, step in ((12, -1), (19, 1)):
        put(image, x, 21, ANTLER)
        put(image, x + step, 20, ANTLER)
        put(image, x + 2 * step, 19, ANTLER)
    rect(image, 30, 28, 32, 31, STONE_DARK)  # the brazier
    put(image, 31, 27, FLAME)
    put(image, 31, 26, FLAME_HOT)
    put(image, 31, 25, ORE)
    return outline(image)


def fish_rack(image, left, top, bottom):
    """A pole rack with fish hung up to dry."""
    rect(image, left, top, left, bottom, BARK)
    rect(image, left + 6, top, left + 6, bottom, BARK)
    rect(image, left, top, left + 6, top, BARK)
    for x in (left + 2, left + 4):
        rect(image, x, top + 1, x, top + 5, FISH)
        put(image, x, top + 1, FISH_LIGHT)
        put(image, x, top + 6, FISH_LIGHT)


def fishing_lodge():
    """The humans' fishing lodge: a plank hut on stilts at the water, a net hung on its wall, fish drying on a rack."""
    image = Image.new("RGBA", (38, 38), (0, 0, 0, 0))
    rect(image, 2, 33, 30, 35, WATER)  # the water's edge under the stilts
    for x in (4, 10, 18, 26):
        put(image, x, 34, WATER_LIGHT)
    for x in (7, 15, 23):  # stilts
        rect(image, x, 29, x + 1, 35, TIMBER_DARK)
    rect(image, 4, 28, 28, 29, TIMBER_DARK)  # the deck
    rect(image, 6, 17, 26, 27, TIMBER)
    for x in range(7, 26, 3):
        rect(image, x, 17, x, 27, TIMBER_DARK)
    rect(image, 23, 17, 26, 27, TIMBER_DARK)
    roof(image, 6, 18, 16, 3, 13, THATCH_LIGHT, THATCH, THATCH_DARK)
    rect(image, 13, 21, 17, 27, DOOR)
    for y in range(19, 26):  # the net on the wall
        for x in range(19, 23):
            put(image, x, y, NET if (x + y) % 2 == 0 else NET_DARK)
    fish_rack(image, 30, 20, 31)
    return outline(image)


def fishers_hall():
    """The dwarves' fishing place: a squat stone hall by a cold stream, a turf roof, fish on a rack and a creel."""
    image = Image.new("RGBA", (38, 36), (0, 0, 0, 0))
    rect(image, 2, 32, 28, 34, WATER)
    for x in (5, 13, 21):
        put(image, x, 33, ICE_LIGHT)
    rect(image, 5, 18, 26, 31, STONE)
    for y in range(19, 32, 3):
        for x in range(5 + (y % 2) * 2, 27, 5):
            put(image, x, y, STONE_DARK)
    rect(image, 23, 18, 26, 31, STONE_DARK)
    roof(image, 10, 18, 15, 5, 13, (110, 150, 80, 255), MOSS, (70, 100, 56, 255))
    rect(image, 12, 24, 17, 31, DOOR)
    rect(image, 19, 27, 22, 31, TIMBER)  # a creel by the door
    rect(image, 19, 27, 22, 27, TIMBER_DARK)
    put(image, 20, 26, FISH_LIGHT)
    fish_rack(image, 29, 19, 31)
    return outline(image)


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/buildings/" + name).encode()).hexdigest()[:15], 16)


def write(image, name):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".png")
    image.save(path)
    meta = path + ".hymeta"
    handle = handle_for(meta, name)
    lines = [f"Handle: {handle}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest", "  Wrap: Clamp",
             "  SpriteMode: Single", "  PixelsPerUnit: 32"]
    with open(meta, "w", encoding="utf-8", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")
    print(f"{name}: {image.width} x {image.height}, handle {handle}")


def main():
    images = {"Granary": granary(), "HuntingLodge": hunting_lodge(), "Barn": barn(), "LarderBower": larder_bower(),
              "HuntersBlind": hunters_blind(), "SeedHollow": seed_hollow(), "ColdCellar": cold_cellar(), "StoneLodge": stone_lodge(),
              "FishingLodge": fishing_lodge(), "FishersHall": fishers_hall()}
    for name, image in images.items():
        write(image, name)
    if "--preview" in sys.argv:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
        os.makedirs(out, exist_ok=True)
        preview = Image.new("RGBA", (44 * len(images), 44), (120, 150, 90, 255))
        for i, image in enumerate(images.values()):
            preview.alpha_composite(image, (i * 44 + (44 - image.width) // 2, 44 - image.height))
        preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(os.path.join(out, "buildings_preview.png"))


if __name__ == "__main__":
    main()
