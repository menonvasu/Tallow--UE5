"""Import the upstairs replacement meshes (SourceAssets/FinalPass/Env/Export/SM_Env_U*, from prepare_upper_blender.py)
into /Game/FinalPass/Env/Upper, with their own baked maps as M_Env_Prop instances, and build the night glow behind
the front windows' boards (M_Env_Upper_NightGlow / MI_Env_Upper_NightGlow). Re-runnable (replaces).

  run.sh headless import_upper_meshes.py upper-meshes
Each mesh gets one simple box collision (so Nell cannot walk into the parts the old mesh did not cover); the old
actor stays as invisible blocking collision under it (apply_upper.py).
Report: <project>/Review/Results/upper-meshes.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import MAT

SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env' / 'Export'
OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DEST = '/Game/FinalPass/Env/Upper'
E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools(); SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
L = u.MaterialEditingLibrary
MESHES = {   # mesh: tuning of its own maps on M_Env_Prop (Tint rgb+saturation, Rough mul/add/metal/AO)
    'SM_Env_U1_RockingHorse': dict(Tint=(0.92, 0.86, 0.8, 0.9), Rough=(1.0, 0.02, 0, 0.8)),
}
R = {'errors': [], 'meshes': {}}
# The night outside a boarded window, seen only through the gaps: unlit, two-sided, a dim cold grey (the street and
# the moon beyond the glass). It casts no light; the boards stay silhouettes.
GLOW = (0.5, 0.6, 0.75, 0.05)


def build_night_glow():
    path = f'{DEST}/Materials/M_Env_Upper_NightGlow'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Env_Upper_NightGlow', DEST + '/Materials', u.Material, u.MaterialFactoryNew())
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    glow = L.create_material_expression(m, u.MaterialExpressionVectorParameter, -600, 0)
    glow.set_editor_property('parameter_name', 'Glow'); glow.set_editor_property('default_value', u.LinearColor(*GLOW))
    mul = L.create_material_expression(m, u.MaterialExpressionMultiply, -300, 0)
    L.connect_material_expressions(glow, 'RGB', mul, 'A'); L.connect_material_expressions(glow, 'A', mul, 'B')
    L.connect_material_property(mul, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    L.recompile_material(m); E.save_loaded_asset(m, False)
    ipath = f'{DEST}/Materials/MI_Env_Upper_NightGlow'
    mi = u.load_asset(ipath) if E.does_asset_exist(ipath) else AT.create_asset('MI_Env_Upper_NightGlow', DEST + '/Materials', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    L.set_material_instance_parent(mi, m)
    L.set_material_instance_vector_parameter_value(mi, 'Glow', u.LinearColor(*GLOW))
    L.update_material_instance(mi); E.save_loaded_asset(mi, False)
    R['glow'] = ipath


def texture(path, kind):
    t = u.AssetImportTask(); t.filename = str(path); t.destination_path = DEST + '/Textures'; t.automated = True
    t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t])
    a = u.load_asset(f'{DEST}/Textures/{path.stem}')
    if kind == 'N':
        a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
    elif kind == 'ARM':
        a.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
    else:
        a.set_editor_property('srgb', True)
    a.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if kind == 'N' else u.TextureGroup.TEXTUREGROUP_WORLD)
    E.save_loaded_asset(a, False)
    return a


def main():
    yield 1
    fbx = u.FbxImportUI(); fbx.import_materials = False; fbx.import_textures = False; fbx.import_as_skeletal = False
    fbx.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    d = fbx.static_mesh_import_data
    d.combine_meshes = True; d.import_uniform_scale = 1.0; d.auto_generate_collision = False
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    build_night_glow(); yield 0.5
    prop = u.load_asset(f'{MAT}/M_Env_Prop')
    for name, tune in MESHES.items():
        t = u.AssetImportTask(); t.filename = str(SRC / (name + '.fbx')); t.destination_path = DEST + '/Meshes'; t.automated = True
        t.replace_existing = True; t.save = False; t.options = fbx
        AT.import_asset_tasks([t])
        mesh = u.load_asset(f'{DEST}/Meshes/{name}')
        if not mesh: R['errors'].append('import ' + name); continue
        info = json.loads((SRC / (name + '.json')).read_text())
        for slot_name, row in info.get('maps', {}).items():
            mi_name = f"MI_Env_{name.replace('SM_Env_', '')}_{row['slot']}"
            path = f'{DEST}/Materials/{mi_name}'
            mi = u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(mi_name, DEST + '/Materials', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
            L.set_material_instance_parent(mi, prop)
            for key, param in (('D', 'Base'), ('ARM', 'ARM'), ('N', 'Norm')):
                if row.get(key): L.set_material_instance_texture_parameter_value(mi, param, texture(SRC / row[key], key))
            for k, v in tune.items():
                L.set_material_instance_vector_parameter_value(mi, k, u.LinearColor(*[float(x) for x in v]))
            L.update_material_instance(mi); E.save_loaded_asset(mi, False)
            mesh.set_material(row['slot'], mi)
        SME.remove_collisions(mesh)
        SME.add_simple_collisions(mesh, u.ScriptCollisionShapeType.BOX)
        E.save_loaded_asset(mesh, False)
        b = mesh.get_bounding_box()
        R['meshes'][name] = {'min': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)], 'max': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
                             'slots': [str(s.material_slot_name) for s in mesh.static_materials], 'tris': mesh.get_num_triangles(0)}
        yield 0.5
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'upper-meshes.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_UPPER_MESHES ' + json.dumps(R))
