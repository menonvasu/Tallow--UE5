"""The menu close-up: Nell in the chair by the candle, my waltz, and Mim's face in the candle's flicker. Re-runnable.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -run=pythonscript -script=/abs/path/build_menu_scene.py

Changes to Lvl_MenuPresentation (only these, by label): adds or updates the CineCameraActor 'MenuCloseUpCamera' and
the TallowMenuScene 'MenuScene'; points TALLOW_PlayDirector's MenuSequence at the new sequence. Nothing placed
before is moved. The old LS_MainMenu_Stillness and ReviewCamera_V02 are left as they were (unused by the menu now).
New assets (owned here): /Game/FinalPass/Menu/LS_Menu_NellCloseUp.
Reads menu_staging.json (camera) and menu_frames.json (where her face is, from solve_face.py; verify_menu.py checks it in UE).
Run import_menu_audio.py first.
"""
import json, math
from pathlib import Path
import unreal as u

HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE / 'menu_staging.json').read_text())
FRAMES = json.loads((HERE / 'menu_frames.json').read_text()) if (HERE / 'menu_frames.json').exists() else {'apparitions': []}
MAP = '/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation'
DEST = '/Game/FinalPass/Menu'
SEQ = DEST + '/LS_Menu_NellCloseUp'
E = u.EditorAssetLibrary
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
A = u.get_editor_subsystem(u.EditorActorSubsystem)
LOOP = 600  # 20 s at 30 fps

# The calm shot's light, scaled only while the menu is up (the intro after it keeps the room's own levels).
# The candle is the key now; the big warm card that stood in for it drops back to a bounce; the cold top light
# becomes a thin rim on her hair and the chair; the fill barely lifts the shadow side.
LIGHTS = {'MenuCandleLight': (9.0, True), 'MenuWarm': (.45, False), 'MenuCold': (.35, False), 'MenuFill': (.6, False)}


def V(p): return u.Vector(*p)


def by_label():
    return {a.get_actor_label(): a for a in A.get_all_level_actors()}


def camera(actors):
    cam = actors.get('MenuCloseUpCamera')
    if not cam:
        cam = A.spawn_actor_from_class(u.CineCameraActor, V(CONFIG['camera']['eye']))
        cam.set_actor_label('MenuCloseUpCamera')
        cam.set_folder_path('Presentation/Cinematics')
    c = CONFIG['camera']
    eye, target = V(c['eye']), V(c['target'])
    rot = u.MathLibrary.find_look_at_rotation(eye, target)
    cam.set_actor_location(eye, False, False)
    cam.set_actor_rotation(rot, False)
    cc = cam.get_cine_camera_component()
    fb = cc.get_editor_property('filmback')
    fb.set_editor_property('sensor_width', c['filmback'])
    fb.set_editor_property('sensor_height', c['filmback'] / c['aspect'])
    cc.set_editor_property('filmback', fb)
    cc.set_editor_property('current_focal_length', c['focal'])
    cc.set_editor_property('current_aperture', c['aperture'])
    fs = cc.get_editor_property('focus_settings')
    fs.set_editor_property('focus_method', u.CameraFocusMethod.MANUAL)
    fs.set_editor_property('manual_focus_distance', u.Vector.dot(V(c['focus_point']) - eye, u.MathLibrary.get_forward_vector(rot)))
    cc.set_editor_property('focus_settings', fs)
    post = cc.get_editor_property('post_process_settings')
    for key, value in [('override_auto_exposure_method', True), ('auto_exposure_method', u.AutoExposureMethod.AEM_MANUAL),
                       ('override_auto_exposure_apply_physical_camera_exposure', True), ('auto_exposure_apply_physical_camera_exposure', False),
                       ('override_auto_exposure_bias', True), ('auto_exposure_bias', c.get('exposure', -0.8)),
                       ('override_motion_blur_amount', True), ('motion_blur_amount', 0.0),
                       ('override_bloom_intensity', True), ('bloom_intensity', .35),
                       ('override_vignette_intensity', True), ('vignette_intensity', 0.0),
                       ('override_film_grain_intensity', True), ('film_grain_intensity', .12),
                       ('override_film_toe', True), ('film_toe', .6)]:
        post.set_editor_property(key, value)
    cc.set_editor_property('post_process_settings', post)
    return cam


