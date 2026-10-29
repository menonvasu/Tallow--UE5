"""TALLOW story plates: the papers and drawings of the intro, the endings and the confession.

    python3 plates.py              build every plate, the wordmark and the contact sheet
    python3 plates.py Intro_02 ... build only the named plates

Each plate is one object lying on the soot under one candle, art on one side, the other side
left dark for the headline and sub-line the game sets natively in IM Fell English. No caption
is baked in. Every word and stroke is placed here by hand; paper, ink and crayon are
procedural (paper.py). Outputs: out/<Name>.png (1920x1080 RGB, with a provenance chunk).
"""
import math
import sys
from pathlib import Path

import numpy as np

import look
import drawing
import paper
from paper import Sheet, Table, spline, wander, resample

HERE = Path(__file__).resolve().parent
OUT = HERE / 'out'
F = HERE / 'fonts'
FELL = str(F / 'IMFellEnglish-Regular.ttf')
FELL_I = str(F / 'IMFellEnglish-Italic.ttf')
FELL_SC = str(F / 'IMFeENsc28P.ttf')
CANON = str(F / 'IMFeFCrm28P.ttf')
NEWS_BOLD = str(F / 'OldStandard-Bold.ttf')
TYPED = str(F / 'SpecialElite-Regular.ttf')
COPPER = str(F / 'PinyonScript-Regular.ttf')         # the register's careful hand
ADULT = str(F / 'EuphoriaScript-Regular.ttf')        # the constable's letter, a plain running hand
MAISIE = str(F / 'ReenieBeanie.ttf')
NELL = str(F / 'ShadowsIntoLightTwo-Regular.ttf')

FONT_NOTE = ('IM FELL English and IM FELL English SC and IM FELL French Canon (Igino Marini, SIL OFL 1.1); '
             'Old Standard TT (Alexey Kryukov, SIL OFL 1.1); Special Elite (Astigmatic, Apache 2.0); '
             'Pinyon Script (Nicole Fally, SIL OFL 1.1); Euphoria Script (Sabrina Lopez, SIL OFL 1.1); '
             'Reenie Beanie (James Grieshaber, SIL OFL 1.1); Shadows Into Light Two (Kimberly Geswein, SIL OFL 1.1)')

INK = (30, 26, 24)             # printer's black on newsprint
IRON = (52, 36, 28)            # iron-gall ink, browned
GRAPHITE = (64, 62, 62)
WAX_BLACK = (24, 21, 22)
WAX_RED = (150, 34, 26)
NEWSPRINT = (198, 188, 160)
CARD = (220, 208, 182)
LEDGER = (214, 202, 172)
SCHOOL = (222, 214, 192)
CARTRIDGE = (226, 220, 202)

PLATES = {}


def plate(name, side):
    def deco(fn):
        PLATES[name] = (side, fn)
        return fn
    return deco


def para(sheet, text, path, size, x, y, width, leading, colour=INK, opacity=.92, seed=1, justify=False):
    lines = sheet.wrap(text, path, size, width)
    m = np.zeros((sheet.h, sheet.w), np.float32)
    f = paper.font(path, size)
    from PIL import Image, ImageDraw
    img = Image.new('L', (sheet.w, sheet.h), 0)
    d = ImageDraw.Draw(img)
    for i, line in enumerate(lines):
        yy = y + i * leading
        words = line.split(' ')
        if justify and i < len(lines) - 1 and len(words) > 1:
            gap = (width - sum(f.getlength(w) for w in words)) / (len(words) - 1)
            xx = x
            for w in words:
                d.text((xx, yy), w, font=f, fill=255, anchor='ls')
                xx += f.getlength(w) + gap
        else:
            d.text((x, yy), line, font=f, fill=255, anchor='ls')
    m = np.asarray(img, np.float32) / 255.0
    sheet.deposit(m, colour, opacity, 'print', seed)
    return y + len(lines) * leading


# ----------------------------------------------------------------- the Echo ---
def echo(seed, headline, standfirst, body_cols, date, lower=None, w=1720, h=2360, issue='No. 4,812'):
    """A page of the East London Echo, January 1974, as a cutting: scissors along the top and right."""
    s = Sheet(w, h, seed, NEWSPRINT, tooth=1.4)
    s.shape([('cut', 5), ('cut', 6), ('torn', 18), ('cut', 4)], corner=3)
    s.age(1.0, edge=70, colour=(160, 125, 80))
    L, R = 92, w - 70
    y = 150
    s.print_text('The East London Echo', CANON, 120, (L + R) / 2, y, align='center', seed=seed + 1)
    y += 30
    s.rules(y, 9, 2, L, R, INK, .9, 2.2, seed + 2)
    y += 50
    s.print_text(issue, FELL_SC, 31, L, y, seed=seed + 3)
    s.print_text(date, FELL_SC, 31, (L + R) / 2, y, align='center', seed=seed + 3)
    s.print_text('3p', FELL_SC, 31, R, y, align='right', seed=seed + 3)
    y += 20
    s.rules(y, 1, 1, L, R, INK, .85, 1.6, seed + 4)
    y += 170
    for line in headline:
        s.print_text(line, NEWS_BOLD, 142, L, y, seed=seed + 5)
        y += 150
    y += 4
    s.print_text(standfirst, FELL_I, 58, L, y, seed=seed + 6)
    y += 40
    s.rules(y, 1, 1, L, R, INK, .7, 1.4, seed + 7)
    y += 60
    cols = len(body_cols)
    gut = 40
    cw = (R - L - gut * (cols - 1)) / cols
    for c, text in enumerate(body_cols):
        x = L + c * (cw + gut)
        para(s, text, FELL, 31, x, y, cw, 38, seed=seed + 10 + c, justify=True)
        if c:
            s.vrule(x - gut / 2, y - 30, h - 40 if lower is None else lower[0] - 60, INK, .55, 1.4, seed + 20 + c)
    if lower:
        y2, head, text = lower
        s.rules(y2 - 70, 1, 1, L, R, INK, .7, 1.4, seed + 30)
        s.print_text(head, NEWS_BOLD, 62, L, y2, seed=seed + 31)
        para(s, text, FELL, 31, L, y2 + 62, R - L, 38, seed=seed + 32, justify=True)
    return s


