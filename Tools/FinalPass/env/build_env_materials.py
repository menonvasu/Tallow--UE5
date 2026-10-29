"""House environment library assets: textures, the four masters and the shared instances. Re-runnable (updates in place).

  run.sh headless build_env_materials.py materials          (TALLOW_ENV_REIMPORT=1 reimports the textures)

Data: env_library.py. Sources: SourceAssets/FinalPass/Env (fetch_polyhaven.py, make_wallpaper.py, make_rug.py).
1. Textures -> /Game/FinalPass/Env/Textures (T_Env_<set>_D/_N/_ARM, T_Env_<pattern>_D, T_Env_StarUshak_D).
2. Masters in /Game/FinalPass/Env/Materials:
   M_Env_HouseZones  copy of the house's shared world-aligned master (ArtDirection09 M_A6_World) with the generated
                     room-zone node (house_zones.py) in front of its outputs. Shell instances are made per room set by
                     apply_room_finish.py; outside every room box they render exactly like the legacy surface.
   M_Env_World       the same scanned finish with no legacy side (zone_finish.hlsl): props without their own maps.
   M_Env_Prop        UV-mapped PBR from a prop's own scanned maps, with tint, roughness and a luminance-keyed recolour.
   M_Env_Ember       the parlour's last embers.
3. Instances: MI_Env_Lib_<surface> (env_library.SURFACES), MI_Env_P_<Furnishing>_<slot> for every furnishing whose
   own import carries BaseColor + ARM + Normal (HouseFurnishV03 manifest), MI_Env_G1_Ember.
Report: <project>/Review/Results/env-materials.json
"""
import json
import os
from pathlib import Path
import unreal as u
import env_library as LIB
import house_zones as HZ
from env_common import TEX, MAT, LEGACY, V04

HERE = Path(__file__).resolve().parent
SRC = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets' / 'FinalPass' / 'Env'
FURNISH = Path(os.environ.get('TALLOW_FURNISH_SRC', str(Path.home() / 'Projects/tallow-current/Tallow/SourceAssets/Production/HouseFurnishV03')))
OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
L = u.MaterialEditingLibrary; E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools()
R = {'errors': [], 'textures': [], 'materials': [], 'instances': [], 'props': {}}
SINGLES = dict({k: 'Met/' + f for k, f in LIB.PATTERN_FILES.items()}, StarUshak='Met/star_ushak_rug.png')
VECS = ['Tile', 'Tint', 'Rough', 'Grime', 'Soot0', 'Soot1', 'Mark', 'Pat', 'Opt', 'Extra']
DEFAULTS = dict(ZMin=(0, 0, 0, 0), ZMax=(0, 0, 0, 0), Tile=(100, 100, 100, 100), Tint=(1, 1, 1, 1), Rough=(1, 0, 0, 0.5), Grime=(0, 0, 9999, 0),
                Soot0=(0, 0, -1e4, 1), Soot1=(0, 0, -1e4, 1), Mark=(0, 0, 0, 0), Pat=(0.2, 0, 0, 0), Opt=(0, 0, 1, 0), Extra=(0, 0, 0.2, 0))
SAMPLER = {'C': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'N': u.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'M': u.MaterialSamplerType.SAMPLERTYPE_MASKS}


def tex_asset(name):
    return u.load_asset(f'{TEX}/{name}')


def import_texture(src, name, kind):
    if not Path(src).exists():
        R['errors'].append(f'missing source {src} (run fetch_polyhaven.py)'); return None
    t = u.AssetImportTask(); t.filename = str(src); t.destination_path = TEX; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t])
    tex = u.load_asset(f'{TEX}/{name}')
    if not tex:
        R['errors'].append(f'import failed {src}'); return None
    if kind == 'N':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP); tex.set_editor_property('srgb', False)
        tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == 'ARM':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS); tex.set_editor_property('srgb', False)
        tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR)
    else:
        tex.set_editor_property('srgb', True); tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
    E.save_loaded_asset(tex, False)
    R['textures'].append(tex.get_path_name())
    return tex


