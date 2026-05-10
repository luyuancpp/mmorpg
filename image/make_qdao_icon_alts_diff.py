"""
Cut sharp small icons from q_daoist_login_clear_2560x1080.png at the
locations where it differs from the no-controls overlay. These supplement
the existing icons in ui_controls_v2/icons/ (the "alts" folder).

Output icons are 64x64 by default with these quality steps:
  1. Crop a generous square around each region from the original (full res).
  2. Upscale 2x with LANCZOS4 (super-sample) then unsharp mask for crispness.
  3. Downscale to target size with LANCZOS4.
  4. Apply a soft circular alpha mask (matches the existing alts style).

Run:  python image/make_qdao_icon_alts_diff.py
"""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "q_daoist_login_clear_2560x1080.png"
OUT = ROOT / "ui_controls_v2" / "icons"
ATLAS = ROOT / "ui_controls_v2" / "atlas"
MANIFEST = ROOT / "ui_controls_v2" / "manifest.json"


def crisp_crop(src, cx, cy, crop_size, out_size, mask="circle", sharpen=True):
    """
    Crop a (crop_size x crop_size) square centred on (cx, cy), then resample
    to (out_size x out_size). Two stages:

      1. 2x super-sample upscale with LANCZOS4
      2. Optional unsharp mask
      3. Final downscale to out_size with LANCZOS4

    This consistently beats a single-step resize for tiny icons that have
    high-frequency detail (gold rims, hairline calligraphy strokes).
    """
    half = crop_size // 2
    box = (cx - half, cy - half, cx + half, cy + half)
    tile = src.crop(box).convert("RGBA")

    # 2x super-sample
    tile = tile.resize((tile.width * 2, tile.height * 2), Image.Resampling.LANCZOS)

    if sharpen:
        # Mild unsharp: enhances edges without ringing
        tile = tile.filter(ImageFilter.UnsharpMask(radius=1.2, percent=140, threshold=2))

    # Final size
    tile = tile.resize((out_size, out_size), Image.Resampling.LANCZOS)

    if mask == "circle":
        alpha = Image.new("L", tile.size, 0)
        d = ImageDraw.Draw(alpha)
        inset = max(1, int(out_size * 0.04))
        d.ellipse((inset, inset, out_size - inset, out_size - inset), fill=255)
        # AA the alpha edge
        alpha = alpha.filter(ImageFilter.GaussianBlur(0.5))
        existing = tile.split()[3]
        # Combine existing alpha with circle mask
        combined = Image.eval(alpha, lambda v: v).point(lambda p: p)
        tile.putalpha(combined)
    elif mask == "soft_square":
        alpha = Image.new("L", tile.size, 0)
        d = ImageDraw.Draw(alpha)
        radius = max(6, int(out_size * 0.18))
        d.rounded_rectangle((0, 0, out_size - 1, out_size - 1), radius=radius, fill=255)
        alpha = alpha.filter(ImageFilter.GaussianBlur(0.6))
        tile.putalpha(alpha)
    # else: no mask — keep RGBA as-is

    return tile


def make_atlas(entries, atlas_path):
    pad = 12
    cell = 96
    cols = 8
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell + pad, rows * cell + pad), (0, 0, 0, 0))
    meta = {}
    for i, (key, path) in enumerate(entries):
        img = Image.open(path).convert("RGBA")
        x = pad + (i % cols) * cell + (cell - img.width) // 2
        y = pad + (i // cols) * cell + (cell - img.height) // 2
        sheet.alpha_composite(img, (x, y))
        meta[key] = {"x": x, "y": y, "w": img.width, "h": img.height}
    atlas_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(atlas_path)
    (atlas_path.with_suffix(".json")).write_text(
        json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    src = Image.open(SRC).convert("RGBA")

    # Coordinates discovered via image-diff against the no-controls overlay.
    # crop_size: half-width of the source crop in px BEFORE supersample.
    # out_size: final png dimension. Larger out_size = sharper but bigger file.
    # 64px is the standard "small icon" size; bump to 96 for status dots.
    specs = [
        # name,                                  cx,   cy,   crop, out, mask
        # Right-edge corner ornament (might be close button)
        ("icon_alt_corner_top_right.png",        2283,   87,   90,  96, "soft_square"),

        # Server-card status dots: 8 cards in a 2-column grid (x=997 and x=1494)
        ("icon_alt_server_card_dot_a1.png",       997,  357,   60,  64, "circle"),
        ("icon_alt_server_card_dot_b1.png",      1494,  358,   60,  64, "circle"),
        ("icon_alt_server_card_dot_a2.png",       996,  476,   60,  64, "circle"),
        ("icon_alt_server_card_dot_b2.png",      1494,  476,   60,  64, "circle"),
        ("icon_alt_server_card_dot_a3.png",       997,  594,   60,  64, "circle"),
        ("icon_alt_server_card_dot_b3.png",      1494,  594,   60,  64, "circle"),
        ("icon_alt_server_card_dot_a4.png",       997,  713,   60,  64, "circle"),
        ("icon_alt_server_card_dot_b4.png",      1494,  713,   60,  64, "circle"),

        # Left filter column: small per-tab icons next to the category labels
        ("icon_alt_left_tab_icon_1.png",          689,  545,   56,  72, "soft_square"),
        ("icon_alt_left_tab_icon_2.png",          689,  617,   56,  72, "soft_square"),
        ("icon_alt_left_tab_icon_3.png",          688,  693,   56,  72, "soft_square"),
        ("icon_alt_left_tab_icon_4.png",          689,  767,   56,  72, "soft_square"),

        # Bottom row: three trailing controls (status / select / extra)
        ("icon_alt_bottom_row_a.png",            1176,  898,   60,  64, "circle"),
        ("icon_alt_bottom_row_b.png",            1329,  898,   60,  64, "circle"),
        ("icon_alt_bottom_row_c.png",            1500,  898,   60,  64, "circle"),
    ]

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    made = []
    for name, cx, cy, crop_size, out_size, mask in specs:
        icon = crisp_crop(src, cx, cy, crop_size, out_size, mask=mask, sharpen=True)
        path = OUT / name
        icon.save(path)
        key = path.stem
        manifest[key] = {
            "file": f"icons/{name}",
            "size": [out_size, out_size],
            "role": "foreground icon (alt, diff-derived)",
            "source": "diff(q_daoist_login_clear_2560x1080.png, q_daoist_login_ui_overlay_no_controls_generated_last.png)",
            "crop_at": [cx, cy],
            "crop_size": crop_size,
        }
        made.append((key, path))
        print(f"  {name:42}  {out_size}x{out_size}  from ({cx},{cy})")

    atlas_path = ATLAS / "qdao_login_icon_alts_diff_atlas.png"
    make_atlas(made, atlas_path)
    manifest["qdao_login_icon_alts_diff_atlas"] = {
        "file": f"atlas/{atlas_path.name}",
        "meta": f"atlas/{atlas_path.stem}.json",
        "role": "icon alts atlas (diff-derived)",
    }

    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"\nwrote {len(made)} alt icons + atlas at {atlas_path}")


if __name__ == "__main__":
    main()
