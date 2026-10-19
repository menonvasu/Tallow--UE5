"""Import the V09 Keeper (repaired body + new maw) beside V07 on the SAME skeleton, with real maw materials.
Slots keep V07's names: the body keeps M_KeeperV07_Surface; mouth and teeth get V09 materials from our baked maps
(one shared 2048 atlas; OpenGL normal, so green is flipped). Never replaces earlier meshes.
"""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'Tallow/SourceAssets/Keeper/V09Mouth'
DEST='/Game/Presentation/KeeperRiggedV07';TEX='/Game/Presentation/KeeperV09'
NAME='SK_KeeperRiggedV07_Mouth09'
E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();MEL=u.MaterialEditingLibrary

def texture(kind):
    name='T_KeeperMouthV09_'+kind
    if not E.does_asset_exist(TEX+'/'+name):
        t=u.AssetImportTask();t.filename=str(SRC/'Textures'/(name+'.png'));t.destination_path=TEX;t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
        AT.import_asset_tasks([t])
    t=u.load_asset(TEX+'/'+name);assert t,name
    if kind=='Normal':t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);t.set_editor_property('srgb',False);t.set_editor_property('flip_green_channel',True)
    elif kind in ('Roughness','AO'):t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS);t.set_editor_property('srgb',False)
    E.save_loaded_asset(t,False);return t

def material(name,flesh):
    path=TEX+'/'+name
    m=u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(name,TEX,u.Material,u.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    for flag in ('two_sided','used_with_skeletal_mesh','used_with_morph_targets'):m.set_editor_property(flag,True)
    def sample(kind,y,sampler=None):
        n=MEL.create_material_expression(m,u.MaterialExpressionTextureSample,-500,y);n.texture=texture(kind)
        if sampler:n.sampler_type=sampler
        return n
    base=sample('BaseColor',0)
    if flesh:
        # Darker than the bake: in the dark house the eye adapts up and raw gums read salmon otherwise.
        dim=MEL.create_material_expression(m,u.MaterialExpressionMultiply,-250,0);MEL.connect_material_expressions(base,'RGB',dim,'A')
        k=MEL.create_material_expression(m,u.MaterialExpressionConstant,-400,-80);k.set_editor_property('r',.5);MEL.connect_material_expressions(k,'',dim,'B')
        MEL.connect_material_property(dim,'',u.MaterialProperty.MP_BASE_COLOR)
    else:MEL.connect_material_property(base,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(sample('Roughness',200,u.MaterialSamplerType.SAMPLERTYPE_MASKS),'R',u.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(sample('Normal',400,u.MaterialSamplerType.SAMPLERTYPE_NORMAL),'RGB',u.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(sample('AO',600,u.MaterialSamplerType.SAMPLERTYPE_MASKS),'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.recompile_material(m);E.save_loaded_asset(m,False);return m

def main():
    base=u.load_asset(DEST+'/SK_KeeperRiggedV07');skeleton=base.get_editor_property('skeleton')
    if not E.does_asset_exist(DEST+'/'+NAME):
        options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_as_skeletal=True;options.import_mesh=True;options.import_animations=False
        options.import_materials=False;options.import_textures=False;options.create_physics_asset=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;options.skeleton=skeleton
        data=options.skeletal_mesh_import_data;data.set_editor_property('import_morph_targets',True);data.set_editor_property('import_uniform_scale',100.0)
        task=u.AssetImportTask();task.filename=str(SRC/'Export/SK_KeeperMouthV09.fbx');task.destination_path=DEST;task.destination_name=NAME;task.automated=True;task.save=True;task.replace_existing=False;task.options=options;task.factory=u.FbxFactory()
        flag='Interchange.FeatureFlags.Import.FBX';previous=u.SystemLibrary.get_console_variable_int_value(flag)
        try:u.SystemLibrary.execute_console_command(None,flag+' 0');AT.import_asset_tasks([task])
        finally:u.SystemLibrary.execute_console_command(None,flag+' '+str(previous))
    mesh=u.load_asset(DEST+'/'+NAME);assert mesh and mesh.get_editor_property('skeleton')==skeleton,'Import failed or wrong skeleton'
    height=float(mesh.get_imported_bounds().box_extent.z)*2;assert 220<=height<=260,height
    morphs=[x.get_name().lower() for x in mesh.get_editor_property('morph_targets')];assert {'full_horror','hood_open','chomp'}<=set(morphs),morphs
    surface=u.load_asset(DEST+'/M_KeeperV07_Surface');surface.set_editor_property('used_with_morph_targets',True);E.save_loaded_asset(surface,False)
    chosen={'mouth':material('M_KeeperV09_Mouth',True),'teeth':material('M_KeeperV09_Teeth',False)}
    slots=mesh.get_editor_property('materials');assert len(slots)==3,len(slots)
    for i in range(3):
        slot=slots[i];key=str(slot.get_editor_property('imported_material_slot_name')).lower()
        slot.set_editor_property('material_interface',next((m for k,m in chosen.items() if k in key),surface));slots[i]=slot
    mesh.set_editor_property('materials',slots);mesh.set_editor_property('physics_asset',None);assert E.save_loaded_asset(mesh,False)
    report={'mesh':mesh.get_path_name(),'height_cm':height,'morphs':morphs,'slots':[str(s.get_editor_property('imported_material_slot_name'))+'='+s.get_editor_property('material_interface').get_name() for s in mesh.get_editor_property('materials')]}
    u.log('TALLOW_KEEPER_V09_IMPORTED '+json.dumps(report))

main()
