"""Place Mim's face for the menu's candle flicker (29 Oct: "the light flickers, Mim's face is there, static; it
flickers again; she's gone", and not her side next to Nell).

    python3 Tools/FinalPass/menu/solve_face.py <staging dir> <out menu_frames.json>

<staging dir> is export_staging_meshes.py's output (her held poses in actor space, and grid.npz, the room's voxels with
Nell in them). Only her face is drawn (ATallowMenuScene::FaceSlots), so only her face is placed and tested: the
vertices within FACE_CM of her eyes. Each candidate puts her eyes on a point of the dark beyond Nell's shoulder
(screen column and row, depth from the lens), turns her face to the lens, then TURN degrees off it away from the flame (so the candle rakes across one
side), tilts it slightly, and passes only if the
face is clear of the room and Nell by CLEAR cm, is seen (nothing between it and the lens), and the candle's flame
sees it too (so the flare is what lights it). The best two distinct places become the two appearances.
"""
import json, math, struct, sys
from pathlib import Path
import numpy as np

DIR = Path(sys.argv[1]); OUT = Path(sys.argv[2])
CONFIG = json.loads((Path(__file__).resolve().parent / 'menu_staging.json').read_text())
CAM = CONFIG['camera']
CANDLE = np.array([145.0, -55.0, 94.0])   # MenuCandleLight
RES = 1.5; CLEAR = 4.0; FACE_CM = 42.0
SIZE = 0.45                                 # as in the house (0.51), "a little bit smaller"
POSES = ('pose_MIM_Act_Upright_Idle_2.00', 'pose_MIM_Cap_Smile_Reveal_1.00', 'pose_MIM_Story_Wake_Watch_2.00')
COLUMNS = (0.78, 0.8, 0.82, 0.84, 0.86)
ROWS = (0.26, 0.32, 0.38)
DEPTHS = range(310, 371, 10)
IDEAL = {'column': 0.82, 'row': 0.32, 'depth': 340}  # clear of the frame edge and Nell's head, deep in the room
# Her face turns this far off the lens, away from the flame, so the candle rakes across it from one side and the
# far side falls into shadow (face-on to the flame it reads as a flat lit disc).
TURN = 55.0
# Head tilts (UE pitch turns a +Y face sideways, roll nods it). A doll's tilt, not a pose.
TILTS = ((10.0, 4.0), (-8.0, 8.0))


def load(name):
    with open(DIR / (name + '.bin'), 'rb') as f:
        nv, nt = struct.unpack('<II', f.read(8))
        return np.frombuffer(f.read(nv * 12), '<f4').reshape(nv, 3).astype(np.float64)


def rotation(pitch, yaw, roll):
    """Rows are where local X, Y, Z go (FRotationMatrix)."""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return np.array([[cp * cy, cp * sy, sp],
                     [sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp],
                     [-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp]])


