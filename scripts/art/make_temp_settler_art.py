# Makes temporary settler art, drawn in code until painted art replaces it: Project/Assets/Sprites/Settlers/ Campfire, Tent,
# Hut, ElfHearth, Bower, ElvenLodge, ElvenLantern, LeafHut, TreeHouse, MineEntrance, StoneHut, StoneHall (objects, 32 px a
# tile) and Settler, ElfFolk, Elf, Dwarf (16 x 26 cells: rows idle, walk, work, each for SE, SW, NE, NW: SettlersView's
# order), with their .hymeta (handles kept). Pillow only.
#   py scripts/art/make_temp_settler_art.py [--preview]   (--preview also writes scripts/art/out/settlers_preview.png)
import hashlib
import os
import re
import sys
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Project", "Assets", "Sprites", "Settlers")

OUTLINE = (34, 24, 20, 255)
SKIN = (214, 168, 124, 255)
SKIN_DARK = (176, 128, 92, 255)
HAIR = (66, 42, 28, 255)
HAIR_LIGHT = (98, 66, 42, 255)
FUR = (146, 100, 58, 255)
FUR_DARK = (108, 72, 40, 255)
FUR_LIGHT = (178, 132, 84, 255)
BELT = (76, 54, 34, 255)
WRAP = (96, 70, 46, 255)
STONE = (128, 124, 116, 255)
STONE_DARK = (88, 84, 80, 255)
STONE_LIGHT = (164, 160, 150, 255)
LOG = (112, 74, 42, 255)
LOG_DARK = (74, 48, 28, 255)
EMBER = (232, 96, 36, 255)
FLAME = (252, 176, 52, 255)
FLAME_HOT = (255, 236, 150, 255)
HIDE = (186, 148, 102, 255)
HIDE_LIGHT = (212, 178, 130, 255)
HIDE_DARK = (146, 110, 72, 255)
DOOR = (52, 36, 26, 255)
POLE = (84, 56, 34, 255)

HUMAN = {"skin": SKIN, "skin_dark": SKIN_DARK, "hair": HAIR, "hair_light": HAIR_LIGHT, "body": FUR, "body_dark": FUR_DARK,
         "body_light": FUR_LIGHT, "belt": BELT, "legs": WRAP, "tool": POLE, "ears": False}
# Wood elves: pale, fair-haired, in moss-green cloaks with leaf-brown leggings; pointed ears; a slender staff.
ELF = {"skin": (232, 206, 176, 255), "skin_dark": (196, 166, 136, 255), "hair": (226, 210, 142, 255),
       "hair_light": (246, 236, 182, 255), "body": (74, 128, 70, 255), "body_dark": (48, 92, 52, 255),
       "body_light": (110, 162, 92, 255), "belt": (148, 112, 58, 255), "legs": (96, 78, 52, 255), "tool": (180, 156, 104, 255),
       "ears": True}
# Elves (the first, of the forest's edge): dark chestnut hair, silver-blue cloaks over grey; pointed ears; a slim staff.
ELF_FOLK = {"skin": (230, 200, 168, 255), "skin_dark": (192, 160, 128, 255), "hair": (112, 66, 40, 255),
            "hair_light": (148, 92, 56, 255), "body": (92, 118, 156, 255), "body_dark": (64, 84, 118, 255),
            "body_light": (130, 156, 192, 255), "belt": (176, 168, 150, 255), "legs": (84, 86, 96, 255), "tool": (196, 186, 160, 255),
            "ears": True}
DWARF = {"skin": (226, 172, 136, 255), "skin_dark": (188, 134, 100, 255), "beard": (178, 92, 44, 255),
         "beard_dark": (132, 64, 30, 255), "helmet": (150, 156, 166, 255), "helmet_light": (196, 202, 210, 255),
         "body": (78, 96, 128, 255), "body_dark": (54, 68, 96, 255), "body_light": (104, 124, 158, 255), "belt": (96, 66, 38, 255),
         "buckle": (220, 184, 80, 255), "boots": (70, 50, 34, 255), "tool": (120, 120, 128, 255), "haft": (110, 76, 44, 255)}
