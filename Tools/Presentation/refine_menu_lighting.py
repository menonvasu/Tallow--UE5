"""Apply the reviewed menu lighting to isolated presentation maps only.

Run after room/cinematic creation. Never loads or alters the shared gameplay map.
"""
import time
import unreal as u

LEVELS = (
    '/Game/Presentation/Environment/Maps/Lvl_MenuHeroRoomV02',
    '/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation',
)

def main():
    ed = u.get_editor_subsystem(u.LevelEditorSubsystem)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    for level in LEVELS:
        assert ed.load_level(level), level
        for actor in actors.get_all_level_actors():
            label = actor.get_actor_label()
            if label == 'MenuWarm':
                actor.set_actor_location(u.Vector(145, -15, 94), False, False)
                actor.set_actor_rotation(u.MathLibrary.find_look_at_rotation(actor.get_actor_location(), u.Vector(83, -72, 90)), False)
                component = actor.get_component_by_class(u.RectLightComponent)
                component.set_intensity(8)
                component.set_editor_property('source_width', 25)
                component.set_editor_property('source_height', 25)
            elif label in ('MenuCold', 'MenuFill'):
                actor.get_component_by_class(u.RectLightComponent).set_intensity(18 if label == 'MenuCold' else .4)
            elif label == 'MenuCandleLight':
                actor.point_light_component.set_intensity(.6)
            if isinstance(actor, u.CameraActor):
                post = actor.camera_component.get_editor_property('post_process_settings')
                post.set_editor_property('override_auto_exposure_bias', True)
                post.set_editor_property('auto_exposure_bias', -.8)
                actor.camera_component.set_editor_property('post_process_settings', post)
        assert ed.save_current_level()

if __name__ == '__main__':
    main()
    started = time.monotonic()
    def quit_after_save(dt):
        if time.monotonic() - started > 3:
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()
    handle = u.register_slate_post_tick_callback(quit_after_save)
