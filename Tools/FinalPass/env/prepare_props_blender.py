"""Prepare downloaded BlenderKit models for Unreal: drop parts we don't use, join, fit to the size the room needs,
pivot at the base centre, export FBX with one material slot per source material, and (bake=True) write each slot's
own maps as Unreal-ready PNGs: <SM>_<slot>_D (colour), _N (normal, DirectX), _ARM (AO 1, roughness, metal).

blender -b --factory-startup <model.blend> --python prepare_props_blender.py -- <name> <out_dir>
Names and their recipes are in SPEC. Orientation: the front faces Blender -Y, which arrives in Unreal facing +Y.
Writes <out_dir>/<SM name>.fbx and <SM name>.json (bounds in cm, slots, triangle count).
"""
import json
import os
import sys
import bpy
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
name, out = argv[0], argv[1]
os.makedirs(out, exist_ok=True)
SPEC = {
    'FireTools': dict(sm='SM_Env_G1_FireTools', drop=(), scale=1.25, budget=20000),
    'Cooker': dict(sm='SM_Env_G5_Cooker', drop=(), scale=1.3, budget=20000, bake=True),
    'StudentLamp': dict(sm='SM_Env_G4_StudentLamp', drop=(), scale=1.15, budget=24000, bake=True),
}
spec = SPEC[name]
for o in list(bpy.data.objects):
    if o.type != 'MESH' or any(o.name.startswith(d) for d in spec['drop']):
        if o.type == 'MESH':
            bpy.data.objects.remove(o)
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
for o in meshes:
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
for o in [o for o in bpy.data.objects if o.type != 'MESH']:
    bpy.data.objects.remove(o)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.make_single_user(object=True, obdata=True)
bpy.ops.object.convert(target='MESH')          # bake the source's subdivision so the budget below is the real count
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.join()
ob = bpy.context.object
ob.data.calc_loop_triangles()
if len(ob.data.loop_triangles) > spec['budget']:
    dec = ob.modifiers.new('budget', 'DECIMATE'); dec.ratio = spec['budget'] / len(ob.data.loop_triangles)
    bpy.ops.object.modifier_apply(modifier=dec.name)
ob.name = ob.data.name = spec['sm']
vs = [v.co for v in ob.data.vertices]
lo = Vector([min(v[i] for v in vs) for i in range(3)]); hi = Vector([max(v[i] for v in vs) for i in range(3)])
ob.data.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
size = hi - lo
if 'fit' in spec:
    fx, fy, fz = spec['fit']
    s = Matrix.Diagonal((fx / 100 / size.x, fy / 100 / size.y, fz / 100 / size.z, 1))
else:
    s = Matrix.Scale(spec['scale'], 4)
ob.data.transform(s)
ob.data.update()
# Unreal reads the slot names; keep them short and stable.
for i, slot in enumerate(ob.material_slots):
    if slot.material:
        slot.material.name = 'Src_' + ''.join(c if c.isalnum() else '_' for c in slot.material.name)
vs = [v.co for v in ob.data.vertices]
lo = [round(100 * min(v[i] for v in vs), 2) for i in range(3)]; hi = [round(100 * max(v[i] for v in vs), 2) for i in range(3)]
ob.data.calc_loop_triangles()
info = {'name': spec['sm'], 'min_cm': lo, 'max_cm': hi, 'slots': [s.material.name if s.material else None for s in ob.material_slots],
        'tris': len(ob.data.loop_triangles), 'source': bpy.data.filepath and os.path.basename(bpy.data.filepath)}
json.dump(info, open(os.path.join(out, spec['sm'] + '.json'), 'w'), indent=1)
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=os.path.join(out, spec['sm'] + '.fbx'), use_selection=True, object_types={'MESH'},
                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                         mesh_smooth_type='FACE', path_mode='STRIP', embed_textures=False)

def upstream_image(sock, depth=0):
    """(image, channel 0-2 or None for rgb, inverted) feeding a socket, through separate/invert/normal-map nodes."""
    if not sock.is_linked or depth > 6: return None
    link = sock.links[0]; n = link.from_node
    if n.type == 'TEX_IMAGE' and n.image: return (n.image, None, False)
    if n.type in ('SEPARATE_COLOR', 'SEPRGB', 'SEPARATE_RGB'):
        ch = ['Red', 'Green', 'Blue', 'R', 'G', 'B'].index(link.from_socket.name) % 3
        r = upstream_image(n.inputs[0], depth + 1)
        return (r[0], ch, r[2]) if r else None
    if n.type == 'INVERT':
        r = upstream_image(n.inputs['Color'], depth + 1)
        return (r[0], r[1], not r[2]) if r else None
    for inp in n.inputs:
        if inp.type in ('RGBA', 'VECTOR', 'VALUE') and inp.is_linked:
            r = upstream_image(inp, depth + 1)
            if r: return r
    return None


def pixels(img):
    import numpy as np
    w, h = img.size; a = np.empty(w * h * 4, dtype=np.float32); img.pixels.foreach_get(a)
    return a.reshape(h, w, 4), w, h


def save(arr, w, h, path, srgb):
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False, float_buffer=False, is_data=not srgb)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    img.pixels.foreach_set(arr.reshape(-1)); img.filepath_raw = path; img.file_format = 'PNG'; img.save()


if spec.get('bake'):
    import numpy as np
    maps = {}
    for i, slot in enumerate(ob.material_slots):
        m = slot.material
        bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m and m.use_nodes else None
        if not bsdf: continue
        stem = os.path.join(out, f"{spec['sm']}_{i}")
        base = upstream_image(bsdf.inputs['Base Color'])
        if not base: continue
        b, w, h = pixels(base[0]); b[..., 3] = 1.0
        save(b, w, h, stem + '_D.png', True)
        arm = np.ones((h, w, 4), dtype=np.float32)
        for key, ch_out, default in (('Roughness', 1, bsdf.inputs['Roughness'].default_value), ('Metallic', 2, bsdf.inputs['Metallic'].default_value)):
            r = upstream_image(bsdf.inputs[key])
            if r:
                px, rw, rh = pixels(r[0])
                v = px[..., r[1] if r[1] is not None else 0]
                if (rw, rh) != (w, h):
                    v = np.asarray(v[np.linspace(0, rh - 1, h).astype(int)][:, np.linspace(0, rw - 1, w).astype(int)])
                arm[..., ch_out] = 1 - v if r[2] else v
            else:
                arm[..., ch_out] = default
        save(arm, w, h, stem + '_ARM.png', False)
        nm = next((n for n in m.node_tree.nodes if n.type == 'NORMAL_MAP'), None)
        nr = upstream_image(nm.inputs['Color']) if nm else None
        if nr:
            px, nw, nh = pixels(nr[0]); px[..., 1] = 1 - px[..., 1]; px[..., 3] = 1     # OpenGL -> DirectX
            save(px, nw, nh, stem + '_N.png', False)
        maps[slot.material.name] = {'slot': i, 'D': stem + '_D.png', 'ARM': stem + '_ARM.png', 'N': stem + '_N.png' if nr else None}
    info['maps'] = {k: {kk: (os.path.basename(vv) if isinstance(vv, str) else vv) for kk, vv in v.items()} for k, v in maps.items()}
    json.dump(info, open(os.path.join(out, spec['sm'] + '.json'), 'w'), indent=1)
print('ENV_PREP', json.dumps(info))
