"""Upstairs environment pass on Lvl_Tallow_Playable: the props fix_furniture.py cannot reach, lit candles, sconces,
the night behind the front windows' boards and the few moves that compose the rooms. Re-runnable; saves the map.

  run.sh headless apply_upper.py apply-upper          (TALLOW_ENV_DRY=1: report only, nothing saved)

Run after build_env_materials.py, import_upper_meshes.py, apply_room_finish.py and fix_furniture.py (TALLOW_ENV_ROOMS=U);
then map/rebuild_nav.py. Every change is by actor label and absolute (running twice gives the same map).
1. Part materials of the upstairs actors that are not plain static mesh actors (sconces, the toy train, the nursery
   mobile, the telephone receiver, the hanging drawings): the same rule as fix_furniture.py, slot by slot (the mesh's
   own imported material when it has real maps, else the library surface its slot name names). Then the few own
   materials that look wrong in the rooms get a library surface by hand (OVERRIDE): the sage-and-rust cabinets, the
   teal dust sheet, the near-black rugs, and the pale stretched "world walnut" on the upstairs door and gate leaves
   (their frames and leaves only; the doors, locks and power gates work as before). The projector and the release
   latch are left alone.
2. Light (lumens), only where a family would have put a flame: the five sconces and the back-stair sconce, the
   landing candle on the hall console, Maisie's candle in the collection (Nell's stays unlit). Every one is tagged
   Tallow.AuthoredLight (the house darkness rule keeps it) and carries the house-candle dim tag, so
   candles/dim_house_candles.py at 0.75 reproduces these values. The street side is the moon side: behind the boards
   of the front windows a dim cold glow (unlit, casts nothing) shows only through the gaps, so the boards stay black
   silhouettes against slits of night. (A rect light behind the boards was tried: the boards transmit it and glow.)
3. Composition: the toy elephant sits on a chair at the toy room's tea table (measured onto the seat); the 1970s
   records on the toy room floor go (a period slip, and one thing fewer); the generated rocking horse gives way to a
   plank-cut one of the same length (REPLACE).
Nothing I placed by hand moves (rooms/Review/Results/locked-actors.json), and nothing a pickup spot, hide volume,
crawl link, Mim marker or story object stands on moves.
Report: <project>/Review/Results/apply-upper.json
"""
import json
import os
from pathlib import Path
import unreal as u
import env_library as LIB
from env_common import LEVEL, MAT

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
E = u.EditorAssetLibrary
R = {'errors': [], 'materials': [], 'moved': [], 'hidden': [], 'spawned': [], 'lights': [], 'dry': DRY}
LOCKED = set(json.loads((Path(__file__).resolve().parent / 'locked-actors.json').read_text())['locked'])   # actors I placed by hand
FOLDER, TAG = 'Env/Upper', 'Env.Upper'
AUTHORED = 'Tallow.AuthoredLight'
DIM_TAG, DIM = 'Tallow.CandleDim.Orig=', 0.75
KEEP_OUT = ('FINAL_', 'Collection_', 'DOOR_', 'Gate', 'Projector')        # never restyled here

