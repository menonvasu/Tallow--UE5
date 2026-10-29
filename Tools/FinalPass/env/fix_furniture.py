"""Give furniture and props back their own materials, part by part. Re-runnable; saves the map.

  run.sh headless fix_furniture.py fix-furniture TALLOW_ENV_ROOMS=G       (prefix list: G, B, U, or 'G1,G2'; empty = all)
  TALLOW_ENV_DRY=1 reports only.

A slot is fixed when it renders a shared house surface (ArtDirection09 MI_A9_*_World), a flat 32 px swatch (the A9
prop instances) or the engine default. In order (env_library.py):
  1. a furnishing with a scanned instance for that slot (MI_Env_P_<Name>[_<slot>], built from its own import)
  2. the mesh's own imported material when that one has real maps
  3. the slot's name says what the part is made of (SLOT_WORDS) -> MI_Env_Lib_<surface>
Shell meshes (apply_room_finish.py), flames and my hand-set materials are never touched. No geometry moves.
Report: <project>/Review/Results/fix-furniture.json
"""
import json
import os
from pathlib import Path
import unreal as u
import env_library as LIB
from env_common import LEVEL, MAT

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ROOMS = [r.strip() for r in os.environ.get('TALLOW_ENV_ROOMS', '').split(',') if r.strip()]
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
E = u.EditorAssetLibrary
SHELL = ('HI_SM_HouseV01_', 'EV04_SM_HouseV04_', 'EV05_SM_HouseV05_', 'House_', 'TRIM_', 'StairHallWall', 'Basement_', 'Board',
         'FrontDoor_', 'A9_Parlour_Cornice', 'A9_Parlour_Hearth', 'ENV_G1_')
R = {'errors': [], 'fixed': [], 'left': [], 'dry': DRY, 'rooms': ROOMS}
CACHE = {}


def flagged(m):
    """Why a material is not a part's own: 'world', 'flat', 'default' or None."""
    if m is None: return 'default'
    k = m.get_path_name()
    if k in CACHE: return CACHE[k]
    why, cur, sizes = None, m, []
    while isinstance(cur, u.MaterialInstance):
        for x in cur.get_editor_property('texture_parameter_values'):
            if x.parameter_value: sizes.append(x.parameter_value.blueprint_get_size_x())
        cur = cur.get_editor_property('parent')
    base = cur.get_path_name() if cur else ''
    if '/Engine/' in base and ('WorldGrid' in base or 'DefaultMaterial' in base): why = 'default'
    elif m.get_name().startswith('M_HouseV04_') and m.get_name() not in LIB.KEEP_OWN: why = 'world'   # the preview's generic finishes
    elif base.endswith('M_A6_World') and '/ArtDirection09/' in base: why = 'world'
    elif sizes and max(sizes) <= 64: why = 'flat'
    CACHE[k] = why
    return why


def surface_for(slot_name):
    s = slot_name.lower().replace('_', '')
    for words, surface in LIB.SLOT_WORDS:
        if any(w in s for w in words):
            return surface
    return None


def asset(name):
    p = f'{MAT}/{name}'
    return u.load_asset(p) if E.does_asset_exist(p) else None


def main():
    assert ED.load_level(LEVEL); yield 3
    for a in ACT.get_all_level_actors():
        if not isinstance(a, u.StaticMeshActor): continue
        label = a.get_actor_label()
        if label.startswith(SHELL) or a.get_name() in LIB.HAND_MATERIALS: continue
        c = a.get_component_by_class(u.StaticMeshComponent)
        if not c or not c.static_mesh or not c.is_visible() or a.get_editor_property('hidden'): continue
        o, _ = a.get_actor_bounds(False)
        room = LIB.room_of(o.x, o.y, o.z) or 'outside'
        if ROOMS and not any(room.startswith(r) for r in ROOMS): continue
        mesh = c.static_mesh; own = mesh.static_materials
        name = mesh.get_name().replace('SM_Furnish_', '') if mesh.get_name().startswith('SM_Furnish_') else None
        for i in range(c.get_num_materials()):
            cur = c.get_material(i)
            why = flagged(cur)
            if not why or (cur and 'Flame' in cur.get_name()): continue
            slot = str(own[i].material_slot_name) if i < len(own) else str(i)
            pick, how = None, None
            if name:
                pick = asset(f'MI_Env_P_{name}_{i}') or (asset(f'MI_Env_P_{name}') if len(own) == 1 else None)
                how = 'scanned' if pick else None
            if not pick and i < len(own) and own[i].material_interface and not flagged(own[i].material_interface):
                pick, how = own[i].material_interface, 'own'
            if not pick:
                surf = surface_for(slot)
                pick = asset(f'MI_Env_Lib_{surf}') if surf else None
                how = f'surface {surf}' if pick else None
            row = {'label': label, 'room': room, 'mesh': mesh.get_name(), 'slot': slot, 'was': cur.get_name() if cur else None, 'why': why}
            if not pick:
                R['left'].append(row); continue
            row.update(now=pick.get_name(), how=how)
            if not DRY: c.set_material(i, pick)
            R['fixed'].append(row)
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'fix-furniture.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_FIX fixed=%d left=%d saved=%s' % (len(R['fixed']), len(R['left']), R.get('saved')))
