"""
Generate tab backgrounds that match V3Card's exact palette + add a discreet
Spring Festival accent (a stylized gold seal stamp).

Color sampling from card_med_idle.png / card_med_active.png:
  IDLE   gradient:  #fcf4e9 (top)  ->  #f5e2c8 (bottom)   = cream paper
  ACTIVE gradient:  #2d9f7d (top)  ->  #067167 (bottom)   = jade green

Spring accent: a tiny abstracted seal-style "fu" mark (square frame + cross
inside, plus four corner ticks). Placed slightly right-of-center so it reads
as a "stamp pressed into the corner" rather than a logo.

Sizes: 512x184. 9-grid 32/32/448/120 keeps corners crisp at any tab size.
"""
import os
from PIL import Image, ImageDraw, ImageFilter

OUT_DIR_ROOT = r"F:\work\mmorpg\client\fairygui\qdao\assets\qdao"
OUT_DIR_V3   = r"F:\work\mmorpg\client\fairygui\qdao\assets\qdao\v3\ui"

W, H = 512, 184
R = 24
BORDER_OUTER = 2
BORDER_INNER = 1
INNER_INSET = 6


# ── helpers ───────────────────────────────────────────────────────

def lerp_color(c1, c2, t):
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(len(c1)))


def vertical_gradient(size, top, bottom):
    w, h = size
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = lerp_color(top, bottom, t)
        for x in range(w):
            px[x, y] = c
    return img


