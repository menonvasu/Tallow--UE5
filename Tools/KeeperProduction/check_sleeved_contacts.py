"""Evaluate saved clips at stepped pose times: geometry contacts, looping and root stability."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-direct-animated.blend'))
obj=bpy.data.objects['SK_Keeper_Tripo_Direct'];rig=bpy.data.objects['KEEPER_PRODUCTION_RIG'];scene=bpy.context.scene
source=json.loads((OUT/'production-audit.json').read_text());arms=source['arms'];parts=source['parts']
clips=json.loads((OUT/'animation-audit.json').read_text())['clips'];report=[]
for clip in clips:
    name=clip['name'];act=bpy.data.actions['AN_Keeper_'+name];morph=bpy.data.actions['MORPH_Keeper_'+name]
    rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0];obj.data.shape_keys.animation_data.action=morph;obj.data.shape_keys.animation_data.action_slot=morph.slots[0]
    mins=[];targets=[];root=[];samples=[]
    for frame in range(1,clip['frames']+1,2):
        scene.frame_set(frame);bpy.context.view_layer.update();ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();t=(frame-1)/(clip['frames']-1)
        one={'frame':frame,'hands':[]}
        for a in arms:
            if a['pair']==1:continue
            prefix=f"arm_{a['pair']}.{a['side']}";ids=[i for p in parts if p['name'].startswith(prefix+'_digit') for i in p['indices']]
            idx=min(ids,key=lambda i:me.vertices[i].co.z);point=me.vertices[idx].co.copy();mins.append(point.z)
            q=(t+(0 if (a['pair']==2)==(a['side']=='L') else .5))%1
            gait=name in ['Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl']
            stance=gait and q<.6
            if stance:targets.append(abs(point.z))
            one['hands'].append({'name':prefix,'minimum_z':point.z,'stance':stance,'point':list(point)})
        root.append(list(rig.pose.bones['root'].matrix.translation));samples.append(one);ev.to_mesh_clear()
    row={'name':name,'root_max_displacement_m':max(Vector(r).length for r in root),'minimum_support_hand_z_m':min(mins),'max_stance_height_error_m':max(targets,default=0),'samples':samples}
    report.append(row)
(OUT/'contact-audit.json').write_text(json.dumps({'stage':'saved_source_evaluation','clips':report,'ground_z_m':0,'sample_rate':12},indent=2))
print('KEEPER_CONTACT '+json.dumps([{k:v for k,v in r.items() if k!='samples'} for r in report]))
