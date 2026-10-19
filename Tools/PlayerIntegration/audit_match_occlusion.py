import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];p=bpy.data.objects['SK_Tallow_Generated_Partial'];c=bpy.data.objects['SM_CandleV02_Crooked'];a=bpy.data.actions['AN_PlayerV05_RelightMatch'];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(20);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();camera=Vector((0,-.2,1.1));index=r.pose.bones['index_03_l'].tail.copy();thumb=r.pose.bones['thumb_03_l'].tail.copy();pinch=(index+thumb)/2;wick=c.matrix_world@Vector((.004,0,.255));head=pinch+(wick-pinch).normalized()*.041
report={'camera':list(camera),'pinch':list(pinch),'match_head':list(head),'wick':list(wick),'rays':{}}
for label,target in [('pinch',pinch),('shaft_mid',(pinch+head)/2),('match_head',head),('flame_mid',head+Vector((0,0,.004))),('flame_tip',head+Vector((0,0,.008))),('flame_tall_tip',head+Vector((0,0,.012))),('wick',wick)]:
 hits=[]
 for ob in [p,c]:
  inv=ob.matrix_world.inverted();origin=inv@camera;end=inv@target;direction=end-origin;bvh=BVHTree.FromObject(ob,dg);loc,norm,face,distance=bvh.ray_cast(origin,direction.normalized(),direction.length)
  if loc is not None:
   gap=(ob.matrix_world@loc-target).length
   hits.append({'object':ob.name,'surface_cm_in_front_of_target':gap*100,'hit':list(ob.matrix_world@loc)})
 report['rays'][label]=hits
print('RESULT',json.dumps(report),flush=True)
import math
from mathutils import Matrix
basis={p.name:p.matrix_basis.copy() for p in r.pose.bones};H=r.pose.bones['hand_l'].matrix.copy();r.animation_data.action=None;axis=(head-pinch).normalized();candidates=[]
def aim(name,target):
 bpy.context.view_layer.update();pb=r.pose.bones[name];point=pb.head.copy();q=(pb.tail-point).rotation_difference(target-point);pb.matrix=Matrix.Translation(point)@q.to_matrix().to_4x4()@Matrix.Translation(-point)@pb.matrix;bpy.context.view_layer.update()
for degrees in [-60,-50,-40,-30,-20,-10,10,20,30,40,50,60]:
 for pb in r.pose.bones:pb.matrix_basis=basis[pb.name]
 bpy.context.view_layer.update();newH=Matrix.Translation(pinch)@Matrix.Rotation(math.radians(degrees),4,axis)@Matrix.Translation(-pinch)@H;target=newH.translation;shoulder=r.pose.bones['upperarm_l'].head.copy();d=target-shoulder;L=r.data.bones['upperarm_l'].length;M=r.data.bones['lowerarm_l'].length
 if d.length>=L+M:continue
 direction=d.normalized();length=(L*L-M*M+d.length_squared)/(2*d.length);pole=Vector((-1,0,-.7));pole=(pole-direction*pole.dot(direction)).normalized();elbow=shoulder+direction*length+pole*math.sqrt(max(0,L*L-length*length));aim('upperarm_l',elbow);aim('lowerarm_l',target);r.pose.bones['hand_l'].matrix=newH;bpy.context.view_layer.update();bvh=BVHTree.FromObject(p,bpy.context.evaluated_depsgraph_get());clear={}
 for label,point in [('head',head),('shaft_mid',(head+pinch)/2),('flame_mid',head+Vector((0,0,.004))),('flame_tip',head+Vector((0,0,.008)))]:
  ray=point-camera;hit,_,_,_=bvh.ray_cast(camera,ray.normalized(),ray.length);clear[label]=hit is None
 newpinch=(r.pose.bones['index_03_l'].tail+r.pose.bones['thumb_03_l'].tail)/2;candidates.append({'roll_degrees':degrees,'clear':clear,'pinch_change_m':(newpinch-pinch).length,'wrist':list(target)})
print('CANDIDATES',json.dumps(candidates),flush=True)
