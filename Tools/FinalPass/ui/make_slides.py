"""Assemble the twenty story plates in slides/<Name>.png, ready for import_ui_assets.py.

Document and drawing plates come from plates/out (built by plates/plates.py). Photo plates are real game
stills from capture_ui.py (TALLOW_UI_CAPTURE=stills), graded into the soot-and-tallow world by the same
plate grade (plates/look.py: grade_photo). Title plates are a still taken down nearly to soot, so the
wordmark the game sets over it reads. No caption is baked in: the game sets the words in Fell.

  python3 make_slides.py <stills dir>     (default: the ui-final copy's Saved/Presentation/Capture/stills)
"""
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'plates'))
import look  # noqa: E402

STILLS = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3] / 'Tallow/Saved/Presentation/Capture/stills'
KEEP = STILLS.parent / 'stills-keep'   # the playable-map stills chosen for plates (the run layout moves furniture between runs)
OUT = HERE / 'slides'
DOCS = HERE / 'plates' / 'out'

# Plate name -> (still, caption side). 'T' is a title plate; 'M' the morning grade and 'D' a darker night,
# both with the caption on the left.
PHOTOS = {
    'Intro_01': ('menu_empty', 'L'),           # the chair with no child in it: one at a time
    'Intro_05': ('stairs_a', 'L'),             # a staircase
    'Intro_08': ('hall_hand', 'L'),            # her candle, at the foot of the stairs
    'Intro_09': ('menu_nell', 'L'),            # Nell in the chair (the menu's close-up)
    'Intro_Title': ('menu_nell', 'T'),
    'EndA_01': ('menu_empty', 'M'),            # the same empty chair in the grey of morning: she got out
    'EndA_04': ('stairs_a', 'T'),
    'EndB_02': ('menu_mim', 'L'),              # Mim, in the menu's flash
    'Confess_02': ('menu_nell', 'D'),          # the house had been waiting: the first picture again, darker
}
DOC_PLATES = ['Intro_02', 'Intro_03', 'Intro_04', 'Intro_06', 'Intro_07', 'EndA_02', 'EndA_03', 'EndB_01', 'EndB_03', 'EndB_04', 'Confess_01']


def title_plate(img, seed):
    """A still sunk almost to soot: only the brightest shapes survive, far below the wordmark's ivory."""
    graded = np.asarray(look.grade_photo(img, 'L', seed=seed), np.float64) / 255.0
    h, w = graded.shape[:2]
    y, x = np.mgrid[0:h, 0:w]
    # Darkest across the middle band where the wordmark and its line sit; a linear fall, no vignette.
    band = 1.0 - .82 * np.clip(1.0 - np.abs(y / h - .47) / .3, 0, 1)
    return look.to_image(graded * .42 * band[..., None])


def morning_plate(img, seed):
    """The same grade lifted and drained toward grey: the light that comes before anyone opens the door."""
    graded = look.grade_photo(img, 'L', seed=seed, desat=.85, target_median=34.0 / 255.0)
    rgb = np.asarray(graded, np.float64) / 255.0
    grey = rgb.mean(axis=2, keepdims=True)
    rgb = .55 * rgb + .45 * grey * np.array([.97, 1.0, 1.04])
    return look.to_image(rgb)


def main():
    OUT.mkdir(exist_ok=True)
    for name in DOC_PLATES:
        src = DOCS / f'{name}.png'
        if src.exists():
            shutil.copy2(src, OUT / f'{name}.png')
            print('doc', name)
        else:
            print('MISSING doc plate', name)
    for i, (name, (still, side)) in enumerate(PHOTOS.items()):
        src = STILLS / f'{still}.png'
        if not src.exists() and (KEEP / f'{still}.png').exists():
            src = KEEP / f'{still}.png'
        if not src.exists():
            print('MISSING still', still)
            continue
        img = Image.open(src).convert('RGB').resize((1920, 1080), Image.LANCZOS)
        # A still darker than the grade's target is opened up first (exposure only), so the grade has form to keep.
        raw = np.asarray(img, np.float64) / 255.0
        med = float(np.median(raw.mean(axis=2)))
        if 0 < med < 30.0 / 255.0:
            img = look.to_image(np.clip(raw * min(4.0, (30.0 / 255.0) / med), 0, 1))
        if side == 'T':
            plate = title_plate(img, 40 + i)
        elif side == 'M':
            plate = morning_plate(img, 40 + i)
        elif side == 'D':
            plate = look.grade_photo(img, 'L', seed=40 + i, target_median=12.0 / 255.0, ramp=.97)
        else:
            plate = look.grade_photo(img, side, seed=40 + i)
        look.save_png(plate, OUT / f'{name}.png',
                      f'Tools/FinalPass/ui/make_slides.py: a real TALLOW game still ({still}, rendered by Unreal from '
                      'Lvl_Tallow_Playable or Lvl_MenuPresentation by capture_ui.py), graded by plates/look.py. No generated or traced pixels.')
        print('photo', name, 'from', still)


if __name__ == '__main__':
    main()
