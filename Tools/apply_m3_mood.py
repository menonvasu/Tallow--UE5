"""M3 mood pass: switch off the greybox work lights, leave faint moonlight, lock exposure so the dark stays dark,
and add fog, vignette and grain. The candle becomes the player's real light. Re-runnable."""
import unreal as u
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
actors=eas.get_all_level_actors()
for a in actors:
    lab=a.get_actor_label()
    if lab.startswith('M3_WorkLight'):
        for c in a.get_components_by_class(u.LightComponent): c.set_editor_property('intensity',0.0); c.set_visibility(False)
        a.set_actor_hidden_in_game(True)
    if lab=='M3_Moon':
        c=a.get_component_by_class(u.DirectionalLightComponent)
        c.set_editor_property('intensity',0.06); c.set_editor_property('light_color',u.Color(r=150,g=170,b=255,a=255))
for a in actors:
    if a.get_actor_label() in ('M3_Mood_PostProcess','M3_Mood_Fog'): eas.destroy_actor(a)
pp=eas.spawn_actor_from_class(u.PostProcessVolume,u.Vector(0,0,0)); pp.set_actor_label('M3_Mood_PostProcess')
pp.set_editor_property('unbound',True)
st=pp.get_editor_property('settings')
def setp(name,val):
    st.set_editor_property('override_'+name,True); st.set_editor_property(name,val)
setp('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL)
setp('auto_exposure_apply_physical_camera_exposure',False)
setp('auto_exposure_bias',float(__import__('os').environ.get('MOOD_EXPOSURE','1.0')))
setp('vignette_intensity',0.75); setp('film_grain_intensity',0.35); setp('bloom_intensity',0.5)
setp('color_saturation',u.Vector4(0.8,0.8,0.8,1.0))
pp.set_editor_property('settings',st)
fog=eas.spawn_actor_from_class(u.ExponentialHeightFog,u.Vector(0,0,-300)); fog.set_actor_label('M3_Mood_Fog')
fc=fog.get_component_by_class(u.ExponentialHeightFogComponent)
fc.set_editor_property('fog_density',0.06); fc.set_editor_property('fog_inscattering_luminance',u.LinearColor(0.004,0.004,0.006,1)); fc.set_editor_property('start_distance',150.0)
# report the candle light the flame component drives
cls=u.load_class(None,'/Game/FirstPerson/Blueprints/BP_TallowCharacter.BP_TallowCharacter_C')
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
for hnd in sub.k2_gather_subobject_data_for_blueprint(u.load_asset('/Game/FirstPerson/Blueprints/BP_TallowCharacter')):
    d=sub.k2_find_subobject_data_from_handle(hnd); o=u.SubobjectDataBlueprintFunctionLibrary.get_object(d)
    if o and 'Light' in o.get_class().get_name():
        u.log('MOOD candle light %s intensity=%s radius=%s units=%s'%(o.get_name(),o.get_editor_property('intensity'),o.get_editor_property('attenuation_radius'),o.get_editor_property('intensity_units')))
assert ed.save_current_level()
u.log('MOOD applied')
