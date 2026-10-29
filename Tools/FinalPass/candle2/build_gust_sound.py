"""The gust that takes the candle (29 Oct: your candle betrays you). Plays nothing.

    python3 build_gust_sound.py <project>   -> <project>/Tallow/SourceAssets/FinalPass/Candle/S_Candle_Gust.wav + gust-sound.json

S_Candle_Gust   a short push of air past the flame as Mim comes right up, just before the puff (P_Candle_Puff) of it
                going out. The strongest swell of "Wind from inside #1" (bsb_1714, Joseph Sardin, BigSoundBank, CC0),
                120 Hz to 2.6 kHz, under a close breath of shaped noise (250 Hz to 1.8 kHz) so it is near and in the room,
                not outside; 0.12 s rise, a short hold, gone by 0.85 s. Mastered like the bank's details (-24 LUFS, peak -3 dBFS).
"""
from pathlib import Path
import json, math, sys
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'sound')); sys.dont_write_bytecode = True
COPY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[3]
sys.argv = [sys.argv[0], str(COPY)]   # build_sound reads its copy (for the BigSoundBank sources) from argv
import build_sound as bs

OUT = COPY / 'Tallow/SourceAssets/FinalPass/Candle'
RATE = bs.RATE
RNG = np.random.default_rng(1894)
SECONDS = .85


def envelope(n):
    t = np.arange(n) / RATE
    rise, hold = .12, .1
    e = np.where(t < rise, np.sin(np.pi / 2 * t / rise) ** 2, np.exp(-np.maximum(t - rise - hold, 0.) / .16))
    return e * np.clip((SECONDS - t) / .05, 0., 1.)


def gust():
    wind = bs.lp(bs.hp(bs.load('1714', 5, 80), 120), 2600)
    # The strongest swell: the loudest 0.85 s window by its envelope, started a little before its peak.
    e = bs.envelope_db(wind, .01)
    w = int(SECONDS / .01)
    at = int(np.argmax(np.convolve(e, np.ones(w) / w, 'valid')))
    start = max(0, int((at * .01 - .08) * RATE))
    n = int(SECONDS * RATE)
    swell = wind[start:start + n]
    breath = bs.ff(RNG.standard_normal(n + RATE // 10), 'highpass=f=250,highpass=f=250,lowpass=f=1800,lowpass=f=1800')[:n]
    breath *= np.sqrt(np.mean(swell ** 2)) / (np.sqrt(np.mean(breath ** 2)) + 1e-12) * bs.db(-4)
    # The air ruffles the flame as it passes: a quick, shallow flutter.
    t = np.arange(n) / RATE
    flutter = 1. + .18 * np.sin(2 * math.pi * 13. * t) * np.exp(-t / .4)
    a = (swell + breath) * envelope(n) * flutter
    return bs.master(bs.fade(a, .005, .05), -24., -3.)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    a = gust(); path = OUT / 'S_Candle_Gust.wav'; bs.write(path, a)
    row = {'name': 'S_Candle_Gust', 'file': str(path), 'seconds': round(len(a) / RATE, 2), 'lufs': round(bs.measure(a), 1),
           'peak_dbfs': round(20 * math.log10(np.abs(a).max() + 1e-12), 1),
           'recipe': 'strongest 0.85 s swell of bsb_1714, 120 Hz to 2.6 kHz, under shaped noise 250 Hz to 1.8 kHz at -4 dB; 0.12 s rise, gone by 0.85 s',
           'sources': ['bsb_1714 Wind from inside #1 (Joseph Sardin, BigSoundBank, CC0)', 'synthesised'], 'looping': False}
    (OUT / 'gust-sound.json').write_text(json.dumps([row], indent=1) + '\n')
    print('GUST_SOUND', json.dumps(row))


if __name__ == '__main__':
    main()
