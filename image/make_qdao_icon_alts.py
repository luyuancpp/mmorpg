from pathlib import Path
from PIL import Image, ImageDraw
import json


ROOT = Path(__file__).resolve().parent
SRC = ROOT / "q_daoist_login_clear_2560x1080.png"
OUT = ROOT / "ui_controls_v2" / "icons"
ATLAS = ROOT / "ui_controls_v2" / "atlas"
MANIFEST = ROOT / "ui_controls_v2" / "manifest.json"


def remove_light_ui_bg(tile):
    pix = tile.load()
    for y in range(tile.height):
        for x in range(tile.width):
            r, g, b, a = pix[x, y]
            # The source mock keeps most control backgrounds in warm pale tones.
            # Treat those as matte and keep the darker ink/gold icon strokes.
            if r > 155 and g > 135 and b > 105 and (max(r, g, b) - min(r, g, b)) < 90:
                pix[x, y] = (r, g, b, 0)
            elif g > 70 and b > 55 and r < 95 and g > r * 1.25:
                pix[x, y] = (r, g, b, 0)
            elif a:
                boost = min(255, int(a * 1.15))
                pix[x, y] = (r, g, b, boost)
    return tile


def crop_square(src, cx, cy, size, name, mask="circle", out_size=112):
    half = size // 2
    tile = src.crop((cx - half, cy - half, cx + half, cy + half)).convert("RGBA")
    if mask == "cutout":
        tile = remove_light_ui_bg(tile)
    elif mask == "circle":
        alpha = Image.new("L", tile.size, 0)
        d = ImageDraw.Draw(alpha)
        inset = max(1, int(size * 0.04))
        d.ellipse((inset, inset, size - inset, size - inset), fill=255)
        tile.putalpha(alpha)
    elif mask == "soft_square":
        alpha = Image.new("L", tile.size, 0)
        d = ImageDraw.Draw(alpha)
        radius = max(8, int(size * 0.18))
        d.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
        tile.putalpha(alpha)

    if out_size != size:
        tile = tile.resize((out_size, out_size), Image.Resampling.LANCZOS)

    path = OUT / name
    tile.save(path)
    return path, tile.size


def draw_empty_ring(name, out_size=64):
    img = Image.new("RGBA", (out_size, out_size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((8, 8, out_size - 8, out_size - 8), fill=(255, 247, 229, 230))
    d.ellipse((13, 13, out_size - 13, out_size - 13), outline=(104, 73, 38, 255), width=4)
    d.ellipse((19, 19, out_size - 19, out_size - 19), fill=(248, 233, 203, 255))
    d.ellipse((23, 23, out_size - 23, out_size - 23), fill=(91, 73, 50, 255))
    path = OUT / name
    img.save(path)
    return path, img.size


def make_atlas(entries):
    pad = 12
    cell = 128
    cols = 6
    rows = (len(entries) + cols - 1) // cols
    atlas = Image.new("RGBA", (cols * cell + pad, rows * cell + pad), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    atlas_meta = {}
    for i, (key, path, size) in enumerate(entries):
        img = Image.open(path).convert("RGBA")
        x = pad + (i % cols) * cell + (cell - img.width) // 2
        y = pad + (i // cols) * cell + (cell - img.height) // 2
        atlas.alpha_composite(img, (x, y))
        atlas_meta[key] = {"x": x, "y": y, "w": img.width, "h": img.height}
    ATLAS.mkdir(parents=True, exist_ok=True)
    atlas_path = ATLAS / "qdao_login_icon_alts_atlas.png"
    atlas.save(atlas_path)
    (ATLAS / "qdao_login_icon_alts_atlas.json").write_text(
        json.dumps(atlas_meta, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return atlas_path


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    src = Image.open(SRC).convert("RGBA")

    specs = [
        ("icon_server_status_red.png", 998, 358, 58, "circle", 64, "foreground icon"),
        ("icon_server_badge_pavilion.png", 1368, 358, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_pagoda.png", 1368, 480, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_sword.png", 1368, 596, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_whirlpool.png", 1368, 716, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_yinyang.png", 1848, 358, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_mountain.png", 1848, 480, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_lotus.png", 1848, 596, 132, "circle", 112, "foreground icon"),
        ("icon_server_badge_talisman.png", 1848, 716, 132, "circle", 112, "foreground icon"),
        ("icon_category_cloud.png", 688, 398, 44, "cutout", 72, "foreground icon"),
        ("icon_category_leaf.png", 689, 476, 44, "cutout", 72, "foreground icon"),
        ("icon_category_yinyang.png", 689, 552, 44, "cutout", 72, "foreground icon"),
        ("icon_category_mountain.png", 689, 631, 44, "cutout", 72, "foreground icon"),
        ("icon_category_flame.png", 689, 716, 44, "cutout", 72, "foreground icon"),
        ("icon_category_swirl.png", 689, 793, 44, "cutout", 72, "foreground icon"),
        ("icon_bottom_profile_mountain.png", 1048, 917, 140, "circle", 112, "foreground icon"),
        ("icon_bottom_status_red.png", 1175, 914, 58, "circle", 64, "foreground icon"),
        ("ornament_top_tab_diamond_left.png", 934, 221, 64, "cutout", 72, "foreground ornament"),
        ("ornament_top_tab_diamond_mid.png", 1219, 221, 64, "cutout", 72, "foreground ornament"),
        ("ornament_top_tab_hook_right.png", 1581, 220, 84, "cutout", 84, "foreground ornament"),
    ]

    made = []
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    for filename, cx, cy, size, mask, out_size, role in specs:
        path, final_size = crop_square(src, cx, cy, size, filename, mask, out_size)
        key = path.stem
        manifest[key] = {
            "file": f"icons/{filename}",
            "size": list(final_size),
            "role": role,
            "source": "q_daoist_login_clear_2560x1080.png",
        }
        made.append((key, path, final_size))

    for filename in ["icon_server_status_empty.png", "icon_bottom_select_empty.png"]:
        path, final_size = draw_empty_ring(filename)
        key = path.stem
        manifest[key] = {
            "file": f"icons/{filename}",
            "size": list(final_size),
            "role": "foreground icon",
            "source": "redrawn from q_daoist_login_clear_2560x1080.png style",
        }
        made.append((key, path, final_size))

    atlas_path = make_atlas(made)
    manifest["qdao_login_icon_alts_atlas"] = {
        "file": "atlas/qdao_login_icon_alts_atlas.png",
        "meta": "atlas/qdao_login_icon_alts_atlas.json",
        "role": "icon atlas preview",
    }
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"wrote {len(made)} icons")
    print(atlas_path)


if __name__ == "__main__":
    main()
