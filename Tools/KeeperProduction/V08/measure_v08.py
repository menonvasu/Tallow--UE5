"""Motion QC on deform bones: wrist/upper twist vs rest, elbow direction, per-frame angular speed (pops), loop closure.
blender -b --factory-startup <blend> --python measure_v08.py -- <out.json> [Clip,Clip|all]"""
import bpy,sys,json,math
from mathutils import Vector
argv=sys.argv[sys.argv.index('--')+1:];OUT=argv[0];ONLY=None if len(argv)<2 or argv[1]=='all' else argv[1].split(',')
rig=bpy.data.objects.get('RIG_KeeperV07') or next(o for o in bpy.data.objects if o.type=='ARMATURE');s=bpy.context.scene
for o in bpy.data.objects:
    if o.type=='MESH':o.hide_viewport=True
defs=[b.name for b in rig.data.bones if b.use_deform]
def rel(b):
    p=rig.pose.bones[b];par=p.parent
    R=(par.matrix.to_3x3().inverted()@p.matrix.to_3x3()) if par else p.matrix.to_3x3()
    R0=(par.bone.matrix_local.to_3x3().inverted()@p.bone.matrix_local.to_3x3()) if par else p.bone.matrix_local.to_3x3()
    return (R0.inverted()@R).to_quaternion()
def twist(q):t=math.degrees(2*math.atan2(q.y,q.w));return (t+180)%360-180
res={}
for act in sorted([a for a in bpy.data.actions if a.name.startswith('AN_Keeper_')],key=lambda a:a.name):
    name=act.name[10:]
    if ONLY and name not in ONLY:continue
    rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0];f0,f1=[int(round(x)) for x in act.frame_range]
    prev=None;first=None;speed={};tw={};pops=[]
    for f in range(f0,f1+1):
        s.frame_set(f);cur={b:rig.pose.bones[b].matrix.copy() for b in defs}
        if first is None:first=cur
        if prev:
            for b in defs:
                a=math.degrees((prev[b].to_quaternion().rotation_difference(cur[b].to_quaternion())).angle);a=min(a,360-a)
                d=(prev[b].translation-cur[b].translation).length
                if a>speed.get(b,(0,0))[0]:speed[b]=(a,f)
        for p in (1,2,3):
            for sd in 'LR':
                for part in ('upper','hand'):
                    b=f'DEF_arm_{p}.{sd}_{part}';t=twist(rel(b));r=tw.setdefault(f'{p}{sd}_{part}',[1e9,-1e9]);r[0]=min(r[0],t);r[1]=max(r[1],t)
        prev=cur
    loop=max(max((first[b].translation-prev[b].translation).length for b in defs),0)
    top=sorted(((v[0],b,v[1]) for b,v in speed.items()),reverse=True)[:5]
    groups={'body/head':[b for b in speed if 'arm' not in b],'upper':[b for b in speed if b.endswith('upper')],'lower':[b for b in speed if b.endswith('lower')],'hand':[b for b in speed if b.endswith('hand')],'digits':[b for b in speed if 'digit' in b]}
    top=[max(((speed[b][0],b,speed[b][1]) for b in bs),default=(0,'',0)) for g,bs in groups.items()]
    res[name]={'max_deg_per_frame':[(round(a,1),b,f) for a,b,f in top],'twist_ranges':{k:[round(x) for x in v] for k,v in tw.items()},'first_last_bone_delta_m':loop}
    print('QC',name,json.dumps(res[name]),flush=True)
open(OUT,'w').write(json.dumps(res,indent=1))