def inside(poly):
    """A point-in-polygon test for keep() callbacks."""
    P = np.asarray(poly, float)
    xs, ys = P[:, 0], P[:, 1]
    xj, yj = np.roll(xs, 1), np.roll(ys, 1)

    def test(x, y):
        c = ((ys > y) != (yj > y)) & (x < (xj - xs) * (y - ys) / np.where(yj - ys == 0, 1e-9, yj - ys) + xs)
        return bool(np.count_nonzero(c) % 2)
    return test


def mim_drawing(s, F, face, seed, body=None, arms=(), careful=False, colour=WAX_BLACK, pressure=1.0, width=17,
                teeth=13, hands=True, fill_body=True, fur_len=95):
    """Mim as a child draws it on sheet s.

    F: centre of the pale face, face: (rx, ry). body: hand-placed outline points of the black shaggy
    mass behind and below the face (closed), coloured in hard. arms: (root, [joints...], hand angle).
    The eyes are two great black ovals pressed in until one speck of paper shines; the grin runs
    the width of the face, a black slit with teeth top and bottom."""
    F = np.asarray(F, float)
    frx, fry = face
    amp = .45 if careful else 1.0
    in_face = lambda x, y: ((x - F[0]) / (frx + 16)) ** 2 + ((y - F[1]) / (fry + 16)) ** 2 < 1
    outline = None
    if body is not None:
        outline = drawing.line(list(body) + [body[0]], seed + 1, 7 * amp, 110, n_per=10)
        inb = inside(outline)
        keep = lambda x, y: inb(x, y) and not in_face(x, y)
        B = np.mean(np.asarray(body, float), 0)
        ext = np.asarray(body, float)
        rx, ry = (ext[:, 0].max() - ext[:, 0].min()) * .6, (ext[:, 1].max() - ext[:, 1].min()) * .6
        if fill_body:
            for k, (ang, pitch, pr) in enumerate(((.35, 15, 1.0), (-.95, 24, .8), (1.3, 40, .7))):
                fill = drawing.scribble_fill(B, rx, ry, ang, pitch, seed + 2 + k, keep=keep, amp=.1, overshoot=1.0)
                s.crayon(fill, colour, width * 1.35, pressure * pr, seed + 3 + k, tip=4)
        edge = drawing.split_keep(paper.resample(outline, 3.0), lambda x, y: not in_face(x, y))
        s.crayon(edge, colour, width * 1.1, pressure, seed + 6, tip=4)
        hair = []
        for seg in edge:
            hair += drawing.fur(seg, fur_len, 26, seed + 7 + len(hair), lean=.8)
        hair = [h for h in hair if not in_face(*h[-1])]
        s.crayon(hair, colour, width * .85, pressure * .95, seed + 8, tip=4)
    # the face: gone round twice, the second time a little off
    a1 = drawing.circle(F, frx, seed + 9, 1.07, start=-2.2, wobble=.03 * amp, squash=fry / frx)
    a2 = drawing.circle(F + (5, -4), frx * 1.02, seed + 10, .55, start=.6, wobble=.04 * amp, squash=fry / frx)
    s.crayon([a1, a2], colour, width, pressure, seed + 11, tip=4)
    # two great eyes
    for i, (ex, ey, erx, ery) in enumerate(((-.42, -.17, .27, .33), (.41, -.27, .34, .42))):
        E = F + np.array([ex * frx, ey * fry])
        ex_, ey_ = erx * frx, ery * fry
        hl = E + np.array([-.38 * ex_, -.45 * ey_])
        keep = lambda x, y, hl=hl: math.hypot(x - hl[0], y - hl[1]) > frx * .12
        ring = drawing.circle(E, ex_, seed + 20 + i, 1.1, wobble=.05 * amp, squash=ey_ / ex_, n=40)
        s.crayon([ring], colour, width * .9, pressure, seed + 22 + i, tip=4)
        for k, ang in enumerate((1.0 + i * .3, -.5, .3)):
            pup = drawing.scribble_fill(E, ex_ * .93, ey_ * .93, ang, 8, seed + 24 + i * 3 + k, keep=keep, overshoot=.97, amp=.05)
            s.crayon(pup, colour, width * .9, pressure, seed + 26 + i * 3 + k, tip=4)
    # the grin: a black slit the width of the face, teeth along both lips
    l = F + np.array([-.98 * frx, .22 * fry])
    rr = F + np.array([.98 * frx, .16 * fry])
    up = drawing.line([l, F + (-.4 * frx, .34 * fry), F + (.4 * frx, .33 * fry), rr], seed + 30, 1.6 * amp, 60)
    lo = drawing.line([l, F + (-.42 * frx, .74 * fry), F + (.42 * frx, .75 * fry), rr], seed + 31, 2.0 * amp, 60)
    U, Lo = paper.resample(up, 3.0), paper.resample(lo, 3.0)
    n = min(len(U), len(Lo))
    U, Lo = U[np.linspace(0, len(U) - 1, n).astype(int)], Lo[np.linspace(0, len(Lo) - 1, n).astype(int)]
    idx = np.linspace(0, n - 1, teeth * 2 + 1).astype(int)
    top = np.array([U[i] + (Lo[i] - U[i]) * (.42 if k % 2 else 0) for k, i in enumerate(idx)])
    idx2 = np.linspace(0, n - 1, teeth * 2 + 2).astype(int)[1:]
    bot = np.array([Lo[i] + (U[i] - Lo[i]) * (.40 if k % 2 == 0 else 0) for k, i in enumerate(idx2)])
    mouth = inside(np.vstack([top, bot[::-1]]))
    for k, ang in enumerate((.04, .6)):
        s.crayon(drawing.scribble_fill(F + (0, .52 * fry), 1.05 * frx, .3 * fry, ang, 7, seed + 35 + k, keep=mouth, overshoot=1.0, amp=.04),
                 colour, width * .8, pressure, seed + 36 + k, tip=4)
    s.crayon([up, lo], colour, width * .9, pressure, seed + 32, tip=4)
    s.crayon([drawing.line(list(top), seed + 33, .5, 30, n_per=3), drawing.line(list(bot), seed + 34, .5, 30, n_per=3)],
             colour, width * .65, pressure, seed + 38, tip=3)
    # arms: long pulls out toward the edge of the page, sharp elbows, hands of too many fingers
    for k, (root, joints, hand) in enumerate(arms):
        P = drawing.line([np.asarray(root, float)] + [np.asarray(j, float) for j in joints], seed + 40 + k, 2.0 * amp, 160, n_per=4)
        s.crayon([P], colour, width * 1.6, pressure, seed + 60 + k, tip=4)
        if hands:
            _, fs = drawing.arm(P[-1], [P[-1] + (0, 0.01)], hand, 6 if k % 3 == 0 else 5, 150 if not careful else 95, seed + 80 + k, spread=1.6)
            s.crayon(fs, colour, width * .9, pressure, seed + 100 + k, tip=3)
    return outline


