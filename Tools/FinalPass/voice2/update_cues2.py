"""The cue sheet with the new render lengths, and the game's cue table regenerated from it.

The harsher renders are a little longer or shorter than the first ones (skips, stops and reversed grabs move the
ends), and the scheduler chains lines and holds the narration by those lengths. This writes a copy of
Lore/Corrupted/corrupted_cues.json beside this script with `duration_seconds` / `est_duration` from voice2/final and
`interrupt_seconds` recomputed for break chains (whole chain plus 0.25 s, as update_cues.py does), then runs the
overlay's gen_cues.py on it into <project>/Tallow/Source/Tallow/Audio/TallowCorruptedCues.inl. Asset paths, starts,
gaps, placements and subtitles are unchanged. Re-runnable.

    python3 update_cues2.py <copy-dir>
"""
from pathlib import Path
import json
import subprocess
import sys
import wave

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SRC = ROOT / 'Tallow/SourceAssets/Lore/Corrupted/corrupted_cues.json'
OUT = HERE / 'corrupted_cues.json'
FINAL = HERE / 'final'
GEN = ROOT / 'Tools/FinalPass/overlay/gen_cues.py'


def seconds(path):
    with wave.open(str(path)) as w:
        return round(w.getnframes() / w.getframerate(), 3)


def main():
    copy = Path(sys.argv[1]).resolve()
    data = json.loads(SRC.read_text())
    changes = []
    for c in data['cues']:
        s = seconds(FINAL / f"{c['line_id']}.wav")
        changes.append((c['line_id'], c['duration_seconds'], s))
        c['duration_seconds'] = s
        c['est_duration'] = s
        c['engine_gain_db'] = 0
    for c in data['cues']:
        if c['placement'] != 'break' or c['chain_after']:
            continue
        total, cur = c['duration_seconds'], c['line_id']
        while True:
            nxt = next((d for d in data['cues'] if d['chain_after'] == cur), None)
            if not nxt:
                break
            total += nxt['chain_gap'] + nxt['duration_seconds']
            cur = nxt['line_id']
        c['interrupt_seconds'] = round(total + .25, 3)
    data['timing_note'] += ' Lengths from the second, harsher render (Tools/FinalPass/voice2).'
    OUT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + '\n')
    inl = copy / 'Tallow/Source/Tallow/Audio/TallowCorruptedCues.inl'
    subprocess.run([sys.executable, str(GEN), str(OUT), str(inl)], check=True)
    d = [b - a for _, a, b in changes]
    print(f'{len(changes)} cues; length change {min(d):+.2f} to {max(d):+.2f} s, mean {sum(d) / len(d):+.2f} s -> {inl}')


if __name__ == '__main__':
    main()
