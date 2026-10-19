import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';rows=[]
for name in ['GothicTable','PaintedChair','Sofa']:
 bpy.ops.wm.open_mainfile(filepath=str(O/'BlenderKitRaw'/name/'source.blend'));bpy.context.view_layer.update();row={'name':name,'objects':[],'materials':[]}
 for o in bpy.context.scene.objects:
  if o.type=='MESH':o.data.calc_loop_triangles();row['objects'].append({'name':o.name,'triangles':len(o.data.loop_triangles),'dimensions':list(o.dimensions),'location':list(o.location),'modifiers':[(m.name,m.type) for m in o.modifiers]})
 for m in bpy.data.materials:
  if m.node_tree:row['materials'].append({'name':m.name,'images':[(n.name,n.image.name,n.image.size[:],bool(n.image.packed_file)) for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]})
 rows.append(row)
(O/'furniture-intake.json').write_text(json.dumps(rows,indent=2));print('RESULT '+json.dumps(rows))
