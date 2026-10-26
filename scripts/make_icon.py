"""Generate the Qingmo application icons.

Design: a white ink drop next to a cyan text caret on a dark rounded square.
Everything is drawn at 1024x1024 and downsampled by Pillow, so the small
sizes stay readable.
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "resources"
WIN32 = ROOT / "src" / "win32"

SIZE = 1024
INK_TOP = (9, 16, 29)
INK_BOTTOM = (24, 48, 76)
CYAN = (34, 211, 238, 255)
PAPER = (248, 250, 252, 255)


def bezier(p0, p1, p2, p3, steps=140):
    points = []
    for i in range(steps + 1):
        t = i / steps
        mt = 1.0 - t
        x = mt**3 * p0[0] + 3 * mt**2 * t * p1[0] + 3 * mt * t**2 * p2[0] + t**3 * p3[0]
        y = mt**3 * p0[1] + 3 * mt**2 * t * p1[1] + 3 * mt * t**2 * p2[1] + t**3 * p3[1]
        points.append((x, y))
    return points


def rounded_mask():
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle([46, 46, SIZE - 46, SIZE - 46], radius=232, fill=255)
    return mask


def background():
    gradient = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(gradient)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        color = tuple(round(a + (b - a) * t) for a, b in zip(INK_TOP, INK_BOTTOM))
        draw.line([(0, y), (SIZE, y)], fill=color + (255,))

    mask = rounded_mask()
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    canvas.paste(gradient, (0, 0), mask)

    # Soft cyan glow behind the ink drop.
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([170, 210, 760, 800], fill=(34, 211, 238, 34))
    glow = glow.filter(ImageFilter.GaussianBlur(95))
    glow_alpha = Image.composite(glow.getchannel("A"), Image.new("L", (SIZE, SIZE), 0), mask)
    glow.putalpha(glow_alpha)
    canvas = Image.alpha_composite(canvas, glow)

    # Thin rim.
    ImageDraw.Draw(canvas).rounded_rectangle(
        [46, 46, SIZE - 46, SIZE - 46],
        radius=232,
        outline=(125, 211, 252, 46),
        width=9,
    )
    return canvas


def drop_points():
    tip = (424, 196)
    left = bezier(tip, (330, 360), (290, 500), (290, 590))

    # Round bottom: a true ink drop, not a leaf.
    center = (424, 590)
    radius = 134
    arc = []
    for i in range(91):
        theta = math.radians(180 - i * 2)
        arc.append((center[0] + radius * math.cos(theta),
                    center[1] + radius * math.sin(theta)))

    right = bezier((558, 590), (558, 500), (518, 360), tip)
    return left + arc[1:] + right[1:]


def draw_drop(canvas):
    points = drop_points()

    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).polygon([(x, y + 26) for x, y in points], fill=(0, 0, 0, 105))
    shadow = shadow.filter(ImageFilter.GaussianBlur(34))
    canvas = Image.alpha_composite(canvas, shadow)

    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.polygon(points, fill=PAPER)

    # A thin inner rim keeps the drop crisp without a muddy highlight.
    rim = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(rim).line(points + [points[0]], fill=(255, 255, 255, 90), width=3, joint="curve")
    layer = Image.alpha_composite(layer, rim)

    return Image.alpha_composite(canvas, layer)


def caret(canvas):
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    # Vertical stem.
    d.rounded_rectangle([658, 322, 712, 706], radius=27, fill=CYAN)
    # Top and bottom caps make it read as a text cursor / I-beam.
    d.rounded_rectangle([594, 322, 776, 378], radius=26, fill=CYAN)
    d.rounded_rectangle([594, 650, 776, 706], radius=26, fill=CYAN)
    # A tiny white glint on the stem.
    d.rounded_rectangle([674, 372, 686, 500], radius=6, fill=(224, 251, 255, 120))
    return Image.alpha_composite(canvas, layer)


def spinner(canvas):
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.arc([596, 322, 800, 526], start=28, end=322, fill=CYAN, width=30)
    d.ellipse([770, 384, 826, 440], fill=CYAN)
    return Image.alpha_composite(canvas, layer)


def save_ico(image, path, sizes=(16, 20, 24, 32, 40, 48, 64, 128, 256)):
    image.save(path, format="ICO", sizes=[(s, s) for s in sizes])


def main():
    RES.mkdir(exist_ok=True)
    WIN32.mkdir(exist_ok=True)

    base = draw_drop(background())
    main_icon = caret(base)
    busy_icon = spinner(base)

    main_icon.resize((512, 512), Image.Resampling.LANCZOS).save(RES / "QingMo-512.png")
    main_icon.resize((256, 256), Image.Resampling.LANCZOS).save(RES / "QingMo-256.png")
    main_icon.resize((128, 128), Image.Resampling.LANCZOS).save(RES / "QingMo-128.png")

    save_ico(main_icon, WIN32 / "QingMo.ico")
    save_ico(busy_icon, WIN32 / "QingMoBusy.ico")

    print("Wrote", WIN32 / "QingMo.ico")
    print("Wrote", WIN32 / "QingMoBusy.ico")
    print("Wrote previews in", RES)


if __name__ == "__main__":
    main()
