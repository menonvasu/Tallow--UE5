"""The chandler's things for the cellar: what a tallow chandler's family kept below stairs in 1894. Built here, not
downloaded, because none of the free sources has period candle-making gear. Small pieces only.

blender -b --factory-startup --python build_cellar_props_blender.py -- <out_dir>
Outputs (FBX, Unreal cm after export; pivot noted per piece; front faces Blender -Y = Unreal +Y):
  SM_Env_B_CandleRack    a broach rod hung from the joists on two cords, eight pairs of dipped candles drying on their
                         wicks (pivot: rod centre at the ceiling; slots Wood, Wax, Twine, Iron)
  SM_Env_B_CandleMould   a twelve-tube tin mould frame, wicks tied off over a stick on top (pivot: base centre; Iron, Twine, Wood)
  SM_Env_B_TallowCakes   cakes of rendered tallow on a cutting board, a knife beside them (pivot: base centre; Wood, Wax, Iron)
  SM_Env_B_WickSpool     a spool of cotton wick and a loose tail (pivot: base centre; Wood, Twine)
  SM_Env_B_CandleBundle  a dozen finished candles tied in a bundle by their wicks (pivot: base centre; Wax, Twine)
  SM_Env_B_CandleBox     the tin candle box hung on the wall by the stair (pivot: back centre on the wall; Iron)
  SM_Env_B_CoalSpill     coal fallen from the bunker door across the paviours, with the shovel (pivot: base; Coal, Iron, Wood)
  SM_Env_B_Airer         a ceiling airer over the wash tub, a sheet and a child's shift drying on it (pivot: centre at the
                         ceiling; Wood, Cloth, Twine, Iron)
Sizes are real sizes times 1.3 (the house is built a third over life size). Writes <name>.json beside each FBX.
"""
import json
import math
import os
import random
import sys
import bmesh
import bpy
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
out = argv[0]
os.makedirs(out, exist_ok=True)
K = 1.3 / 100.0          # real cm -> Blender metres at house scale


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def tag(geom, index):
    for f in [g for g in geom if isinstance(g, bmesh.types.BMFace)]:
        f.material_index = index


def faces_of(verts):
    return list({f for v in verts for f in v.link_faces})


def cylinder(bm, a, b, r0, r1=None, seg=10, index=0, cap=True):
    """Tapered round between two points (Blender metres)."""
    r1 = r0 if r1 is None else r1
    a, b = Vector(a), Vector(b); d = b - a; n = d.normalized()
    t = n.orthogonal().normalized(); u = n.cross(t)
    rings = []
    for c, r in ((a, r0), (b, r1)):
        rings.append([bm.verts.new(c + (t * math.cos(2 * math.pi * i / seg) + u * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)])
    fs = [bm.faces.new((rings[0][i], rings[0][(i + 1) % seg], rings[1][(i + 1) % seg], rings[1][i])) for i in range(seg)]
    if cap:
        fs.append(bm.faces.new(list(reversed(rings[0])))); fs.append(bm.faces.new(rings[1]))
    for f in fs: f.material_index = index
    return rings


def lathe(bm, profile, centre, seg=12, index=0, wobble=0.0, rng=None):
    """Surface of revolution about +Z through centre; profile = [(radius m, z m)] bottom to top (closed at both ends)."""
    c = Vector(centre); rings = []
    for k, (r, z) in enumerate(profile):
        ring = []
        for i in range(seg):
            w = 1 + (rng.uniform(-wobble, wobble) if rng and wobble else 0)
            ang = 2 * math.pi * i / seg
            ring.append(bm.verts.new(c + Vector((math.cos(ang) * r * w, math.sin(ang) * r * w, z))))
        rings.append(ring)
    fs = []
    for k in range(len(rings) - 1):
        for i in range(seg):
            fs.append(bm.faces.new((rings[k][i], rings[k][(i + 1) % seg], rings[k + 1][(i + 1) % seg], rings[k + 1][i])))
    fs.append(bm.faces.new(list(reversed(rings[0])))); fs.append(bm.faces.new(rings[-1]))
    for f in fs: f.material_index = index
    return rings


def box(bm, lo, hi, index=0):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r['verts']:
        v.co = Vector((lo[0] if v.co.x < 0 else hi[0], lo[1] if v.co.y < 0 else hi[1], lo[2] if v.co.z < 0 else hi[2]))
    for f in faces_of(r['verts']): f.material_index = index
    return r['verts']


def rock(bm, centre, size, rng, index=0, sub=1):
    """An irregular lump (coal, tallow crumbs): a jittered icosphere."""
    r = bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
    sx, sy, sz = size
    for v in r['verts']:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz)) * rng.uniform(0.8, 1.15) + Vector(centre)
    for f in faces_of(r['verts']): f.material_index = index
    return r['verts']


