"""Lead-run Unreal import of NEW isolated attic-kit assets. Never loads/saves maps."""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SRC=ROOT/'Tallow/SourceAssets/Production/AtticKit';DEST='/Game/Presentation/Environment/AtticKit'
E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
names=[r['name'] for r in json.loads((SRC/'geometry-manifest.json').read_text())]
files=[n+'.fbx' for n in names]+['T_AtticKit_BaseColor.png','T_AtticKit_Roughness.png']
assert all(not E.does_asset_exist(DEST+'/'+Path(f).stem) for f in files) and not E.does_asset_exist(DEST+'/M_AtticKit_SageTimber'),'Destination already exists; explicit versioned update required'
for f in files:assert (SRC/f).is_file() and not (SRC/f).read_bytes()[:80].startswith(b'version https://git-lfs'),f
jobs=[]
for f in files:
 t=u.AssetImportTask();t.filename=str(SRC/f);t.destination_path=DEST;t.destination_name=Path(f).stem;t.automated=True;t.save=True;t.replace_existing=False
 if f.endswith('.fbx'):
  opts=u.FbxImportUI();opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False;t.options=opts
 jobs.append(t)
AT.import_asset_tasks(jobs)
base=u.load_asset(DEST+'/T_AtticKit_BaseColor');rough=u.load_asset(DEST+'/T_AtticKit_Roughness');assert base and rough
base.set_editor_property('srgb',True);rough.set_editor_property('srgb',False);rough.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
mat=AT.create_asset('M_AtticKit_SageTimber',DEST,u.Material,u.MaterialFactoryNew())
for tex,prop,linear in [(base,u.MaterialProperty.MP_BASE_COLOR,False),(rough,u.MaterialProperty.MP_ROUGHNESS,True)]:
 node=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,250 if linear else 0);node.texture=tex
 if linear:node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
 M.connect_material_property(node,'R' if linear else 'RGB',prop)
M.recompile_material(mat)
for name in names:
 mesh=u.load_asset(DEST+'/'+name);assert mesh;mesh.set_material(0,mat);E.save_loaded_asset(mesh,False)
for a in (base,rough,mat):E.save_loaded_asset(a,False)
u.log('TALLOW isolatedAtticKit imported; no map touched')