TIMBER = (120, 84, 50, 255)
TIMBER_DARK = (82, 56, 34, 255)
ROCK = (122, 118, 112, 255)
ROCK_DARK = (86, 82, 78, 255)
ROCK_LIGHT = (160, 156, 148, 255)
CART = (96, 70, 46, 255)
ORE = (210, 170, 70, 255)
LEAF = (78, 136, 64, 255)
LEAF_DARK = (48, 96, 48, 255)
LEAF_LIGHT = (122, 176, 86, 255)
BARK = (92, 70, 48, 255)
GLOW = (184, 246, 214, 255)
GLOW_DIM = (110, 196, 170, 255)
MOSS = (96, 128, 72, 255)


def put(image, x, y, color):
    if 0 <= x < image.width and 0 <= y < image.height:
        image.putpixel((x, y), color)


def rect(image, x0, y0, x1, y1, color):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            put(image, x, y, color)


def outline(image):
    """A dark outline around everything drawn (4-neighbours), like the other sprites."""
    pixels = image.load()
    edge = []
    for y in range(image.height):
        for x in range(image.width):
            if pixels[x, y][3] != 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < image.width and 0 <= ny < image.height and pixels[nx, ny][3] != 0 and pixels[nx, ny] != OUTLINE:
                    edge.append((x, y))
                    break
    for x, y in edge:
        pixels[x, y] = OUTLINE
    return image


def settler(facing, motion, frame, look=HUMAN):
    """One 16 x 26 cell. facing: SE, SW, NE, NW (SW and NW are mirrors); motion: idle, walk, work; look: a palette."""
    cell = Image.new("RGBA", (16, 26), (0, 0, 0, 0))
    back = facing in ("NE", "NW")
    bob = 1 if (motion == "idle" and frame == 1) or (motion == "walk" and frame in (1, 3)) else 0
    top = 3 + bob
    # Legs (walk: one forward, one back; the body above them bobs)
    stride = {0: (0, 0), 1: (-1, 1), 2: (0, 0), 3: (1, -1)}[frame % 4] if motion == "walk" else (0, 0)
    rect(cell, 6 + stride[0], 19, 7 + stride[0], 23, look["legs"])
    rect(cell, 9 + stride[1], 19, 10 + stride[1], 23, look["legs"])
    rect(cell, 5 + stride[0], 24, 7 + stride[0], 24, look["skin_dark"])
    rect(cell, 9 + stride[1], 24, 11 + stride[1], 24, look["skin_dark"])
    # Body: a fur tunic, light from the upper left
    rect(cell, 5, top + 8, 11, top + 16, look["body"])
    rect(cell, 5, top + 8, 6, top + 16, look["body_light"])
    rect(cell, 10, top + 8, 11, top + 16, look["body_dark"])
    rect(cell, 5, top + 13, 11, top + 13, look["belt"])
    for x in (6, 8, 10):
        put(cell, x, top + 16, look["body_dark"])  # a ragged hem
    # Arms: at the sides, swinging when walking, up and down when working
    if motion == "work":
        lift = -5 if frame % 2 == 0 else -1
        rect(cell, 12, top + 9 + lift, 13, top + 13 + lift, look["skin"])
        rect(cell, 12, top + 7 + lift, 14, top + 8 + lift, look["tool"])  # a tool
        rect(cell, 3, top + 9, 4, top + 13, look["skin"])
    else:
        swing = {0: 0, 1: 1, 2: 0, 3: -1}[frame % 4] if motion == "walk" else 0
        rect(cell, 3, top + 9 + swing, 4, top + 14 + swing, look["skin"])
        rect(cell, 12, top + 9 - swing, 13, top + 14 - swing, look["skin_dark"])
    # Head: hair all round from the back, a face turned right from the front
    rect(cell, 5, top + 1, 11, top + 7, look["hair"] if back else look["skin"])
    if back:
        rect(cell, 6, top + 1, 9, top + 2, look["hair_light"])
        rect(cell, 5, top + 6, 11, top + 7, look["hair"])
    else:
        rect(cell, 5, top, 11, top + 2, look["hair"])
        rect(cell, 5, top + 3, 6, top + 6, look["hair"])
        put(cell, 10, top + 4, OUTLINE)  # an eye, looking right
        put(cell, 8, top + 4, OUTLINE)
        rect(cell, 8, top + 6, 10, top + 6, look["skin_dark"])
    rect(cell, 5, top, 11, top, look["hair"])
    if look["ears"]:
        put(cell, 4, top + 3, look["skin"])  # pointed ears past the hair
        put(cell, 3, top + 2, look["skin"])
        put(cell, 12, top + 3, look["skin_dark"])
        put(cell, 13, top + 2, look["skin_dark"])
    outline(cell)
    if facing in ("SW", "NW"):
        cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
    return cell


