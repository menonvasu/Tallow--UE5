"""Create isolated menu, wake-up and final-flame presentation sequences.

Run after HeroRoomV02 and SeatedBoundIdle are imported, with SequencerScripting
enabled. Creates new maps and assets only. Gameplay owns when these play, input
locking, skip handling and the transition after OnFinished.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
DEST = '/Game/Presentation/Cinematics'
E = u.EditorAssetLibrary
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
ASSETS = u.AssetToolsHelpers.get_asset_tools()

def sequence(name, frames):
    seq = ASSETS.create_asset(name, DEST, u.LevelSequence, u.LevelSequenceFactoryNew())
    assert seq
    seq.set_display_rate(u.FrameRate(30, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(frames)
    return seq

def bounded(track, frames):
    section = track.add_section()
    section.set_range(0, frames)
    section.set_completion_mode(u.MovieSceneCompletionMode.RESTORE_STATE)
    return section

def transform(seq, actor, rows, frames):
    binding = seq.add_possessable(actor)
    section = bounded(binding.add_track(u.MovieScene3DTransformTrack), frames)
    channels = section.get_channels_by_type(u.MovieSceneScriptingDoubleChannel)
    assert len(channels) == 9
    for frame, location, rotation in rows:
        values = [location.x, location.y, location.z, rotation.roll, rotation.pitch, rotation.yaw, 1, 1, 1]
        for channel, value in zip(channels, values):
            channel.add_key(u.FrameNumber(frame), value, interpolation=u.MovieSceneKeyInterpolation.AUTO)
    return binding

def camera_track(seq, camera, rows, frames):
    binding = transform(seq, camera, rows, frames)
    cut = bounded(seq.add_track(u.MovieSceneCameraCutTrack), frames)
    cut.set_camera_binding_id(seq.get_binding_id(binding))

def float_track(seq, obj, prop, keys, frames):
    track = seq.add_possessable(obj).add_track(u.MovieSceneFloatTrack)
    track.set_property_name_and_path(prop, prop)
    section = bounded(track, frames)
    channel = section.get_channels_by_type(u.MovieSceneScriptingFloatChannel)[0]
    for frame, value in keys:
        channel.add_key(u.FrameNumber(frame), value, interpolation=u.MovieSceneKeyInterpolation.AUTO)

def fade(seq, keys, frames, keep_final=False):
    section = bounded(seq.add_track(u.MovieSceneFadeTrack), frames)
    if keep_final:
        section.set_completion_mode(u.MovieSceneCompletionMode.KEEP_STATE)
    section.set_editor_property('fade_audio', False)  # Original sound pass is deliberately later.
    channel = section.get_channels_by_type(u.MovieSceneScriptingFloatChannel)[0]
    for frame, value in keys:
        channel.add_key(u.FrameNumber(frame), value, interpolation=u.MovieSceneKeyInterpolation.LINEAR)

def visibility(seq, actor, hidden, frames):
    track = seq.add_possessable(actor).add_track(u.MovieSceneVisibilityTrack)
    track.set_property_name_and_path('bHidden', 'bHidden')
    section = bounded(track, frames)
    # Visibility tracks expose visible=true, unlike their underlying bHidden field.
    section.get_channels_by_type(u.MovieSceneScriptingBoolChannel)[0].set_default(not hidden)

def animate(seq, component, clip, frames):
    section = bounded(seq.add_possessable(component).add_track(u.MovieSceneSkeletalAnimationTrack), frames)
    params = section.get_editor_property('params')
    params.set_editor_property('animation', clip)
    # UE5.8 stores PlayRate as MovieSceneTimeWarpVariant; its default is 1x.
    section.set_editor_property('params', params)

def look(location, target):
    return u.MathLibrary.find_look_at_rotation(location, target)

def configure_camera(camera, fov=70):
    camera.camera_component.set_field_of_view(fov)
    post = u.PostProcessSettings()
    for key, value in [('override_auto_exposure_method', True), ('auto_exposure_method', u.AutoExposureMethod.AEM_MANUAL),
                       ('override_auto_exposure_apply_physical_camera_exposure', True), ('auto_exposure_apply_physical_camera_exposure', False),
                       ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0),
                       ('override_motion_blur_amount', True), ('motion_blur_amount', 0),
                       ('override_bloom_intensity', True), ('bloom_intensity', .15)]:
        post.set_editor_property(key, value)
    camera.camera_component.set_editor_property('post_process_settings', post)

def candle(label, location):
    actor = ACTORS.spawn_actor_from_class(u.TallowCandlePresentationActor, location)
    actor.set_actor_label(label)
    stick = u.load_asset('/Game/Props/Candle/wooden_candlestick_1k')
    assert stick
    actor.candle.set_static_mesh(stick)
    actor.candle.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    actor.wick.set_relative_location(u.Vector(0, 0, stick.get_bounding_box().max.z), False, False)
    actor.flame.flame_mesh = u.load_asset('/Game/Presentation/Flame/SM_TallowFlame')
    actor.flame.flame_material = u.load_asset('/Game/Presentation/Flame/M_TallowFlame')
    actor.draft_exposure = .8
    actor.flame.initialize_visual()
    actor.set_folder_path('Presentation/Cinematics')
    return actor

def menu_and_intro(clip):
    level = DEST + '/Maps/Lvl_MenuPresentation'
    assert ED.new_level_from_template(level, '/Game/Presentation/Environment/Maps/Lvl_MenuHeroRoomV02')
    all_actors = ACTORS.get_all_level_actors()
    camera = next(a for a in all_actors if a.get_actor_label() == 'ReviewCamera_V02')
    player = ACTORS.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector(83, -72, 12), u.Rotator(pitch=0, yaw=-180, roll=0))
    player.skeletal_mesh_component.set_skeletal_mesh_asset(u.load_asset('/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Full'))
    player.set_actor_enable_collision(False)
    player.skeletal_mesh_component.override_animation_data(clip, True, False, 0, 1)
    player.skeletal_mesh_component.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    player.set_actor_label('BoundPlayer_Cinematic')
    # Side cabinet gives the candle an actual supporting surface beside the chair.
    cabinet = ACTORS.spawn_actor_from_class(u.StaticMeshActor, u.Vector(145, -55, 4))
    cabinet.set_actor_label('CandleSideCabinet')
    cabinet.static_mesh_component.set_static_mesh(u.load_asset('/Game/Presentation/Environment/Cabinet/SM_Cabinet_SageTimber'))
    assert cabinet.static_mesh_component.static_mesh
    cabinet.set_actor_scale3d(u.Vector(.48, .48, .48))
    cabinet.set_actor_enable_collision(False)
    prop = candle('MenuCandle', u.Vector(145, -55, 70))
    light = ACTORS.spawn_actor_from_class(u.PointLight, u.Vector(145, -55, 94))
    light.set_actor_label('MenuCandleLight')
    light.point_light_component.set_intensity(2.5)
    light.point_light_component.set_light_color(u.LinearColor(1, .5, .19, 1))

    menu = sequence('LS_MainMenu_Stillness', 360)
    origin = camera.get_actor_location()
    rotation = camera.get_actor_rotation()
    camera_track(menu, camera, [(0, origin, rotation), (180, origin + u.Vector(.8, -.8, .3), rotation), (360, origin, rotation)], 360)
    animate(menu, player.skeletal_mesh_component, clip, 360)
    float_track(menu, prop, 'DraftExposure', [(0, .7), (70, 1), (170, .72), (265, .96), (360, .7)], 360)

    partial = ACTORS.spawn_actor_from_class(u.SkeletalMeshActor, player.get_actor_location(), player.get_actor_rotation())
    partial.set_actor_label('IntroPartialBody')
    partial.skeletal_mesh_component.set_skeletal_mesh_asset(u.load_asset('/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Partial'))
    partial.skeletal_mesh_component.override_animation_data(clip, True, False, 0, 1)
    partial.set_actor_hidden_in_game(True)
    partial.set_actor_enable_collision(False)
    intro_camera = ACTORS.spawn_actor_from_class(u.CameraActor, u.Vector(83, -64, 103))
    intro_camera.set_actor_label('IntroWakeCamera')
    configure_camera(intro_camera, 70)
    intro = sequence('LS_Intro_Wake', 240)
    eye = intro_camera.get_actor_location()
    camera_track(intro, intro_camera, [(0, eye + u.Vector(0, 0, -2), look(eye, u.Vector(83, -30, 56))),
                                      (65, eye, look(eye, u.Vector(83, -30, 56))),
                                      (145, eye, look(eye, u.Vector(145, -55, 91))),
                                      (240, eye, look(eye, u.Vector(130, 30, 94)))], 240)
    visibility(intro, player, True, 240)
    visibility(intro, partial, False, 240)
    animate(intro, partial.skeletal_mesh_component, clip, 240)
    fade(intro, [(0, 1), (18, 1), (55, 0), (240, 0)], 240)
    for asset in [menu, intro]:
        assert E.save_loaded_asset(asset, False)
    assert ED.save_current_level()

def outro():
    assert ED.new_level_from_template(DEST + '/Maps/Lvl_OutroPresentation', '/Game/Presentation/Environment/Maps/Lvl_AtticHeroRoomV02')
    origin = u.Vector(100, -340, 120)
    target = u.Vector(-40, -100, 100)
    rotation = look(origin, target)
    camera = ACTORS.spawn_actor_from_class(u.CameraActor, origin, rotation)
    camera.set_actor_label('LastFlameCamera')
    configure_camera(camera, 65)
    forward = u.MathLibrary.get_forward_vector(rotation)
    right = u.MathLibrary.get_right_vector(rotation)
    prop = candle('LastFlame', origin + forward * 48 + right * 14 + u.Vector(0, 0, -24))
    seq = sequence('LS_Outro_LastFlame', 300)
    camera_track(seq, camera, [(0, origin, rotation), (210, origin - forward * 12, rotation), (300, origin - forward * 12, rotation)], 300)
    pos = prop.get_actor_location()
    transform(seq, prop, [(0, pos, u.Rotator()), (210, pos - forward * 12, u.Rotator()), (300, pos - forward * 12, u.Rotator())], 300)
    float_track(seq, prop, 'DraftExposure', [(0, .3), (100, .75), (160, 1), (300, 1)], 300)
    state = seq.add_possessable(prop).add_track(u.MovieSceneByteTrack)
    state.set_property_name_and_path('FlameState', 'FlameState')
    channel = bounded(state, 300).get_channels_by_type(u.MovieSceneScriptingByteChannel)[0]
    channel.add_key(u.FrameNumber(0), 1)
    channel.add_key(u.FrameNumber(210), 0)
    # Keep the final black frame over any restored candle state; the gameplay
    # owner explicitly clears the fade when showing the ending/menu destination.
    fade(seq, [(0, 0), (258, 0), (285, 1), (300, 1)], 300, keep_final=True)
    assert E.save_loaded_asset(seq, False)
    assert ED.save_current_level()

def main():
    for path in [DEST + '/Maps/Lvl_MenuPresentation', DEST + '/Maps/Lvl_OutroPresentation',
                 DEST + '/LS_MainMenu_Stillness', DEST + '/LS_Intro_Wake', DEST + '/LS_Outro_LastFlame']:
        assert not E.does_asset_exist(path), 'Refusing existing cinematic destination: ' + path
    clip = u.load_asset('/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_SeatedBoundIdle')
    assert clip, 'Import validated SeatedV04 first'
    menu_and_intro(clip)
    u.SystemLibrary.collect_garbage()
    outro()
    out = ROOT / 'Tallow/Saved/Presentation/cinematic-build.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({'created': ['LS_MainMenu_Stillness', 'LS_Intro_Wake', 'LS_Outro_LastFlame'],
                               'status': 'Built; rendered timing and binding review required before gameplay handoff'}, indent=2))

if __name__ == '__main__':
    main()
