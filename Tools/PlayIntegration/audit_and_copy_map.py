"""Inspect the current game map and create an isolated integration copy once."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Tallow/Saved/Integration'
OUT.mkdir(parents=True, exist_ok=True)
SOURCE = '/Game/Maps/Lvl_Tallow_M3_Greybox'
DEST = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)

def vec(value):
    return [round(value.x, 4), round(value.y, 4), round(value.z, 4)]

def props(obj, names):
    result = {}
    for name in names:
        try:
            v = obj.get_editor_property(name)
            result[name] = v.get_path_name() if isinstance(v, u.Object) else str(v)
        except Exception:
            pass
    return result

assert levels.load_level(SOURCE)
result = {'source': SOURCE, 'destination': DEST, 'actors': []}
for a in actors.get_all_level_actors():
    origin, extent = a.get_actor_bounds(False)
    r = a.get_actor_rotation()
    item = {'name': a.get_name(), 'label': a.get_actor_label(), 'class': a.get_class().get_path_name(),
            'location': vec(a.get_actor_location()), 'rotation': [r.pitch, r.yaw, r.roll],
            'scale': vec(a.get_actor_scale3d()), 'bounds_origin': vec(origin), 'bounds_extent': vec(extent),
            'tags': [str(t) for t in a.tags], 'components': []}
    item['properties'] = props(a, ['required_fuses', 'current_fuses', 'target_door', 'b_auto_tie_player',
                                   'spawn_point', 'default_game_mode', 'default_pawn_class', 'player_controller_class'])
    for c in a.get_components_by_class(u.SceneComponent):
        cr = c.get_world_rotation()
        component = {'name': c.get_name(), 'class': c.get_class().get_name(), 'location': vec(c.get_world_location()),
                     'rotation': [cr.pitch, cr.yaw, cr.roll], 'scale': vec(c.get_world_scale())}
        component.update(props(c, ['static_mesh', 'skeletal_mesh_asset', 'capsule_radius', 'capsule_half_height',
                                    'box_extent', 'collision_enabled', 'collision_profile_name', 'intensity',
                                    'attenuation_radius', 'light_color']))
        if isinstance(c, u.StaticMeshComponent) and c.static_mesh:
            b = c.static_mesh.get_bounding_box()
            component['mesh_min'] = vec(b.min)
            component['mesh_max'] = vec(b.max)
        item['components'].append(component)
    result['actors'].append(item)

for path in ['/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter', '/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode']:
    bp = u.EditorAssetLibrary.load_blueprint_class(path)
    if bp:
        default = u.get_default_object(bp)
        info = props(default, ['default_pawn_class', 'player_controller_class', 'run_speed', 'walkspeed'])
        for c in default.get_components_by_class(u.ActorComponent):
            if isinstance(c, u.CharacterMovementComponent):
                info['movement'] = props(c, ['max_step_height', 'walkable_floor_angle', 'max_walk_speed', 'nav_agent_props'])
            if isinstance(c, u.CapsuleComponent):
                info['capsule'] = props(c, ['capsule_radius', 'capsule_half_height'])
        result[path] = info

(OUT / 'source-map-audit.json').write_text(json.dumps(result, indent=2))
if not u.EditorAssetLibrary.does_asset_exist(DEST):
    assert u.EditorAssetLibrary.duplicate_asset(SOURCE, DEST), 'Map duplication failed'
    assert u.EditorAssetLibrary.save_asset(DEST), 'Map copy did not save'
    result['copied'] = True
else:
    result['copied'] = False
u.log('TALLOW_MAP_AUDIT ' + json.dumps({'actors': len(result['actors']), 'destination': DEST, 'copied': result['copied']}))
