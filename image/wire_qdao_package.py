"""
Wire ui_controls_v2 + the clean scene background into the FairyGUI qdao
package.

Steps:
  1. Copy every PNG from image/ui_controls_v2/{backgrounds,foregrounds,icons}
     and image/q_daoist_scene_bg_2560x1080.png into
     client/fairygui/qdao/assets/qdao/.
  2. Generate one package.xml that registers every PNG plus the three screen
     components and the common components we'll be authoring.
  3. Write a .png.xml meta beside every PNG. 9-grid params are filled in
     for the stretchable templates (panels, bars, buttons, tabs).

Run after: python image/wire_qdao_package.py
"""
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parent.parent
SRC_IMG = ROOT / "image"
SRC_UI = SRC_IMG / "ui_controls_v2"
DST = ROOT / "client/fairygui/qdao/assets/qdao"
DST_COMMON = DST / "common"
SCENE_BG = SRC_IMG / "q_daoist_scene_bg_2560x1080.png"

DST.mkdir(parents=True, exist_ok=True)
DST_COMMON.mkdir(parents=True, exist_ok=True)


def png_size(p: Path):
    with p.open("rb") as f:
        f.read(16)
        w = struct.unpack(">I", f.read(4))[0]
        h = struct.unpack(">I", f.read(4))[0]
    return w, h


# Stable id allocator: id -> (8-hex). Existing screens keep their ids so the
# project doesn't lose previous .objs metadata.
NEXT_ID = 0x10000  # well clear of older a000000? range
def next_id():
    global NEXT_ID
    s = f"{NEXT_ID:08x}"
    NEXT_ID += 1
    return s


# Reserved component ids the project already uses.
COMPONENTS = [
    ("a0000001", "LoginScreen.xml", "/"),
    ("a0000002", "ServerSelectScreen.xml", "/"),
    ("a0000005", "SceneScreen.xml", "/"),
    ("c0000001", "QdaoButtonPrimary.xml", "/common/"),
    ("c0000002", "QdaoButtonSecondary.xml", "/common/"),
    ("c0000003", "QdaoSearchBox.xml", "/common/"),
    ("c0000004", "QdaoTabIdle.xml", "/common/"),
    ("c0000005", "QdaoTabActive.xml", "/common/"),
    ("c0000006", "QdaoServerCard.xml", "/common/"),
    ("c0000007", "QdaoBottomBar.xml", "/common/"),
    ("c0000008", "QdaoMainPanel.xml", "/common/"),
]

# 9-grid hints for stretchable templates. (left, top, w, h) into the slice.
NINE_GRID = {
    "panel_main_1280x760_bg.png": (60, 60, 1160, 640),
    "panel_main_1280x760_fg.png": (60, 60, 1160, 640),
    "bottom_bar_1120x148_bg.png": (40, 28, 1040, 92),
    "bottom_bar_1120x148_fg.png": (40, 28, 1040, 92),
    "button_primary_480x128_bg.png":   (28, 28, 424, 72),
    "button_primary_480x128_fg.png":   (28, 28, 424, 72),
    "button_secondary_420x112_bg.png": (28, 28, 364, 56),
    "button_secondary_420x112_fg.png": (28, 28, 364, 56),
    "search_box_420x76_bg.png": (24, 18, 372, 40),
    "search_box_420x76_fg.png": (24, 18, 372, 40),
    "server_card_640x128_bg.png": (32, 24, 576, 80),
    "server_card_640x128_fg.png": (32, 24, 576, 80),
    "tab_idle_360x88_bg.png":   (32, 22, 296, 44),
    "tab_idle_360x88_fg.png":   (32, 22, 296, 44),
    "tab_active_360x88_bg.png": (32, 22, 296, 44),
    "tab_active_360x88_fg.png": (32, 22, 296, 44),
    "q_daoist_scene_bg_2560x1080.png": None,  # full-bleed, no 9-grid
}


def collect_pngs():
    """Yield (src_path, dst_filename) pairs for every PNG to import."""
    # ui_controls_v2 sub-folders - copy with a flat dst filename.
    for sub in ("backgrounds", "foregrounds", "icons"):
        for p in sorted((SRC_UI / sub).glob("*.png")):
            yield p, p.name
    # Scene background
    yield SCENE_BG, "qdao_scene_bg.png"


def copy_assets():
    pngs = []
    for src, name in collect_pngs():
        dst = DST / name
        if not dst.exists() or src.stat().st_mtime > dst.stat().st_mtime:
            shutil.copy2(src, dst)
        w, h = png_size(dst)
        pngs.append((name, w, h))
    return pngs


def write_png_meta(name, w, h, image_id):
    p = DST / f"{name}.xml"
    grid = NINE_GRID.get(name)
    extra_attrs = ""
    if grid is not None:
        gx, gy, gw, gh = grid
        extra_attrs = f' scale="9grid" scale9grid="{gx},{gy},{gw},{gh}"'
    if name == "qdao_scene_bg.png":
        extra_attrs += ' duplicatePadding="true" disableTrim="true"'
    p.write_text(
        f'<?xml version="1.0" encoding="utf-8"?>\n'
        f'<image id="{image_id}" name="{name}" path="/" exported="true" '
        f'size="{w},{h}"{extra_attrs} qualityOption="source"/>\n',
        encoding="utf-8",
    )


def write_package_xml(pngs):
    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<packageDescription id="qdao2026">',
        '  <resources>',
    ]
    # Components first
    for cid, name, path in COMPONENTS:
        lines.append(f'    <component id="{cid}" name="{name}" path="{path}" exported="true"/>')

    # Images
    for name, w, h in pngs:
        image_id = next_id()
        grid = NINE_GRID.get(name)
        attrs = f'id="{image_id}" name="{name}" path="/" exported="true" size="{w},{h}"'
        if grid is not None:
            gx, gy, gw, gh = grid
            attrs += f' scale="9grid" scale9grid="{gx},{gy},{gw},{gh}"'
        if name == "qdao_scene_bg.png":
            attrs += ' duplicatePadding="true" disableTrim="true"'
        attrs += ' qualityOption="source"'
        lines.append(f"    <image {attrs}/>")
        write_png_meta(name, w, h, image_id)
    lines += [
        '  </resources>',
        '  <publish name="qdao"/>',
        '</packageDescription>',
        '',
    ]
    (DST / "package.xml").write_text("\n".join(lines), encoding="utf-8")


def main():
    pngs = copy_assets()
    write_package_xml(pngs)
    print(f"Imported {len(pngs)} PNGs.")
    print(f"  package.xml: {len(COMPONENTS)} components + {len(pngs)} images")
    print(f"Run FairyGUI Editor to see the resources, then publish (F8).")


if __name__ == "__main__":
    main()
