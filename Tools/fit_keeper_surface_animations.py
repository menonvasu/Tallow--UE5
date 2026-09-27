"""Fit authored wall/ceiling leg chains to the M3 corridor, on the separate export .blend.
Preserves each segment length and all original body/head/hand motion. Not runtime IK.
"""
import bpy, math, json
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path('/home/tyron/Projects/cgra252-team9')
scene=bpy.context.scene
source=bpy.data.objects['KEEPER_RIG'];rig=bpy.data.objects['KeeperSkeleton'];mesh=bpy.data.objects['SK_Keeper']
carrier=bpy.data.objects['FacialCurveCarrier']
rest={b.name:b.matrix_local.copy() for b in rig.data.bones}
for t in source.animation_data.nla_tracks:t.mute=True
source.data.pose_position='POSE';rig.data.pose_position='POSE'
scale=json.loads((ROOT/'Tools/keeper-animation-export.json').read_text())['export_scale']
controls={'FULL_HORROR':'rage','CHOMP':'bite','Blink_L':'blink_L','Blink_R':'blink_R'}

def oriented(old, start, end):
    direction=end-start
    rotation=old.col[1].xyz.normalized().rotation_difference(direction.normalized())@old.to_quaternion()
    return Matrix.LocRotScale(start,rotation,old.to_scale())

def fit(pose, limit):
    maximum_error=0
    for side in ('L','R'):
        sign=-1 if side=='L' else 1
        for i in (1,2,3):
            names=['leg.%s%d.%d'%(side,i,j) for j in range(3)]
            a,b,c=[pose[n] for n in names]
            start=a.translation.copy();knee=b.translation.copy();ankle=c.translation.copy()
            toe=c@Vector((0,rig.data.bones[names[2]].length,0))
            l1=(knee-start).length;l2=(ankle-knee).length;l3=(toe-ankle).length
            target=ankle.copy();target.x=sign*min(abs(target.x),limit*.87)
            tip=toe.copy();tip.x=sign*min(abs(tip.x),limit*.96)
            # Ankle-to-toe orientation changes, but its original length is retained.
            tip=target+(tip-target).normalized()*l3
            axis=target-start;distance=axis.length;axis.normalize()
            assert abs(l1-l2)<distance<l1+l2, 'Target outside two-bone reach'
            along=(l1*l1-l2*l2+distance*distance)/(2*distance)
            radius=math.sqrt(max(0,l1*l1-along*along));centre=start+axis*along
            pole=knee-centre;pole-=axis*pole.dot(axis)
            pole.normalize();perp=axis.cross(pole).normalized()
            candidates=[centre+radius*(pole*math.cos(j*math.tau/720)+perp*math.sin(j*math.tau/720)) for j in range(720)]
            # Keep knees on their own side of the face, away from its central cavity.
            valid=[v for v in candidates if .7<sign*v.x<limit]
            if not valid: valid=[v for v in candidates if abs(v.x)<limit]
            assert valid, 'No compact knee solution'
            folded=min(valid,key=lambda v:(v-knee).length_squared)
            pose[names[0]]=oriented(a,start,folded)
            pose[names[1]]=oriented(b,folded,target)
            pose[names[2]]=oriented(c,target,tip)
            socket='socket.%s%d'%(side,i)
            pose[socket]=oriented(pose[socket],start,folded)
            maximum_error=max(maximum_error,abs((folded-start).length-l1),abs((target-folded).length-l2),abs((tip-target).length-l3))
    return maximum_error

reports=[]
for name,limit in [('WallCrawl',1.15),('CeilingCrawl',2.55)]:
    original=bpy.data.actions['KEEPER_'+name]
    source.animation_data.action=original;source.animation_data.action_slot=original.slots[0]
    frames=[];error=0
    for f in range(1,int(original.frame_range[1])+1):
        scene.frame_set(f);bpy.context.view_layer.update()
        inverse=(source.pose.bones['root'].matrix@rest['root'].inverted()).inverted()
        pose={b.name:inverse@b.matrix.copy() for b in source.pose.bones}
        error=max(error,fit(pose,limit))
        frames.append((pose,{k:float(source.get(k,0)) for k in controls.values()}))
    rig.animation_data_clear();rig.animation_data_create()
    action=bpy.data.actions.new('COMPACT_'+name);action.use_fake_user=True
    slot=action.slots.new(id_type='OBJECT',name=rig.name)
    rig.animation_data.action=action;rig.animation_data.action_slot=slot
    carrier.data.shape_keys.animation_data_clear()
    for f,(pose,values) in enumerate(frames,1):
        for b in rig.pose.bones:
            parent=b.parent
            b.matrix_basis=(rest[b.name].inverted()@rest[parent.name]@pose[parent.name].inverted()@pose[b.name]) if parent else rest[b.name].inverted()@pose[b.name]
            b.rotation_mode='QUATERNION'
            for prop in ('location','rotation_quaternion','scale'):b.keyframe_insert(prop,frame=f,group=b.name)
        for key,control in controls.items():
            k=carrier.data.shape_keys.key_blocks[key];k.value=values[control];k.keyframe_insert('value',frame=f)
    scene.frame_start=1;scene.frame_end=len(frames);scene.frame_set(1);bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);carrier.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(ROOT/'Tallow/SourceAssets/Keeper/Animated'/('AN_Keeper_'+name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},global_scale=scale,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=False,mesh_smooth_type='FACE',use_mesh_modifiers=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,bake_anim_step=1)
    # Inspect real deformed mesh bounds at several poses, not just bone tips.
    bounds=[]
    for f in (1,24,48,72,96):
        scene.frame_set(f)
        for key,control in controls.items():mesh.data.shape_keys.key_blocks[key].value=frames[f-1][1][control]
        bpy.context.view_layer.update()
        evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        points=[mesh.matrix_world@v.co for v in evaluated.data.vertices]
        bounds.append({'frame':f,'min_cm':[min(p[i] for p in points)*scale*100 for i in range(3)],'max_cm':[max(p[i] for p in points)*scale*100 for i in range(3)]})
    scene.frame_set(48);bpy.context.view_layer.update()
    for ob in scene.objects:ob.hide_render=ob not in (rig,mesh,scene.camera)
    scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=900;scene.render.resolution_y=900;scene.render.resolution_percentage=100
    scene.render.filepath=str(ROOT/'Tools/previews/keeper'/('Compact'+name+'.png'));bpy.ops.render.render(write_still=True)
    reports.append({'clip':name,'max_segment_length_error_source_units':error,'width_limit_source_units':limit,'sample_bounds':bounds})
(ROOT/'Tools/keeper-surface-fit.json').write_text(json.dumps({'clips':reports,'scope':'Authored corridor poses, not arbitrary runtime surface IK'},indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath='/home/tyron/Projects/the-keeper/keeper-unreal-export.blend')
result={'clips':reports}
