"""Import immutable detailed Tripo raw into separate high/production source, author independent UVs."""
import argparse,bpy,bmesh,math,json,sys,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
p=argparse.ArgumentParser();p.add_argument('--prop',choices=['RestraintChair','CollectionDoll'],required=True);p.add_argument('--raw',type=Path,required=True);a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production'/a.prop;OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);bpy.ops.import_scene.gltf(filepath=str(a.raw));meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];bpy.ops.object.select_all(action='DESELECT')
for o in meshes:o.select_set(True)
bpy.context.view_layer.objects.active=meshes[0]
if len(meshes)>1:bpy.ops.object.join()
high=bpy.context.object;high.name='HIGH_'+a.prop;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
pts=[high.matrix_world@v.co for v in high.data.vertices];lo=Vector([min(v[i] for v in pts) for i in range(3)]);hi=Vector([max(v[i] for v in pts) for i in range(3)]);center=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z));height=2.05 if a.prop=='RestraintChair' else .70;scale=height/(hi.z-lo.z);rotation=Matrix.Rotation(-math.pi/2,4,'Z')
for v in high.data.vertices:v.co=rotation@((high.matrix_world@v.co-center)*scale)
high.matrix_world.identity();high.data.calc_loop_triangles();raw_tri=len(high.data.loop_triangles)
# Keep raw UV names and material links intact on HIGH.
low=high.copy();low.data=high.data.copy();low.name='SM_'+a.prop;bpy.context.scene.collection.objects.link(low);bpy.ops.object.select_all(action='DESELECT');low.select_set(True);bpy.context.view_layer.objects.active=low
bm=bmesh.new();bm.from_mesh(low.data);before=len(bm.verts);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-7);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));welded=before-len(bm.verts);bm.to_mesh(low.data);bm.free();low.data.calc_loop_triangles()
if len(low.data.loop_triangles)>19500:
 mod=low.modifiers.new('Game budget preserving raw for bake','DECIMATE');mod.ratio=19000/len(low.data.loop_triangles);mod.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=mod.name)
bm=bmesh.new();bm.from_mesh(low.data);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-7);bm.to_mesh(low.data);bm.free()
for uv in list(low.data.uv_layers):low.data.uv_layers.remove(uv)
low.data.uv_layers.new(name='ProductionUV')
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.006);bpy.ops.uv.average_islands_scale();bpy.ops.uv.pack_islands(rotate=True,margin=.008);bpy.ops.object.mode_set(mode='OBJECT')
# Mark actual production chart discontinuities as editable seams for later art revisions.
bm=bmesh.new();bm.from_mesh(low.data);layer=bm.loops.layers.uv.active
for e in bm.edges:
 if len(e.link_loops)!=2:e.seam=True;continue
 first,second=e.link_loops;pair1={v.vert.index:tuple(v[layer].uv) for v in (first,first.link_loop_next)};pair2={v.vert.index:tuple(v[layer].uv) for v in (second,second.link_loop_next)}
 e.seam=any((Vector(pair1[k])-Vector(pair2[k])).length>1e-6 for k in pair1) if pair1.keys()==pair2.keys() else True
bm.to_mesh(low.data);bm.free();low.data.calc_loop_triangles();uv=low.data.uv_layers['ProductionUV'];triangles=[[[float(c) for c in uv.data[i].uv] for i in t.loops] for t in low.data.loop_triangles];(OUT/'uv-triangles.json').write_text(json.dumps(triangles))
bpy.context.view_layer.update();bm=bmesh.new();bm.from_mesh(low.data);report={'prop':a.prop,'raw_source':str(a.raw.resolve().relative_to(ROOT)),'raw_sha256':hashlib.sha256(a.raw.read_bytes()).hexdigest(),'raw_triangles':raw_tri,'production_triangles':len(low.data.loop_triangles),'vertices':len(low.data.vertices),'welded_vertices':welded,'boundary_edges':sum(e.is_boundary for e in bm.edges),'multi_face_edges':sum(len(e.link_faces)>2 for e in bm.edges),'degenerate_faces':sum(f.calc_area()<=1e-12 for f in bm.faces),'dimensions_cm':[float(v*100) for v in low.dimensions],'uv_method':'independent angle-limited charts, averaged and packed; raw UV retained only on high source','high_materials':[m.name for m in high.data.materials if m]};bm.free();assert report['production_triangles']<=20000
high.hide_render=True;high.hide_set(True);low.hide_render=False
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.resolution_x=800;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
(OUT/'prepare-report.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Prepared_HighAndProduction.blend'));print('RESULT '+json.dumps(report))