@plate('Intro_03', 'L')
def intro_03():
    t = Table('L', 303, light=(900, -120), reach=860)
    s = Sheet(1900, 1540, 3031, SCHOOL, tooth=1.6)
    s.shape([('torn', 22), ('cut', 4), ('cut', 4), ('cut', 4)], corner=4)
    s.age(.7, edge=50)
    s.fox([(1720, 1360, 40, .22), (160, 1420, 26, .18)])
    F = (800, 430)
    body = [(560, 300), (610, 170), (800, 120), (990, 170), (1050, 300), (1100, 560), (1190, 880), (1250, 1180),
            (1230, 1420), (1000, 1480), (640, 1490), (380, 1440), (350, 1180), (420, 880), (500, 560)]
    arms = [
        ((520, 520), [(330, 470), (210, 300), (90, 250)], math.radians(215)),
        ((470, 760), [(260, 760), (170, 610), (60, 600)], math.radians(190)),
        ((430, 990), [(250, 1060), (160, 1200), (70, 1230)], math.radians(170)),
        ((400, 1240), [(260, 1330), (230, 1450), (120, 1500)], math.radians(150)),
        ((590, 240), [(470, 130), (420, 40)], math.radians(250)),
        ((1080, 470), [(1290, 420), (1400, 270), (1560, 230)], math.radians(-30)),
        ((1010, 230), [(1130, 120), (1260, 60)], math.radians(-60)),
        ((1150, 760), [(1380, 700), (1470, 560), (1620, 520)], math.radians(-20)),
        ((1190, 980), [(1380, 1010), (1500, 880), (1620, 800)], math.radians(-40)),
        ((1230, 1200), [(1380, 1290), (1450, 1420), (1580, 1470)], math.radians(20)),
    ]
    mim_drawing(s, F, (215, 250), 3100, body=body, arms=arms)
    # Maisie, in red, small beside it, standing on the line
    girl, gface, dc, (drx, dry) = drawing.stick_girl((1660, 880), 560, 3170)
    s.crayon(drawing.scribble_fill(dc, drx, dry, 1.3, 13, 3171), WAX_RED, 18, 1.0, 3172, tip=4)
    s.crayon(girl, WAX_RED, 15, 1.0, 3173, tip=4)
    s.crayon(gface, WAX_RED, 8, .9, 3174, tip=2)
    s.crayon([drawing.line([(1400, 1430), (1600, 1424), (1860, 1432)], 3175, 3, 120)], WAX_BLACK, 12, .7, 3176, tip=3)
    m = s.hand_mask(['mim'], MAISIE, 170, 120, 220, 0, 3180, rot=5, bob=.1)
    s.deposit(m, GRAPHITE, .95, 'pencil', 3181, .9)
    t.place(s, 1395, 505, angle=-2.6, lift=.5)
    return t, "Maisie's crayon drawing of Mim with ten arms and a small girl in red beside it, on torn school paper."


