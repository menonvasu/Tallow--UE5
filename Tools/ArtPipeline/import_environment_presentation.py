"""Run inside Unreal by lead integrator. Imports NEW isolated assets; never loads/saves maps."""
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'Tallow/SourceAssets/Production/CabinetFinished';DEST='/Game/Presentation/Environment/Cabinet'
E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
names=['SM_Cabinet_SageTimber','T_Cabinet_BaseColor','T_Cabinet_Roughness','M_Cabinet_SageTimber']
assert all(not E.does_asset_exist(DEST+'/'+n) for n in names), 'Destination assets already exist: stop for explicit versioned update'
for filename in ['SM_Cabinet_SageTimber.fbx','T_Cabinet_BaseColor.png','T_Cabinet_Roughness.png']:
 assert (SRC/filename).is_file(), filename
 assert not (SRC/filename).read_bytes()[:80].startswith(b'version https://git-lfs'), 'Resolve LFS before import'
tasks=[]
for filename in ['SM_Cabinet_SageTimber.fbx','T_Cabinet_BaseColor.png','T_Cabinet_Roughness.png']:
 t=u.AssetImportTask();t.filename=str(SRC/filename);t.destination_path=DEST;t.destination_name=Path(filename).stem;t.automated=True;t.save=True;t.replace_existing=False
 if filename.endswith('.fbx'):
  options=u.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;t.options=options
 tasks.append(t)
AT.import_asset_tasks(tasks)
base=u.load_asset(DEST+'/T_Cabinet_BaseColor');rough=u.load_asset(DEST+'/T_Cabinet_Roughness');mesh=u.load_asset(DEST+'/SM_Cabinet_SageTimber')
assert base and rough and mesh,'Inspect import log; no map changed'
base.set_editor_property('srgb',True);rough.set_editor_property('srgb',False);rough.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
mat=AT.create_asset('M_Cabinet_SageTimber',DEST,u.Material,u.MaterialFactoryNew())
for texture,prop,linear in [(base,u.MaterialProperty.MP_BASE_COLOR,False),(rough,u.MaterialProperty.MP_ROUGHNESS,True)]:
 node=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,0 if not linear else 250);node.texture=texture
 if linear:node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
 M.connect_material_property(node,'R' if linear else 'RGB',prop)
M.recompile_material(mat);mesh.set_material(0,mat)
for asset in [base,rough,mat,mesh]:E.save_loaded_asset(asset,False)
u.log('TALLOW: new presentation cabinet imported with original2k UV textures; no map/collision/gameplay changes. Inspect scale/material in staging before placement.')
