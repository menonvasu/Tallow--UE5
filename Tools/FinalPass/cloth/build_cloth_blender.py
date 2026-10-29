"""Cloth that hangs like cloth: dust sheets simulated over what they cover, and laundry hung over the airer's rails.
Replaces the stiff bell and cone sheets (SM_HouseV04_DustSheetTall / DustSheetChair) and the airer's sheet.

blender -b --factory-startup --python build_cloth_blender.py -- <out_dir> [piece,piece]
Pieces (FBX, Unreal cm after export; front faces Blender -Y = Unreal +Y; pivot at the base centre unless noted):
  SM_Cloth_SheetedForm   a dust sheet thrown over a dress form (the house's own SM_HouseV04_DressForm, kept under it for
                         its stand); slots Cloth, Form, Iron, Wood, Brass. Stands in for the tall dust sheet.
  SM_Cloth_SheetedChair  a dust sheet thrown over a buttoned armchair (the chair is built here, legs only show);
                         slots Cloth, Wood. Stands in for the chair dust sheet.
  SM_Cloth_Airer         the laundry airer of SM_Env_B_Airer (its frame, cords and pulleys) with a sheet folded over the two
                         inner rails and a child's pinafore over an outer one (pivot: centre at the ceiling); slots Wood,
                         Cloth, Twine, Iron.
Every sheet is a flat plane of real size (house scale, 1.3 x real) thrown onto what it covers and left to fall with
Blender's cloth solver (linen weight, self collision), then smoothed, thinned and given thickness. The walls and the
neighbouring dress forms each piece stands beside in the map (probe_space.py) are not in the solve (a flat sheet would
start through them): fit() measures the settled hem against them, and apply_cloth.py turns a piece where needed.
CLOTH_CACHE=<dir> keeps the settled shapes and reuses them, for finishing passes without solving again.
Vertex colour: R = occlusion (rays against the settled scene), G = dust on what faces up, B = grime near the floor;
UVs: 1 unit = 35 cm of cloth (the Linen scan's repeat at house scale). Writes <name>.json beside each FBX.
"""
import json
import math
import os
import random
import sys
import time
import bmesh
import bpy
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index('--') + 1:]
out = argv[0]
want = argv[1].split(',') if len(argv) > 1 and not argv[1].startswith('--') else None
os.makedirs(out, exist_ok=True)
HOUSE = os.path.expanduser('~/Projects/tallow-current/Tallow/SourceAssets/Production/HouseV04Preview/Export')
AIRER = os.path.join(os.path.dirname(os.path.abspath(out.rstrip('/'))), 'Cellar', 'SM_Env_B_Airer.fbx')
K = 1.3 / 100.0          # real cm -> Blender metres at house scale
UV = 0.35                # metres of cloth per UV unit
SC = bpy.context.scene
CACHE = os.environ.get('CLOTH_CACHE')          # a folder: settled shapes are kept there and reused (finishing passes only)
PIECE = ['']


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    global SC
    SC = bpy.context.scene


def mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def link(ob):
    SC.collection.objects.link(ob)
    return ob


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    obs = [o for o in bpy.data.objects if o not in before]
    for o in [o for o in obs if o.type != 'MESH']: bpy.data.objects.remove(o)
    return [o for o in bpy.data.objects if o not in before]


def block(name, lo, hi, bevel=0.0):
    """A collider box (Blender metres), optionally with rounded edges so cloth slides over it like padding."""
    me = bpy.data.meshes.new(name); ob = link(bpy.data.objects.new(name, me))
    bm = bmesh.new(); r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r['verts']:
        v.co = Vector((lo[0] if v.co.x < 0 else hi[0], lo[1] if v.co.y < 0 else hi[1], lo[2] if v.co.z < 0 else hi[2]))
    if bevel:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=3, affect='EDGES', profile=0.5)
    bm.to_mesh(me); bm.free()
    return ob


def collide(ob, thickness=0.008, friction=5.0):
    ob.modifiers.new('Collision', 'COLLISION')
    ob.collision.thickness_outer = thickness; ob.collision.cloth_friction = friction
    ob.collision.damping = 0.3
    return ob