def dipped_candle(bm, top, length, rng, index, seg=10):
    """A candle dipped on a hanging wick: thin at the wick, swelling to a rounded drip at the bottom, never quite round."""
    L = length; base = Vector(top)
    prof = []
    steps = 9
    for k in range(steps + 1):
        f = k / steps                      # 0 top .. 1 bottom
        r = (0.55 + 0.45 * f ** 0.6) * 1.05 * K
        prof.append((r, -f * L))
    prof.append((0.75 * K, -L - 0.5 * K)); prof.append((0.3 * K, -L - 1.1 * K))      # the drip nub
    prof = list(reversed(prof))            # lathe wants bottom to top
    lathe(bm, prof, base, seg, index, wobble=0.06, rng=rng)


def finish(name, slots, extra=None):
    ob = bpy.context.object
    for o in list(bpy.data.objects):
        o.select_set(o.type == 'MESH')
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = ob.data.name = name
    for p in ob.data.polygons: p.use_smooth = True
    ob.data.update()
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
    lo = [round(100 * min(v[i] for v in vs), 2) for i in range(3)]; hi = [round(100 * max(v[i] for v in vs), 2) for i in range(3)]
    ob.data.calc_loop_triangles()
    info = {'name': name, 'min_cm': lo, 'max_cm': hi, 'slots': [s.material.name for s in ob.material_slots], 'tris': len(ob.data.loop_triangles)}
    info.update(extra or {})
    json.dump(info, open(os.path.join(out, name + '.json'), 'w'), indent=1)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                             mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False)
    print('ENV_PROP', json.dumps(info))


def new_object(name, slots):
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    for s in slots: ob.data.materials.append(mat(s))
    return ob


def write(ob, bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data); bm.free()
    # Box-project UVs so the library's world-aligned finishes and any UV material both have something sane.
    bpy.context.view_layer.objects.active = ob; ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=0.3)
    bpy.ops.object.mode_set(mode='OBJECT')


# ---------------------------------------------------------------------------------------------------------------
def candle_rack():
    reset(); rng = random.Random(11)
    ob = new_object('SM_Env_B_CandleRack', ['Wood', 'Wax', 'Twine', 'Iron']); bm = bmesh.new()
    rod_len, drop = 110 * K, 52 * K                      # broach rod 110 cm, hung 52 cm below the joists
    z = -drop
    cylinder(bm, (-rod_len / 2, 0, z), (rod_len / 2, 0, z), 1.6 * K, seg=10, index=0)
    for x in (-rod_len / 2 + 6 * K, rod_len / 2 - 6 * K):
        cylinder(bm, (x, 0, z + 1.4 * K), (x, 0, -2.5 * K), 0.25 * K, seg=5, index=2)       # cord up to the hook
        cylinder(bm, (x, 0, -2.5 * K), (x, 0, 0), 0.45 * K, seg=6, index=3)                 # iron hook in the joist
    pairs = 8
    for i in range(pairs):
        x = -rod_len / 2 + (i + 0.75) * rod_len / (pairs + 0.5) + rng.uniform(-1.5, 1.5) * K
        for side in (-1, 1):
            # one wick over the rod, a candle on each side
            off = side * 2.4 * K
            cylinder(bm, (x, 0, z + 1.6 * K), (x, off, z - 2.2 * K), 0.18 * K, seg=4, index=2)
            L = rng.uniform(23, 27) * K
            dipped_candle(bm, (x + rng.uniform(-0.3, 0.3) * K, off, z - 2.2 * K), L, rng, 1)
    write(ob, bm)
    bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_CandleRack', None, {'pivot': 'rod centre at the ceiling', 'hang_drop_cm': round(drop * 100, 1)})


