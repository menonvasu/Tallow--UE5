import bpy,bmesh,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HeroRoomV02';rows=json.loads((O/'architecture-metrics.json').read_text());out=[]
for row in rows:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);bpy.ops.import_scene.fbx(filepath=str(O/(row['mesh']+'.fbx')))
 obj=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=obj.data;m.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(m);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 r={'mesh':row['mesh'],'triangles':len(m.loop_triangles),'boundary_edges':sum(e.is_boundary for e in bm.edges),'multiface_edges':sum(len(e.link_faces)>2 for e in bm.edges),'degenerate_faces':sum(f.calc_area()<1e-12 for f in bm.faces),'dimensions_cm':[round(v*100,4) for v in obj.dimensions],'uv_layers':len(m.uv_layers),'uv_semantics':'authored tiling UV; overlap and repeat intentional, not baked lightmap UV'};bm.free();assert r['triangles']==row['triangles'] and r['triangles']<8000 and r['multiface_edges']==0 and r['degenerate_faces']==0
 if 'Wallpaper' not in row['mesh']:assert r['boundary_edges']==0
 assert r['uv_layers']>=1;out.append(r)
(O/'export-validation.json').write_text(json.dumps(out,indent=2));print('VERIFIED',len(out),'FBX exports')