def sheet(name, w, h, step, z, rng, yaw=0.0, offset=(0.0, 0.0), shape=None, noise=0.012):
    """A sheet w x h (metres) of quads about `step` apart at height z, turned by yaw (degrees) about its centre and moved
    by offset. shape(x, y, z) -> (x, y, z) starts it already hanging (hung()): the flat sheet is kept as the 'Flat'
    shape key, which the solver takes its rest lengths from, so the cloth falls into its own folds instead of keeping
    the start shape. Small random ripples break the symmetry the way a thrown sheet does."""
    nx, ny = max(2, round(w / step)), max(2, round(h / step))
    me = bpy.data.meshes.new(name); ob = link(bpy.data.objects.new(name, me))
    ob.data.materials.append(mat('Cloth'))
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new('UVMap')
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    ph = [rng.uniform(0, 6.3) for _ in range(4)]
    grid, flat = [], []
    for j in range(ny + 1):
        row = []
        for i in range(nx + 1):
            x, y = -w / 2 + w * i / nx, -h / 2 + h * j / ny
            dz = noise * (math.sin(x * 7.1 + ph[0]) * math.sin(y * 5.3 + ph[1]) + 0.5 * math.sin(x * 13.7 + y * 11.1 + ph[2]))
            p = (offset[0] + c * x - s * y, offset[1] + s * x + c * y, z + dz)
            flat.append(p)
            v = bm.verts.new(shape(*p) if shape else p)
            row.append((v, x, y))
        grid.append(row)
    for j in range(ny):
        for i in range(nx):
            q = [grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]]
            f = bm.faces.new([p[0] for p in q])
            for loop, (_, x, y) in zip(f.loops, q):
                loop[uvl].uv = ((x + w / 2) / UV, (y + h / 2) / UV)
    bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    if shape:
        ob.shape_key_add(name='Basis'); k = ob.shape_key_add(name='Flat'); k.value = 0.0
        for d, p in zip(k.data, flat): d.co = p
    return ob


def hung(cx, cy, k, r0, rng, wave=0.022, length=0.15):
    """Start shape over a rail pair: gathered along the rail by k about cx, then hanging straight down past r0 either side
    in loose pleats, so the cloth falls into soft folds from the rails instead of hanging as a board."""
    ph = [rng.uniform(0, 6.3) for _ in range(4)]
    def m(x, y, z):
        X = cx + (x - cx) * k; e = abs(y - cy) - r0
        if e <= 0: return (X, y, z)
        side = 1 if y > cy else -1
        t = (x - cx) * 2 * math.pi
        pleat = wave * min(1.0, e / 0.12) * (1.0 + 0.6 * math.sin(t / length + ph[side > 0])
                                             + 0.4 * math.sin(t / (length * 0.43) + ph[2 + (side > 0)]) * math.sin(t / (length * 2.7)))
        return (X, cy + side * (r0 + 0.012 + pleat), z - e)
    return m


def pin(cloth, centre, radius):
    """Hold the points within radius of centre (x, y) where they start: the patch that rests on a post's top."""
    g = cloth.vertex_groups.new(name='Pin')
    ids = [v.index for v in cloth.data.vertices if math.hypot(v.co.x - centre[0], v.co.y - centre[1]) <= radius]
    g.add(ids, 1.0, 'REPLACE')
    return len(ids)


