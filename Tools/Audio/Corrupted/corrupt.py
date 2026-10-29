"""Corruption chain for Mim's second narration.

Each line gets the preset its cue names in corrupted_cues.json (CR_UNDER,
CR_SEAM, CR_BREAK, CR_AFTER, CR_FLUE, CR_MANY), scaled by intensity: the worse
the information, the lighter the damage. Everything random is seeded from the
line id, so a line always renders the same and no two lines render alike.

Chain: the cast voice dropped 2 semitones exactly as the scary pass drops
Mim's spoken lines (so it is the Mim the player hears in the house), then a
first-syllable stutter, old-recording band limit and grit, a low second
register under it (3 to 5 semitones down, slightly late, quiet, its own wow),
dropouts, tape wow and flutter with slow speed drift, a tape start into the
line or a short tape stop on the tail, a reversed fragment (first word or
tail), reverb or chimney resonance, hiss and crackle. One short stretch per
line can drop every effect at once: a moment of sudden clarity, dry and close.
No line is ever pitched down as a whole beyond the in-game -2.

Level is baked: active-speech RMS sits relative to the narration it plays on
(FinalPass/Mastered/VO): break cues at narration level, seam/after/flue cues
3 to 9 dB under it, under cues at the script's gain. True peak stays under
-1 dBFS. Play the files at 0 dB in the engine.

    .venv/bin/python corrupt.py C004 IN.wav OUT.wav
    .venv/bin/python corrupt.py --batch RAW_DIR OUT_DIR   (RAW_DIR/C001.wav ...)
"""
from __future__ import annotations
from pathlib import Path
import argparse
import json
import subprocess
import tempfile
import zlib
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt, sosfilt, fftconvolve, lfilter, resample_poly

ROOT = Path(__file__).resolve().parents[3]
LORE = ROOT / 'Tallow/SourceAssets/Lore/Corrupted'
NARRATION = ROOT / 'Tools/Audio/FinalPass/Mastered/VO'
SR = 48000
BASE_SHIFT = -2.0          # the scary pass drops Mim's converted voice by this much
NARRATION_RMS = -22.9      # median active RMS of the mastered narration
CEILING = -1.2             # true-peak target, dBFS (spec: under -1)

# Lighter damage as the information gets worse.
INTENSITY = {1: 1.0, 2: 0.85, 3: 0.65, 4: 0.4}

PRESETS = {
    'CR_UNDER': dict(band=(250, 3500), grit=.25, wow=1.0, flutter=.5, shadow=-6, reverb='dark',
                     hiss=-40, crackle=4, dropouts=2, reverse='pre', stutter=0, tape='stop?',
                     clarity=.5, fade=.02),
    'CR_SEAM': dict(band=(200, 5200), grit=.35, wow=.5, flutter=1.0, shadow=-13, reverb=None,
                    hiss=-44, crackle=2, dropouts=1, reverse='tail', stutter=.4, tape='stop?',
                    clarity=.6, fade=.015, gate=True),
    'CR_BREAK': dict(band=(160, 6500), grit=.3, wow=.6, flutter=.7, shadow=-12, reverb='room',
                     hiss=-46, crackle=1, dropouts='release', reverse='pre?', stutter=1.0,
                     tape='start', clarity=.7, fade=.01),
    'CR_AFTER': dict(band=None, grit=0, wow=0, flutter=0, shadow=-21, reverb='close',
                     hiss=None, crackle=0, dropouts=0, reverse=None, stutter=0, tape=None,
                     clarity=0, fade=.01),
    'CR_FLUE': dict(band=(140, 4000), grit=.15, wow=.4, flutter=.4, shadow=-16, reverb='flue',
                    hiss=-48, crackle=2, dropouts=1, reverse='tail?', stutter=.3, tape=None,
                    clarity=0, fade=.015, flue=True),
    'CR_MANY': dict(band=(220, 5500), grit=.2, wow=.4, flutter=.5, shadow=-15, reverb=None,
                    hiss=-46, crackle=1, dropouts=1, reverse=None, stutter=0, tape=None,
                    clarity=0, fade=.015, many=True),
}


# ---------------------------------------------------------------- basics

