"""Render actual imported presentation assets in a separate editor.

TALLOW_REVIEW_KIND: player (production adapter), menu, intro, outro, attic, keeper.
Never opens gameplay maps. Use -RenderOffscreen, Python and SequencerScripting.
Launch with -ExecCmds="py <absolute script path>" so tick-driven captures can
complete; -ExecutePythonScript exits as soon as the initial script returns.
"""
import json
import os
import runpy
import time
import traceback
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
KIND = os.environ.get('TALLOW_REVIEW_KIND', 'player')
OUT = ROOT / 'Tallow/Saved/Presentation/Preview/Delivery' / KIND
OUT.mkdir(parents=True, exist_ok=True)
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
STATE = {'phase': 'setup', 'time': time.monotonic(), 'errors': [], 'observed': []}
MODES = {
    'player': ('/Game/Presentation/Maps/Lvl_PlayerContactReview_v3', None, ['Hold', 'Cup', 'Interact', 'Relight', 'Rest']),
    'menu': ('/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation', 'LS_MainMenu_Stillness', [0.1, 3, 6, 9]),
    'intro': ('/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation', 'LS_Intro_Wake', [0.1, 2, 4, 6.5, 7.8]),
    'outro': ('/Game/Presentation/Cinematics/Maps/Lvl_OutroPresentation', 'LS_Outro_LastFlame', [.1, 3, 6.8, 7.2, 8.5, 9.7]),
    'attic': ('/Game/Presentation/Environment/Maps/Lvl_AtticHeroRoomV02', None, [0]),
    'keeper': ('/Game/Presentation/Maps/Lvl_KeeperContactReview_v1', None, ['Idle', 'Reveal', 'JumpscareDeath', 'WallCrawl', 'CeilingCrawl']),
}

def finish(error=None):
    if error:
        STATE['errors'].append(error)
        u.log_error(error)
    if not error:
        for row in STATE['observed']:
            path = OUT / row['image']
            if not path.is_file() or path.stat().st_size < 100:
                STATE['errors'].append('Missing or empty capture: ' + str(path))
    result = {'kind': KIND, 'execution_complete': not STATE['errors'], 'errors': STATE['errors'], 'observed': STATE['observed'],
              'status': 'Rendered evidence; human visual review is a separate gate'}
    (OUT / 'result.json').write_text(json.dumps(result, indent=2))
    ED.editor_request_end_play()
    STATE.update(phase='finished', time=time.monotonic())

def manual_camera(camera):
    post = camera.camera_component.get_editor_property('post_process_settings')
    for name, value in [('override_auto_exposure_method', True), ('auto_exposure_method', u.AutoExposureMethod.AEM_MANUAL),
                        ('override_auto_exposure_apply_physical_camera_exposure', True), ('auto_exposure_apply_physical_camera_exposure', False),
                        ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0),
                        ('override_motion_blur_amount', True), ('motion_blur_amount', 0)]:
        post.set_editor_property(name, value)
    camera.camera_component.set_editor_property('post_process_settings', post)

def keeper_room(path):
    assert not u.EditorAssetLibrary.does_asset_exist(path), 'Keeper review exists; reuse it explicitly instead'
    assert ED.new_level(path)
    u.EditorLevelLibrary.get_editor_world().get_world_settings().set_editor_property('default_game_mode', u.GameModeBase)
    actor = ACTORS.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector())
    actor.set_actor_label('KeeperProductionReview')
    sc = actor.skeletal_mesh_component
    sc.set_skeletal_mesh_asset(u.load_asset('/Game/Presentation/KeeperGeneratedV06/SK_Keeper_Tripo_Direct'))
    sc.set_bounds_scale(3)
    sc.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    actor.set_actor_enable_collision(False)
    camera = ACTORS.spawn_actor_from_class(u.CameraActor, u.Vector(100, -450, 140), u.MathLibrary.find_look_at_rotation(u.Vector(100, -450, 140), u.Vector(0, 0, 115)))
    camera.camera_component.set_field_of_view(45)
    manual_camera(camera)
    for pos, power, color in [((80, -160, 90), 35, (1, .55, .28)), ((-100, 100, 220), 65, (.4, .55, 1))]:
        light = ACTORS.spawn_actor_from_class(u.PointLight, u.Vector(*pos))
        light.point_light_component.set_intensity(power)
        light.point_light_component.set_light_color(u.LinearColor(*color, 1))
    ED.save_current_level()