def import_textures():
    again = os.environ.get('TALLOW_ENV_REIMPORT') == '1'
    for key, sid in LIB.SETS.items():
        for kind, tag in (('D', 'diff'), ('N', 'nor_dx'), ('ARM', 'arm')):
            name = f'T_Env_{key}_{kind}'
            if again or not E.does_asset_exist(f'{TEX}/{name}'):
                import_texture(SRC / 'PolyHaven' / sid / f'{sid}_{tag}.png', name, kind)
    for key, rel in SINGLES.items():
        name = f'T_Env_{key}_D'
        if again or not E.does_asset_exist(f'{TEX}/{name}'):
            import_texture(SRC / rel, name, 'D')


def node(m, cls, x=0, y=0, **kw):
    n = L.create_material_expression(m, cls, x, y)
    for k, v in kw.items(): n.set_editor_property(k, v)
    return n


def custom(m, code, names, x, y, outs=()):
    c = node(m, u.MaterialExpressionCustom, x, y, code=code, output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for k in names:
        q = u.CustomInput(); q.set_editor_property('input_name', k); ins.append(q)
    c.set_editor_property('inputs', ins)
    if outs:
        lst = []
        for name, kind in outs:
            o = u.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', kind); lst.append(o)
        c.set_editor_property('additional_outputs', lst)
    return c


OUTS = [('R', u.CustomMaterialOutputType.CMOT_FLOAT1), ('Nrm', u.CustomMaterialOutputType.CMOT_FLOAT3),
        ('M', u.CustomMaterialOutputType.CMOT_FLOAT1), ('S', u.CustomMaterialOutputType.CMOT_FLOAT1)]
PROPS = [('LC', u.MaterialProperty.MP_BASE_COLOR, ''), ('LR', u.MaterialProperty.MP_ROUGHNESS, 'R'), ('LN', u.MaterialProperty.MP_NORMAL, 'Nrm'),
         ('LM', u.MaterialProperty.MP_METALLIC, 'M'), ('LS', u.MaterialProperty.MP_SPECULAR, 'S')]


def shared_sampler(n):
    n.set_editor_property('sampler_source', u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    return n


def build_zones_master():
    """Legacy house master + the room-zone node. When the zone layout changes (input count), the node and its
    parameters are rebuilt in place on the same legacy graph, so existing shell instances keep their parent."""
    path = f'{MAT}/M_Env_HouseZones'
    if not E.does_asset_exist(path):
        E.duplicate_asset(f'{LEGACY}/M_A6_World', path)
    m = u.load_asset(path)
    cur = L.get_material_property_input_node(m, u.MaterialProperty.MP_BASE_COLOR)
    legacy = None
    if isinstance(cur, u.MaterialExpressionCustom) and cur.get_editor_property('desc') == 'Env house zones':
        if len(cur.get_editor_property('inputs')) == len(HZ.inputs()):
            cur.set_editor_property('code', HZ.hlsl())
        else:
            ins = L.get_inputs_for_material_expression(m, cur)
            legacy = {k: (ins[i], '') for i, (k, _, _) in enumerate(PROPS)}
            for x in ins[len(PROPS):]:
                if x: L.delete_material_expression(m, x)
            L.delete_material_expression(m, cur)
    else:
        legacy = {k: (L.get_material_property_input_node(m, p), L.get_material_property_input_node_output_name(m, p)) for k, p, _ in PROPS}
    if legacy:
        c = custom(m, HZ.hlsl(), HZ.inputs(), 600, 0, OUTS); c.set_editor_property('desc', 'Env house zones')
        for k, _, _ in PROPS:
            src, out = legacy[k]; assert L.connect_material_expressions(src, out, c, k), k
        assert L.connect_material_expressions(node(m, u.MaterialExpressionWorldPosition, 0, 300), '', c, 'P')
        assert L.connect_material_expressions(node(m, u.MaterialExpressionVertexNormalWS, 0, 360), '', c, 'VN')
        y = 420
        for i in range(HZ.ZONES):
            for v in HZ.ZONE_VECTORS:
                d = DEFAULTS.get('Z' + v, DEFAULTS.get(v, (0, 0, 0, 0)))
                p = node(m, u.MaterialExpressionVectorParameter, 0, y, parameter_name=f'Z{i}{v}', default_value=u.LinearColor(*d), group=f'Zone{i}')
                assert L.connect_material_expressions(p, 'RGBA', c, f'Z{i}{v}'); y += 50
        for i in range(HZ.SPOTS):
            p = node(m, u.MaterialExpressionVectorParameter, 0, y, parameter_name=f'S{i}', default_value=u.LinearColor(0, 0, -1e4, 1), group='Soot')
            assert L.connect_material_expressions(p, 'RGBA', c, f'S{i}'); y += 50
        defaults = {'C': 'T_Env_Paper_D', 'N': 'T_Env_Paper_N', 'M': 'T_Env_Paper_ARM'}
        for k, kind in HZ.TEXTURES:
            t = tex_asset('T_Env_WillowBough_D' if k.startswith('Pat') else defaults[kind])
            tn = shared_sampler(node(m, u.MaterialExpressionTextureObjectParameter, 0, y, parameter_name=k, texture=t, sampler_type=SAMPLER[kind], group='Textures'))
            assert L.connect_material_expressions(tn, '', c, k), k; y += 80
        for k, p, out in PROPS:
            assert L.connect_material_property(c, out, p), k
    L.recompile_material(m); E.save_loaded_asset(m, False)
    R['materials'].append(path)
    return m


def build_world_master():
    """The scanned finish alone (zone_finish.hlsl with the legacy side held at zero and the zone everywhere)."""
    path = f'{MAT}/M_Env_World'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Env_World', MAT, u.Material, u.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    names = ['LC', 'LR', 'LN', 'LM', 'LS', 'P', 'VN', 'ZMin', 'ZMax'] + VECS + ['Base', 'Norm', 'ARM', 'PatTex']
    c = custom(m, (HERE / 'zone_finish.hlsl').read_text(), names, 600, 0, OUTS)
    zero = node(m, u.MaterialExpressionConstant3Vector, 0, -200, constant=u.LinearColor(0, 0, 1, 0))
    for k in ('LC', 'LR', 'LN', 'LM', 'LS'):
        assert L.connect_material_expressions(zero, '', c, k)
    assert L.connect_material_expressions(node(m, u.MaterialExpressionWorldPosition, 0, 0), '', c, 'P')
    assert L.connect_material_expressions(node(m, u.MaterialExpressionVertexNormalWS, 0, 60), '', c, 'VN')
    for k, v in (('ZMin', (-1e6, -1e6, -1e6, 0)), ('ZMax', (1e6, 1e6, 1e6, 1))):
        assert L.connect_material_expressions(node(m, u.MaterialExpressionConstant4Vector, 0, 120, constant=u.LinearColor(*v)), '', c, k)
    y = 200
    for k in VECS:
        p = node(m, u.MaterialExpressionVectorParameter, 0, y, parameter_name='Env' + k, default_value=u.LinearColor(*DEFAULTS[k]))
        assert L.connect_material_expressions(p, 'RGBA', c, k); y += 60
    for k, t, kind in (('Base', 'T_Env_Paint_D', 'C'), ('Norm', 'T_Env_Paint_N', 'N'), ('ARM', 'T_Env_Paint_ARM', 'M'), ('PatTex', 'T_Env_WillowBough_D', 'C')):
        tn = shared_sampler(node(m, u.MaterialExpressionTextureObjectParameter, 0, y, parameter_name='Env' + k, texture=tex_asset(t), sampler_type=SAMPLER[kind]))
        assert L.connect_material_expressions(tn, '', c, k); y += 80
    for k, p, out in PROPS:
        assert L.connect_material_property(c, out, p), k
    L.recompile_material(m); E.save_loaded_asset(m, False)
    R['materials'].append(path)
    return m


PROP_CODE = r'''float l = dot(B, float3(0.2126, 0.7152, 0.0722));
float3 c = lerp(l.xxx, B, Tint.w) * Tint.rgb;
float k = Recol.w * smoothstep(Range.x, Range.y, l);
c = lerp(c, l * Recol.rgb / max(Range.z, 0.01), k);
return c * lerp(1, AO, Rough.w);'''
NORMAL_CODE = r'''float3 n = float3(N.xy * Str, N.z);
n.xy += D.xy * DStr;
return normalize(n);'''


def build_prop_master():
    path = f'{MAT}/M_Env_Prop'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Env_Prop', MAT, u.Material, u.MaterialFactoryNew())
    tc = node(m, u.MaterialExpressionTextureCoordinate, -1700, 0)
    xf = node(m, u.MaterialExpressionVectorParameter, -1700, 120, parameter_name='UVXform', default_value=u.LinearColor(1, 1, 0, 0))
    uv = custom(m, 'return float3(UV * X.xy + X.zw, 0);', ['UV', 'X'], -1500, 0)
    assert L.connect_material_expressions(tc, '', uv, 'UV') and L.connect_material_expressions(xf, 'RGBA', uv, 'X')
    uv2 = node(m, u.MaterialExpressionComponentMask, -1350, 0, r=True, g=True, b=False, a=False); L.connect_material_expressions(uv, '', uv2, '')

    def sample(name, t, kind, x, y):
        n = node(m, u.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=tex_asset(t), sampler_type=SAMPLER[kind])
        return n
    base = sample('Base', 'T_Env_Paint_D', 'C', -1100, -300); arm = sample('ARM', 'T_Env_Paint_ARM', 'M', -1100, 0); nrm = sample('Norm', 'T_Env_Paint_N', 'N', -1100, 300)
    for t in (base, arm, nrm): L.connect_material_expressions(uv2, '', t, 'UVs')
    dtile = node(m, u.MaterialExpressionScalarParameter, -1400, 600, parameter_name='DetailTile', default_value=1.0)
    duv = node(m, u.MaterialExpressionMultiply, -1250, 600); L.connect_material_expressions(uv2, '', duv, 'A'); L.connect_material_expressions(dtile, '', duv, 'B')
    det = sample('DetailNorm', 'T_Env_Velvet_N', 'N', -1100, 600); L.connect_material_expressions(duv, '', det, 'UVs')
    vec = {k: node(m, u.MaterialExpressionVectorParameter, -800, -500 + 90 * i, parameter_name=k, default_value=u.LinearColor(*d))
           for i, (k, d) in enumerate([('Tint', (1, 1, 1, 1)), ('Rough', (1, 0, 1, 0.8)), ('Recol', (0, 0, 0, 0)), ('Range', (0, 1, 0.2, 0))])}
    sc = {k: node(m, u.MaterialExpressionScalarParameter, -800, 450 + 70 * i, parameter_name=k, default_value=d)
          for i, (k, d) in enumerate([('Normal', 1.0), ('Detail', 0.0), ('Spec', 0.4)])}
    col = custom(m, PROP_CODE, ['B', 'AO', 'Tint', 'Rough', 'Recol', 'Range'], -400, -300)
    for k, (src, out) in {'B': (base, 'RGB'), 'AO': (arm, 'R'), 'Tint': (vec['Tint'], 'RGBA'), 'Rough': (vec['Rough'], 'RGBA'),
                          'Recol': (vec['Recol'], 'RGBA'), 'Range': (vec['Range'], 'RGBA')}.items():
        assert L.connect_material_expressions(src, out, col, k), k
    L.connect_material_property(col, '', u.MaterialProperty.MP_BASE_COLOR)
    rmul = node(m, u.MaterialExpressionMultiply, -500, 0); L.connect_material_expressions(arm, 'G', rmul, 'A'); L.connect_material_expressions(vec['Rough'], 'R', rmul, 'B')
    radd = node(m, u.MaterialExpressionAdd, -350, 0); L.connect_material_expressions(rmul, '', radd, 'A'); L.connect_material_expressions(vec['Rough'], 'G', radd, 'B')
    rsat = node(m, u.MaterialExpressionSaturate, -200, 0); L.connect_material_expressions(radd, '', rsat, '')
    L.connect_material_property(rsat, '', u.MaterialProperty.MP_ROUGHNESS)
    mmul = node(m, u.MaterialExpressionMultiply, -350, 120); L.connect_material_expressions(arm, 'B', mmul, 'A'); L.connect_material_expressions(vec['Rough'], 'B', mmul, 'B')
    L.connect_material_property(mmul, '', u.MaterialProperty.MP_METALLIC)
    L.connect_material_property(sc['Spec'], '', u.MaterialProperty.MP_SPECULAR)
    nn = custom(m, NORMAL_CODE, ['N', 'D', 'Str', 'DStr'], -400, 400)
    for k, (src, out) in {'N': (nrm, 'RGB'), 'D': (det, 'RGB'), 'Str': (sc['Normal'], ''), 'DStr': (sc['Detail'], '')}.items():
        assert L.connect_material_expressions(src, out, nn, k), k
    L.connect_material_property(nn, '', u.MaterialProperty.MP_NORMAL)
    L.layout_material_expressions(m); L.recompile_material(m); E.save_loaded_asset(m, False)
    R['materials'].append(path)
    return m


EMBER_CODE = r'''// Slow breathing glow of the last embers: each lump breathes on its own phase, never fully dark.
float n = frac(sin(dot(floor(P / 2.5), float3(12.9898, 78.233, 37.719))) * 43758.5453);
float b = 0.55 + 0.45 * sin(T * (0.35 + 0.4 * n) + n * 6.2831);
return Glow.rgb * Glow.a * (0.35 + 0.65 * b * b);'''


def build_ember():
    path = f'{MAT}/M_Env_Ember'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Env_Ember', MAT, u.Material, u.MaterialFactoryNew())
    glow = node(m, u.MaterialExpressionVectorParameter, -700, 0, parameter_name='Glow', default_value=u.LinearColor(1.0, 0.28, 0.06, 4.0))
    c = custom(m, EMBER_CODE, ['P', 'T', 'Glow'], -400, 0)
    assert L.connect_material_expressions(node(m, u.MaterialExpressionWorldPosition, -700, 150), '', c, 'P')
    assert L.connect_material_expressions(node(m, u.MaterialExpressionTime, -700, 250), '', c, 'T')
    assert L.connect_material_expressions(glow, 'RGBA', c, 'Glow')
    L.connect_material_property(c, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    L.connect_material_property(node(m, u.MaterialExpressionConstant3Vector, -400, -200, constant=u.LinearColor(0.03, 0.025, 0.02, 1)), '', u.MaterialProperty.MP_BASE_COLOR)
    L.connect_material_property(node(m, u.MaterialExpressionConstant, -400, 200, r=0.8), '', u.MaterialProperty.MP_ROUGHNESS)
    L.recompile_material(m); E.save_loaded_asset(m, False)
    mi = instance('MI_Env_G1_Ember', m)
    L.set_material_instance_vector_parameter_value(mi, 'Glow', u.LinearColor(1.0, 0.26, 0.05, 14.0))
    L.update_material_instance(mi); E.save_loaded_asset(mi, False)
    R['materials'].append(path)


def instance(name, parent):
    path = f'{MAT}/{name}'
    mi = u.load_asset(path) if E.does_asset_exist(path) else AT.create_asset(name, MAT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    if mi.get_editor_property('parent') != parent:
        L.set_material_instance_parent(mi, parent)
    R['instances'].append(path)
    return mi


def set_params(mi, params, prefix=''):
    for k, v in params.items():
        if k == 'pattern':
            L.set_material_instance_texture_parameter_value(mi, prefix + 'PatTex', tex_asset(f'T_Env_{v}_D'))
        elif isinstance(v, tuple):
            L.set_material_instance_vector_parameter_value(mi, prefix + k, u.LinearColor(*[float(x) for x in v]))
        elif isinstance(v, str):
            L.set_material_instance_texture_parameter_value(mi, prefix + k, tex_asset(v))
        else:
            L.set_material_instance_scalar_parameter_value(mi, prefix + k, float(v))


def surface_instances(world):
    for name, (texset, params) in LIB.SURFACES.items():
        mi = instance(f'MI_Env_{name}' if name.startswith('G1_') else f'MI_Env_Lib_{name}', world)
        set_params(mi, dict(Base=f'T_Env_{texset}_D', Norm=f'T_Env_{texset}_N', ARM=f'T_Env_{texset}_ARM'), 'Env')
        set_params(mi, {k: DEFAULTS[k] for k in VECS}, 'Env')      # clear what an earlier run set
        set_params(mi, params, 'Env')
        L.update_material_instance(mi); E.save_loaded_asset(mi, False)


def furnish_textures(name, slot):
    """The furnishing's own scanned maps, by the import's naming (T_V0x_T_Furnish_<Name>[_<slot>]_<Role>)."""
    found = {}
    for role, key in (('BaseColor', 'Base'), ('ARM', 'ARM'), ('Normal', 'Norm')):
        for stem in (f'T_V04_T_Furnish_{name}_{slot}_{role}', f'T_V04_T_Furnish_{name}_{role}',
                     f'T_V05_T_Furnish_{name}_{slot}_{role}', f'T_V05_T_Furnish_{name}_{role}'):
            if E.does_asset_exist(f'{V04}/Textures/{stem}'):
                found[key] = u.load_asset(f'{V04}/Textures/{stem}'); break
    return found


def prop_instances(prop):
    """One instance per furnishing slot whose own import has colour, ARM and normal; see-through slots are left alone."""
    manifest = json.loads((FURNISH / 'Export' / 'manifest.json').read_text())
    for name, row in manifest.items():
        mats = row.get('materials') or []
        for i, mrow in enumerate(mats):
            slot = f'{name}_{i}' if len(mats) > 1 else name
            if mrow.get('alpha') == 'BLEND':
                continue
            tex = furnish_textures(name, i)
            if set(tex) != {'Base', 'ARM', 'Norm'}:
                continue
            mi = instance(f'MI_Env_P_{slot}', prop)
            for k, t in tex.items(): L.set_material_instance_texture_parameter_value(mi, k, t)
            set_params(mi, LIB.PROP_TUNE.get(name, {}))
            L.update_material_instance(mi); E.save_loaded_asset(mi, False)
            R['props'][slot] = mi.get_path_name()
    # The parlour rug: the Met Star Ushak photograph across the rug mesh's own UVs (-0.715..0.715 by 0.505..1.495).
    mi = instance('MI_Env_P_Rug', prop)
    set_params(mi, dict(UVXform=(1 / 1.43, 1 / 0.99, 0.5, -0.51), Base='T_Env_StarUshak_D', ARM='T_Env_Paper_ARM', Norm='T_Env_Velvet_N',
                        DetailNorm='T_Env_Velvet_N', Tint=(0.62, 0.56, 0.52, 0.85), Rough=(0, 0.88, 0, 0), Normal=0.0, Detail=0.45, DetailTile=9.0))
    L.update_material_instance(mi); E.save_loaded_asset(mi, False)


def main():
    import_textures(); yield 1
    hz = build_zones_master(); yield 1
    world = build_world_master(); yield 1
    prop = build_prop_master(); yield 1
    surface_instances(world); prop_instances(prop); build_ember(); yield 1
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'env-materials.json').write_text(json.dumps(R, indent=1))
    u.log('ENV_MATERIALS textures=%d materials=%d instances=%d props=%d errors=%s' % (len(R['textures']), len(R['materials']), len(R['instances']), len(R['props']), R['errors']))
