"""Parlour environment pass on Lvl_Tallow_Playable: finishes, furniture, dressing and lights. Re-runnable; saves the map.

  run.sh headless apply_parlour.py apply          (TALLOW_ENV_DRY=1: report only, nothing saved)

Needs build_env_materials.py first. Every change is by actor label and absolute (running twice gives the same map).
Only parlour actors change, plus material overrides on the shared shell meshes that run through the parlour; those
overrides use zone instances, which render exactly as before outside the parlour. Nothing I placed by hand is moved
(the locked list is checked). Spawned actors are labelled ENV_G1_* in the Env/Parlour folder.
Then run map/rebuild_nav.py (the chair swap keeps the same blocking collision, but the map is saved).
Report: <project>/Review/Results/apply-parlour.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import LEVEL, MAT, MESH, V04, FOLDER, TAG

HI = '/Game/Presentation/Environment/HouseIntegrationV01'

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
R = {'errors': [], 'materials': [], 'moved': [], 'hidden': [], 'shown': [], 'spawned': [], 'lights': [], 'dry': DRY}
# my hand edits of 28 Oct (rooms/diff_hand_edits.py): never moved or restyled here.
LOCKED = {'StaticMeshActor_24', 'StaticMeshActor_25', 'StaticMeshActor_467', 'TallowPatrolPoint_16'}

# Parlour-only pieces. The shell (floor, ceiling, walls, skirting, dado, joinery) is apply_room_finish.py's, and the
# ordinary furniture parts are fix_furniture.py's; these are the pieces with a parlour-specific finish.
MATERIALS = {   # label: one instance per slot (None keeps the slot)
    'A9_Parlour_Hearth': ['MI_Env_G1_Mantel', 'MI_Env_G1_HearthTiles', 'MI_Env_Lib_Iron'],
    # the house's own carved camelback sofa (Poly Haven sofa_03, its own scans) back in its place
    'HI_ParlourSofa': [f'{HI}/Materials/M_House_Sofa_0', f'{HI}/Materials/M_House_Sofa_1'],
    'EV04_G1_Armchair_00': ['MI_Env_P_Armchair'],
    'EV04_G1_GreenChair_00': ['MI_Env_P_GreenChair'],
    'EV04_G1_MantelClock_00': ['MI_Env_P_MantelClock_0', f'{V04}/Materials/M_V04F_MantelClock_1'],   # dial glass: its own
    'EV05_G1_UprightPiano': ['MI_Env_P_UprightPiano'],
    'EV04_G1_RugLarge_00': ['MI_Env_P_Rug'],
}
# The cartoon reading chair gives way to the carved green chair that already stands in the same place (its
# blocking collision was always the one navigation used).
HIDE = ['A9_Parlour_ReadingChair', 'A9_Parlour_Sofa', 'A9_Parlour_Curtains01', 'A9_Parlour_Curtains02']
# Drawn over by new meshes; the originals stay as invisible blocking collision so navigation does not change.
GHOST = ['HI_SM_HouseV01_Flue_Ground']
UNGHOST = ['EV05_G1_UprightPiano']           # an earlier pass drew a new piano over it; keep the house's piano
REMOVE = ['ENV_G1_Piano']
SHOW = ['EV04_G1_GreenChair_00', 'HI_ParlourSofa']   # both were the blocking collision all along
MOVES = {   # label: (location, yaw) absolute
    'EV04_G1_MantelClock_00': ((130.0, -325.0, 125.0), -90.0),     # forward on the shelf, clear of the overmantel mirror
    'EV04_G1_PaintingLandscape_00': ((574.9, -470.0, 140.0), 90.0),  # off the pier between the windows (the drapes stack there)
}
SCALES = {  # label: scale (the Star Ushak carpet is 274 x 167 cm; the rug mesh was 260 x 180)
    'EV04_G1_RugLarge_00': (1.06, 0.93, 1.0),
}
SPAWN = {   # label: (mesh, location, yaw, scale, materials)
    'ENV_G1_OvermantelMirror': (f'{V04}/Furniture/SM_Furnish_Mirror', (117.6, -325.0, 133.0), -90.0, 1.25, ['MI_Env_P_Mirror']),
    'ENV_G1_MantelCandleN': (f'{V04}/Furniture/SM_Furnish_Candlestick', (128.0, -262.0, 125.0), -90.0, 0.9, ['MI_Env_P_Candlestick']),
    'ENV_G1_MantelCandleS': (f'{V04}/Furniture/SM_Furnish_Candlestick', (128.0, -388.0, 125.0), -90.0, 0.9, ['MI_Env_P_Candlestick']),
    'ENV_G1_ChimneyBreast': (f'{MESH}/SM_Env_G1_ChimneyBreast', (0.0, 0.0, 0.0), 0.0, 1.0, []),
    'ENV_G1_FireGrate': (f'{MESH}/SM_Env_G1_FireGrate', (0.0, 0.0, 0.0), 0.0, 1.0, []),
    'ENV_G1_Coal': (f'{MESH}/SM_Env_G1_Coal', (0.0, 0.0, 0.0), 0.0, 1.0, []),
    'ENV_G1_FireTools': (f'{MESH}/SM_Env_G1_FireTools', (128.0, -220.0, 0.0), 0.0, 1.0, []),   # clear of the surround's foot
    'ENV_G1_Curtains01': (f'{MESH}/SM_Env_G1_Curtains', (185.0, -12.0, 0.0), 0.0, 1.0, []),
    'ENV_G1_Curtains02': (f'{MESH}/SM_Env_G1_Curtains', (400.0, -12.0, 0.0), 0.0, 1.0, []),
}
COLLIDE = {'ENV_G1_FireTools'}   # small enough to walk into: it blocks like any prop
# Lights (lumens). Practicals are written with the house-candle dim tag (candles/dim_house_candles.py) set to value / 0.75,
# so a later re-run of that script at its 0.75 factor reproduces these values instead of the old ones.
DIM_TAG, DIM = 'Tallow.CandleDim.Orig=', 0.75
AUTHORED = 'Tallow.AuthoredLight'
LIGHTS = {   # label: (location or None to keep, lumens, radius, source radius, colour, shadows, practical)
    'A9_Parlour_TableCandle': (None, 5.0, 420.0, 2.5, (255, 214, 160), True, True),
    'A9_Parlour_OilLamp': (None, 3.5, 380.0, 3.0, (255, 205, 150), True, True),
    'A9_Parlour_Window': (None, 18.0, 700.0, None, (176, 196, 214), True, False),          # moonlight through the boards
    'A9_Parlour_WindowReturn': (None, 10.0, 620.0, None, (176, 196, 214), True, False),
    'ENV_G1_MantelCandleLightN': ((131.0, -262.0, 150.0), 2.2, 320.0, 1.5, (255, 212, 158), True, True),
    'ENV_G1_MantelCandleLightS': ((131.0, -388.0, 150.0), 2.2, 320.0, 1.5, (255, 212, 158), True, True),
    'ENV_G1_EmberGlow': ((106.0, -325.0, 24.0), 1.5, 170.0, 6.0, (255, 92, 36), True, False),
}
FLAMES = {'ENV_G1_MantelFlameN': 'ENV_G1_MantelCandleN', 'ENV_G1_MantelFlameS': 'ENV_G1_MantelCandleS'}   # flame: candlestick


def actors():
    return {a.get_actor_label(): a for a in ACT.get_all_level_actors()}


def mesh_comp(a):
    return a.get_component_by_class(u.StaticMeshComponent)


def apply_materials(by):
    for label, slots in MATERIALS.items():
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        if a.get_name() in LOCKED: R['errors'].append('locked ' + label); continue
        c = mesh_comp(a)
        for i, name in enumerate(slots):
            if name is None or i >= c.get_num_materials(): continue
            mi = u.load_asset(name if name.startswith('/Game/') else f'{MAT}/{name}')
            if not mi: R['errors'].append('no material ' + name); continue
            if not DRY: c.set_material(i, mi)
            R['materials'].append([label, i, name])


def set_visible(a, on):
    c = mesh_comp(a)
    a.set_actor_hidden_in_game(not on)
    c.set_visibility(on)
    c.set_editor_property('hidden_in_game', not on)


def main():
    assert ED.load_level(LEVEL), LEVEL
    yield 3
    by = actors()
    apply_materials(by)
    for label in HIDE:
        a = by[label]
        if not DRY: set_visible(a, False); a.set_actor_enable_collision(False)
        R['hidden'].append(label)
    for label in GHOST:
        a = by[label]
        assert a.get_name() not in LOCKED, label
        if not DRY: mesh_comp(a).set_visibility(False)
        R['hidden'].append(label + ' (drawing only)')
    for label in UNGHOST:
        if not DRY: mesh_comp(by[label]).set_visibility(True)
        R['shown'].append(label)
    for label in REMOVE:
        if by.get(label) and not DRY: ACT.destroy_actor(by[label])
        R['hidden'].append(label + ' (removed)')
    for label in SHOW:
        a = by[label]
        if not DRY: set_visible(a, True)
        R['shown'].append(label)
    for label, (loc, yaw) in MOVES.items():
        a = by[label]
        assert a.get_name() not in LOCKED, label
        before = a.get_actor_location()
        if not DRY: a.set_actor_location_and_rotation(u.Vector(*loc), u.Rotator(0, 0, yaw), False, True)
        R['moved'].append({'label': label, 'from': [round(before.x, 1), round(before.y, 1), round(before.z, 1)], 'to': list(loc), 'yaw': yaw})
    for label, sc in SCALES.items():
        if not DRY: by[label].set_actor_scale3d(u.Vector(*sc))
        R['moved'].append({'label': label, 'scale': list(sc)})
    for label, (mesh, loc, yaw, scale, mats) in SPAWN.items():
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, u.Vector(*loc), u.Rotator(0, 0, yaw))
            a.set_actor_label(label)
        if a:
            c = mesh_comp(a); c.set_static_mesh(u.load_asset(mesh)); c.set_mobility(u.ComponentMobility.STATIC)
            a.set_actor_location_and_rotation(u.Vector(*loc), u.Rotator(0, 0, yaw), False, True)
            a.set_actor_scale3d(u.Vector(scale, scale, scale)); a.set_folder_path(FOLDER)
            a.tags = [u.Name(TAG)]
            c.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS if label in COLLIDE else u.CollisionEnabled.NO_COLLISION)
            for i, name in enumerate(mats): c.set_material(i, u.load_asset(f'{MAT}/{name}'))
            o, e = a.get_actor_bounds(False)
            R['spawned'].append({'label': label, 'min': [round(o.x - e.x, 1), round(o.y - e.y, 1), round(o.z - e.z, 1)],
                                 'max': [round(o.x + e.x, 1), round(o.y + e.y, 1), round(o.z + e.z, 1)]})
    by = actors()
    tf, tb = by['A9_Parlour_TableFlame'], by['A9_Parlour_TableCandleBody']
    o, e = tb.get_actor_bounds(False); lift = tf.get_actor_location().z - (o.z + e.z)
    for label, stick in FLAMES.items():
        o, e = by[stick].get_actor_bounds(False)
        loc = u.Vector(by[stick].get_actor_location().x, by[stick].get_actor_location().y, o.z + e.z + lift)
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, loc, u.Rotator(0, 0, 0)); a.set_actor_label(label)
        if a:
            src = mesh_comp(tf); c = mesh_comp(a)
            c.set_static_mesh(src.static_mesh); c.set_mobility(u.ComponentMobility.MOVABLE)
            for i in range(src.get_num_materials()): c.set_material(i, src.get_material(i))
            a.set_actor_location(loc, False, True); a.set_actor_scale3d(tf.get_actor_scale3d()); a.set_folder_path(FOLDER)
            a.tags = [u.Name(TAG)]; c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); c.set_cast_shadow(False)
            R['spawned'].append({'label': label, 'at': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]})
    for label, (loc, lm, radius, src_r, col, shadows, practical) in LIGHTS.items():
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.PointLight, u.Vector(*loc), u.Rotator(0, 0, 0)); a.set_actor_label(label)
            a.set_folder_path(FOLDER)
        if not a: continue
        lc = a.get_component_by_class(u.LocalLightComponent)
        before = round(lc.intensity, 3)
        if not DRY:
            if loc: a.set_actor_location(u.Vector(*loc), False, True)
            lc.set_mobility(u.ComponentMobility.MOVABLE)
            lc.set_editor_property('intensity_units', u.LightUnits.LUMENS)
            lc.set_editor_property('intensity', lm); lc.set_editor_property('attenuation_radius', radius)
            if src_r is not None: lc.set_editor_property('source_radius', src_r)
            lc.set_editor_property('light_color', u.Color(r=col[0], g=col[1], b=col[2], a=255))
            lc.set_editor_property('cast_shadows', shadows)
            if practical:
                key = label + '/' + lc.get_name()
                a.tags = [t for t in a.tags if not str(t).startswith(DIM_TAG + key + '=')] + [u.Name(DIM_TAG + key + '=' + repr(lm / DIM))]
            if label.startswith('ENV_G1_') and u.Name(TAG) not in a.tags:
                a.tags = list(a.tags) + [u.Name(TAG)]
            # ATallowHousePresence zeroes every light without this tag at begin play.
            if u.Name(AUTHORED) not in a.tags:
                a.tags = list(a.tags) + [u.Name(AUTHORED)]
        R['lights'].append({'label': label, 'before': before, 'lumens': lm, 'radius': radius})
    # Rug UVs (so the carpet photograph maps once across it).
    rug = u.load_asset('/Game/Presentation/Environment/HouseV04Preview/Meshes/SM_HouseV04_RugLarge') or mesh_comp(by['EV04_G1_RugLarge_00']).static_mesh
    sec = u.ProceduralMeshLibrary.get_section_from_static_mesh(rug, 0, 0)
    uvs = sec[3]
    R['rug_uv'] = [round(min(t.x for t in uvs), 3), round(max(t.x for t in uvs), 3), round(min(t.y for t in uvs), 3), round(max(t.y for t in uvs), 3)]
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'apply-parlour.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_APPLY materials=%d spawned=%d errors=%s saved=%s' % (len(R['materials']), len(R['spawned']), R['errors'], R.get('saved')))
