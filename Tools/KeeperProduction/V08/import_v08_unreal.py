"""Import the V08 Keeper animation library onto the existing V07 skeleton in a NEW folder; never touches V07 clips.
Legacy FBX, uniform scale 100 (the V07 contract). Every clip must bind to SK_KeeperRiggedV07_Skeleton and match
its contract length.
"""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'Tallow/SourceAssets/Keeper/V08Animation'
DEST='/Game/Presentation/KeeperV08'
E=u.EditorAssetLibrary
CONTRACT=json.loads((SOURCE/'animation-contract.json').read_text())

def main():
    skeleton=u.load_asset('/Game/Presentation/KeeperRiggedV07/SK_KeeperRiggedV07_Skeleton');assert skeleton,'V07 skeleton missing'
    tasks=[]
    for clip in CONTRACT['clips']:
        name='AN_Keeper_'+clip['name']
        if E.does_asset_exist(DEST+'/'+name):continue
        options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_as_skeletal=True;options.import_mesh=False;options.import_animations=True
        options.import_materials=False;options.import_textures=False;options.create_physics_asset=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION;options.skeleton=skeleton
        options.anim_sequence_import_data.set_editor_property('import_uniform_scale',100.0)
        task=u.AssetImportTask();task.filename=str(SOURCE/'Export'/(name+'.fbx'));task.destination_path=DEST;task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False;task.options=options;task.factory=u.FbxFactory()
        assert Path(task.filename).is_file(),task.filename;tasks.append(task)
    flag='Interchange.FeatureFlags.Import.FBX';previous=u.SystemLibrary.get_console_variable_int_value(flag)
    try:
        u.SystemLibrary.execute_console_command(None,flag+' 0');u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    finally:u.SystemLibrary.execute_console_command(None,flag+' '+str(previous))
    report={}
    for clip in CONTRACT['clips']:
        asset=u.load_asset(DEST+'/AN_Keeper_'+clip['name']);assert asset,'Import failed: '+clip['name']
        assert asset.get_editor_property('skeleton')==skeleton,'Skeleton mismatch: '+clip['name']
        assert abs(asset.get_play_length()-clip['seconds'])<.06,(clip['name'],asset.get_play_length(),clip['seconds'])
        E.save_loaded_asset(asset,False);report[clip['name']]=round(asset.get_play_length(),3)
    u.log('TALLOW_KEEPER_V08_IMPORTED '+json.dumps(report))

main()
