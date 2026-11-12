# Makes the game UI's placeholder art: one atlas Project/Assets/Textures/UI/MenuArt.png of named sprites (frames, buttons,
# parchment, talent nodes, icons, a Brush<TileId> picture per terrain type cut from TerrainAtlas.png) and its .hymeta
# (SpriteMode Multiple, sprites by name, handle kept). Drawn in code at 4x and scaled down until painted art replaces it.
# Nine-slice borders and icon names live in Game/src/Views/MenuArt.h: keep them in step. Pillow only.
#   py scripts/art/make_ui_art.py [--preview]   (--preview also writes scripts/art/out/menu_art_preview.png)
import hashlib
import math
import os
import re
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Project", "Assets", "Textures", "UI")
NAME = "MenuArt"
SS = 4        # drawn this many times larger, then scaled down (smooth edges)
GUTTER = 4    # transparent pixels around each sprite (linear filtering never reads a neighbour)

NAVY_TOP = (30, 38, 74)
NAVY_BOTTOM = (14, 19, 44)
TILE_TOP = (24, 30, 60)
TILE_BOTTOM = (11, 15, 34)
GOLD = (216, 178, 90)
GOLD_LIGHT = (246, 222, 150)
GOLD_DARK = (122, 92, 38)
EDGE = (8, 9, 18)
PURPLE_TOP = (128, 82, 222)
PURPLE_BOTTOM = (62, 34, 140)
BLUE_TOP = (58, 128, 230)
BLUE_BOTTOM = (26, 64, 150)
CYAN = (120, 214, 255)
GREEN = (150, 224, 112)
PARCHMENT_TOP = (244, 233, 206)
PARCHMENT_BOTTOM = (226, 208, 166)
PARCHMENT_RIM = (150, 112, 52)
STONE = (120, 124, 138)


def gradient(size, top, bottom, alpha=255):
    width, height = size
    image = Image.new("RGBA", size)
    draw = ImageDraw.Draw(image)
    for y in range(height):
        t = y / max(height - 1, 1)
        color = tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        draw.line([(0, y), (width, y)], fill=color + (alpha,))
    return image


def rounded_mask(size, radius, inset=0):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([inset, inset, size[0] - 1 - inset, size[1] - 1 - inset], radius=max(radius - inset, 0),
                                           fill=255)
    return mask


def ring(size, radius, inset, width, color):
    """A rounded outline `width` thick whose outer edge is `inset` in from the image's."""
    layer = Image.new("RGBA", size, color + (0,))
    outer = rounded_mask(size, radius, inset)
    inner = rounded_mask(size, radius, inset + width)
    layer.putalpha(ImageChops.subtract(outer, inner))
    return layer