# Sconces (house presence candle practicals): lumens, attenuation radius, source radius. Their runtime flicker drives
# the light from PracticalLumens, so that is what is written (with the dim tag on it).
SCONCES = {
    'A6_011_CandleSconce': (2.2, 340.0, 1.5),     # toy room, west wall by the crawl bench
    'A6_012_CandleSconce': (1.8, 320.0, 1.5),     # collection, beside the door from the back landing
    'A6_013_CandleSconce': (2.0, 340.0, 1.5),     # landing, beside the bedroom door
    'A6_014_CandleSconce': (2.0, 340.0, 1.5),     # bedroom, east wall
    'A6_015_CandleSconce': (1.6, 300.0, 1.5),     # nursery, east wall
    'A6_009_CandleSconce': (1.2, 260.0, 1.5),     # the back stairs coming up into the nursery
}
# Flames on candles that already stand in the rooms: (flame label, candle label, light label, lumens, radius, flame size
# against the parlour's table candle: Maisie's is a thick pillar)
FLAMES = [
    ('ENV_U3_ConsoleFlame', 'EV04_U3_Candlestick_00', 'ENV_U3_ConsoleCandleLight', 1.8, 330.0, 1.0),
    ('ENV_U2_MaisieFlame', 'A6_041_CandleMaisieLabel', 'ENV_U2_MaisieCandleLight', 1.2, 260.0, 1.6),
]
FLAME_SRC = ('A9_Parlour_TableFlame', 'A9_Parlour_TableCandleBody')     # the house's practical flame and its candle
# The night behind the front windows' boards: an unlit two-sided plane 6 cm behind the boards, the size of the opening.
# label: (boards it sits behind, prefix of their labels), size (width, height) cm. The plane is centred on the boards.
GLOW = {
    'ENV_U1_NightFront60': ('Board_upper_front_60_', (112.0, 290.0)),
    'ENV_U1_NightFront200': ('Board_upper_front_200_', (112.0, 175.0)),
    'ENV_U1_NightFront390': ('Board_upper_front_390_', (112.0, 175.0)),
    'ENV_U3_NightFront780': ('Board_upper_front_780_', (112.0, 175.0)),
}
GLOW_MAT = '/Game/FinalPass/Env/Upper/Materials/MI_Env_Upper_NightGlow'
PLANE = '/Engine/BasicShapes/Plane'
REMOVE = ['ENV_U1_MoonFront', 'ENV_U4_MoonEast', 'ENV_U5_MoonBack']     # an earlier try (rect lights behind the boards)
# Own materials that look wrong in these rooms -> a library surface, every slot (label: surface).
OVERRIDE = {
    'HI_ToyShelfCabinet': 'PaintedOlive',                   # sage paint with bright orange rust: a nursery cupboard, olive
    'HI_CollectionCabinet0': 'UpperCabinet', 'HI_CollectionCabinet1': 'UpperCabinet',   # the collection's cabinets:
    'HI_CollectionCabinet2': 'UpperCabinet', 'HI_CollectionCabinet3': 'UpperCabinet',   # kept, dark, waxed (env_rooms)
    'EV04_U3_DustSheetTall_01': 'DustSheet',                # its own linen is teal (cabinet 3 is only restyled)
    'EV04_U1_RugSmall_00': 'Velvet', 'EV04_U4_RugSmall_01': 'Velvet', 'EV04_U5_RugSmall_02': 'Velvet',   # own: near black
}
# Door and gate leaves and frames upstairs: the stretched pale world walnut -> the dark joinery wood (look only).
DOORS = ['DOOR_BedroomLocked', 'DOOR_BedroomLocked_Nursery', 'ToyRoomCollectionPowerGate', 'CollectionCrawlPowerGate',
         'FinalCollectionPowerGate']
HIDE = ['A6_026_VinylRecords']
SEAT = {'EV04_U1_ToyElephant_00': ('EV04_U1_NurseryChair_00', 200.0)}    # prop: (chair it sits on, yaw)
# Replacements drawn over the old actor, which stays as invisible blocking collision (navigation keeps its footprint):
# old label: (new label, mesh, yaw added to the old actor's yaw so the new one lies the same way)
REPLACE = {
    'EV05_U1_RockingHorse': ('ENV_U1_RockingHorse', '/Game/FinalPass/Env/Upper/Meshes/SM_Env_U1_RockingHorse', 90.0),
}


def actors():
    return {a.get_actor_label(): a for a in ACT.get_all_level_actors()}


def asset(path):
    return u.load_asset(path) if E.does_asset_exist(path) else None


FLAG = {}


def flagged(m):
    """Why a material is not a part's own: 'world', 'flat', 'default' or None (as fix_furniture.py)."""
    if m is None: return 'default'
    k = m.get_path_name()
    if k in FLAG: return FLAG[k]
    why, cur, sizes = None, m, []
    while isinstance(cur, u.MaterialInstance):
        for x in cur.get_editor_property('texture_parameter_values'):
            if x.parameter_value: sizes.append(x.parameter_value.blueprint_get_size_x())
        cur = cur.get_editor_property('parent')
    base = cur.get_path_name() if cur else ''
    if '/Engine/' in base and ('WorldGrid' in base or 'DefaultMaterial' in base): why = 'default'
    elif base.endswith('M_A6_World') and '/ArtDirection09/' in base: why = 'world'
    elif sizes and max(sizes) <= 64: why = 'flat'
    FLAG[k] = why
    return why


