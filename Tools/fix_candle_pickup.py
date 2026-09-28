"""Candle pickup was a grey cube with its light sealed inside it (invisible in the dark). Makes it a small wax candle
standing on a crate, with the light at the flame point above it. Level instance only; re-runnable."""
import unreal as u
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
acts=eas.get_all_level_actors()
for a in acts:
    if a.get_actor_label()=='Candle_Crate': eas.destroy_actor(a)
pick=[a for a in acts if a.get_actor_label()=='M3_CandlePickup'][0]
crate=eas.spawn_actor_from_class(u.StaticMeshActor,u.Vector(365,-325,-330+27.5)); crate.set_actor_label('Candle_Crate')
crate.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube')); crate.set_actor_scale3d(u.Vector(0.45,0.45,0.55))
crate.static_mesh_component.set_material(0,u.load_asset('/Game/Environment/HouseMaterials/MI_Furniture')); crate.set_folder_path('Greybox/Furniture')
mesh=pick.get_component_by_class(u.StaticMeshComponent)
mesh.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cylinder'))
pick.set_actor_scale3d(u.Vector(0.07,0.07,0.2))                         # 7 cm wide, 20 cm tall wax candle
pick.set_actor_location(u.Vector(365,-325,-275+10),False,False)         # standing on the crate top (-275)
mesh.set_cast_shadow(False)
mesh.set_material(0,u.load_asset('/Game/Environment/HouseMaterials/MI_Plaster'))
light=pick.get_component_by_class(u.PointLightComponent)
light.set_relative_location(u.Vector(0,0,75),False,False)                # 15 cm above the candle (scaled by 0.2)
light.set_editor_property('light_color',u.Color(r=255,g=150,b=70,a=255)); light.set_editor_property('source_radius',1.0); light.set_editor_property('intensity',1500.0)   # warm flame, not blinding up close
assert ed.save_current_level(); u.log('CANDLE pickup fixed')
