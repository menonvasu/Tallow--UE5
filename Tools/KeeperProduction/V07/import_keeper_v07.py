"""Idempotent NEW-namespace import only. Never changes gameplay classes/maps or V06.
Legacy FBX import scale100 is required by measured export contract; verify240cm.
Existing complete assets are audited, never silently replaced. A partial import
can resume by importing absent assets only; invalid existing assets fail closed.
"""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07'
DEST='/Game/Presentation/KeeperRiggedV07'
E=u.EditorAssetLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()
CONTRACT=json.loads((SOURCE/'animation-contract.json').read_text())

def path(name):return DEST+'/'+name

def save(asset):
    assert asset and asset.get_path_name().startswith(DEST+'/'), 'External or absent asset forbidden'
    assert E.save_loaded_asset(asset,False)
    package=asset.get_path_name().split('.')[0]
    assert (Path(u.Paths.project_content_dir())/(package[len('/Game/'):]+'.uasset')).is_file()

def load_or_import(file,name,options=None):
    existing=u.load_asset(path(name)) if E.does_asset_exist(path(name)) else None
    if existing:return existing
    assert file.is_file(), 'Missing source: '+str(file)
    task=u.AssetImportTask();task.filename=str(file);task.destination_path=DEST;task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False
    if options:task.options=options;task.factory=u.FbxFactory()
    flag='Interchange.FeatureFlags.Import.FBX';previous=u.SystemLibrary.get_console_variable_int_value(flag)
    try:
        u.SystemLibrary.execute_console_command(None,flag+' 0')
        assert u.SystemLibrary.get_console_variable_int_value(flag)==0
        ASSETS.import_asset_tasks([task])
    finally:u.SystemLibrary.execute_console_command(None,flag+' '+str(previous))
    asset=u.load_asset(path(name));assert asset,'Import failed: '+str(file);return asset

def options_for(skeleton=None):
    options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_as_skeletal=True;options.import_materials=False;options.import_textures=False;options.create_physics_asset=False
    options.import_mesh=skeleton is None;options.import_animations=skeleton is not None
    options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH if skeleton is None else u.FBXImportType.FBXIT_ANIMATION
    if skeleton is None:
        data=options.skeletal_mesh_import_data;data.set_editor_property('import_morph_targets',True)
    else:
        options.skeleton=skeleton;data=options.anim_sequence_import_data
    data.set_editor_property('import_uniform_scale',100.0)
    return options

def constant_material(name,color,roughness):
    material=u.load_asset(path(name)) if E.does_asset_exist(path(name)) else None
    if material:return material
    material=ASSETS.create_asset(name,DEST,u.Material,u.MaterialFactoryNew());material.set_editor_property('used_with_skeletal_mesh',True);material.set_editor_property('two_sided',True)
    node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionConstant3Vector,-300,0);node.set_editor_property('constant',u.LinearColor(*color,1));u.MaterialEditingLibrary.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionConstant,-300,150);rough.set_editor_property('r',roughness);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.recompile_material(material);save(material);return material

def main():
    assert len(list((SOURCE/'Export').glob('AN_Keeper_*.fbx')))==len(CONTRACT['clips'])==20
    mesh=load_or_import(SOURCE/'Export/SK_KeeperRiggedV07.fbx','SK_KeeperRiggedV07',options_for())
    height=float(mesh.get_imported_bounds().box_extent.z)*2
    assert 220<=height<=260,'Unexpected imported height cm: '+str(height)
    data=mesh.get_editor_property('asset_import_data')
    assert isinstance(data,u.FbxSkeletalMeshImportData) and abs(data.get_editor_property('import_uniform_scale')-100)<.001,'Wrong persisted mesh import metadata'
    assert data.get_editor_property('import_morph_targets'),'Morph setting absent'
    morphs=[m.get_name().lower() for m in mesh.get_editor_property('morph_targets')]
    assert 'full_horror' in morphs and 'hood_open' in morphs,'Missing mantle or hood morph: '+repr(morphs)
    skeleton=mesh.get_editor_property('skeleton');save(skeleton);mesh.set_editor_property('physics_asset',None);save(mesh)
    clips={}
    for clip in CONTRACT['clips']:
        name='AN_Keeper_'+clip['name'];asset=load_or_import(SOURCE/'Export'/(name+'.fbx'),name,options_for(skeleton));bound=asset.get_editor_property('skeleton')
        assert bound==skeleton,'Clip skeleton mismatch: '+name
        data=asset.get_editor_property('asset_import_data');assert isinstance(data,u.FbxAnimSequenceImportData) and abs(data.get_editor_property('import_uniform_scale')-100)<.001,'Clip scale mismatch: '+name
        save(asset);clips[clip['name']]=asset.get_path_name()
    textures={}
    for suffix in ['BaseColor','Roughness','Normal','Metallic']:
        tex=load_or_import(SOURCE/'Textures'/('T_KeeperTripo_'+suffix+'.png'),'T_KeeperV07_'+suffix)
        tex.set_editor_property('srgb',suffix=='BaseColor')
        if suffix=='Normal':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('flip_green_channel',True)
        elif suffix in ['Roughness','Metallic']:tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        save(tex);textures[suffix]=tex
    material=u.load_asset(path('M_KeeperV07_Surface')) if E.does_asset_exist(path('M_KeeperV07_Surface')) else None
    if not material:
        material=ASSETS.create_asset('M_KeeperV07_Surface',DEST,u.Material,u.MaterialFactoryNew());material.set_editor_property('used_with_skeletal_mesh',True);material.set_editor_property('two_sided',True)
        for row,(suffix,prop) in enumerate([('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Roughness',u.MaterialProperty.MP_ROUGHNESS),('Normal',u.MaterialProperty.MP_NORMAL),('Metallic',u.MaterialProperty.MP_METALLIC)]):
            node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionTextureSample,-400,row*180);node.texture=textures[suffix]
            if suffix=='Normal':node.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL
            elif suffix in ['Roughness','Metallic']:node.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS
            u.MaterialEditingLibrary.connect_material_property(node,'R' if suffix in ['Roughness','Metallic'] else 'RGB',prop)
        u.MaterialEditingLibrary.recompile_material(material);save(material)
    mouth=constant_material('M_KeeperV07_Mouth',(.007,.001,.002),.88);teeth=constant_material('M_KeeperV07_Teeth',(.30,.23,.13),.85)
    slots=mesh.get_editor_property('materials');assert len(slots)==3,'Unexpected source material slots: '+str(len(slots))
    for i in range(len(slots)):
        slot=slots[i];slot_name=str(slot.get_editor_property('imported_material_slot_name')).lower()
        chosen=mouth if 'mouth' in slot_name else teeth if 'teeth' in slot_name else material
        slot.set_editor_property('material_interface',chosen);slots[i]=slot
    mesh.set_editor_property('materials',slots);save(mesh);save(skeleton);assert E.save_directory(DEST,False,True)
    materials=[s.get_editor_property('material_interface').get_path_name() for s in mesh.get_editor_property('materials')]
    assert set(materials)=={material.get_path_name(),mouth.get_path_name(),teeth.get_path_name()},'Incorrect material slot assignment: '+repr(materials)
    report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'height_cm':height,'morphs':morphs,'clips':clips,'material_paths':materials,'physics':None,'status':'Saved staged assets; fresh-process render and gameplay route acceptance still required','forward':'+Y','mesh_relative_yaw_for_X_actor':-90,'mesh_scale':1,'head_bone':'head'}
    output=ROOT/'Tallow/Saved/Presentation/keeper-v07-import.json';output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n');u.log('KeeperV07 import '+json.dumps(report))

main()
