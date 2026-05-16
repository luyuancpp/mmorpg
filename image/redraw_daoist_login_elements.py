from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import math
import json


OUT = Path("q_daoist_login_ui_10240_redraw_clear_final_layers/q_daoist_login_buttons_redrawn_atomic")
OUT.mkdir(parents=True, exist_ok=True)


def rgba(hex_color, a=255):
    hex_color = hex_color.lstrip("#")
    return tuple(int(hex_color[i:i + 2], 16) for i in (0, 2, 4)) + (a,)


def canvas(w, h, scale=3):
    return Image.new("RGBA", (w * scale, h * scale), (0, 0, 0, 0))


def down(img, w, h):
    return img.resize((w, h), Image.Resampling.LANCZOS)


def rounded_rect_mask(size, radius, scale=3):
    w, h = size
    mask = Image.new("L", (w * scale, h * scale), 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle([0, 0, w * scale - 1, h * scale - 1], radius=radius * scale, fill=255)
    return mask


def linear_gradient(size, top, bottom, horizontal=False):
    w, h = size
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            t = x / max(1, w - 1) if horizontal else y / max(1, h - 1)
            pix[x, y] = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(4))
    return img


def radial_gradient(size, inner, outer, center=None, radius_scale=0.75):
    w, h = size
    cx, cy = center or (w * 0.42, h * 0.35)
    maxr = min(w, h) * radius_scale
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            t = min(1.0, math.hypot(x - cx, y - cy) / maxr)
            t = t * t * (3 - 2 * t)
            pix[x, y] = tuple(int(inner[i] * (1 - t) + outer[i] * t) for i in range(4))
    return img


def add_painter_texture(img, seed=1, strength=18):
    w, h = img.size
    tex = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(tex)
    rng = (seed * 1103515245 + 12345) & 0x7fffffff

    def rand():
        nonlocal rng
        rng = (rng * 1103515245 + 12345) & 0x7fffffff
        return rng / 0x7fffffff

    for _ in range(130):
        x = int(rand() * w)
        y = int(rand() * h)
        rw = int((18 + rand() * 90) * w / 360)
        rh = int((8 + rand() * 48) * h / 360)
        col = (255, 255, 255, int(rand() * strength)) if rand() > 0.45 else (75, 45, 20, int(rand() * strength * 0.55))
        d.ellipse([x - rw, y - rh, x + rw, y + rh], fill=col)
    tex = tex.filter(ImageFilter.GaussianBlur(max(1, int(w / 180))))
    img.alpha_composite(tex)


def soften_alpha_edges(img, radius=0.55):
    rgba = img.convert("RGBA")
    alpha = rgba.getchannel("A").filter(ImageFilter.GaussianBlur(radius))
    rgba.putalpha(alpha)
    return rgba


def painted_finish(img, seed=1):
    """Make procedural drawing read less like assembled vectors."""
    w, h = img.size
    rng = (seed * 1664525 + 1013904223) & 0xFFFFFFFF

    def rand():
        nonlocal rng
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        return rng / 0xFFFFFFFF

    # Very slight color bleed only. Too much blur makes these UI icons muddy.
    base = img.filter(ImageFilter.GaussianBlur(0.28))
    img = Image.blend(base, img, 0.30)

    glaze = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glaze)
    for _ in range(90):
        x = int(rand() * w)
        y = int(rand() * h)
        rw = int((10 + rand() * 55) * w / 420)
        rh = int((4 + rand() * 25) * h / 420)
        col = (255, 246, 210, int(4 + rand() * 12)) if rand() > 0.42 else (60, 32, 12, int(rand() * 7))
        gd.ellipse([x - rw, y - rh, x + rw, y + rh], fill=col)
    glaze = glaze.filter(ImageFilter.GaussianBlur(max(1, int(w / 260))))
    img.alpha_composite(glaze)

    # A few hand-brush highlight strokes, intentionally imperfect.
    stroke = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(stroke)
    for _ in range(16):
        x = rand() * w
        y = rand() * h
        pts = []
        for i in range(4):
            pts.append((x + i * (18 + rand() * 18) - rand() * 12, y + math.sin(i + rand()) * 10))
        sd.line(pts, fill=(255, 250, 220, int(10 + rand() * 22)), width=max(1, int(1 + rand() * 3)), joint="curve")
    img.alpha_composite(stroke.filter(ImageFilter.GaussianBlur(0.18)))
    return img.filter(ImageFilter.UnsharpMask(radius=0.8, percent=170, threshold=2))


def paste_masked(base, layer, mask):
    base.alpha_composite(Image.composite(layer, Image.new("RGBA", layer.size, (0, 0, 0, 0)), mask))


def draw_clouds(d, x, y, s=1.0, color=(255, 255, 255, 90), width=5):
    pts = [
        (x, y + 32 * s), (x + 42 * s, y + 32 * s),
        (x + 45 * s, y + 8 * s), (x + 88 * s, y + 8 * s),
        (x + 92 * s, y + 42 * s), (x + 142 * s, y + 42 * s),
    ]
    d.line(pts, fill=color, width=max(1, int(width * s)), joint="curve")
    d.arc([x + 10 * s, y + 4 * s, x + 70 * s, y + 64 * s], 180, 355, fill=color, width=max(1, int(width * s)))
    d.arc([x + 60 * s, y - 14 * s, x + 130 * s, y + 56 * s], 180, 355, fill=color, width=max(1, int(width * s)))


