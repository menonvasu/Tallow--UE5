import bpy,json
r=bpy.data.objects['RIG_Tallow_Generated'];r.animation_data.action=None
for p in r.pose.bones:p.matrix_basis.identity()
bpy.context.view_layer.update()
print('RESULT',json.dumps({'version':bpy.app.version_string,'file':bpy.data.filepath,'objects':[(o.name,o.type) for o in bpy.context.scene.objects],'bones':{b.name:{'head':list(b.head_local),'tail':list(b.tail_local)} for b in r.data.bones if any(n in b.name for n in ['hand','thumb','index','middle','ring','pinky'])}},indent=2))