@plate('Intro_02', 'R')
def intro_02():
    t = Table('R', 202, light=(1010, -60), reach=760)
    body = [
        "The girl missing from her home in Bethnal Green since Sunday night was returned to her own doorstep at "
        "first light yesterday. Her mother found her sitting on the step in clean clothes, her hair washed and "
        "plaited, and so polite, she said, that she hardly knew her. The child has no memory of where she has "
        "been, or of how she came home. Detectives from H Division examined the house and the yard and say "
        "there is no evidence of harm. No further action will be taken. The family doctor described her as "
        "well nourished and calm.",
        "It is the fourth such return this winter. Each child was missing for a night or two; each came home "
        "fed and quiet, and none could say where they had been. Older readers may recall the same story in "
        "these streets in 1958, and before the war. \"They come back so good,\" said a neighbour, who asked "
        "not to be named. \"You would think that would be a blessing. They don't laugh any more. They sit "
        "where you put them.\" Another mother told the Echo her son now draws the same picture every day.",
        "A spokesman for the Metropolitan Police said that parents should keep children indoors after dark "
        "while the power cuts continue, and that there is no cause for alarm. The children will not be "
        "questioned further. Their families have asked to be left alone, and the Echo respects that wish. "
        "Supplies in the East End will be switched off again from five o'clock this evening. Householders are "
        "advised to keep candles and matches to hand, and never to leave a flame unattended where a child "
        "can reach it. Continued on page four.",
    ]
    news = echo(2021, ['CHILDREN', 'RETURN HOME'], 'Police find no evidence of harm.', body,
                'Thursday, January 17, 1974', h=1660)
    news.crease((0, 980), (1720, 1000), .9)
    to = t.place(news, 500, 440, angle=2.4, lift=.55)
    # the police slip, typed, pinned to the cutting with one red stitch
    slip = Sheet(700, 380, 2077, CARD, tooth=1.0)
    slip.shape([('cut', 2), ('cut', 2), ('cut', 2), ('cut', 2)], corner=4)
    slip.age(.8, edge=40)
    slip.print_text('METROPOLITAN POLICE   .   H DIVISION', TYPED, 26, 130, 66, colour=(40, 38, 46), seed=7)
    slip.rules(84, 1, 1, 46, 654, (60, 60, 70), .5, 1.3, 8)
    slip.print_text('RETURNED.', TYPED, 64, 46, 178, colour=(36, 34, 42), seed=9)
    slip.print_text('No further action.', TYPED, 40, 46, 250, colour=(36, 34, 42), seed=10)
    sig = wander(spline([(470, 318), (500, 296), (520, 322), (548, 300), (566, 318), (620, 304)]), 3, 1.2)
    slip.pencil([sig], 2.6, .7, 11)
    slip.print_text('P.C. 412 H', TYPED, 22, 46, 330, colour=(50, 48, 56), opacity=.8, seed=12)
    on = t.place(slip, 830, 820, angle=-6.0, lift=.35)
    # one stitch through the slip's corner and the cutting under it, and its two loose ends
    a, b, c, d = on(30, 28), on(84, 82), on(84, 28), on(30, 82)
    t.thread([spline([a, (a + b) / 2 + (1, -2), b], 6), spline([c, (c + d) / 2 + (-1, -2), d], 6)], 3.0)
    t.thread([spline([d, d + (-10, 18), d + (-26, 40), d + (-24, 70), d + (-34, 96)], 10)], 2.6)
    return t, 'A cutting from a 1974 local paper with a typed police slip stitched to it by a red thread.'


# ---------------------------------------------------------- the register ---
def ledger(seed, w=1700, h=2300, top=330, pitch=92, cols=(150, 900, 1110, 1330, 1640), gutter='left'):
    """A page of Pryce's private register: printed ruling, red column rules, the binding's shadow."""
    s = Sheet(w, h, seed, LEDGER, tooth=1.2)
    s.shape([('cut', 3), ('cut', 3), ('cut', 3), ('cut', 3)], corner=5)
    s.age(1.1, edge=90, colour=(150, 115, 72))
    s.fox([(w * .82, h * .2, 34, .3), (w * .12, h * .78, 46, .25), (w * .6, h * .9, 22, .2)])
    s.rules(top, pitch, 40, cols[0] - 40, cols[-1] + 20, (110, 128, 150), .5, 1.8, seed + 1)
    for x in cols[:-1]:
        s.vrule(x - 14, top - pitch * .9, h - 40, (150, 50, 42), .42, 1.6, seed + 2)
    s.vrule(cols[0] - 22, top - pitch * .9, h - 40, (150, 50, 42), .42, 1.6, seed + 3)
    for name, x0, x1 in zip(('Name', 'Aged', 'Year', 'Entered'), cols[:-1], cols[1:]):
        s.print_text(name, FELL_I, 40, (x0 + x1) / 2 - 6, top - 22, align='center', colour=(60, 52, 48), opacity=.75, seed=seed + 4)
    # the page curves down into the binding
    xs = np.arange(w, dtype=np.float32)
    g = np.clip((xs - (w - 200)) / 200, 0, 1) if gutter == 'right' else np.clip((200 - xs) / 200, 0, 1)
    s.rgb *= (1 - .5 * g ** 1.6)[None, :, None]
    s.height += (g ** 2 * 12)[None, :]
    return s, top, pitch, cols


def entry(s, row, cells, top, pitch, cols, seed, ink=IRON, size=66, opacity=.92, strike=False, slow=False):
    """One line in the careful hand: the same copperplate in every decade."""
    y = top + row * pitch - 18
    for i, text in enumerate(cells):
        if not text:
            continue
        x = cols[i] + (14 if i == 0 else 22)
        m = s.hand_mask([text], COPPER, size * (1.12 if slow else 1), x, y, 0, seed + i * 11, rot=.8 if not slow else 1.6, bob=.015, grow=.02)
        if slow:
            # written slowly: the nib pressed harder and the line swells, edges still crisp
            g = m.copy()
            for dx, dy in ((2, 0), (0, 2), (2, 2), (-1, 1), (1, -1)):
                g = np.maximum(g, np.roll(np.roll(m, dy, 0), dx, 1))
            m = g
        s.deposit(m, ink, opacity, 'ink', seed + i)
    if strike:
        x0, x1 = strike if isinstance(strike, tuple) else (cols[0] + 6, cols[-1] - 30)
        P = drawing.line([(x0, y - size * .22), ((x0 + x1) / 2, y - size * .24), (x1, y - size * .2)], seed + 77, 1.2, 300)
        m = s.strokes_mask([P], 4.0, None, 1, seed)
        s.deposit(m, ink, opacity, 'ink', seed + 78)


