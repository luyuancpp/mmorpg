from pathlib import Path
from PIL import Image, ImageEnhance, ImageFilter, ImageDraw
import colorsys
import json
import math
import random


SRC_DIR = Path("q_daoist_login_ui_10240_redraw_clear_final_layers/q_daoist_login_buttons_redrawn_atomic")
OUT_DIR = SRC_DIR / "qstyle_icon_variants_1000"
OUT_DIR.mkdir(parents=True, exist_ok=True)


BASE_NAMES = [
    "icon_yin_yang.png",
    "icon_pagoda.png",
    "icon_cauldron.png",
    "icon_mountain.png",
    "icon_lotus.png",
    "icon_sword.png",
    "icon_water.png",
    "icon_compass.png",
    "icon_fire.png",
    "icon_peach_spirit.png",
]


def shift_hue(img, hue_shift, sat_mul, val_mul):
    rgba = img.convert("RGBA")
    px = rgba.load()
    w, h = rgba.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            rf, gf, bf = r / 255.0, g / 255.0, b / 255.0
            hh, ss, vv = colorsys.rgb_to_hsv(rf, gf, bf)
            # Protect ivory highlights and gold rims from drifting too far.
            if ss < 0.16 and vv > 0.68:
                local_shift = hue_shift * 0.18
                local_sat = 1.0 + (sat_mul - 1.0) * 0.25
            elif 0.08 < hh < 0.17 and ss > 0.25 and vv > 0.35:
                local_shift = hue_shift * 0.25
                local_sat = 1.0 + (sat_mul - 1.0) * 0.35
            else:
                local_shift = hue_shift
                local_sat = sat_mul
            hh = (hh + local_shift) % 1.0
            ss = max(0.0, min(1.0, ss * local_sat))
            vv = max(0.0, min(1.0, vv * val_mul))
            nr, ng, nb = colorsys.hsv_to_rgb(hh, ss, vv)
            px[x, y] = (int(nr * 255), int(ng * 255), int(nb * 255), a)
    return rgba


def add_subtle_finish(img, rng):
    rgba = img.convert("RGBA")
    w, h = rgba.size

    # Keep alpha crisp.
    alpha = rgba.getchannel("A")

    if rng.random() < 0.6:
        rgba = ImageEnhance.Color(rgba).enhance(rng.uniform(0.94, 1.12))
    rgba = ImageEnhance.Contrast(rgba).enhance(rng.uniform(0.96, 1.10))
    rgba = ImageEnhance.Brightness(rgba).enhance(rng.uniform(0.94, 1.08))

    # A restrained painterly glaze, clipped by existing alpha.
    glaze = Image.new("RGBA", rgba.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(glaze)
    for _ in range(rng.randint(8, 18)):
        x = rng.randint(30, w - 30)
        y = rng.randint(30, h - 30)
        rw = rng.randint(8, 48)
        rh = rng.randint(4, 22)
        if rng.random() < 0.55:
            col = (255, 239, 188, rng.randint(6, 22))
        else:
            col = (38, 22, 8, rng.randint(3, 12))
        d.ellipse([x - rw, y - rh, x + rw, y + rh], fill=col)
    glaze = glaze.filter(ImageFilter.GaussianBlur(rng.uniform(0.4, 1.2)))
    rgba.alpha_composite(glaze)

    # Optional tiny rotation/scale variation, still centered on a 420 canvas.
    if rng.random() < 0.55:
        angle = rng.uniform(-2.0, 2.0)
        rotated = rgba.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
        scale = rng.uniform(0.965, 1.02)
        nw, nh = int(rotated.width * scale), int(rotated.height * scale)
        rotated = rotated.resize((nw, nh), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        canvas.alpha_composite(rotated, ((w - nw) // 2, (h - nh) // 2))
        rgba = canvas

    rgba.putalpha(alpha if rgba.size == alpha.size else rgba.getchannel("A"))
    return rgba.filter(ImageFilter.UnsharpMask(radius=0.6, percent=95, threshold=3))


def make_contact_sheet(files, out_path, limit=120):
    thumbs = []
    for p in files[:limit]:
        im = Image.open(p).convert("RGBA")
        tile = Image.new("RGBA", (180, 210), (245, 245, 245, 255))
        d = ImageDraw.Draw(tile)
        for y in range(0, 168, 14):
            for x in range(0, 180, 14):
                d.rectangle([x, y, x + 13, y + 13], fill=(224, 224, 224, 255) if (x // 14 + y // 14) % 2 else (246, 246, 246, 255))
        sc = min(150 / im.width, 150 / im.height)
        im = im.resize((int(im.width * sc), int(im.height * sc)), Image.Resampling.LANCZOS)
        tile.alpha_composite(im, ((180 - im.width) // 2, (168 - im.height) // 2))
        d.text((6, 176), p.name[:24], fill=(0, 0, 0, 255))
        thumbs.append(tile.convert("RGB"))
    cols = 5
    rows = math.ceil(len(thumbs) / cols)
    sheet = Image.new("RGB", (cols * 180, rows * 210), (255, 255, 255))
    for i, t in enumerate(thumbs):
        sheet.paste(t, ((i % cols) * 180, (i // cols) * 210))
    sheet.save(out_path, quality=92)


def main():
    bases = []
    for name in BASE_NAMES:
        path = SRC_DIR / name
        if not path.exists():
            raise FileNotFoundError(path)
        bases.append((name, Image.open(path).convert("RGBA")))

    manifest = []
    written = []
    total = 1000
    per_base = total // len(bases)
    index = 1
    for base_i, (base_name, base_img) in enumerate(bases):
        stem = Path(base_name).stem.replace("icon_", "")
        for j in range(per_base):
            rng = random.Random(20260516 + base_i * 10000 + j)
            hue_shift = rng.uniform(-0.035, 0.035)
            sat_mul = rng.uniform(0.90, 1.18)
            val_mul = rng.uniform(0.92, 1.10)
            img = shift_hue(base_img, hue_shift, sat_mul, val_mul)
            img = add_subtle_finish(img, rng)
            filename = f"variant_{index:04d}_{stem}_{j + 1:03d}.png"
            out_path = OUT_DIR / filename
            img.save(out_path)
            written.append(out_path)
            manifest.append({
                "file": filename,
                "base": base_name,
                "hue_shift": round(hue_shift, 5),
                "saturation": round(sat_mul, 4),
                "value": round(val_mul, 4),
            })
            index += 1

    (OUT_DIR / "manifest.json").write_text(json.dumps({
        "count": len(manifest),
        "source_dir": str(SRC_DIR),
        "variant_method": "color grade, subtle painterly glaze, minor rotation/scale, transparent PNG",
        "items": manifest,
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    make_contact_sheet(written, OUT_DIR / "contact_sheet_first_120.jpg")
    print(OUT_DIR.resolve())
    print(len(written))


if __name__ == "__main__":
    main()
