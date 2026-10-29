"""The parlour fireplace opening. The house's chimney breast (SM_HouseV04_Flue_Ground) is closed, so the tiled surround
framed a flat patch of wallpaper. This rebuilds the breast's ground-floor block (same envelope and arris) with a real
firebox cut into it and makes the coal grate that sits inside it.

blender -b --factory-startup --python build_hearth_blender.py -- <hearth-geometry.json> <out_dir>
Outputs (Unreal world coordinates in cm, so the actors sit at the origin):
  SM_Env_G1_ChimneyBreast.fbx  the block minus the firebox (slot 0 wall, slot 1 firebox)
  SM_Env_G1_FireGrate.fbx      cast-iron basket grate, ash pan front and the register plate above the opening
  SM_Env_G1_Coal.fbx           a heap of coal in the basket burnt down to a few embers (slot 1), lumps fallen into the pan
The opening matches the gap in the surround (y -370..-280); the firebox is 36 cm deep with splayed cheeks and a
fireback leaning in towards the throat, the shape of a late Victorian register grate.
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
geom = json.load(open(argv[0])); out = argv[1]
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)


def B(x, y, z):
    """Unreal world cm -> Blender metres (the FBX import flips Y back)."""
    return Vector((x / 100.0, -y / 100.0, z / 100.0))


def material(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def mesh_object(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob)
    return ob


def box(bm, lo, hi, mat=0):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r['verts']:
        v.co = Vector((lo[0] if v.co.x < 0 else hi[0], lo[1] if v.co.y < 0 else hi[1], lo[2] if v.co.z < 0 else hi[2]))
    for f in {f for v in r['verts'] for f in v.link_faces}:
        f.material_index = mat
    return r['verts']


def ubox(bm, a, b, mat=0):
    """Axis box between two Unreal points."""
    pa, pb = B(*a), B(*b)
    return box(bm, [min(pa[i], pb[i]) for i in range(3)], [max(pa[i], pb[i]) for i in range(3)], mat)


def bar(bm, a, b, r, mat=0, seg=8):
    """Round bar between two Unreal points, radius r cm."""
    pa, pb = B(*a), B(*b); d = pb - pa
    res = bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r / 100, radius2=r / 100, depth=d.length)
    rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((pa + pb) / 2) @ rot, verts=res['verts'])
    for f in {f for v in res['verts'] for f in v.link_faces}:
        f.material_index = mat


# 1. Chimney breast: a block with the original's envelope and its 0.8 cm arris, then the firebox cut out of it.
src = geom['HI_SM_HouseV01_Flue_Ground']['sections'][0]
lo = [min(p[i] for p in src['verts']) for i in range(3)]; hi = [max(p[i] for p in src['verts']) for i in range(3)]
bm = bmesh.new()
ubox(bm, lo, hi)
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.008, segments=2, affect='EDGES', profile=0.5)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
for f in bm.faces: f.material_index = 0
breast = mesh_object('SM_Env_G1_ChimneyBreast', bm)
front = max(p[0] for p in src['verts'])          # breast face, x = 117.85
cut = bmesh.new()
# Opening 88 wide x 96 high at the face, 60 wide at the back, back 36 cm in, fireback leaning 12 cm forward at the top.
fx, bx, top = front + 2.0, front - 36.0, 96.0
pts = [(fx, -369.0, -2.0), (fx, -281.0, -2.0), (fx, -281.0, top), (fx, -369.0, top),
       (bx, -355.0, -2.0), (bx, -295.0, -2.0), (bx + 12.0, -295.0, top), (bx + 12.0, -355.0, top)]
cv = [cut.verts.new(B(*p)) for p in pts]
for q in [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]:
    cut.faces.new([cv[i] for i in q])
bmesh.ops.recalc_face_normals(cut, faces=cut.faces)
cutter = mesh_object('cutter', cut)
breast.data.materials.append(material('Wall')); breast.data.materials.append(material('Firebox'))
cutter.data.materials.append(material('Firebox'))
mod = breast.modifiers.new('firebox', 'BOOLEAN'); mod.operation = 'DIFFERENCE'; mod.object = cutter; mod.solver = 'EXACT'
mod.material_mode = 'TRANSFER'
bpy.context.view_layer.objects.active = breast
bpy.ops.object.modifier_apply(modifier=mod.name)
bpy.data.objects.remove(cutter)

# 2. Grate: basket of round bars on short legs, solid ash-pan front, register plate closing the top of the opening.
bm = bmesh.new()
gx0, gx1 = front - 30.0, front - 4.0       # basket depth
gy0, gy1 = -350.0, -300.0
for k in range(5):                          # front bars
    z = 16.0 + 6.0 * k
    bar(bm, (gx1, gy0, z), (gx1, gy1, z), 0.9)
for y in (gy0, gy1):                        # sides
    for k in range(5):
        z = 16.0 + 6.0 * k
        bar(bm, (gx0 + 4.0, y, z), (gx1, y, z), 0.8)
    bar(bm, (gx1, y, 10.0), (gx1, y, 42.0), 1.4)                     # front posts
    bar(bm, (gx0 + 4.0, y, 10.0), (gx0 + 4.0, y, 40.0), 1.2)         # back posts
for k in range(7):                          # bottom grid
    y = gy0 + (gy1 - gy0) * k / 6.0
    bar(bm, (gx0 + 4.0, y, 14.0), (gx1, y, 14.0), 0.7)
for y in (gy0 + 2.0, gy1 - 2.0):           # legs to the hearth
    for x in (gx0 + 6.0, gx1 - 1.0):
        bar(bm, (x, y, 0.0), (x, y, 14.0), 1.3)
ubox(bm, (front - 2.0, -352.0, 0.0), (front + 1.0, -298.0, 11.0))                # ash-pan front (fret)
ubox(bm, (front + 0.2, -370.0, 92.0), (front + 2.0, -280.0, 111.0))              # register plate
ubox(bm, (front + 0.2, -370.0, 0.0), (front + 2.0, -368.0, 92.0))                # cheek strips of the iron frame
ubox(bm, (front + 0.2, -282.0, 0.0), (front + 2.0, -280.0, 92.0))
grate = mesh_object('SM_Env_G1_FireGrate', bm)
grate.data.materials.append(material('Iron'))

# 3. Coal: irregular lumps heaped in the basket, burnt down to the last few embers.
rnd = random.Random(1894)
bm = bmesh.new()
for i in range(70):
    u, v = rnd.random(), rnd.random()
    x = gx0 + 6.0 + u * (gx1 - gx0 - 9.0); y = gy0 + 3.0 + v * (gy1 - gy0 - 6.0)
    mound = 1.0 - ((u - .5) ** 2 + (v - .5) ** 2) * 2.2
    z = 17.0 + rnd.random() * 14.0 * max(mound, .2)
    r = 2.0 + rnd.random() * 2.6
    res = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=r / 100)
    for vtx in res['verts']:
        vtx.co = Vector((vtx.co.x * (0.8 + .5 * rnd.random()), vtx.co.y * (0.8 + .5 * rnd.random()), vtx.co.z * (0.6 + .4 * rnd.random())))
    rot = Matrix.Rotation(rnd.random() * math.tau, 4, 'Z') @ Matrix.Rotation(rnd.random() * .8, 4, 'X')
    bmesh.ops.transform(bm, matrix=Matrix.Translation(B(x, y, z)) @ rot, verts=res['verts'])
# A handful still alight, low in the middle of the heap (slot 1, Ember).
for i in range(7):
    x = gx0 + 9.0 + rnd.random() * (gx1 - gx0 - 15.0); y = -338.0 + rnd.random() * 26.0; z = 16.5 + rnd.random() * 5.0
    res = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=(1.8 + rnd.random()) / 100)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(B(x, y, z)), verts=res['verts'])
    for f in {f for v in res['verts'] for f in v.link_faces}:
        f.material_index = 1
for i in range(6):                          # fallen into the pan
    x = front - 3.0 + rnd.random() * 3.0; y = gy0 + 6.0 + rnd.random() * (gy1 - gy0 - 12.0)
    res = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=(1.2 + .8 * rnd.random()) / 100)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(B(x + 6.0, y, 2.5)), verts=res['verts'])
coal = mesh_object('SM_Env_G1_Coal', bm)
coal.data.materials.append(material('Coal')); coal.data.materials.append(material('Ember'))

info = {}
for ob in (breast, grate, coal):
    ob.data.update(); ob.data.calc_loop_triangles()
    vv = [v.co for v in ob.data.vertices]
    info[ob.name] = {'tris': len(ob.data.loop_triangles), 'min_ue': [round(100 * min(v[0] for v in vv), 1), round(-100 * max(v[1] for v in vv), 1), round(100 * min(v[2] for v in vv), 1)],
                     'max_ue': [round(100 * max(v[0] for v in vv), 1), round(-100 * min(v[1] for v in vv), 1), round(100 * max(v[2] for v in vv), 1)],
                     'slots': [m.name for m in ob.data.materials]}
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, ob.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                             mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False)
json.dump(info, open(os.path.join(out, 'hearth.json'), 'w'), indent=1)
print('ENV_HEARTH', json.dumps(info))
