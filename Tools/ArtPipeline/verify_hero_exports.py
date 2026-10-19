import argparse,bpy,bmesh,json,sys,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--prop',required=True);a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production'/a.prop;expected=json.loads((OUT/'prepare-report.json').read_text());report=[]
for suffix in ['glb','fbx']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);path=OUT/('SM_'+a.prop+'.'+suffix)
 if suffix=='glb':bpy.ops.import_scene.gltf(filepath=str(path))
 else:bpy.ops.import_scene.fbx(filepath=str(path))
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(objects)==1;o=objects[0];m=o.data;m.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(m);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);r={'format':suffix,'triangles':len(m.loop_triangles),'dimensions_cm':[float(v*100) for v in o.dimensions],'boundary_edges':sum(e.is_boundary for e in bm.edges),'multi_face_edges':sum(len(e.link_faces)>2 for e in bm.edges),'degenerate_faces':sum(f.calc_area()<=1e-12 for f in bm.faces),'uv_layers':len(m.uv_layers),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()};bm.free();assert r['triangles']==expected['production_triangles'] and r['triangles']<=20000 and r['degenerate_faces']==0 and r['uv_layers']==1;assert all(abs(v-t)<.01 for v,t in zip(r['dimensions_cm'],expected['dimensions_cm']));report.append(r)
(OUT/'export-validation.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps(report))
