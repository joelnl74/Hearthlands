# Makes the animal sprite sheets in Project/Assets/Sprites/Animals from the packs in Art/Animals/Source (credits in
# Art/Animals/CREDITS.md), with their .hymeta (a grid of cells, kept handles). Walkers: rows idle, walk, run, each for SE, SW,
# NE, NW (AnimalsView's order); an animation a pack lacks repeats another. Birds: the pack's sheets as they are (rows fly
# left, away, toward, right, then the same on the ground). Faint pixels (drawn shadows) go: the engine casts its own. Pillow only.
#   py scripts/art/make_animal_sheets.py [--preview]   (--preview also writes scripts/art/out/animals_preview.png)
import hashlib
import os
import re
import sys
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SOURCE = os.path.join(ROOT, "Art", "Animals", "Source")
OUT = os.path.join(ROOT, "Project", "Assets", "Sprites", "Animals")
DIRECTIONS = ["SE", "SW", "NE", "NW"]


def strip(path, frames):
    """A horizontal strip cut into `frames` equal frames."""
    image = Image.open(path).convert("RGBA")
    width = image.width // frames
    return [image.crop((i * width, 0, (i + 1) * width, image.height)) for i in range(frames)]


def grid_row(path, cell, row, frames):
    image = Image.open(path).convert("RGBA")
    return [image.crop((i * cell, row * cell, (i + 1) * cell, (row + 1) * cell)) for i in range(frames)]


def stag():
    def take(direction, animation, frames):
        return strip(os.path.join(SOURCE, "Critters", "stag", f"critter_stag_{direction}_{animation}.png"), frames)
    return {a: {d: take(d, a, n) for d in DIRECTIONS} for a, n in (("idle", 24), ("walk", 11), ("run", 10))}


def boar():
    def take(direction, animation, frames):
        folder = os.path.join(SOURCE, "Critters", "boar")
        return [Image.open(os.path.join(folder, f"boar_{direction}_{animation}_{i}.png")).convert("RGBA") for i in range(frames)]
    run = {d: take(d, "run", 4) for d in DIRECTIONS}
    return {"idle": {d: take(d, "idle", 7) for d in DIRECTIONS}, "walk": run, "run": run}


def wolf():
    folder = os.path.join(SOURCE, "Critters", "wolf", "no shadow & effects")
    rows = {"SW": 0, "SE": 1, "NW": 2, "NE": 3}  # the pack's row order
    idle = {d: grid_row(os.path.join(folder, "wolf-idle.png"), 64, rows[d], 4) for d in DIRECTIONS}
    run = {d: grid_row(os.path.join(folder, "wolf-run.png"), 64, rows[d], 8) for d in DIRECTIONS}
    return {"idle": idle, "walk": run, "run": run}


def badger():
    def take(direction, animation, frames):
        return strip(os.path.join(SOURCE, "Critters", "badger", f"critter_badger_{direction}_{animation}.png"), frames)
    walk = {d: take(d, "walk", 9) for d in DIRECTIONS}
    return {"idle": {d: take(d, "idle", 22) for d in DIRECTIONS}, "walk": walk, "run": walk}


# Sheet name: what reads its animations from the pack
WALKERS = {"Deer": stag, "Boar": boar, "Wolf": wolf, "Badger": badger}
# Sheet name: the pack's bird sheet
BIRDS = {
    "Sparrow": "bird_3_sparrow", "Robin": "bird_3_robin", "Cardinal": "bird_2_cardinal", "Bluejay": "bird_1_bluejay",
    "Eagle": "bird_2_eagle", "Egret": "bird_1_white", "Crow": "bird_2_black",
}


def clean(image):
    """Drops faint pixels (the packs' drawn shadows) and makes the rest opaque: the sprites are cut-outs."""
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            pixels[x, y] = (r, g, b, 255) if a >= 128 else (0, 0, 0, 0)
    return image


