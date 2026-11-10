# Makes a smooth AngelCode BMFont (text .fnt + one page PNG) for Hydro's runtime UI from a TTF, printable ASCII only.
# Writes Project/Assets/Fonts/<Name>.fnt and <Name>_0.png (+ .hymeta with fixed handles, the page filtered Linear).
#   py scripts/fonts/make_font.py [ttf] [size]   (default: Hydro's Oxygen/assets/Fonts/opensans/OpenSans-SemiBold.ttf, 26)
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
HYDRO = os.environ.get("HYDRO_DIR", os.path.join(ROOT, "..", "Hydro"))
DEFAULT_TTF = os.path.join(HYDRO, "Oxygen", "assets", "Fonts", "opensans", "OpenSans-SemiBold.ttf")
NAME = "OpenSans"
FONT_HANDLE = 7311820458392716001  # fixed, so scenes and scripts can name the font
PAGE_HANDLE = 7311820458392716002
PAGE = 256
PADDING = 2


def make(ttf, size):
    font = ImageFont.truetype(ttf, size)
    ascent, descent = font.getmetrics()
    page = Image.new("RGBA", (PAGE, PAGE), (255, 255, 255, 0))
    draw = ImageDraw.Draw(page)
    x, y, row = PADDING, PADDING, 0
    chars = []
    for code in range(32, 127):
        ch = chr(code)
        left, top, right, bottom = font.getbbox(ch, anchor="la")
        width, height = max(right - left, 0), max(bottom - top, 0)
        if x + width + PADDING > PAGE:
            x, y, row = PADDING, y + row + PADDING, 0
        if y + height + PADDING > PAGE:
            raise SystemExit("make_font: the glyphs don't fit one %d x %d page; use a smaller size" % (PAGE, PAGE))
        if width > 0 and height > 0:
            draw.text((x - left, y - top), ch, font=font, fill=(255, 255, 255, 255), anchor="la")
        chars.append((code, x, y, width, height, left, top, round(font.getlength(ch))))
        x += width + PADDING
        row = max(row, height)

    out = os.path.join(ROOT, "Project", "Assets", "Fonts")
    os.makedirs(out, exist_ok=True)
    page_file = "%s_0.png" % NAME
    page.save(os.path.join(out, page_file))
    lines = [
        'info face="Open Sans SemiBold" size=%d bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=%d,%d'
        % (size, PADDING, PADDING),
        "common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=1 packed=0" % (ascent + descent, ascent, PAGE, PAGE),
        'page id=0 file="%s"' % page_file,
        "chars count=%d" % len(chars),
    ]
    for code, cx, cy, width, height, left, top, advance in chars:
        lines.append("char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15"
                     % (code, cx, cy, width, height, left, top, advance))
    with open(os.path.join(out, NAME + ".fnt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, NAME + ".fnt.hymeta"), "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Font\n" % FONT_HANDLE)
    with open(os.path.join(out, page_file + ".hymeta"), "w", newline="\r\n") as f:
        f.write("Handle: %d\nType: Texture2D\nTextureImport:\n  Format: RGBA\n  Filter: Linear\n  Wrap: Clamp\n"
                "  SpriteMode: Single\n  PixelsPerUnit: 100\n" % PAGE_HANDLE)
    print("wrote", os.path.join(out, NAME + ".fnt"), "line height", ascent + descent)


if __name__ == "__main__":
    make(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TTF, int(sys.argv[2]) if len(sys.argv) > 2 else 26)
