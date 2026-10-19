import argparse,bpy,bmesh,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--prop',required=True);a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production'/a.prop;bpy.ops.wm.open_mainfile(filepath=str(OUT/'Prepared_HighAndProduction.blend'));o=bpy.data.objects['SM_'+a.prop];bpy.ops.object.select_all(action='DESELECT');o.hide_set(False);o.select_set(True);bpy.context.view_layer.objects.active=o
bm=bmesh.new();bm.from_mesh(o.data);removed=0
if a.prop=='RestraintChair':
 branches=[e for e in bm.edges if len(e.link_faces)>2];assert len(branches)<=1
 for e in branches:
  extra=min(e.link_faces,key=lambda f:f.calc_area());assert extra.calc_area()<2e-5;bmesh.ops.delete(bm,geom=[extra],context='FACES_ONLY');removed+=1
 bmesh.ops.delete(bm,geom=[e for e in bm.edges if e.is_wire],context='EDGES');bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_edges],context='VERTS')
bm.to_mesh(o.data);bm.free();bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.pack_islands(rotate=True,margin=.0015);bpy.ops.object.mode_set(mode='OBJECT');o.data.calc_loop_triangles();uv=o.data.uv_layers['ProductionUV'];(OUT/'uv-triangles.json').write_text(json.dumps([[[float(c) for c in uv.data[i].uv] for i in t.loops] for t in o.data.loop_triangles]));bm=bmesh.new();bm.from_mesh(o.data);report=json.loads((OUT/'prepare-report.json').read_text());report.update(production_triangles=len(o.data.loop_triangles),boundary_edges=sum(e.is_boundary for e in bm.edges),multi_face_edges=sum(len(e.link_faces)>2 for e in bm.edges),degenerate_faces=sum(f.calc_area()<=1e-12 for f in bm.faces),localized_branch_faces_removed=removed,texture_resolution=4096);bm.free();(OUT/'prepare-report.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Prepared_HighAndProduction.blend'));print('RESULT '+json.dumps(report))