def rounded_mask(size, r):
    w, h = size
    s = 4
    m = Image.new("L", (w * s, h * s), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle((0, 0, w * s - 1, h * s - 1), radius=r * s, fill=255)
    return m.resize((w, h), Image.LANCZOS)


def stroke_rounded(draw, box, r, color, width):
    x0, y0, x1, y1 = box
    for i in range(width):
        draw.rounded_rectangle((x0 + i, y0 + i, x1 - i, y1 - i),
                               radius=max(1, r - i), outline=color, width=1)


def add_paper_noise(img, mask, intensity=4):
    """Very subtle paper grain — V3Card looks smooth so we keep this gentle."""
    import random
    random.seed(7)
    w, h = img.size
    noise = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    pn = noise.load()
    for y in range(h):
        for x in range(w):
            if mask.getpixel((x, y)) > 0:
                v = random.randint(-intensity, intensity)
                a = max(0, min(255, abs(v) * 3))
                if v < 0:
                    pn[x, y] = (60, 40, 25, a)
                else:
                    pn[x, y] = (255, 245, 215, a // 2)
    img.alpha_composite(noise)


def add_top_highlight(img, mask, alpha=40, blur=10):
    w, h = img.size
    hi = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(hi)
    d.rounded_rectangle((10, 8, w - 10, h // 2 + 4),
                        radius=R - 4, fill=(255, 250, 220, alpha))
    hi = hi.filter(ImageFilter.GaussianBlur(blur))
    r, g, b, a = hi.split()
    new_a = Image.new("L", img.size, 0)
    pm = mask.load(); pa = a.load(); pn = new_a.load()
    for y in range(h):
        for x in range(w):
            pn[x, y] = (pa[x, y] * pm[x, y]) // 255
    hi = Image.merge("RGBA", (r, g, b, new_a))
    img.alpha_composite(hi)


def draw_seal(canvas, cx, cy, size, color, line_w=3):
    """
    Stylized abstract seal — 'fu'-like geometry without trying to be calligraphic.
    Frame: square outline.
    Inside: vertical bar + horizontal bar (cross), four small ticks at corners.
    Drawn supersampled then downscaled for clean lines.
    """
    s = size
    SS = 3  # supersample factor
    layer = Image.new("RGBA", (s * 2 * SS, s * 2 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx2 = cy2 = s * SS  # local center
    half = int(s * 0.85 * SS)
    lw = line_w * SS

    # outer square frame
    d.rectangle((cx2 - half, cy2 - half, cx2 + half, cy2 + half),
                outline=color, width=lw)
    # inner cross (slightly inset)
    inset = int(half * 0.25)
    d.line((cx2, cy2 - half + inset, cx2, cy2 + half - inset),
           fill=color, width=lw)
    d.line((cx2 - half + inset, cy2, cx2 + half - inset, cy2),
           fill=color, width=lw)
    # four corner ticks (decorative)
    tick = int(half * 0.18)
    pad = int(half * 0.32)
    for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        x = cx2 + dx * pad
        y = cy2 + dy * pad
        d.line((x - tick, y, x + tick, y), fill=color, width=max(1, lw - 1))
        d.line((x, y - tick, x, y + tick), fill=color, width=max(1, lw - 1))

    # downscale with LANCZOS for AA
    layer = layer.resize((s * 2, s * 2), Image.LANCZOS)
    canvas.alpha_composite(layer, (cx - s, cy - s))


# ── builders ──────────────────────────────────────────────────────

def build_idle():
    """
    V3Card-matched cream body + soft gold seal watermark on the right side.
    """
    mask = rounded_mask((W, H), R)
    # exact V3Card sample
    body = vertical_gradient((W, H), (252, 244, 233), (245, 226, 200))
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(body, (0, 0), mask)

    add_paper_noise(img, mask, intensity=3)
    add_top_highlight(img, mask, alpha=30, blur=12)

    d = ImageDraw.Draw(img)
    # warm gold thin border (subtle so it doesn't shout — Spring Festival hint
    # without breaking V3Card's quiet feel)
    stroke_rounded(d, (0, 0, W - 1, H - 1), R, (200, 160, 90, 200), BORDER_OUTER)
    # very faint inner hairline
    stroke_rounded(d,
                   (INNER_INSET, INNER_INSET, W - 1 - INNER_INSET, H - 1 - INNER_INSET),
                   max(2, R - INNER_INSET), (140, 100, 50, 70), BORDER_INNER)

    # central seal watermark — pale gold so it reads as a "pressed mark"
    # in the cream paper, not a logo. Slightly right-of-center for asymmetry.
    seal_cx = W * 70 // 100
    seal_cy = H // 2
    draw_seal(img, seal_cx, seal_cy, size=28,
              color=(200, 160, 90, 70), line_w=3)

    return img


def build_active():
    """
    V3Card-matched jade body + visible gold seal stamp + gold border.
    Selection cue = green tone (already a strong contrast vs idle), gold border,
    and the seal is now clearly visible in bright gold.
    """
    mask = rounded_mask((W, H), R)
    # exact V3Card sample
    body = vertical_gradient((W, H), (45, 159, 125), (6, 113, 103))
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(body, (0, 0), mask)

    add_paper_noise(img, mask, intensity=4)
    add_top_highlight(img, mask, alpha=50, blur=14)

    d = ImageDraw.Draw(img)
    # bright gold border — Spring Festival "trim" without going red
    stroke_rounded(d, (0, 0, W - 1, H - 1), R, (255, 220, 130, 240), BORDER_OUTER + 1)
    # inner gold hairline
    stroke_rounded(d,
                   (INNER_INSET, INNER_INSET, W - 1 - INNER_INSET, H - 1 - INNER_INSET),
                   max(2, R - INNER_INSET), (255, 240, 189, 130), BORDER_INNER)

    # gold seal — fully visible against jade ground, this is the festive stamp
    seal_cx = W * 70 // 100
    seal_cy = H // 2
    draw_seal(img, seal_cx, seal_cy, size=28,
              color=(255, 220, 130, 230), line_w=3)

    return img


# ── write outputs ─────────────────────────────────────────────────

def save(img, name):
    for d in (OUT_DIR_ROOT, OUT_DIR_V3):
        path = os.path.join(d, name)
        img.save(path, "PNG", optimize=True)
        print(f"  wrote {path}  ({os.path.getsize(path)} bytes)")


def main():
    print("Building list_idle.png ...")
    save(build_idle(), "list_idle.png")
    print("Building list_active.png ...")
    save(build_active(), "list_active.png")
    print(f"\nSize: {W}x{H}, corner radius: {R}px")
    print('9-grid: scale9grid="32,32,448,120"')
    print()
    print("Idle  : V3Card cream  (#fcf4e9 -> #f5e2c8)  +  pale gold seal + thin gold border")
    print("Active: V3Card jade   (#2d9f7d -> #067167)  +  bright gold seal + gold border")


if __name__ == "__main__":
    main()