def faded(year):
    """Iron-gall ink browns as it ages: the oldest entries are the palest."""
    k = np.clip((1974 - year) / 108.0, 0, 1)
    return tuple(int(round(a + (b - a) * k)) for a, b in zip((34, 28, 26), (120, 88, 60)))


@plate('Intro_04', 'R')
def intro_04():
    t = Table('R', 404, light=(1020, -80), reach=820)
    s, top, pitch, cols = ledger(4041)
    m = s.hand_mask(['Pryce.  Private Register.'], COPPER, 96, 170, 190, 0, 4050, rot=.8, bob=.015, grow=.02)
    s.deposit(m, faded(1866), .9, 'ink', 4051)
    rows = [
        (('Thomas Pryce', 'seven', '1866', ''), 1866, True),
        (('R. P.', '', '1866', 'Unsuitable'), 1866, False),
        (('Alfred Morgan', 'seven', '1894', 'Finished'), 1894, False),
        (('Elsie Jenkins', 'eight', '1894', 'Returned'), 1894, False),
        (('William Rees', 'seven', '1894', 'Finished'), 1894, False),
        (('Doris Hale', 'nine', '1932', 'Finished'), 1932, False),
        (('Arthur Webb', 'six', '1932', 'Finished'), 1932, False),
        (('Peter Lamb', 'eight', '1958', 'Finished'), 1958, False),
        (('Susan Clarke', 'seven', '1973', 'Finished'), 1973, False),
    ]
    for i, (cells, year, strike) in enumerate(rows):
        entry(s, i + 1, cells, top, pitch, cols, 4060 + i * 17, ink=faded(year), strike=strike)
    t.place(s, 470, 570, angle=1.4, lift=.4)
    return t, "A page of Pryce's private register: entries from 1866 to 1973 in one careful copperplate hand, the first crossed out."


@plate('EndB_03', 'R')
def endb_03():
    t = Table('R', 803, light=(1040, -60), reach=820)
    cols = (110, 590, 800, 1020, 1480)
    s, top, pitch, cols = ledger(8031, w=1520, h=2160, top=250, pitch=118, cols=cols)
    entry(s, 1, ('Susan Clarke', 'seven', '1973', 'Finished'), top, pitch, cols, 8050, ink=faded(1973), size=72)
    m = s.hand_mask(['Winter, 1974.'], COPPER, 88, cols[0] + 14, top + 2 * pitch - 18, 0, 8040, rot=.8, bob=.015, grow=.02)
    s.deposit(m, faded(1974), .92, 'ink', 8041)
    entry(s, 3, ('Maisie Evans', 'ten', '1974', 'Not finished.'), top, pitch, cols, 8070, ink=faded(1974), size=72,
          strike=(cols[0] + 6, cols[4] - 20))
    entry(s, 4, ('', '', '', 'Gone.'), top, pitch, cols, 8080, ink=faded(1974), size=72, strike=(cols[3] + 12, cols[3] + 210))
    entry(s, 6, ('Maisie Evans', 'ten', '1974', 'Finished.'), top, pitch, cols, 8090, ink=(26, 22, 22), size=72, slow=True)
    entry(s, 8, ('Eleanor Price', 'ten', '1974', ''), top, pitch, cols, 8095, ink=faded(1974), size=72, opacity=.8)
    t.place(s, 500, 600, angle=-1.6, lift=.4)
    return t, "The register's last entries for winter 1974: Maisie Evans, not finished, struck through; then, written slowly, finished."


# ------------------------------------------------------------ Maisie's book ---
def lined(seed, w, h, tone=SCHOOL, top=230, pitch=88, margin=170, torn=None, red=True):
    s = Sheet(w, h, seed, tone, tooth=1.0)
    s.shape(torn or [('cut', 3), ('cut', 3), ('cut', 3), ('cut', 3)], corner=4)
    s.age(.8, edge=60)
    s.rules(top, pitch, 40, 0, w, (118, 140, 168), .5, 1.6, seed + 1)
    if red:
        s.vrule(margin, 0, h, (170, 52, 46), .5, 1.8, seed + 2)
    return s


def maisie(s, lines, x, y, pitch, seed, size=92, pressure=.8, slope=0.0):
    m = s.hand_mask(lines, MAISIE, size, x, y, pitch, seed, rot=3.5, bob=.07, grow=.07, drift=3, slope=slope)
    s.deposit(m, GRAPHITE, .97, 'pencil', seed + 1, pressure)


@plate('Intro_06', 'R')
def intro_06():
    t = Table('R', 606, light=(1060, -40), reach=820)
    W2, H2, pw = 2240, 1560, 1060
    cover = Sheet(W2, H2, 6061, (104, 100, 86), tooth=1.4)
    cover.shape([('cut', 3), ('cut', 3), ('cut', 3), ('cut', 3)], corner=12)
    cover.age(1.2, edge=50, colour=(120, 100, 70))
    t.place(cover, 520, 520, angle=-2.0, lift=.35)
    for k, (ox, gut) in enumerate(((0, 'right'), (1, 'left'))):
        pg = lined(6070 + k, pw, H2 - 60, top=210, pitch=86, margin=150 if k == 0 else 120)
        xs = np.arange(pw, dtype=np.float32)
        g = np.clip((xs - (pw - 170)) / 170, 0, 1) if gut == 'right' else np.clip((170 - xs) / 170, 0, 1)
        pg.rgb *= (1 - .45 * g ** 1.5)[None, :, None]
        pg.height += (g ** 2 * 10)[None, :]
        if k == 0:
            maisie(pg, ['MAISIE EVANS.', 'HER BOOK.', '', 'KEEP OUT.', '(that means you Nell)', '(not really)', '', 'Winter, 1974.'],
                   190, 284, 86 * 1.0, 6080, size=106, pressure=.95)
            pg.fox([(880, 1300, 30, .2)])
        else:
            maisie(pg, ['Nell believes me.', '', 'Nobody else does.'], 160, 370, 172, 6090, size=118, pressure=1.0)
        dx = (-pw / 4 - 8) if k == 0 else (pw / 4 + 8)
        a = math.radians(-2.0)
        cx, cy = 520 + dx * math.cos(a), 520 - dx * math.sin(a)
        t.place(pg, cx, cy, angle=-2.0 + (0.6 if k == 0 else -0.6), lift=.12)
    return t, "Maisie's exercise book open at its first pages, in her pencil hand."