def load(path) -> np.ndarray:
    x, r = sf.read(path, dtype='float64', always_2d=True)
    x = x.mean(1)
    if r != SR:
        from math import gcd
        g = gcd(r, SR)
        x = resample_poly(x, SR // g, r // g)
    return x


def env_db(x, hop=.005):
    h = int(hop * SR)
    n = max(1, len(x) // h)
    return 20 * np.log10(np.sqrt((x[:n * h].reshape(n, h) ** 2).mean(1) + 1e-12)), h


def speech_span(x, below=35.0):
    db, h = env_db(x)
    live = np.flatnonzero(db > max(db.max() - below, -60))
    if not len(live):
        return 0, len(x)
    return live[0] * h, min(len(x), (live[-1] + 1) * h)


def trim(x, pad=.03):
    a, b = speech_span(x, 45)
    return x[max(0, a - int(pad * SR)):min(len(x), b + int(pad * SR))]


def active_rms_db(x):
    db, h = env_db(x, .01)
    m = db > max(np.percentile(db, 95) - 30, -70)
    fr = x[:len(db) * h].reshape(len(db), h)[m]
    return float(10 * np.log10((fr ** 2).mean() + 1e-12))


def true_peak(x):
    return float(np.max(np.abs(resample_poly(x, 4, 1))) + 1e-12)


def ramp(n):
    return 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, max(1, n)))