try:
    level, seqname, samples = MODES[KIND]
    if KIND == 'keeper':
        runpy.run_path(str(ROOT / 'Tools/KeeperProduction/repair_keeper_dependencies.py'), init_globals={'AUDIT_ONLY': True})
        mesh = u.load_asset('/Game/Presentation/KeeperGeneratedV06/SK_Keeper_Tripo_Direct')
        height = float(mesh.get_imported_bounds().box_extent.z) * 2
        assert 220 <= height <= 260, 'Keeper must be approximately 240cm: ' + str(height)
        morph_names = [target.get_name() for target in mesh.get_editor_property('morph_targets')]
        assert 'full_horror' in [name.lower() for name in morph_names], 'Keeper mantle gather morph is missing'
        clip_scales = {}
        for source in (ROOT / 'Tallow/SourceAssets/Keeper/SleevedV06/Export').glob('AN_Keeper_*.fbx'):
            clip = u.load_asset('/Game/Presentation/KeeperGeneratedV06/' + source.stem)
            data = clip.get_editor_property('asset_import_data')
            assert isinstance(data, u.FbxAnimSequenceImportData), 'Unexpected Keeper clip import metadata'
            clip_scales[source.stem] = data.get_editor_property('import_uniform_scale')
        (OUT / 'fresh-import-audit.json').write_text(json.dumps({'height_cm': height, 'morphs': morph_names, 'legacy_clip_import_scales': clip_scales}, indent=2))
        assert len(clip_scales) == 13 and all(abs(v - 100) < .001 for v in clip_scales.values()), 'Keeper clip unit mismatch: ' + repr(clip_scales)
    if KIND == 'keeper' and not u.EditorAssetLibrary.does_asset_exist(level):
        keeper_room(level)
    else:
        assert ED.load_level(level)
    if KIND == 'keeper':
        # Legacy FBX conversion faces +Y. Frame the front and include the full
        # 240cm silhouette instead of cropping the hood at the former size.
        cameras = [a for a in ACTORS.get_all_level_actors() if isinstance(a, u.CameraActor)]
        lights = [a for a in ACTORS.get_all_level_actors() if isinstance(a, u.PointLight)]
        assert len(cameras) == 1 and len(lights) == 2
        location = u.Vector(100, 650, 140)
        cameras[0].set_actor_location(location, False, False)
        cameras[0].set_actor_rotation(u.MathLibrary.find_look_at_rotation(location, u.Vector(0, 0, 115)), False)
        cameras[0].camera_component.set_field_of_view(50)
        for light, position, power, color in zip(lights, [(80, 160, 140), (-100, -100, 220)], [35, 65], [(1, .74, .54), (.4, .55, 1)]):
            light.set_actor_location(u.Vector(*position), False, False)
            light.point_light_component.set_intensity(power)
            light.point_light_component.set_light_color(u.LinearColor(*color, 1))
        assert ED.save_current_level()
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
        elif STATE['phase'] == 'warmup' and now - STATE['time'] > 12:
            if KIND == 'menu':
                u.SystemLibrary.execute_console_command(None, 'Tallow.Presentation.ExportUI')
            ED.editor_request_begin_play()
            STATE.update(phase='pie', time=now)
        elif STATE['phase'] == 'pie' and now - STATE['time'] > 7:
            world = u.EditorLevelLibrary.get_game_world()
            pc = u.GameplayStatics.get_player_controller(world, 0)
            pawn = u.GameplayStatics.get_player_pawn(world, 0)
            if pawn:
                pawn.set_actor_hidden_in_game(True)
            if KIND == 'player':
                for cls in [u.SkeletalMeshActor, u.TallowCandlePresentationActor]:
                    for actor in u.GameplayStatics.get_all_actors_of_class(world, cls):
                        actor.set_actor_hidden_in_game(True)
                for light in u.GameplayStatics.get_all_actors_of_class(world, u.PointLight):
                    light.point_light_component.set_intensity(light.point_light_component.intensity * .3)
                u.SystemLibrary.execute_console_command(pc, 'Tallow.Presentation.PlayerFixture')
                camera = pc.get_view_target()
                assert camera.get_actor_label() == 'PlayerProductionAdapterFixture'
                manual_camera(camera)
            elif seqname:
                seq = u.load_asset('/Game/Presentation/Cinematics/' + seqname)
                player, actor = u.LevelSequencePlayer.create_level_sequence_player(world, seq, u.MovieSceneSequencePlaybackSettings())
                STATE.update(player=player, sequence_actor=actor)
                if KIND == 'menu':
                    u.SystemLibrary.execute_console_command(pc, 'Tallow.Presentation.MenuFixture main')
            else:
                camera = u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)[0]
                pc.set_view_target_with_blend(camera, 0)
            STATE.update(world=world, pc=pc, phase='sample', i=0, time=now)
        elif STATE['phase'] == 'sample' and now - STATE['time'] > 2:
            if STATE['i'] >= len(samples):
                if KIND == 'outro' and not STATE.get('played_to_completion'):
                    STATE['player'].set_playback_position(u.MovieSceneSequencePlaybackParams(time=0, position_type=u.MovieScenePositionType.TIME))
                    STATE['player'].play()
                    STATE.update(phase='continuous', time=now, played_to_completion=True)
                    return
                finish()
                return
            sample = samples[STATE['i']]
            if KIND == 'player':
                u.SystemLibrary.execute_console_command(STATE['pc'], 'Tallow.Presentation.PlayerPose ' + sample)
                flame = STATE['pc'].get_view_target().get_component_by_class(u.TallowFlameVisualComponent)
                flame.standalone_state = u.CandleState.CUPPED if sample == 'Cup' else u.CandleState.LIT
            elif seqname:
                params = u.MovieSceneSequencePlaybackParams(time=sample, position_type=u.MovieScenePositionType.TIME, update_method=u.UpdatePositionMethod.JUMP)
                STATE['player'].set_playback_position(params)
            elif KIND == 'keeper':
                actor = u.GameplayStatics.get_all_actors_of_class(STATE['world'], u.SkeletalMeshActor)[0]
                sc = actor.skeletal_mesh_component
                sc.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
                clip = u.load_asset('/Game/Presentation/KeeperGeneratedV06/AN_Keeper_' + sample)
                assert clip, sample
                sc.set_animation(clip)
                sc.set_position(.5, False)
                sc.set_play_rate(0)
                camera = STATE['pc'].get_view_target()
                crawling = sample in ('JumpscareDeath', 'WallCrawl', 'CeilingCrawl')
                location = u.Vector(100, 390, 100) if crawling else u.Vector(100, 650, 140)
                target = u.Vector(0, 10, 55) if crawling else u.Vector(0, 0, 115)
                camera.set_actor_location(location, False, False)
                camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(location, target), False)
            STATE.update(phase='capture', sample=sample, time=now)
        elif STATE['phase'] == 'continuous':
            if now - STATE['time'] > 2 and not STATE['player'].is_playing():
                STATE.update(phase='capture', sample='after-finish', time=now)
            elif now - STATE['time'] > 20:
                finish('Outro did not finish continuous playback within 20 seconds')
        elif STATE['phase'] == 'capture' and now - STATE['time'] > 2:
            filename = f'{STATE["i"]:02d}-{STATE["sample"]}.png'
            (OUT / filename).unlink(missing_ok=True)  # Never accept a stale previous capture.
            command = 'Tallow.Presentation.CaptureViewport ' if KIND == 'menu' else 'HighResShot 1280x720 filename='
            u.SystemLibrary.execute_console_command(STATE['pc'], command + str(OUT / filename))
            observation = {'sample': STATE['sample'], 'image': filename, 'view': STATE['pc'].get_view_target().get_actor_label()}
            if KIND == 'keeper':
                keeper = u.GameplayStatics.get_all_actors_of_class(STATE['world'], u.SkeletalMeshActor)[0]
                sc = keeper.skeletal_mesh_component
                observation['bounds'] = [str(v) for v in keeper.get_actor_bounds(False)]
                observation['bones'] = {str(sc.get_bone_name(i)): str(sc.get_socket_location(sc.get_bone_name(i))) for i in range(sc.get_num_bones())}
                observation['mesh'] = sc.get_skeletal_mesh_asset().get_path_name()
                observation['mantle_gather_curve'] = sc.get_anim_instance().get_curve_value('full_horror')
            if seqname:
                observation['sequence_playing'] = STATE['player'].is_playing()
                observation['candles'] = [{ 'label': a.get_actor_label(), 'state': str(a.flame_state), 'draft': a.draft_exposure }
                                          for a in u.GameplayStatics.get_all_actors_of_class(STATE['world'], u.TallowCandlePresentationActor)]
            STATE.update(phase='await_capture', pending=observation, time=now)
        elif STATE['phase'] == 'await_capture':
            path = OUT / STATE['pending']['image']
            if path.is_file() and path.stat().st_size > 100:
                STATE['observed'].append(STATE['pending'])
                STATE.update(phase='sample', i=STATE['i'] + 1, time=now)
            elif now - STATE['time'] > 15:
                finish('Capture timed out: ' + str(path))
    except Exception:
        finish(traceback.format_exc())

HANDLE = u.register_slate_post_tick_callback(tick)
