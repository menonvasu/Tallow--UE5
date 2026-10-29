"""Give the house shell (floor slabs, ceilings, wall runs, skirting, dado, door and window joinery) each room's finish
from env_library.ROOMS. Re-runnable; saves the map.

  run.sh headless apply_room_finish.py room-finish              (TALLOW_ENV_DRY=1: report only)

Every visible shell actor slot that renders a shared house surface (ArtDirection09 MI_A9_*_World, or an earlier
shell instance from this script) gets MI_Env_Shell_<legacy>_<rooms>: a M_Env_HouseZones instance that carries the
legacy instance's own values (so anything outside the listed rooms looks exactly as before) plus one zone per room
the slot reaches. The role of a slot in a room comes from its legacy surface (walls, joinery, paint) and, for floor
slabs, from height (the slab is the floor of the room above it and the ceiling of the room below).
Needs build_env_materials.py first. No geometry moves: navigation is unaffected.
Report: <project>/Review/Results/room-finish.json
"""
import hashlib
import json
import os
from pathlib import Path
import unreal as u
import env_library as LIB
import house_zones as HZ
from env_common import LEVEL, MAT, LEGACY, TEX

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
L = u.MaterialEditingLibrary; E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools()
R = {'errors': [], 'assigned': [], 'instances': {}, 'dry': DRY}
# Drawn no more: the scanned floors carry their own board joints, and these strips run through every level.
DRAW_OFF = ['HI_SM_HouseV01_FloorBoardJoints']
SHELL = ('HI_SM_HouseV01_', 'EV04_SM_HouseV04_', 'EV05_SM_HouseV05_', 'A9_Parlour_Cornice', 'TRIM_', 'StairHallWall', 'House_')
# legacy surface -> role ('slab' = floor or ceiling by height)
ROLE = {'WillowPaper': 'wall', 'RosePaper': 'wall', 'SmokePlaster': 'wall', 'Chalk': 'wall', 'OlivePaint': 'wall_lower', 'SootBrick': 'wall',
        'FloorOak': 'slab', 'Walnut': 'joinery', 'IvoryPaint': 'paint'}
OLD = {'G1_Wallpaper': 'WillowPaper', 'G1_WallLower': 'OlivePaint', 'G1_Paint': 'IvoryPaint', 'G1_PortalPaint': 'OlivePaint',
       'G1_Floor': 'FloorOak', 'G1_Ceiling': 'FloorOak', 'G1_Joinery': 'Walnut'}   # the parlour's first, single-room instances


LEGACY_DIRS = {'MI_A9_': LEGACY, 'MI_A6_': '/Game/Presentation/Environment/HouseArtV06/Materials'}


def legacy_of(m):
    """('WillowPaper', legacy instance) for a slot material, or None when it is not a shell surface."""
    if not m: return None
    n = m.get_name()
    if n[:6] in LEGACY_DIRS and n.endswith('_World'):
        key, legacy = n[6:-len('_World')], m
    elif n.startswith('MI_Env_Shell_'):
        key = n[len('MI_Env_Shell_'):].rsplit('_', 1)[0]; legacy = None
    elif n.startswith('MI_Env_') and n[len('MI_Env_'):] in OLD:
        key = OLD[n[len('MI_Env_'):]]; legacy = None
    else:
        return None
    if key not in ROLE: return None
    if legacy is None:
        for prefix, folder in LEGACY_DIRS.items():
            if E.does_asset_exist(f'{folder}/{prefix}{key}_World'):
                legacy = u.load_asset(f'{folder}/{prefix}{key}_World'); break
    return (key, legacy) if legacy else None


def zone_box(room):
    lo, hi = LIB.LEVELS[room['level']]
    return (room['x'][0], room['y'][0], lo - 5.0), (room['x'][1], room['y'][1], hi + 2.0)


def role_in(room, key, zlo, zhi, label=''):
    if any(w in label for w in ('Stairs', 'StairDetail', 'Balustrade', 'UnderStair')):
        return 'stairs'                     # treads and handrails keep their own look unless a room gives 'stairs'
    r = ROLE[key]
    if r != 'slab': return r
    lo, hi = LIB.LEVELS[room['level']]
    if zlo - 5 <= lo <= zhi + 5: return 'floor'
    if zlo - 5 <= hi <= zhi + 5: return 'ceiling'
    return None


def rooms_for(a, key):
    o, e = a.get_actor_bounds(False)
    lo = (o.x - e.x - 30, o.y - e.y - 30, o.z - e.z + 2); hi = (o.x + e.x + 30, o.y + e.y + 30, o.z + e.z - 2)
    hits = []
    for name, room in LIB.ROOMS.items():
        bl, bh = zone_box(room)
        if all(bl[i] <= hi[i] and bh[i] >= lo[i] for i in range(3)):
            role = role_in(room, key, o.z - e.z, o.z + e.z, a.get_actor_label())
            if role and role in room:
                hits.append((name, role))
    return hits


