"""Create opt-in flame assets only. No maps/Blueprints changed; existing destinations cause refusal.

Unreal 5.8 Python: exec(open('/absolute/path/to/create_flame_assets.py').read())
CLI: python create_flame_assets.py --mesh-only (generates only source OBJ)
Requires EditorScriptingUtilities and PythonScriptPlugin for Unreal import.
"""
from pathlib import Path
import math
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tallow/SourceAssets/ArtDirection/Flame/SM_TallowFlame.obj'
DEST = '/Game/Presentation/Flame'


def flame_geometry(rings=32, sides=24):
    """Unit Z-up mesh with single wick vertex at origin; UV height runs 0..1."""
    vertices = [(0.0, 0.0, 0.0)]
    uv = [(0.5, 0.0)]
    for ring in range(1, rings):
        t = ring / rings
        # Broad lower body, long narrowing tip. Max radius <= .5.
        radius = .5 * math.sin(math.pi * t) ** .7 * (1.0 - .65 * t)
        for side in range(sides):
            angle = 2.0 * math.pi * side / sides
            vertices.append((radius * math.cos(angle), radius * math.sin(angle), t))
            uv.append((side / sides, t))
    top = len(vertices)
    vertices.append((0.0, 0.0, 1.0))
    uv.append((.5, 1.0))
    faces = []
    for side in range(sides):
        nxt = (side + 1) % sides
        faces.append((0, 1 + nxt, 1 + side))
        for ring in range(rings - 2):
            a = 1 + ring * sides + side
            b = 1 + ring * sides + nxt
            c, d = a + sides, b + sides
            faces.extend(((a, b, c), (b, d, c)))
        last = 1 + (rings - 2) * sides
        faces.append((last + side, last + nxt, top))
    return vertices, uv, faces


def write_source():
    if SOURCE.exists():
        raise FileExistsError(f'Refusing to overwrite source: {SOURCE}')
    vertices, uv, faces = flame_geometry()
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    lines = ['# TALLOW unit flame. Z-up; origin is wick; UV V stores height.', 'o TallowFlame', 's 1']
    lines += [f'v {x:.9f} {y:.9f} {z:.9f}' for x, y, z in vertices]
    lines += [f'vt {u:.9f} {v:.9f}' for u, v in uv]
    lines += ['f ' + ' '.join(f'{i+1}/{i+1}' for i in face) for face in faces]
    SOURCE.write_text('\n'.join(lines) + '\n')
    return SOURCE


def create_assets():
    import unreal as u
    names = ['SM_TallowFlame', 'M_TallowFlame']
    for name in names:
        if u.EditorAssetLibrary.does_asset_exist(f'{DEST}/{name}'):
            raise RuntimeError(f'Refusing overwrite: {DEST}/{name}; use a fresh branch/destination')
    if not SOURCE.exists():
        write_source()
    task = u.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DEST
    task.destination_name = names[0]
    task.automated = True
    task.replace_existing = False
    task.save = False
    options = u.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    options.static_mesh_import_data.import_uniform_scale = 1.0
    task.options = options
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = u.load_asset(f'{DEST}/{names[0]}')
    if not isinstance(mesh, u.StaticMesh):
        raise RuntimeError(f'OBJ import failed. Imported paths: {task.imported_object_paths}')
    # Abort on importer unit/axis changes: material depends on exact local Z range.
    box = mesh.get_bounding_box()
    if abs(box.min.z) > .01 or abs(box.max.z - 1.0) > .01:
        raise RuntimeError(f'Expected local Z 0..1 cm. Import bounds: {box}')
    mat = u.AssetToolsHelpers.get_asset_tools().create_asset(names[1], DEST, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', u.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    edit = u.MaterialEditingLibrary

    def node(cls, x, y):
        return edit.create_material_expression(mat, cls, x, y)

    def link(src, dst, pin, output=''):
        if not edit.connect_material_expressions(src, output, dst, pin):
            raise RuntimeError(f'Material connection failed: {pin}')

    def scalar(name, value, y):
        n = node(u.MaterialExpressionScalarParameter, -800, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', value)
        return n

    def custom(code, inputs, x, y):
        n = node(u.MaterialExpressionCustom, x, y)
        n.set_editor_property('code', code)
        n.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
        arr = []
        for name in inputs:
            i = u.CustomInput()
            i.set_editor_property('input_name', name)
            arr.append(i)
        n.set_editor_property('inputs', arr)
        return n

    position = node(u.MaterialExpressionPreSkinnedPosition, -1000, -500)
    deform = custom('float t = saturate(P.z); return float3(P.x*(WidthCm-1)+BendX*t*t, P.y*(WidthCm-1)+BendY*t*t, t*HeightCm-P.z);',
                    ['P', 'HeightCm', 'WidthCm', 'BendX', 'BendY'], -400, -300)
    link(position, deform, 'P')
    for index, (name, default) in enumerate([('HeightCm', 3.0), ('WidthCm', .9), ('BendX', 0.0), ('BendY', 0.0)]):
        link(scalar(name, default, -300 + index * 100), deform, name)
    transform = node(u.MaterialExpressionTransform, -150, -300)
    transform.set_editor_property('transform_source_type', u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    transform.set_editor_property('transform_type', u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(deform, transform, '')
    if not edit.connect_material_property(transform, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        raise RuntimeError('WPO connection failed')
    color = custom('float t=saturate(P.z); float3 warm=lerp(float3(1,0.82,0.35),float3(1,0.12,0.01),t*t); return lerp(float3(0.08,0.14,0.6),warm,smoothstep(0.01,0.18,t))*Emission;',
                   ['P', 'Emission'], -300, 300)
    # PreSkinnedPosition is vertex-only in UE 5.8. Interpolate it explicitly for emissive pixels.
    interpolator = node(u.MaterialExpressionVertexInterpolator, -650, 200)
    link(position, interpolator, '')
    link(interpolator, color, 'P')
    link(scalar('Emission', 4.0, 350), color, 'Emission')
    if not edit.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('Emission connection failed')
    opacity = node(u.MaterialExpressionConstant, -100, 550)
    opacity.set_editor_property('r', .7)
    edit.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY)
    edit.layout_material_expressions(mat)
    edit.recompile_material(mat)
    mesh.set_material(0, mat)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    u.EditorAssetLibrary.save_loaded_asset(mesh)
    u.log('Created opt-in flame assets. No map or Blueprint was modified. Render validation still required.')


if __name__ == '__main__':
    if '--mesh-only' in sys.argv:
        print(write_source())
    else:
        create_assets()
