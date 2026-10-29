"""Ground floor dressing beyond the shared finishes: pieces swapped for better ones of the same role and
size, and the few practical lights the rooms need. Re-runnable; saves the map.

  run.sh headless apply_ground.py ground                 (TALLOW_ENV_DRY=1: report only)

Swaps draw the new piece over the old actor and keep the old one as invisible blocking collision, so navigation, pickup
spots and Mim's routes stay exactly as they were. My hand-placed actors are never touched.
Report: <project>/Review/Results/apply-ground.json
"""
import json
import os
from pathlib import Path
import unreal as u
import env_library as LIB
from env_common import LEVEL, MESH, FOLDER, TAG

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
R = {'errors': [], 'swapped': [], 'spawned': [], 'lights': [], 'dry': DRY}
DIM_TAG, DIM = 'Tallow.CandleDim.Orig=', 0.75
AUTHORED = 'Tallow.AuthoredLight'
LOCKED = {'StaticMeshActor_24', 'StaticMeshActor_25', 'StaticMeshActor_467', 'TallowPatrolPoint_16'}
# old label: (new mesh, label of the new actor, yaw added to the old actor's, uniform scale)
SWAPS = {
    # the generated kitchen range: a worn 1950s enamel cooker, which is what a 1974 kitchen would have
    'EV05_G5_KitchenRange': (f'{MESH}/SM_Env_G5_Cooker', 'ENV_G5_Cooker', 0.0, 1.0),
}
# on top of another actor: label: (mesh, on actor, offset along its own axes (forward x, right y, up from its top), yaw)
ON_TOP = {
    'ENV_G4_StudentLamp': (f'{MESH}/SM_Env_G4_StudentLamp', 'EV05_G4_StudyDesk', (0.0, -18.0, 0.0), 0.0),
}
LIGHTS = {   # label: (attached to, offset (x, y, z up from its top), lumens, radius, colour)
    'ENV_G4_StudentLampLight': ('ENV_G4_StudentLamp', (0.0, 0.0, 40.0), 3.0, 380.0, (255, 206, 150)),
}


def comp(a):
    return a.get_component_by_class(u.StaticMeshComponent)


def spawn_mesh(label, mesh, loc, rot, scale, by):
    a = by.get(label)
    if not a and not DRY:
        a = ACT.spawn_actor_from_class(u.StaticMeshActor, loc, rot); a.set_actor_label(label)
    if a and not DRY:
        c = comp(a); c.set_static_mesh(u.load_asset(mesh)); c.set_mobility(u.ComponentMobility.STATIC)
        a.set_actor_location_and_rotation(loc, rot, False, True); a.set_actor_scale3d(u.Vector(scale, scale, scale))
        a.set_folder_path(FOLDER.replace('Parlour', 'Ground')); a.tags = [u.Name(TAG.replace('Parlour', 'Ground'))]
        c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    return a


def main():
    assert ED.load_level(LEVEL); yield 3
    by = {a.get_actor_label(): a for a in ACT.get_all_level_actors()}
    for old, (mesh, label, dyaw, scale) in SWAPS.items():
        a = by.get(old)
        if not a: R['errors'].append('missing ' + old); continue
        assert a.get_name() not in LOCKED, old
        r = a.get_actor_rotation()
        n = spawn_mesh(label, mesh, a.get_actor_location(), u.Rotator(0, 0, r.yaw + dyaw), scale, by)
        if not DRY: comp(a).set_visibility(False)        # its blocking collision stays
        R['swapped'].append({'old': old, 'new': label})
    by = {a.get_actor_label(): a for a in ACT.get_all_level_actors()}
    for label, (mesh, under, off, dyaw) in ON_TOP.items():
        base = by.get(under)
        if not base: R['errors'].append('missing ' + under); continue
        o, e = base.get_actor_bounds(False)
        f, rgt = base.get_actor_forward_vector(), base.get_actor_right_vector()
        loc = u.Vector(o.x, o.y, o.z + e.z + off[2]) + f * off[0] + rgt * off[1]
        spawn_mesh(label, mesh, loc, u.Rotator(0, 0, base.get_actor_rotation().yaw + dyaw), 1.0, by)
        R['spawned'].append({'label': label, 'on': under, 'at': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]})
    by = {a.get_actor_label(): a for a in ACT.get_all_level_actors()}
    for label, (host, off, lm, radius, col) in LIGHTS.items():
        h = by.get(host)
        if not h: continue
        o, e = h.get_actor_bounds(False)
        loc = u.Vector(o.x + off[0], o.y + off[1], o.z - e.z + off[2])
        a = by.get(label)
        if not a and not DRY:
            a = ACT.spawn_actor_from_class(u.PointLight, loc, u.Rotator(0, 0, 0)); a.set_actor_label(label)
        if not a or DRY: continue
        a.set_actor_location(loc, False, True); a.set_folder_path(FOLDER.replace('Parlour', 'Ground'))
        lc = a.get_component_by_class(u.LocalLightComponent); lc.set_mobility(u.ComponentMobility.MOVABLE)
        lc.set_editor_property('intensity_units', u.LightUnits.LUMENS); lc.set_editor_property('intensity', lm)
        lc.set_editor_property('attenuation_radius', radius); lc.set_editor_property('source_radius', 3.0)
        lc.set_editor_property('light_color', u.Color(r=col[0], g=col[1], b=col[2], a=255)); lc.set_editor_property('cast_shadows', True)
        key = label + '/' + lc.get_name()
        # Tallow.AuthoredLight: ATallowHousePresence zeroes every light without it at begin play.
        a.tags = [u.Name(TAG.replace('Parlour', 'Ground')), u.Name(DIM_TAG + key + '=' + repr(lm / DIM)), u.Name(AUTHORED)]
        R['lights'].append({'label': label, 'lumens': lm, 'at': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]})
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'apply-ground.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_GROUND swapped=%d spawned=%d lights=%d errors=%s saved=%s' % (len(R['swapped']), len(R['spawned']), len(R['lights']), R['errors'], R.get('saved')))