def dwarf(facing, motion, frame):
    """One 16 x 26 cell of a dwarf: short and broad, an iron helmet, a long red beard, a pick for work."""
    d = DWARF
    cell = Image.new("RGBA", (16, 26), (0, 0, 0, 0))
    back = facing in ("NE", "NW")
    bob = 1 if (motion == "idle" and frame == 1) or (motion == "walk" and frame in (1, 3)) else 0
    top = 6 + bob
    stride = {0: (0, 0), 1: (-1, 1), 2: (0, 0), 3: (1, -1)}[frame % 4] if motion == "walk" else (0, 0)
    rect(cell, 5 + stride[0], 21, 7 + stride[0], 24, d["boots"])  # short legs, heavy boots
    rect(cell, 9 + stride[1], 21, 11 + stride[1], 24, d["boots"])
    rect(cell, 4, top + 7, 12, 21, d["body"])  # a broad tunic
    rect(cell, 4, top + 7, 5, 21, d["body_light"])
    rect(cell, 11, top + 7, 12, 21, d["body_dark"])
    rect(cell, 4, 18, 12, 18, d["belt"])
    put(cell, 8, 18, d["buckle"])
    if motion == "work":
        lift = -6 if frame % 2 == 0 else -1
        rect(cell, 13, top + 8 + lift, 14, top + 12 + lift, d["skin"])
        rect(cell, 13, top + 4 + lift, 13, top + 8 + lift, d["haft"])  # the pick
        rect(cell, 11, top + 4 + lift, 15, top + 4 + lift, d["tool"])
        rect(cell, 2, top + 8, 3, top + 12, d["skin"])
    else:
        swing = {0: 0, 1: 1, 2: 0, 3: -1}[frame % 4] if motion == "walk" else 0
        rect(cell, 2, top + 8 + swing, 3, top + 13 + swing, d["skin"])
        rect(cell, 13, top + 8 - swing, 14, top + 13 - swing, d["skin_dark"])
    rect(cell, 5, top, 11, top + 6, d["skin"])  # the head
    rect(cell, 5, top - 1, 11, top + 1, d["helmet"])
    rect(cell, 6, top - 2, 10, top - 2, d["helmet"])
    put(cell, 6, top - 1, d["helmet_light"])
    if back:
        rect(cell, 5, top + 2, 11, top + 6, d["beard_dark"])  # hair under the helmet
        rect(cell, 4, top + 5, 4, top + 8, d["beard_dark"])
        rect(cell, 12, top + 5, 12, top + 8, d["beard_dark"])
    else:
        put(cell, 10, top + 3, OUTLINE)  # an eye under the brim
        put(cell, 8, top + 3, OUTLINE)
        put(cell, 9, top + 2, d["helmet"])  # the nose guard
        rect(cell, 5, top + 5, 11, top + 6, d["beard"])  # the beard down over the chest
        rect(cell, 6, top + 7, 11, top + 10, d["beard"])
        rect(cell, 7, top + 11, 10, top + 12, d["beard_dark"])
        put(cell, 5, top + 4, d["beard"])
    outline(cell)
    if facing in ("SW", "NW"):
        cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
    return cell