def draw_panel(w, h, filename, selected=False, green=False, radius=58, right_round_cut=True):
    s = 3
    img = canvas(w, h, s)
    mask = rounded_rect_mask((w, h), radius, s)

    if green:
        grad = linear_gradient((w * s, h * s), rgba("#36a781", 250), rgba("#006d66", 252))
    elif selected:
        grad = linear_gradient((w * s, h * s), rgba("#fff9ec", 252), rgba("#fde8ce", 252))
    else:
        grad = linear_gradient((w * s, h * s), rgba("#fffaf1", 250), rgba("#f7dfc5", 248))
    paste_masked(img, grad, mask)

    d = ImageDraw.Draw(img)
    box = [9 * s, 9 * s, w * s - 10 * s, h * s - 10 * s]
    d.rounded_rectangle(box, radius=radius * s, outline=rgba("#fffdf4", 230), width=8 * s)
    d.rounded_rectangle([18 * s, 18 * s, w * s - 19 * s, h * s - 19 * s], radius=(radius - 10) * s,
                        outline=rgba("#d7b06a", 180), width=3 * s)
    d.rounded_rectangle([2 * s, 2 * s, w * s - 3 * s, h * s - 3 * s], radius=radius * s,
                        outline=rgba("#7a5728", 105), width=3 * s)

    for i in range(7):
        yy = int((34 + i * 47) * s)
        d.line([(58 * s, yy), ((w - 62) * s, yy + 18 * s)], fill=rgba("#ffffff", 20), width=2 * s)

    draw_clouds(d, (w - 420) * s, (h - 150) * s, s=2.1, color=rgba("#ffffff", 100), width=4 * s)
    draw_clouds(d, (w - 300) * s, (h - 108) * s, s=1.2, color=rgba("#f8d8aa", 100), width=3 * s)

    if green:
        d.line([(90 * s, (h - 34) * s), ((w - 90) * s, (h - 34) * s)], fill=rgba("#f7d17a", 165), width=4 * s)
        d.arc([(w - 330) * s, (h - 260) * s, (w - 90) * s, (h - 20) * s], 205, 292, fill=rgba("#f4cf7f", 120), width=7 * s)

    img = img.filter(ImageFilter.UnsharpMask(radius=1.5, percent=120))
    down(img, w, h).save(OUT / filename)


def draw_top_button():
    w, h, s = 1391, 441, 3
    img = canvas(w, h, s)
    mask = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(mask)
    poly = [(55, 80), (128, 42), (1260, 42), (1336, 82), (1294, 389), (96, 388)]
    md.polygon([(x * s, y * s) for x, y in poly], fill=255)
    grad = linear_gradient(img.size, rgba("#b6efd5", 252), rgba("#00947d", 252))
    paste_masked(img, grad, mask)
    d = ImageDraw.Draw(img)
    for off, col, wid in [(0, "#6b4b1e", 10), (12, "#ffe2a0", 15), (31, "#fef8de", 5)]:
        pts = [((x + off * (1 if x < w / 2 else -1)) * s, (y + off * (1 if y < h / 2 else -1)) * s) for x, y in poly]
        d.line(pts + [pts[0]], fill=rgba(col, 230), width=wid * s, joint="curve")
    d.line([(310 * s, 252 * s), (1075 * s, 252 * s)], fill=rgba("#fff9d9", 220), width=9 * s)
    d.polygon([(680 * s, 235 * s), (715 * s, 252 * s), (680 * s, 270 * s), (645 * s, 252 * s)],
              fill=rgba("#fff9d9", 220))
    d.ellipse([622 * s, 393 * s, 690 * s, 461 * s], fill=rgba("#f5c55c", 245), outline=rgba("#9d6f22", 180), width=5 * s)
    down(img.filter(ImageFilter.UnsharpMask(radius=1.4, percent=110)), w, h).save(OUT / "top_button_green_selected.png")


def draw_search_box():
    w, h, s = 1428, 285, 3
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([28 * s, 45 * s, (w - 32) * s, (h - 54) * s], radius=58 * s,
                        fill=rgba("#fffdfa", 18), outline=rgba("#fff8e5", 245), width=8 * s)
    d.rounded_rectangle([45 * s, 61 * s, (w - 50) * s, (h - 70) * s], radius=46 * s,
                        outline=rgba("#8b682f", 145), width=3 * s)
    # magnifier
    cx, cy, r = 172 * s, 137 * s, 56 * s
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=rgba("#7b4d24", 235), width=14 * s)
    d.ellipse([cx - (r - 16 * s), cy - (r - 16 * s), cx + (r - 16 * s), cy + (r - 16 * s)],
              fill=rgba("#fffdf6", 230))
    d.line([(212 * s, 178 * s), (276 * s, 240 * s)], fill=rgba("#7b4d24", 235), width=16 * s)
    down(img.filter(ImageFilter.UnsharpMask(radius=1.2, percent=125)), w, h).save(OUT / "search_box_with_icon.png")


def draw_red_dot():
    w = h = 128
    s = 4
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)
    cx = cy = w * s // 2
    for r, col in [(56, "#7c4c14"), (48, "#fff2bf"), (39, "#b80703")]:
        d.ellipse([cx - r * s, cy - r * s, cx + r * s, cy + r * s], fill=rgba(col, 255))
    d.ellipse([cx - 31 * s, cy - 36 * s, cx + 28 * s, cy + 26 * s], fill=rgba("#f41c11", 255))
    d.ellipse([cx - 22 * s, cy - 32 * s, cx + 8 * s, cy - 6 * s], fill=rgba("#fff4e7", 180))
    down(img.filter(ImageFilter.GaussianBlur(0.15 * s)), w, h).save(OUT / "status_red_dot.png")


def draw_gold_flower():
    w = h = 360
    s = 4
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)
    cx = cy = w * s // 2
    for ang in [45, 135, 225, 315]:
        rad = math.radians(ang)
        px = cx + math.cos(rad) * 72 * s
        py = cy + math.sin(rad) * 72 * s
        d.regular_polygon((px, py, 55 * s), 4, rotation=ang, fill=rgba("#d99b34", 255),
                          outline=rgba("#6d4517", 170))
        d.regular_polygon((px, py, 42 * s), 4, rotation=ang, fill=rgba("#ffd889", 255))
    d.regular_polygon((cx, cy, 92 * s), 4, rotation=45, fill=rgba("#cc8628", 255),
                      outline=rgba("#6a4215", 190))
    d.ellipse([cx - 42 * s, cy - 42 * s, cx + 42 * s, cy + 42 * s], fill=rgba("#ffe09b", 255),
              outline=rgba("#8d5a1b", 190), width=5 * s)
    d.ellipse([cx - 20 * s, cy - 20 * s, cx + 20 * s, cy + 20 * s], fill=rgba("#c78328", 255))
    down(img.filter(ImageFilter.UnsharpMask(radius=1.2, percent=130)), w, h).save(OUT / "ornament_gold_flower.png")


