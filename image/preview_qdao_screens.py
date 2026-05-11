"""
Render Python previews for all three qdao screens (SceneScreen /
ServerSelectScreen / LoginScreen). Useful for iterating layout
without the publish → import → play loop.

Reads each XML, composites PNG assets at declared xy/size onto the
scene background, draws text labels and list contents.

Usage: python image/preview_qdao_screens.py
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import xml.etree.ElementTree as ET

ROOT = Path("F:/work/mmorpg/client/fairygui/qdao/assets/qdao")
COMMON = ROOT / "common"
OUT = Path("F:/work/mmorpg/image")

SCREENS = [
    ("SceneScreen.xml",        "_preview_scene_screen.png"),
    ("ServerSelectScreen.xml", "_preview_server_select_screen.png"),
    ("LoginScreen.xml",        "_preview_login_screen.png"),
]


def load_png(name):
    p = ROOT / name
    return Image.open(p).convert("RGBA") if p.exists() else None


def font(size):
    try:
        return ImageFont.truetype("simhei.ttf", size)
    except Exception:
        return ImageFont.load_default()


_PKG_IDS = None
def pkg_name_for_id(src_id):
    global _PKG_IDS
    if _PKG_IDS is None:
        pkg = ET.parse(ROOT / "package.xml").getroot()
        _PKG_IDS = {}
        for r in pkg.find("resources"):
            _PKG_IDS[r.get("id")] = (r.tag, r.get("name"))
    pair = _PKG_IDS.get(src_id)
    return pair[1] if pair else None


def composite_image(canvas, src_id, xy, size):
    name = pkg_name_for_id(src_id)
    if not name:
        return
    img = load_png(name)
    if not img:
        return
    if img.size != size:
        img = img.resize(size, Image.LANCZOS)
    canvas.alpha_composite(img, xy)


def composite_common(canvas, file_name, xy, size, title=None, prompt=None):
    """Composite a common component: find its bg/fg images and stack them,
    then recurse into any nested <component> children (with their own
    Button title) so inline buttons like btnEnter/btnDetail inside a
    QdaoServerCard render too."""
    comp_path = COMMON / file_name
    if not comp_path.exists():
        return
    tree = ET.parse(comp_path)
    comp_root = tree.getroot()
    native_sz = tuple(int(v) for v in comp_root.get("size").split(","))
    sx = size[0] / native_sz[0]
    sy = size[1] / native_sz[1]

    display = comp_root.find("displayList")
    if display is None:
        # Fallback for very simple components: iterate any <image>
        for el in comp_root.iter("image"):
            src = el.get("src")
            gear = el.find("gearDisplay")
            if gear is not None and gear.get("pages") == "1":
                continue
            name = pkg_name_for_id(src)
            if not name:
                continue
            img = load_png(name)
            if img is None:
                continue
            if img.size != size:
                img = img.resize(size, Image.LANCZOS)
            canvas.alpha_composite(img, xy)
    else:
        # Render each child in order, translating by xy and scaling to size
        for el in display:
            tag = el.tag
            if el.get("visible") == "false":
                continue
            cxy = el.get("xy"); csz = el.get("size")
            if not cxy or not csz:
                continue
            cx0, cy0 = (int(v) for v in cxy.split(","))
            cw0, ch0 = (int(v) for v in csz.split(","))
            # Scale child to parent size
            px = xy[0] + int(cx0 * sx)
            py = xy[1] + int(cy0 * sy)
            pw = max(1, int(cw0 * sx))
            ph = max(1, int(ch0 * sy))

            if tag == "image":
                gear = el.find("gearDisplay")
                if gear is not None and gear.get("pages") == "1":
                    continue
                src = el.get("src")
                name = pkg_name_for_id(src)
                if not name:
                    continue
                img = load_png(name)
                if img is None:
                    continue
                if img.size != (pw, ph):
                    img = img.resize((pw, ph), Image.LANCZOS)
                canvas.alpha_composite(img, (px, py))
            elif tag == "text":
                text_val = el.get("text", "")
                # Title/subtitle text that is actually runtime-bound may be empty
                # at design-time. Skip empty strings.
                if not text_val:
                    continue
                d = ImageDraw.Draw(canvas)
                fs = int(int(el.get("fontSize", "20")) * sy)
                color = el.get("color", "#3d2914")
                rc = int(color[1:3], 16); gc = int(color[3:5], 16); bc = int(color[5:7], 16)
                f = font(max(10, fs))
                align = el.get("align", "left")
                bbox = d.textbbox((0, 0), text_val, font=f)
                tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
                tx = px
                ty = py + (ph - th) // 2
                if align == "center":
                    tx = px + (pw - tw) // 2
                elif align == "right":
                    tx = px + pw - tw
                d.text((tx, ty), text_val, fill=(rc, gc, bc), font=f)
            elif tag == "component":
                child_file = el.get("fileName", "").replace("common/", "")
                child_title = None
                btn = el.find("Button")
                if btn is not None:
                    child_title = btn.get("title", "") or None
                # Recurse
                composite_common(canvas, child_file, (px, py), (pw, ph),
                                 title=child_title)
            elif tag == "loader":
                url = el.get("url", "")
                if url and url.startswith("ui://qdao2026"):
                    tail = url[len("ui://qdao2026"):]
                    if tail.startswith("/"):
                        name = tail[1:]
                    else:
                        pair = pkg_name_for_id(tail)
                        name = pair if pair else None
                    if name:
                        ip = ROOT / name
                        if ip.exists():
                            ico = Image.open(ip).convert("RGBA")
                            ico = ico.resize((pw, ph), Image.LANCZOS)
                            canvas.alpha_composite(ico, (px, py))
            # graph / ignore others (hit zones are transparent)

    # After rendering the composite, overlay a top-level title / prompt if
    # the caller passed one. Only do this for simple buttons / search boxes
    # that expose a single logical text slot — not containers like
    # QdaoServerCard that already render their own nested text.
    label = title or prompt
    has_own_text = False
    if display is not None:
        for te in display.iter("text"):
            if te.get("text"):
                has_own_text = True
                break
    if label and not has_own_text:
        d = ImageDraw.Draw(canvas)
        fs = max(18, int(size[1] * 0.45))
        f = font(fs)
        bbox = d.textbbox((0, 0), label, font=f)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx = xy[0] + (size[0] - tw) // 2
        ty = xy[1] + (size[1] - th) // 2 - 4

        # Pick text colour based on component purpose
        if "Primary" in file_name:
            ink = (255, 240, 189)
        elif "SearchBox" in file_name:
            tx = xy[0] + 72
            ink = (140, 100, 60)
        else:
            ink = (61, 41, 20)

        d.text((tx, ty), label, fill=ink, font=f)


def render_list(canvas, list_el):
    xy = tuple(int(v) for v in list_el.get("xy").split(","))
    size = tuple(int(v) for v in list_el.get("size").split(","))
    default = list_el.get("defaultItem", "")
    if "/" not in default:
        return
    item_name = default.split("/")[-1] + ".xml"
    items = list(list_el.iter("item"))
    if not items:
        return
    line_gap = int(list_el.get("lineGap", "0"))
    col_gap = int(list_el.get("columnGap", "0"))
    layout = list_el.get("layout", "single_column")

    comp_path = COMMON / item_name
    if not comp_path.exists():
        return
    comp_root = ET.parse(comp_path).getroot()
    item_size = tuple(int(v) for v in comp_root.get("size").split(","))
    iw, ih = item_size

    if layout == "pagination":
        cols = max(1, (size[0] + col_gap) // (iw + col_gap))
        rows = max(1, (size[1] + line_gap) // (ih + line_gap))
        per_page = cols * rows
        for i, it in enumerate(items[:per_page]):
            cx = xy[0] + (i % cols) * (iw + col_gap)
            cy = xy[1] + (i // cols) * (ih + line_gap)
            composite_common(canvas, item_name, (cx, cy), item_size,
                             title=it.get("title", ""))
            icon = it.get("icon", "")
            if icon and icon.startswith("ui://qdao2026/"):
                icon_name = icon.replace("ui://qdao2026/", "")
                ip = ROOT / icon_name
                if ip.exists():
                    ico = Image.open(ip).convert("RGBA")
                    ico = ico.resize((112, 112), Image.LANCZOS)
                    canvas.alpha_composite(ico, (cx + 20, cy + 8))
    else:
        # single-column / flow_vt: stack vertically, shrink-to-fit width
        display_size = (size[0], ih)
        cy = xy[1]
        for it in items:
            if cy + ih > xy[1] + size[1]:
                break
            composite_common(canvas, item_name, (xy[0], cy), display_size,
                             title=it.get("title", ""))
            icon = it.get("icon", "")
            if icon and icon.startswith("ui://qdao2026/"):
                icon_name = icon.replace("ui://qdao2026/", "")
                ip = ROOT / icon_name
                if ip.exists():
                    ico = Image.open(ip).convert("RGBA")
                    # Tab icons are smaller (64x64); badges are 112x112
                    icon_side = min(72, ih - 16)
                    ico = ico.resize((icon_side, icon_side), Image.LANCZOS)
                    canvas.alpha_composite(ico, (xy[0] + 24, cy + (ih - icon_side) // 2))
            cy += ih + line_gap


def render_screen(screen_xml, out_path, title_for_tag):
    tree = ET.parse(ROOT / screen_xml)
    root = tree.getroot()
    W, H = (int(v) for v in root.get("size").split(","))
    canvas = Image.new("RGBA", (W, H), (40, 50, 50, 255))

    display = root.find("displayList")
    if display is None:
        return

    for el in display:
        if el.get("visible") == "false":
            continue
        tag = el.tag
        xy = el.get("xy"); sz = el.get("size")
        if not xy or not sz:
            continue
        xy_t = tuple(int(v) for v in xy.split(","))
        sz_t = tuple(int(v) for v in sz.split(","))

        if tag == "image":
            composite_image(canvas, el.get("src"), xy_t, sz_t)
        elif tag == "text":
            text = el.get("text", "")
            if not text:
                continue
            d = ImageDraw.Draw(canvas)
            fs = int(el.get("fontSize", "20"))
            color = el.get("color", "#3d2914")
            rc = int(color[1:3], 16); gc = int(color[3:5], 16); bc = int(color[5:7], 16)
            f = font(fs)
            align = el.get("align", "left")
            bbox = d.textbbox((0, 0), text, font=f)
            tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
            tx = xy_t[0]
            ty = xy_t[1] + (sz_t[1] - th) // 2
            if align == "center":
                tx = xy_t[0] + (sz_t[0] - tw) // 2
            elif align == "right":
                tx = xy_t[0] + sz_t[0] - tw
            d.text((tx, ty), text, fill=(rc, gc, bc), font=f)
        elif tag == "component":
            file_name = el.get("fileName", "").replace("common/", "")
            title = ""
            btn = el.find("Button")
            if btn is not None:
                title = btn.get("title", "")
            prompt = None
            for prop in el.iter("prop"):
                if prop.get("propValue") == "prompt":
                    prompt = prop.get("propText", "")
                    break
            composite_common(canvas, file_name, xy_t, sz_t,
                             title=title or None, prompt=prompt)
        elif tag == "list":
            render_list(canvas, el)

    # Save as RGB jpeg-equivalent PNG
    canvas.convert("RGB").save(out_path, quality=88)
    print(f"  wrote: {out_path}  ({canvas.size})")


def main():
    for xml_name, out_name in SCREENS:
        print(f"Rendering {xml_name} ...")
        render_screen(xml_name, OUT / out_name, xml_name[:-4])

    # Build a composite comparison (reference mock + all 3 previews)
    mock = Image.open("F:/work/mmorpg/image/q_daoist_login_clear_2560x1080.png").convert("RGB")
    combo = Image.new("RGB", (2560, 1080 * 4 + 180), (25, 25, 25))
    d = ImageDraw.Draw(combo)
    f = font(44)
    d.text((30, 40), "Reference (effect mock)", fill=(220, 200, 140), font=f)
    combo.paste(mock, (0, 100))
    for i, (_, out_name) in enumerate(SCREENS):
        y = 100 + 1080 * (i + 1) + 80 * (i + 1)
        d.text((30, y - 60), {
            "_preview_scene_screen.png":        "SceneScreen (preview)",
            "_preview_server_select_screen.png": "ServerSelectScreen (preview)",
            "_preview_login_screen.png":         "LoginScreen (preview)",
        }[out_name], fill=(220, 200, 140), font=f)
        im = Image.open(OUT / out_name).convert("RGB")
        combo.paste(im, (0, y))
    combo.thumbnail((1600, 2700))
    combo.save(OUT / "_preview_all.png", quality=82)
    print(f"\nCombined preview: image/_preview_all.png ({combo.size})")


if __name__ == "__main__":
    main()
