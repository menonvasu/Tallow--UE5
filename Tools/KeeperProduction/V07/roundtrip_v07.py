import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';rows=[]
for file in ['SK_KeeperRiggedV07.fbx','AN_Keeper_JumpscareDeath.fbx','AN_Keeper_RunChase.fbx']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(OUT/'Export'/file));rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');mesh=next(o for o in bpy.data.objects if o.type=='MESH');mesh.data.calc_loop_triangles();s=bpy.context.scene
 values=[]
 for frame in ([1,23,43,73] if 'Jumpscare' in file else [1]):
  s.frame_set(frame);bpy.context.view_layer.update();values.append({'frame':frame,'morphs':{k.name:k.value for k in mesh.data.shape_keys.key_blocks}})
 row={'file':file,'bone_count':len(rig.data.bones),'head_present':'head' in rig.data.bones,'control_bones':[b.name for b in rig.data.bones if b.name.startswith(('CTRL','MCH'))],'triangles':len(mesh.data.loop_triangles),'dimensions':list(mesh.dimensions),'morph_samples':values,'materials':[m.name for m in mesh.data.materials]};rows.append(row)
 assert row['bone_count']==109 and row['head_present'] and not row['control_bones']
 assert all(k in mesh.data.shape_keys.key_blocks for k in ['FULL_HORROR','HOOD_OPEN'])
 if 'Jumpscare' in file:assert values[0]['morphs']['HOOD_OPEN']<.01 and values[2]['morphs']['HOOD_OPEN']>.99
 print('ROUNDTRIP '+json.dumps(row),flush=True)
(OUT/'fbx-roundtrip.json').write_text(json.dumps(rows,indent=2));print('RESULT roundtrip passed')
