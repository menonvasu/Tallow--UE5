"""Review-driven camera/lighting pass, confined to new presentation assets.

Run after build_cinematic_sequences.py and refine_menu_lighting.py.
Idempotent: updates named actors/tracks; creates only its own missing sky/hands.
"""
from pathlib import Path
import json
import time
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
BASE = '/Game/Presentation/Cinematics'
E = u.EditorAssetLibrary
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
A = u.get_editor_subsystem(u.EditorActorSubsystem)

def rewrite_transform(track, rows):
    section = track.get_sections()[0]
    channels = section.get_channels_by_type(u.MovieSceneScriptingDoubleChannel)
    assert len(channels) == 9
    for channel in channels:
        for key in channel.get_keys():
            channel.remove_key(key)
    for frame, location, rotation in rows:
        values = [location.x, location.y, location.z, rotation.roll, rotation.pitch, rotation.yaw, 1, 1, 1]
        for channel, value in zip(channels, values):
            channel.add_key(u.FrameNumber(frame), value, interpolation=u.MovieSceneKeyInterpolation.AUTO)

def transform_tracks(seq):
    return [track for binding in seq.get_bindings() for track in binding.get_tracks() if isinstance(track, u.MovieScene3DTransformTrack)]

def sky():
    path = BASE + '/M_ColdSky'
    mat = u.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('M_ColdSky', BASE, u.Material, u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    color = u.MaterialEditingLibrary.create_material_expression(mat, u.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', u.LinearColor(.19, .25, .32, 1))
    u.MaterialEditingLibrary.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.recompile_material(mat)
    E.save_loaded_asset(mat, False)
    actor = next((a for a in A.get_all_level_actors() if a.get_actor_label() == 'ColdSky_Backdrop'), None)
    if not actor:
        actor = A.spawn_actor_from_class(u.StaticMeshActor, u.Vector(100, -550, 150), u.Rotator(roll=90))
        actor.set_actor_label('ColdSky_Backdrop')
    actor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Plane'))
    actor.static_mesh_component.set_material(0, mat)
    actor.static_mesh_component.set_cast_shadow(False)
    actor.set_actor_scale3d(u.Vector(8, 6, 1))
    actor.set_actor_enable_collision(False)

def room_light():
    for actor in A.get_all_level_actors():
        label = actor.get_actor_label()
        if label in ('ColdExit', 'Amber', 'RoomFill'):
            actor.get_component_by_class(u.RectLightComponent).set_intensity({'ColdExit': 12, 'Amber': 2.5, 'RoomFill': .7}[label])
        if isinstance(actor, u.CameraActor):
            post = actor.camera_component.get_editor_property('post_process_settings')
            post.set_editor_property('override_vignette_intensity', True)
            post.set_editor_property('vignette_intensity', .45)
            actor.camera_component.set_editor_property('post_process_settings', post)

def main():
    assert ED.load_level(BASE + '/Maps/Lvl_MenuPresentation')
    actors = {a.get_actor_label(): a for a in A.get_all_level_actors()}
    camera = actors['ReviewCamera_V02']
    location = u.Vector(20, 280, 105)
    rotation = u.MathLibrary.find_look_at_rotation(location, u.Vector(20, -70, 90))
    rotation.roll = -1.2
    camera.set_actor_location(location, False, False)
    camera.set_actor_rotation(rotation, False)
    camera.camera_component.set_field_of_view(46)
    actors['MenuWarm'].get_component_by_class(u.RectLightComponent).set_intensity(5.5)
    seq = u.load_asset(BASE + '/LS_MainMenu_Stillness')
    tracks = transform_tracks(seq)
    assert len(tracks) == 1
    rewrite_transform(tracks[0], [(0, location, rotation), (180, location + u.Vector(.8, -.8, .3), rotation), (360, location, rotation)])
    E.save_loaded_asset(seq, False)
    seq = u.load_asset(BASE + '/LS_Intro_Wake')
    eye = actors['IntroWakeCamera'].get_actor_location()
    tracks = transform_tracks(seq)
    assert len(tracks) == 1
    rewrite_transform(tracks[0], [(0, eye, u.MathLibrary.find_look_at_rotation(eye, u.Vector(42, -42, 59))),
                                 (65, eye, u.MathLibrary.find_look_at_rotation(eye, u.Vector(42, -42, 59))),
                                 (145, eye, u.MathLibrary.find_look_at_rotation(eye, u.Vector(145, -55, 91))),
                                 (240, eye, u.MathLibrary.find_look_at_rotation(eye, u.Vector(122, -40, 60)))])
    E.save_loaded_asset(seq, False)
    assert ED.save_current_level()

    assert ED.load_level('/Game/Presentation/Environment/Maps/Lvl_AtticHeroRoomV02')
    room_light()
    sky()
    assert ED.save_current_level()

    assert ED.load_level(BASE + '/Maps/Lvl_OutroPresentation')
    room_light()
    sky()
    actors = {a.get_actor_label(): a for a in A.get_all_level_actors()}
    camera, candle = actors['LastFlameCamera'], actors['LastFlame']
    location = u.Vector(100, -125, 118)
    rotation = u.MathLibrary.find_look_at_rotation(location, u.Vector(100, -500, 125))
    end = location + u.MathLibrary.get_forward_vector(rotation) * 50
    camera.set_actor_location(location, False, False)
    camera.set_actor_rotation(rotation, False)
    cc = camera.camera_component
    for key, value in [('enable_first_person_field_of_view', True), ('first_person_field_of_view', 90), ('enable_first_person_scale', True), ('first_person_scale', .6)]:
        cc.set_editor_property(key, value)
    pose = u.load_asset('/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_Hold')
    hands = actors.get('LastFlameHands')
    if not hands:
        hands = A.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector())
        hands.set_actor_label('LastFlameHands')
    sc = hands.skeletal_mesh_component
    sc.set_skeletal_mesh_asset(u.load_asset('/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Partial'))
    sc.override_animation_data(pose, False, False, 17 / 30, 0)
    sc.set_bounds_scale(3)
    sc.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    sc.set_first_person_primitive_type(u.FirstPersonPrimitiveType.FIRST_PERSON)
    hands.set_actor_enable_collision(False)
    candle.flame.first_person_visual = True  # Persist creation policy for the transient runtime flame mesh.
    for component in candle.get_components_by_class(u.StaticMeshComponent):
        component.set_first_person_primitive_type(u.FirstPersonPrimitiveType.FIRST_PERSON)
    seq = u.load_asset(BASE + '/LS_Outro_LastFlame')
    tracks = transform_tracks(seq)
    assert len(tracks) in (2, 3), len(tracks)
    # Creation order is camera, candle, then the optional hands refinement.
    rewrite_transform(tracks[0], [(0, location, rotation), (210, end, rotation), (300, end, rotation)])
    camera.set_actor_location(location, False, False)
    transform = camera.get_actor_transform()
    candle_pos = u.MathLibrary.transform_location(transform, u.Vector(45, 26, -30))
    hand_pos = u.MathLibrary.transform_location(transform, u.Vector(20, 0, -110))
    hand_rot = u.MathLibrary.compose_rotators(u.Rotator(yaw=90), rotation)
    delta = end - location
    candle.set_actor_location(candle_pos, False, False)
    candle.set_actor_rotation(rotation, False)
    hands.set_actor_location(hand_pos, False, False)
    hands.set_actor_rotation(hand_rot, False)
    rewrite_transform(tracks[1], [(0, candle_pos, rotation), (210, candle_pos + delta, rotation), (300, candle_pos + delta, rotation)])
    if len(tracks) == 2:
        track = seq.add_possessable(hands).add_track(u.MovieScene3DTransformTrack)
        section = track.add_section()
        section.set_range(0, 300)
        section.set_completion_mode(u.MovieSceneCompletionMode.RESTORE_STATE)
    else:
        track = tracks[2]
    rewrite_transform(track, [(0, hand_pos, hand_rot), (210, hand_pos + delta, hand_rot), (300, hand_pos + delta, hand_rot)])
    E.save_loaded_asset(seq, False)
    assert ED.save_current_level()
    (ROOT/'Tallow/Saved/Presentation/cinematic-staging-refinement.json').write_text(json.dumps({'applied': True, 'visual_review': 'required'}, indent=2))

if __name__ == '__main__':
    main()
    started = time.monotonic()
    def quit_after_save(dt):
        if time.monotonic() - started > 3:
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()
    handle = u.register_slate_post_tick_callback(quit_after_save)
