"""Root UE material-only refresh, no level edits or shared material writes."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';D='/Game/Presentation/Environment/HouseIntegrationV01';lib=u.EditorAssetLibrary;at=u.AssetToolsHelpers.get_asset_tools();made={}
for key in ['Limewash','Wallpaper']:
 fn=S/'Surfaces'/('T_House_'+key+'_BaseColor.png');t=u.AssetImportTask();t.set_editor_property('filename',str(fn));t.set_editor_property('destination_path',D+'/Textures');t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',True);at.import_asset_tasks([t]);tex=lib.load_asset(D+'/Textures/'+fn.stem);name='M_House_'+key;mat=lib.load_asset(D+'/Materials/'+name) or at.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(mat);e=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0);e.set_editor_property('texture',tex);u.MaterialEditingLibrary.connect_material_property(e,'RGB',u.MaterialProperty.MP_BASE_COLOR);rough=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionConstant,0,180);rough.set_editor_property('r',.89);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.recompile_material(mat);lib.save_loaded_asset(mat);made[key]=mat
for row in json.loads((S/'Architecture/manifest.json').read_text()):
 m=lib.load_asset(D+'/Architecture/'+row['mesh'])
 if not m:continue
 for i,n in enumerate(row['materials']):
  if n.endswith('LimePlaster'):m.set_material(i,made['Limewash'])
  elif n.endswith('FadedWallpaper'):m.set_material(i,made['Wallpaper'])
 lib.save_loaded_asset(m)
print('HOUSE_SURFACE_REFRESH_COMPLETE')