def shell_instance(key, legacy, hits, master):
    sig = hashlib.sha1(json.dumps([key, hits, {n: LIB.ROOMS[n] for n, _ in hits}], sort_keys=True, default=str).encode()).hexdigest()[:6]
    name = f'MI_Env_Shell_{key}_{sig}'
    if name in R['instances']: return u.load_asset(f'{MAT}/{name}')
    sets, pats, spots = [], [], []
    for room_name, role in hits:
        texset, params = LIB.ROOMS[room_name][role]
        if texset not in sets: sets.append(texset)
        if params.get('pattern') and params['pattern'] not in pats: pats.append(params['pattern'])
        for s in LIB.ROOMS[room_name].get('soot', []):
            if s not in spots: spots.append(s)
    if len(sets) > 2 or len(pats) > 3 or len(hits) > HZ.ZONES:
        R['errors'].append(f'{name}: {len(sets)} sets, {len(pats)} patterns, {len(hits)} rooms (max 2/3/{HZ.ZONES}); extra rooms dropped')
    R['instances'][name] = {'legacy': key, 'rooms': hits, 'sets': sets[:2], 'patterns': pats[:3]}
    if DRY: return None
    path = f'{MAT}/{name}'
    mi = u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(name, MAT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    L.set_material_instance_parent(mi, master)
    for p in L.get_scalar_parameter_names(legacy):
        L.set_material_instance_scalar_parameter_value(mi, p, L.get_material_instance_scalar_parameter_value(legacy, p))
    for p in L.get_texture_parameter_names(legacy):
        L.set_material_instance_texture_parameter_value(mi, p, L.get_material_instance_texture_parameter_value(legacy, p))
    for p in L.get_vector_parameter_names(legacy):
        L.set_material_instance_vector_parameter_value(mi, p, L.get_material_instance_vector_parameter_value(legacy, p))
    def tex(n):
        t = u.load_asset(f'{TEX}/{n}') if E.does_asset_exist(f'{TEX}/{n}') else None
        if not t: R['errors'].append(f'{name}: no texture {n} in this copy (build_env_materials.py after fetch_polyhaven.py)')
        return t or u.load_asset(f'{TEX}/T_Env_Paper_{n.rsplit("_", 1)[1]}')
    for slot, texset in zip('AB', (sets + sets)[:2]):
        for k, kind in (('Base', 'D'), ('Norm', 'N'), ('ARM', 'ARM')):
            L.set_material_instance_texture_parameter_value(mi, f'{slot}_{k}', tex(f'T_Env_{texset}_{kind}'))
    for i in range(3):
        L.set_material_instance_texture_parameter_value(mi, f'Pat{i}', tex(f'T_Env_{(pats + ["WillowBough"] * 3)[i]}_D'))
    for i in range(HZ.SPOTS):
        s = spots[i] if i < len(spots) else (0, 0, -1e4, 1)
        L.set_material_instance_vector_parameter_value(mi, f'S{i}', u.LinearColor(*[float(x) for x in s]))
    for i in range(HZ.ZONES):
        if i >= len(hits):
            L.set_material_instance_vector_parameter_value(mi, f'Z{i}Max', u.LinearColor(0, 0, 0, 0)); continue
        room_name, role = hits[i]; room = LIB.ROOMS[room_name]
        texset, params = room[role]
        bl, bh = zone_box(room)
        vals = dict(Tile=(100, 100, 100, 100), Tint=(1, 1, 1, 1), Rough=(1, 0, 0, 0.5), Grime=(0, 0, 9999, 0), Mark=(0, 0, 0, 0),
                    Pat=(0.2, 0, 0, 0), Opt=(0, 0, 1, 0), Extra=(0, 0, 0.2, 0))
        vals.update({k: v for k, v in params.items() if k in vals})
        ex = list(vals['Extra']); ex[3] = float(pats.index(params['pattern'])) if params.get('pattern') in pats else 0.0
        vals['Extra'] = tuple(ex)
        vals['Min'] = bl + (float(sets.index(texset)) if texset in sets[:2] else 0.0,)
        vals['Max'] = bh + (1.0,)
        for v, val in vals.items():
            L.set_material_instance_vector_parameter_value(mi, f'Z{i}{v}', u.LinearColor(*[float(x) for x in val]))
    L.update_material_instance(mi); E.save_loaded_asset(mi, False)
    return mi


def main():
    assert ED.load_level(LEVEL); yield 3
    master = u.load_asset(f'{MAT}/M_Env_HouseZones')
    for a in ACT.get_all_level_actors():
        if a.get_actor_label() in DRAW_OFF:
            c = a.get_component_by_class(u.StaticMeshComponent)
            if c and not DRY: c.set_visibility(False)       # collision (if any) stays
            R.setdefault('drawn_off', []).append(a.get_actor_label())
            continue
        if not isinstance(a, u.StaticMeshActor) or not a.get_actor_label().startswith(SHELL): continue
        if a.get_name() in LIB.HAND_MATERIALS: continue          # I set these by hand
        c = a.get_component_by_class(u.StaticMeshComponent)
        if not c or not c.static_mesh or not c.is_visible() or a.get_editor_property('hidden'): continue
        for i in range(c.get_num_materials()):
            lg = legacy_of(c.get_material(i))
            if not lg: continue
            key, legacy = lg
            hits = rooms_for(a, key)
            if not hits:
                if c.get_material(i) != legacy and not DRY:
                    c.set_material(i, legacy)       # no recipe reaches it any more: back to the house surface
                continue
            mi = shell_instance(key, legacy, hits, master)
            if mi and not DRY: c.set_material(i, mi)
            R['assigned'].append([a.get_actor_label(), i, key, hits])
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'room-finish.json').write_text(json.dumps(R, indent=1, default=str))
    u.log('ENV_ROOMS assigned=%d instances=%d errors=%s saved=%s' % (len(R['assigned']), len(R['instances']), R['errors'], R.get('saved')))
