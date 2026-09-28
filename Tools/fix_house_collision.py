"""Makes every M3 house mesh solid: turns on per-section collision (complex-as-simple then works) and saves.
Found 2026-09-28: ground and upper storey meshes had section collision off, so walls and floors were walk-through."""
import unreal as u
sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
fixed=[]
for path in u.EditorAssetLibrary.list_assets('/Game/Environment/M3House',recursive=False,include_folder=False):
    m=u.load_asset(path)
    if not isinstance(m,u.StaticMesh): continue
    changed=False
    for s in range(sub.get_number_materials(m) if hasattr(sub,'get_number_materials') else 1):
        if not sub.is_section_collision_enabled(m,0,s):
            sub.enable_section_collision(m,True,0,s); changed=True
    bs=m.get_editor_property('body_setup'); bs.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if changed:
        u.EditorAssetLibrary.save_loaded_asset(m); fixed.append(m.get_name())
u.log('COLLISION fixed %d meshes: %s'%(len(fixed),fixed))