@plate('Intro_07', 'L')
def intro_07():
    t = Table('L', 707, light=(880, -100), reach=820)
    s = lined(7071, 1220, 1640, top=200, pitch=86, margin=150, torn=[('cut', 3), ('cut', 3), ('cut', 3), ('torn', 22)])
    s.crease((0, 820), (1220, 830), .8)
    maisie(s, ['The first time I ran', 'because I was frightened.', '', 'The second time I knew', 'what was there.', '',
               'I should have run again.'], 180, 286, 86, 7080, size=96, pressure=.8)
    maisie(s, ["But I didn't want to", 'anymore.'], 180, 286 + 86 * 8, 86, 7090, size=100, pressure=1.0)
    t.place(s, 1390, 470, angle=3.2, lift=.45)
    return t, "Maisie's last page, torn from her book, in her pencil hand."


@plate('EndA_02', 'R')
def enda_02():
    t = Table('R', 902, light=(1000, -60), reach=780)
    body = [
        "Eleanor Price, who was missing from her home for a night, was found shortly before dawn yesterday on "
        "the steps of an empty house off Columbia Road. She was exhausted but unharmed. Her clothes, her mother "
        "said, were clean and pressed. She was unable to account for her absence, and was taken home by a "
        "constable from H Division, who described her as calm and very well behaved.",
        "Her family has asked that she be allowed to rest. She will not be questioned further. A neighbour said "
        "Eleanor had been out looking for a friend, a girl of her age from the same street, who has not been "
        "seen since the autumn. Police say there is no connection, and no cause for alarm. Eleanor has asked "
        "for paper and crayons, her mother said. She draws all day.",
    ]
    news = echo(9021, ['ANOTHER CHILD', 'FOUND SAFE'], 'Eleanor Price is home.', body, 'Saturday, January 26, 1974',
                w=1400, h=1700, issue='No. 4,820')
    news.crease((700, 0), (690, 1700), .9)
    t.place(news, 470, 460, angle=-2.8, lift=.5)
    return t, 'A cutting from the same local paper a week later: another child found safe.'


# ------------------------------------------------------------- Nell's drawing ---
def wax_stick(length, radius, seed, wax=(30, 27, 28), wrapper=(88, 84, 74)):
    """A wax crayon lying on its side, worn to a blunt slant, its paper wrapper torn back."""
    L, R = int(length), int(radius)
    h, w = R * 2 + 8, L + 8
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = (yy - h / 2) / R                          # -1..1 across the barrel
    tipx = 4 + L * .14
    x0 = 4.0
    # the slant at the tip end
    edge_x = x0 + (tipx - x0) * (1 - (v + 1) / 2 * .8)
    a = ((np.abs(v) <= 1) & (xx >= edge_x) & (xx <= w - 4)).astype(np.float32)
    a = look.blur(a, 1)
    nz = np.sqrt(np.clip(1 - v ** 2, 0, 1))
    lit = np.clip(.25 + .75 * (nz * .6 - v * .5), 0, 1.2)
    col = np.array(wax, np.float32) / 255.0
    rgb = col[None, None, :] * (.6 + .8 * lit[..., None])
    spec = np.exp(-((v + .45) / .12) ** 2)[..., None] * .22
    rgb = rgb + spec
    wrap0, wrap1 = x0 + L * .32, w - 10
    tear = wrap0 + 10 * np.sin(v * 5 + seed)
    on_wrap = (xx > tear) & (xx < wrap1)
    wcol = np.array(wrapper, np.float32) / 255.0
    band = ((xx > wrap0 + L * .2) & (xx < wrap0 + L * .32)).astype(np.float32)[..., None]
    wrgb = wcol[None, None, :] * (.55 + .7 * lit[..., None]) * (1 - .55 * band)
    rgb = np.where(on_wrap[..., None], wrgb, rgb)
    return np.dstack([np.clip(rgb, 0, 1), a])