def candle_mould():
    reset(); rng = random.Random(5)
    ob = new_object('SM_Env_B_CandleMould', ['Iron', 'Twine', 'Wood']); bm = bmesh.new()
    cols, rows, pitch = 6, 2, 3.8 * K
    w, d = cols * pitch + 2 * K, rows * pitch + 2 * K
    H = 20 * K                                   # short enough for a bookcase shelf (31 cm clear)
    box(bm, (-w / 2, -d / 2, H - 1.2 * K), (w / 2, d / 2, H), 0)                    # the trough plate on top
    box(bm, (-w / 2, -d / 2, H - 4 * K), (-w / 2 + 0.3 * K, d / 2, H), 0)           # trough walls
    box(bm, (w / 2 - 0.3 * K, -d / 2, H - 4 * K), (w / 2, d / 2, H), 0)
    box(bm, (-w / 2, -d / 2, H - 4 * K), (w / 2, -d / 2 + 0.3 * K, H), 0)
    box(bm, (-w / 2, d / 2 - 0.3 * K, H - 4 * K), (w / 2, d / 2, H), 0)
    for c in range(cols):
        for r in range(rows):
            x = -w / 2 + 1 * K + (c + 0.5) * pitch; y = -d / 2 + 1 * K + (r + 0.5) * pitch
            cylinder(bm, (x, y, 2.5 * K), (x, y, H - 1.2 * K), 1.1 * K, 1.45 * K, seg=10, index=0)
            cylinder(bm, (x, y, H), (x, y, H + 2.6 * K), 0.12 * K, seg=4, index=1)      # wick up to the stick
    for x in (-w / 2, w / 2 - 1.2 * K):                                                   # end frames to stand on
        box(bm, (x, -d / 2, 0), (x + 1.2 * K, d / 2, H - 1.2 * K), 0)
    cylinder(bm, (-w / 2 - 2 * K, 0, H + 2.8 * K), (w / 2 + 2 * K, 0, H + 2.8 * K), 0.45 * K, seg=6, index=2)   # wick stick
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_CandleMould', None, {'pivot': 'base centre'})


def tallow_cakes():
    reset(); rng = random.Random(3)
    ob = new_object('SM_Env_B_TallowCakes', ['Wood', 'Wax', 'Iron']); bm = bmesh.new()
    bw, bd, bt = 40 * K, 26 * K, 2.2 * K
    box(bm, (-bw / 2, -bd / 2, 0), (bw / 2, bd / 2, bt), 0)
    # cakes: rounded slabs as cast in a pan (domed, the edge slumped), never quite round
    for (cx, cy, r, h) in ((-8, -2, 8.0, 3.6), (4, 3.5, 7.0, 3.3), (11, -5.5, 4.6, 2.8)):
        prof = [(r * 0.92 * K, 0), (r * K, 0.35 * h * K), (r * 0.97 * K, 0.7 * h * K), (r * 0.8 * K, 0.95 * h * K), (r * 0.4 * K, 1.04 * h * K), (0.01 * K, 1.06 * h * K)]
        lathe(bm, prof, (cx * K, cy * K, bt), 18, 1, wobble=0.07, rng=rng)
    for i in range(6):                                                              # shavings
        rock(bm, (rng.uniform(-14, 16) * K, rng.uniform(-10, 10) * K, bt + 0.35 * K), (0.9 * K, 0.6 * K, 0.35 * K), rng, 1, sub=1)
    box(bm, (2 * K, -11 * K, bt), (18 * K, -9.6 * K, bt + 0.35 * K), 2)              # knife blade
    box(bm, (-7 * K, -11.1 * K, bt), (2 * K, -9.5 * K, bt + 1.4 * K), 0)            # handle
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_TallowCakes', None, {'pivot': 'base centre'})


def wick_spool():
    reset(); rng = random.Random(9)
    ob = new_object('SM_Env_B_WickSpool', ['Wood', 'Twine']); bm = bmesh.new()
    lathe(bm, [(3.2 * K, 0), (3.2 * K, 0.9 * K), (1.6 * K, 0.9 * K), (1.6 * K, 7.1 * K), (3.2 * K, 7.1 * K), (3.2 * K, 8 * K)], (0, 0, 0), 16, 0)
    lathe(bm, [(2.6 * K, 0.9 * K), (2.75 * K, 2.5 * K), (2.8 * K, 4 * K), (2.75 * K, 5.5 * K), (2.6 * K, 7.1 * K)], (0, 0, 0), 16, 1, 0.02, rng)
    pts = [(2.7, 0, 4.5), (4, -2, 1.5), (7, -4, 0.15), (11, -3, 0.15), (14, -6, 0.15)]
    for a, b in zip(pts, pts[1:]):
        cylinder(bm, [p * K for p in a], [p * K for p in b], 0.16 * K, seg=4, index=1)
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_WickSpool', None, {'pivot': 'base centre'})