def simulate(cloth, frames, mass=0.3, bending=0.5, stiff=15.0, self_collide=True, quality=6, friction=5.0, air=1.0):
    """Drop the cloth onto every collider in the scene and keep the settled shape."""
    import numpy as np
    key = os.path.join(CACHE, f'{PIECE[0]}-{cloth.name}-{len(cloth.data.vertices)}-{frames}.npy') if CACHE else None
    if key and os.path.exists(key):
        co = np.load(key); cloth.shape_key_clear(); cloth.data.vertices.foreach_set('co', co.ravel()); cloth.data.update()
        return 0.0
    m = cloth.modifiers.new('Cloth', 'CLOTH'); s = m.settings
    if cloth.data.shape_keys: s.rest_shape_key = cloth.data.shape_keys.key_blocks['Flat']
    if cloth.vertex_groups.get('Pin'): s.vertex_group_mass = 'Pin'; s.pin_stiffness = 1.0
    s.quality = quality; s.mass = mass; s.air_damping = air
    s.tension_stiffness = s.compression_stiffness = s.shear_stiffness = stiff; s.bending_stiffness = bending
    s.tension_damping = s.compression_damping = s.shear_damping = 5.0; s.bending_damping = 0.5
    cs = m.collision_settings
    cs.use_collision = True; cs.distance_min = 0.006; cs.collision_quality = 3; cs.friction = friction
    cs.use_self_collision = self_collide; cs.self_distance_min = 0.004; cs.self_friction = 5.0
    m.point_cache.frame_start = 1; m.point_cache.frame_end = frames
    SC.frame_start, SC.frame_end = 1, frames
    t0 = time.time()
    for f in range(1, frames + 1):
        SC.frame_set(f)
        if f % 20 == 0: print('CLOTH_SIM', cloth.name, f, round(time.time() - t0, 1), flush=True)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(cloth.evaluated_get(dg))
    old = cloth.data; cloth.modifiers.clear(); cloth.data = me; bpy.data.meshes.remove(old)
    me.materials.clear(); me.materials.append(mat('Cloth'))
    if key:
        co = np.zeros(len(me.vertices) * 3, dtype=np.float32); me.vertices.foreach_get('co', co); np.save(key, co)
    return round(time.time() - t0, 1)


def settle_report(cloth):
    vs = [cloth.matrix_world @ v.co for v in cloth.data.vertices]
    return {'min': [round(min(v[i] for v in vs), 3) for i in range(3)], 'max': [round(max(v[i] for v in vs), 3) for i in range(3)]}


def finish_cloth(cloth, scene_obs, ratio, thickness=0.004, smooth=6):
    """Smooth the solver's jitter, thin the mesh, give the cloth its thickness, and paint occlusion / dust / grime."""
    bpy.context.view_layer.objects.active = cloth
    if smooth:
        m = cloth.modifiers.new('Smooth', 'SMOOTH'); m.iterations = smooth; m.factor = 0.5
    if ratio < 1.0:
        d = cloth.modifiers.new('Thin', 'DECIMATE'); d.ratio = ratio; d.use_collapse_triangulate = False
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(cloth.evaluated_get(dg))
    old = cloth.data; cloth.modifiers.clear(); cloth.data = me; bpy.data.meshes.remove(old)
    paint(cloth, scene_obs)
    so = cloth.modifiers.new('Thick', 'SOLIDIFY'); so.thickness = thickness; so.offset = -1.0; so.use_rim = True
    so.use_even_offset = False; so.thickness_clamp = 1.0
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(cloth.evaluated_get(dg))
    old = cloth.data; cloth.modifiers.clear(); cloth.data = me; bpy.data.meshes.remove(old)
    for p in me.polygons: p.use_smooth = True


def paint(cloth, others):
    """R: occlusion from 24 rays over the hemisphere (cloth and colliders); G: dust where the cloth faces up, more on
    top; B: grime in the last 12 cm above the floor."""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    for o in [cloth] + others:
        e = o.evaluated_get(dg); me = e.to_mesh()
        tmp = bmesh.new(); tmp.from_mesh(me); tmp.transform(o.matrix_world)
        m2 = bpy.data.meshes.new('tmp'); tmp.to_mesh(m2); tmp.free(); bm.from_mesh(m2); bpy.data.meshes.remove(m2)
        e.to_mesh_clear()
    tree = BVHTree.FromBMesh(bm); bm.free()
    me = cloth.data
    col = me.color_attributes.get('Col') or me.color_attributes.new('Col', 'BYTE_COLOR', 'POINT')
    rng = random.Random(7)
    dirs = []
    while len(dirs) < 24:
        v = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
        if 0.05 < v.length <= 1: dirs.append(v.normalized())
    zs = [v.co.z for v in me.vertices]; ztop = max(zs); zlow = min(zs)
    for v in me.vertices:
        p = cloth.matrix_world @ v.co; n = (cloth.matrix_world.to_3x3() @ v.normal).normalized()
        occ = 0.0
        for side in (1, -1):                   # the sheet is seen from both sides: take the more open one
            hits = 0
            for d in dirs:
                if d.dot(n * side) <= 0: d = -d
                if tree.ray_cast(p + n * side * 0.004, d, 0.35)[0] is not None: hits += 1
            occ = max(occ, 1 - hits / len(dirs))
        up = max(0.0, n.z if n.z > 0 else -n.z)
        dust = (up ** 3) * (0.4 + 0.6 * (p.z - zlow) / max(0.01, ztop - zlow))
        grime = max(0.0, 1 - (p.z - zlow) / 0.12)
        col.data[v.index].color = (occ, min(1.0, dust), grime, 1.0)


