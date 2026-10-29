"""The cellar's brick arches: a finish that lays the brick of each doorway arch as a Victorian rowlock arch. Re-runnable.

  run.sh headless build_cellar_materials.py cellar-materials      (after env/build_env_materials.py)

The arch rings of the cellar doorways (Portal_Basement and its boiler-wall part) are smooth bands; a world-aligned brick
runs its courses straight across them, which no bricklayer would do. M_Env_ArchBrick finds the doorway a pixel belongs
to (the nearest arch centre in ARCHES), and above the springing line maps the scanned brick round that centre: bed joints
radial, the bricks standing on end round the curve. Below the springing line the jambs are ordinary coursed brickwork,
and faces that do not look out of the wall (the ring's edges, thresholds) fall back to plain world-aligned brick.
Instance: MI_Env_B_ArchBrick (Firebrick set, sooted). Report: <project>/Review/Results/cellar-materials.json
"""
import json
import os
from pathlib import Path
import unreal as u
import build_env_materials as BEM
from env_common import MAT

OUT = Path(os.environ['TALLOW_ENV_OUT']) / 'Results'
L = u.MaterialEditingLibrary; E = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools()
# Arch centres measured from the portal meshes (export_house_geometry.py): (x, y, springing z, wall axis), axis 0 = the
# wall runs along y (its face looks along x), 1 = the wall runs along x. Outer radius 95, ring top at z -68.
ARCHES = [(286.5, -650.0, -163.0, 1), (585.0, -325.0, -163.0, 0), (585.0, -1039.0, -163.0, 0), (975.0, -259.0, -163.0, 0),
          (975.0, -1039.0, -163.0, 0), (1429.0, -560.0, -163.0, 1)]
N = 8
CODE_HEAD = r'''// Rowlock brick arches: polar mapping round the nearest doorway's centre above the springing line.
float3 n = normalize(VN);
float4 A = A0; float bd = distance(P, A0.xyz);
'''
CODE_TAIL = r'''bool alongY = A.w < 0.5;                       // the wall runs along y: its face looks along x
float3 eh = alongY ? float3(0, 1, 0) : float3(1, 0, 0);
float3 en = alongY ? float3(1, 0, 0) : float3(0, 1, 0);
float h = dot(P - A.xyz, eh), dz = P.z - A.z;
float2 it = 1.0 / max(Tile.xy, 0.001);
float2 uv; float3 tu, tv;
if (abs(dot(n, en)) > 0.7 && dz > 0)
{
    float r = max(length(float2(h, dz)), 0.001);
    float th = atan2(dz, h);
    uv = float2(r, th * Tile.z);                  // texture rows (bed joints) run out from the centre
    tu = (eh * h + float3(0, 0, 1) * dz) / r;
    tv = (eh * -dz + float3(0, 0, 1) * h) / r;
}
else
{
    float3 a = abs(n);
    bool fx = a.x >= a.y && a.x >= a.z, fy = !fx && a.y >= a.z;
    float sx = n.x >= 0 ? 1.0 : -1.0, sy = n.y >= 0 ? 1.0 : -1.0;
    uv = fx ? float2(-sx * P.y, -P.z) : (fy ? float2(sy * P.x, -P.z) : float2(P.x, P.y));
    tu = fx ? float3(0, -sx, 0) : (fy ? float3(sy, 0, 0) : float3(1, 0, 0));
    tv = (fx || fy) ? float3(0, 0, -1) : float3(0, 1, 0);
}
float3 base = Texture2DSample(Base, BaseSampler, uv * it).rgb;
float3 arm = Texture2DSample(ARM, ARMSampler, uv * it).rgb;
float2 t = Texture2DSample(Norm, NormSampler, uv * it).rg * 2 - 1;
const float3 L = float3(0.2126, 0.7152, 0.0722);
float3 c = lerp(dot(base, L).xxx, base, Tint.w) * Tint.rgb;
c *= lerp(1.0, arm.r, Opt.w);
float soot = Soot.w * smoothstep(Soot.x, Soot.y, P.z);       // smoke darkens the crown
c *= 1 - soot;
float3 g = t.x * tu + t.y * tv; g -= n * dot(g, n);
Nrm = normalize(n + g * Opt.z);
R = saturate(arm.g * Rough.x + Rough.y + 0.2 * soot);
M = 0; S = Rough.w;
return c;
'''


