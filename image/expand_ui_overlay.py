"""
Make the UI overlay transparent where it's near-white (the "blank canvas"
part) and keep only the UI plate art in the middle.

Strategy:
  1. Load source overlay (RGB, 1930x815, ~80% white background).
  2. Build alpha channel:
       - pixels with brightness > 245 AND low saturation → alpha 0 (transparent)
       - pixels darker / more saturated → keep, alpha 255
       - smooth edges with a small Gaussian blur on alpha for AA
  3. Place into a 2560x1080 RGBA canvas, centered (xy=315,132), surrounding
     pixels transparent.
  4. Save as qdao_ui_overlay_bg.png so it can be composited *on top of*
     qdao_scene_bg.png.

Run:  python image/expand_ui_overlay.py
"""
import cv2
import numpy as np
from pathlib import Path

SRC = "F:/work/mmorpg/image/q_daoist_login_ui_overlay_no_controls_generated_last.png"
DST = "F:/work/mmorpg/client/fairygui/qdao/assets/qdao/qdao_ui_overlay_bg.png"

# Read source as BGR; we'll add alpha ourselves.
src = cv2.imread(SRC, cv2.IMREAD_COLOR)
sh, sw = src.shape[:2]
print(f"source overlay: {sw}x{sh}")

# Compute brightness + saturation to decide what's "blank background"
hsv = cv2.cvtColor(src, cv2.COLOR_BGR2HSV)
S = hsv[..., 1]
V = hsv[..., 2]
gray = src.mean(axis=2)

# Alpha: 0 where (brightness > 245 AND saturation < 30)  — i.e. flat white
# 255 elsewhere — keep all UI plate / panels / decorations
white_mask = (gray > 245) & (S < 30)
print(f"white-background pixels: {white_mask.sum()} ({100*white_mask.mean():.1f}%)")

alpha = np.full((sh, sw), 255, np.uint8)
alpha[white_mask] = 0

# Smooth the alpha edge so the UI plate doesn't show hard cutouts.
alpha = cv2.GaussianBlur(alpha, (5, 5), 1.0)

# Stack BGR + A
rgba = cv2.merge([src[..., 0], src[..., 1], src[..., 2], alpha])

# Place into a 2560x1080 transparent canvas, centered
TW, TH = 2560, 1080
canvas = np.zeros((TH, TW, 4), np.uint8)  # fully transparent
cx = (TW - sw) // 2
cy = (TH - sh) // 2
canvas[cy: cy + sh, cx: cx + sw] = rgba
print(f"placed at ({cx},{cy}) in a {TW}x{TH} transparent canvas")

# Save
Path(DST).parent.mkdir(parents=True, exist_ok=True)
cv2.imwrite(DST, canvas, [cv2.IMWRITE_PNG_COMPRESSION, 6])

# Verify the result
import os
print(f"wrote: {DST}  ({os.path.getsize(DST)//1024} KB)")
# Re-read to confirm alpha
back = cv2.imread(DST, cv2.IMREAD_UNCHANGED)
print(f"final shape: {back.shape}, dtype: {back.dtype}")
back_alpha = back[..., 3]
print(f"final alpha: min={back_alpha.min()} max={back_alpha.max()} "
      f"transparent_pct={100*(back_alpha == 0).mean():.1f}% "
      f"opaque_pct={100*(back_alpha == 255).mean():.1f}%")