def keep_only(obs):
    for o in list(bpy.data.objects):
        if o not in obs: bpy.data.objects.remove(o)


def export(name, obs, extra):
    keep_only(obs)
    bpy.ops.object.select_all(action='DESELECT')
    for o in obs: o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    if len(obs) > 1: bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = ob.data.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
    lo = [round(100 * min(v[i] for v in vs), 2) for i in range(3)]; hi = [round(100 * max(v[i] for v in vs), 2) for i in range(3)]
    ob.data.calc_loop_triangles()
    info = {'name': name, 'min_cm': lo, 'max_cm': hi, 'slots': [s.material.name for s in ob.material_slots], 'tris': len(ob.data.loop_triangles)}
    info.update(extra)
    json.dump(info, open(os.path.join(out, name + '.json'), 'w'), indent=1)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                             mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False, colors_type='LINEAR')
    print('CLOTH_PROP', json.dumps(info), flush=True)


# ---------------------------------------------------------------------------------------------------------------
def walls():
    return [collide(block('floor', (-3, -3, -0.2), (3, 3, 0.0)), 0.004, 8.0)]


def dress_form(at=(0, 0), yaw=0.0):
    obs = import_fbx(os.path.join(HOUSE, 'SM_HouseV04_DressForm.fbx'))
    ob = obs[0]
    names = {'M_HouseV04_Linen': 'Form', 'M_HouseV04_Iron': 'Iron', 'M_HouseV04_WarmOak': 'Wood', 'M_HouseV04_Brass': 'Brass'}
    for s in ob.material_slots:
        base = s.material.name.split('.')[0]; s.material = mat(names.get(base, base))
    ob.location = (at[0], at[1], 0.0); ob.rotation_euler = (0, 0, math.radians(yaw))
    bpy.context.view_layer.update()
    return ob


def fit(cloth, xmax=None, ymax=None, xmin=None, ymin=None, others=()):
    """How much room the settled cloth leaves: to each wall plane and to each neighbour's surface (metres; negative =
    through it)."""
    vs = [cloth.matrix_world @ v.co for v in cloth.data.vertices]
    room = {}
    if xmax is not None: room['xmax'] = round(xmax - max(v.x for v in vs), 3)
    if ymax is not None: room['ymax'] = round(ymax - max(v.y for v in vs), 3)
    if xmin is not None: room['xmin'] = round(min(v.x for v in vs) - xmin, 3)
    if ymin is not None: room['ymin'] = round(min(v.y for v in vs) - ymin, 3)
    dg = bpy.context.evaluated_depsgraph_get()
    for o in others:
        bm = bmesh.new(); bm.from_mesh(o.evaluated_get(dg).to_mesh()); bm.transform(o.matrix_world)
        tree = BVHTree.FromBMesh(bm); bm.free()
        room[o.name] = round(min(tree.find_nearest(v)[3] for v in vs), 3)
    print('CLOTH_FIT', cloth.name, json.dumps(room), flush=True)
    return room


# Sheets that stand in the store: thrown on flat from above and left to fall (no start shape, so the folds are the
# solver's own). The walls and neighbours are not in the solve (a flat sheet would start through them); fit() measures
# the settled hem against them instead, and the size and throw are chosen so it clears them.
FORM_THROW = dict(size=(180, 240), yaw=8.0, offset=(0.0, -0.03), z=1.722, seed=11, friction=12.0, air=1.0, frames=160, pin=0.035)
CHAIR_THROW = dict(size=(190, 205), yaw=-4.0, offset=(0.02, -0.12), z=1.34, seed=5)
if os.environ.get('CLOTH_THROW'):          # trials: CLOTH_THROW='{"size": [180, 240], "yaw": 8}' overrides the throw
    FORM_THROW.update(json.loads(os.environ['CLOTH_THROW'])); CHAIR_THROW.update(json.loads(os.environ['CLOTH_THROW']))


