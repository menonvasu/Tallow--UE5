"""Imports the Poly Haven wooden candlestick (CC0) as the candle prop: held in hand and on the pickup crate.
Material keeps a faint warm self-glow so the wax never reads as a black silhouette. Re-runnable."""
import unreal as u, os
ROOT=os.path.join(u.Paths.project_dir(),"SourceAssets","Props","Candle")
DEST='/Game/Props/Candle'; at=u.AssetToolsHelpers.get_asset_tools(); mel=u.MaterialEditingLibrary
def task(src,opts=None):
    t=u.AssetImportTask(); t.filename=src; t.destination_path=DEST; t.automated=True; t.replace_existing=True; t.save=True
    if opts: t.options=opts
    at.import_asset_tasks([t]); return t.imported_object_paths
u.EditorAssetLibrary.does_asset_exist(DEST+"/wooden_candlestick_1k") and u.EditorAssetLibrary.delete_asset(DEST+"/wooden_candlestick_1k")
tex=u.load_asset(task(os.path.join(ROOT,'textures','wooden_candlestick_diff_1k.jpg'))[0])
o=u.FbxImportUI(); o.import_materials=False; o.import_textures=False; o.import_as_skeletal=False; o.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
o.static_mesh_import_data.combine_meshes=True; o.static_mesh_import_data.import_uniform_scale=1.0
paths=task(os.path.join(ROOT,'wooden_candlestick_1k.fbx'),o); mesh=u.load_asset([p for p in paths if 'candlestick' in p.lower()][0])
def material(name):
    p=DEST+'/'+name
    if u.EditorAssetLibrary.does_asset_exist(p): u.EditorAssetLibrary.delete_asset(p)
    return at.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
m=material('M_Candlestick')
s=mel.create_material_expression(m,u.MaterialExpressionTextureSample,-600,0); s.texture=tex
mel.connect_material_property(s,'RGB',u.MaterialProperty.MP_BASE_COLOR)
r=mel.create_material_expression(m,u.MaterialExpressionConstant,-400,200); r.r=0.7; mel.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS)
g=mel.create_material_expression(m,u.MaterialExpressionMultiply,-300,300); c=mel.create_material_expression(m,u.MaterialExpressionConstant3Vector,-600,300); c.constant=u.LinearColor(0.06,0.03,0.012,1)
mel.connect_material_expressions(s,'RGB',g,'A'); mel.connect_material_expressions(c,'',g,'B'); mel.connect_material_property(g,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
f=material('M_CandleFlame'); f.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT); f.set_editor_property('blend_mode',u.BlendMode.BLEND_ADDITIVE)
fc=mel.create_material_expression(f,u.MaterialExpressionConstant3Vector,-400,0); fc.constant=u.LinearColor(6.0,2.2,0.5,1)
mel.connect_material_property(fc,'',u.MaterialProperty.MP_EMISSIVE_COLOR); mel.recompile_material(f)
mesh.set_material(0,m)
for a in [mesh,m,f]: u.EditorAssetLibrary.save_loaded_asset(a)
b=mesh.get_bounding_box(); u.log_warning('CANDLESTICK %s bounds min=%s max=%s'%(mesh.get_path_name(),b.min,b.max))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
pick=[a for a in eas.get_all_level_actors() if a.get_actor_label()=='M3_CandlePickup'][0]
pm=pick.get_component_by_class(u.StaticMeshComponent); pm.set_static_mesh(mesh); pm.set_material(0,m)
pick.set_actor_scale3d(u.Vector(1,1,1)); pick.set_actor_location(u.Vector(365,-325,-275-b.min.z),False,False)
light=pick.get_component_by_class(u.PointLightComponent); light.set_relative_location(u.Vector(0,0,b.max.z+4),False,False)
assert ed.save_current_level(); u.log_warning('CANDLESTICK pickup swapped')