def draw_circle_icon(filename, bg1, bg2, symbol, symbol_color="#f7d58b"):
    w = h = 360
    s = 4
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)
    cx = cy = w * s // 2
    r = 154 * s

    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse([cx - r - 22 * s, cy - r - 12 * s, cx + r + 22 * s, cy + r + 28 * s], fill=rgba("#3b230d", 80))
    shadow = shadow.filter(ImageFilter.GaussianBlur(7 * s))
    img.alpha_composite(shadow)

    d.ellipse([cx - r - 22 * s, cy - r - 22 * s, cx + r + 22 * s, cy + r + 22 * s], fill=rgba("#81531f", 255))
    d.ellipse([cx - r - 14 * s, cy - r - 14 * s, cx + r + 14 * s, cy + r + 14 * s], fill=rgba("#f5d077", 255))
    d.ellipse([cx - r - 2 * s, cy - r - 2 * s, cx + r + 2 * s, cy + r + 2 * s], fill=rgba("#fff5bf", 255))
    mask = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(mask)
    md.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    grad = radial_gradient(img.size, rgba(bg1, 255), rgba(bg2, 255), center=(cx - 42 * s, cy - 52 * s), radius_scale=0.95)
    add_painter_texture(grad, seed=sum(ord(c) for c in filename), strength=28)
    paste_masked(img, grad, mask)

    # Hand-painted decorative rings.
    d.arc([cx - r + 10*s, cy - r + 10*s, cx + r - 10*s, cy + r - 10*s], 200, 520, fill=rgba("#fff6cc", 170), width=5 * s)
    d.arc([cx - r + 26*s, cy - r + 26*s, cx + r - 26*s, cy + r - 26*s], 32, 292, fill=rgba("#5b3916", 95), width=3 * s)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=rgba("#b87925", 255), width=8 * s)
    d.ellipse([cx - (r - 18 * s), cy - (r - 18 * s), cx + (r - 18 * s), cy + (r - 18 * s)], outline=rgba("#fff8d6", 135), width=5 * s)

    shine = Image.new("RGBA", img.size, (0, 0, 0, 0))
    shd = ImageDraw.Draw(shine)
    shd.ellipse([72*s, 52*s, 260*s, 164*s], fill=rgba("#ffffff", 32))
    shine = shine.filter(ImageFilter.GaussianBlur(14*s))
    img.alpha_composite(shine)

    col = rgba(symbol_color, 248)
    light = rgba("#fff0b9", 250)
    dark = rgba("#5b3516", 185)
    ink = rgba("#3e2613", 185)

    def line(points, width=12):
        d.line([(int(x * s), int(y * s)) for x, y in points], fill=dark, width=(width + 7) * s, joint="curve")
        d.line([(int(x * s), int(y * s)) for x, y in points], fill=col, width=width * s, joint="curve")

    def blob(points, fill=col, outline=dark, width=5):
        d.polygon([(int(x*s), int(y*s)) for x, y in points], fill=outline)
        cxp = sum(x for x, _ in points) / len(points)
        cyp = sum(y for _, y in points) / len(points)
        inner = []
        for x, y in points:
            inner.append((cxp + (x - cxp) * 0.91, cyp + (y - cyp) * 0.91))
        d.polygon([(int(x*s), int(y*s)) for x, y in inner], fill=fill)

    if symbol == "yin_yang":
        d.ellipse([76*s, 66*s, 284*s, 274*s], fill=rgba("#f8f1d8", 250), outline=ink, width=5*s)
        d.pieslice([76*s, 66*s, 284*s, 274*s], 270, 90, fill=rgba("#0f4246", 250))
        d.ellipse([128*s, 66*s, 232*s, 170*s], fill=rgba("#0f4246", 250))
        d.ellipse([128*s, 170*s, 232*s, 274*s], fill=rgba("#f8f1d8", 250))
        d.ellipse([169*s, 108*s, 193*s, 132*s], fill=rgba("#f8f1d8", 250))
        d.ellipse([169*s, 210*s, 193*s, 234*s], fill=rgba("#0f4246", 250))
    elif symbol == "pagoda":
        for y, ww in [(242, 176), (198, 146), (158, 112)]:
            blob([(180-ww/2, y), (180, y-34), (180+ww/2, y), (180+ww/2-16, y+16), (180-ww/2+16, y+16)], fill=light)
            line([(180 - ww/2 + 16, y+16), (180 + ww/2 - 16, y+16)], 11)
        blob([(180, 82), (202, 124), (158, 124)], fill=light)
        line([(180, 118), (180, 266)], 9)
        line([(122, 266), (238, 266)], 12)
    elif symbol == "mountain":
        blob([(54,252),(138,102),(216,252)], fill=rgba("#fff4d8",245))
        blob([(146,252),(236,80),(312,252)], fill=rgba("#f4fbef",245))
        blob([(108,158),(138,102),(160,162),(144,148),(130,176)], fill=rgba("#1d7092",230), outline=rgba("#1d7092",230))
        blob([(205,140),(236,80),(266,146),(244,132),(232,168)], fill=rgba("#0e4771",235), outline=rgba("#0e4771",235))
        line([(66,254),(138,102),(216,254),(236,80),(312,254)], 8)
    elif symbol == "lotus":
        petals = [(180,96,34,82), (132,152,42,76), (228,152,42,76), (102,218,58,46), (258,218,58,46), (180,208,70,86)]
        for px, py, rw, rh in petals:
            d.ellipse([(px-rw)*s,(py-rh//2)*s,(px+rw)*s,(py+rh//2)*s], fill=dark)
            d.ellipse([(px-rw+5)*s,(py-rh//2+5)*s,(px+rw-5)*s,(py+rh//2-5)*s], fill=rgba("#ffc0a6",240))
        d.ellipse([126*s,146*s,234*s,256*s], fill=rgba("#ff7668",250), outline=dark, width=5*s)
        d.ellipse([150*s,120*s,210*s,238*s], fill=rgba("#ff9a87",230))
    elif symbol == "sword":
        blob([(180,42),(145,136),(170,126),(170,238),(190,238),(190,126),(215,136)], fill=light)
        line([(102,210),(258,210)], 15)
        blob([(180,240),(150,306),(210,306)], fill=light)
        d.ellipse([166*s,190*s,194*s,218*s], fill=rgba("#fff6c7",245), outline=dark, width=4*s)
    elif symbol == "cauldron":
        d.ellipse([116*s,118*s,244*s,204*s], fill=rgba("#fff2c0",235), outline=dark, width=6*s)
        d.rectangle([98*s,168*s,262*s,250*s], fill=rgba("#9b621e",245), outline=dark, width=6*s)
        d.rounded_rectangle([118*s,150*s,242*s,238*s], radius=24*s, fill=rgba("#b87b28",245), outline=dark, width=5*s)
        line([(112,248),(82,308)], 10); line([(248,248),(278,308)], 10)
        line([(124,126),(236,126)], 12)
        d.rectangle([154*s,88*s,206*s,106*s], fill=light)
    elif symbol == "water":
        line([(72,234),(114,194),(154,230),(198,190),(238,226),(292,174)], 13)
        line([(86,270),(128,232),(166,262),(214,224),(264,252)], 10)
        line([(130,118),(96,168),(150,148),(118,206),(236,92)], 11)
        line([(150,184),(206,158),(190,216),(260,190)], 8)
    elif symbol == "leaf":
        d.ellipse([106*s,72*s,254*s,270*s], fill=rgba("#fff4df",242), outline=dark, width=6*s)
        d.ellipse([128*s,92*s,232*s,250*s], fill=rgba("#e7f3d8",230))
        line([(180,86),(180,276)], 7)
        line([(180,158),(126,124)], 7); line([(180,190),(236,154)], 7); line([(180,222),(132,206)], 6)
    elif symbol == "fire":
        blob([(180,52),(124,172),(154,160),(110,286),(250,286),(210,164),(240,180)], fill=light)
        blob([(184,128),(148,238),(218,238)], fill=rgba("#fff8db",230), outline=rgba("#fff8db",210))
        line([(110,286),(250,286)], 9)
    elif symbol == "compass":
        d.ellipse([100*s,100*s,260*s,260*s], outline=dark, width=8*s)
        line([(180,66),(180,294)], 6); line([(66,180),(294,180)], 6)
        blob([(180,54),(212,180),(180,306),(148,180)], fill=light)

    # A small offset highlight keeps the symbols closer to the soft original game-art look.
    glaze = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glaze)
    gd.arc([cx - 116*s, cy - 122*s, cx + 100*s, cy + 88*s], 205, 302, fill=rgba("#ffffff", 92), width=4*s)
    img.alpha_composite(glaze)

    down(img.filter(ImageFilter.UnsharpMask(radius=1.2, percent=120)), w, h).save(OUT / filename)


def draw_small_symbol(filename, symbol):
    draw_circle_icon(filename, "#f7ead3", "#fff9ef", symbol, "#6a3d1e")


def draw_qstyle_icon(filename, theme, symbol):
    """Crisp Q-version Daoist badge redrawn from scratch, matching the source atlas more closely."""
    w = h = 420
    s = 4
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)

    def S(v):
        return int(round(v * s))

    def box(x0, y0, x1, y1):
        return [S(x0), S(y0), S(x1), S(y1)]

    def ellipse(x0, y0, x1, y1, fill, outline=None, width=1):
        d.ellipse(box(x0, y0, x1, y1), fill=fill, outline=outline, width=S(width))

    def line(points, fill, width=8):
        d.line([(S(x), S(y)) for x, y in points], fill=fill, width=S(width), joint="curve")

    def poly(points, fill, outline=None):
        d.polygon([(S(x), S(y)) for x, y in points], fill=fill, outline=outline)

    def soft_shadow(x0, y0, x1, y1, alpha=90, blur=8):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        ld.ellipse(box(x0, y0, x1, y1), fill=rgba("#3b2109", alpha))
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(S(blur))))

    def stroke_line(points, fill, width=10, stroke=rgba("#38210e", 220)):
        line(points, stroke, width + 5)
        line(points, fill, width)
        line([(x - 1.5, y - 1.5) for x, y in points], rgba("#fff0b8", 90), max(2, width // 3))

    def stroke_poly(points, fill, stroke=rgba("#38210e", 220), width=4):
        # Offset polygons give a soft painted outline while keeping the icon clear.
        for dx, dy in [(-width, 0), (width, 0), (0, -width), (0, width), (-width, -width), (width, width)]:
            d.polygon([(S(x + dx / s), S(y + dy / s)) for x, y in points], fill=stroke)
        poly(points, fill)
        cx = sum(x for x, _ in points) / len(points)
        cy = sum(y for _, y in points) / len(points)
        inner = [(cx + (x - cx) * 0.78, cy + (y - cy) * 0.78) for x, y in points]
        poly(inner, rgba("#fff7d6", 70))

    # Cream side tab / depth, visible in the original atlas.
    soft_shadow(70, 70, 360, 368, 78, 7)
    d.rounded_rectangle(box(124, 104, 386, 364), radius=S(42), fill=rgba("#fff5df", 248))
    d.rounded_rectangle(box(139, 118, 398, 345), radius=S(34), fill=rgba("#e8c59b", 105))

    # Gold ring stack.
    ellipse(55, 45, 367, 357, rgba("#784719", 255))
    ellipse(62, 51, 360, 349, rgba("#d2902c", 255))
    ellipse(72, 60, 350, 338, rgba("#ffe49b", 255))
    ellipse(82, 70, 340, 328, rgba("#b46b1d", 255))
    ellipse(88, 76, 334, 322, rgba("#fff3bf", 255))

    palette = {
        "teal": ("#19b99d", "#005c62"),
        "green": ("#5d943d", "#203d1c"),
        "blue": ("#1997d3", "#05275a"),
        "gold": ("#d79a32", "#4f2108"),
        "cream": ("#fff4dc", "#e4c199"),
    }[theme]
    disk = radial_gradient(img.size, rgba(palette[0], 255), rgba(palette[1], 255),
                           center=(S(148), S(112)), radius_scale=0.62)
    add_painter_texture(disk, seed=sum(ord(c) for c in filename) + 311, strength=12)
    mask = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(mask)
    md.ellipse(box(94, 82, 328, 316), fill=255)
    img.alpha_composite(Image.composite(disk, Image.new("RGBA", img.size, (0, 0, 0, 0)), mask))
    ellipse(94, 82, 328, 316, None, rgba("#90571d", 210), 4)
    d.arc(box(103, 91, 319, 307), 198, 520, fill=rgba("#fff9d6", 170), width=S(4))
    d.arc(box(112, 100, 310, 298), 30, 255, fill=rgba("#1f170f", 45), width=S(3))

    ink = rgba("#3b2412", 230)
    gold = rgba("#ffd98d", 255)
    cream = rgba("#fff0c7", 255)
    dark_blue = rgba("#062a58", 245)
    light_blue = rgba("#9fe2ed", 245)

    if symbol == "yin_yang":
        ellipse(105, 88, 320, 303, rgba("#f6f0dc", 255), ink, 4)
        d.pieslice(box(105, 88, 320, 303), 270, 90, fill=rgba("#00636c", 255))
        ellipse(158, 88, 267, 197, rgba("#00636c", 255))
        ellipse(158, 197, 267, 303, rgba("#f6f0dc", 255))
        ellipse(199, 130, 225, 156, rgba("#f6f0dc", 255), ink, 2)
        ellipse(199, 236, 225, 262, rgba("#00636c", 255), ink, 2)
    elif symbol == "pagoda":
        for y, rw, bw in [(245, 176, 118), (199, 142, 92), (158, 110, 66)]:
            stroke_poly([(210-rw/2, y), (210, y-35), (210+rw/2, y), (210+rw/2-18, y+15), (210-rw/2+18, y+15)], gold)
            d.rounded_rectangle(box(210-bw/2, y+6, 210+bw/2, y+42), radius=S(9),
                                fill=rgba("#38b28f", 245), outline=ink, width=S(3))
        stroke_poly([(210, 82), (234, 125), (186, 125)], gold)
        stroke_line([(210, 116), (210, 292)], gold, 8, ink)
        stroke_line([(136, 292), (284, 292)], gold, 11, ink)
        ellipse(181, 230, 241, 292, rgba("#bdeed4", 250), ink, 4)
    elif symbol == "cauldron":
        ellipse(142, 115, 282, 205, rgba("#ffd27a", 255), ink, 6)
        d.rounded_rectangle(box(116, 160, 306, 269), radius=S(30), fill=rgba("#9c5b18", 255), outline=ink, width=S(7))
        d.rounded_rectangle(box(139, 145, 283, 247), radius=S(24), fill=rgba("#cf8327", 255), outline=rgba("#5e3212", 230), width=S(5))
        ellipse(170, 199, 254, 281, rgba("#fff0bd", 255), rgba("#7a4316", 240), 6)
        stroke_line([(136, 266), (101, 330)], rgba("#c57521", 255), 12, ink)
        stroke_line([(288, 266), (323, 330)], rgba("#c57521", 255), 12, ink)
        stroke_line([(146, 124), (278, 124)], gold, 12, ink)
        d.rounded_rectangle(box(180, 82, 244, 106), radius=S(5), fill=cream, outline=ink, width=S(3))
    elif symbol == "mountain":
        stroke_poly([(52, 298), (155, 96), (242, 298)], cream)
        stroke_poly([(143, 304), (261, 70), (368, 304)], rgba("#fff7dc", 255))
        poly([(116, 174), (155, 96), (190, 178), (162, 158), (143, 209)], dark_blue)
        poly([(221, 150), (261, 70), (303, 164), (273, 143), (252, 199)], rgba("#072653", 255))
        stroke_line([(70, 265), (151, 238), (223, 262), (307, 229)], light_blue, 8, rgba("#0d4d75", 190))
    elif symbol == "lotus":
        for x, y, rw, rh, col in [
            (212, 117, 35, 84, "#ffafa0"),
            (166, 171, 45, 78, "#ffc0ad"),
            (258, 171, 45, 78, "#ffc0ad"),
            (126, 244, 64, 49, "#ffd2bd"),
            (298, 244, 64, 49, "#ffd2bd"),
            (212, 231, 73, 105, "#ff857a"),
        ]:
            ellipse(x-rw, y-rh/2, x+rw, y+rh/2, rgba(col, 255), ink, 5)
            ellipse(x-rw+15, y-rh/2+13, x+rw-18, y+rh/2-18, rgba("#fff4d6", 50))
        stroke_line([(119, 304), (168, 260), (213, 305), (260, 260), (317, 304)], rgba("#76b957", 255), 10, ink)
    elif symbol == "sword":
        stroke_poly([(212, 52), (179, 148), (203, 135), (203, 257), (221, 257), (221, 135), (245, 148)], gold)
        stroke_line([(119, 222), (178, 204), (212, 226), (247, 204), (305, 222)], rgba("#c47b25", 255), 13, ink)
        ellipse(192, 202, 232, 242, rgba("#fff1bd", 255), ink, 4)
        stroke_poly([(212, 258), (175, 340), (249, 340)], gold)
    elif symbol == "water":
        # Big spiral and waves like the original blue badge.
        d.arc(box(105, 82, 324, 300), 15, 340, fill=rgba("#75d7e4", 255), width=S(20))
        d.arc(box(145, 123, 277, 253), 20, 340, fill=dark_blue, width=S(34))
        d.arc(box(158, 136, 252, 229), 18, 338, fill=cream, width=S(9))
        stroke_line([(86, 270), (136, 226), (180, 260), (232, 210), (296, 238)], light_blue, 11, ink)
        stroke_line([(92, 230), (145, 182), (204, 220), (268, 152), (338, 207)], rgba("#71d7e2", 255), 13, ink)
    elif symbol == "leaf":
        ellipse(132, 78, 292, 292, rgba("#f6ebd5", 255), ink, 5)
        ellipse(154, 102, 270, 270, rgba("#e0edca", 245))
        stroke_line([(212, 86), (212, 304)], rgba("#c8ae65", 255), 6, ink)
        stroke_line([(212, 166), (150, 128)], rgba("#c8ae65", 255), 6, ink)
        stroke_line([(212, 204), (274, 166)], rgba("#c8ae65", 255), 6, ink)
        stroke_line([(212, 242), (154, 224)], rgba("#c8ae65", 255), 5, ink)
    elif symbol == "fire":
        stroke_poly([(212, 58), (144, 190), (176, 174), (126, 308), (298, 308), (248, 174), (282, 194)], gold)
        poly([(218, 130), (174, 258), (256, 258)], rgba("#fff9dc", 225))
        stroke_line([(130, 308), (294, 308)], rgba("#c47b25", 255), 8, ink)
    elif symbol == "compass":
        for ang in range(0, 360, 45):
            rad = math.radians(ang)
            px, py = 212 + math.cos(rad) * 104, 174 + math.sin(rad) * 91
            stroke_poly([(px, py-25), (px+23, py), (px, py+25), (px-23, py)], rgba("#f4c164", 255), ink, 3)
        ellipse(149, 110, 275, 236, rgba("#592b12", 255), rgba("#ffd987", 255), 7)
        stroke_line([(212, 130), (212, 214)], gold, 7, ink)
        stroke_line([(172, 172), (252, 172)], gold, 7, ink)
        stroke_line([(190, 150), (234, 194)], gold, 6, ink)

    # Small highlights/sparkles, restrained for clarity.
    d.arc(box(112, 94, 304, 278), 202, 292, fill=rgba("#fff8d2", 120), width=S(4))
    for x, y, r in [(128, 112, 3), (299, 126, 3), (318, 238, 2)]:
        ellipse(x-r, y-r, x+r, y+r, rgba("#fff2bd", 180))

    img = painted_finish(img, seed=sum(ord(c) for c in filename) + 101)
    down(img, w, h).save(OUT / filename)


def draw_qstyle_icon_old_soft(filename, theme, symbol):
    """Draw a Q-version Daoist game badge from scratch, no source pixels reused."""
    w = h = 420
    s = 4
    img = canvas(w, h, s)
    d = ImageDraw.Draw(img)
    cx = cy = w * s // 2
    front_r = 156 * s

    def S(v):
        return int(v * s)

    def ellipse(box, fill, outline=None, width=1):
        d.ellipse([S(box[0]), S(box[1]), S(box[2]), S(box[3])], fill=fill, outline=outline, width=S(width))

    def poly(points, fill, outline=None):
        d.polygon([(S(x), S(y)) for x, y in points], fill=fill, outline=outline)

    def line(points, fill, width=8):
        d.line([(S(x), S(y)) for x, y in points], fill=fill, width=S(width), joint="curve")

    def soft_ellipse(box, fill, blur=3):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        ld.ellipse([S(box[0]), S(box[1]), S(box[2]), S(box[3])], fill=fill)
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(S(blur))))

    # Soft card-like thickness behind the medallion, matching the original round badges.
    soft_ellipse((56, 70, 388, 390), rgba("#4a2b0d", 70), 7)
    d.rounded_rectangle([S(108), S(95), S(382), S(374)], radius=S(46), fill=rgba("#fff6e4", 245))
    d.rounded_rectangle([S(124), S(112), S(393), S(356)], radius=S(38), fill=rgba("#f1d9b7", 130))
    ellipse((54, 44, 370, 360), rgba("#8a561d", 255))
    ellipse((64, 50, 360, 348), rgba("#f4c462", 255))
    ellipse((75, 62, 349, 338), rgba("#fff1b6", 255))

    # Painted inner disk.
    inner = Image.new("RGBA", img.size, (0, 0, 0, 0))
    inner_mask = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(inner_mask)
    md.ellipse([S(86), S(73), S(338), S(325)], fill=255)
    if theme == "teal":
        bg = radial_gradient(img.size, rgba("#22c7a7", 255), rgba("#00565d", 255), center=(S(150), S(110)), radius_scale=0.62)
    elif theme == "green":
        bg = radial_gradient(img.size, rgba("#6fa542", 255), rgba("#183d1d", 255), center=(S(150), S(105)), radius_scale=0.68)
    elif theme == "blue":
        bg = radial_gradient(img.size, rgba("#229bd5", 255), rgba("#062557", 255), center=(S(145), S(105)), radius_scale=0.66)
    elif theme == "gold":
        bg = radial_gradient(img.size, rgba("#d99b2f", 255), rgba("#4f2108", 255), center=(S(150), S(110)), radius_scale=0.7)
    else:
        bg = radial_gradient(img.size, rgba("#fff8e8", 255), rgba("#e4c6a0", 255), center=(S(150), S(105)), radius_scale=0.75)
    add_painter_texture(bg, seed=sum(ord(c) for c in filename) + 19, strength=18)
    img.alpha_composite(Image.composite(bg, Image.new("RGBA", img.size, (0, 0, 0, 0)), inner_mask))

    # Inner rings and glassy highlights.
    d.arc([S(91), S(78), S(333), S(320)], 205, 520, fill=rgba("#fff8d7", 135), width=S(5))
    d.arc([S(103), S(90), S(321), S(309)], 18, 292, fill=rgba("#4b2c12", 80), width=S(4))
    d.ellipse([S(86), S(73), S(338), S(325)], outline=rgba("#b57724", 255), width=S(7))
    d.ellipse([S(98), S(85), S(326), S(313)], outline=rgba("#fff6c9", 85), width=S(3))
    soft_ellipse((106, 72, 292, 178), rgba("#ffffff", 9), 7)

    ink = rgba("#40240f", 210)
    gold = rgba("#ffd57e", 255)
    gold2 = rgba("#c97b22", 255)
    cream = rgba("#fff0bf", 255)
    seed = sum(ord(c) for c in filename) + 77
    rng = (seed * 1103515245 + 12345) & 0x7fffffff

    def rand():
        nonlocal rng
        rng = (rng * 1103515245 + 12345) & 0x7fffffff
        return rng / 0x7fffffff

    def jitter(points, amount=3.2):
        return [(x + (rand() - 0.5) * amount, y + (rand() - 0.5) * amount) for x, y in points]

    def brush_line(points, color, width=10, shadow=ink, passes=5):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        for _ in range(max(2, passes // 2)):
            pts = [(S(x + (rand() - 0.5) * 3.6), S(y + (rand() - 0.5) * 3.6)) for x, y in points]
            ld.line(pts, fill=shadow, width=S(width + 5 + rand() * 2), joint="curve")
        for _ in range(passes):
            pts = [(S(x + (rand() - 0.5) * 4.5), S(y + (rand() - 0.5) * 4.5)) for x, y in points]
            c = list(color)
            c[3] = int(c[3] * (0.72 + rand() * 0.22))
            ld.line(pts, fill=tuple(c), width=S(width - 2 + rand() * 4), joint="curve")
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(0.08 * s)))

    def brush_blob(points, fill, outline=ink, passes=7):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        for _ in range(3):
            ld.polygon([(S(x), S(y)) for x, y in jitter(points, 5.5)], fill=outline)
        for _ in range(passes):
            c = list(fill)
            c[3] = int(c[3] * (0.74 + rand() * 0.20))
            ld.polygon([(S(x), S(y)) for x, y in jitter(points, 4.8)], fill=tuple(c))
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(0.06 * s)))

    def brush_ellipse(box, fill, outline=ink, passes=7):
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        x0, y0, x1, y1 = box
        for _ in range(3):
            j = (rand() - 0.5) * 4
            ld.ellipse([S(x0-j), S(y0+j), S(x1+j), S(y1-j)], fill=outline)
        for _ in range(passes):
            c = list(fill)
            c[3] = int(c[3] * (0.72 + rand() * 0.22))
            jx = (rand() - 0.5) * 5
            jy = (rand() - 0.5) * 5
            ld.ellipse([S(x0+jx), S(y0+jy), S(x1+jx), S(y1+jy)], fill=tuple(c))
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(0.06 * s)))

    def stroked_poly(points, fill, stroke=ink, width=5):
        brush_blob(points, fill, stroke)

    def stroked_line(points, fill=gold, width=11, stroke=ink):
        brush_line(points, fill, width, stroke)

    if symbol == "yin_yang":
        brush_ellipse((105, 82, 319, 296), rgba("#f7f2df", 255), ink, 6)
        d.pieslice([S(105), S(82), S(319), S(296)], 270, 90, fill=rgba("#00626c", 255))
        brush_ellipse((158, 82, 266, 190), rgba("#00626c", 245), rgba("#00626c", 245), 5)
        brush_ellipse((158, 190, 266, 296), rgba("#f7f2df", 245), rgba("#f7f2df", 245), 5)
        brush_ellipse((197, 123, 225, 151), rgba("#f7f2df", 255), ink, 4)
        brush_ellipse((197, 227, 225, 255), rgba("#00626c", 255), ink, 4)
        d.arc([S(112), S(90), S(311), S(289)], 236, 322, fill=rgba("#ffffff", 145), width=S(5))
    elif symbol == "pagoda":
        # Rounded, chibi Daoist pavilion with curved golden roofs.
        for y, roof_w, body_w in [(245, 178, 118), (198, 145, 92), (155, 112, 66)]:
            stroked_poly([(212 - roof_w/2, y), (212, y - 35), (212 + roof_w/2, y),
                          (212 + roof_w/2 - 20, y + 16), (212 - roof_w/2 + 20, y + 16)], gold)
            brush_blob([(212 - body_w/2, y + 4), (212 + body_w/2, y + 4), (212 + body_w/2 - 6, y + 43),
                        (212 - body_w/2 + 6, y + 43)], rgba("#38b89b", 215), ink, 5)
        stroked_poly([(212, 80), (236, 124), (188, 124)], gold)
        stroked_line([(212, 116), (212, 292)], gold, 8)
        stroked_line([(138, 292), (286, 292)], gold, 12)
        brush_ellipse((180, 230, 244, 294), rgba("#b7f1d5", 235), ink, 5)
    elif symbol == "cauldron":
        # Squat incense cauldron, thick golden paint, dark belly.
        brush_ellipse((140, 116, 284, 206), rgba("#ffd475", 245), ink, 7)
        brush_blob([(118, 174), (132, 156), (292, 156), (306, 176), (292, 268), (132, 268)],
                   rgba("#a56620", 245), ink, 8)
        brush_blob([(144, 154), (280, 154), (276, 248), (148, 248)], rgba("#d28c2f", 240), rgba("#6c3a13", 190), 6)
        brush_ellipse((172, 198, 252, 278), rgba("#fff0bd", 245), rgba("#7a4316", 220), 6)
        stroked_line([(138, 266), (104, 330)], gold2, 12)
        stroked_line([(286, 266), (320, 330)], gold2, 12)
        stroked_line([(148, 122), (276, 122)], gold, 13)
        brush_blob([(180,83),(244,83),(244,106),(180,106)], cream, ink, 4)
        soft_ellipse((150, 132, 280, 178), rgba("#fff5bf", 38), 4)
    elif symbol == "mountain":
        # Layered snowy mountains, oversized and cropped like the original.
        stroked_poly([(55, 294), (156, 105), (238, 294)], cream)
        stroked_poly([(145, 300), (260, 76), (363, 300)], rgba("#fff6d8", 255))
        brush_blob([(118, 174), (156, 105), (188, 176), (162, 158), (142, 204)], rgba("#0d3b68", 230), rgba("#0d3b68", 190), 5)
        brush_blob([(222, 150), (260, 76), (300, 164), (272, 144), (252, 197)], rgba("#0a2d56", 235), rgba("#0a2d56", 190), 5)
        stroked_line([(68, 296), (156, 105), (238, 296), (260, 76), (365, 302)], cream, 8)
        stroked_line([(78, 262), (154, 238), (224, 260), (302, 232)], rgba("#8ed4df", 230), 7, rgba("#0d4970", 160))
    elif symbol == "lotus":
        # Soft peach lotus petals on green disk.
        petals = [
            (212, 118, 36, 82, "#ffb3a5"),
            (166, 170, 45, 78, "#ffc2af"),
            (258, 170, 45, 78, "#ffc2af"),
            (128, 244, 62, 48, "#ffd2bb"),
            (296, 244, 62, 48, "#ffd2bb"),
            (212, 230, 72, 104, "#ff8679"),
        ]
        for px, py, rw, rh, col in petals:
            brush_ellipse((px-rw, py-rh/2, px+rw, py+rh/2), rgba(col, 235), ink, 6)
            soft_ellipse((px-rw+14, py-rh/2+10, px+rw-18, py+rh/2-16), rgba("#fff9e7", 55), 2)
        stroked_line([(120, 304), (168, 260), (214, 304), (260, 260), (316, 304)], rgba("#7fc15e", 250), 10, ink)
    elif symbol == "sword":
        # Chunky ornate sword, not a generic arrow.
        stroked_poly([(212, 52), (180, 147), (203, 135), (203, 258), (221, 258), (221, 135), (244, 147)], gold)
        stroked_line([(120, 222), (178, 204), (212, 226), (247, 204), (304, 222)], gold2, 14)
        brush_ellipse((192, 202, 232, 242), rgba("#fff3bb", 250), ink, 5)
        stroked_poly([(212, 258), (176, 340), (248, 340)], gold)
        d.arc([S(124), S(126), S(300), S(304)], 210, 320, fill=rgba("#fff4c8", 120), width=S(5))
    elif symbol == "water":
        # Decorative Q swirl wave.
        stroked_line([(96, 246), (148, 190), (208, 228), (270, 154), (338, 210)], rgba("#77d2dc", 250), 14, ink)
        stroked_line([(95, 292), (144, 248), (194, 282), (250, 230), (310, 258)], rgba("#9ae5ec", 235), 10, ink)
        brush_line([(270, 128), (230, 100), (170, 112), (132, 160), (148, 220), (210, 226), (236, 184)],
                   rgba("#7edce6", 240), 18, ink, 8)
        brush_line([(226, 166), (196, 142), (164, 158), (164, 194), (202, 202)], rgba("#083459", 245), 22, rgba("#083459", 190), 6)
        brush_line([(218, 174), (192, 160), (176, 174), (178, 192), (204, 194)], rgba("#fff0b8", 230), 7, rgba("#fff0b8", 180), 4)
    elif symbol == "leaf":
        brush_ellipse((132, 78, 292, 292), rgba("#f4ead5", 245), ink, 6)
        brush_ellipse((154, 102, 270, 270), rgba("#dcebc7", 220), rgba("#dcebc7", 120), 5)
        stroked_line([(212, 86), (212, 304)], rgba("#dcc986", 255), 7)
        stroked_line([(212, 166), (150, 128)], rgba("#dcc986", 255), 7)
        stroked_line([(212, 204), (274, 166)], rgba("#dcc986", 255), 7)
        stroked_line([(212, 242), (154, 224)], rgba("#dcc986", 255), 6)
    elif symbol == "fire":
        stroked_poly([(212, 58), (144, 190), (176, 174), (126, 308), (298, 308), (248, 174), (282, 194)], gold)
        poly([(218, 130), (174, 258), (256, 258)], rgba("#fff9dc", 210))
        stroked_line([(130, 308), (294, 308)], gold2, 9)
    elif symbol == "compass":
        # Golden compass/coin talisman with ornate petals.
        brush_ellipse((148, 108, 276, 236), rgba("#5b2c12", 245), rgba("#f6c569", 245), 8)
        for ang in range(0, 360, 45):
            rad = math.radians(ang)
            px, py = 212 + math.cos(rad) * 102, 172 + math.sin(rad) * 90
            brush_blob([(px, py-26), (px+24, py), (px, py+26), (px-24, py)], rgba("#f4c164", 235), ink, 4)
        brush_ellipse((160, 120, 264, 224), rgba("#5a2a12", 245), rgba("#ffd987", 240), 6)
        stroked_line([(212, 130), (212, 214)], gold, 7)
        stroked_line([(172, 172), (252, 172)], gold, 7)
        stroked_line([(190, 150), (234, 194)], gold, 6)

    # Small hand-painted sparkles and final softening.
    for x, y, rr in [(128, 112, 4), (300, 122, 3), (318, 238, 3), (140, 278, 2)]:
        ellipse((x-rr, y-rr, x+rr, y+rr), rgba("#fff4c7", 190))
    final_alpha = img.getchannel("A")
    img = painted_finish(img, seed=seed)
    img.putalpha(final_alpha)
    down(img, w, h).save(OUT / filename)