def main():
    index = json.loads((DIR / 'index.json').read_text())
    poses = {p['name']: p for p in index['poses']}
    g = np.load(DIR / 'grid.npz')
    blocked, lo, shape = g['blocked'], g['lo'], np.array(g['shape'])
    eye = np.array(CAM['eye']); fwd = np.array(CAM['target']) - eye; fwd /= np.linalg.norm(fwd)
    right = np.array([-fwd[1], fwd[0], 0.0]); right /= np.linalg.norm(right)
    up = np.cross(fwd, right); up /= np.linalg.norm(up)
    tanh = CAM['filmback'] / 2 / CAM['focal']; tanv = tanh / CAM['aspect']
    reach = int(math.ceil(CLEAR / RES))

    # The room grown by CLEAR in every direction (a cube, axis by axis), so one lookup tests the clearance.
    grown = blocked.copy()
    for axis in range(3):
        step = grown.copy()
        for k in range(1, reach + 1):
            step |= np.roll(grown, k, axis) | np.roll(grown, -k, axis)
        grown = step

    def solid(p):
        """Within CLEAR of anything solid (a point outside the grid is in open air)."""
        ii = np.floor((p - lo) / RES).astype(int)
        ok = (ii >= 0).all(1) & (ii < shape).all(1)
        hit = np.zeros(len(p), bool)
        hit[ok] = grown[ii[ok, 0], ii[ok, 1], ii[ok, 2]]
        return hit

    def clear_line(a, b, skip=0.0):
        """Nothing solid between a and b, past the first skip cm (the flame sits inside its own candle)."""
        for t in np.arange(max(0.02, skip / np.linalg.norm(b - a)), 0.97, 0.004):
            p = a + (b - a) * t
            ii = np.floor((p - lo) / RES).astype(int)
            if (ii >= 0).all() and (ii < shape).all() and blocked[tuple(ii)]: return False
        return True

    rows = []
    for name in POSES:
        pose = poses[name]
        mesh = load(name)
        eyes0 = np.array(pose['eyes'])
        face0 = mesh[np.linalg.norm(mesh - eyes0, axis=1) < FACE_CM][::4]
        for column in COLUMNS:
            for row in ROWS:
                for depth in DEPTHS:
                    face = eye + depth * (fwd + right * (2 * column - 1) * tanh + up * (1 - 2 * row) * tanv)
                    to_lens = math.degrees(math.atan2(eye[1] - face[1], eye[0] - face[0]))
                    to_flame = math.degrees(math.atan2(CANDLE[1] - face[1], CANDLE[0] - face[0]))
                    away = 1.0 if ((to_lens - to_flame + 180) % 360 - 180) > 0 else -1.0
                    yaw = to_lens + away * TURN - 90  # +Y at the lens, turned off the flame
                    for k, (pitch, roll) in enumerate(TILTS):
                        R = rotation(pitch, yaw, roll)
                        root = face - (eyes0 * SIZE) @ R
                        w = (face0 * SIZE) @ R + root
                        if solid(w).any(): continue
                        if not clear_line(eye, face) or not clear_line(CANDLE, face, skip=6.0): continue
                        facing = float(R[1] @ ((eye - face) / np.linalg.norm(eye - face)))
                        score = (abs(column - IDEAL['column']) * 2 + abs(row - IDEAL['row']) * 2
                                 + abs(depth - IDEAL['depth']) / 150 + np.linalg.norm(face - CANDLE) / 400)
                        rows.append({'pose': name, 'clip': pose['clip'], 't': pose['t'], 'scale': SIZE, 'tilt': k,
                                     'pitch': pitch, 'yaw': round(yaw, 2), 'roll': roll, 'root': root.tolist(),
                                     'face': face.tolist(), 'face_screen': [column, row], 'depth': depth,
                                     'facing': round(facing, 3), 'flame_angle': round(math.degrees(math.acos(max(-1, min(1, float(R[1] @ ((CANDLE - face) / np.linalg.norm(CANDLE - face))))))), 1), 'candle_cm': round(float(np.linalg.norm(face - CANDLE)), 1),
                                     'score': round(score, 4)})
    rows.sort(key=lambda r: r['score'])
    print(len(rows), 'clean places')
    best = []
    for r in rows:
        # Two different faces in two different places, each with its own tilt.
        if any(b['clip'] == r['clip'] or b['tilt'] == r['tilt'] or abs(b['face_screen'][1] - r['face_screen'][1]) < .05
               or abs(b['depth'] - r['depth']) < 20 for b in best): continue
        best.append(r)
        if len(best) == 2: break
    assert best, 'no clean place for her face'
    for r in best:
        print(r['clip'], r['t'], 'screen', r['face_screen'], 'depth', r['depth'], 'candle', r['candle_cm'], 'flame angle', r['flame_angle'], 'facing', r['facing'], 'tilt', r['pitch'], r['roll'])
    shows = [{'frames': [{'root': r['root'], 'yaw': r['yaw'], 'pitch': r['pitch'], 'roll': r['roll'], 'scale': r['scale'],
                          'clip': r['clip'], 't': r['t'], 'seconds': 0.35}],
              'sting': 'low', 'empty_chair': 0.0, 'staging': [r]} for r in best]
    OUT.write_text(json.dumps({'about': "From solve_face.py: Mim's face only, static, in the dark beyond Nell, caught by the "
                                        'candle between two dips. Read by build_menu_scene.py.', 'apparitions': shows}, indent=1))


main()