def sheeted_form():
    """Unreal: B3 store at (900, -1236) beside the east wall (52.5 cm off) and the south wall (41.4 cm), the next dress
    form 88 cm west; U3 landing at 0.8 scale, 31.5 cm off its east wall (39 cm unscaled). The hem has to clear the
    tighter of both: x +0.39, Blender y +0.41 (= Unreal -y). The form faces Unreal +y (Blender -y), up the hall."""
    T = FORM_THROW; reset(); rng = random.Random(T['seed'])
    form = collide(dress_form(), 0.02, T['friction'])
    # the neck post is thinner than the cloth's mesh: a round cap over it (hidden under the sheet), and the few points of
    # the sheet that land on its top held there (cloth friction alone lets a sheet slide off a post), so the sheet rests
    # on the post instead of the post poking through between the cloth's points
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.07, location=(0, 0, 1.635), segments=24, ring_count=12)
    cap = collide(bpy.context.object, 0.01, T['friction'])
    floor = walls()
    # a single bed sheet (real 180 x 240 cm) thrown on from the front: it falls to the floor at the front and back
    # corners, stops short of it at the sides and shows the stand's feet
    cloth = sheet('SheetCloth', T['size'][0] * K, T['size'][1] * K, 0.025, T['z'], rng, yaw=T['yaw'], offset=T['offset'], noise=0.004)
    pin(cloth, (0.0, 0.0), T['pin'])
    t = simulate(cloth, T['frames'], mass=0.4, bending=2.0, quality=8, friction=T['friction'], air=T['air'])
    rep = settle_report(cloth)
    neighbour = dress_form((-0.88, 0.02)); neighbour.name = 'next_form'
    room = fit(cloth, xmax=0.39, ymax=0.41, others=[neighbour])
    bpy.data.objects.remove(neighbour)
    finish_cloth(cloth, [form, cap] + floor, ratio=0.5)
    export('SM_Cloth_SheetedForm', [cloth, form], {'pivot': 'base centre', 'sim_seconds': t, 'settled': rep, 'room': room,
                                                  'throw': T, 'covers': 'SM_HouseV04_DressForm'})


def armchair():
    """A buttoned Victorian armchair as the sheet sees it: seat, rolled arms, a tall back, turned legs. Blender m."""
    W, D = 0.86, 0.80                    # outer width, depth (house scale)
    parts = [
        block('seat', (-W / 2 + 0.1, -D / 2, 0.26), (W / 2 - 0.1, D / 2 - 0.12, 0.52), 0.05),
        block('armL', (-W / 2, -D / 2 + 0.02, 0.26), (-W / 2 + 0.13, D / 2 - 0.05, 0.74), 0.06),
        block('armR', (W / 2 - 0.13, -D / 2 + 0.02, 0.26), (W / 2, D / 2 - 0.05, 0.74), 0.06),
        block('back', (-W / 2 + 0.02, D / 2 - 0.18, 0.26), (W / 2 - 0.02, D / 2, 1.12), 0.08),
        block('crest', (-W / 2 + 0.12, D / 2 - 0.2, 1.04), (W / 2 - 0.12, D / 2 - 0.02, 1.2), 0.08),
    ]
    legs = []
    for x in (-W / 2 + 0.06, W / 2 - 0.06):
        for y in (-D / 2 + 0.06, D / 2 - 0.06):
            legs.append(block('leg', (x - 0.03, y - 0.03, 0.0), (x + 0.03, y + 0.03, 0.27), 0.01))
    for o in legs: o.data.materials.append(mat('Wood'))
    return parts, legs


