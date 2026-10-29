"""Cellar environment pass on Lvl_Tallow_Playable (B1 wake room .. B5 laundry): the pieces with a cellar-specific finish,
the chandler's things, the dress forms off the writing wall, and the cellar's lights. Re-runnable; saves the map.

  run.sh headless apply_cellar.py apply-cellar          (TALLOW_ENV_DRY=1: report only, nothing saved)

Order (ENV-RECIPE): build_env_materials.py, build_cellar_materials.py, import_cellar_meshes.py, apply_room_finish.py, then this, then
fix_furniture.py TALLOW_ENV_ROOMS=B, then map/rebuild_nav.py LAST (the dress forms move and carry collision).
Every change is by actor label and absolute (running twice gives the same map). My hand-placed actors are never moved
(their materials may be set: none of his edits were material edits except the two flues). Spawned actors are labelled
ENV_B*_ in the Env/Cellar folder and tagged Env.Cellar. The report lists every light in the cellar before and after.
Report: <project>/Review/Results/apply-cellar.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import LEVEL, MAT, MESH

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
R = {'errors': [], 'materials': [], 'moved': [], 'spawned': [], 'lights': [], 'lights_before': [], 'dry': DRY}
FOLDER, TAG = 'Env/Cellar', 'Env.Cellar'
CELLAR = ((-30.0, 1590.0), (-1330.0, 30.0), (-345.0, -20.0))
# my hand edits of 28 Oct (rooms/Review/Results/locked-actors.json): never moved here.
LOCKED = {
    'BP_TallowFusePickup_C_3', 'PlayerStart_0', 'StaticMeshActor_106', 'StaticMeshActor_107', 'StaticMeshActor_24',
    'StaticMeshActor_25', 'StaticMeshActor_350', 'StaticMeshActor_359', 'StaticMeshActor_367', 'StaticMeshActor_368',
    'StaticMeshActor_369', 'StaticMeshActor_382', 'StaticMeshActor_383', 'StaticMeshActor_390', 'StaticMeshActor_436',
    'StaticMeshActor_452', 'StaticMeshActor_467', 'StaticMeshActor_489', 'StaticMeshActor_543', 'StaticMeshActor_567',
    'StaticMeshActor_568', 'StaticMeshActor_619', 'StaticMeshActor_72', 'TallowHousePresence_37',
    'TallowHousePresence_47', 'TallowHousePresence_48', 'TallowHousePresence_49', 'TallowJournal_32',
    'TallowPatrolPoint_16', 'TallowProjector_0', 'TargetPoint_19'}
HAND_MATERIALS = {'StaticMeshActor_24', 'StaticMeshActor_25'}

# label (or label prefix ending in '*'): {slot: library surface, or a /Game path; None keeps the slot}
MATERIALS = {
    # brick: the arches of every cellar doorway laid as rowlock arches (build_cellar_materials.py), the flue breast in
    # the wake room and the bricked-up companion opening as plain coursed brick
    'EV04_SM_HouseV04_Portal_Basement': {0: f'{MAT}/MI_Env_B_ArchBrick'},
    'EV04_SM_HouseV04_Portal_Basement_BoilerWall': {0: f'{MAT}/MI_Env_B_ArchBrick'},
    'HI_SM_HouseV01_Flue_Basement': {0: 'CellarBrick'},
    'EV05_SM_HouseV05_CompanionInfill': {0: 'CellarBrick'},
    # the wake room's timber screens: lime-washed boards, not the plaster below the dado
    'HI_SM_HouseV01_WakeScreen': {0: 'ScreenBoards'},
    'HI_SM_HouseV01_WakeScreenDado': {0: 'ScreenBoards'},
    'HI_SM_HouseV01_WakeTimberScreen': {0: 'ScreenBoards'},
    'Board_basement_*': {0: 'WindowBoards'},
    'Basement_StairApproach_High': {0: 'StoneStep'},
    'Basement_StairApproach_Low': {0: 'StoneStep'},
    # generated meshes keep their shape but none of their generated pixels: black-leaded / japanned iron
    'EV05_B4_CoalBoiler': {0: 'Japanned'},
    'EV05_B2_SewingMachine': {0: 'Japanned'},
    # old greybox furniture whose own materials are flat colours
    'EV04_B4_CoalBunker_00': {0: 'WindowBoards', 1: 'Coal'},
    'EV05_B2_WorkBench': {0: 'BenchTop', 1: 'OldOak'},          # its own procedural paint read as a red-striped toy
    'EV04_B2_DressForm_00': {0: 'DustSheet', 1: 'Iron', 2: 'OldOak', 3: 'Brass'},
    'EV04_B2_DressForm_01': {0: 'DustSheet', 1: 'Iron', 2: 'OldOak', 3: 'Brass'},
    'EV04_B2_DressForm_02': {0: 'DustSheet', 1: 'Iron', 2: 'OldOak', 3: 'Brass'},
    'HI_UtilityCupboard': {0: 'CupboardPaint'},                 # its own chipped sage paint read as a toy
    'EV04_B2_SideCabinet_00': {0: 'OldOak', 1: 'Brass', 2: 'PaintedOlive'},
    'EV04_B3_DustSheetChair_00': {0: 'DustSheet'},
    'EV04_B3_DustSheetTall_00': {0: 'DustSheet'},
    'STORY_WakeAbsentFrames': {0: 'OldOak', 1: 'Brass'},
    'STORY_WakeEmptyPegs': {0: 'OldOak', 1: 'Brass'},
    'STORY_WorkshopFamily': {0: 'OldOak', 1: 'Brass'},
    'STORY_WakeRunner': {0: 'Runner'},
    'M3_ChairEscape': {0: 'OldOak'},
    'A6_027_CrawlBench': {0: 'OldOak', 1: 'WindowBoards'},
    # the cellar stair's boarded sides and sloped soffit (the grain of the joinery finish streaked down the slope)
    'HI_SM_HouseV01_BasementStairs': {0: 'StairBoards', 1: 'StairBoards'},
    'HI_SM_HouseV01_StairDetail_BasementStairs': {0: 'StairBoards', 1: 'StairBoards'},
}
# The chapter-one words land on the workshop's south wall behind the bench: the three dress forms that stood against
# it leave that wall. One stays, alone in the workshop's south-east corner turned towards the bench; two go to the
# cellar hall's store among the dust sheets. (The north wall has the arch to the wake room: nothing stands there.)
MOVES = {   # label: (location, yaw added to the original yaw or None to keep)
    'EV04_B2_DressForm_00': ((524.0, -1236.0, None), 54.0),       # (their fronts faced +y against the south wall)
    'EV04_B2_DressForm_01': ((700.0, -1136.0, None), -70.0),      # towards the hall's walk
    'EV04_B2_DressForm_02': ((812.0, -1238.0, None), 0.0),        # up the hall, at whoever comes down it
}
SPAWN = {   # label: (mesh, location, yaw)
    'ENV_B2_CandleRack': ('SM_Env_B_CandleRack', (250.0, -1040.0, -30.0), 0.0),        # over the bench, from the joists
    'ENV_B2_Mould': ('SM_Env_B_CandleMould', (140.0, -701.0, -183.6), 0.0),             # bookcase shelf at eye height
    'ENV_B2_Bundle': ('SM_Env_B_CandleBundle', (86.0, -699.0, -182.9), 4.0),            # (shelf tops -183.6 -217.7 -245.5)
    'ENV_B2_Spool': ('SM_Env_B_WickSpool', (138.0, -697.0, -217.7), 20.0),
    'ENV_B2_Cakes': ('SM_Env_B_TallowCakes', (117.0, -701.0, -245.5), 0.0),
    'ENV_B3_CandleBox': ('SM_Env_B_CandleBox', (952.0, -600.0, -172.0), 90.0),           # above the trunk with the matches
    'ENV_B4_CoalSpill': ('SM_Env_B_CoalSpill', (1112.0, -109.0, -330.0), 0.0),          # out of the bunker door
    'ENV_B5_Airer': ('SM_Env_B_Airer', (1500.0, -905.0, -30.0), 90.0),                  # over the wash tub, against the wall
}
# Lights (lumens). Practicals carry the house-candle dim tag (candles/dim_house_candles.py) at value / 0.75, as
# apply_parlour.py does, so a re-run of that script reproduces these values.
DIM_TAG, DIM = 'Tallow.CandleDim.Orig=', 0.75
AUTHORED = 'Tallow.AuthoredLight'
LIGHTS = {   # label: (location or None to keep, lumens, radius, source radius, colour, shadows, practical)
    'ENV_B2_LanternLight': ((334.0, -1018.0, -222.0), 4.0, 320.0, 2.0, (255, 196, 128), True, True),
    'ENV_B4_BoilerGlow': ((1438.0, -160.0, -296.0), 4.0, 280.0, 8.0, (255, 80, 30), True, False),
}
FLAMES = {'ENV_B2_LanternFlame': ((334.0, -1018.0, -226.0), 0.55)}      # flame: (location, scale), the parlour table flame
OFF = []         # unmotivated fill lights to switch off (none: every cellar fill was already at zero)
NO_SHADOW = ['EV04_B2_Lantern_01']   # the bench lantern's own body must not shadow the flame inside it


def actors():
    return {a.get_actor_label(): a for a in ACT.get_all_level_actors()}


def in_cellar(p):
    return all(lo <= v <= hi for v, (lo, hi) in zip((p.x, p.y, p.z), CELLAR))


def light_row(a):
    lc = a.get_component_by_class(u.LocalLightComponent) or a.get_component_by_class(u.LightComponent)
    if not lc: return None
    p = a.get_actor_location()
    row = {'label': a.get_actor_label(), 'class': a.get_class().get_name(), 'at': [round(p.x), round(p.y), round(p.z)],
           'intensity': round(lc.intensity, 3), 'visible': bool(lc.is_visible()) and not a.get_editor_property('hidden'), 'shadows': bool(lc.get_editor_property('cast_shadows')),
           'tags': [str(t) for t in a.tags]}
    try:
        row['units'] = str(lc.get_editor_property('intensity_units')).split('.')[-1]
        row['radius'] = round(lc.get_editor_property('attenuation_radius'))
        c = lc.get_editor_property('light_color'); row['colour'] = [c.r, c.g, c.b]
    except Exception:
        pass
    return row


def inventory(key):
    for a in ACT.get_all_level_actors():
        if not in_cellar(a.get_actor_location()): continue
        for lc in a.get_components_by_class(u.LightComponent):
            row = light_row(a)
            if row: R[key].append(dict(row, comp=lc.get_name())); break


def surface(name):
    return u.load_asset(name if name.startswith('/Game/') else f'{MAT}/MI_Env_Lib_{name}')


def mesh_comp(a):
    return a.get_component_by_class(u.StaticMeshComponent)


def apply_materials(by):
    for key, slots in MATERIALS.items():
        if key.endswith('*'):     # a family of actors: only the cellar's own (shell meshes sit at the origin, so by bounds)
            hits = [a for l, a in by.items() if l.startswith(key[:-1]) and in_cellar(a.get_actor_bounds(False)[0])]
        else:
            hits = [by[key]] if key in by else []
        if not hits: R['errors'].append('missing ' + key); continue
        for a in hits:
            if a.get_name() in HAND_MATERIALS: R['errors'].append('hand material ' + key); continue
            c = mesh_comp(a)
            for i, name in slots.items():
                if name is None or i >= c.get_num_materials(): continue
                mi = surface(name)
                if not mi: R['errors'].append('no material ' + name); continue
                if not DRY: c.set_material(i, mi)
                R['materials'].append([a.get_actor_label(), i, mi.get_name()])


def main():
    assert ED.load_level(LEVEL), LEVEL
    yield 3
    inventory('lights_before')
    by = actors()
    apply_materials(by)
    for label, (loc, dyaw) in MOVES.items():
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        assert a.get_name() not in LOCKED, label
        before = a.get_actor_location(); rot = a.get_actor_rotation()
        tag = 'Env.Cellar.Yaw0='
        old = [str(t) for t in a.tags if str(t).startswith(tag)]
        yaw0 = float(old[0][len(tag):]) if old else rot.yaw          # the original yaw, so re-runs stay absolute
        to = u.Vector(loc[0], loc[1], before.z if loc[2] is None else loc[2])
        if not DRY:
            if not old: a.tags = list(a.tags) + [u.Name(tag + repr(round(rot.yaw, 3)))]
            a.set_actor_location_and_rotation(to, u.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=yaw0 + (dyaw or 0.0)), False, True)
        R['moved'].append({'label': label, 'from': [round(before.x, 1), round(before.y, 1), round(before.z, 1)],
                           'to': [round(to.x, 1), round(to.y, 1), round(to.z, 1)], 'yaw': round(yaw0 + (dyaw or 0.0), 1)})
    for label, (mesh, loc, yaw) in SPAWN.items():
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, u.Vector(*loc), u.Rotator(yaw=yaw)); a.set_actor_label(label)
        if not a: R['spawned'].append({'label': label, 'dry': True}); continue
        c = mesh_comp(a); c.set_static_mesh(u.load_asset(f'{MESH}/{mesh}')); c.set_mobility(u.ComponentMobility.STATIC)
        a.set_actor_location_and_rotation(u.Vector(*loc), u.Rotator(yaw=yaw), False, True)
        a.set_actor_scale3d(u.Vector(1, 1, 1)); a.set_folder_path(FOLDER); a.tags = [u.Name(TAG)]
        c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); c.set_editor_property('can_ever_affect_navigation', False)
        o, e = a.get_actor_bounds(False)
        R['spawned'].append({'label': label, 'min': [round(o.x - e.x, 1), round(o.y - e.y, 1), round(o.z - e.z, 1)],
                             'max': [round(o.x + e.x, 1), round(o.y + e.y, 1), round(o.z + e.z, 1)]})
    by = actors()
    src = mesh_comp(by['A9_Parlour_TableFlame']) if 'A9_Parlour_TableFlame' in by else None
    for label, (loc, scale) in FLAMES.items():
        a = by.get(label)
        if not a and not DRY and src:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, u.Vector(*loc), u.Rotator()); a.set_actor_label(label)
        if not a or not src: continue
        c = mesh_comp(a); c.set_static_mesh(src.static_mesh); c.set_mobility(u.ComponentMobility.MOVABLE)
        for i in range(src.get_num_materials()): c.set_material(i, src.get_material(i))
        a.set_actor_location(u.Vector(*loc), False, True); a.set_actor_scale3d(by['A9_Parlour_TableFlame'].get_actor_scale3d() * scale)
        a.set_folder_path(FOLDER); a.tags = [u.Name(TAG)]
        c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); c.set_cast_shadow(False)
        R['spawned'].append({'label': label, 'at': list(loc)})
    for label, (loc, lm, radius, src_r, col, shadows, practical) in LIGHTS.items():
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.PointLight, u.Vector(*loc), u.Rotator()); a.set_actor_label(label); a.set_folder_path(FOLDER)
        if not a: continue
        lc = a.get_component_by_class(u.LocalLightComponent)
        if not DRY:
            if loc: a.set_actor_location(u.Vector(*loc), False, True)
            lc.set_mobility(u.ComponentMobility.MOVABLE)
            lc.set_editor_property('intensity_units', u.LightUnits.LUMENS)
            lc.set_editor_property('intensity', lm); lc.set_editor_property('attenuation_radius', radius)
            if src_r is not None: lc.set_editor_property('source_radius', src_r)
            lc.set_editor_property('light_color', u.Color(r=col[0], g=col[1], b=col[2], a=255))
            lc.set_editor_property('cast_shadows', shadows)
            # Tallow.AuthoredLight: ATallowHousePresence zeroes every other light in the house at begin play.
            tags = [t for t in a.tags if str(t) not in (TAG, AUTHORED) and not str(t).startswith(DIM_TAG)]
            if practical:
                tags.append(u.Name(DIM_TAG + label + '/' + lc.get_name() + '=' + repr(lm / DIM)))
            a.tags = tags + [u.Name(AUTHORED), u.Name(TAG)]
        R['lights'].append({'label': label, 'lumens': lm, 'radius': radius})
    for label in NO_SHADOW:
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        if not DRY: mesh_comp(a).set_cast_shadow(False)
        R['materials'].append([label, 'no shadow'])
    for label in OFF:
        a = by.get(label)
        if not a: R['errors'].append('missing light ' + label); continue
        lc = a.get_component_by_class(u.LightComponent)
        if not DRY: lc.set_visibility(False)
        R['lights'].append({'label': label, 'off': True})
    yield 1
    R['lights_after'] = []
    inventory('lights_after')
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'apply-cellar.json').write_text(json.dumps(R, indent=1, default=str))
    u.log('ENV_CELLAR materials=%d moved=%d spawned=%d lights=%d errors=%s saved=%s' % (
        len(R['materials']), len(R['moved']), len(R['spawned']), len(R['lights']), R['errors'], R.get('saved')))
