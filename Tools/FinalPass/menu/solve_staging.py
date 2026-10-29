"""Find clip-free stagings for Mim's two flash frames on the menu close-up.

    python3 Tools/FinalPass/menu/solve_staging.py <staging dir from export_staging_meshes.py> <out.json>

The room, the chair, the cabinet, the candle and Nell (posed) are voxelised at 1.5 cm; everything not reachable from
outside the room's air (seeded from the lens as well) is solid. Mim spans more than this little room, so, as with any
set built for one lens, what counts is what the lens and its flash can see: a candidate Mim (one exported pose, one
yaw, one place) passes only if none of her vertices or triangle samples the lens can see (inside the view with a MARGIN of screen to
spare, and not behind a wall) lands in or within CLEAR cm of anything, what she stands on in view is the floor, her lowest point is on
the floor (0.6 cm above it: planted, not floating), and nothing of her is within NEAR cm of the lens anywhere (so the
lens and the flash beside it are never inside her). The voxel grid is cached in the staging dir (grid.npz). Passing candidates are scored on
where her face lands in frame and whether it faces the lens. The UE side re-checks the winners with exact
mesh-mesh intersection (verify in capture_menu.py).
"""
import json, math, struct, sys
from pathlib import Path
import numpy as np

DIR = Path(sys.argv[1]); OUT = Path(sys.argv[2])
CONFIG = json.loads((Path(__file__).resolve().parent / 'menu_staging.json').read_text())
CAM = dict(CONFIG['camera'], goals=CONFIG['goals'])
RES = 1.5
CLEAR = 1.5
NEAR = 40.0
# Mim at the size the player meets her in the house (actor 0.6 x MimBodyScale 0.85).
SCALE = float(CONFIG.get('mim_scale', 0.51))
MARGIN = 0.06


def load(name):
    with open(DIR / (name + '.bin'), 'rb') as f:
        nv, nt = struct.unpack('<II', f.read(8))
        v = np.frombuffer(f.read(nv * 12), '<f4').reshape(nv, 3).astype(np.float64)
        t = np.frombuffer(f.read(nt * 12), '<i4').reshape(nt, 3)
    return v, t


def samples(v, t, spacing):
    """Points on every triangle no further than `spacing` apart (vertices, edges and interiors)."""
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    longest = np.maximum(np.linalg.norm(b - a, axis=1), np.maximum(np.linalg.norm(c - b, axis=1), np.linalg.norm(a - c, axis=1)))
    n = np.clip(np.ceil(longest / spacing).astype(int), 1, 200)
    out = [v]
    for k in np.unique(n):
        sel = n == k
        if k <= 1:
            out.append((a[sel] + b[sel] + c[sel]) / 3); continue
        i, j = np.meshgrid(np.arange(k + 1), np.arange(k + 1), indexing='ij')
        keep = (i + j) <= k
        u_, w_ = (i[keep] / k)[None, :, None], (j[keep] / k)[None, :, None]
        pts = a[sel][:, None] * (1 - u_ - w_) + b[sel][:, None] * u_ + c[sel][:, None] * w_
        out.append(pts.reshape(-1, 3))
    return np.concatenate(out)