def sheeted_chair():
    """Unreal: B3 store at (700, -1220), back to the south wall (57.5 cm off), the west wall 92.5 cm off, dress forms at
    (0, +84) turned -70 and (+112, -18). Faces Unreal +y (Blender -y), into the store."""
    T = CHAIR_THROW; reset(); rng = random.Random(T['seed'])
    parts, legs = armchair()
    for o in parts + legs: collide(o, 0.012, 10.0)
    floor = walls()
    # a double sheet (real 190 x 205 cm) thrown on from the front: it falls to the floor at the sides, just short of it in
    # front and down the back against the wall without heaping there
    cloth = sheet('SheetCloth', T['size'][0] * K, T['size'][1] * K, 0.025, T['z'], rng, yaw=T['yaw'], offset=T['offset'], noise=0.01)
    t = simulate(cloth, 180, mass=0.4, bending=2.0, quality=8, friction=10.0)
    rep = settle_report(cloth)
    others = [dress_form((0.0, -0.84), 70.0), dress_form((1.12, 0.18))]
    others[0].name, others[1].name = 'form_front', 'form_east'
    room = fit(cloth, ymax=0.565, xmin=-0.915, others=others)
    for o in others: bpy.data.objects.remove(o)
    finish_cloth(cloth, parts + legs + floor, ratio=0.5)
    export('SM_Cloth_SheetedChair', [cloth] + legs, {'pivot': 'base centre', 'sim_seconds': t, 'settled': rep, 'room': room,
                                                    'throw': T, 'covers': 'armchair (legs kept)'})


def airer():
    """The airer of SM_Env_B_Airer without its sheet and shift. Rails (Blender, airer local, pivot at the ceiling) at
    y -9, -3, 3, 9 (x K) and z -20 K, 95 K long along x. A sheet goes over the two inner rails and falls both sides; a
    child's pinafore over the outer rail at +y."""
    reset(); rng = random.Random(3)
    frame = import_fbx(AIRER)[0]
    bpy.context.view_layer.objects.active = frame
    names = [s.material.name.split('.')[0] for s in frame.material_slots]
    bm = bmesh.new(); bm.from_mesh(frame.data)
    cloth_idx = names.index('Cloth')
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == cloth_idx], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(frame.data); bm.free()
    for s in frame.material_slots: s.material = mat(s.material.name.split('.')[0])
    bpy.context.view_layer.update()
    # rails as smooth colliders (the mesh rails are 8-sided)
    rails = []
    L, drop = 95 * K, 20 * K
    for y in (-9 * K, -3 * K, 3 * K, 9 * K):
        me = bpy.data.meshes.new('rail'); ob = link(bpy.data.objects.new('rail', me))
        bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.9 * K, radius2=0.9 * K, depth=L)
        bm.to_mesh(me); bm.free()
        ob.rotation_euler = (0, math.radians(90), 0); ob.location = (0, y, -drop)
        rails.append(collide(ob, 0.004, 20.0))
    # a cot sheet (real 58 wide along the rails, 88 across) laid over both inner rails, falling 40 cm a side; a child's
    # pinafore (real 28 x 46 cm) over the outer rail at the free end
    s1 = sheet('SheetCloth', 66 * K, 88 * K, 0.02, -drop + 0.02, rng, yaw=4.0, offset=(-14 * K, 0.04), noise=0.006,
               shape=hung(-14 * K, 0.0, 0.87, 0.05, rng, 0.016, 0.21))
    t1 = simulate(s1, 140, mass=0.5, bending=0.15, stiff=10.0, friction=20.0)
    rep = settle_report(s1)
    collide(s1, 0.004, 10.0)
    s2 = sheet('ShiftCloth', 30 * K, 46 * K, 0.015, -drop + 0.02, rng, yaw=-5.0, offset=(31 * K, 9 * K), noise=0.005,
               shape=hung(31 * K, 9 * K, 0.85, 0.02, rng, 0.012, 0.13))
    t2 = simulate(s2, 120, mass=0.3, bending=0.1, stiff=8.0, friction=20.0)
    s1.modifiers.clear()
    finish_cloth(s1, rails, ratio=0.7, thickness=0.003)
    finish_cloth(s2, rails, ratio=0.8, thickness=0.003)
    for o in (s1, s2):
        o.data.materials.clear(); o.data.materials.append(mat('Cloth'))
    # keep the frame's slot order: Wood, Cloth, Twine, Iron
    export('SM_Cloth_Airer', [frame, s1, s2], {'pivot': 'centre at the ceiling', 'sim_seconds': [t1, t2], 'settled': rep})


for name, build in (('SM_Cloth_SheetedForm', sheeted_form), ('SM_Cloth_SheetedChair', sheeted_chair), ('SM_Cloth_Airer', airer)):
    if want and name not in want: continue
    PIECE[0] = name
    build()