@plate('EndA_03', 'L')
def enda_03():
    t = Table('L', 913, light=(880, -100), reach=840)
    s = Sheet(1800, 1420, 9131, CARTRIDGE, tooth=1.1)
    s.shape([('cut', 3), ('cut', 3), ('cut', 3), ('cut', 3)], corner=4)
    s.age(.5, edge=40)
    s.fox([(1650, 1300, 30, .18)])
    # first a circle, then two eyes, then a smile, then ten arms: she has drawn seven
    F = np.array([800, 620])
    fr = (250, 270)
    rays = [200, 160, 125, 230, 270, 315, 18, 340]
    arms = []
    for k, deg in enumerate(rays):
        a = math.radians(deg)
        root = F + np.array([math.cos(a) * fr[0], math.sin(a) * fr[1]]) * 1.02
        L = 330 + 40 * ((k * 5) % 3)
        bend = math.radians(18 if k % 2 else -16)
        j1 = root + np.array([math.cos(a), math.sin(a)]) * L * .55
        j2 = j1 + np.array([math.cos(a + bend), math.sin(a + bend)]) * L * .5
        arms.append((root, [j1, j2], a + bend))
    # Nell goes over every line twice, pressing hard; the last arm stops halfway, where the crayon was put down
    for k in range(2):
        mim_drawing(s, F, fr, 9140 + k * 300, arms=arms[:7], careful=True, width=14, pressure=1.0, teeth=12)
    # she counts them in pencil as she goes
    for k, (root, joints, hand) in enumerate(arms[:7]):
        a = math.radians(rays[k])
        p = joints[0] + np.array([-math.sin(a), math.cos(a)]) * 62 + (-18, 26)
        m = s.hand_mask([str(k + 1)], NELL, 92, p[0], p[1], 0, 9170 + k, rot=4, bob=.05)
        s.deposit(m, GRAPHITE, .95, 'pencil', 9180 + k, 1.0)
    last = arms[7]
    half = drawing.line([last[0], last[0] + (last[1][0] - last[0]) * .8], 9190, 1.0, 120, n_per=4)
    s.crayon([half, half + (1.5, 1)], WAX_BLACK, 14 * 1.6, 1.0, 9191, tip=4)
    on = t.place(s, 1365, 505, angle=1.8, lift=.45)
    end = on(*half[-1])
    stick = wax_stick(560, 30, 9150, wrapper=(150, 140, 118))
    t.place(stick, end[0] + 150, end[1] + 70, angle=-28, lift=.25)
    return t, "Nell's own drawing in black crayon: a circle, two eyes, a smile, seven of ten arms, the crayon put down."


@plate('EndB_01', 'L')
def endb_01():
    t = Table('L', 801, light=(880, -100), reach=860)
    s = Sheet(1900, 1540, 8011, SCHOOL, tooth=1.6)
    s.shape([('cut', 4), ('cut', 4), ('torn', 20), ('cut', 4)], corner=4)
    s.age(.7, edge=50)
    s.fox([(170, 200, 30, .2)])
    # the house drawn round them both: four walls and a roof, each pushed as its own line
    walls = [((330, 1420), (322, 650)), ((316, 660), (955, 225)), ((940, 230), (1580, 645)), ((1572, 640), (1580, 1425)),
             ((300, 1428), (1610, 1418)), ((1250, 440), (1252, 240)), ((1245, 248), (1365, 246)), ((1360, 240), (1362, 520))]
    s.crayon([drawing.line([a, ((a[0] + b[0]) / 2 + 3, (a[1] + b[1]) / 2 - 2), b], 8020 + i, 3, 220, n_per=6) for i, (a, b) in enumerate(walls)],
             WAX_BLACK, 20, 1.0, 8022, tip=4)
    F = (760, 760)
    body = [(620, 690), (650, 590), (760, 560), (870, 590), (900, 690), (950, 880), (1000, 1100), (1010, 1330),
            (900, 1400), (760, 1405), (610, 1400), (500, 1330), (520, 1100), (570, 880)]
    arms = [
        ((620, 800), [(440, 760), (330, 640), (150, 610)], math.radians(200)),
        ((580, 960), [(400, 1000), (300, 1080), (120, 1100)], math.radians(170)),
        ((540, 1150), [(400, 1220), (250, 1300), (90, 1380)], math.radians(160)),
        ((660, 620), [(640, 450), (700, 300), (680, 120)], math.radians(260)),
        ((860, 610), [(940, 420), (1050, 250), (1120, 80)], math.radians(-80)),
        ((910, 760), [(1100, 660), (1300, 560), (1500, 420), (1700, 360)], math.radians(-20)),
        ((1000, 1300), [(1150, 1380), (1300, 1470), (1420, 1530)], math.radians(60)),
        ((520, 1300), [(400, 1400), (300, 1500)], math.radians(120)),
        ((640, 1390), [(620, 1480), (560, 1540)], math.radians(110)),
    ]
    mim_drawing(s, F, (130, 150), 8030, body=body, arms=arms, width=15, fur_len=70)
    # the tenth arm holds Maisie's hand
    hold = drawing.line([(960, 940), (1080, 960), (1160, 1010)], 8040, 2, 120, n_per=6)
    s.crayon([hold], WAX_BLACK, 24, 1.0, 8041, tip=4)
    girl, gface, dc, (drx, dry) = drawing.stick_girl((1240, 880), 470, 8050)
    s.crayon(drawing.scribble_fill(dc, drx, dry, 1.3, 13, 8051), WAX_RED, 18, 1.0, 8052, tip=4)
    s.crayon(girl, WAX_RED, 15, 1.0, 8053, tip=4)
    s.crayon(gface, WAX_RED, 8, .9, 8054, tip=2)
    for word, x, y, sd in (('MIM', 690, 478, 8060), ('ME', 1310, 830, 8061)):
        m = s.hand_mask([word], MAISIE, 120 if word == 'MIM' else 150, x, y, 0, sd, rot=6, bob=.1, grow=.1, track=10)
        m = np.clip(look.blur(m, 2) * 1.8, 0, 1)
        s.deposit(m, WAX_BLACK, 1.0, 'crayon', sd + 1, 1.0)
    t.place(s, 1395, 505, angle=-1.8, lift=.5)
    return t, "Maisie's crayon drawing of Mim holding her hand inside the house, labelled MIM and ME, Maisie in red."