def sequence(cam, nell, candle):
    if E.does_asset_exist(SEQ):
        assert E.delete_asset(SEQ), SEQ
    seq = u.AssetToolsHelpers.get_asset_tools().create_asset('LS_Menu_NellCloseUp', DEST, u.LevelSequence, u.LevelSequenceFactoryNew())
    seq.set_display_rate(u.FrameRate(30, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(LOOP)

    def bounded(track):
        section = track.add_section()
        section.set_range(0, LOOP)
        section.set_completion_mode(u.MovieSceneCompletionMode.RESTORE_STATE)
        return section

    # A held breath of a camera: a slow loop of under a centimetre, never a push that would jump at the seam.
    binding = seq.add_possessable(cam)
    channels = bounded(binding.add_track(u.MovieScene3DTransformTrack)).get_channels_by_type(u.MovieSceneScriptingDoubleChannel)
    eye = V(CONFIG['camera']['eye']); rot = cam.get_actor_rotation(); r = CONFIG['camera']['drift_cm']
    for k in range(5):
        a = 2 * math.pi * k / 4
        p = eye + u.Vector(r * math.sin(a), r * .6 * math.sin(a), r * .5 * math.cos(a) - r * .5)
        values = [p.x, p.y, p.z, rot.roll, rot.pitch + .15 * math.sin(a), rot.yaw + .2 * math.cos(a) - .2, 1, 1, 1]
        for channel, value in zip(channels, values):
            channel.add_key(u.FrameNumber(k * LOOP // 4), value, interpolation=u.MovieSceneKeyInterpolation.AUTO)
    cut = bounded(seq.add_track(u.MovieSceneCameraCutTrack))
    cut.set_camera_binding_id(seq.get_binding_id(binding))
    # Her breathing: the inspected seated clip the old menu used, looping.
    section = bounded(seq.add_possessable(nell.skeletal_mesh_component).add_track(u.MovieSceneSkeletalAnimationTrack))
    params = section.get_editor_property('params')
    params.set_editor_property('animation', u.load_asset('/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_SeatedBoundIdle'))
    section.set_editor_property('params', params)
    # The flame leans in a draught now and then.
    track = seq.add_possessable(candle).add_track(u.MovieSceneFloatTrack)
    track.set_property_name_and_path('DraftExposure', 'DraftExposure')
    ch = bounded(track).get_channels_by_type(u.MovieSceneScriptingFloatChannel)[0]
    for f, v in [(0, .7), (90, .95), (170, .72), (260, .78), (330, 1.0), (420, .74), (520, .9), (600, .7)]:
        ch.add_key(u.FrameNumber(f), v, interpolation=u.MovieSceneKeyInterpolation.AUTO)
    assert E.save_loaded_asset(seq, False)
    return seq


def scene(actors, cam, nell):
    s = actors.get('MenuScene')
    if not s:
        s = A.spawn_actor_from_class(u.TallowMenuScene, u.Vector(0, 0, 0))
        s.set_actor_label('MenuScene')
        s.set_folder_path('Presentation/Cinematics')
    mim = u.load_asset('/Game/Characters/Mim/SK_Mim')
    for comp in (s.mim, s.mim_close):
        comp.set_skeletal_mesh_asset(mim)
    s.set_editor_property('camera', cam)
    s.set_editor_property('nell', nell)
    lights = []
    for label, (scale, candle) in LIGHTS.items():
        light = actors[label]
        comp = light.get_component_by_class(u.LightComponent)
        if comp.get_editor_property('mobility') == u.ComponentMobility.STATIC:
            comp.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
        entry = u.TallowMenuLight()
        entry.set_editor_property('light', light)
        entry.set_editor_property('menu_scale', scale)
        entry.set_editor_property('candle', candle)
        lights.append(entry)
    s.set_editor_property('lights', lights)
    s.set_editor_property('music', u.load_asset(DEST + '/Audio/MUS_BrokenDollsWaltz'))
    stings = {'high': u.load_asset(DEST + '/Audio/SFX_MenuSting_HighBig'), 'low': u.load_asset(DEST + '/Audio/SFX_MenuSting_Low')}
    shows = []
    for spec in FRAMES['apparitions']:
        shot = u.TallowMenuApparition()
        frames = []
        for i, f in enumerate(spec['frames']):
            frame = u.TallowMenuFlashFrame()
            frame.set_editor_property('puppet', i)
            rot = u.Rotator(roll=f.get('roll', 0.0), pitch=f.get('pitch', 0.0), yaw=f['yaw'])
            frame.set_editor_property('transform', u.Transform(V(f['root']), rot, u.Vector(1, 1, 1) * f.get('scale', 1.0)))
            frame.set_editor_property('pose', u.load_asset('/Game/Characters/Mim/Anims/' + f['clip']))
            frame.set_editor_property('pose_time', f['t'])
            frame.set_editor_property('seconds', f['seconds'])
            frames.append(frame)
        shot.set_editor_property('frames', frames)
        shot.set_editor_property('sting', stings[spec['sting']])
        shot.set_editor_property('empty_chair_seconds', spec.get('empty_chair', 0.0))
        shows.append(shot)
    s.set_editor_property('apparitions', shows)
    return s


def main():
    assert ED.load_level(MAP)
    actors = by_label()
    nell, candle, director = actors['BoundPlayer_Cinematic'], actors['MenuCandle'], actors['TALLOW_PlayDirector']
    cam = camera(actors)
    seq = sequence(cam, nell, candle)
    scene(actors, cam, nell)
    director.set_editor_property('menu_sequence', seq)
    assert ED.save_current_level()
    u.log('TALLOW_MENU_BUILD ok apparitions=%d' % len(FRAMES['apparitions']))


main()
