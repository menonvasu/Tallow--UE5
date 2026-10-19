"""Editor-only fresh namespace import. Root executes; no gameplay assets/maps are touched."""
import json
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];PLAYER=ROOT/'Tallow/SourceAssets/Player/InteractionV05';CANDLE=ROOT/'Tallow/SourceAssets/Production/CandleV02'
PD='/Game/Presentation/PlayerInteractionV05';CD='/Game/Presentation/CandleV02';E=u.EditorAssetLibrary;A=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
report=json.loads((PLAYER/'animation-validation.json').read_text());assert report['bones']==52
for dest in [PD,CD]:assert not E.list_assets(dest,recursive=True,include_folder=False),'Refusing replacement: '+dest

def task(path,name,dest,options=None):
 assert path.is_file(),path
 t=u.AssetImportTask();t.filename=str(path);t.destination_path=dest;t.destination_name=name;t.automated=True;t.replace_existing=False;t.save=True
 if options:t.options=options
 A.import_asset_tasks([t]);assert t.imported_object_paths,path
 return [u.load_asset(p) for p in t.imported_object_paths]
def opts(skeleton=None,animation=False,static=False):
 o=u.FbxImportUI()
 for key,value in {'automated_import_should_detect_type':False,'import_mesh':not animation,'import_as_skeletal':not static,'import_animations':animation,'import_materials':False,'import_textures':False}.items():o.set_editor_property(key,value)
 o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH if static else u.FBXImportType.FBXIT_ANIMATION if animation else u.FBXImportType.FBXIT_SKELETAL_MESH)
 if skeleton:o.set_editor_property('skeleton',skeleton)
 if not animation and not static:o.get_editor_property('skeletal_mesh_import_data').set_editor_property('import_morph_targets',False)
 return o
full=next(x for x in task(PLAYER/'SK_PlayerV05_Full.fbx','SK_PlayerV05_Full',PD,opts()) if isinstance(x,u.SkeletalMesh))
height_cm=full.get_imported_bounds().box_extent.z*2
assert 110<height_cm<130, 'Full player import unit failure: '+str(height_cm)
partial=next(x for x in task(PLAYER/'SK_PlayerV05_Partial.fbx','SK_PlayerV05_Partial',PD,opts(full.skeleton)) if isinstance(x,u.SkeletalMesh))
clip_assets={}
for name in report['clips']:
 clip=next(x for x in task(PLAYER/('AN_PlayerV05_'+name+'.fbx'),'AN_PlayerV05_'+name,PD,opts(full.skeleton,animation=True)) if isinstance(x,u.AnimSequence))
 clip_assets[name]=clip
 assert abs(clip.get_play_length()-report['clips'][name]['seconds'])<.04,(name,clip.get_play_length())
candle=next(x for x in task(CANDLE/'SM_CandleV02_Crooked.fbx','SM_CandleV02_Crooked',CD,opts(static=True)) if isinstance(x,u.StaticMesh))

assert 24<candle.get_bounding_box().max.z-candle.get_bounding_box().min.z<27, 'Candle import unit failure'

def material(dest,name,source,prefix,metal=False,skinned=False):
 mat=A.create_asset(name,dest,u.Material,u.MaterialFactoryNew());assert mat
 if skinned:mat.set_editor_property('used_with_skeletal_mesh',True)
 labels=['BaseColor','Roughness','NormalGL']+(['Metallic'] if metal else [])
 properties={'BaseColor':u.MaterialProperty.MP_BASE_COLOR,'Roughness':u.MaterialProperty.MP_ROUGHNESS,'NormalGL':u.MaterialProperty.MP_NORMAL,'Metallic':u.MaterialProperty.MP_METALLIC}
 for i,label in enumerate(labels):
  name=prefix+label;tx=next(x for x in task(source/(name+'.png'),name,dest) if isinstance(x,u.Texture2D));tx.set_editor_property('srgb',label=='BaseColor')
  if label=='NormalGL':tx.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tx.set_editor_property('flip_green_channel',True)
  elif label!='BaseColor':tx.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
  assert E.save_loaded_asset(tx,False)
  n=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,i*180);n.texture=tx;n.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if label=='NormalGL' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if label=='BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_MASKS
  M.connect_material_property(n,'RGB' if label in ['BaseColor','NormalGL'] else 'R',properties[label])
 M.recompile_material(mat);assert E.save_loaded_asset(mat,False);return mat
pm=material(PD,'M_PlayerV05',PLAYER/'Textures','T_PlayerV05_',skinned=True)
cm=material(CD,'M_CandleV02',CANDLE,'T_CandleV02_',metal=True)
for mesh in [full,partial]:
 slots=mesh.get_editor_property('materials')
 for i in range(len(slots)):
  slot=slots[i];slot.set_editor_property('material_interface',pm);slots[i]=slot
 mesh.set_editor_property('materials',slots)
 assert all(x.get_editor_property('material_interface')==pm for x in mesh.get_editor_property('materials'))
 assert E.save_loaded_asset(mesh,False)
candle.set_material(0,cm);assert E.save_loaded_asset(candle,False)
for dest in [PD,CD]:assert E.save_directory(dest,only_if_is_dirty=False,recursive=True)
u.log('Player V05/Candle V02 imported. Material slots checked. Root must render attachment, transitions and actual gameplay before acceptance.')