def glow(size, radius, inset, color, blur, strength=255):
    layer = Image.new("RGBA", size, color + (0,))
    mask = ImageChops.subtract(rounded_mask(size, radius, inset), rounded_mask(size, radius, inset + 2 * SS))
    mask = mask.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: min(255, v * strength // 255 * 2))
    layer.putalpha(mask)
    return layer


def frame(size, radius, top, bottom, rims, fill_alpha=245, halo=None, inner_line=None):
    """A rounded panel: a vertical gradient fill, rims [(inset, width, color)] outside in, an optional glow around it."""
    w, h = size[0] * SS, size[1] * SS
    big = (w, h)
    r = radius * SS
    image = Image.new("RGBA", big, (0, 0, 0, 0))
    if halo is not None:
        image.alpha_composite(glow(big, r, 6 * SS, halo, 3 * SS))
    pad = 8 * SS if halo is not None else 0
    fill = gradient(big, top, bottom, fill_alpha)
    fill.putalpha(ImageChops.multiply(fill.getchannel("A"), rounded_mask(big, r, pad)))
    image.alpha_composite(fill)
    # a soft shade inside the rim so the fill looks inset
    shade = ring(big, r, pad, 6 * SS, (0, 0, 0)).filter(ImageFilter.GaussianBlur(3 * SS))
    shade.putalpha(shade.getchannel("A").point(lambda v: v // 2))
    image.alpha_composite(shade)
    for inset, width, color in rims:
        image.alpha_composite(ring(big, r, pad + inset * SS, width * SS, color))
    if inner_line is not None:
        inset, color, alpha = inner_line
        line = ring(big, r, pad + inset * SS, max(SS, SS), color)
        line.putalpha(line.getchannel("A").point(lambda v: v * alpha // 255))
        image.alpha_composite(line)
    return image.resize(size, Image.LANCZOS)


def gold_rims(width=3):
    return [(0, 1, EDGE), (1, 1, GOLD_DARK), (2, width, GOLD), (2, 1, GOLD_LIGHT), (2 + width, 1, EDGE)]


def ornament(size):
    """A gold rule fading at both ends with a diamond in the middle (drawn at its size, never stretched)."""
    w, h = size[0] * SS, size[1] * SS
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    cx, cy = w // 2, h // 2
    for x in range(w):
        t = abs(x - cx) / cx
        alpha = round(255 * max(0.0, 1.0 - t) ** 0.7)
        draw.line([(x, cy - SS), (x, cy + SS)], fill=GOLD + (alpha,))
    d = h // 2 - SS
    draw.polygon([(cx, cy - d), (cx + d, cy), (cx, cy + d), (cx - d, cy)], fill=GOLD_LIGHT + (255,), outline=GOLD_DARK + (255,))
    for side in (-1, 1):
        x = cx + side * 6 * d
        s = d // 2
        draw.polygon([(x, cy - s), (x + s, cy), (x, cy + s), (x - s, cy)], fill=GOLD + (255,))
    return image.resize(size, Image.LANCZOS)


def diamond(size, color, outline):
    w, h = size[0] * SS, size[1] * SS
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    m = SS
    ImageDraw.Draw(image).polygon([(w // 2, m), (w - m, h // 2), (w // 2, h - m), (m, h // 2)], fill=color + (255,),
                                  outline=outline + (255,), width=SS)
    return image.resize(size, Image.LANCZOS)


def art_slot(size):
    """A grey landscape (sky, two hill lines, a light behind them), tinted per tile until painted art replaces it."""
    w, h = size[0] * SS, size[1] * SS
    image = gradient((w, h), (205, 205, 205), (120, 120, 120))
    draw = ImageDraw.Draw(image)
    sun = Image.new("L", (w, h), 0)
    ImageDraw.Draw(sun).ellipse([w * 0.55, h * 0.12, w * 0.85, h * 0.42], fill=255)
    image.paste((250, 250, 250, 255), mask=sun.filter(ImageFilter.GaussianBlur(10 * SS)))
    for base, height, shade, waves in ((0.62, 0.18, 150, 3), (0.78, 0.14, 95, 5)):
        points = [(0, h)]
        for i in range(0, 65):
            x = w * i / 64
            y = h * base - h * height * (0.5 + 0.5 * math.sin(i / 64 * 6.283 * waves / 2 + base * 7))
            points.append((x, y))
        points.append((w, h))
        draw.polygon(points, fill=(shade, shade, shade, 255))
    vignette = ImageChops.subtract(Image.new("L", (w, h), 255), rounded_mask((w, h), 18 * SS, 6 * SS).filter(
        ImageFilter.GaussianBlur(14 * SS)))
    image = Image.composite(Image.new("RGBA", (w, h), (30, 30, 30, 255)), image, vignette.point(lambda v: v * 3 // 4))
    return image.resize(size, Image.LANCZOS)


def medallion(size):
    w, h = size[0] * SS, size[1] * SS
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    halo = Image.new("L", (w, h), 0)
    ImageDraw.Draw(halo).ellipse([4 * SS, 4 * SS, w - 4 * SS, h - 4 * SS], fill=180)
    image.paste(GOLD_LIGHT + (255,), mask=halo.filter(ImageFilter.GaussianBlur(4 * SS)))
    draw = ImageDraw.Draw(image)
    draw.ellipse([8 * SS, 8 * SS, w - 8 * SS, h - 8 * SS], fill=GOLD_DARK + (255,))
    draw.ellipse([10 * SS, 10 * SS, w - 10 * SS, h - 10 * SS], fill=GOLD + (255,))
    draw.ellipse([15 * SS, 15 * SS, w - 15 * SS, h - 15 * SS], fill=EDGE + (255,))
    inner = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(inner).ellipse([17 * SS, 17 * SS, w - 17 * SS, h - 17 * SS], fill=(255, 255, 255, 255))
    fill = gradient((w, h), (52, 66, 120), (14, 18, 42))
    fill.putalpha(inner.getchannel("A"))
    image.alpha_composite(fill)
    return image.resize(size, Image.LANCZOS)


def check(size):
    w, h = size[0] * SS, size[1] * SS
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([SS, SS, w - SS, h - SS], fill=(26, 70, 30, 255))
    draw.ellipse([3 * SS, 3 * SS, w - 3 * SS, h - 3 * SS], fill=(78, 168, 72, 255), outline=GREEN + (255,), width=SS)
    draw.line([(w * 0.28, h * 0.52), (w * 0.44, h * 0.68), (w * 0.73, h * 0.34)], fill=(255, 255, 255, 255), width=4 * SS,
              joint="curve")
    return image.resize(size, Image.LANCZOS)


def lock(size):
    w, h = size[0] * SS, size[1] * SS
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([2 * SS, 2 * SS, w - 2 * SS, h - 2 * SS], fill=(14, 16, 30, 230), outline=GOLD_DARK + (255,), width=SS)
    draw.arc([w * 0.33, h * 0.20, w * 0.67, h * 0.58], 180, 360, fill=GOLD_LIGHT + (255,), width=3 * SS)
    draw.line([(w * 0.33, h * 0.39), (w * 0.33, h * 0.48)], fill=GOLD_LIGHT + (255,), width=3 * SS)
    draw.line([(w * 0.67, h * 0.39), (w * 0.67, h * 0.48)], fill=GOLD_LIGHT + (255,), width=3 * SS)
    draw.rounded_rectangle([w * 0.26, h * 0.46, w * 0.74, h * 0.78], radius=2 * SS, fill=GOLD + (255,))
    draw.ellipse([w * 0.46, h * 0.56, w * 0.54, h * 0.64], fill=EDGE + (255,))
    return image.resize(size, Image.LANCZOS)


def canvas(size):
    return Image.new("RGBA", (size[0] * SS, size[1] * SS), (0, 0, 0, 0))


def finish(image, size):
    """Scaled down with a dark outline so icons read on navy and on parchment alike."""
    alpha = image.getchannel("A")
    outline = Image.new("RGBA", image.size, EDGE + (0,))
    outline.putalpha(alpha.filter(ImageFilter.MaxFilter(2 * SS + 1)).point(lambda v: min(255, v * 2)))
    outline.alpha_composite(image)
    return outline.resize(size, Image.LANCZOS)


def icon_people(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    for cx, scale, color in ((0.36, 0.85, GOLD_DARK), (0.6, 1.0, GOLD_LIGHT)):
        r = w * 0.12 * scale
        cy = h * 0.32
        draw.ellipse([w * cx - r, cy - r, w * cx + r, cy + r], fill=color + (255,))
        body = w * 0.22 * scale
        draw.pieslice([w * cx - body, h * 0.52, w * cx + body, h * 0.52 + body * 2.4], 180, 360, fill=color + (255,))
    return finish(image, size)


def icon_tent(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.polygon([(w * 0.5, h * 0.12), (w * 0.92, h * 0.86), (w * 0.08, h * 0.86)], fill=GOLD + (255,))
    draw.polygon([(w * 0.5, h * 0.12), (w * 0.92, h * 0.86), (w * 0.62, h * 0.86)], fill=GOLD_DARK + (255,))
    draw.polygon([(w * 0.5, h * 0.48), (w * 0.62, h * 0.86), (w * 0.38, h * 0.86)], fill=EDGE + (255,))
    return finish(image, size)


def icon_food(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.ellipse([w * 0.16, h * 0.28, w * 0.84, h * 0.92], fill=(198, 52, 44, 255))
    draw.ellipse([w * 0.28, h * 0.36, w * 0.44, h * 0.52], fill=(240, 140, 120, 255))
    draw.line([(w * 0.5, h * 0.32), (w * 0.54, h * 0.1)], fill=(110, 70, 30, 255), width=2 * SS)
    draw.ellipse([w * 0.54, h * 0.08, w * 0.84, h * 0.26], fill=(110, 190, 80, 255))
    return finish(image, size)


def icon_wood(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    log = Image.new("RGBA", image.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(log)
    d.rounded_rectangle([w * 0.12, h * 0.36, w * 0.88, h * 0.64], radius=h * 0.14, fill=(140, 92, 50, 255))
    d.line([(w * 0.2, h * 0.44), (w * 0.7, h * 0.44)], fill=(100, 64, 34, 255), width=SS)
    d.line([(w * 0.25, h * 0.55), (w * 0.75, h * 0.55)], fill=(100, 64, 34, 255), width=SS)
    d.ellipse([w * 0.7, h * 0.36, w * 0.88, h * 0.64], fill=(222, 182, 120, 255), outline=(120, 80, 40, 255), width=SS)
    image.alpha_composite(log.rotate(30, resample=Image.BICUBIC, center=(w / 2, h / 2)))
    del draw
    return finish(image, size)


def icon_house(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.polygon([(w * 0.5, h * 0.1), (w * 0.94, h * 0.48), (w * 0.06, h * 0.48)], fill=GOLD_DARK + (255,))
    draw.rectangle([w * 0.18, h * 0.46, w * 0.82, h * 0.9], fill=GOLD + (255,))
    draw.rectangle([w * 0.42, h * 0.6, w * 0.58, h * 0.9], fill=EDGE + (255,))
    return finish(image, size)


def icon_hourglass(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.rectangle([w * 0.2, h * 0.08, w * 0.8, h * 0.16], fill=GOLD + (255,))
    draw.rectangle([w * 0.2, h * 0.84, w * 0.8, h * 0.92], fill=GOLD + (255,))
    draw.polygon([(w * 0.26, h * 0.16), (w * 0.74, h * 0.16), (w * 0.5, h * 0.5), (w * 0.74, h * 0.84), (w * 0.26, h * 0.84),
                  (w * 0.5, h * 0.5)], fill=(200, 210, 230, 200), outline=GOLD_LIGHT + (255,), width=SS)
    draw.polygon([(w * 0.34, h * 0.84), (w * 0.66, h * 0.84), (w * 0.5, h * 0.62)], fill=GOLD_LIGHT + (255,))
    return finish(image, size)


def icon_fire(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.line([(w * 0.18, h * 0.88), (w * 0.82, h * 0.74)], fill=(120, 76, 40, 255), width=4 * SS)
    draw.line([(w * 0.18, h * 0.74), (w * 0.82, h * 0.88)], fill=(150, 96, 52, 255), width=4 * SS)
    for scale, color in ((1.0, (230, 90, 30)), (0.7, (250, 170, 40)), (0.4, (255, 236, 150))):
        top = h * (0.78 - 0.66 * scale)
        draw.polygon([(w * 0.5, top), (w * (0.5 + 0.26 * scale), h * 0.64), (w * 0.5, h * 0.8),
                      (w * (0.5 - 0.26 * scale), h * 0.64)], fill=color + (255,))
        draw.ellipse([w * (0.5 - 0.24 * scale), h * (0.8 - 0.36 * scale), w * (0.5 + 0.24 * scale), h * 0.8], fill=color + (255,))
    return finish(image, size)


def icon_trophy(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.arc([w * 0.06, h * 0.16, w * 0.4, h * 0.5], 90, 270, fill=GOLD + (255,), width=3 * SS)
    draw.arc([w * 0.6, h * 0.16, w * 0.94, h * 0.5], 270, 90, fill=GOLD + (255,), width=3 * SS)
    draw.pieslice([w * 0.22, h * -0.08, w * 0.78, h * 0.62], 0, 180, fill=GOLD_LIGHT + (255,))
    draw.rectangle([w * 0.44, h * 0.6, w * 0.56, h * 0.76], fill=GOLD + (255,))
    draw.rectangle([w * 0.28, h * 0.76, w * 0.72, h * 0.9], fill=GOLD_DARK + (255,))
    return finish(image, size)


def icon_leaf(size=(40, 40)):
    image = canvas(size)
    w, h = image.size
    leaf = Image.new("RGBA", image.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(leaf)
    d.ellipse([w * 0.3, h * 0.08, w * 0.7, h * 0.86], fill=(110, 196, 80, 255))
    d.line([(w * 0.5, h * 0.14), (w * 0.5, h * 0.96)], fill=(50, 110, 40, 255), width=2 * SS)
    image.alpha_composite(leaf.rotate(-35, resample=Image.BICUBIC, center=(w / 2, h / 2)))
    return finish(image, size)


def icon_drop(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.polygon([(w * 0.5, h * 0.06), (w * 0.78, h * 0.56), (w * 0.22, h * 0.56)], fill=(70, 150, 235, 255))
    draw.ellipse([w * 0.22, h * 0.36, w * 0.78, h * 0.92], fill=(70, 150, 235, 255))
    draw.ellipse([w * 0.34, h * 0.56, w * 0.46, h * 0.72], fill=(200, 235, 255, 255))
    return finish(image, size)


def icon_sprout(size=(40, 40)):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    w, h = image.size
    draw.pieslice([w * 0.1, h * 0.66, w * 0.9, h * 1.1], 180, 360, fill=(120, 82, 50, 255))
    draw.line([(w * 0.5, h * 0.7), (w * 0.5, h * 0.36)], fill=(70, 150, 60, 255), width=2 * SS)
    draw.ellipse([w * 0.14, h * 0.22, w * 0.5, h * 0.46], fill=(110, 200, 80, 255))
    draw.ellipse([w * 0.5, h * 0.14, w * 0.86, h * 0.4], fill=(140, 220, 100, 255))
    return finish(image, size)


def brush_pictures():
    """Brush<TileId>: 2 x 2 of a type's first texture variants from the terrain atlas (one row per id), rounded, with
    a raised edge for rock (Raise) and a sunk one for the riverbed (Dig)."""
    atlas_path = os.path.join(ROOT, "Project", "Assets", "Textures", "TerrainAtlas.png")
    pictures = {}
    if os.path.exists(atlas_path) == False:
        return pictures
    atlas = Image.open(atlas_path).convert("RGBA")
    tile = 32
    for tile_id in range(min(atlas.height // tile, 16)):
        row = atlas.crop((0, tile_id * tile, atlas.width, (tile_id + 1) * tile))
        if row.getchannel("A").getbbox() is None:
            continue
        picture = Image.new("RGBA", (64, 64))
        for i in range(4):
            picture.paste(row.crop((i * tile, 0, (i + 1) * tile, tile)), ((i % 2) * tile, (i // 2) * tile))
        shade = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        d = ImageDraw.Draw(shade)
        if tile_id == 6:
            d.rectangle([0, 0, 63, 5], fill=(255, 255, 255, 70))
            d.rectangle([0, 58, 63, 63], fill=(0, 0, 0, 90))
        elif tile_id == 5:
            d.rectangle([0, 0, 63, 7], fill=(0, 0, 0, 110))
            d.rectangle([0, 0, 7, 63], fill=(0, 0, 0, 80))
        picture.alpha_composite(shade)
        picture.putalpha(ImageChops.multiply(picture.getchannel("A"), rounded_mask((64, 64), 8)))
        pictures["Brush%d" % tile_id] = picture
    return pictures


def sprites():
    """Name: image, in atlas order."""
    pill = 30
    return {
        "Panel": frame((96, 96), 10, NAVY_TOP, NAVY_BOTTOM, gold_rims(2), 238, inner_line=(8, GOLD, 90)),
        "Tile": frame((96, 96), 12, TILE_TOP, TILE_BOTTOM, gold_rims(3), 250),
        "TileGreen": frame((96, 96), 12, (30, 58, 40), (12, 30, 18), [(0, 1, EDGE), (1, 1, (60, 110, 40)), (2, 3, GREEN),
                                                                     (2, 1, (220, 250, 200)), (5, 1, EDGE)], 250),
        "Select": frame((96, 96), 14, (0, 0, 0), (0, 0, 0), [(4, 2, CYAN), (4, 1, (230, 250, 255))], 0, halo=CYAN),
        "Parchment": frame((128, 128), 12, PARCHMENT_TOP, PARCHMENT_BOTTOM,
                           [(0, 1, EDGE), (1, 2, PARCHMENT_RIM), (3, 2, GOLD), (5, 1, PARCHMENT_RIM)], 255,
                           inner_line=(10, PARCHMENT_RIM, 110)),
        "ParchmentHeader": frame((96, 48), 8, (204, 186, 146), (186, 164, 120), [(0, 1, PARCHMENT_RIM)], 255),
        "ButtonPurple": frame((96, 48), 14, PURPLE_TOP, PURPLE_BOTTOM, gold_rims(2), 255),
        "ButtonDark": frame((96, 48), 10, (34, 40, 70), (18, 22, 44), [(0, 1, EDGE), (1, 1, GOLD_DARK), (2, 1, GOLD),
                                                                       (3, 1, EDGE)], 250),
        "ButtonBlue": frame((96, 48), 10, BLUE_TOP, BLUE_BOTTOM, [(0, 1, EDGE), (1, 2, CYAN), (1, 1, (230, 250, 255)),
                                                                  (3, 1, EDGE)], 255),
        "NodeLocked": frame((96, 64), pill, (32, 34, 46), (18, 19, 28), [(0, 1, EDGE), (1, 2, STONE), (3, 1, EDGE)], 245),
        "NodeOpen": frame((96, 64), pill, (34, 54, 92), (16, 26, 52), [(0, 1, EDGE), (1, 2, CYAN), (1, 1, (230, 250, 255)),
                                                                       (3, 1, EDGE)], 250),
        "NodeOwned": frame((96, 64), pill, (92, 70, 30), (46, 32, 12), gold_rims(3), 255),
        "NodeSelect": frame((96, 64), pill + 4, (0, 0, 0), (0, 0, 0), [(4, 2, CYAN), (4, 1, (230, 250, 255))], 0, halo=CYAN),
        "Ribbon": frame((128, 48), 6, PURPLE_TOP, PURPLE_BOTTOM, gold_rims(2), 255),
        "BarTrack": frame((32, 16), 7, (8, 10, 20), (20, 24, 40), [(0, 1, GOLD_DARK)], 255),
        "BarFill": frame((32, 16), 7, (140, 226, 255), (46, 120, 220), [], 255),
        "BarFillGold": frame((32, 16), 7, GOLD_LIGHT, GOLD_DARK, [], 255),
        "Ornament": ornament((512, 24)),
        "Diamond": diamond((24, 24), GOLD, GOLD_DARK),
        "DiamondBlue": diamond((24, 24), CYAN, (40, 90, 160)),
        "ArtSlot": art_slot((192, 128)),
        "Medallion": medallion((128, 128)),
        "Check": check((40, 40)),
        "Lock": lock((40, 40)),
        "IconPeople": icon_people(),
        "IconTent": icon_tent(),
        "IconFood": icon_food(),
        "IconWood": icon_wood(),
        "IconHouse": icon_house(),
        "IconHourglass": icon_hourglass(),
        "IconFire": icon_fire(),
        "IconTrophy": icon_trophy(),
        "IconLeaf": icon_leaf(),
        "IconDrop": icon_drop(),
        "IconSprout": icon_sprout(),
    } | brush_pictures()


def pack(images, width):
    """Shelves left to right, top to bottom; returns {name: (x, y)} and the atlas height (a power of two)."""
    places = {}
    x, y, row = GUTTER, GUTTER, 0
    for name, image in sorted(images.items(), key=lambda item: -item[1].height):
        if x + image.width + GUTTER > width:
            x, y, row = GUTTER, y + row + GUTTER, 0
        places[name] = (x, y)
        x += image.width + GUTTER
        row = max(row, image.height)
    height = 64
    while height < y + row + GUTTER:
        height *= 2
    return places, height


def kept_handle(meta_path):
    if os.path.exists(meta_path):
        match = re.search(r"Handle:\s*(\d+)", open(meta_path).read())
        if match:
            return int(match.group(1))
    return int.from_bytes(hashlib.sha1(("Hearthlands/" + NAME).encode()).digest()[:8], "little") & 0x7FFFFFFFFFFFFFFF


def main():
    images = sprites()
    width = 1024
    places, height = pack(images, width)
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for name, image in images.items():
        atlas.alpha_composite(image, places[name])
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, NAME + ".png")
    atlas.save(path)
    meta_path = path + ".hymeta"
    lines = ["Handle: %d" % kept_handle(meta_path), "Type: Texture2D", "TextureImport:", "  Format: SRGB", "  Filter: Linear",
             "  Wrap: Clamp", "  SpriteMode: Multiple", "  PixelsPerUnit: 100", "  Slicing:", "    Mode: Manual",
             "    CellSize: [32, 32]", "    Offset: [0, 0]", "    Spacing: [0, 0]", "    Columns: 0", "    Rows: 0",
             "    RowNames: []", "    DefaultPivot: [0.5, 0.5]", "  Sprites:"]
    for name in images:
        x, y = places[name]
        lines.append("    - {Name: %s, Rect: [%d, %d, %d, %d], Pivot: [0.5, 0.5]}" % (name, x, y, images[name].width,
                                                                                    images[name].height))
    with open(meta_path, "w", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote", path, "%d x %d," % (width, height), len(images), "sprites")
    if "--preview" in sys.argv:
        preview = Image.new("RGBA", atlas.size, (60, 70, 60, 255))
        preview.alpha_composite(atlas)
        os.makedirs(os.path.join(ROOT, "scripts", "art", "out"), exist_ok=True)
        preview.save(os.path.join(ROOT, "scripts", "art", "out", "menu_art_preview.png"))


if __name__ == "__main__":
    main()
