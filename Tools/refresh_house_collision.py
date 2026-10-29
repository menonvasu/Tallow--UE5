"""Rebuild collision-derived data after changing imported meshes' collision mode."""
import unreal as u,os
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sub,'Run in normal UnrealEditor, not UnrealEditor-Cmd'
for path in u.EditorAssetLibrary.list_assets('/Game/Environment/M3House',recursive=False,include_folder=False):
 m=u.load_asset(path)
 if isinstance(m,u.StaticMesh):
  m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  assert sub.remove_collisions(m)
  u.EditorAssetLibrary.save_loaded_asset(m)
ed.save_current_level()
