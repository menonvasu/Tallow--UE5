"""Give the outro garden the house's scanned surfaces instead of flat colours.
Reuses M_TallowWorldAligned (Poly Haven CC0, credited in SourceAssets/Textures/PolyHaven/CREDITS.json) and adds
brown_mud_leaves_01 for the ground. Only touches HouseOutroV01 assets; the gate keeps its iron finish.
"""
import unreal as u
from pathlib import Path
R=Path(__file__).resolve().parents[2];SRC=R/'Tallow/SourceAssets/Production/HouseOutroV01/Textures'
D='/Game/Presentation/Environment/HouseOutroV01';HOUSE='/Game/Environment/HouseMaterials'
E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();MEL=u.MaterialEditingLibrary

def texture(name):
    path=D+'/Textures/'+name
    if not E.does_asset_exist(path):
        t=u.AssetImportTask();t.filename=str(SRC/(name+'.jpg'));t.destination_path=D+'/Textures';t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
        AT.import_asset_tasks([t])
    t=u.load_asset(path);assert t,path
    if name.endswith('_nor'):
        t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);t.set_editor_property('srgb',False);t.set_editor_property('flip_green_channel',True)  # Poly Haven GL normal
    elif name.endswith('_rough'):
        t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS);t.set_editor_property('srgb',False)
    E.save_loaded_asset(t);return t

def house(tid,kind):return u.load_asset(HOUSE+'/Textures/T_%s_%s'%(tid,kind))

def instance(name,maps,tile,tint):
    path=D+'/Materials/'+name
    mi=u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(name,D+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi,u.load_asset(HOUSE+'/M_TallowWorldAligned'))
    for param,tex in zip(['BaseColorTex','NormalTex','RoughTex'],maps):assert tex,name;MEL.set_material_instance_texture_parameter_value(mi,param,tex)
    MEL.set_material_instance_scalar_parameter_value(mi,'TileSize',float(tile));MEL.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*tint,1))
    E.save_loaded_asset(mi);return mi

mud=instance('MI_Outro_Mud',[texture('T_brown_mud_leaves_01_'+k) for k in ('diff','nor','rough')],170,(.5,.48,.45))
stone=instance('MI_Outro_PathStone',[house('concrete_floor_worn_001',k) for k in ('diff','nor','rough')],110,(.42,.44,.47))
timber=instance('MI_Outro_FenceTimber',[house('weathered_planks',k) for k in ('diff','nor','rough')],80,(.34,.3,.27))
mesh=u.load_asset(D+'/Meshes/SM_Outro_Garden');assert mesh
# Slot order from manifest.json: Earth, WetStone, CharredWood, Iron.
for i,m in enumerate([mud,stone,timber]):mesh.set_material(i,m)
E.save_loaded_asset(mesh);u.log('TALLOW_OUTRO_MATERIALS '+str([s.get_editor_property('material_interface').get_name() for s in mesh.get_editor_property('static_materials')]))
