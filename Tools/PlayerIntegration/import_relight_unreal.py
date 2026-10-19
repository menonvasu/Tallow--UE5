from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SOURCE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';DEST='/Game/Presentation/PlayerInteractionV05'
E=u.EditorAssetLibrary;A=u.AssetToolsHelpers.get_asset_tools();mesh=u.load_asset(DEST+'/SK_PlayerV05_Full');assert mesh
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

for name,duration in [('RelightMatch',1.2)]:
 asset='AN_PlayerV05_'+name;assert not E.does_asset_exist(DEST+'/'+asset),'Refusing replacement '+asset
 clip=next(x for x in task(SOURCE/(asset+'.fbx'),asset,DEST,opts(mesh.skeleton,animation=True)) if isinstance(x,u.AnimSequence))
 assert abs(clip.get_play_length()-duration)<.04
 assert E.save_loaded_asset(clip,False)
