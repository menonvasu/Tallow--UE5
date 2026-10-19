import bpy,json,math
r=bpy.data.objects['RIG_Tallow_Generated'];o=bpy.data.objects['SK_Tallow_Generated_Partial'];c=bpy.data.objects['SM_CandleV02_Crooked'];dg=bpy.context.evaluated_depsgraph_get();e=o.evaluated_get(dg);me=e.to_mesh();out=[]
for v in o.data.vertices:
 names=[o.vertex_groups[g.group].name for g in v.groups]
 if any(n.endswith('_r') and n.startswith(('thumb','index','middle','ring','pinky','hand')) for n in names):
  p=me.vertices[v.index].co
  if .85<p.z<.9:out.append(((p.x-.265)**2+(p.y-.245)**2)**.5-.0105)
print('RESULT',json.dumps({'bones':{p.name:{'head':list(p.head),'tail':list(p.tail)} for p in r.pose.bones if p.name.endswith('_r') and p.name.startswith(('hand','thumb','index','middle','ring','pinky'))},'stem_clearance_min':min(out),'penetrating':sum(x<-.001 for x in out),'samplecount':len(out)},indent=2))
e.to_mesh_clear()
