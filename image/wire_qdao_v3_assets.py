#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Wire the v3 atlases (redrawn UI + 124 weapon/relic icons + 23 character
portraits) into the existing qdao FairyGUI package as a /v3/ subfolder.

Idempotent: re-running overwrites copied PNGs and per-asset .png.xml meta,
and rewrites the appended <image> entries for /v3/ paths. Existing
non-v3 entries are preserved exactly.

Run:
    python image/wire_qdao_v3_assets.py
"""
from __future__ import annotations

import os
import re
import shutil
import sys
import pathlib
import typing

import PIL

REPO: pathlib.Path = pathlib.Path(__file__).resolve().parent.parent
IMAGE_DIR: pathlib.Path = REPO / "image"
PKG_DIR: pathlib.Path = REPO / "client" / "fairygui" / "qdao" / "assets" / "qdao"
PACKAGE_XML: pathlib.Path = PKG_DIR / "package.xml"

# --- Source roots ---------------------------------------------------------
UI_SRC: pathlib.Path = IMAGE_DIR / "q_daoist_login_ui_10240_redraw_clear_final_layers" / "q_daoist_login_buttons_redrawn_atomic"
WEAPONS_DIR_A: pathlib.Path = UI_SRC / "qstyle_redrawn_600x600" / "west_eight_immortals_redrawn_100_600x600"
WEAPONS_DIR_B: pathlib.Path = UI_SRC / "qstyle_redrawn_600x600" / "character_artifacts_redrawn_24_600x600"
CHAR_SRC: pathlib.Path = IMAGE_DIR / "q_daoist_character_pack_4096"

# --- ID allocation --------------------------------------------------------
# Existing entries end at 0x0001003b. v3 entries start at 0x00020000 to
# leave plenty of room for future additions in the legacy / root path.
V3_ID_BASE = 0x00020000

# --- UI bg/icons spec (single layer, redrawn) ----------------------------
# (source_filename, target_name, target_w, target_h, scale9 (l,t,w,h) or None)
UI_PIECES: list[tuple[str, str, int, int, tuple[int, int, int, int] | None]] = [
    # Tab / button (idle vs active green-fill)
    ("top_button_green_selected.png",          "tab_button_green_active_v3.png", 360, 114, (44, 28, 272, 58)),
    ("list_bg_unselected.png",                 "list_row_idle_v3.png",           360,  96, (40, 22, 280, 52)),
    ("list_bg_selected_green.png",             "list_row_active_v3.png",         360,  96, (40, 22, 280, 52)),
    # Server cards (wide / medium, idle vs active green)
    ("server_card_bg_wide.png",                "server_card_wide_idle_v3.png",   640, 141, (60, 30, 520, 80)),
    ("server_card_bg_wide_selected_green.png", "server_card_wide_active_v3.png", 640, 141, (60, 30, 520, 80)),
    ("server_card_bg_medium.png",              "server_card_med_idle_v3.png",    520, 126, (60, 30, 400, 66)),
    ("server_card_bg_medium_selected_green.png","server_card_med_active_v3.png", 520, 126, (60, 30, 400, 66)),
    # Bottom bar
    ("bottom_bar_bg_green_white.png",          "bottom_bar_v3.png",             1280,  79, (60, 16, 1160, 48)),
    # Search box (already has icon baked in)
    ("search_box_with_icon.png",               "search_box_with_icon_v3.png",    420,  84, (60, 20, 280, 44)),
    # Misc
    ("status_red_dot.png",                     "status_red_dot_v3.png",           32,  32, None),
    ("ornament_gold_flower.png",               "ornament_gold_flower_v3.png",    120, 120, None),
]

# Badge sheet is left as a single un-cut texture for now; user can slice
# in editor if needed.
UI_SHEETS: list[tuple[str, str, int, int]] = [
    ("ai_qstyle_badges_sheet_chroma.png",      "badges_chroma_sheet_v3.png",   1024, 512),
]

# --- Weapon/relic icon roots ---------------------------------------------
WEAPON_ICON_SIZE = 256  # downscaled from 600

# --- Character portrait spec ---------------------------------------------
CHAR_PORTRAIT_SIZE = 1024  # downscaled from 4096

# 23 character files we want (excludes 00_reference and the uncropped one)
CHAR_PATTERN: re.Pattern[str] = re.compile(r"^\d{2}_.+_transparent_4096\.png$")


def _meta_xml(image_id: str, name: str, path: str, w: int, h: int,
              scale9: tuple[int, int, int, int] | None = None,
              raw_bg: bool = False) -> str:
    attrs: list[str] = [
        f'id="{image_id}"',
        f'name="{name}"',
        f'path="{path}"',
        'exported="true"',
        f'size="{w},{h}"',
    ]
    if scale9 is not None:
        l, t, ww, hh = scale9
        attrs.append('scale="9grid"')
        attrs.append(f'scale9grid="{l},{t},{ww},{hh}"')
    if raw_bg:
        attrs.append('duplicatePadding="true"')
        attrs.append('disableTrim="true"')
    attrs.append('qualityOption="source"')
    return '<image ' + ' '.join(attrs) + '/>'


def _write_png_xml(target_png: pathlib.Path, entry_xml: str) -> None:
    meta_path: pathlib.Path = target_png.with_suffix(target_png.suffix + ".xml")
    meta_path.write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n' + entry_xml + '\n',
        encoding="utf-8",
    )


def _resize_and_save(src: pathlib.Path, dst: pathlib.Path, w: int, h: int) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    with PIL.Image.open(src) as im:
        if im.mode != "RGBA":
            im = im.convert("RGBA")
        if im.size != (w, h):
            im = im.resize((w, h), PIL.Image.LANCZOS)
        im.save(dst, format="PNG", optimize=True)


def collect_ui_entries(next_id: list[int]) -> list[str]:
    entries: list[str] = []
    target_dir: pathlib.Path = PKG_DIR / "v3" / "ui"

    for src_name, dst_name, w, h, scale9 in UI_PIECES:
        src: pathlib.Path = UI_SRC / src_name
        if not src.exists():
            print(f"  [skip] missing: {src.relative_to(REPO)}", file=sys.stderr)
            continue
        dst: pathlib.Path = target_dir / dst_name
        _resize_and_save(src, dst, w, h)
        image_id: str = f"{next_id[0]:08x}"; next_id[0] += 1
        xml: str = _meta_xml(image_id, dst_name, "/v3/ui/", w, h, scale9)
        _write_png_xml(dst, xml)
        entries.append("    " + xml)

    for src_name, dst_name, w, h in UI_SHEETS:
        src: pathlib.Path = UI_SRC / src_name
        if not src.exists():
            print(f"  [skip] missing sheet: {src.relative_to(REPO)}", file=sys.stderr)
            continue
        dst: pathlib.Path = target_dir / dst_name
        _resize_and_save(src, dst, w, h)
        image_id: str = f"{next_id[0]:08x}"; next_id[0] += 1
        # Sheets shouldn't be trimmed (we cut cells from them later).
        xml: str = _meta_xml(image_id, dst_name, "/v3/ui/", w, h, raw_bg=True)
        _write_png_xml(dst, xml)
        entries.append("    " + xml)

    print(f"[ui] wrote {len([e for e: str in entries])} entries to {target_dir.relative_to(REPO)}")
    return entries


def collect_weapon_entries(next_id: list[int]) -> list[str]:
    entries: list[str] = []
    target_dir: pathlib.Path = PKG_DIR / "v3" / "icons_weapon"
    sources: list[pathlib.Path] = []
    for root: pathlib.Path in (WEAPONS_DIR_A, WEAPONS_DIR_B):
        if not root.exists():
            print(f"  [skip] missing weapon dir: {root}", file=sys.stderr)
            continue
        sources.extend(sorted(p for p: pathlib.Path in root.glob("*.png")
                              if not p.name.startswith("contact_sheet")))

    for src: pathlib.Path in sources:
        dst: pathlib.Path = target_dir / src.name
        _resize_and_save(src, dst, WEAPON_ICON_SIZE, WEAPON_ICON_SIZE)
        image_id: str = f"{next_id[0]:08x}"; next_id[0] += 1
        xml: str = _meta_xml(image_id, src.name, "/v3/icons_weapon/",
                        WEAPON_ICON_SIZE, WEAPON_ICON_SIZE)
        _write_png_xml(dst, xml)
        entries.append("    " + xml)

    print(f"[weapons] wrote {len(entries)} icons to {target_dir.relative_to(REPO)}")
    return entries


def collect_character_entries(next_id: list[int]) -> list[str]:
    entries: list[str] = []
    target_dir: pathlib.Path = PKG_DIR / "v3" / "characters"
    sources: list[Path] = sorted(
        p for p: pathlib.Path in CHAR_SRC.glob("*.png")
        if CHAR_PATTERN.match(p.name)
        and not p.name.startswith("00_reference")
    )

    for src: pathlib.Path in sources:
        # Compact target name: strip the verbose _transparent_4096 suffix.
        stem: str = src.stem.replace("_transparent_4096", "")
        dst_name: str = f"{stem}_v3.png"
        dst: pathlib.Path = target_dir / dst_name
        _resize_and_save(src, dst, CHAR_PORTRAIT_SIZE, CHAR_PORTRAIT_SIZE)
        image_id: str = f"{next_id[0]:08x}"; next_id[0] += 1
        xml: str = _meta_xml(image_id, dst_name, "/v3/characters/",
                        CHAR_PORTRAIT_SIZE, CHAR_PORTRAIT_SIZE)
        _write_png_xml(dst, xml)
        entries.append("    " + xml)

    print(f"[characters] wrote {len(entries)} portraits to {target_dir.relative_to(REPO)}")
    return entries


def rewrite_package_xml(new_entries: typing.Iterable[str]) -> None:
    text: str = PACKAGE_XML.read_text(encoding="utf-8")

    # Remove any previously-wired v3 entries before appending.
    text: str = re.sub(
        r'\n    <image [^/]*path="/v3/[^"]+"[^/]*/>',
        "",
        text,
    )

    insertion: str = "\n" + "\n".join(new_entries)
    text: str = text.replace("  </resources>", insertion + "\n  </resources>", 1)

    backup: pathlib.Path = PACKAGE_XML.with_suffix(".xml.bak")
    if not backup.exists():
        shutil.copy2(PACKAGE_XML, backup)
    PACKAGE_XML.write_text(text, encoding="utf-8")
    print(f"[package.xml] appended {sum(1 for _: str in new_entries) if not isinstance(new_entries, list) else len(new_entries)} entries (backup -> {backup.name})")


def main() -> int:
    if not PACKAGE_XML.exists():
        print(f"package.xml not found at {PACKAGE_XML}", file=sys.stderr)
        return 1

    next_id: list[int] = [V3_ID_BASE]
    all_entries: list[str] = []
    all_entries.extend(collect_ui_entries(next_id))
    all_entries.extend(collect_weapon_entries(next_id))
    all_entries.extend(collect_character_entries(next_id))
    rewrite_package_xml(all_entries)
    print(f"done. total v3 entries: {len(all_entries)}, next free id: 0x{next_id[0]:08x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