def candle_bundle():
    reset(); rng = random.Random(21)
    ob = new_object('SM_Env_B_CandleBundle', ['Wax', 'Twine']); bm = bmesh.new()
    L = 24 * K
    k = 0
    for row, n in ((0, 5), (1, 4), (2, 3)):
        for i in range(n):
            y = (i - (n - 1) / 2) * 2.1 * K + rng.uniform(-0.2, 0.2) * K
            zc = 1.0 * K + row * 1.75 * K
            x0 = -L / 2 + rng.uniform(-0.8, 0.8) * K
            cylinder(bm, (x0, y, zc), (x0 + L, y, zc), 1.05 * K, 0.75 * K, seg=10, index=0)
            cylinder(bm, (x0 + L, y, zc), (x0 + L + 1.6 * K, y + rng.uniform(-0.5, 0.5) * K, zc + 0.3 * K), 0.12 * K, seg=4, index=1)
            k += 1
    for x in (-4 * K, 6 * K):                                                         # two ties of twine round the bundle
        cylinder(bm, (x, 0, 0), (x + 0.6 * K, 0, 0), 0.01, seg=4, index=1)
        ring = lathe(bm, [(5.6 * K, -0.3 * K), (5.6 * K, 0.3 * K)], (0, 0, 0), 14, 1)
        for rr in ring:
            for v in rr:
                v.co = Matrix.Rotation(math.pi / 2, 4, 'Y') @ v.co
                v.co = Vector((v.co.x + x, v.co.y, v.co.z * 0.62 + 3.6 * K))
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_CandleBundle', None, {'pivot': 'base centre', 'candles': k})


def candle_box():
    reset(); rng = random.Random(2)
    ob = new_object('SM_Env_B_CandleBox', ['Iron']); bm = bmesh.new()
    L, r = 34 * K, 5.2 * K
    # a horizontal tin tube with a hinged lid end, on a back plate with a hanging hole; wall at y = 0, box out to -y
    cylinder(bm, (-L / 2, -r - 1.2 * K, 0), (L / 2, -r - 1.2 * K, 0), r, seg=16, index=0)
    cylinder(bm, (L / 2, -r - 1.2 * K, 0), (L / 2 + 0.6 * K, -r - 1.2 * K, 0), r + 0.5 * K, seg=16, index=0)
    box(bm, (-L / 2 - 1 * K, -1.2 * K, -r), (L / 2 + 1 * K, 0, r), 0)
    box(bm, (-3 * K, -1.0 * K, r), (3 * K, 0, r + 7 * K), 0)
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_CandleBox', None, {'pivot': 'back centre (on the wall), tube axis along x'})


def coal_spill():
    reset(); rng = random.Random(7)
    ob = new_object('SM_Env_B_CoalSpill', ['Coal', 'Iron', 'Wood']); bm = bmesh.new()
    # a fan of lumps from the bunker door (y=0) running out about 60 cm, heaped at the door and thinning out
    for i in range(190):
        d = abs(rng.gauss(0, 1)) * 20
        ang = rng.uniform(-0.8, 0.8)
        x, y = math.sin(ang) * d * 1.4, math.cos(ang) * d + 3
        h = 13 * math.exp(-d / 13)                                  # heap height above this spot
        s = rng.uniform(1.1, 2.8) * (1.2 - min(d, 50) / 90)
        z = rng.uniform(0, max(h - s, 0)) + s * 0.55
        rock(bm, (x * K, y * K, z * K), (s * K, s * 0.85 * K, s * 0.65 * K), rng, 0, sub=1)
    # the coal shovel lying on the heap's edge
    bx, by = 44 * K, 16 * K                                   # beside the heap, not in it; the handle points into the room
    box(bm, (bx - 11 * K, by - 13 * K, 0.4 * K), (bx + 11 * K, by + 13 * K, 0.75 * K), 1)
    box(bm, (bx - 11 * K, by - 13 * K, 0.4 * K), (bx - 10.4 * K, by + 13 * K, 4 * K), 1)
    box(bm, (bx + 10.4 * K, by - 13 * K, 0.4 * K), (bx + 11 * K, by + 13 * K, 4 * K), 1)
    box(bm, (bx - 11 * K, by - 13 * K, 0.4 * K), (bx + 11 * K, by - 12.4 * K, 4 * K), 1)
    cylinder(bm, (bx, by + 13 * K, 2 * K), (bx + 6 * K, by + 60 * K, 6 * K), 1.3 * K, seg=8, index=2)
    cylinder(bm, (bx + 6 * K, by + 60 * K, 6 * K), (bx + 6.6 * K, by + 66 * K, 6.5 * K), 1.6 * K, seg=8, index=2)
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_CoalSpill', None, {'pivot': 'base, heap foot at the bunker door'})


