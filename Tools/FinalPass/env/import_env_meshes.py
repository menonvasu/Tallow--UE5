"""Import the parlour's prepared meshes (SourceAssets/FinalPass/Env/Export, from prepare_props_blender.py and
build_hearth_blender.py) into /Game/FinalPass/Env/Meshes and give every slot its instance. Re-runnable (replaces).

  run.sh headless import_env_meshes.py meshes
Collision: only the fire-irons get one (a simple box, so Nell cannot walk through them); the chimney breast is drawn
over the original actor, whose blocking collision stays and keeps navigation as it was.
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import MAT, MESH, TEX

SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env' / 'Export'
OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools(); SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
MESHES = {   # mesh: ({slot name: instance}, collision box)
    'SM_Env_G1_ChimneyBreast': ({'Wall': 'MI_Env_G1_Chimney', 'Firebox': 'MI_Env_G1_Firebox'}, False),
    'SM_Env_G1_FireGrate': ({'Iron': 'MI_Env_Lib_Iron'}, False),
    'SM_Env_G1_Coal': ({'Coal': 'MI_Env_Lib_Coal', 'Ember': 'MI_Env_G1_Ember'}, False),
    'SM_Env_G1_Curtains': ({'Cloth': 'MI_Env_Lib_Damask', 'Brass': 'MI_Env_Lib_Brass'}, False),
    'SM_Env_G1_FireTools': ({'Src_broom': 'MI_Env_Lib_Ebony', 'Src_Rough_Iron_Steel': 'MI_Env_Lib_Iron', 'Src_Wood': 'MI_Env_Lib_DarkWood'}, True),
    # BlenderKit pieces with their own maps (prepare_props_blender.py bake=True): those slots get MI_Env_P_<SM>_<slot>.
    'SM_Env_G5_Cooker': ({}, False),
    'SM_Env_G4_StudentLamp': ({'Src_Victorian_Double_Student_Lamp_Glass': 'MI_Env_Lib_Porcelain'}, False),
}
R = {'errors': [], 'meshes': {}}
L = u.MaterialEditingLibrary


def baked_instances(name):
    """The prepared mesh's own maps -> textures under Textures/Props and one M_Env_Prop instance per slot."""
    info = SRC / (name + '.json')
    maps = json.loads(info.read_text()).get('maps', {}) if info.exists() else {}
    out = {}
    for slot_name, row in maps.items():
        tex = {}
        for key, kind in (('D', 'Base'), ('ARM', 'ARM'), ('N', 'Norm')):
            if not row.get(key): continue
            t = u.AssetImportTask(); t.filename = str(SRC / row[key]); t.destination_path = TEX + '/Props'; t.automated = True
            t.replace_existing = True; t.save = False
            AT.import_asset_tasks([t])
            a = u.load_asset(f"{TEX}/Props/{Path(row[key]).stem}")
            if key == 'N':
                a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
            elif key == 'ARM':
                a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
            E.save_loaded_asset(a, False); tex[kind] = a
        mi_name = f"MI_Env_P_{name.replace('SM_Env_', '')}_{row['slot']}"
        path = f'{MAT}/{mi_name}'
        mi = u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(mi_name, MAT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        L.set_material_instance_parent(mi, u.load_asset(f'{MAT}/M_Env_Prop'))
        for k, a in tex.items(): L.set_material_instance_texture_parameter_value(mi, k, a)
        L.update_material_instance(mi); E.save_loaded_asset(mi, False)
        out[slot_name] = mi
    return out


def main():
    yield 1
    fbx = u.FbxImportUI(); fbx.import_materials = False; fbx.import_textures = False; fbx.import_as_skeletal = False
    fbx.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    d = fbx.static_mesh_import_data
    d.combine_meshes = True; d.import_uniform_scale = 1.0; d.auto_generate_collision = False
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    for name, (slots, box) in MESHES.items():
        t = u.AssetImportTask(); t.filename = str(SRC / (name + '.fbx')); t.destination_path = MESH; t.automated = True
        t.replace_existing = True; t.save = False; t.options = fbx
        AT.import_asset_tasks([t])
        mesh = u.load_asset(f'{MESH}/{name}')
        if not mesh: R['errors'].append('import ' + name); continue
        baked = baked_instances(name)
        for i, sm in enumerate(mesh.static_materials):
            key = str(sm.material_slot_name)
            mi = slots.get(key)
            if mi: mesh.set_material(i, u.load_asset(f'{MAT}/{mi}'))
            elif key in baked: mesh.set_material(i, baked[key])
            else: R['errors'].append(f'{name}: no instance for slot {key}')
        SME.remove_collisions(mesh)
        if box: SME.add_simple_collisions(mesh, u.ScriptCollisionShapeType.BOX)
        E.save_loaded_asset(mesh, False)
        b = mesh.get_bounding_box()
        R['meshes'][name] = {'min': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)], 'max': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
                             'slots': [str(s.material_slot_name) for s in mesh.static_materials], 'tris': mesh.get_num_triangles(0)}
        yield 0.5
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'env-meshes.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_MESHES ' + json.dumps(R))