def mine_entrance():
    """The dwarves' invitation: a timbered mine mouth in a heap of rock, a cart of ore before it."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(10, 30):  # the rock heap
        half = min(15, 6 + (y - 10))
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, ROCK_LIGHT if x < 12 else (ROCK_DARK if x > 22 else ROCK))
    for x, y in ((7, 20), (24, 22), (11, 13), (21, 14), (5, 27), (27, 27)):
        put(image, x, y, ROCK_DARK)
    rect(image, 11, 16, 20, 29, DOOR)  # the dark mouth
    rect(image, 10, 15, 11, 29, TIMBER)  # timber frame
    rect(image, 20, 15, 21, 29, TIMBER_DARK)
    rect(image, 9, 14, 22, 15, TIMBER)
    rect(image, 13, 26, 18, 26, TIMBER_DARK)  # rails into the mine
    rect(image, 13, 28, 18, 28, TIMBER_DARK)
    rect(image, 23, 25, 28, 28, CART)  # an ore cart
    for x in (24, 26):
        put(image, x, 24, ORE)
    put(image, 25, 23, ORE)
    put(image, 16, 18, (250, 196, 96, 255))  # a lamp inside
    return outline(image)


def stone_hut():
    """The dwarves' shelter: a low hall dug into a hump of rock, a round wooden door."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    for y in range(12, 30):
        half = min(14, 5 + (y - 12))
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, ROCK_LIGHT if x < 12 else (ROCK_DARK if x > 21 else ROCK))
    for y in range(20, 30):  # the round door
        half = 3 if y > 21 else 2
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, TIMBER if x % 2 else TIMBER_DARK)
    put(image, 18, 25, ORE)  # its iron ring
    rect(image, 22, 9, 23, 13, ROCK_DARK)  # a chimney
    return outline(image)


def settler_sheet(look=HUMAN):
    motions = (("idle", 2), ("walk", 4), ("work", 2))
    columns = max(frames for _, frames in motions)
    sheet = Image.new("RGBA", (16 * columns, 26 * 12), (0, 0, 0, 0))
    names = []
    for m, (motion, frames) in enumerate(motions):
        for d, facing in enumerate(("SE", "SW", "NE", "NW")):
            names.append(f"{motion}_{facing}")
            for frame in range(frames):
                drawn = dwarf(facing, motion, frame) if look is DWARF else settler(facing, motion, frame, look)
                sheet.paste(drawn, (frame * 16, (m * 4 + d) * 26))
    return sheet, columns, names


