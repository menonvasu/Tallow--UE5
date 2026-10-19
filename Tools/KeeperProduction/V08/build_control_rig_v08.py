"""V08 control rig: V07 face rig with its drivers intact, poles calibrated on a BENT chain, palm axes measured, chomp control."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[3];V07=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';OUT=ROOT/'Tallow/SourceAssets/Keeper/V08Animation';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(V07/'keeper-v07-face-rig.blend'))
rig=bpy.data.objects['RIG_KeeperV07'];obj=bpy.data.objects['SK_KeeperRiggedV07'];vl=bpy.context.view_layer
assert len(rig.animation_data.drivers)==90,'V07 face rig must still carry its 90 drivers'
ARMS=[(p,s) for p in (1,2,3) for s in 'LR']
def upd():
    # Plain update_tag() does not reliably re-run custom-property drivers headless; TIME refresh does.
    rig.update_tag(refresh={'OBJECT','DATA','TIME'});vl.update()
def reset():
    for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
    upd()
def perp(v,axis):return (v-axis*v.dot(axis))
report={'arms':{}}
for pair,side in ARMS:
    a=f'arm_{pair}.{side}';ctrl=rig.pose.bones['CTRL_'+a+'_hand'];pole=rig.pose.bones['CTRL_POLE_'+a+'_hand'];up=rig.pose.bones['MCH_'+a+'_upper'];lo=rig.pose.bones['MCH_'+a+'_lower']
    reset();sh=up.head.copy();L=up.length+lo.length;d=(lo.tail-sh).normalized();sg=1 if side=='L' else -1
    tgt=sh+d*L*.70
    def elbow_dir(roll,u):
        ctrl['elbow_roll']=roll;m=ctrl.matrix.copy();m.translation=tgt;ctrl.matrix=m
        pm=pole.matrix.copy();pm.translation=sh+d*L*.35+u*.6;pole.matrix=pm;upd()
        e=perp(up.tail-sh,(lo.tail-sh).normalized());return e.normalized()
    u=perp(Vector((sg,0,0)),d).normalized()
    # pole_angle is clamped to [-pi,pi] by Blender: search that range (V07 searched 0..2pi, half of it clamped).
    best=max(((elbow_dir(-math.pi+i*math.tau/360,u).dot(u),-math.pi+i*math.tau/360) for i in range(360)))
    lo_,hi_=best[1]-.03,best[1]+.03
    for _ in range(40):
        m1,m2=lo_+(hi_-lo_)/3,hi_-(hi_-lo_)/3
        if elbow_dir(m1,u).dot(u)<elbow_dir(m2,u).dot(u):lo_=m1
        else:hi_=m2
    roll=(lo_+hi_)/2
    # Verify on two other pole directions: elbow must point at the pole.
    checks=[]
    for w in [perp(Vector((0,0,1)),d).normalized(),perp(Vector((0,1,0)),d).normalized()]:
        checks.append(math.degrees(elbow_dir(roll,w).angle(w)))
    ctrl['elbow_roll']=roll;ctrl.id_properties_ui('elbow_roll').update(min=-math.pi*2,max=math.pi*2,default=roll,description='Calibrated so the elbow points AT CTRL_POLE (V08). Leave constant; move the pole.')
    # Palm side: fingertips curl toward it. Measured in the hand bone's rest frame.
    reset();hb=rig.pose.bones['DEF_'+a+'_hand'];vg={g.name:g.index for g in obj.vertex_groups}
    tips=[v.index for v in obj.data.vertices if v.groups and max(v.groups,key=lambda g:g.weight).group in [vg['DEF_'+a+f'_digit{k}_c'] for k in (2,3,4,5) if 'DEF_'+a+f'_digit{k}_c' in vg]]
    def tipmean():
        ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();c=sum((me.vertices[i].co for i in tips),Vector());ev.to_mesh_clear();return c/len(tips)
    for f in ['thumb','index','middle','ring','little']:ctrl[f+'_curl']=0.
    upd();t0=tipmean()
    for f in ['thumb','index','middle','ring','little']:ctrl[f+'_curl']=.6
    upd();t1=tipmean()
    for f in ['thumb','index','middle','ring','little']:ctrl[f+'_curl']=0.
    R=hb.bone.matrix_local.to_3x3();palm_local=perp(R.inverted()@(t1-t0),Vector((0,1,0))).normalized()
    thumb=[v.index for v in obj.data.vertices if v.groups and max(v.groups,key=lambda g:g.weight).group==vg['DEF_'+a+'_digit1_b']]
    tc=sum((obj.data.vertices[i].co for i in thumb),Vector())/len(thumb);thumb_local=perp(R.inverted()@(tc-hb.bone.head_local),Vector((0,1,0))).normalized()
    tip_local=R.inverted()@(t0-hb.bone.head_local)
    ctrl['palm_axis']=list(palm_local);ctrl['thumb_axis']=list(thumb_local)
    report['arms'][a]={'elbow_roll':roll,'pole_check_error_deg':checks,'palm_axis_local':list(palm_local),'thumb_axis_local':list(thumb_local),'fingertip_mean_local':list(tip_local),'chain_m':L,'palm_world_rest':list(R@palm_local)}
    print('ARM',a,json.dumps(report['arms'][a]),flush=True)
reset()
# Chomp: mouth-wide control for the remodelled maw (CHOMP morph, same name in the game mesh).
head=rig.pose.bones['CTRL_head'];head['chomp']=0.;head.id_properties_ui('chomp').update(min=0,max=1,description='Open the maw wide (CHOMP morph)')
fc=obj.data.shape_keys.key_blocks['CHOMP'].driver_add('value');drv=fc.driver;drv.expression='value';v=drv.variables.new();v.name='value';v.targets[0].id=rig;v.targets[0].data_path=head.path_from_id()+'["chomp"]'
rig['rig_version']='V08: V07 control rig, drivers intact, poles calibrated on bent chains (elbow points at CTRL_POLE), palm axes measured, chomp control'
report['drivers_rig']=len(rig.animation_data.drivers);report['drivers_mesh']=len(obj.data.shape_keys.animation_data.drivers)
(OUT/'rig-calibration-v08.json').write_text(json.dumps(report,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v08-control-rig.blend'));print('RESULT '+json.dumps({k:v for k,v in report.items() if k!='arms'}))
