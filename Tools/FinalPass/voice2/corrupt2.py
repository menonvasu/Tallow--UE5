"""Second, harsher corruption of Mim's hidden narration (C001..C081), from the same converted takes.

my playtest (29 Oct): the overdub "is not creepy glitched enough, or loud; it's quite hard to hear; it's supposed to
play over". This pass keeps the cast voice and the in-game -2 semitone drop, and does far more damage around it:

  through the speaker  band-limited, a resonant cone peak and some breakup, so it reads as a voice from a device
  twin                 the same words 3 to 7 semitones down, late, on its own wobble, with a short comb on it; once a
                       line it swells up over the sweet voice for a word, then sinks back (never a plain pitch-down)
  doubling             a slowly sweeping 6 to 14 ms copy, so the voice phases against itself
  tape                 heavy wow and flutter, warble bursts, a drag that sags the pitch mid-line, spin-up and stops
  skips                first-syllable and mid-line repeats, one to three grains, some crushed
  crush bursts         sample-rate and bit-depth crush for 70 to 240 ms at a time
  dropouts             hard cuts to silence, and on some lines a fast chop
  reversed grabs       a reversed pre-echo into the first word, a short reversed chunk in place, a reversed tail
  clarity              on most lines one word comes through dry, full-band and close, with nothing on it at all

Each line draws a dominant flavour (tape, speaker, skip, twin) from its preset, seeded from the line id, and the
intensity tier sets how much happens (the worst information still gets the least, but nothing is clean any more).
The same id always renders the same file.

Level is baked as heard: integrated loudness (BS.1770) against the narration file it plays over, less the widget's
own volume on that narration (readings .72, slides .85); both play under SC_Tallow_Master. Under lines sit 4 to 6 LU
under it, seams at it, after lines just over it, break lines 3.4 to 4 LU over it. True peak under -1 dBFS. Play at unity.

    ../../Audio/Corrupted/.venv/bin/python corrupt2.py --batch RAW_DIR OUT_DIR
    ../../Audio/Corrupted/.venv/bin/python corrupt2.py C017 IN.wav OUT.wav
"""
from __future__ import annotations
from pathlib import Path
import argparse
import json
import math
import sys
import zlib
import numpy as np
import pyloudnorm as pyln
from scipy.ndimage import minimum_filter1d
from scipy.signal import butter, lfilter, resample_poly, sosfiltfilt

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Audio/Corrupted'))
import corrupt as base  # noqa: E402  (the first pass: loading, pitch, reverb, flue, hiss)

SR = base.SR
LORE = ROOT / 'Tallow/SourceAssets/Lore/Corrupted'
NARRATION = ROOT / 'Tools/Audio/FinalPass/Mastered/VO'
CEILING = -1.2
MULT = {'read': .72, 'slide': .85}          # TallowJournalWidget / TallowSlidesWidget narration volume
# How much happens on a line, by intensity tier (1 = lightest information, 4 = the worst).
TIER = {1: 1.0, 2: .9, 3: .78, 4: .62}
# Heard level against the narration, LU, by placement (under keeps the script's two depths, both much nearer now).
# Breaks at the top of their 2 to 4 LU and after lines a touch over level: the first lines played on no sound class,
# 2.2 dB over the narration's SC_Tallow_Master, so they were heard at +5.7 and +1.4 and must not come back quieter.
REL = {'seam': 0.0, 'after': .8, 'break': 3.7}
UNDER = {-16: -6.0, -8: -4.0}

FLAVOURS = ('tape', 'speaker', 'skip', 'twin')
PRESETS = {
    #            speaker band       peak Hz       twin dB   flavour weights (tape, speaker, skip, twin)   room
    'CR_UNDER': dict(band=(320, 3000), peak=(1100, 1800), twin=-6, w=(3, 2, 1, 3), room='dark', wet=-12,
                     clarity=.35, pre=True, tail=False, start=False, stops=.5, hiss=-40),
    'CR_SEAM': dict(band=(260, 4200), peak=(1500, 2400), twin=-9, w=(2, 3, 3, 1), room=None, wet=0,
                    clarity=.55, pre=False, tail=.35, start=False, stops=.45, hiss=-44),
    'CR_BREAK': dict(band=(220, 5500), peak=(1700, 2800), twin=-8, w=(2, 2, 3, 2), room='room', wet=-17,
                     clarity=.6, pre=.45, tail=False, start=True, stops=.25, hiss=-46),
    'CR_AFTER': dict(band=(180, 6500), peak=(1800, 3000), twin=-9, w=(3, 1, 1, 4), room='close', wet=-20,
                     clarity=.75, pre=False, tail=False, start=False, stops=.35, hiss=-50),
    'CR_FLUE': dict(band=None, peak=(700, 1100), twin=-10, w=(3, 1, 2, 2), room='flue', wet=-10,
                    clarity=0, pre=False, tail=.4, start=False, stops=.3, hiss=-48, flue=True),
    'CR_MANY': dict(band=(240, 5000), peak=(1600, 2600), twin=-11, w=(1, 1, 2, 4), room=None, wet=0,
                    clarity=0, pre=False, tail=False, start=True, stops=0, hiss=-46, many=True),
}


