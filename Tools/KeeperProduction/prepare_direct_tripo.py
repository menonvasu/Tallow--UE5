"""Preserve Tripo character surface. Cleanup + explicit seam unwrap on a derived copy.
Usage: blender -b --factory-startup --python-exit-code 1 -P prepare_direct_tripo.py -- --raw /abs/model.glb --out /abs/DirectV04 --front-rotation -90
Never edits the raw file or replaces its surface with primitives. No remesh/decimate.
"""
import argparse,bpy,bmesh,json,math,hashlib,sys
from pathlib import Path
from mathutils import Vector,Matrix
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
p=argparse.ArgumentParser();p.add_argument('--raw',required=True);p.add_argument('--out',required=True);p.add_argument('--front-rotation',type=float,default=-90);a=p.parse_args(args)
raw=Path(a.raw).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(raw))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];bpy.context.view_layer.update()
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
lo=Vector([min(p[i] for p in points) for i in range(3)]);hi=Vector([max(p[i] for p in points) for i in range(3)])
center=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z));scale=2.4/(hi.z-lo.z);rotation=Matrix.Rotation(math.radians(a.front_rotation),4,'Z')
for o in meshes:
    mat=o.matrix_world.copy()
    for v in o.data.vertices:v.co=rotation@((mat@v.co-center)*scale)
    o.matrix_world=Matrix.Identity(4)
# Preserve a whole imported source scene with original UV/materials before touching topology.
bpy.ops.wm.save_as_mainfile(filepath=str(out/'keeper-tripo-source-normalized.blend'))
# Join only the imported character meshes, with material/UV data retained.
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:o.select_set(True)
bpy.context.view_layer.objects.active=meshes[0]
if len(meshes)>1:bpy.ops.object.join()
obj=bpy.context.object;obj.name='SK_Keeper_Tripo_Direct';mesh=obj.data
mesh.calc_loop_triangles();before={'verts':len(mesh.vertices),'triangles':len(mesh.loop_triangles),'materials':[m.name for m in mesh.materials]}
if before['triangles']>80000:raise RuntimeError(f"Raw has {before['triangles']} triangles: preserve source and review reduction before proceeding")
bm=bmesh.new();bm.from_mesh(mesh)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.000001)
bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=0.0000001)
# Remove only exact duplicate faces (same welded vertices), never detached fringe islands.
seen=set();dupes=[]
for face in bm.faces:
    signature=frozenset(v for v in face.verts)
    if signature in seen:dupes.append(face)
    else:seen.add(signature)
if dupes:bmesh.ops.delete(bm,geom=dupes,context='FACES_ONLY')
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free();mesh.update()
# Preserve original UV for texture sampling while authoring new seams and ProductionUV.
if not mesh.uv_layers:raise RuntimeError('Generated mesh has no source UV: cannot promise original PBR transfer')
source_uv=mesh.uv_layers.active;old_uv_name=source_uv.name;source_uv.name='TripoSourceUV'
for mat in mesh.materials:
    if not mat or not mat.use_nodes:continue
    for old_node in mat.node_tree.nodes:
        if old_node.type=='UVMAP' and old_node.uv_map==old_uv_name:old_node.uv_map='TripoSourceUV'
    uv=mat.node_tree.nodes.new('ShaderNodeUVMap');uv.uv_map='TripoSourceUV';uv.label='Immutable Tripo texture coordinate source'
    for n in list(mat.node_tree.nodes):
        if n.type=='TEX_IMAGE' and not n.inputs['Vector'].is_linked:mat.node_tree.links.new(uv.outputs['UV'],n.inputs['Vector'])
prod=mesh.uv_layers.new(name='ProductionUV');mesh.uv_layers.active=prod;prod.active_render=True
# Fresh projection approved for this dense generated surface after angle unwrap collapsed fingers.
# Source UV is bake-only; ProductionUV is independently projected at 55 degrees then normalized/packed.
seams=0
bpy.context.view_layer.objects.active=obj;bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(55),island_margin=.002);bpy.ops.uv.select_all(action='SELECT');bpy.ops.uv.average_islands_scale();bpy.ops.uv.pack_islands(rotate=True,margin_method='FRACTION',margin=.0005);bpy.ops.object.mode_set(mode='OBJECT')
mesh.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(mesh)
report={'pipeline':'direct Tripo surface retained, no primitive replacement/remesh/decimation','source':str(raw),'sha256':hashlib.sha256(raw.read_bytes()).hexdigest(),'before':before,'after':{'verts':len(mesh.vertices),'triangles':len(mesh.loop_triangles),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'loose_verts':sum(not v.link_edges for v in bm.verts),'zero_area_faces':sum(f.calc_area()<1e-12 for f in bm.faces),'exact_duplicate_faces_removed':len(dupes),'seams':seams},'uv_method':'fresh 55 degree projection, normalized island scale and independent pack; original UV sampling only','original_uv':'TripoSourceUV','production_uv':'ProductionUV','height_m':2.4,'normalization_rotation_z_degrees':a.front_rotation,'unverified':['fresh UV visual distortion/overlap','original PBR bake','rig fit and deformation','engine import']};bm.free()
(out/'direct-cleanup-audit.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(out/'keeper-tripo-clean-uv.blend'))
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='SINGLE';scene.display.shading.single_color=(.5,.48,.45);scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH';scene.render.resolution_x=1000;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.world.color=(.14,.14,.14)
camd=bpy.data.cameras.new('DirectAudit');cam=bpy.data.objects.new('DirectAudit',camd);scene.collection.objects.link(cam);scene.camera=cam;camd.type='ORTHO';camd.ortho_scale=2.9
for name,pos in [('front',(0,-5,1.2)),('threequarter',(3,-5,1.4)),('back',(0,5,1.2))]:
    cam.location=pos;cam.rotation_euler=(Vector((0,0,1.2))-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(out/('direct-'+name+'.png'));bpy.ops.render.render(write_still=True)
print('KEEPER_DIRECT '+json.dumps(report))