# ---------------------------------------------------------- the empty label ---
@plate('EndB_04', 'R')
def endb_04():
    t = Table('R', 804, light=(1020, -20), reach=820)
    s = Sheet(1900, 940, 8041, CARD, tooth=1.0)
    s.shape([('cut', 2), ('cut', 2), ('cut', 2), ('cut', 2)], corner=6)
    # a tag: the hole end clipped at both corners
    yy, xx = np.mgrid[0:s.h, 0:s.w].astype(np.float32)
    clip = (xx > 200 - np.minimum(yy, s.h - yy) * 1.0).astype(np.float32)
    s.alpha *= look.blur(clip, 1)
    s.age(1.1, edge=60, colour=(150, 118, 76))
    hole = np.array([145, 470])
    ring = np.exp(-((np.hypot(xx - hole[0], yy - hole[1]) - 52) / 11) ** 2)
    s.rgb *= (1 - .2 * ring[..., None])
    s.alpha *= np.clip((np.hypot(xx - hole[0], yy - hole[1]) - 26) / 2, 0, 1)
    m = s.hand_mask(['Eleanor Price'], COPPER, 220, 330, 430, 0, 8042, rot=.8, bob=.015, grow=.02)
    s.deposit(m, faded(1974), .94, 'ink', 8043)
    m = s.hand_mask(['place prepared'], COPPER, 150, 440, 680, 0, 8044, rot=.8, bob=.015, grow=.02)
    s.deposit(m, faded(1974), .9, 'ink', 8045)
    for (x, y, rx, ry, sd, op) in ((1560, 220, 150, 90, 8046, .75), (1700, 780, 90, 60, 8047, .7), (560, 830, 110, 50, 8048, .6)):
        s.stain(x, y, rx, ry, sd, tint=(.8, .76, .68), gloss=.06, opacity=op)
    on = t.place(s, 610, 560, angle=-6.0, lift=.3)
    h = on(*hole)
    # the red thread through the hole, tied to nothing, trailing off into the dark
    t.thread([spline([h + (-3, -16), h + (-34, -46), h + (-96, -40), h + (-170, 6), h + (-240, 60), h + (-330, 46)], 16),
              spline([h + (-3, 16), h + (-30, 52), h + (-80, 84), h + (-130, 150), h + (-160, 240), h + (-230, 300)], 16)], 5.2,
             colour=np.minimum(1, look.THREAD * 1.35))
    return t, 'A wax-stained card label for an empty candle mould, tied by a red thread to nothing.'


# ------------------------------------------------------------- the letter ---
@plate('Confess_01', 'L')
def confess_01():
    t = Table('L', 1101, light=(880, -100), reach=840)
    s = Sheet(1480, 1900, 11011, (218, 208, 184), tooth=.9)
    s.shape([('cut', 3), ('cut', 3), ('cut', 3), ('cut', 3)], corner=5)
    s.age(1.0, edge=70, colour=(150, 116, 74))
    # laid paper: the wire lines of the mould, faint against the light
    yy = np.arange(s.h, dtype=np.float32)
    xx = np.arange(s.w, dtype=np.float32)
    s.rgb *= (1 - .018 * (np.sin(yy * 2.4) > .6))[:, None, None]
    s.rgb *= (1 - .03 * (np.abs(((xx + 30) % 92) - 46) < 1.6))[None, :, None]
    s.crease((0, 633), (1480, 640), 1.0)
    s.crease((0, 1266), (1480, 1270), 1.0)
    s.fox([(1300, 300, 26, .25), (200, 1720, 40, .2)])
    m = s.hand_mask(['Bethnal Green, 1894.'], ADULT, 70, 860, 200, 0, 11020, rot=1.5, bob=.03, grow=.03)
    s.deposit(m, faded(1894), .85, 'ink', 11021)
    lines = ['Dear Elsie,', '', 'I thought I was keeping you safe.', 'I took your hand and walked you',
             'to the house myself.', 'You asked if I was coming inside.', 'I told you to be good.', '',
             'I was there. And I did nothing.', '', '            Your father.']
    m = s.hand_mask(lines, ADULT, 94, 150, 390, 128, 11030, rot=1.4, bob=.025, grow=.03, drift=4, slope=-6)
    s.deposit(m, faded(1894), .92, 'ink', 11031)
    # a blot where the pen stopped
    yy2, xx2 = np.mgrid[0:s.h, 0:s.w].astype(np.float32)
    # a blot under the line where the pen was held still too long
    bx = 150 + paper.font(ADULT, 94).getlength('I was there. And I did nothing.') * .78
    by = 390 + 8 * 128 + 34
    blot = np.clip((13 - np.hypot(xx2 - bx, (yy2 - by) * 1.35)) / 2, 0, 1)
    blot = np.maximum(blot, np.clip((5 - np.hypot(xx2 - bx - 26, yy2 - by + 6)) / 2, 0, 1))
    s.deposit(blot, faded(1894), .95, 'ink', 11032)
    t.place(s, 1360, 500, angle=2.6, lift=.45)
    return t, "The constable's unsent letter to his daughter, 1894, in his own hand on folded laid notepaper."



def build(names=None):
    OUT.mkdir(exist_ok=True)
    for name, (side, fn) in PLATES.items():
        if names and name not in names:
            continue
        t, what = fn()
        rgb = t.render() if isinstance(t, Table) else t
        prov = (f'TALLOW story plate {name}. {what} Generated by Tools/FinalPass/ui/plates/plates.py '
                f'(numpy, pycairo, Pillow): procedural paper, ink and crayon, every word and stroke placed by hand '
                f'in code. No photographs, scans, traced or generated imagery. Fonts: {FONT_NOTE}.')
        look.save_png(look.to_image(rgb), OUT / f'{name}.png', prov)
        print('built', name)


if __name__ == '__main__':
    build(set(sys.argv[1:]) or None)