def contact_sheet():
    files = sorted(p for p in OUT.glob("*.png"))
    thumbs = []
    for p in files:
        im = Image.open(p).convert("RGBA")
        maxw, maxh = 360, 230
        sc = min(maxw / im.width, maxh / im.height, 1)
        tw, th = max(1, int(im.width * sc)), max(1, int(im.height * sc))
        im = im.resize((tw, th), Image.Resampling.LANCZOS)
        tile = Image.new("RGBA", (420, 300), (245, 245, 245, 255))
        td = ImageDraw.Draw(tile)
        for y in range(0, 250, 20):
            for x in range(0, 420, 20):
                td.rectangle([x, y, x + 19, y + 19], fill=(224, 224, 224, 255) if (x//20 + y//20) % 2 else (246, 246, 246, 255))
        tile.alpha_composite(im, ((420 - tw) // 2, (250 - th) // 2))
        td.text((8, 262), p.name[:46], fill=(25, 25, 25, 255))
        thumbs.append(tile.convert("RGB"))
    cols = 3
    rows = math.ceil(len(thumbs) / cols)
    sheet = Image.new("RGB", (cols * 420, rows * 300), (255, 255, 255))
    for i, t in enumerate(thumbs):
        sheet.paste(t, ((i % cols) * 420, (i // cols) * 300))
    sheet.save(OUT / "contact_sheet_redrawn.jpg", quality=92)


def main():
    draw_top_button()
    draw_panel(1419, 380, "list_bg_unselected.png", selected=False, radius=58)
    draw_panel(1419, 380, "list_bg_selected_green.png", selected=True, green=True, radius=58)
    draw_panel(2606, 574, "server_card_bg_wide.png", selected=False, radius=78)
    draw_panel(2364, 574, "server_card_bg_medium.png", selected=False, radius=78)
    draw_panel(2606, 574, "server_card_bg_wide_selected_green.png", selected=True, green=True, radius=78)
    draw_panel(2364, 574, "server_card_bg_medium_selected_green.png", selected=True, green=True, radius=78)
    draw_panel(6679, 413, "bottom_bar_bg_green_white.png", selected=True, green=True, radius=64)
    draw_search_box()
    draw_red_dot()
    draw_gold_flower()
    icons = [
        ("icon_yin_yang.png", "teal", "yin_yang"),
        ("icon_pagoda.png", "teal", "pagoda"),
        ("icon_mountain.png", "blue", "mountain"),
        ("icon_lotus.png", "green", "lotus"),
        ("icon_sword.png", "teal", "sword"),
        ("icon_cauldron.png", "gold", "cauldron"),
        ("icon_water.png", "blue", "water"),
        ("icon_leaf.png", "cream", "leaf"),
        ("icon_fire.png", "cream", "fire"),
        ("icon_compass.png", "green", "compass"),
    ]
    for name, theme, sym in icons:
        draw_qstyle_icon(name, theme, sym)
    manifest = {
        "backgrounds": [
            "top_button_green_selected.png",
            "list_bg_unselected.png",
            "list_bg_selected_green.png",
            "server_card_bg_wide.png",
            "server_card_bg_medium.png",
            "server_card_bg_wide_selected_green.png",
            "server_card_bg_medium_selected_green.png",
            "bottom_bar_bg_green_white.png",
            "search_box_with_icon.png"
        ],
        "status": ["status_red_dot.png"],
        "ornaments": ["ornament_gold_flower.png"],
        "round_icons": [name for name, _, _ in icons],
        "notes": "All PNG files are transparent. Background frames intentionally contain no text, red dot, or round class icon so they can be recomposed in UI."
    }
    (OUT / "manifest_redrawn.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    contact_sheet()
    print(OUT.resolve())


if __name__ == "__main__":
    main()