def surface_for(slot_name):
    s = slot_name.lower().replace('_', '')
    for words, surface in LIB.SLOT_WORDS:
        if any(w in s for w in words):
            return surface
    return None


def fix_parts(a):
    label = a.get_actor_label()
    for c in a.get_components_by_class(u.StaticMeshComponent):
        if not c.static_mesh or not c.is_visible(): continue
        own = c.static_mesh.static_materials
        for i in range(c.get_num_materials()):
            cur = c.get_material(i)
            why = flagged(cur)
            if not why or (cur and 'Flame' in cur.get_name()): continue
            slot = str(own[i].material_slot_name) if i < len(own) else str(i)
            pick, how = None, None
            if i < len(own) and own[i].material_interface and not flagged(own[i].material_interface):
                pick, how = own[i].material_interface, 'own'
            else:
                surf = surface_for(slot)
                pick = asset(f'{MAT}/MI_Env_Lib_{surf}') if surf else None
                how = f'surface {surf}' if pick else None
            if not pick:
                R['errors'].append(f'no fix for {label} {c.get_name()} slot {slot}'); continue
            if not DRY: c.set_material(i, pick)
            R['materials'].append([label, c.get_name(), slot, cur.get_name() if cur else None, pick.get_name(), how])


def tag_light(a, lc, lm, practical):
    tags = [t for t in a.tags if str(t) != AUTHORED and str(t) != TAG]
    if practical:
        key = a.get_actor_label() + '/' + lc.get_name()
        tags = [t for t in tags if not str(t).startswith(DIM_TAG + key + '=')] + [u.Name(DIM_TAG + key + '=' + repr(lm / DIM))]
    a.tags = tags + [u.Name(AUTHORED), u.Name(TAG)]


def point_light(by, label, loc, lm, radius, src, colour=(255, 212, 158)):
    a = by.get(label)
    if not a and not DRY:
        a = ACT.spawn_actor_from_class(u.PointLight, u.Vector(*loc), u.Rotator(0, 0, 0)); a.set_actor_label(label)
    if not a: return
    lc = a.get_component_by_class(u.LocalLightComponent)
    if not DRY:
        a.set_actor_location(u.Vector(*loc), False, True); a.set_folder_path(FOLDER)
        lc.set_mobility(u.ComponentMobility.MOVABLE)
        lc.set_editor_property('intensity_units', u.LightUnits.LUMENS)
        lc.set_editor_property('intensity', lm); lc.set_editor_property('attenuation_radius', radius)
        lc.set_editor_property('source_radius', src)
        lc.set_editor_property('light_color', u.Color(r=colour[0], g=colour[1], b=colour[2], a=255))
        lc.set_editor_property('cast_shadows', True)
        lc.set_editor_property('indirect_lighting_intensity', 0.0); lc.set_editor_property('volumetric_scattering_intensity', 0.0)
        tag_light(a, lc, lm, True)
    R['lights'].append({'label': label, 'at': [round(v, 1) for v in loc], 'lumens': lm, 'radius': radius})


def top_of(a):
    o, e = a.get_actor_bounds(False)
    return u.Vector(a.get_actor_location().x, a.get_actor_location().y, o.z + e.z)


def seat_height(chair):
    """Highest surface of the chair's own render mesh straight under its centre, below its top third (the seat)."""
    c = chair.get_component_by_class(u.StaticMeshComponent)
    o, e = chair.get_actor_bounds(False)
    loc = chair.get_actor_location()
    hit = u.SystemLibrary.line_trace_single(chair.get_world(), u.Vector(loc.x, loc.y, o.z + e.z * 0.33), u.Vector(loc.x, loc.y, o.z - e.z),
                                            u.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], u.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    if not t or not t[0] or t[9] != chair: return None
    return t[4].z