def main():
    index = json.loads((DIR / 'index.json').read_text())
    env = [load(e['name']) for e in index['env']]
    eye = np.array(CAM['eye'])
    cache = DIR / 'grid.npz'
    if cache.exists() and cache.stat().st_mtime > (DIR / 'index.json').stat().st_mtime:
        g = np.load(cache)
        blocked, lo, shape = g['blocked'], g['lo'], tuple(g['shape'])
        floor_z, floor_lo, floor_hi = float(g['floor_z']), g['floor_lo'], g['floor_hi']
    else:
        allv = np.concatenate([v for v, _ in env])
        lo = allv.min(0) - 6; hi = allv.max(0) + 6
        shape = np.ceil((hi - lo) / RES).astype(int) + 1
        surf = np.zeros(shape, bool)
        is_floor = ['Floor' in e['label'] for e in index['env']]
        for (v, t), fl in zip(env, is_floor):
            if fl: continue
            p = samples(v, t, RES * 0.5)
            idx = np.floor((p - lo) / RES).astype(int)
            surf[idx[:, 0], idx[:, 1], idx[:, 2]] = True
        # Air is what the outside can reach; anything enclosed (inside a leg, a body, a wall) is solid.
        free = ~surf
        air = np.zeros(shape, bool)
        air[0, :, :] = free[0]; air[-1] = free[-1]; air[:, 0] = free[:, 0]; air[:, -1] = free[:, -1]; air[:, :, -1] = free[:, :, -1]
        e_ = np.floor((eye - lo) / RES).astype(int); air[e_[0], e_[1], e_[2]] = free[e_[0], e_[1], e_[2]]
        for _ in range(2000):
            grown = air.copy()
            grown[1:] |= air[:-1]; grown[:-1] |= air[1:]; grown[:, 1:] |= air[:, :-1]; grown[:, :-1] |= air[:, 1:]
            grown[:, :, 1:] |= air[:, :, :-1]; grown[:, :, :-1] |= air[:, :, 1:]
            grown &= free
            if (grown == air).all(): break
            air = grown
        solid = ~air
        # Clearance: grow the solid by CLEAR cm.
        grow = int(math.ceil(CLEAR / RES))
        blocked = solid.copy()
        for _ in range(grow):
            g = blocked.copy()
            g[1:] |= blocked[:-1]; g[:-1] |= blocked[1:]; g[:, 1:] |= blocked[:, :-1]; g[:, :-1] |= blocked[:, 1:]
            g[:, :, 1:] |= blocked[:, :, :-1]; g[:, :, :-1] |= blocked[:, :, 1:]
            blocked = g
        floor_v = np.concatenate([v for (v, _), fl in zip(env, is_floor) if fl])
        floor_z = float(np.percentile(floor_v[:, 2], 99.5))
        floor_lo, floor_hi = floor_v[:, :2].min(0), floor_v[:, :2].max(0)
        np.savez_compressed(cache, blocked=blocked, lo=lo, shape=np.array(shape), floor_z=floor_z, floor_lo=floor_lo, floor_hi=floor_hi)

    target = np.array(CAM['target'])
    fwd = target - eye; fwd /= np.linalg.norm(fwd)
    right = np.array([-fwd[1], fwd[0], 0.0]); right /= np.linalg.norm(right)  # UE: yaw turns +X toward +Y, +Y is right
    up = np.cross(fwd, right); up /= np.linalg.norm(up)
    tanh = CAM['filmback'] / 2 / CAM['focal']; tanv = tanh / CAM['aspect']

    def screen(p):
        d = p - eye
        z = d @ fwd
        return 0.5 + (d @ right) / (z * tanh) / 2, 0.5 - (d @ up) / (z * tanv) / 2, z

    def hits(pts):
        # Everything of her the lens or its flash can see stands over the floor and touches nothing.
        d = pts - eye
        z = d @ fwd
        zs = np.maximum(z, 1e-3)
        sx = 0.5 + (d @ right) / (zs * tanh) / 2; sy = 0.5 - (d @ up) / (zs * tanv) / 2
        seen = (z > 1) & (sx > -MARGIN) & (sx < 1 + MARGIN) & (sy > -MARGIN) & (sy < 1 + MARGIN)
        p = pts[seen]
        p = p[visible(p)]
        # What she stands on in view is floor (limbs in the air may reach past its edge into the dark).
        low = p[p[:, 2] < floor_z + 25]
        if (low[:, :2] < floor_lo).any() or (low[:, :2] > floor_hi).any(): return 10 ** 6
        idx = np.floor((p - lo) / RES).astype(int)
        inside = (idx >= 0).all(1) & (idx < np.array(shape)).all(1)
        if not inside.all(): return 10 ** 6
        return int(blocked[idx[:, 0], idx[:, 1], idx[:, 2]].sum())

    def visible(p):
        # Seen: nothing solid on the ray from the lens to within 3 voxels of the point (behind a wall is not seen).
        d = p - eye; L = np.linalg.norm(d, axis=1); out = np.ones(len(p), bool)
        for t in np.arange(0.02, 1.0, 0.012):
            q = eye + d * t
            ii = np.floor((q - lo) / RES).astype(int); ok = (ii >= 0).all(1) & (ii < np.array(shape)).all(1)
            hit = np.zeros(len(p), bool); hit[ok] = blocked[ii[ok, 0], ii[ok, 1], ii[ok, 2]]
            out &= ~(hit & (L * (1 - t) > 3 * RES))
        return out

    goals = CAM['goals']
    results = {g: [] for g in goals}
    for pose in index['poses']:
        v, t = load(pose['name']); v = v * SCALE
        pts_full = samples(v, t, 1.5)
        pts = pts_full[::7]
        eyes = np.array(pose['eyes']) * SCALE; minz = v[:, 2].min()
        for yaw in range(0, 360, 10):
            c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            rot = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])
            local_eyes = rot @ eyes
            face_dir = rot @ np.array([0, 1.0, 0])  # Mim's mesh faces +Y
            rp = pts @ rot.T
            root_z = floor_z + 0.6 - minz
            for g, spec in goals.items():
                for d in np.arange(spec['dist'][0], spec['dist'][1] + 1e-6, spec['step']):
                    # her eyes on the goal's screen column at this distance, at the height the pose stands to
                    sx = spec['screen'][0]
                    ray = fwd + right * (2 * sx - 1) * tanh
                    ray /= np.linalg.norm(ray)
                    flat = np.array([ray[0], ray[1], 0]); flat /= np.linalg.norm(flat)
                    horiz = d
                    e = eye + flat * horiz
                    e[2] = root_z + local_eyes[2]
                    root = e - local_eyes
                    sx_, sy_, depth = screen(e)
                    if depth < 30: continue
                    if abs(sy_ - spec['screen'][1]) > spec.get('tol_y', .2): continue
                    facing = float(face_dir @ ((eye - e) / np.linalg.norm(eye - e)))
                    if facing < spec.get('facing', .5): continue
                    world = rp + root
                    if np.min(np.linalg.norm(world - eye, axis=1)) < NEAR: continue
                    n = hits(world)
                    if n: continue
                    score = abs(sy_ - spec['screen'][1]) * 2 + (1 - facing) + abs(depth - spec['ideal']) / 100
                    results[g].append({'pose': pose['name'], 'clip': pose['clip'], 't': pose['t'], 'yaw': yaw,
                                       'root': root.tolist(), 'scale': SCALE, 'eyes': e.tolist(), 'screen': [sx_, sy_], 'depth': depth,
                                       'facing': facing, 'score': score})
    final = {}
    for g, rows in results.items():
        rows.sort(key=lambda r: r['score'])
        checked = []
        seen = set()
        for r in rows:
            if (r['clip'], r['t']) in seen: continue
            v, t = load(r['pose']); v = v * SCALE
            c, s = math.cos(math.radians(r['yaw'])), math.sin(math.radians(r['yaw']))
            rot = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])
            world = samples(v, t, 1.0) @ rot.T + np.array(r['root'])
            r['full_hits'] = hits(world)
            r['min_lens_cm'] = float(np.min(np.linalg.norm(world - eye, axis=1)))
            r['lowest_above_floor_cm'] = float(world[:, 2].min() - floor_z)
            if r['full_hits'] == 0:
                checked.append(r); seen.add((r['clip'], r['t']))
            if len(checked) >= 8: break
        final[g] = checked
    OUT.write_text(json.dumps({'floor_z': floor_z, 'grid_cm': RES, 'clear_cm': CLEAR, 'near_cm': NEAR, 'camera': CAM,
                               'candidates': {g: len(r) for g, r in results.items()}, 'best': final}, indent=1))
    for g, rows in final.items():
        print(g)
        for r in rows: print('  ', r['clip'], r['t'], 'yaw', r['yaw'], 'screen', [round(x, 2) for x in r['screen']], 'depth', round(r['depth']), 'facing', round(r['facing'], 2), 'score', round(r['score'], 3))


main()