# ---------------------------------------------------------------- small tools

def ms(t):
    return int(round(t * SR / 1000.0))


def edge(n, r):
    """A gain window of length n with r-sample raised-cosine edges."""
    g = np.ones(n)
    r = min(r, n // 2)
    if r:
        g[:r] = base.ramp(r)
        g[-r:] = base.ramp(r)[::-1]
    return g


def splice(x, a, b, seg, r=ms(4)):
    """Replace x[a:b] with seg (same length), crossfaded over r samples at both ends."""
    y = x.copy()
    g = edge(b - a, r)
    y[a:b] = x[a:b] * (1 - g) + seg * g
    return y


def lufs(x):
    if len(x) < int(.45 * SR):
        x = np.pad(x, (0, int(.45 * SR) - len(x)))
    return float(pyln.Meter(SR).integrated_loudness(x))


def onsets(x, on, off):
    """Word and syllable starts (samples) inside the speech, from the energy rise."""
    import librosa
    y = resample_poly(x, 1, 3)
    f = librosa.onset.onset_detect(y=y, sr=16000, hop_length=160, backtrack=True, units='samples', delta=.05)
    s = sorted({int(v * 3) for v in f if on <= v * 3 < off - ms(80)} | {on})
    out = []
    for v in s:
        if not out or v - out[-1] > ms(90):
            out.append(v)
    return out


def peaking(x, f0, gain_db, q):
    a = 10 ** (gain_db / 40)
    w = 2 * math.pi * f0 / SR
    al = math.sin(w) / (2 * q)
    b = [1 + al * a, -2 * math.cos(w), 1 - al * a]
    d = [1 + al / a, -2 * math.cos(w), 1 - al / a]
    return lfilter(np.array(b) / d[0], np.array(d) / d[0], x)


def frac_delay(x, d):
    """x delayed by d samples (an array, per sample)."""
    t = np.arange(len(x)) - d
    return np.interp(t, np.arange(len(x)), x, left=0.0, right=0.0)


class Spots:
    """Places on the line that already carry an effect, so two never land on the same syllable."""

    def __init__(self, n):
        self.n, self.used = n, []

    def free(self, a, b):
        return 0 <= a < b <= self.n and all(b <= u0 or a >= u1 for u0, u1 in self.used)

    def take(self, a, b):
        self.used.append((a, b))

    def pick(self, rng, lo, hi, length, tries=24):
        for _ in range(tries):
            if hi - length <= lo:
                return None
            a = int(rng.uniform(lo, hi - length))
            if self.free(a, a + length):
                self.take(a, a + length)
                return a
        return None


# ---------------------------------------------------------------- effects

def stutter(x, rng, at, grain, reps, crushed):
    """Repeat the grain starting at `at` reps times before letting the word go: a skipping disc."""
    g = x[at:at + grain] * edge(grain, ms(3))
    parts = []
    for i in range(reps):
        piece = g * rng.uniform(.75, 1.0)
        if crushed and i == reps - 1:
            piece = crush(piece, rng.integers(6, 12), rng.integers(4, 7))
        parts += [piece, np.zeros(int(rng.uniform(0, ms(22))))]
    add = np.concatenate(parts)
    return np.concatenate([x[:at], add, x[at:]]), len(add)


def crush(seg, hold, bits):
    """Sample-rate crush (sample and hold every `hold` samples) and bit crush to `bits`, on the segment's own scale."""
    held = np.repeat(seg[::hold], hold)[:len(seg)]
    peak = np.max(np.abs(held)) + 1e-9
    q = 2 ** (bits - 1)
    return np.round(held / peak * q) / q * peak


def speaker(x, rng, band, peak):
    lo, hi = band
    lo *= rng.uniform(.85, 1.25)
    hi *= rng.uniform(.8, 1.15)
    y = sosfiltfilt(butter(4, lo, 'highpass', fs=SR, output='sos'), x)
    y = sosfiltfilt(butter(4, hi, 'lowpass', fs=SR, output='sos'), y)
    f0 = rng.uniform(*peak)
    y = peaking(y, f0, rng.uniform(5, 9), rng.uniform(1.4, 2.6))
    drive = rng.uniform(1.8, 4.5)
    p = np.max(np.abs(y)) + 1e-9
    broken = np.tanh(drive * y / p) / np.tanh(drive) * p
    mix = rng.uniform(.45, .85)
    return (1 - mix) * y + mix * broken, {'band_hz': [round(lo), round(hi)], 'peak_hz': round(f0), 'drive': round(drive, 2)}


def doubling(x, rng):
    n = len(x)
    t = np.arange(n) / SR
    d0, dm, f = rng.uniform(6, 14), rng.uniform(.8, 2.2), rng.uniform(.25, 1.1)
    d = ms(d0) + ms(dm) * np.sin(2 * np.pi * f * t + rng.uniform(0, 6.3))
    return frac_delay(x, d) * 10 ** (rng.uniform(-9, -5) / 20), round(d0, 1)


def twin(x, rng, level_db, swell=None):
    st = float(rng.uniform(-7, -3))
    s = base.pitch(x, st)
    s = base.band(s, 80, 2600)
    s = base.delay(s, rng.uniform(.012, .032))
    s = base.warp(s, 1 + .006 * base.smooth_noise(rng, len(s), rng.uniform(1.5, 4)))
    comb = ms(rng.uniform(2, 6))
    s = s + .6 * np.concatenate([np.zeros(comb), s[:-comb]])
    g = np.full(len(s), 10 ** (level_db / 20))
    if swell:
        a, b, top = swell
        w = edge(b - a, ms(60))
        g[a:b] = g[a:b] * (1 - w) + 10 ** (top / 20) * w
    return s * g, round(st, 2)


def chop(x, a, length, rng):
    """A fast gate: on and off every 20 to 40 ms."""
    period = ms(rng.uniform(20, 40))
    g = np.ones(length)
    for i in range(0, length, period):
        g[i + period // 2:i + period] = 0
    g = np.convolve(g, np.ones(ms(1.5)) / ms(1.5), 'same')
    y = x.copy()
    y[a:a + length] *= g
    return y


def dropout(x, a, length):
    g = np.ones(len(x))
    r = ms(1.2)
    seg = np.zeros(length)
    seg[:r] = np.linspace(1, 0, r)
    seg[-r:] = np.linspace(0, 1, r)
    g[a:a + length] = seg[:len(g[a:a + length])]
    return x * g


def tape(x, rng, mask, wow, flutter, warbles, drags, start, stop_at, stop_len):
    """Read the mix off a bad tape. Returns the warped mix; slowdowns lengthen it so nothing is lost but the stop."""
    n = len(x)
    m = int(n * 1.35) + ms(400)
    t = np.arange(m) / SR
    msk = np.pad(mask, (0, m - n), mode='edge')
    w = wow * (.65 * np.sin(2 * np.pi * rng.uniform(.4, 1.1) * t + rng.uniform(0, 6.3)) + .35 * base.smooth_noise(rng, m, 1.2))
    f = flutter * np.sin(2 * np.pi * rng.uniform(6, 12) * t + rng.uniform(0, 6.3))
    speed = 1 + msk * (w + f)
    for a, length, depth, rate in warbles:
        tt = np.arange(length) / SR
        speed[a:a + length] += msk[a:a + length] * depth * np.sin(2 * np.pi * rate * tt) * edge(length, ms(20))
    for a, length, depth in drags:
        speed[a:a + length] *= 1 - depth * np.sin(np.pi * np.arange(length) / length) * msk[a:a + length]
    if start:
        k = ms(start)
        speed[:k] *= np.linspace(.1, 1, k) ** 1.6
    pos = np.cumsum(speed) - speed[0]
    end = int(np.searchsorted(pos, n - 1))
    if stop_at is not None:
        # The stop: from the end of the speech the reel winds down to nothing.
        a = int(np.searchsorted(pos, stop_at))
        k = ms(stop_len * 1000)
        if a + k < m:
            speed[a:a + k] *= np.linspace(1, .04, k) ** 1.4
            speed[a + k:] = 0.0
            pos = np.cumsum(speed) - speed[0]
            end = min(a + k + ms(30), m)
    pos = np.clip(pos[:end], 0, n - 1)
    return np.interp(pos, np.arange(n), x)


def compress(x, ratio=3.0, above=-8.0, att=.003, rel=.07):
    """Dense and forward: frames more than `above` dB under the line's peak are left, louder ones held down."""
    h = ms(1)
    k = len(x) // h
    db = 20 * np.log10(np.sqrt((np.pad(x, (0, (k + 1) * h - len(x)))[:(k + 1) * h].reshape(k + 1, h) ** 2).mean(1)) + 1e-9)
    thr = db.max() + above
    want = np.minimum(0, (thr - db) * (1 - 1 / ratio))
    g, out = 0.0, np.empty_like(want)
    a, r = 1 - math.exp(-h / (att * SR)), 1 - math.exp(-h / (rel * SR))
    for i, v in enumerate(want):
        g += (a if v < g else r) * (v - g)
        out[i] = g
    gain = np.interp(np.arange(len(x)), np.arange(len(out)) * h + h // 2, 10 ** (out / 20))
    return x * gain


def limit(x, ceiling_db=CEILING):
    c = 10 ** (ceiling_db / 20)
    for _ in range(8):
        up = np.abs(resample_poly(x, 4, 1))
        env = np.pad(up, (0, (-len(up)) % 4)).reshape(-1, 4).max(1)[:len(x)]
        if env.max() <= c:
            return x
        need = minimum_filter1d(np.minimum(1, c / (env + 1e-12)), ms(2) * 2 + 1)
        g = lfilter([.03], [1, -.97], need, zi=[.97])[0]
        g = lfilter([.03], [1, -.97], g[::-1], zi=[.97])[0][::-1]
        x = x * np.minimum(g, need) * .997   # look-ahead and smooth, but never letting a peak through
    return x * (c / base.true_peak(x)) if base.true_peak(x) > c else x


# ---------------------------------------------------------------- level

def narration_lufs(cue):
    name = cue['vo_asset'].rsplit('.', 1)[-1]
    heard = 20 * math.log10(MULT[cue['recording_key'].split(':')[0]])
    return lufs(base.load(NARRATION / f'{name}.wav')) + heard, name


def relative(cue, rng):
    if cue['placement'] == 'under':
        return UNDER.get(int(cue.get('gain_db', -8)), -5.0)
    return REL[cue['placement']] + float(rng.uniform(-.3, .3))


# ---------------------------------------------------------------- chain

def corrupt(cue, raw, extra_takes=()):
    lid = cue['line_id']
    rng = np.random.default_rng(zlib.crc32((lid + ':voice2').encode()))
    p = PRESETS[cue['preset']]
    k = TIER[cue['intensity']]
    seam = cue['placement'] == 'seam'
    w = np.array(p['w'], float)
    flavour = FLAVOURS[int(rng.choice(4, p=w / w.sum()))]
    boost = {f: (1.45 if f == flavour else .8) for f in FLAVOURS}
    notes = {'line': lid, 'preset': cue['preset'], 'placement': cue['placement'], 'intensity': cue['intensity'],
             'amount': k, 'flavour': flavour, 'base_shift_st': base.BASE_SHIFT}

    x = base.pitch(base.trim(raw), base.BASE_SHIFT)
    if p.get('many'):
        takes = [x] + [base.pitch(base.trim(t), base.BASE_SHIFT) for t in extra_takes]
        while len(takes) < 3:
            takes.append(base.warp(x, 1 + .008 * base.smooth_noise(rng, len(x), 2.5)))
        out = np.zeros(1)
        offs = []
        for i, t in enumerate(takes[:3]):
            t = base.pitch(t, rng.uniform(-.2, .2)) if i else t
            off = 0 if i == 0 else rng.uniform(.008, .015) * i
            t = base.delay(t, off, len(t) + int(off * SR))
            offs.append(round(off * 1000, 1))
            out = np.pad(out, (0, max(0, len(t) - len(out)))) + np.pad(t, (0, max(0, len(out) - len(t))))
        x = out / 3 ** .5
        notes['stack_offsets_ms'] = offs
        notes['stand_in_takes'] = max(0, 2 - len(extra_takes))

    on, off = base.speech_span(x)
    starts = onsets(x, on, off)

    # How much happens: a budget of events that grows with the length of the line and shrinks with the tier, drawn
    # from the flavour's own mix. Every line gets at least one hard dropout.
    dur = (off - on) / SR
    budget = max(2, int(round(k * (1.0 + 1.4 * dur) + rng.uniform(-.4, .4))))
    weights = {'tape': (1, 1, 2, 1, 0), 'speaker': (1, 3, 1.5, .5, .5), 'skip': (3, 1, 1.5, 1, 1.5),
               'twin': (1, 1, 1.5, 1.5, 0)}[flavour]
    caps = {'skip': 1 if seam else 3 if flavour == 'skip' else 2, 'crush': 3, 'drop': 3, 'reverse': 1, 'chop': 1}
    plan = {'skip': 0, 'crush': 0, 'drop': 1, 'reverse': 0, 'chop': 0}
    kinds = list(plan)
    for _ in range(budget - 1):
        w = np.array([weights[i] if plan[kd] < caps[kd] else 0 for i, kd in enumerate(kinds)], float)
        if w.sum() <= 0:
            break
        plan[kinds[int(rng.choice(len(kinds), p=w / w.sum()))]] += 1
    notes['events'] = dict(plan)
    # The last word carries the information: no event lands on it (clarity may).
    last = starts[-1] if len(starts) > 1 and off - starts[-1] < .6 * (off - on) else off
    if dur < 1.1:
        # A word or two: damage only the first half, small, so the line can still be made out.
        last = min(last, on + (off - on) // 2)

    # Skips: inserted, so they come first and everything after works on the longer line.
    skips = []
    middle = [s for s in starts[1:] if s < last]
    picks = ([starts[0]] + list(rng.permutation(middle))[:plan['skip'] - 1]) if plan['skip'] else []
    for at in sorted(picks, reverse=True):
        grain = ms(rng.uniform(45, 100))
        reps = int(rng.integers(1, 4 if flavour == 'skip' and at == starts[0] and not seam else 3))
        x, added = stutter(x, rng, at, grain, reps, crushed=rng.random() < .4)
        skips.append({'at_s': round(at / SR, 2), 'grain_ms': round(grain / SR * 1000), 'repeats': reps})
        off += added
        last += added
        starts = [s + added if s > at else s for s in starts]
    if skips:
        notes['skips'] = skips[::-1]

    lead = 0
    if p['pre'] and rng.random() < (p['pre'] if p['pre'] is not True else 1.0):
        # A reversed pre-echo that swells into the first word.
        length = ms(rng.uniform(220, 340))
        frag = x[on:on + length][::-1] * base.ramp(length) ** 2 * 10 ** (rng.uniform(-10, -6) / 20)
        frag = base.reverb(frag, rng, 'room', -4)[:length + ms(40)]
        lead = len(frag)
        x = np.concatenate([np.zeros(lead), x])
        x[on:on + len(frag)] += frag
        on, off, last = on + lead, off + lead, last + lead
        starts = [s + lead for s in starts]
        notes['reversed_pre_s'] = round(length / SR, 2)

    n = len(x)
    dry = x.copy()
    spots = Spots(n)
    spots.take(last, off)

    # Clarity: one word, dry and close, with nothing on it.
    mask = np.ones(n)
    short = dur < 1.1 or len(cue['subtitle'].split()) <= 2
    if short and not p.get('flue'):
        # A word or two: the last of it always comes through clean, or the line is lost.
        a = max(last, on + int(.45 * (off - on)))
        b = min(off + ms(30), a + ms(520))
        r = ms(20)
        mask[a:b] = 0
        mask[max(0, a - r):a] = base.ramp(min(r, a))[::-1]
        mask[b:b + r] = base.ramp(len(mask[b:b + r]))
        spots.take(a - r, b + r)
        notes['clarity_s'] = [round(a / SR, 2), round(b / SR, 2)]
    elif p['clarity'] and rng.random() < p['clarity'] and off - on > .55 * SR and len(starts) > 1:
        later = [s for s in starts if s > on + .35 * (off - on)] or starts[-1:]
        a = int(rng.choice(later))
        b = min(off, a + ms(rng.uniform(280, 520)))
        if b - a > ms(180):
            r = ms(25)
            mask[a:b] = 0
            mask[max(0, a - r):a] = base.ramp(min(r, a))[::-1]
            mask[b:b + r] = base.ramp(len(mask[b:b + r]))
            spots.take(a - r, b + r)
            notes['clarity_s'] = [round(a / SR, 2), round(b / SR, 2)]

    # The voice through the speaker (or up the flue), with its sweeping double.
    if p.get('flue'):
        v, combs = base.flue(x, rng)
        v = peaking(v, rng.uniform(*p['peak']), 4, 1.2)
        notes['flue_combs_hz'] = combs
    else:
        v, sp = speaker(x, rng, p['band'], p['peak'])
        notes['speaker'] = sp
    dbl, d0 = doubling(v, rng)
    v = v + dbl * (1.3 if flavour == 'speaker' else 1.0)
    notes['doubling_ms'] = d0

    # The twin, and the word it takes over.
    swell = None
    tw_level = min(-4.0, p['twin'] + (4 if flavour == 'twin' else 0) + 3 * (k - .8))
    if len(starts) > 2 and rng.random() < (.9 if flavour == 'twin' else .45):
        i = int(rng.integers(1, len(starts) - 1))
        a, b = starts[i], starts[i + 1]
        if spots.free(a, b) and b - a > ms(120) and dur >= 1.1:
            swell = (a, b, rng.uniform(-3, 0))
            notes['twin_takes_over_s'] = [round(a / SR, 2), round(b / SR, 2)]
    tw, st = twin(x, rng, tw_level, swell)
    tw = tw[:n] if len(tw) >= n else np.pad(tw, (0, n - len(tw)))
    if swell:
        g = np.ones(n)
        g[swell[0]:swell[1]] = 1 - .35 * edge(swell[1] - swell[0], ms(60))
        v = v * g
    mix = v + tw
    notes['twin'] = {'semitones': st, 'level_db': round(tw_level, 1)}

    # Crush bursts.
    bursts = []
    for _ in range(plan['crush']):
        length = min(ms(rng.uniform(70, 220)), int(.16 * (off - on)))
        a = spots.pick(rng, on, last, length)
        if a is None:
            continue
        hold, bits = int(rng.integers(5, 14)), int(rng.integers(3, 7))
        mix = splice(mix, a, a + length, crush(mix[a:a + length], hold, bits) * 1.1)
        bursts.append([round(a / SR, 2), round(length / SR, 3), round(SR / hold), bits])
    if bursts:
        notes['crush_bursts'] = bursts          # start s, length s, held rate Hz, bits

    # A short reversed chunk in place.
    if plan['reverse']:
        length = ms(rng.uniform(60, 120))
        a = spots.pick(rng, on + ms(120), last, length)
        if a is not None:
            mix = splice(mix, a, a + length, mix[a:a + length][::-1], ms(5))
            notes['reversed_in_place_s'] = [round(a / SR, 2), round(length / SR, 3)]

    # Hard dropouts, and a fast chop.
    cuts = []
    for _ in range(plan['drop']):
        length = min(ms(rng.uniform(28, 85 if not seam else 60)), int(.07 * (off - on)))
        a = spots.pick(rng, on + ms(100), last, length)
        if a is not None:
            mix = dropout(mix, a, length)
            cuts.append([round(a / SR, 2), round(length / SR, 3)])
    if cuts:
        notes['dropouts_s'] = cuts
    if plan['chop']:
        length = ms(rng.uniform(110, 190))
        a = spots.pick(rng, on + ms(120), last, length)
        if a is not None:
            mix = chop(mix, a, length, rng)
            notes['chop_s'] = [round(a / SR, 2), round(length / SR, 3)]

    # Clarity wins over everything above.
    mix = mask * mix + (1 - mask) * dry * 10 ** (1.5 / 20)

    # Tape: wow, flutter, warbles, a drag, a spin-up or a stop.
    tb = boost['tape']
    warbles, drags = [], []
    for _ in range(int(round(tb * k * rng.uniform(.6, 1.6)))):
        length = ms(rng.uniform(140, 320))
        a = int(rng.uniform(on, max(on + 1, off - length)))
        warbles.append((a, length, rng.uniform(.012, .03) * tb, rng.uniform(5, 9)))
    if rng.random() < .55 * tb * k and not seam:
        length = ms(rng.uniform(130, 260))
        a = int(rng.uniform(on + .2 * (off - on), max(on + .2 * (off - on) + 1, off - length)))
        drags.append((a, length, rng.uniform(.22, .42)))
    stop = None
    if rng.random() < p['stops'] * (1.4 if flavour == 'tape' else 1.0):
        stop = rng.uniform(.18, .32 if seam else .42)
    start = rng.uniform(.06, .12) if p['start'] else None
    wow = rng.uniform(.006, .014) * tb * (.6 + .4 * k)
    flutter = rng.uniform(.0018, .0035) * tb
    mix = tape(mix, rng, mask, wow, flutter, warbles, drags, start, off if stop else None, stop or 0)
    notes['tape'] = {'wow': round(wow, 4), 'flutter': round(flutter, 4), 'warbles': len(warbles),
                     'drag': bool(drags), 'start_s': round(start, 3) if start else None,
                     'stop_s': round(stop, 3) if stop else None}

    # A reversed tail after the last word.
    if p['tail'] and not stop and rng.random() < p['tail']:
        length = ms(rng.uniform(160, 280))
        e = min(len(mix), off + ms(30))
        frag = mix[max(0, e - length):e][::-1] * base.ramp(min(length, e))[::-1] * 10 ** (rng.uniform(-11, -7) / 20)
        mix = np.concatenate([mix, np.zeros(len(frag))])
        mix[e:e + len(frag)] += frag
        notes['reversed_tail_s'] = round(length / SR, 2)

    if p['room']:
        mix = base.reverb(mix, rng, p['room'], p['wet'])
        db, h = base.env_db(mix, .01)
        live = np.flatnonzero(db > db.max() - 50)
        mix = mix[:min(len(mix), (live[-1] + 1) * h + ms(50))]
        notes['room'] = p['room']

    mix = compress(mix)
    vr = 10 ** (base.active_rms_db(mix) / 20)
    bed = base.hiss_bed(len(mix), rng, p['hiss'], 3 * k, vr)
    mix = mix + bed * k
    mix = base.fade(mix, .008)

    narr, name = narration_lufs(cue)
    rel = relative(cue, rng)
    target = narr + rel
    for _ in range(3):
        mix = mix * 10 ** ((target - lufs(mix)) / 20)
        mix = limit(mix)
    notes['level'] = {'narration': name, 'narration_heard_lufs': round(narr, 2), 'relative_lu': round(rel, 2),
                      'target_lufs': round(target, 2), 'lufs': round(lufs(mix), 2),
                      'true_peak_db': round(20 * math.log10(base.true_peak(mix)), 2)}
    return mix, notes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('line', nargs='?')
    ap.add_argument('src', nargs='?')
    ap.add_argument('dst', nargs='?')
    ap.add_argument('--batch', nargs=2, metavar=('RAW_DIR', 'OUT_DIR'))
    ap.add_argument('--only', nargs='*', help='batch: just these ids')
    a = ap.parse_args()
    cues = base.cue_table()
    jobs = []
    if a.batch:
        raw, out = map(Path, a.batch)
        for f in sorted(raw.glob('C[0-9][0-9][0-9].wav')):
            if a.only and f.stem not in a.only:
                continue
            jobs.append((f.stem, f, out / f'{f.stem}.wav', sorted(raw.glob(f'{f.stem}_t*.wav'))))
    else:
        jobs.append((a.line, Path(a.src), Path(a.dst), []))
    report = {}
    for lid, src, dst, extra in jobs:
        y, notes = corrupt(cues[lid], base.load(src), [base.load(t) for t in extra])
        base.write(dst, y)
        notes['seconds'] = round(len(y) / SR, 3)
        report[lid] = notes
        lv = notes['level']
        print(f"{lid} {notes['preset']:8s} i{notes['intensity']} {notes['flavour']:7s} {notes['seconds']:5.2f}s "
              f"LUFS {lv['lufs']:6.1f} (narr {lv['narration_heard_lufs']:6.1f} {lv['relative_lu']:+.1f}) "
              f"tp {lv['true_peak_db']:5.1f}", flush=True)
    if a.batch:
        path = Path(a.batch[1]) / 'render.json'
        old = json.loads(path.read_text()) if path.exists() and a.only else {}
        old.update(report)
        path.write_text(json.dumps(dict(sorted(old.items())), indent=1) + '\n')


if __name__ == '__main__':
    main()
