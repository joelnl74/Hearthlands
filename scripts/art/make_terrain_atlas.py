# Makes Project/Assets/Textures/TerrainAtlas.png, tile_blend.frag's atlas, from the 32 x 32 textures in Art/Terrain.
# One row per terrain id (0 sand, 1 dirt, 2 grass, 3 water, 4 snow, 5 riverbed, 6 rock, 7 farmland), Columns variants
# per row: the shader picks one per tile, so a texture listed more often shows up more. Rows WetRow + id hold the wet look: land with high moisture,
# water next to land (shallow). A row left out repeats the dry one. Every variant's main colour becomes its row's first
# texture's, so tiles of one type meet without a seam. Pillow only.
#   py scripts/art/make_terrain_atlas.py [--preview]   (--preview also writes scripts/art/out/atlas_preview.png)
import os
import sys
from PIL import Image

S = 32         # texels per tile: the scene's Tiling.x
Columns = 8    # variants per row
MaxTypes = 16  # tile_blend.frag's MaxTypes
WetRow = 16    # tile_blend.frag's WetRow: the wet rows start here
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ART = os.path.join(ROOT, "Art", "Terrain")

SAND_RAMP = ["CDB073", "DEC784", "EBD9A4"]
DIRT_RAMP = ["3A2A1F", "4A3628", "564030", "6B5442"]

# id: [(texture, times)], the times adding up to Columns. "darker:<name>" moves each colour one step down its ramp.
DRY = {
    0: [("sand_01", 4), ("sand_02", 3), ("sand_shells_01", 1)],
    1: [("dirt_01", 4), ("dirt_02", 3), ("dirt_cracked_01", 1)],
    2: [("grass_plain_01", 2), ("grass_plain_02", 2), ("grass_plain_03", 1), ("grass_01", 2), ("grass_meadow_01", 1)],
    3: [("water_01", 8)],
    4: [("snow_01", 4), ("snow_02", 2), ("snow_03", 2)],
    5: [("riverbed_01", 3), ("riverbed_02", 3), ("riverbed_03", 2)],
    6: [("Temp/rock_01", 4), ("Temp/rock_02", 3), ("Temp/rock_cracked_01", 1)],
    7: [("Temp/farmland_01", 5), ("Temp/farmland_02", 3)],
}
WET = {
    0: [("darker:sand_01", 4), ("darker:sand_02", 4)],
    1: [("Temp/bank_wet_01", 5), ("darker:dirt_02", 3)],
    2: [("grass_lush_01", 3), ("grass_dark_01", 3), ("grass_plain_01", 1), ("grass_meadow_01", 1)],
    3: [("Temp/water_shallow_01", 8)],
    7: [("Temp/farmland_wet_01", 8)],
}


def darker(image):
    ramps = [[tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in ramp] for ramp in (SAND_RAMP, DIRT_RAMP)]
    out = image.copy()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = image.getpixel((x, y))
            for ramp in ramps:
                if (r, g, b) in ramp:
                    out.putpixel((x, y), ramp[max(0, ramp.index((r, g, b)) - 1)] + (a,))
                    break
    return out


def load(name):
    dark = name.startswith("darker:")
    path = os.path.join(ART, name.split(":")[-1] + ".png")
    image = Image.open(path).convert("RGBA")
    if image.size != (S, S):
        raise SystemExit(f"{path}: {image.size}, expected {S} x {S}")
    return darker(image) if dark else image


def main_colour(image):
    counts = {}
    for pixel in image.getdata():
        counts[pixel] = counts.get(pixel, 0) + 1
    return max(counts, key=counts.get)


def recolour(image, old, new):
    out = image.copy()
    for y in range(image.height):
        for x in range(image.width):
            if image.getpixel((x, y)) == old:
                out.putpixel((x, y), new)
    return out


def fill_row(atlas, row, entries):
    names = [name for name, times in entries for _ in range(times)]
    if len(names) != Columns:
        raise SystemExit(f"row {row}: {len(names)} variants, expected {Columns}")
    base = main_colour(load(names[0]))
    for column, name in enumerate(names):
        image = load(name)
        atlas.paste(recolour(image, main_colour(image), base), (column * S, row * S))


def make_atlas():
    atlas = Image.new("RGBA", (S * Columns, S * (WetRow + MaxTypes)), (0, 0, 0, 0))
    for tile_id, entries in DRY.items():
        fill_row(atlas, tile_id, entries)
        fill_row(atlas, WetRow + tile_id, WET.get(tile_id, entries))
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
        atlas.resize((atlas.width * 2, atlas.height * 2), Image.NEAREST).save(os.path.join(out, "atlas_preview.png"))


if __name__ == "__main__":
    main()
