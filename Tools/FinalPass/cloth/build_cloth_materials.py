"""The cloth finish: worn linen laid along the cloth's own UVs (so the weave follows every fold), darkened in the folds by
the occlusion painted into the vertex colour, greyed with dust where it faces up and dirtied at the hem.

  run.sh headless build_cloth_materials.py cloth-materials
Makes /Game/FinalPass/Cloth/Materials/M_Cloth_Linen and its instances (CLOTH below) from the library's Linen scan
(Poly Haven rough_linen, CC0): its normal and ARM maps (T_Env_Linen_N/_ARM, so run after build_env_materials.py) and its
colour map imported again as /Game/FinalPass/Cloth/Textures/T_Cloth_Linen_D. The library's T_Env_Linen_D was taken for
a normal map on import (the scan is a blue linen, which the importer's normal-map guess catches): compressed as one, it
cannot be sampled as colour. Re-runnable (rebuilds).
Vertex colour (build_cloth_blender.py): R occlusion, G dust, B grime. Report: <project>/Review/Results/cloth-materials.json
"""
import json
import os
from pathlib import Path
import unreal as u
from env_common import TEX

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
DEST = '/Game/FinalPass/Cloth/Materials'
CTEX = '/Game/FinalPass/Cloth/Textures'
SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env' / 'PolyHaven' / 'rough_linen'
L = u.MaterialEditingLibrary; E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools()
R = {'errors': [], 'materials': []}
SAMPLER = {'C': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'N': u.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'M': u.MaterialSamplerType.SAMPLERTYPE_MASKS}
# Tint (rgb, saturation of the scan), Occ (strength, power), Dust (rgb, amount), Grime (rgb, amount),
# Rough (mul, add, dust lift), Normal strength, Spec. Unbleached cotton gone grey: albedo about 0.2, never white,
# fully rough, so a candle at arm's length lights it without blowing out.
CLOTH = {
    'MI_Cloth_DustSheet': dict(Tint=(0.56, 0.53, 0.47, 0.0), Occ=(0.85, 1.6, 0, 0), Dust=(0.13, 0.125, 0.115, 0.55),
                               Grime=(0.06, 0.05, 0.04, 0.75), Rough=(0.6, 0.42, 0.08, 0), Normal=0.8, Spec=0.3),
    'MI_Cloth_Laundry': dict(Tint=(0.58, 0.56, 0.52, 0.0), Occ=(0.8, 1.6, 0, 0), Dust=(0.2, 0.19, 0.17, 0.1),
                             Grime=(0.15, 0.13, 0.1, 0.2), Rough=(0.5, 0.45, 0.0, 0), Normal=0.7, Spec=0.3),
}
COLOUR = r'''float l = dot(B, float3(0.2126, 0.7152, 0.0722));
float3 c = lerp(l.xxx, B, Tint.w) * Tint.rgb;
c *= lerp(1, pow(saturate(V.r), Occ.y), Occ.x);
c = lerp(c, Dust.rgb, saturate(V.g * Dust.w));
c = lerp(c, Grime.rgb, saturate(V.b * Grime.w));
return c;'''
ROUGH = r'''return saturate(A * Rough.x + Rough.y + V * Rough.z);'''
NORMAL = r'''return normalize(float3(N.xy * S, N.z));'''


def node(m, cls, x=0, y=0, **kw):
    n = L.create_material_expression(m, cls, x, y)
    for k, v in kw.items(): n.set_editor_property(k, v)
    return n


def custom(m, code, names, x, y, kind=u.CustomMaterialOutputType.CMOT_FLOAT3):
    c = node(m, u.MaterialExpressionCustom, x, y, code=code, output_type=kind)
    ins = []
    for k in names:
        q = u.CustomInput(); q.set_editor_property('input_name', k); ins.append(q)
    c.set_editor_property('inputs', ins)
    return c


def import_colour():
    """The linen's colour map as a colour texture (sRGB, default compression), whatever the importer guesses."""
    t = u.AssetImportTask(); t.filename = str(SRC / 'rough_linen_diff.png'); t.destination_path = CTEX
    t.destination_name = 'T_Cloth_Linen_D'; t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t])
    tex = u.load_asset(f'{CTEX}/T_Cloth_Linen_D')
    if not tex: R['errors'].append('import T_Cloth_Linen_D'); return None
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('srgb', True); tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
    E.save_loaded_asset(tex, False)
    R['materials'].append(tex.get_path_name())
    return tex


