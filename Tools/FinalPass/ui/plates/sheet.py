"""Contact sheet of every plate in story order, the caption's text box outlined faintly.

    python3 sheet.py   -> out/sheet.png
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import look
from plates import PLATES, OUT, FELL

ORDER = ['Intro_02', 'Intro_03', 'Intro_04', 'Intro_06', 'Intro_07', 'EndA_02', 'EndA_03', 'EndB_01', 'EndB_03',
         'EndB_04', 'Confess_01']


def main(cols=3, w=640):
    h = w * 9 // 16
    names = [n for n in ORDER if (OUT / f'{n}.png').exists()]
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * w + (cols + 1) * 12, rows * (h + 34) + 12), (24, 22, 20))
    d = ImageDraw.Draw(sheet)
    f = ImageFont.truetype(FELL, 20)
    k = w / look.W
    for i, n in enumerate(names):
        im = Image.open(OUT / f'{n}.png').convert('RGB').resize((w, h), Image.LANCZOS)
        x, y = 12 + (i % cols) * (w + 12), 12 + (i // cols) * (h + 34)
        sheet.paste(im, (x, y + 26))
        side = PLATES[n][0]
        x0, y0, x1, y1 = look.TEXT_BOX[side]
        d.rectangle([x + x0 * k, y + 26 + y0 * k, x + x1 * k, y + 26 + y1 * k], outline=(110, 70, 60))
        d.text((x, y + 2), f'{n}   caption {side}', font=f, fill=(200, 190, 170))
    sheet.save(OUT / 'sheet.png')
    print('built sheet')


if __name__ == '__main__':
    main()
