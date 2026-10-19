"""Import the split-seam Keeper mesh beside V07 on the SAME skeleton, so all V07 clips still drive it.
The coincident seam was welded by the FBX importer; Revision02 opens a 1mm seam (in the mesh vertices, not only the shape keys) so HOOD_OPEN parts cleanly.
Never replaces SK_KeeperRiggedV07; configure_playable.py chooses which mesh the Keeper wears.
"""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'Tallow/SourceAssets/Keeper/V07RevealRevision02/Export/SK_KeeperRevealRevision02.fbx'
DEST='/Game/Presentation/KeeperRiggedV07'
NAME='SK_KeeperRiggedV07_Reveal02'
E=u.EditorAssetLibrary
ASSETS=u.AssetToolsHelpers.get_asset_tools()

def path(name):return DEST+'/'+name

def main():
    base=u.load_asset(path('SK_KeeperRiggedV07'));assert base,'V07 base mesh missing'
    skeleton=base.get_editor_property('skeleton')
    if not E.does_asset_exist(path(NAME)):
        assert SOURCE.is_file(),'Missing source: '+str(SOURCE)
        options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_as_skeletal=True;options.import_mesh=True;options.import_animations=False
        options.import_materials=False;options.import_textures=False;options.create_physics_asset=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;options.skeleton=skeleton
        data=options.skeletal_mesh_import_data;data.set_editor_property('import_morph_targets',True);data.set_editor_property('import_uniform_scale',100.0)
        task=u.AssetImportTask();task.filename=str(SOURCE);task.destination_path=DEST;task.destination_name=NAME;task.automated=True;task.save=True;task.replace_existing=False;task.options=options;task.factory=u.FbxFactory()
        flag='Interchange.FeatureFlags.Import.FBX';previous=u.SystemLibrary.get_console_variable_int_value(flag)
        try:
            u.SystemLibrary.execute_console_command(None,flag+' 0');ASSETS.import_asset_tasks([task])
        finally:u.SystemLibrary.execute_console_command(None,flag+' '+str(previous))
    mesh=u.load_asset(path(NAME));assert mesh,'Import failed'
    assert mesh.get_editor_property('skeleton')==skeleton,'Revision is not on the V07 skeleton'
    height=float(mesh.get_imported_bounds().box_extent.z)*2;assert 220<=height<=260,'Unexpected imported height cm: '+str(height)
    morphs=[m.get_name().lower() for m in mesh.get_editor_property('morph_targets')]
    assert 'full_horror' in morphs and 'hood_open' in morphs,'Missing mantle or hood morph: '+repr(morphs)
    # Same three slots as V07: surface, mouth, teeth.
    by_name={str(s.get_editor_property('imported_material_slot_name')).lower():s.get_editor_property('material_interface') for s in base.get_editor_property('materials')}
    slots=mesh.get_editor_property('materials');assert len(slots)==3,'Unexpected material slots: '+str(len(slots))
    for i in range(len(slots)):
        slot=slots[i];key=str(slot.get_editor_property('imported_material_slot_name')).lower();assert key in by_name,'Unknown slot '+key
        slot.set_editor_property('material_interface',by_name[key]);slots[i]=slot
    mesh.set_editor_property('materials',slots);mesh.set_editor_property('physics_asset',None)
    assert E.save_loaded_asset(mesh,False)
    report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'height_cm':height,'morphs':morphs}
    out=ROOT/'Tallow/Saved/Presentation/keeper-reveal-revision02-import.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n')
    u.log('TALLOW_KEEPER_REVEAL02_IMPORTED '+json.dumps(report))

main()