def main():
    assert ED.load_level(LEVEL), LEVEL
    u.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll'); yield 4
    by = actors()
    # 1. parts
    for label, a in sorted(by.items()):
        if isinstance(a, u.StaticMeshActor) or label.startswith(KEEP_OUT) or any(k in label for k in KEEP_OUT): continue
        if not isinstance(a, (u.TallowHousePresence,)): continue
        o, _ = a.get_actor_bounds(False)
        room = LIB.room_of(o.x, o.y, o.z) or ''
        if not room.startswith('U'): continue
        if a.get_name() in LOCKED: R['errors'].append('locked ' + label); continue
        fix_parts(a)
    # 2a. sconces
    for label, (lm, radius, src) in SCONCES.items():
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        lc = a.get_editor_property('practical')
        before = a.get_editor_property('practical_lumens')
        if not DRY:
            a.set_editor_property('practical_lumens', lm)
            lc.set_editor_property('attenuation_radius', radius); lc.set_editor_property('source_radius', src)
            key = label + '/' + lc.get_name()
            a.tags = [t for t in a.tags if not str(t).startswith(DIM_TAG + key + '=')] + [u.Name(DIM_TAG + key + '=' + repr(lm / DIM))]
        R['lights'].append({'label': label, 'before': round(before, 3), 'lumens': lm, 'radius': radius})
    # 2b. flames on the candles that stand in the rooms
    ref, ref_body = by[FLAME_SRC[0]], by[FLAME_SRC[1]]
    o, e = ref_body.get_actor_bounds(False); lift = ref.get_actor_location().z - (o.z + e.z)
    src = ref.get_component_by_class(u.StaticMeshComponent)
    for flame, candle, light, lm, radius, size in FLAMES:
        c = by.get(candle)
        if not c: R['errors'].append('missing ' + candle); continue
        at = top_of(c) + u.Vector(0, 0, lift)
        a = by.get(flame)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, at, u.Rotator(0, 0, 0)); a.set_actor_label(flame)
        if a and not DRY:
            m = a.get_component_by_class(u.StaticMeshComponent)
            m.set_static_mesh(src.static_mesh); m.set_mobility(u.ComponentMobility.MOVABLE)
            for i in range(src.get_num_materials()): m.set_material(i, src.get_material(i))
            a.set_actor_location(at, False, True); a.set_actor_scale3d(ref.get_actor_scale3d() * size); a.set_folder_path(FOLDER)
            a.tags = [u.Name(TAG)]; m.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); m.set_cast_shadow(False)
        R['spawned'].append({'label': flame, 'on': candle, 'at': [round(at.x, 1), round(at.y, 1), round(at.z, 1)]})
        point_light(by, light, (at.x, at.y, at.z + 3.0), lm, radius, 1.2)
    # 2c. the night behind the front windows' boards
    for label in REMOVE:
        if by.get(label):
            if not DRY: ACT.destroy_actor(by[label])
            R['hidden'].append(label + ' (removed)')
    glow = asset(GLOW_MAT); plane = asset(PLANE)
    for label, (prefix, (width, height)) in GLOW.items():
        boards = [a for l, a in by.items() if l.startswith(prefix)]
        if not boards: R['errors'].append('no boards ' + prefix); continue
        lo = [min(b.get_actor_bounds(False)[0].to_tuple()[i] - b.get_actor_bounds(False)[1].to_tuple()[i] for b in boards) for i in range(3)]
        hi = [max(b.get_actor_bounds(False)[0].to_tuple()[i] + b.get_actor_bounds(False)[1].to_tuple()[i] for b in boards) for i in range(3)]
        front = prefix.startswith('Board_upper_front')         # boards across y (front/back wall) or x (side walls)
        at = u.Vector((lo[0] + hi[0]) / 2, hi[1] + 6.0 if front else (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2)
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, at, u.Rotator(90, 0, 0)); a.set_actor_label(label)
        if a and not DRY:
            c = a.get_component_by_class(u.StaticMeshComponent)
            c.set_static_mesh(plane); c.set_material(0, glow); c.set_mobility(u.ComponentMobility.STATIC)
            a.set_actor_location_and_rotation(at, u.Rotator(90, 0, 0 if front else 90), False, True)
            a.set_actor_scale3d(u.Vector(width / 100.0, height / 100.0, 1.0)); a.set_folder_path(FOLDER); a.tags = [u.Name(TAG)]
            c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); c.set_cast_shadow(False)
            c.set_editor_property('affect_distance_field_lighting', False)
        if a:
            o, e = a.get_actor_bounds(False)
            R['spawned'].append({'label': label, 'behind': prefix, 'min': [round(o.x - e.x, 1), round(o.y - e.y, 1), round(o.z - e.z, 1)],
                                 'max': [round(o.x + e.x, 1), round(o.y + e.y, 1), round(o.z + e.z, 1)]})
    # 2d. own materials that look wrong here, and the door and gate leaves
    for label, surf in OVERRIDE.items():
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        mi = asset(f'{MAT}/MI_Env_Lib_{surf}')
        c = a.get_component_by_class(u.StaticMeshComponent)
        for i in range(c.get_num_materials()):
            if not DRY: c.set_material(i, mi)
            R['materials'].append([label, c.get_name(), i, 'override', mi.get_name()])
    dark = asset(f'{MAT}/MI_Env_Lib_DarkWood')
    for label in DOORS:
        a = by.get(label)
        if not a: R['errors'].append('missing ' + label); continue
        for c in a.get_components_by_class(u.StaticMeshComponent):
            for i in range(c.get_num_materials()):
                cur = c.get_material(i)
                if cur and cur.get_name() in ('MI_A9_Walnut_World', dark.get_name()):
                    if not DRY: c.set_material(i, dark)
                    R['materials'].append([label, c.get_name(), i, cur.get_name(), dark.get_name()])
    # 3. composition
    for label in HIDE:
        a = by.get(label)
        if not a: continue
        assert a.get_name() not in LOCKED, label
        if not DRY:
            a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
            a.get_component_by_class(u.StaticMeshComponent).set_visibility(False)
        R['hidden'].append(label)
    for old_label, (label, mesh, dyaw) in REPLACE.items():
        old = by.get(old_label)
        if not old: R['errors'].append('missing ' + old_label); continue
        assert old.get_name() not in LOCKED, old_label
        loc, yaw = old.get_actor_location(), old.get_actor_rotation().yaw + dyaw
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.StaticMeshActor, loc, u.Rotator(0, 0, yaw)); a.set_actor_label(label)
        if a and not DRY:
            c = a.get_component_by_class(u.StaticMeshComponent)
            c.set_static_mesh(u.load_asset(mesh)); c.set_mobility(u.ComponentMobility.STATIC)
            a.set_actor_location_and_rotation(loc, u.Rotator(0, 0, yaw), False, True); a.set_folder_path(FOLDER)
            a.tags = [u.Name(TAG)]
            c.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
            old.get_component_by_class(u.StaticMeshComponent).set_visibility(False)
        if a:
            o, e = a.get_actor_bounds(False)
            R['spawned'].append({'label': label, 'over': old_label, 'yaw': round(yaw, 1), 'min': [round(o.x - e.x, 1), round(o.y - e.y, 1), round(o.z - e.z, 1)],
                                 'max': [round(o.x + e.x, 1), round(o.y + e.y, 1), round(o.z + e.z, 1)]})
    for label, (chair_label, yaw) in SEAT.items():
        a, chair = by.get(label), by.get(chair_label)
        if not a or not chair: R['errors'].append('missing ' + label); continue
        assert a.get_name() not in LOCKED, label
        z = seat_height(chair)
        if z is None: R['errors'].append('no seat under ' + chair_label); continue
        before = a.get_actor_location()
        if not DRY:
            a.set_actor_location_and_rotation(u.Vector(chair.get_actor_location().x, chair.get_actor_location().y, before.z), u.Rotator(0, 0, yaw), False, True)
            o, e = a.get_actor_bounds(False)
            a.set_actor_location(u.Vector(a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z + (z + 0.05) - (o.z - e.z)), False, True)
        after = a.get_actor_location()
        R['moved'].append({'label': label, 'from': [round(before.x, 1), round(before.y, 1), round(before.z, 1)],
                           'to': [round(after.x, 1), round(after.y, 1), round(after.z, 1)], 'seat_z': round(z, 2), 'on': chair_label})
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'apply-upper.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_UPPER materials=%d lights=%d spawned=%d moved=%d errors=%s saved=%s' % (
        len(R['materials']), len(R['lights']), len(R['spawned']), len(R['moved']), R['errors'], R.get('saved')))
