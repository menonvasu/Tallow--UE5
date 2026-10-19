"""Fresh-process FBX roundtrip of every V08 clip: skeleton, frame range, morph curves and a pose sample against the bake."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/V08Animation'
contract=json.loads((OUT/'animation-contract.json').read_text());rows=[]
expect={r['clip']:r['samples_m'] for r in json.loads((OUT/'export-audit.json').read_text())['bake_fidelity']}
for clip in contract['clips']:
    name=clip['name'];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(OUT/'Export'/('AN_Keeper_'+name+'.fbx')))
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');mesh=next(o for o in bpy.data.objects if o.type=='MESH');s=bpy.context.scene
    act=rig.animation_data.action;fr=[int(round(x)) for x in act.frame_range]
    kb=mesh.data.shape_keys.key_blocks;skad=mesh.data.shape_keys.animation_data
    # Blender's importer guesses bone tails and CONNECTS single children (ignoring their location channels); Unreal has no such
    # concept, so disconnect before comparing evaluated joints with the source bake.
    bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
    for eb in rig.data.edit_bones:eb.use_connect=False
    bpy.ops.object.mode_set(mode='OBJECT')
    samples={};err=0;off=fr[0]-1
    for f,exp in expect[name].items():
        s.frame_set(int(f)+off);bpy.context.view_layer.update();e=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        got={b:list((e.matrix_world@e.pose.bones[b].head)/100) for b in exp};err=max(err,max(sum((a-b)**2 for a,b in zip(got[b],exp[b]))**.5 for b in exp))
        samples[f]={'morphs':{k.name:round(k.value,3) for k in kb if k.name!='Basis'}}
    row={'clip':name,'bones':len(rig.data.bones),'head_present':'head' in rig.data.bones,'control_bones':[b.name for b in rig.data.bones if b.name.startswith(('CTRL','MCH'))],
         'frame_range':fr,'expected_frames':clip['frames'],'morph_keys':[k.name for k in kb],'morph_animated':bool(skad and skad.action),'max_joint_error_m':err,'samples':samples}
    assert row['bones']==109 and row['head_present'] and not row['control_bones'] and all(k in row['morph_keys'] for k in ['FULL_HORROR','HOOD_OPEN','CHOMP'])
    assert fr[1]-fr[0]+1==clip['frames'],(name,fr)
    assert err<1e-3,(name,'joint error',err)
    rows.append(row);print('ROUNDTRIP',name,fr,'morph_animated',row['morph_animated'],'joint_err %.2e'%err,flush=True)
(OUT/'fbx-roundtrip.json').write_text(json.dumps(rows,indent=1));print('RESULT roundtrip passed',len(rows))
