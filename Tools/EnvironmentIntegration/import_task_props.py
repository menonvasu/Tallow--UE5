"""Root: imports task visuals, leaves all existing gameplay ownership untouched."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01/TaskProps';D='/Game/Presentation/Environment/HouseIntegrationV01';cfg=json.loads((S/'manifest.json').read_text());lib=u.EditorAssetLibrary;at=u.AssetToolsHelpers.get_asset_tools();materials={}
for name,row in cfg['materials'].items():
 m=lib.load_asset(D+'/Materials/'+name) or at.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(m);c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector,0,0);c.set_editor_property('constant',u.LinearColor(*row['color']));u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
 for field,prop in [('metallic',u.MaterialProperty.MP_METALLIC),('roughness',u.MaterialProperty.MP_ROUGHNESS)]:
  c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant,0,0);c.set_editor_property('r',row[field]);u.MaterialEditingLibrary.connect_material_property(c,'',prop)
 u.MaterialEditingLibrary.recompile_material(m);lib.save_loaded_asset(m);materials[name]=m
for row in cfg['meshes']:
 name=row['mesh'];t=u.AssetImportTask();t.set_editor_property('filename',str(S/(name+'.fbx')));t.set_editor_property('destination_path',D+'/TaskProps');t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',True);o=u.FbxImportUI();o.set_editor_property('import_materials',False);o.set_editor_property('import_textures',False);o.set_editor_property('import_as_skeletal',False);o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH);o.get_editor_property('static_mesh_import_data').set_editor_property('combine_meshes',True);t.set_editor_property('options',o);at.import_asset_tasks([t]);m=lib.load_asset(D+'/TaskProps/'+name)
 for i,n in enumerate(row['materials']):m.set_material(i,materials[n])
 lib.save_loaded_asset(m)
print('HOUSE_TASK_PROPS_IMPORTED')
