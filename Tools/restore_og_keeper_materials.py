"""Original Keeper restored from git: relink its material slots (Keeper_X) to the Proxy_X materials beside it."""
import unreal as u
m=u.load_asset('/Game/Characters/Keeper/Animated/SK_Keeper'); mats=m.materials; miss=[]
for s in mats:
    name=str(s.material_slot_name); p='/Game/Characters/Keeper/Proxy_'+name[len('Keeper_'):]
    mat=u.load_asset(p) if u.EditorAssetLibrary.does_asset_exist(p) else None
    if not mat: miss.append(name); continue
    if isinstance(mat,u.Material):
        mat.set_editor_property('used_with_skeletal_mesh',True); mat.set_editor_property('used_with_morph_targets',True)
        u.MaterialEditingLibrary.recompile_material(mat); u.EditorAssetLibrary.save_loaded_asset(mat)
    s.material_interface=mat
m.set_editor_property('materials',mats); u.EditorAssetLibrary.save_loaded_asset(m)
u.log_warning('OGKEEPER slots=%d missing=%s'%(len(mats),miss)); u.SystemLibrary.quit_editor()
