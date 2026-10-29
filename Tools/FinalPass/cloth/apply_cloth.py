"""Draw the simulated cloth over the stiff sheets on Lvl_Tallow_Playable. Re-runnable; saves the map.

  run.sh headless apply_cloth.py apply-cloth              (TALLOW_ENV_DRY=1: report only, nothing saved)

Order: after the cellar pass (env-cellar/apply_cellar.py spawns ENV_B5_Airer) and import_cloth_meshes.py; then
map/rebuild_nav.py LAST as always (nothing here changes collision: see below).
Swaps follow apply_ground.py: the new piece is drawn at the old actor's place and scale, standing on the floor under
it, turned so its hem clears the walls beside it (build_cloth_blender.py measures the settled cloth); the old actor stops drawing and keeps its blocking collision, so navigation, Mim's routes and anything that
traces against it see exactly what they saw before. The new actors have no collision. The airer hangs from the ceiling
out of everyone's reach: its old actor is only hidden. Spawned actors are labelled CLOTH_* in the Env/Cloth folder and
tagged Env.Cloth. Report: <project>/Review/Results/apply-cloth.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import LEVEL

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DRY = os.environ.get('TALLOW_ENV_DRY') == '1'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem); ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
MESH = '/Game/FinalPass/Cloth/Meshes'
FOLDER, TAG = 'Env/Cloth', 'Env.Cloth'
R = {'errors': [], 'swapped': [], 'dry': DRY}
# old label: (new mesh, new label, stand on the floor under the old one (else keep its pivot), yaw added to the old one's)
SWAPS = {
    # the cellar hall's store, by the door: the tall cone and the bell become a dress form and an armchair under dust
    # sheets. The form stands in the corner with its back to the hall (turned 165: the sheet's wide side clears the east
    # wall by 11 cm and the south wall by 16, the next form by 17); the chair keeps its turn (its hem was settled
    # against both walls and both forms: 3 cm from the south wall, 0.1 cm from the front form's foot).
    'EV04_B3_DustSheetTall_00': ('SM_Cloth_SheetedForm', 'CLOTH_B3_SheetedForm', True, 165.0),
    'EV04_B3_DustSheetChair_00': ('SM_Cloth_SheetedChair', 'CLOTH_B3_SheetedChair', True, 0.0),
    # the upper landing's tall sheet (0.75 scale, like the old one): the same sheeted form, facing the landing, its hem
    # 10 cm clear of the wall beside it
    'EV04_U3_DustSheetTall_01': ('SM_Cloth_SheetedForm', 'CLOTH_U3_SheetedForm', True, 90.0),
    # the laundry airer (env-cellar): the same frame with the washing hung over its rails
    'ENV_B5_Airer': ('SM_Cloth_Airer', 'CLOTH_B5_Airer', False, 0.0),
}


def comp(a):
    return a.get_component_by_class(u.StaticMeshComponent)


def r(v): return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def main():
    assert ED.load_level(LEVEL), LEVEL
    yield 3
    by = {a.get_actor_label(): a for a in ACT.get_all_level_actors()}
    for old, (mesh, label, on_floor, dyaw) in SWAPS.items():
        a = by.get(old)
        if not a: R['errors'].append('missing ' + old); continue
        m = u.load_asset(f'{MESH}/{mesh}')
        if not m: R['errors'].append('no mesh ' + mesh); continue
        o, e = a.get_actor_bounds(False)
        loc = a.get_actor_location(); rot = a.get_actor_rotation(); scale = a.get_actor_scale3d()
        if on_floor: loc = u.Vector(loc.x, loc.y, o.z - e.z)            # the old piece's bottom is the floor
        n = by.get(label)
        if not DRY:
            if not n:
                n = ACT.spawn_actor_from_class(u.StaticMeshActor, loc, u.Rotator(yaw=rot.yaw + dyaw)); n.set_actor_label(label)
            c = comp(n); c.set_static_mesh(m); c.set_mobility(u.ComponentMobility.STATIC)
            n.set_actor_location_and_rotation(loc, u.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=rot.yaw + dyaw), False, True)
            n.set_actor_scale3d(scale); n.set_folder_path(FOLDER); n.tags = [u.Name(TAG)]
            c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); c.set_editor_property('can_ever_affect_navigation', False)
            comp(a).set_visibility(False)                      # its blocking collision stays
        row = {'old': old, 'new': label, 'mesh': mesh, 'at': r(loc), 'yaw': round(rot.yaw + dyaw, 1), 'scale': round(scale.x, 3),
               'old_min': r(o - e), 'old_max': r(o + e)}
        if n and not DRY:
            no, ne = n.get_actor_bounds(False); row.update(new_min=r(no - ne), new_max=r(no + ne))
        R['swapped'].append(row)
    yield 1
    if not DRY:
        R['saved'] = bool(ED.save_current_level())
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'apply-cloth.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_CLOTH swapped=%d errors=%s saved=%s' % (len(R['swapped']), R['errors'], R.get('saved')))