def hlsl():
    pick = ''.join(f'if (A{i}.w > -0.5 && distance(P, A{i}.xyz) < bd) {{ bd = distance(P, A{i}.xyz); A = A{i}; }}\n' for i in range(1, N))
    return CODE_HEAD + pick + CODE_TAIL


def build_master():
    path = f'{MAT}/M_Env_ArchBrick'
    if E.does_asset_exist(path):
        m = u.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = AT.create_asset('M_Env_ArchBrick', MAT, u.Material, u.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    names = ['P', 'VN'] + [f'A{i}' for i in range(N)] + ['Tile', 'Tint', 'Rough', 'Opt', 'Soot', 'Base', 'Norm', 'ARM']
    c = BEM.custom(m, hlsl(), names, 600, 0, BEM.OUTS)
    assert L.connect_material_expressions(BEM.node(m, u.MaterialExpressionWorldPosition, 0, 0), '', c, 'P')
    assert L.connect_material_expressions(BEM.node(m, u.MaterialExpressionVertexNormalWS, 0, 60), '', c, 'VN')
    y = 120
    defaults = dict(Tile=(195, 195, 85, 0), Tint=(1, 1, 1, 1), Rough=(1, 0, 0, 0.4), Opt=(0, 0, 1, 0.6), Soot=(-160, -60, 0, 0))
    for i in range(N):
        p = BEM.node(m, u.MaterialExpressionVectorParameter, 0, y, parameter_name=f'A{i}', default_value=u.LinearColor(0, 0, -1e5, -1), group='Arches')
        assert L.connect_material_expressions(p, 'RGBA', c, f'A{i}'); y += 50
    for k, d in defaults.items():
        p = BEM.node(m, u.MaterialExpressionVectorParameter, 0, y, parameter_name=k, default_value=u.LinearColor(*d))
        assert L.connect_material_expressions(p, 'RGBA', c, k); y += 50
    for k, t, kind in (('Base', 'T_Env_Firebrick_D', 'C'), ('Norm', 'T_Env_Firebrick_N', 'N'), ('ARM', 'T_Env_Firebrick_ARM', 'M')):
        tn = BEM.shared_sampler(BEM.node(m, u.MaterialExpressionTextureObjectParameter, 0, y, parameter_name=k, texture=BEM.tex_asset(t), sampler_type=BEM.SAMPLER[kind]))
        assert L.connect_material_expressions(tn, '', c, k); y += 80
    for k, p, out in BEM.PROPS:
        assert L.connect_material_property(c, out, p), k
    L.recompile_material(m); E.save_loaded_asset(m, False)
    return m


def main():
    yield 1
    m = build_master(); yield 1
    mi = BEM.instance('MI_Env_B_ArchBrick', m)
    for i in range(N):
        a = ARCHES[i] if i < len(ARCHES) else (0, 0, -1e5, -1)
        L.set_material_instance_vector_parameter_value(mi, f'A{i}', u.LinearColor(*[float(x) for x in a]))
    # Firebrick scan is 1.5 m square (x1.3); Tile.z: the arc length per radian at which the bricks stand on end
    for k, v in dict(Tile=(195, 195, 85, 0), Tint=(0.58, 0.43, 0.36, 0.6), Rough=(1.0, 0.06, 0, 0.3), Opt=(0, 0, 1.0, 0.7),
                     Soot=(-150, -60, 0, 0.45)).items():
        L.set_material_instance_vector_parameter_value(mi, k, u.LinearColor(*[float(x) for x in v]))
    L.update_material_instance(mi); E.save_loaded_asset(mi, False)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cellar-materials.json').write_text(json.dumps({'master': m.get_path_name(), 'instance': mi.get_path_name(), 'arches': ARCHES}, indent=1))
    u.log('ENV_CELLAR_MATERIALS arches=%d' % len(ARCHES))