def handle_for(meta_path, name):
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as file:
            found = re.search(r"^Handle:\s*(\d+)", file.read(), re.MULTILINE)
            if found:
                return int(found.group(1))
    return int(hashlib.sha1(("hearthlands/animals/" + name).encode()).hexdigest()[:15], 16)


def write_meta(png_path, name, cell, columns, rows, row_names):
    meta_path = png_path + ".hymeta"
    lines = [
        f"Handle: {handle_for(meta_path, name)}",
        "Type: Texture2D",
        "TextureImport:",
        "  Format: SRGB",
        "  Filter: Nearest",
        "  Wrap: Clamp",
        "  SpriteMode: Multiple",
        "  PixelsPerUnit: 32",
        "  Slicing:",
        "    Mode: Grid",
        f"    CellSize: [{cell[0]}, {cell[1]}]",
        "    Offset: [0, 0]",
        "    Spacing: [0, 0]",
        f"    Columns: {columns}",
        f"    Rows: {rows}",
        "    RowNames: [" + ", ".join(row_names) + "]",
        "    DefaultPivot: [0.5, 0.5]",
        "  Sprites: []",
    ]
    with open(meta_path, "w", encoding="utf-8", newline="\r\n") as file:
        file.write("\n".join(lines) + "\n")


def make_walker(name, animations):
    frames = [frame for animation in animations.values() for row in animation.values() for frame in row]
    size = frames[0].size
    box = None
    for frame in frames:
        frame_box = clean(frame).getbbox()
        if frame_box:
            box = frame_box if box is None else (min(box[0], frame_box[0]), min(box[1], frame_box[1]),
                                                  max(box[2], frame_box[2]), max(box[3], frame_box[3]))
    assert all(frame.size == size for frame in frames), name + ": frames of different sizes"
    cell = (box[2] - box[0], box[3] - box[1])
    columns = max(len(row) for animation in animations.values() for row in animation.values())
    sheet = Image.new("RGBA", (cell[0] * columns, cell[1] * 12), (0, 0, 0, 0))
    row_names = []
    for a, animation in enumerate(("idle", "walk", "run")):
        for d, direction in enumerate(DIRECTIONS):
            row = a * 4 + d
            row_names.append(f"{animation}_{direction}")
            for i, frame in enumerate(animations[animation][direction]):
                sheet.paste(frame.crop(box), (i * cell[0], row * cell[1]))
    path = os.path.join(OUT, name + ".png")
    sheet.save(path)
    write_meta(path, name, cell, columns, 12, row_names)
    counts = [len(animations[a]["SE"]) for a in ("idle", "walk", "run")]
    print(f"{name}: cell {cell[0]} x {cell[1]}, {columns} columns; frames idle {counts[0]}, walk {counts[1]}, run {counts[2]}")
    return sheet


def make_bird(name, source):
    sheet = clean(Image.open(os.path.join(SOURCE, "Birds", source + ".png")).convert("RGBA"))
    path = os.path.join(OUT, name + ".png")
    sheet.save(path)
    names = [f"fly_{d}" for d in ("left", "away", "toward", "right")] + [f"ground_{d}" for d in ("left", "away", "toward", "right")]
    write_meta(path, name, (32, 32), 3, 8, names)
    print(f"{name}: from {source}, cell 32 x 32, 3 columns")
    return sheet


def main():
    os.makedirs(OUT, exist_ok=True)
    sheets = [make_walker(name, make()) for name, make in WALKERS.items()]
    sheets += [make_bird(name, source) for name, source in BIRDS.items()]
    if "--preview" in sys.argv:
        width = max(sheet.width for sheet in sheets)
        preview = Image.new("RGBA", (width, sum(sheet.height + 4 for sheet in sheets)), (120, 150, 90, 255))
        y = 0
        for sheet in sheets:
            preview.alpha_composite(sheet, (0, y))
            y += sheet.height + 4
        out = os.path.join(ROOT, "scripts", "art", "out")
        os.makedirs(out, exist_ok=True)
        preview.resize((preview.width * 2, preview.height * 2), Image.NEAREST).save(os.path.join(out, "animals_preview.png"))


if __name__ == "__main__":
    main()
