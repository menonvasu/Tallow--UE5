import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];SRC=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06';OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07'
bpy.ops.wm.open_mainfile(filepath=str(SRC/'keeper-direct-delivery.blend'))
rig=bpy.data.objects['KEEPER_PRODUCTION_RIG'];mesh=bpy.data.objects['SK_Keeper_Tripo_Direct']
data={'blender':bpy.app.version_string,'file':bpy.data.filepath,'mesh':mesh.name,'vertices':len(mesh.data.vertices),'dimensions':list(mesh.dimensions),'bones':[{'name':b.name,'parent':b.parent.name if b.parent else None,'deform':b.use_deform,'head':list(b.head_local),'tail':list(b.tail_local)} for b in rig.data.bones],'constraints':{p.name:[c.type for c in p.constraints] for p in rig.pose.bones if p.constraints},'custom_shapes':sum(bool(p.custom_shape) for p in rig.pose.bones),'shape_keys':[k.name for k in mesh.data.shape_keys.key_blocks],'objects':[(o.name,o.type) for o in bpy.data.objects]}
(OUT/'source-inspection.json').write_text(json.dumps(data,indent=2));print('RESULT '+json.dumps({k:v for k,v in data.items() if k not in ['bones','objects']}))
