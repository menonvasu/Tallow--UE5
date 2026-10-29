"""Import the cellar's chandler props (SourceAssets/FinalPass/Env/Cellar, from build_cellar_props_blender.py) into
/Game/FinalPass/Env/Meshes and give every slot its library surface. Re-runnable (replaces).

  run.sh headless import_cellar_meshes.py cellar-meshes
None of them carries collision: they hang overhead, sit on shelves or lie flat on the floor, so navigation never
sees them. Report: <project>/Review/Results/cellar-meshes.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import MAT, MESH

SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env' / 'Cellar'
OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools(); SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
SLOTS = {'Wood': 'MI_Env_Lib_DarkWood', 'Wax': 'MI_Env_Lib_Tallow', 'Twine': 'MI_Env_Lib_DustSheet', 'Iron': 'MI_Env_Lib_Iron',
         'Coal': 'MI_Env_Lib_Coal', 'Cloth': 'MI_Env_Lib_DustSheet'}
TIN = {'SM_Env_B_CandleMould', 'SM_Env_B_CandleBox'}          # tinplate, not cast iron
MESHES = ['SM_Env_B_CandleRack', 'SM_Env_B_CandleMould', 'SM_Env_B_TallowCakes', 'SM_Env_B_WickSpool', 'SM_Env_B_CandleBundle',
          'SM_Env_B_CandleBox', 'SM_Env_B_CoalSpill', 'SM_Env_B_Airer']
R = {'errors': [], 'meshes': {}}


def main():
    yield 1
    fbx = u.FbxImportUI(); fbx.import_materials = False; fbx.import_textures = False; fbx.import_as_skeletal = False
    fbx.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    d = fbx.static_mesh_import_data
    d.combine_meshes = True; d.import_uniform_scale = 1.0; d.auto_generate_collision = False
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    for name in MESHES:
        t = u.AssetImportTask(); t.filename = str(SRC / (name + '.fbx')); t.destination_path = MESH; t.automated = True
        t.replace_existing = True; t.save = False; t.options = fbx
        AT.import_asset_tasks([t])
        mesh = u.load_asset(f'{MESH}/{name}')
        if not mesh: R['errors'].append('import ' + name); continue
        for i, sm in enumerate(mesh.static_materials):
            key = str(sm.material_slot_name).split('.')[0]
            mi = 'MI_Env_Lib_Tin' if key == 'Iron' and name in TIN else SLOTS.get(key)
            if mi: mesh.set_material(i, u.load_asset(f'{MAT}/{mi}'))
            else: R['errors'].append(f'{name}: no surface for slot {key}')
        SME.remove_collisions(mesh)
        E.save_loaded_asset(mesh, False)
        b = mesh.get_bounding_box()
        R['meshes'][name] = {'min': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)], 'max': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
                             'slots': [str(s.material_slot_name) for s in mesh.static_materials], 'tris': mesh.get_num_triangles(0)}
        yield 0.5
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cellar-meshes.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_MESHES ' + json.dumps(R))
