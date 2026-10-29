"""Import the simulated cloth pieces (SourceAssets/FinalPass/Env/Cloth, from build_cloth_blender.py) into
/Game/FinalPass/Cloth/Meshes with their vertex colours, and give every slot its finish. Re-runnable (replaces).

  run.sh headless import_cloth_meshes.py cloth-meshes
None of them carries collision: each is drawn over the old piece, which stays as the invisible blocking shape
(apply_cloth.py), or hangs from the ceiling. Report: <project>/Review/Results/cloth-meshes.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import MAT

SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env' / 'Cloth'
OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DEST = '/Game/FinalPass/Cloth/Meshes'
CMAT = '/Game/FinalPass/Cloth/Materials'
E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools(); SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
SLOTS = {   # mesh: {slot: material}
    'SM_Cloth_SheetedForm': {'Cloth': f'{CMAT}/MI_Cloth_DustSheet', 'Form': f'{MAT}/MI_Env_Lib_DustSheet', 'Iron': f'{MAT}/MI_Env_Lib_Iron',
                             'Wood': f'{MAT}/MI_Env_Lib_OldOak', 'Brass': f'{MAT}/MI_Env_Lib_Brass'},
    'SM_Cloth_SheetedChair': {'Cloth': f'{CMAT}/MI_Cloth_DustSheet', 'Wood': f'{MAT}/MI_Env_Lib_OldOak'},
    'SM_Cloth_Airer': {'Wood': f'{MAT}/MI_Env_Lib_DarkWood', 'Cloth': f'{CMAT}/MI_Cloth_Laundry', 'Twine': f'{MAT}/MI_Env_Lib_DustSheet',
                       'Iron': f'{MAT}/MI_Env_Lib_Iron'},
}
R = {'errors': [], 'meshes': {}}


def main():
    yield 1
    fbx = u.FbxImportUI(); fbx.import_materials = False; fbx.import_textures = False; fbx.import_as_skeletal = False
    fbx.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    d = fbx.static_mesh_import_data
    d.combine_meshes = True; d.import_uniform_scale = 1.0; d.auto_generate_collision = False
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('vertex_color_import_option', u.VertexColorImportOption.REPLACE)
    for name, slots in SLOTS.items():
        t = u.AssetImportTask(); t.filename = str(SRC / (name + '.fbx')); t.destination_path = DEST; t.automated = True
        t.replace_existing = True; t.save = False; t.options = fbx
        AT.import_asset_tasks([t])
        mesh = u.load_asset(f'{DEST}/{name}')
        if not mesh: R['errors'].append('import ' + name); continue
        for i, sm in enumerate(mesh.static_materials):
            key = str(sm.material_slot_name).split('.')[0]
            if key in slots: mesh.set_material(i, u.load_asset(slots[key]))
            else: R['errors'].append(f'{name}: no finish for slot {key}')
        SME.remove_collisions(mesh)
        E.save_loaded_asset(mesh, False)
        b = mesh.get_bounding_box()
        R['meshes'][name] = {'min': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)], 'max': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
                             'slots': [str(s.material_slot_name) for s in mesh.static_materials], 'tris': mesh.get_num_triangles(0)}
        yield 0.5
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cloth-meshes.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_CLOTH_MESHES ' + json.dumps(R))