def drape(bm, x0, x1, z, side, depth, rng, index, seg=24, rows=14, fold=1.6, hem=0.0):
    """Cloth hung over a rail along x at height z: falls on one side (+1/-1 in y) to the given depth, folds deepening
    towards the hem. A thin closed shell, so it reads from both sides."""
    grid = []
    phase = rng.uniform(0, 6.28); f2 = rng.uniform(0, 6.28)
    for j in range(rows + 1):
        t = j / rows
        row = []
        for i in range(seg + 1):
            s = i / seg
            x = x0 + (x1 - x0) * s
            a = fold * K * (0.25 + t) * (math.sin(s * 9.0 + phase) + 0.45 * math.sin(s * 23.0 + f2))
            y = side * (0.8 * K + a + 0.6 * K * t)
            zz = z - t * depth - (hem * K * math.sin(s * 3.1 + phase) if j == rows else 0)
            row.append(Vector((x, y, zz)))
        grid.append(row)
    top = [[bm.verts.new(p) for p in row] for row in grid]
    back = [[bm.verts.new(p - Vector((0, side * 0.35 * K, 0))) for p in row] for row in grid]
    fs = []
    for j in range(rows):
        for i in range(seg):
            fs.append(bm.faces.new((top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i])))
            fs.append(bm.faces.new((back[j][i], back[j + 1][i], back[j + 1][i + 1], back[j][i + 1])))
    for j in range(rows):
        for a, b in ((top[j][0], top[j + 1][0]), (top[j][seg], top[j + 1][seg])):
            pass
    for i in range(seg):
        fs.append(bm.faces.new((top[rows][i], back[rows][i], back[rows][i + 1], top[rows][i + 1])))
    for f in fs: f.material_index = index


def airer():
    reset(); rng = random.Random(4)
    ob = new_object('SM_Env_B_Airer', ['Wood', 'Cloth', 'Twine', 'Iron']); bm = bmesh.new()
    L, drop = 95 * K, 20 * K                                  # rails 95 cm, hung 26 cm below the joists
    for y in (-9 * K, -3 * K, 3 * K, 9 * K):                  # four rails between two end frames
        cylinder(bm, (-L / 2, y, -drop), (L / 2, y, -drop), 0.9 * K, seg=8, index=0)
    for x in (-L / 2 + 2 * K, L / 2 - 2 * K):
        box(bm, (x - 0.8 * K, -11 * K, -drop - 1.5 * K), (x + 0.8 * K, 11 * K, -drop + 1.5 * K), 0)
        cylinder(bm, (x, 0, -drop + 1.5 * K), (x, 0, -2 * K), 0.22 * K, seg=4, index=2)     # cord to the pulley
        cylinder(bm, (x, 0, -2 * K), (x, 0, 0), 1.2 * K, seg=8, index=3)                     # pulley block
    # a sheet over the two inner rails, falling both sides; a child's shift over an outer rail
    drape(bm, -40 * K, 30 * K, -drop + 0.9 * K, 1, 42 * K, rng, 1, hem=1.5)
    drape(bm, -40 * K, 30 * K, -drop + 0.9 * K, -1, 34 * K, rng, 1, hem=1.0)
    drape(bm, 33 * K, 44 * K, -drop + 0.9 * K, -1, 30 * K, rng, 1, seg=10, fold=0.9, hem=2.0)
    write(ob, bm); bpy.context.view_layer.objects.active = ob
    finish('SM_Env_B_Airer', None, {'pivot': 'centre at the ceiling'})


for build in (candle_rack, candle_mould, tallow_cakes, wick_spool, candle_bundle, candle_box, coal_spill, airer):
    build()
