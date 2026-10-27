#!/usr/bin/env python3
"""Regenerates Assets/background.jpg from Assets/source/background-original.webp.

The AI-generated artwork has captions in the MIX/TONE / SYNC area that do not line up
with the controls.  This script lifts those captions out as sprites, repairs the plate
texture underneath, and pastes the captions again at their new, aligned positions
(every control gets its own caption slot).  Design coordinates are 1920 x 1280.

Usage:  pip install pillow numpy && python3 tools/make_background.py
"""
import os
import numpy as np
from PIL import Image, ImageFilter, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Assets", "source", "background-original.webp")
OUT = os.path.join(ROOT, "Assets", "background.jpg")

img = Image.open(SRC).convert("RGB")
arr = np.array(img).astype(float)
lum = arr.mean(axis=2)

# ---- 1. lift the captions out as sprites (RGBA, alpha from brightness) -------
def sprite(box):
    x0, y0, x1, y1 = box
    crop = img.crop((x0, y0, x1 + 1, y1 + 1))
    l = np.array(crop).astype(float).mean(axis=2)
    a = np.clip((l - 95.0) / 95.0, 0, 1)
    rgba = np.dstack([np.array(crop), (a * 255).astype(np.uint8)]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")

S = {
    "ECHO":    sprite((1443, 710, 1515, 752)),   # two-line captions
    "REVERB":  sprite((1595, 710, 1667, 753)),
    "BASS":    sprite((1336, 799, 1382, 818)),
    "TREBLE":  sprite((1444, 800, 1512, 818)),
    "OUTPUT":  sprite((1596, 800, 1668, 818)),
    "SYNC":    sprite((994, 698, 1041, 716)),
    "NOTE":    sprite((1241, 698, 1286, 716)),
    "FREE":    sprite((990, 834, 1038, 854)),
    "HOST":    sprite((1096, 834, 1149, 854)),
    "Q":       sprite((1205, 833, 1232, 856)),   # 1/4
    "E":       sprite((1243, 833, 1269, 856)),   # 1/8
    "ED":      sprite((1280, 833, 1321, 856)),   # 1/8D
    "MIXTONE": sprite((1407, 515, 1516, 541)),
}

# ---- 2. repair the plate texture where the captions were --------------------------
def feathered_paste(dst, patch, xy, feather=4):
    m = Image.new("L", patch.size, 0)
    ImageDraw.Draw(m).rectangle((feather, feather, patch.width - feather - 1, patch.height - feather - 1), fill=255)
    dst.paste(patch, xy, m.filter(ImageFilter.GaussianBlur(feather * 0.7)))

x0, x1 = 985, 1692
top = img.crop((x0, 556, x1, 682)).transpose(Image.FLIP_TOP_BOTTOM)    # 126 rows of clean plate
bottom = img.crop((x0, 556, x1, 620))                                  # 64 rows
fill = Image.new("RGB", (x1 - x0, 126 + 64))
fill.paste(top, (0, 0)); fill.paste(bottom, (0, 126))                  # one patch: no visible seam
feathered_paste(img, fill, (x0, 684))
feathered_paste(img, img.crop((1400, 556, 1525, 596)), (1400, 505))     # old MIX/TONE heading

# ---- 3. paste the captions at their new, aligned places ---------------------------------
def put(name, cx, top):
    sp = S[name]
    img.paste(sp, (int(round(cx - sp.width / 2)), int(top)), sp)

COLS = [1021, 1177, 1334, 1491, 1647]          # knob columns (same as the TEMPO/TAPE row)
put("MIXTONE", 1462, 507)
for name, cx in zip(["BASS", "TREBLE", "OUTPUT", "ECHO", "REVERB"], COLS):
    put(name, cx, 697)

SYNC_X, NOTE_X = 1099, 1334                      # switch centres
put("SYNC", SYNC_X, 731)
put("FREE", SYNC_X - 58, 846)
put("HOST", SYNC_X + 58, 846)
put("NOTE", NOTE_X, 731)
put("Q", NOTE_X - 46, 846)
put("E", NOTE_X, 846)
put("ED", NOTE_X + 46, 846)

img.save(OUT, quality=93, optimize=True)
print("written", OUT)