def fade(x, t):
    n = min(int(t * SR), len(x) // 2)
    if n:
        x = x.copy()
        x[:n] *= ramp(n)
        x[-n:] *= ramp(n)[::-1]
    return x


def band(x, lo=None, hi=None, order=2, zero_phase=True):
    if lo:
        sos = butter(order, lo, 'highpass', fs=SR, output='sos')
        x = sosfiltfilt(sos, x) if zero_phase else sosfilt(sos, x)
    if hi:
        sos = butter(order, hi, 'lowpass', fs=SR, output='sos')
        x = sosfiltfilt(sos, x) if zero_phase else sosfilt(sos, x)
    return x


def pitch(x, semitones):
    """Rubber Band through FFmpeg, same settings as the voice pass (formant shifted)."""
    if not semitones:
        return x.copy()
    ratio = 2 ** (semitones / 12)
    with tempfile.TemporaryDirectory(prefix='corrupt-') as d:
        src, dst = Path(d) / 'in.wav', Path(d) / 'out.wav'
        sf.write(src, x, SR, subtype='FLOAT')
        subprocess.run(['ffmpeg', '-nostdin', '-hide_banner', '-loglevel', 'error', '-y', '-i', str(src),
                        '-af', f'rubberband=pitch={ratio:.9f}:transients=smooth:detector=soft:'
                               'formant=shifted:pitchq=quality',
                        '-ar', str(SR), '-ac', '1', '-c:a', 'pcm_f32le', str(dst)], check=True)
        y, _ = sf.read(dst, dtype='float64')
    return np.pad(y, (0, max(0, len(x) - len(y))))[:len(x)]


def delay(x, seconds, length=None):
    n = int(seconds * SR)
    y = np.concatenate([np.zeros(n), x])
    return y[:length or len(x)]


def smooth_noise(rng, n, rate_hz):
    """Band-limited random curve in -1..1, wandering at about rate_hz."""
    k = max(4, int(n / SR * rate_hz) + 4)
    pts = rng.uniform(-1, 1, k)
    return np.interp(np.linspace(0, k - 1, n), np.arange(k), pts)


# ---------------------------------------------------------------- effects

def stutter(x, rng, onset):
    """One repeat of the first syllable: a grain, a tiny gap, then the line."""
    g = int(rng.uniform(.075, .12) * SR)
    gap = int(rng.uniform(.018, .04) * SR)
    grain = x[onset:onset + g].copy()
    grain *= np.concatenate([ramp(int(.004 * SR)), np.ones(len(grain) - int(.010 * SR)),
                             ramp(int(.006 * SR))[::-1]])[:len(grain)]
    return np.concatenate([x[:onset], grain * rng.uniform(.8, 1.0), np.zeros(gap), x[onset:]]), g + gap


def grit(x, amount):
    if amount <= 0:
        return x
    k = 1 + 7 * amount
    peak = np.max(np.abs(x)) + 1e-9
    y = np.tanh(k * x / peak) / np.tanh(k) * peak
    return (1 - amount) * x + amount * y


def shadow_voice(x, rng, gain_db, wobble):
    """A second, lower register: the same words 3 to 5 semitones down, late and quiet."""
    st = rng.uniform(-5, -3)
    s = pitch(x, st)
    s = band(s, 90, 2400)
    s = delay(s, rng.uniform(.012, .035))
    if wobble:
        s = warp(s, 1 + .004 * smooth_noise(rng, len(s), 1.5))
    return s * 10 ** (gain_db / 20), st


def warp(x, speed):
    """Read the tape at a varying speed (pitch and time move together, like tape)."""
    pos = np.cumsum(speed) - speed[0]
    pos = np.clip(pos, 0, len(x) - 1)
    return np.interp(pos, np.arange(len(x)), x)


def tape_speed(n, rng, mask, wow, flutter, start=None, stop=None, tail_at=None):
    t = np.arange(n) / SR
    w = (wow * .007 * (.65 * np.sin(2 * np.pi * rng.uniform(.35, .9) * t + rng.uniform(0, 6.3))
                       + .35 * smooth_noise(rng, n, .8)))
    f = flutter * .0016 * np.sin(2 * np.pi * rng.uniform(6, 11) * t + rng.uniform(0, 6.3))
    speed = 1 + mask * (w + f)
    if start:
        k = int(start * SR)
        speed[:k] *= np.linspace(.08, 1, k) ** 1.5
    if stop and tail_at is not None:
        k = int(stop * SR)
        a = max(0, min(n - k, tail_at - k))
        speed[a:a + k] *= np.linspace(1, .05, k) ** 1.3
        speed[a + k:] = .05
    return speed


def dropouts(x, rng, where, depth_db=-32):
    g = np.ones(len(x))
    for a, length in where:
        n = int(length * SR)
        r = int(.003 * SR)
        a = int(a)
        seg = np.full(n, 10 ** (depth_db / 20))
        seg[:r] = np.linspace(1, seg[r], r)
        seg[-r:] = np.linspace(seg[-r], 1, r)
        g[a:a + n] = np.minimum(g[a:a + n], seg[:len(g[a:a + n])])
    return x * g


def impulse(rng, kind):
    rt, lp, length = {'dark': (1.7, 2400, 2.2), 'room': (.35, 6000, .6), 'close': (.18, 7000, .3),
                      'flue': (.45, 3000, .7)}[kind]
    n = int(length * SR)
    t = np.arange(n) / SR
    ir = rng.standard_normal(n) * np.exp(-6.9 * t / rt)
    ir = band(ir, 120, lp, zero_phase=False)
    pre = int({'dark': .025, 'room': .008, 'close': .003, 'flue': .004}[kind] * SR)
    ir = np.concatenate([np.zeros(pre), ir])
    return ir / (np.sqrt((ir ** 2).sum()) + 1e-12)


def reverb(x, rng, kind, wet_db):
    tail = fftconvolve(x, impulse(rng, kind))
    y = np.pad(x, (0, len(tail) - len(x)))
    return y + tail * 10 ** (wet_db / 20) * (np.sqrt((x ** 2).sum()) / (np.sqrt((tail ** 2).sum()) + 1e-12))


def flue(x, rng):
    """Narrow tube: two feedback combs between 180 and 400 Hz, boxy, low-passed at 4 kHz."""
    f1 = rng.uniform(180, 260)
    f2 = f1 * rng.uniform(1.45, 1.6)
    y = np.zeros_like(x)
    for f, g in ((f1, rng.uniform(.55, .68)), (f2, rng.uniform(.4, .52))):
        d = int(SR / f)
        a = np.zeros(d + 1)
        a[0], a[-1] = 1, -g
        y += lfilter([1 - g], a, x)
    y = .6 * y + .4 * x
    # Boxy: a broad bump around 600-900 Hz, highs gone.
    sos = butter(2, (rng.uniform(550, 700), rng.uniform(900, 1200)), 'bandpass', fs=SR, output='sos')
    y = y + .5 * sosfiltfilt(sos, y)
    return band(y, 140, 4000, order=4), (round(f1), round(f2))


def hiss_bed(n, rng, level, crackle_rate, voice_rms):
    h = band(rng.standard_normal(n), 1800, 9000, zero_phase=False)
    h *= (1 + .25 * smooth_noise(rng, n, 2))
    h *= voice_rms * 10 ** (level / 20) / (np.sqrt((h ** 2).mean()) + 1e-12)
    if crackle_rate:
        k = rng.poisson(crackle_rate * n / SR)
        for p in rng.integers(0, n - 200, k):
            click = rng.standard_normal(rng.integers(20, 90)) * voice_rms * rng.uniform(.3, 1.2)
            h[p:p + len(click)] += band(click, 800, None, zero_phase=False) if len(click) > 30 else click
    return h


def gate(x, floor_db=-38, att=.002, rel=.04):
    db, h = env_db(x, .002)
    thr = db.max() + floor_db
    g = np.where(db > thr, 1.0, 10 ** ((db - thr) * 1.5 / 20).clip(-60 / 20 * 1.5, 0))
    g = np.repeat(g, h)[:len(x)]
    g = np.pad(g, (0, len(x) - len(g)), mode='edge')
    out = np.empty_like(g)
    a, r, v = 1 - np.exp(-1 / (att * SR)), 1 - np.exp(-1 / (rel * SR)), 1.0
    for i, gi in enumerate(g):
        v += (a if gi > v else r) * (gi - v)
        out[i] = v
    return x * out


def limit(x, ceiling_db):
    c = 10 ** (ceiling_db / 20)
    for _ in range(6):
        up = np.abs(resample_poly(x, 4, 1))
        env = up.reshape(-1, 4).max(1) if len(up) % 4 == 0 else np.pad(up, (0, (-len(up)) % 4)).reshape(-1, 4).max(1)
        env = env[:len(x)]
        if env.max() <= c:
            return x
        g = np.minimum(1, c / (env + 1e-12))
        # Look-ahead min then smooth release, so peaks are caught without clicks.
        w = int(.002 * SR)
        g = np.array([g[max(0, i - w):i + w + 1].min() for i in range(len(g))]) if len(g) < 400000 else g
        g = lfilter([.02], [1, -.98], g[::-1])[::-1]
        g = np.minimum(g, 1)
        x = x * g * .995
    return x * (c / true_peak(x)) if true_peak(x) > c else x


# ---------------------------------------------------------------- chain

def cue_table():
    d = json.loads((LORE / 'corrupted_cues.json').read_text())
    return {c['line_id']: c for c in d['cues']}


def narration_rms(cue):
    name = cue['vo_asset'].rsplit('.', 1)[-1]
    f = NARRATION / f'{name}.wav'
    return active_rms_db(load(f)) if f.exists() else NARRATION_RMS


def target_level(cue, narr):
    g = cue.get('gain_db', 0)
    if cue['placement'] == 'break':
        rel = 0.0
    elif cue['placement'] == 'under':
        rel = float(g)
    else:
        rel = float(np.clip(g, -9, -3))
    return narr + rel, rel


def corrupt(cue, x, extra_takes=(), base_shift=BASE_SHIFT):
    lid = cue['line_id']
    rng = np.random.default_rng(zlib.crc32(lid.encode()))
    p = PRESETS[cue['preset']]
    k = INTENSITY[cue['intensity']]
    notes = {'line': lid, 'preset': cue['preset'], 'intensity': cue['intensity'], 'amount': k,
             'base_shift_st': base_shift}

    x = trim(x)
    x = pitch(x, base_shift)
    if p.get('many'):
        takes = [x] + [pitch(trim(t), base_shift) for t in extra_takes]
        while len(takes) < 3:  # not enough takes: stand-ins from the first, detuned and warped
            takes.append(warp(x, 1 + .006 * smooth_noise(rng, len(x), 2)))
        out, offs = np.zeros(1), []
        for i, t in enumerate(takes[:3]):
            on, _ = speech_span(t)
            t = t[max(0, on - int(.03 * SR)):]
            t = pitch(t, rng.uniform(-.2, .2)) if i else t
            off = 0 if i == 0 else rng.uniform(.008, .015) * i
            t = delay(t, off, len(t) + int(off * SR))
            offs.append(round(off * 1000, 1))
            out = np.pad(out, (0, max(0, len(t) - len(out)))) + np.pad(t, (0, max(0, len(out) - len(t))))
        x = out / 3 ** .5
        notes['stack_offsets_ms'] = offs
        notes['stand_in_takes'] = max(0, 3 - 1 - len(extra_takes))

    on, off = speech_span(x)
    if p['stutter'] and rng.random() < p['stutter'] * (1 if k > .5 else .5):
        x, added = stutter(x, rng, on)
        off += added
        notes['stutter'] = True

    n = len(x)
    # Clarity: one stretch (a word or two near the end) with no corruption at all.
    mask = np.full(n, k)
    if p['clarity'] and rng.random() < p['clarity'] and (off - on) > .5 * SR:
        length = int(min(rng.uniform(.3, .6) * SR, (off - on) * .5))
        a = int(rng.uniform(on + .45 * (off - on), max(on + .45 * (off - on) + 1, off - length)))
        r = int(.025 * SR)
        mask[a:a + length] = 0
        mask[a - r:a] = k * ramp(r)[::-1]
        mask[a + length:a + length + r] = k * ramp(r)
        notes['clarity_s'] = [round(a / SR, 2), round((a + length) / SR, 2)]

    voice = x
    if p['band']:
        lo, hi = p['band']
        voice = band(voice, lo, hi, order=2)
    voice = grit(voice, p['grit'] * k)
    voice = mask / max(k, 1e-9) * voice + (1 - mask / max(k, 1e-9)) * x if k else x

    sh, st = shadow_voice(x, rng, p['shadow'], wobble=p['wow'] > 0)
    shadow_mask = mask / max(k, 1e-9) if p['clarity'] else np.ones(n)
    mix = voice + sh * shadow_mask * (1 if cue['preset'] == 'CR_AFTER' else (.6 + .4 * k))
    notes['shadow'] = {'semitones': round(st, 2), 'gain_db': p['shadow']}

    if p.get('flue'):
        mix, combs = flue(mix, rng)
        mix *= 10 ** (-3 / 20)
        notes['flue_combs_hz'] = combs

    spots = []
    if p['dropouts'] == 'release':
        a = on + rng.uniform(.72, .86) * (off - on)
        spots.append((a, rng.uniform(.04, .07)))
    elif p['dropouts']:
        for _ in range(int(round(p['dropouts'] * k + rng.uniform(-.3, .3)))):
            a = rng.uniform(on + .18 * SR, max(on + .18 * SR + 1, off - .12 * SR))
            if mask[int(a)] > 0:
                spots.append((a, rng.uniform(.025, .06)))
    if spots:
        mix = dropouts(mix, rng, spots)
        notes['dropouts_s'] = [[round(a / SR, 2), round(b, 3)] for a, b in spots]

    tape = p['tape']
    start = .06 if tape == 'start' else None
    stop = None
    if tape == 'stop?' and rng.random() < .45 * k:
        stop = rng.uniform(.14, .22)
    pad_tail = int((stop or 0) * SR * .5)
    mix = np.pad(mix, (0, pad_tail))
    m2 = np.pad(mask, (0, pad_tail), mode='edge')
    if p['wow'] or p['flutter'] or start or stop:
        speed = tape_speed(len(mix), rng, m2, p['wow'], p['flutter'], start, stop, tail_at=off)
        mix = warp(mix, speed)
        notes['tape'] = {'wow': p['wow'] * k, 'flutter': p['flutter'] * k,
                         'start_s': start, 'stop_s': round(stop, 3) if stop else None}

    rev = p['reverse']
    if rev and rev.endswith('?'):
        rev = rev[:-1] if rng.random() < .5 * (k + .2) else None
    if rev == 'pre':
        length = int(rng.uniform(.25, .38) * SR)
        frag = mix[on:on + length][::-1] * ramp(length) ** 2 * 10 ** (rng.uniform(-14, -10) / 20)
        frag = reverb(frag, rng, 'room', -6)[:length + int(.05 * SR)]
        lead = len(frag)
        mix = np.concatenate([np.zeros(lead), mix])
        mix[on:on + len(frag)] += frag  # ends exactly where the first word starts
        notes['reversed'] = f'first word, {length / SR:.2f} s pre-echo'
    elif rev == 'tail':
        length = int(rng.uniform(.22, .35) * SR)
        e = min(len(mix), off + int(.03 * SR))
        frag = mix[max(0, e - length):e][::-1] * ramp(min(length, e))[::-1] * 10 ** (rng.uniform(-15, -11) / 20)
        mix = np.concatenate([mix, np.zeros(len(frag))])
        mix[e:e + len(frag)] += frag
        notes['reversed'] = f'tail, {length / SR:.2f} s'

    if p.get('gate'):
        mix = gate(mix)
    if p['reverb']:
        wet = {'dark': -7, 'room': -16, 'close': -22, 'flue': -9}[p['reverb']]
        mix = reverb(mix, rng, p['reverb'], wet + (0 if p['reverb'] in ('dark', 'flue') else 0))
        notes['reverb'] = p['reverb']
        # Cut the reverb tail once it is inaudible.
        db, h = env_db(mix, .01)
        live = np.flatnonzero(db > db.max() - 48)
        mix = mix[:min(len(mix), (live[-1] + 1) * h + int(.05 * SR))]

    vr = 10 ** (active_rms_db(mix) / 20)
    if p['hiss']:
        bed = hiss_bed(len(mix), rng, p['hiss'], p['crackle'] * k, vr)
        hm = np.pad(mask / max(k, 1e-9), (0, max(0, len(mix) - n)), mode='edge')[:len(mix)]
        hm = np.pad(hm, (len(mix) - len(hm), 0), mode='edge') if len(hm) < len(mix) else hm
        mix = mix + bed * (.15 + .85 * hm) * k
        notes['hiss_db'] = p['hiss']
    mix = fade(mix, p['fade'])

    narr = narration_rms(cue)
    target, rel = target_level(cue, narr)
    mix *= 10 ** ((target - active_rms_db(mix)) / 20)
    mix = limit(mix, CEILING)
    notes['level'] = {'narration_rms_db': round(narr, 2), 'relative_db': rel,
                      'target_rms_db': round(target, 2), 'rms_db': round(active_rms_db(mix), 2),
                      'true_peak_db': round(20 * np.log10(true_peak(mix)), 2)}
    return mix, notes


def write(path, x):
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(path, x, SR, subtype='PCM_24')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('line', nargs='?')
    ap.add_argument('src', nargs='?')
    ap.add_argument('dst', nargs='?')
    ap.add_argument('--takes', nargs='*', default=[], help='extra takes for CR_MANY')
    ap.add_argument('--batch', nargs=2, metavar=('RAW_DIR', 'OUT_DIR'))
    ap.add_argument('--base-shift', type=float, default=BASE_SHIFT)
    a = ap.parse_args()
    cues = cue_table()
    jobs = []
    if a.batch:
        raw, out = map(Path, a.batch)
        for f in sorted(raw.glob('C[0-9][0-9][0-9].wav')):
            extra = sorted(raw.glob(f'{f.stem}_t*.wav'))
            jobs.append((f.stem, f, out / f'{f.stem}.wav', extra))
    else:
        jobs.append((a.line, Path(a.src), Path(a.dst), [Path(t) for t in a.takes]))
    report = {}
    for lid, src, dst, extra in jobs:
        y, notes = corrupt(cues[lid], load(src), [load(t) for t in extra], a.base_shift)
        write(dst, y)
        notes['seconds'] = round(len(y) / SR, 3)
        report[lid] = notes
        lv = notes['level']
        print(f"{lid} {notes['preset']:8s} i{notes['intensity']} {notes['seconds']:5.2f}s "
              f"rms {lv['rms_db']:6.1f} (narr {lv['narration_rms_db']:6.1f} {lv['relative_db']:+.0f}) "
              f"tp {lv['true_peak_db']:5.1f}")
    if a.batch:
        (Path(a.batch[1]) / 'render.json').write_text(json.dumps(report, indent=1) + '\n')
    else:
        Path(str(jobs[0][2]).rsplit('.', 1)[0] + '.json').write_text(json.dumps(report[jobs[0][0]], indent=1) + '\n')


if __name__ == '__main__':
    main()
