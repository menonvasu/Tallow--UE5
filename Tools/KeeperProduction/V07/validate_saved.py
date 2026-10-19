"""Independent saved-source evaluation: all frames, actual weighted mesh contacts and rig hygiene."""
import bpy,json,math,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-animated.blend'));rig=bpy.data.objects['RIG_KeeperV07'];obj=bpy.data.objects['SK_KeeperRiggedV07'];scene=bpy.context.scene
source=json.loads((OUT/'contact-generation.json').read_text());rows=[]
for clip in source:
 act=bpy.data.actions['AN_Keeper_'+clip['name']];rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0];errors=[];lowest=[];root=[];morph=[];samples=[];first_surface=None;last_surface=None
 for sample in clip['contacts']:
  scene.frame_set(sample['frame']);rig.update_tag();bpy.context.view_layer.update();ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();coords=np.empty(len(me.vertices)*3,dtype=np.float32);me.vertices.foreach_get('co',coords);coords=coords.reshape(-1,3);assert np.isfinite(coords).all()
  if first_surface is None:first_surface=coords.copy()
  last_surface=coords.copy()
  one={'frame':sample['frame'],'contacts':[]}
  for contact in sample['support']:
   if not contact['stance']:continue
   actual=coords[contact['vertex']];error=float(np.linalg.norm(actual-np.array(contact['desired'])));errors.append(error);one['contacts'].append({'hand':contact['hand'],'error_m':error,'actual':actual.tolist()})
  lowest.append(float(coords[:,2].min()));root.append(float(rig.pose.bones['root'].matrix.translation.length));morph.append({k:float(obj.data.shape_keys.key_blocks[k].value) for k in ['FULL_HORROR','HOOD_OPEN']});samples.append(one);ev.to_mesh_clear()
 row={'clip':clip['name'],'sample_count':len(samples),'max_stance_slip_m':max(errors,default=0),'lowest_surface_z_m':min(lowest),'max_root_motion_m':max(root),'loop_surface_delta_m':float(np.linalg.norm(first_surface-last_surface,axis=1).max()) if clip['loop'] else None,'morph_range':{k:[min(d[k] for d in morph),max(d[k] for d in morph)] for k in morph[0]},'frames':samples};rows.append(row);print('AUDIT '+json.dumps({k:v for k,v in row.items() if k!='frames'}),flush=True)
for row,clip in zip(rows,source):
 prev={};steps=[];speed=clip['reference_speed_cm_s']/100
 for sample in row['frames']:
  current={}
  for contact in sample['contacts']:
   point=Vector(contact['actual']);point.y-=speed*(sample['frame']-1)/24
   if contact['hand'] in prev:steps.append((point-prev[contact['hand']]).length)
   current[contact['hand']]=point
  prev=current
 row['maximum_world_contact_step_m']=max(steps,default=0)
weights=[sum(g.weight>1e-6 for g in v.groups) for v in obj.data.vertices];unweighted=sum(not v.groups for v in obj.data.vertices);sums=[sum(g.weight for g in v.groups) for v in obj.data.vertices];obj.data.calc_loop_triangles()
invalid=[fc.data_path for fc in rig.animation_data.drivers if not fc.driver.is_valid] if rig.animation_data else []
# Direct root-follow test on the actual evaluated weighted surface.
rig.animation_data.action=None;bpy.context.view_layer.update()
def surface():
 ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=np.empty(len(me.vertices)*3,dtype=np.float32);me.vertices.foreach_get('co',co);ev.to_mesh_clear();return co.reshape(-1,3)
base=surface();delta=Matrix.Translation((.13,-.08,.04))@Matrix.Rotation(.2,4,'Z');ctrl=rig.pose.bones['CTRL_root'];ctrl.matrix=delta@ctrl.matrix;rig.update_tag();bpy.context.view_layer.update();moved=surface();expected=np.array([delta@Vector(co) for co in base]);root_follow_error=float(np.linalg.norm(moved-expected,axis=1).max())
report={'root_follow_error_m':root_follow_error,'source':'keeper-v07-animated.blend','triangles':len(obj.data.loop_triangles),'vertices':len(obj.data.vertices),'bones':len(rig.data.bones),'deform_bones':sum(b.use_deform for b in rig.data.bones),'max_influences':max(weights),'unweighted':unweighted,'max_weight_sum_error':max(abs(v-1) for v in sums),'invalid_drivers':invalid,'all_frames':rows,'source_only':True,'engine_import_and_route_validation':'root owner pending'}
(OUT/'saved-source-validation.json').write_text(json.dumps(report,indent=2));assert root_follow_error<.0001,'Root does not carry all controlled parts'
assert max((r['loop_surface_delta_m'] or 0) for r in rows)<.001,'Loop boundary discontinuity'
assert report['max_influences']<=4 and unweighted==0 and not invalid
assert max(r['max_stance_slip_m'] for r in rows)<.001,'Contact drift over1mm'
assert min(r['lowest_surface_z_m'] for r in rows)>-.001,'Surface penetrates floor by over1mm'
assert max(r['max_root_motion_m'] for r in rows)<1e-5,'Unexpected root motion'
print('RESULT '+json.dumps({k:v for k,v in report.items() if k!='all_frames'}))
