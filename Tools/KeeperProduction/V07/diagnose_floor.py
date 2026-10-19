import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-animated.blend'));rig=bpy.data.objects['RIG_KeeperV07'];o=bpy.data.objects['SK_KeeperRiggedV07'];s=bpy.context.scene
for name,frames in [('Reveal',73),('RunChase',13),('Reset',73)]:
 a=bpy.data.actions['AN_Keeper_'+name];rig.animation_data.action=a;rig.animation_data.action_slot=a.slots[0];best=(1,None)
 for f in range(1,frames+1):
  s.frame_set(f);bpy.context.view_layer.update();ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();v=min(me.vertices,key=lambda v:v.co.z)
  if v.co.z<best[0]:best=(v.co.z,dict(frame=f,index=v.index,original=list(o.data.vertices[v.index].co),position=list(v.co),groups=[(o.vertex_groups[g.group].name,g.weight) for g in o.data.vertices[v.index].groups],morph=o.data.shape_keys.key_blocks['FULL_HORROR'].value))
  ev.to_mesh_clear()
 print('FLOOR '+name+' '+json.dumps(best),flush=True)