def build_master():
    path = f'{DEST}/M_Cloth_Linen'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Cloth_Linen', DEST, u.Material, u.MaterialFactoryNew())
    tc = node(m, u.MaterialExpressionTextureCoordinate, -1500, 0)
    uvs = node(m, u.MaterialExpressionScalarParameter, -1500, 120, parameter_name='UVScale', default_value=1.0)
    uv = node(m, u.MaterialExpressionMultiply, -1300, 0); L.connect_material_expressions(tc, '', uv, 'A'); L.connect_material_expressions(uvs, '', uv, 'B')
    tex = {}
    for i, (k, t, kind) in enumerate((('Base', f'{CTEX}/T_Cloth_Linen_D', 'C'), ('ARM', f'{TEX}/T_Env_Linen_ARM', 'M'),
                                      ('Norm', f'{TEX}/T_Env_Linen_N', 'N'))):
        tex[k] = node(m, u.MaterialExpressionTextureSampleParameter2D, -1100, -300 + 300 * i, parameter_name=k,
                      texture=u.load_asset(t), sampler_type=SAMPLER[kind])
        L.connect_material_expressions(uv, '', tex[k], 'UVs')
    vc = node(m, u.MaterialExpressionVertexColor, -1100, 600)
    vec = {k: node(m, u.MaterialExpressionVectorParameter, -800, -500 + 90 * i, parameter_name=k, default_value=u.LinearColor(*d))
           for i, (k, d) in enumerate([('Tint', (0.66, 0.62, 0.55, 0)), ('Occ', (0.85, 1.6, 0, 0)), ('Dust', (0.13, 0.125, 0.115, 0.5)),
                                       ('Grime', (0.06, 0.05, 0.04, 0.7)), ('Rough', (0.6, 0.42, 0.08, 0))])}
    sc = {k: node(m, u.MaterialExpressionScalarParameter, -800, 450 + 70 * i, parameter_name=k, default_value=d)
          for i, (k, d) in enumerate([('Normal', 0.8), ('Spec', 0.3)])}
    col = custom(m, COLOUR, ['B', 'V', 'Tint', 'Occ', 'Dust', 'Grime'], -400, -300)
    for k, (src, out) in {'B': (tex['Base'], 'RGB'), 'V': (vc, ''), 'Tint': (vec['Tint'], 'RGBA'), 'Occ': (vec['Occ'], 'RGBA'),
                          'Dust': (vec['Dust'], 'RGBA'), 'Grime': (vec['Grime'], 'RGBA')}.items():
        assert L.connect_material_expressions(src, out, col, k), k
    L.connect_material_property(col, '', u.MaterialProperty.MP_BASE_COLOR)
    r = custom(m, ROUGH, ['A', 'V', 'Rough'], -400, 0, u.CustomMaterialOutputType.CMOT_FLOAT1)
    for k, (src, out) in {'A': (tex['ARM'], 'G'), 'V': (vc, 'G'), 'Rough': (vec['Rough'], 'RGBA')}.items():
        assert L.connect_material_expressions(src, out, r, k), k
    L.connect_material_property(r, '', u.MaterialProperty.MP_ROUGHNESS)
    L.connect_material_property(sc['Spec'], '', u.MaterialProperty.MP_SPECULAR)
    n = custom(m, NORMAL, ['N', 'S'], -400, 300)
    assert L.connect_material_expressions(tex['Norm'], 'RGB', n, 'N') and L.connect_material_expressions(sc['Normal'], '', n, 'S')
    L.connect_material_property(n, '', u.MaterialProperty.MP_NORMAL)
    L.layout_material_expressions(m); L.recompile_material(m); E.save_loaded_asset(m, False)
    R['materials'].append(path)
    return m


def main():
    yield 1
    import_colour(); yield 1
    m = build_master()
    for name, params in CLOTH.items():
        path = f'{DEST}/{name}'
        mi = u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(name, DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        L.set_material_instance_parent(mi, m)
        for k, v in params.items():
            if isinstance(v, tuple): L.set_material_instance_vector_parameter_value(mi, k, u.LinearColor(*[float(x) for x in v]))
            else: L.set_material_instance_scalar_parameter_value(mi, k, float(v))
        L.update_material_instance(mi); E.save_loaded_asset(mi, False)
        R['materials'].append(path)
    yield 1
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cloth-materials.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_CLOTH_MATERIALS ' + json.dumps(R))
