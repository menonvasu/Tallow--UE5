"""Saved-source validation on the evaluated, weighted mesh: planted contacts (vertex vs authored point, world-compensated
step), floor penetration, loop closure, root motion, arm twist vs rest, elbow direction, per-frame pops, morph ranges.
blender -b --factory-startup --threads 4 <animated.blend> --python validate_v08.py -- <out.json> [Clip,Clip|all]"""
import bpy,sys,json,math
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
argv=sys.argv[sys.argv.index('--')+1:];OUT=Path(argv[0]);ONLY=None if len(argv)<2 or argv[1]=='all' else argv[1].split(',')
rig=bpy.data.objects['RIG_KeeperV07'];obj=bpy.data.objects['SK_KeeperRiggedV07'];s=bpy.context.scene;obj.hide_viewport=False
src=json.loads(Path(bpy.data.filepath).with_suffix('.contacts.json').read_text());defs=[b.name for b in rig.data.bones if b.use_deform]
def rel_q(b):
    p=rig.pose.bones[b];par=p.parent;R=par.matrix.to_3x3().inverted()@p.matrix.to_3x3();R0=par.bone.matrix_local.to_3x3().inverted()@p.bone.matrix_local.to_3x3();return (R0.inverted()@R).to_quaternion()
def twist(q):t=math.degrees(2*math.atan2(q.y,q.w));return (t+180)%360-180
def wrist_twist(a):
    lo=rig.pose.bones[f'DEF_arm_{a}_lower'];ha=rig.pose.bones[f'DEF_arm_{a}_hand']
    rel=(lo.bone.matrix_local.to_3x3().inverted()@ha.bone.matrix_local.to_3x3()).inverted()@(lo.matrix.to_3x3().inverted()@ha.matrix.to_3x3());return twist(rel.to_quaternion())
rows=[]
for clip in src:
    name=clip['name']
    if ONLY and name not in ONLY:continue
    act=bpy.data.actions['AN_Keeper_'+name];rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0]
    errs=[];low=[];first=last=None;prev=None;speed={};tw={};prevw={};steps=[];morph={'FULL_HORROR':[1,0],'HOOD_OPEN':[1,0],'CHOMP':[1,0]};rootm=0;lowest_frame=None
    for smp in clip['contacts']:
        f=smp['frame'];s.frame_set(f);bpy.context.view_layer.update()
        ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=np.empty(len(me.vertices)*3,np.float32);me.vertices.foreach_get('co',co);co=co.reshape(-1,3);ev.to_mesh_clear()
        assert np.isfinite(co).all()
        if first is None:first=co.copy()
        last=co;z=float(co[:,2].min());low.append(z)
        if lowest_frame is None or z<lowest_frame[0]:lowest_frame=(z,f)
        cur={}
        for c in smp['support']:
            if not c['stance']:continue
            a=Vector(co[c['vertex']].tolist());e=(a-Vector(c['desired'])).length;errs.append(e)
            yaw=math.radians(c.get('actor_yaw_deg',0));w=Matrix.Rotation(yaw,3,'Z')@(a-Vector((0,clip['reference_speed_cm_s']/100*(f-1)/24,0)))
            cur[c['hand']]=w
            if c['hand'] in prevw:steps.append((w-prevw[c['hand']]).length)
        prevw=cur
        rootm=max(rootm,rig.pose.bones['root'].matrix.translation.length)
        for k_ in morph:
            v=float(obj.data.shape_keys.key_blocks[k_].value);morph[k_]=[min(morph[k_][0],v),max(morph[k_][1],v)]
        mats={b:rig.pose.bones[b].matrix.copy() for b in defs}
        if prev:
            for b in defs:
                a_=math.degrees(prev[b].to_quaternion().rotation_difference(mats[b].to_quaternion()).angle);a_=min(a_,360-a_)
                if a_>speed.get(b,(0,0))[0]:speed[b]=(a_,f)
        prev=mats
        for p in (1,2,3):
            for sd in 'LR':
                a=f'{p}.{sd}';u=twist(rel_q(f'DEF_arm_{a}_upper'));h=wrist_twist(a)
                for key_,v in ((f'{p}{sd}_upper',u),(f'{p}{sd}_wrist',h)):
                    r=tw.setdefault(key_,[1e9,-1e9]);r[0]=min(r[0],v);r[1]=max(r[1],v)
    groups={'body_head':[b for b in speed if 'arm' not in b],'upper':[b for b in speed if b.endswith('upper')],'lower':[b for b in speed if b.endswith('lower')],'hand':[b for b in speed if b.endswith('hand')],'digits':[b for b in speed if 'digit' in b]}
    pops={g:max(((round(speed[b][0],1),b,speed[b][1]) for b in bs),default=(0,'',0)) for g,bs in groups.items()}
    row={'clip':name,'frames':clip['frames'],'loop':clip['loop'],'reference_speed_cm_s':clip['reference_speed_cm_s'],'stance_samples':len(errs),
         'max_contact_error_m':max(errs,default=0.),'max_world_contact_step_m':max(steps,default=0.),'lowest_surface_z_m':min(low),'lowest_frame':lowest_frame[1],
         'loop_surface_delta_m':float(np.linalg.norm(first-last,axis=1).max()) if clip['loop'] else None,'max_root_motion_m':rootm,
         'twist_deg':{k_:[round(x) for x in v] for k_,v in tw.items()},'max_upper_twist_deg':max(max(abs(v[0]),abs(v[1])) for k_,v in tw.items() if 'upper' in k_),
         'max_wrist_twist_deg':max(max(abs(v[0]),abs(v[1])) for k_,v in tw.items() if 'wrist' in k_),'peak_deg_per_frame':pops,'morph_range':morph}
    rows.append(row);print('VAL',name,json.dumps({k_:row[k_] for k_ in ['max_contact_error_m','max_world_contact_step_m','lowest_surface_z_m','lowest_frame','loop_surface_delta_m','max_upper_twist_deg','max_wrist_twist_deg']}),'pops',pops['upper'][0],pops['hand'][0],pops['body_head'][0],flush=True)
# rig hygiene
weights=[sum(g.weight>1e-6 for g in v.groups) for v in obj.data.vertices]
rep={'source':Path(bpy.data.filepath).name,'deform_bones':len(defs),'max_influences':max(weights),'rig_drivers':len(rig.animation_data.drivers),'invalid_drivers':[d.data_path for d in rig.animation_data.drivers if not d.driver.is_valid],'clips':rows,
     'worst':{'contact_error_m':max(r['max_contact_error_m'] for r in rows),'world_step_m':max(r['max_world_contact_step_m'] for r in rows),'lowest_surface_z_m':min(r['lowest_surface_z_m'] for r in rows),
              'loop_delta_m':max((r['loop_surface_delta_m'] or 0) for r in rows),'root_motion_m':max(r['max_root_motion_m'] for r in rows),'upper_twist_deg':max(r['max_upper_twist_deg'] for r in rows),'wrist_twist_deg':max(r['max_wrist_twist_deg'] for r in rows)}}
OUT.write_text(json.dumps(rep,indent=1));print('RESULT',json.dumps(rep['worst']))
