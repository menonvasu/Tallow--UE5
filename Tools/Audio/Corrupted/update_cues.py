"""Write the rendered lengths and asset paths into corrupted_cues.json.

For every cue: `asset` (the imported SoundWave), `duration_seconds` (the rendered
file), `est_duration` replaced by that length, and `engine_gain_db` 0 because the
level is baked into the file by corrupt.py (`gain_db` stays as the script's intent).
Break cues that start a chain hold the original for the whole chain: the sum of the
chained lengths and gaps plus 0.25 s, as the timing note asks. Re-runnable.

    .venv/bin/python update_cues.py
"""
from pathlib import Path
import json
import soundfile as sf

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
LORE = ROOT / 'Tallow/SourceAssets/Lore/Corrupted'
CUES = LORE / 'corrupted_cues.json'
FINAL = LORE / 'final'
DEST = '/Game/FinalPass/Corrupted'


def main():
    data = json.loads(CUES.read_text())
    imported = {}
    result = HERE / 'import-result.json'
    if result.exists():
        imported = {a['line']: a for a in json.loads(result.read_text())['assets']}
    by_id = {c['line_id']: c for c in data['cues']}
    for c in data['cues']:
        f = FINAL / f"{c['line_id']}.wav"
        seconds = round(sf.info(f).duration, 3)
        name = f"VO_Corrupted_{c['line_id']}"
        c['asset'] = imported.get(c['line_id'], {}).get('asset', f'{DEST}/{name}.{name}')
        c['duration_seconds'] = seconds
        c['est_duration'] = seconds
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
    data['timing_note'] = (data['timing_note'].split(' Rendered:')[0] +
                           ' Rendered: est_duration and duration_seconds are the real file lengths; '
                           'interrupt_seconds on a break cue that starts a chain covers the whole chain plus 0.25 s. '
                           'Levels are baked into the files (play at engine_gain_db 0).')
    CUES.write_text(json.dumps(data, indent=1, ensure_ascii=False) + '\n')
    print(f"{len(data['cues'])} cues updated, {sum(1 for c in data['cues'] if c['line_id'] in imported)} with imported assets")


if __name__ == '__main__':
    main()
