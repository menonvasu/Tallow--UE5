"""Parlour curtains: heavy lined drapes on a brass pole, gathered into tie-backs either side of a window.
Replaces the flat wavy strips of SM_A9_ParlourCurtains (same role, same window, same footprint).

blender -b --factory-startup --python build_curtains_blender.py -- <out_dir>
Output SM_Env_G1_Curtains.fbx in actor space (Unreal cm): x along the wall centred on the window, y = 2 at the wall
face with the room towards -y, z = 0 at the floor. Slot 0 cloth, slot 1 brass. Two-sided cloth (both faces built).
Shape: each panel is a strip of fabric with soft vertical folds; it hangs from rings at the pole, is drawn in to a
narrow bundle by the tie-back at hand height, flares again below and just breaks on the floor.
"""
import json
import math
import os
import sys
import bmesh
import bpy
from mathutils import Vector, Matrix

out = sys.argv[sys.argv.index('--') + 1:][0]
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
B = lambda x, y, z: Vector((x / 100.0, -y / 100.0, z / 100.0))
HALF = 94.0              # panel outer edges sit at +-HALF (old mesh: 187 cm across)
TOP, TIE, POLE = 211.0, 96.0, 216.0
FOLDS, NU, NV = 7, 84, 72


def smooth(a, b, t):
    t = max(0.0, min(1.0, t)); t = t * t * (3 - 2 * t)
    return a + (b - a) * t


def width(z):
    """Gathered width of a panel at height z."""
    if z >= TIE:
        return smooth(15.0, 50.0, (z - TIE) / (TOP - TIE) * 1.15)
    return smooth(15.0, 36.0, (TIE - z) / TIE * 1.4)


def depth(z):
    if z >= TIE:
        return smooth(9.0, 4.5, (z - TIE) / (TOP - TIE))
    return smooth(9.0, 6.5, (TIE - z) / TIE)


def panel(bm, side):
    """side -1: left panel (outer edge at -HALF), +1: right. Face and lining are two sheets 0.4 cm apart."""
    outer = side * HALF
    for lining in (False, True):
        rows = []
        for j in range(NV + 1):
            v = j / NV
            z = TOP * (1 - v) + 0.6 * v
            w, d = width(z), depth(z)
            row = []
            for i in range(NU + 1):
                u = i / NU
                ph = 2 * math.pi * FOLDS * u + 0.35 * math.sin(3.1 * u + 1.7)
                fold = 0.5 - 0.5 * math.cos(ph)
                x = outer - side * w * u
                y = -6.0 - d * fold * (0.85 + 0.3 * math.sin(5.3 * u + 0.4)) + (0.4 if lining else 0.0)
                if z < 5.0:                       # the hem just breaks on the boards
                    k = (5.0 - z) / 5.0
                    x += side * 2.0 * k; y -= 3.0 * k * fold
                row.append(bm.verts.new(B(x, y, z)))
            rows.append(row)
        for j in range(NV):
            for i in range(NU):
                q = (rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i])
                if (side > 0) != lining: q = q[::-1]
                bm.faces.new(q).material_index = 0


def cylinder(bm, a, b, r, mat, seg=12):
    pa, pb = B(*a), B(*b); dvec = pb - pa
    res = bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r / 100, radius2=r / 100, depth=dvec.length)
    rot = dvec.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((pa + pb) / 2) @ rot, verts=res['verts'])
    for f in {f for v in res['verts'] for f in v.link_faces}: f.material_index = mat


def sphere(bm, c, r, mat):
    res = bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=r / 100)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(B(*c)), verts=res['verts'])
    for f in {f for v in res['verts'] for f in v.link_faces}: f.material_index = mat


def ring(bm, c, r, tube, mat, axis='X', seg=10):
    pts = []
    for k in range(seg):
        a = 2 * math.pi * k / seg
        p = (c[0], c[1] + r * math.cos(a), c[2] + r * math.sin(a)) if axis == 'X' else (c[0] + r * math.cos(a), c[1] + r * math.sin(a), c[2])
        pts.append(p)
    for k in range(seg):
        cylinder(bm, pts[k], pts[(k + 1) % seg], tube, mat, seg=6)


bm = bmesh.new()
for side in (-1, 1):
    panel(bm, side)
    # tie-back: a band of the same cloth round the bundle, hooked to the wall
    cx = side * (HALF - width(TIE) / 2)
    for k in range(14):
        a0, a1 = 2 * math.pi * k / 14, 2 * math.pi * (k + 1) / 14
        p0 = (cx + 9.5 * math.cos(a0), -6.5 + 6.5 * math.sin(a0), TIE)
        p1 = (cx + 9.5 * math.cos(a1), -6.5 + 6.5 * math.sin(a1), TIE)
        cylinder(bm, p0, p1, 1.6, 0, seg=6)
    cylinder(bm, (side * (HALF + 1), 2.0, TIE), (side * (HALF + 1), -7.0, TIE), 0.9, 1)
    # rings on the pole above the panel
    for k in range(9):
        x = side * (HALF - 2 - k * (width(TOP) - 4) / 8)
        ring(bm, (x, -7.0, POLE - 0.2), 2.4, 0.35, 1)
cylinder(bm, (-HALF - 5, -7.0, POLE), (HALF + 5, -7.0, POLE), 1.5, 1)                   # pole
for side in (-1, 1):
    sphere(bm, (side * (HALF + 7.5), -7.0, POLE), 3.0, 1)                                # finials
    cylinder(bm, (side * (HALF + 2), 2.0, POLE), (side * (HALF + 2), -7.0, POLE), 0.8, 1)   # brackets
me = bpy.data.meshes.new('SM_Env_G1_Curtains'); bm.to_mesh(me); bm.free()
ob = bpy.data.objects.new('SM_Env_G1_Curtains', me); bpy.context.collection.objects.link(ob)
for name in ('Cloth', 'Brass'):
    ob.data.materials.append(bpy.data.materials.new(name))
for p in me.polygons: p.use_smooth = True
me.calc_loop_triangles()
vv = [v.co for v in me.vertices]
info = {'tris': len(me.loop_triangles), 'min_cm': [round(100 * min(v[0] for v in vv), 1), round(-100 * max(v[1] for v in vv), 1), round(100 * min(v[2] for v in vv), 1)],
        'max_cm': [round(100 * max(v[0] for v in vv), 1), round(-100 * min(v[1] for v in vv), 1), round(100 * max(v[2] for v in vv), 1)]}
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=os.path.join(out, 'SM_Env_G1_Curtains.fbx'), use_selection=True, object_types={'MESH'},
                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                         mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False)
json.dump(info, open(os.path.join(out, 'curtains.json'), 'w'), indent=1)
print('ENV_CURTAINS', json.dumps(info))