def campfire():
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    # A ring of stones, flattened as the ground is seen from 30 degrees
    stones = [(8, 23), (11, 21), (15, 20), (19, 20), (23, 21), (25, 24), (23, 27), (19, 28), (14, 28), (10, 27)]
    for i, (x, y) in enumerate(stones):
        rect(image, x - 1, y - 1, x + 1, y, STONE if i % 3 else STONE_DARK)
        put(image, x - 1, y - 1, STONE_LIGHT)
    # Embers and crossed logs
    rect(image, 12, 23, 21, 25, EMBER)
    for i in range(10):
        put(image, 11 + i, 26 - i // 3, LOG)
        put(image, 11 + i, 27 - i // 3, LOG_DARK)
        put(image, 21 - i, 26 - i // 3, LOG)
        put(image, 21 - i, 27 - i // 3, LOG_DARK)
    # A small flame (the game adds flickering particles over it)
    flame = [(14, 22), (15, 22), (16, 22), (17, 22), (18, 22), (15, 21), (16, 21), (17, 21), (15, 20), (16, 20), (16, 19),
             (16, 18), (17, 20), (14, 21), (18, 21)]
    for x, y in flame:
        put(image, x, y, FLAME)
    for x, y in ((16, 21), (16, 20), (15, 21), (17, 21)):
        put(image, x, y, FLAME_HOT)
    return outline(image)


def tent():
    image = Image.new("RGBA", (32, 40), (0, 0, 0, 0))
    apex, base_left, base_right, base_y = 16, 3, 28, 36
    for y in range(7, base_y + 1):
        t = (y - 7) / (base_y - 7)
        left = round(apex - (apex - base_left) * t)
        right = round(apex + (base_right - apex) * t)
        for x in range(left, right + 1):
            # Light from the upper left: the left flank lighter, the right darker, a seam down the front
            color = HIDE_LIGHT if x < apex - 2 else (HIDE_DARK if x > apex + 3 else HIDE)
            put(image, x, y, color)
        if y % 6 == 0:
            for x in range(left + 1, right, 3):
                put(image, x, y, HIDE_DARK)  # stitches between the hides
    # The door: a dark opening at the front, flap folded back
    for y in range(24, base_y + 1):
        half = (y - 24) // 2
        for x in range(apex - half, apex + half + 1):
            put(image, x, y, DOOR)
        put(image, apex + half + 1, y, HIDE_LIGHT)
    # Poles through the smoke hole
    for i in range(6):
        put(image, apex - 1 - i // 2, 6 - i, POLE)
        put(image, apex + 1 + i // 2, 6 - i, POLE)
        put(image, apex, 6 - i, POLE)
    return outline(image)


def lantern():
    """The elves' invitation: a slender post with a hanging lantern that glows pale green, mossy stones at its foot."""
    image = Image.new("RGBA", (32, 40), (0, 0, 0, 0))
    rect(image, 15, 8, 16, 35, BARK)
    put(image, 15, 8, look_light(BARK))
    rect(image, 17, 9, 21, 9, BARK)  # the arm
    rect(image, 21, 10, 21, 12, BARK)
    rect(image, 19, 13, 23, 19, GLOW_DIM)  # the lantern
    rect(image, 20, 14, 22, 18, GLOW)
    rect(image, 19, 13, 23, 13, BARK)
    rect(image, 19, 19, 23, 19, BARK)
    for x, y, color in ((11, 34, STONE), (12, 33, MOSS), (18, 35, STONE_DARK), (19, 34, MOSS), (14, 36, STONE), (17, 36, MOSS)):
        rect(image, x, y, x + 1, y + 1, color)
    for x, y in ((14, 31), (17, 32), (13, 33)):
        put(image, x, y, LEAF_LIGHT)  # ferns at the foot
    return outline(image)


def look_light(color):
    return tuple(min(255, c + 30) for c in color[:3]) + (255,)


def leaf_hut():
    """The elves' shelter: a low dome of woven boughs and leaves with a round door."""
    image = Image.new("RGBA", (32, 36), (0, 0, 0, 0))
    cx, base = 16, 32
    for y in range(12, base + 1):
        t = (y - 12) / (base - 12)
        half = round(4 + 11 * (1 - (1 - t) ** 2))
        for x in range(cx - half, cx + half + 1):
            shade = LEAF_LIGHT if x < cx - half // 2 else (LEAF_DARK if x > cx + half // 2 else LEAF)
            put(image, x, y, shade)
        for x in range(cx - half + (y % 3), cx + half, 4):
            put(image, x, y, LEAF_DARK if y % 2 else LEAF_LIGHT)  # leaves overlapping
    for y in range(22, base + 1):  # the round door
        half = 3 if y > 23 else 2
        for x in range(cx - half, cx + half + 1):
            put(image, x, y, DOOR)
    for i in range(5):  # a bough arching over the top
        put(image, cx - 6 + i * 3, 11 + abs(2 - i), BARK)
    put(image, cx + 7, 16, GLOW)  # a small lantern by the door
    return outline(image)


def elf_hearth():
    """The elves' centre: a ring of pale stones round a low fire burning silver-blue, a carved post with a hanging charm."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    pale, pale_dark = (206, 208, 214, 255), (150, 154, 166, 255)
    stones = [(8, 23), (11, 21), (15, 20), (19, 20), (23, 21), (25, 24), (23, 27), (19, 28), (14, 28), (10, 27)]
    for i, (x, y) in enumerate(stones):
        rect(image, x - 1, y - 1, x + 1, y, pale if i % 3 else pale_dark)
    rect(image, 12, 23, 21, 25, (120, 170, 220, 255))
    for x, y in ((14, 22), (15, 21), (16, 20), (17, 21), (18, 22), (16, 19), (15, 22), (17, 22), (16, 21), (16, 18)):
        put(image, x, y, (170, 214, 255, 255))
    for x, y in ((16, 20), (16, 21), (15, 21)):
        put(image, x, y, (236, 246, 255, 255))
    rect(image, 26, 10, 27, 26, BARK)  # a carved post
    put(image, 26, 10, look_light(BARK))
    rect(image, 23, 12, 25, 12, BARK)
    rect(image, 23, 13, 23, 16, (200, 220, 240, 255))  # the charm
    return outline(image)


def bower():
    """The elves' first shelter: an arch of bent saplings woven with leafy boughs, open at the front."""
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    cx, base = 16, 29
    for y in range(12, base + 1):
        t = (y - 12) / (base - 12)
        half = round(3 + 10 * (1 - (1 - t) ** 2))
        for x in range(cx - half, cx + half + 1):
            shade = LEAF_LIGHT if x < cx - half // 2 else (LEAF_DARK if x > cx + half // 2 else (96, 148, 80, 255))
            put(image, x, y, shade)
        put(image, cx - half, y, BARK)  # the bent saplings at the edges
        put(image, cx + half, y, BARK)
    for y in range(19, base + 1):  # the open front
        half = (y - 19) // 2 + 2
        for x in range(cx - half, cx + half + 1):
            put(image, x, y, DOOR)
    for i in range(5):
        put(image, cx - 8 + i * 4, 13 + (i % 2), (150, 196, 120, 255))
    return outline(image)


def elven_lodge():
    """The elves' glen and village house: timber walls under a long roof that sweeps up at both ends, a round window."""
    image = Image.new("RGBA", (36, 40), (0, 0, 0, 0))
    roof, roof_dark, roof_light = (84, 120, 112, 255), (58, 88, 84, 255), (116, 152, 140, 255)
    rect(image, 7, 24, 28, 37, TIMBER)
    for x in range(9, 28, 4):
        rect(image, x, 24, x, 37, TIMBER_DARK)
    rect(image, 25, 24, 28, 37, TIMBER_DARK)
    for y in range(10, 26):  # the roof, its ends sweeping up
        t = (y - 10) / 15
        half = round(5 + 13 * t)
        for x in range(18 - half, 18 + half + 1):
            put(image, x, y, roof_light if x < 14 else (roof_dark if x > 23 else roof))
        if y % 3 == 1:
            for x in range(18 - half + 1, 18 + half, 3):
                put(image, x, y, roof_dark)
    for x, y in ((1, 23), (2, 24), (34, 23), (33, 24), (0, 22), (35, 22)):  # the upswept eaves
        put(image, x, y, roof_light if x < 18 else roof_dark)
    rect(image, 15, 29, 20, 37, DOOR)
    for x, y in ((10, 27), (11, 26), (12, 27), (11, 28)):  # a round window, lit
        put(image, x, y, (200, 226, 255, 255))
    put(image, 11, 27, (236, 246, 255, 255))
    return outline(image)


def timber_hut():
    """The humans' hamlet shelter: a timber hut with a thatched roof, a door and a window."""
    image = Image.new("RGBA", (32, 40), (0, 0, 0, 0))
    thatch, thatch_dark, thatch_light = (196, 160, 82, 255), (150, 118, 56, 255), (222, 190, 112, 255)
    rect(image, 6, 22, 26, 37, TIMBER)  # the walls: logs
    for y in range(23, 38, 3):
        rect(image, 6, y, 26, y, TIMBER_DARK)
    rect(image, 22, 22, 26, 37, TIMBER_DARK)  # the shaded side
    for y in range(8, 24):  # the roof, eaves past the walls
        t = (y - 8) / 15
        half = round(3 + 13 * t)
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, thatch_light if x < 12 else (thatch_dark if x > 21 else thatch))
        if y % 3 == 0:
            for x in range(16 - half + 1, 16 + half, 4):
                put(image, x, y, thatch_dark)
    rect(image, 13, 28, 18, 37, DOOR)  # the door
    put(image, 17, 33, ORE)  # its latch
    rect(image, 8, 26, 10, 28, (250, 210, 120, 255))  # a lit window
    rect(image, 20, 6, 21, 10, STONE_DARK)  # a chimney
    return outline(image)


def tree_house():
    """The elves' glade shelter: a round house of woven boughs up in a tree, a ladder down and a lantern."""
    image = Image.new("RGBA", (32, 44), (0, 0, 0, 0))
    rect(image, 14, 22, 18, 43, BARK)  # the trunk
    rect(image, 17, 22, 18, 43, (70, 52, 36, 255))
    for x, y in ((12, 42), (20, 42), (11, 43), (21, 43)):
        put(image, x, y, BARK)  # roots
    for y in range(2, 14):  # the crown above
        half = round(12 * (1 - abs(y - 8) / 7))
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, LEAF_LIGHT if x < 13 else (LEAF_DARK if x > 20 else LEAF))
    for y in range(13, 24):  # the house: a dome of boughs on a platform
        half = round(4 + 6 * (1 - abs(y - 18) / 6))
        for x in range(16 - half, 16 + half + 1):
            put(image, x, y, MOSS if x < 13 else (LEAF_DARK if x > 20 else (88, 120, 62, 255)))
    rect(image, 5, 24, 27, 25, TIMBER)  # the platform
    rect(image, 14, 18, 17, 23, DOOR)
    for y in range(26, 43, 2):  # a ladder
        rect(image, 9, y, 11, y, TIMBER_DARK)
    rect(image, 9, 26, 9, 42, TIMBER)
    rect(image, 11, 26, 11, 42, TIMBER)
    rect(image, 23, 19, 24, 21, GLOW)  # a lantern
    return outline(image)


def stone_hall():
    """The dwarves' hold: a carved stone front set into the rock, a great double door, braziers either side."""
    image = Image.new("RGBA", (40, 36), (0, 0, 0, 0))
    for y in range(6, 34):  # the rock it is cut into
        half = min(19, 8 + (y - 6))
        for x in range(20 - half, 20 + half + 1):
            put(image, x, y, ROCK_LIGHT if x < 13 else (ROCK_DARK if x > 27 else ROCK))
    rect(image, 9, 14, 30, 33, STONE)  # the carved front
    rect(image, 9, 14, 30, 15, STONE_LIGHT)
    rect(image, 27, 16, 30, 33, STONE_DARK)
    for x in range(11, 29, 3):  # its lintel's runes
        put(image, x, 17, ORE)
    for y in range(21, 34):  # the double door, arched
        half = 4 if y > 22 else 3
        for x in range(20 - half, 20 + half + 1):
            put(image, x, y, TIMBER if x != 20 else TIMBER_DARK)
    put(image, 18, 28, ORE)
    put(image, 22, 28, ORE)
    for x in (11, 28):  # braziers
        rect(image, x - 1, 29, x + 1, 31, STONE_DARK)
        put(image, x, 28, FLAME)
        put(image, x, 27, FLAME_HOT)
    return outline(image)


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/settlers/" + name).encode()).hexdigest()[:15], 16)


def write(image, name, grid=None):
    path = os.path.join(OUT, name + ".png")
    image.save(path)
    meta = path + ".hymeta"
    lines = [f"Handle: {handle_for(meta, name)}", "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Nearest",
             "  Wrap: Clamp"]
    if grid is None:
        lines += ["  SpriteMode: Single", "  PixelsPerUnit: 32"]
    else:
        cell, columns, rows, names = grid
        lines += ["  SpriteMode: Multiple", "  PixelsPerUnit: 32", "  Slicing:", "    Mode: Grid",
                  f"    CellSize: [{cell[0]}, {cell[1]}]", "    Offset: [0, 0]", "    Spacing: [0, 0]", f"    Columns: {columns}",
                  f"    Rows: {rows}", "    RowNames: [" + ", ".join(names) + "]", "    DefaultPivot: [0.5, 0.5]", "  Sprites: []"]
    with open(meta, "w", encoding="utf-8", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")
    print(f"{name}: {image.width} x {image.height}, handle {handle_for(meta, name)}")


def main():
    os.makedirs(OUT, exist_ok=True)
    sheet, columns, names = settler_sheet()
    fire = campfire()
    shelter = tent()
    house = timber_hut()
    write(fire, "Campfire")
    write(shelter, "Tent")
    write(house, "Hut")
    write(sheet, "Settler", ((16, 26), columns, 12, names))
    folk, _, _ = settler_sheet(ELF_FOLK)
    hearth = elf_hearth()
    bough = bower()
    lodge = elven_lodge()
    write(hearth, "ElfHearth")
    write(bough, "Bower")
    write(lodge, "ElvenLodge")
    write(folk, "ElfFolk", ((16, 26), columns, 12, names))
    elves, _, _ = settler_sheet(ELF)
    light = lantern()
    hut = leaf_hut()
    write(light, "ElvenLantern")
    treehouse = tree_house()
    write(hut, "LeafHut")
    write(treehouse, "TreeHouse")
    write(elves, "Elf", ((16, 26), columns, 12, names))
    dwarves, _, _ = settler_sheet(DWARF)
    mine = mine_entrance()
    hall = stone_hut()
    write(mine, "MineEntrance")
    great = stone_hall()
    write(hall, "StoneHut")
    write(great, "StoneHall")
    write(dwarves, "Dwarf", ((16, 26), columns, 12, names))
    if "--preview" in sys.argv:
        preview = Image.new("RGBA", (sheet.width * 4 + 194, max(sheet.height, 140)), (120, 150, 90, 255))
        preview.alpha_composite(sheet, (0, 0))
        preview.alpha_composite(folk, (sheet.width + 8, 0))
        preview.alpha_composite(elves, (sheet.width * 2 + 16, 0))
        preview.alpha_composite(dwarves, (sheet.width * 3 + 24, 0))
        x = sheet.width * 4 + 32
        preview.alpha_composite(fire, (x, 0))
        preview.alpha_composite(shelter, (x + 36, 0))
        preview.alpha_composite(light, (x, 44))
        preview.alpha_composite(hut, (x + 36, 44))
        preview.alpha_composite(mine, (x, 84))
        preview.alpha_composite(hall, (x + 36, 84))
        preview.alpha_composite(house, (x + 72, 0))
        preview.alpha_composite(treehouse, (x + 72, 44))
        preview.alpha_composite(great, (x + 72, 92))
        preview.alpha_composite(hearth, (x + 116, 0))
        preview.alpha_composite(bough, (x + 116, 44))
        preview.alpha_composite(lodge, (x + 116, 84))
        out = os.path.join(ROOT, "scripts", "art", "out")
        os.makedirs(out, exist_ok=True)
        preview.resize((preview.width * 4, preview.height * 4), Image.NEAREST).save(os.path.join(out, "settlers_preview.png"))


if __name__ == "__main__":
    main()
