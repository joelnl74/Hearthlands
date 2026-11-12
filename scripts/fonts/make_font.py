# Makes a smooth AngelCode BMFont (text .fnt + one page PNG) for Hydro's runtime UI from a TTF / WOFF, printable ASCII only.
# Writes Project/Assets/Fonts/<Name>.fnt and <Name>_0.png (+ .hymeta with fixed handles, the page filtered Linear).
#   py scripts/fonts/make_font.py [ttf] [size] [name]   (default: Hydro's OpenSans-SemiBold.ttf, 26, OpenSans)
# The World Menu's serif fonts (Cormorant Garamond Bold, SIL OFL, from the npm package @fontsource/cormorant-garamond):
#   py scripts/fonts/make_font.py cormorant-garamond-latin-700-normal.woff 60 MenuTitle
#   py scripts/fonts/make_font.py cormorant-garamond-latin-700-normal.woff 36 MenuHeading
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
HYDRO = os.environ.get("HYDRO_DIR", os.path.join(ROOT, "..", "Hydro"))
DEFAULT_TTF = os.path.join(HYDRO, "Oxygen", "assets", "Fonts", "opensans", "OpenSans-SemiBold.ttf")
# Name: (font handle, page handle, face); fixed, so scenes and scripts can name the font
FONTS = {
    "OpenSans": (7311820458392716001, 7311820458392716002, "Open Sans SemiBold"),
    "MenuTitle": (7311820458392716011, 7311820458392716012, "Cormorant Garamond Bold"),
    "MenuHeading": (7311820458392716021, 7311820458392716022, "Cormorant Garamond Bold"),
}
PADDING = 2


def pack(font, page_size):
    """The glyphs' places on one page_size page, or None when they don't fit."""
    x, y, row = PADDING, PADDING, 0
    places = []
    for code in range(32, 127):
        left, top, right, bottom = font.getbbox(chr(code), anchor="la")
        width, height = max(right - left, 0), max(bottom - top, 0)
        if x + width + PADDING > page_size:
            x, y, row = PADDING, y + row + PADDING, 0
        if y + height + PADDING > page_size:
            return None
        places.append((code, x, y, width, height, left, top, round(font.getlength(chr(code)))))
        x += width + PADDING
        row = max(row, height)
    return places


def make(ttf, size, name):
    if name not in FONTS:
        raise SystemExit("make_font: unknown name %s (add its handles to FONTS)" % name)
    font_handle, page_handle, face = FONTS[name]
    font = ImageFont.truetype(ttf, size)
    ascent, descent = font.getmetrics()
    page_size = 256
    chars = pack(font, page_size)
    while chars is None and page_size < 2048:
        page_size *= 2
        chars = pack(font, page_size)
    if chars is None:
        raise SystemExit("make_font: the glyphs don't fit one 2048 x 2048 page; use a smaller size")
    page = Image.new("RGBA", (page_size, page_size), (255, 255, 255, 0))
    draw = ImageDraw.Draw(page)
    for code, cx, cy, width, height, left, top, advance in chars:
        if width > 0 and height > 0:
            draw.text((cx - left, cy - top), chr(code), font=font, fill=(255, 255, 255, 255), anchor="la")

    out = os.path.join(ROOT, "Project", "Assets", "Fonts")
    os.makedirs(out, exist_ok=True)
    page_file = "%s_0.png" % name
    page.save(os.path.join(out, page_file))
    lines = [
        'info face="%s" size=%d bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=%d,%d'
        % (face, size, PADDING, PADDING),
        "common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=1 packed=0" % (ascent + descent, ascent, page_size, page_size),
        'page id=0 file="%s"' % page_file,
        "chars count=%d" % len(chars),
    ]
    for code, cx, cy, width, height, left, top, advance in chars:
        lines.append("char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15"
                     % (code, cx, cy, width, height, left, top, advance))
    with open(os.path.join(out, name + ".fnt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, name + ".fnt.hymeta"), "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Font\n" % font_handle)
    with open(os.path.join(out, page_file + ".hymeta"), "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: RGBA\n  Filter: Linear\n  Wrap: Clamp\n"
                "  SpriteMode: Single\n  PixelsPerUnit: 100\n" % page_handle)
    print("wrote", os.path.join(out, name + ".fnt"), "line height", ascent + descent, "page", page_size)


if __name__ == "__main__":
    make(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TTF, int(sys.argv[2]) if len(sys.argv) > 2 else 26,
         sys.argv[3] if len(sys.argv) > 3 else "OpenSans")
