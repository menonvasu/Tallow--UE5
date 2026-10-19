"""Isolated imported-player projection proof. Never opens or changes gameplay maps.

Run in a separate rendering editor with Python enabled. Each run writes a new
review map; TALLOW_PLAYER_REVIEW defaults to Lvl_PlayerContactReview_v1.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Tallow/Saved/Presentation/Preview/Player'
OUT.mkdir(parents=True, exist_ok=True)
STATE = {'phase': 'setup', 'time': time.monotonic(), 'errors': []}
ed = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)

def finish(error=None):
    if error:
        STATE['errors'].append(error)
        u.log_error(error)
    (OUT / 'render-result.json').write_text(json.dumps({
        'phase': 'failed' if STATE['errors'] else 'complete',
        'errors': STATE['errors'],
        'contract': 'World FOV70; FP FOV90; FP scale0.6; candle camera offset45,26,-30cm',
        'bones': STATE.get('bones', {}),
    }, indent=2))
    ed.editor_request_end_play()
    STATE.update(phase='finished', time=time.monotonic())

try:
    level = '/Game/Presentation/Maps/' + os.environ.get('TALLOW_PLAYER_REVIEW', 'Lvl_PlayerContactReview_v1')
    assert not u.EditorAssetLibrary.does_asset_exist(level), 'Choose a new review map name'
    assert ed.new_level(level)
    u.EditorLevelLibrary.get_editor_world().get_world_settings().set_editor_property('default_game_mode', u.GameModeBase)
    mesh = u.load_asset('/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Partial')
    assert mesh
    hands = actors.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector(20, 0, 0), u.Rotator(pitch=0, yaw=90, roll=0))
    hands.set_actor_label('PlayerHandsReview')
    hands.set_actor_enable_collision(False)
    sc = hands.skeletal_mesh_component
    sc.set_skeletal_mesh_asset(mesh)
    sc.set_bounds_scale(3)
    sc.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    sc.set_first_person_primitive_type(u.FirstPersonPrimitiveType.FIRST_PERSON)
    sc.override_animation_data(u.load_asset('/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_Hold'), False, False, 17 / 30, 1)
    candle = actors.spawn_actor_from_class(u.TallowCandlePresentationActor, u.Vector(45, 26, 80))
    candle.set_actor_label('PlayerCandleReview')
    stick = u.load_asset('/Game/Props/Candle/wooden_candlestick_1k')
    assert stick
    candle.candle.set_static_mesh(stick)
    candle.wick.set_relative_location(u.Vector(0, 0, stick.get_bounding_box().max.z), False, False)
    candle.flame.flame_mesh = u.load_asset('/Game/Presentation/Flame/SM_TallowFlame')
    candle.flame.flame_material = u.load_asset('/Game/Presentation/Flame/M_TallowFlame')
    candle.draft_exposure = .7
    candle.flame.initialize_visual()
    for component in candle.get_components_by_class(u.StaticMeshComponent):
        component.set_first_person_primitive_type(u.FirstPersonPrimitiveType.FIRST_PERSON)
    camera = actors.spawn_actor_from_class(u.CameraActor, u.Vector(0, 0, 110))
    camera.set_actor_label('PlayerContactCamera')
    cc = camera.camera_component
    cc.set_field_of_view(70)
    cc.set_editor_property('enable_first_person_field_of_view', True)
    cc.set_editor_property('first_person_field_of_view', 90)
    cc.set_editor_property('enable_first_person_scale', True)
    cc.set_editor_property('first_person_scale', .6)
    post = u.PostProcessSettings()
    for name, value in [
        ('override_auto_exposure_method', True), ('auto_exposure_method', u.AutoExposureMethod.AEM_MANUAL),
        ('override_auto_exposure_apply_physical_camera_exposure', True), ('auto_exposure_apply_physical_camera_exposure', False),
        ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0),
        ('override_motion_blur_amount', True), ('motion_blur_amount', 0),
        ('override_bloom_intensity', True), ('bloom_intensity', .15),
    ]:
        post.set_editor_property(name, value)
    cc.set_editor_property('post_process_settings', post)
    for location, intensity, color in [((-10, 0, 145), 15, (1, .86, .65)), ((50, -55, 130), 6, (.55, .7, 1))]:
        light = actors.spawn_actor_from_class(u.PointLight, u.Vector(*location))
        light.point_light_component.set_intensity(intensity)
        light.point_light_component.set_light_color(u.LinearColor(*color, 1))
    ed.save_current_level()
    STATE.update(phase='warmup', time=time.monotonic())
except Exception:
    finish(traceback.format_exc())

def tick(dt):
    try:
        now = time.monotonic()
        if STATE['phase'] == 'finished':
            if now - STATE['time'] > 3:
                u.unregister_slate_post_tick_callback(HANDLE)
                u.SystemLibrary.quit_editor()
        elif STATE['phase'] == 'warmup' and now - STATE['time'] > 15:
            u.SystemLibrary.execute_console_command(None, 'Tallow.Presentation.ExportUI')
            ed.editor_request_begin_play()
            STATE.update(phase='pie', time=now)
        elif STATE['phase'] == 'pie' and now - STATE['time'] > 8:
            world = u.EditorLevelLibrary.get_game_world()
            pc = u.GameplayStatics.get_player_controller(world, 0)
            camera = u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)[0]
            pc.set_view_target_with_blend(camera, 0)
            pawn = u.GameplayStatics.get_player_pawn(world, 0)
            if pawn:
                pawn.set_actor_hidden_in_game(True)
            hands = u.GameplayStatics.get_all_actors_of_class(world, u.SkeletalMeshActor)[0]
            candle = u.GameplayStatics.get_all_actors_of_class(world, u.TallowCandlePresentationActor)[0]
            for component in candle.get_components_by_class(u.StaticMeshComponent):
                component.set_first_person_primitive_type(u.FirstPersonPrimitiveType.FIRST_PERSON)
            STATE.update(pc=pc, hands=hands, candle=candle, phase='pose', i=0, time=now)
        elif STATE['phase'] == 'pose' and now - STATE['time'] > 3:
            names = ['Rest', 'Hold', 'Cup', 'Interact', 'Relight']
            if STATE['i'] >= len(names):
                finish()
                return
            name = names[STATE['i']]
            sc = STATE['hands'].skeletal_mesh_component
            # OverrideAnimationData serializes the initial pose but does not
            # reinitialize an already single-node instance when swapping clips.
            sc.set_animation(u.load_asset('/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_' + name))
            sc.set_position(17 / 30, False)
            sc.set_play_rate(0)
            STATE['candle'].flame_state = u.CandleState.CUPPED if name == 'Cup' else u.CandleState.LIT
            STATE.update(phase='capture', name=name, time=now)
        elif STATE['phase'] == 'capture' and now - STATE['time'] > 2:
            sc = STATE['hands'].skeletal_mesh_component
            locations = {str(sc.get_bone_name(i)): sc.get_socket_location(sc.get_bone_name(i)) for i in range(sc.get_num_bones())}
            STATE.setdefault('bones', {})[STATE['name']] = {n: [v.x, v.y, v.z] for n, v in locations.items()}
            u.SystemLibrary.execute_console_command(STATE['pc'], 'HighResShot 1280x720 filename=' + str(OUT / ('FP-' + STATE['name'] + '.png')))
            STATE.update(phase='pose', i=STATE['i'] + 1, time=now)
    except Exception:
        finish(traceback.format_exc())

HANDLE = u.register_slate_post_tick_callback(tick)
