"""The objective sounds (29 Oct: a faint sound you can follow to what the chapter needs next). Plays nothing.

    python3 build_guide_sounds.py <project>   -> <project>/Tallow/SourceAssets/FinalPass/Tapes/*.wav + guide-sounds.json

AT_TapeHiss_Loop     the chapter's tape machine, switched on and waiting: tape hiss (shaped noise, the band a
                     quarter-inch tape hisses in, 1.5 to 11 kHz, drifting a little as tape does) over the projector's
                     own motor (S_Projector_Run_Loop) low-passed to a soft purr at -21 dB, so through a wall, where the
                     hiss is muffled away, the purr is what is left. 24 s loop.
AT_BatteryHum_Loop   the battery: the humming box (AT_HummingBox_Loop, the transformer recordings) three semitones
                     up, band-limited 90 Hz to 1.8 kHz, and breathing slowly in and out (2.8 s), so it is the box's
                     family and still not the box. One loop of the pitched source (8.4 s).
The fuse box itself is AT_HummingBox_Loop as it is. Mastered like the house beds (-30 LUFS, peak -6 dBFS) so the
runtime volumes sit next to theirs; the levels are set by guide_levels.py.
"""
from pathlib import Path
import json, math, subprocess, sys
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'sound')); sys.dont_write_bytecode = True
import build_sound as bs      # the house bank's own signal helpers (load, ff, master, loop, write)

COPY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[3]
BUILT = COPY / 'Tallow/SourceAssets/Audio/FinalPassSound/Built'
EXPORT = HERE.parent / 'ambience/Review/Export'      # the game's own waves (ambience/inventory_audio.py)
OUT = COPY / 'Tallow/SourceAssets/FinalPass/Tapes'
RATE = bs.RATE
RNG = np.random.default_rng(1894)


def wav(path):
    return np.frombuffer(subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-i', str(path), '-ac', '1', '-ar', str(RATE), '-f', 'f32le', '-'],
                                        capture_output=True, check=True).stdout, np.float32).astype(np.float64)


def tape_hiss():
    seconds = 24.
    n = int((seconds + 2.) * RATE)
    hiss = RNG.standard_normal(n)
    # Tape hiss rises toward the top: high-passed at 1.5 kHz, a broad lift around 5 kHz, rolled off past 11 kHz.
    hiss = bs.ff(hiss, 'highpass=f=1500,highpass=f=1500,equalizer=f=5000:t=q:w=0.8:g=4,lowpass=f=11000,lowpass=f=11000')
    # Tape is never quite steady: a slow drift of about a decibel, no faster than a breath.
    t = np.arange(n) / RATE
    drift = .55 * np.sin(2 * math.pi * t / 6.5 + .4) + .35 * np.sin(2 * math.pi * t / 2.7 + 1.9)
    hiss *= bs.db(drift)
    hiss = bs.loop(hiss, seconds)
    motor = wav(EXPORT / 'Projector/S_Projector_Run_Loop.wav')               # an 8 s loop: three of it is the 24 s loop
    motor = bs.ff(np.tile(motor, 3), 'lowpass=f=420,lowpass=f=420,highpass=f=45')[:len(hiss)]
    motor = np.pad(motor, (0, len(hiss) - len(motor)))
    motor *= np.sqrt(np.mean(hiss ** 2)) / (np.sqrt(np.mean(motor ** 2)) + 1e-12) * bs.db(-21)
    return bs.master(hiss + motor, -30., -6.)


def battery_hum():
    box = wav(BUILT / 'AT_HummingBox_Loop.wav')
    up = 2 ** (3 / 12)
    # Three semitones up: the whole loop resampled, so it still loops.
    hum = bs.ff(box, 'asetrate=%d,aresample=%d,highpass=f=90,highpass=f=90,lowpass=f=1800,lowpass=f=1800' % (int(RATE * up), RATE))
    t = np.arange(len(hum)) / RATE
    cycles = max(1, round(len(hum) / RATE / 2.8))
    breathe = .5 - .5 * np.cos(2 * math.pi * cycles * t / (len(hum) / RATE))    # whole cycles: seamless at the loop
    hum *= bs.db(-5. * (1 - breathe))
    return bs.master(hum, -30., -6.)


def main():
    OUT.mkdir(parents=True, exist_ok=True); rows = []
    for name, build, recipe, sources in (
            ('AT_TapeHiss_Loop', tape_hiss, 'shaped noise 1.5 to 11 kHz with a slow 1 dB drift, over S_Projector_Run_Loop low-passed to 420 Hz at -21 dB; 24 s loop',
             ['synthesised', 'S_Projector_Run_Loop (project sound)']),
            ('AT_BatteryHum_Loop', battery_hum, 'AT_HummingBox_Loop up 3 semitones, 90 Hz to 1.8 kHz, breathing 5 dB over about 2.8 s; one loop',
             ['AT_HummingBox_Loop (bsb_0086, bsb_2437, Joseph Sardin, BigSoundBank, CC0)'])):
        a = build(); path = OUT / (name + '.wav'); bs.write(path, a)
        rows.append({'name': name, 'file': str(path), 'seconds': round(len(a) / RATE, 2), 'lufs': round(bs.measure(a), 1),
                     'peak_dbfs': round(20 * math.log10(np.abs(a).max() + 1e-12), 1), 'recipe': recipe, 'sources': sources, 'looping': True})
        print('GUIDE_SOUND', json.dumps(rows[-1]))
    (OUT / 'guide-sounds.json').write_text(json.dumps(rows, indent=1) + '\n')


if __name__ == '__main__':
    main()
