import bpy,json,math
from pathlib import Path
from mathutils import Matrix
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';r=bpy.data.objects['RIG_Tallow_Generated'];poses=json.loads((OUT/'pose-bases.json').read_text());r.animation_data.action=None
for p in r.pose.bones:p.matrix_basis=Matrix(poses['Hold'][p.name])
bpy.context.view_layer.update()
SOURCE='/home/tyron/Projects/tallow-workstreams/player/Tallow/SourceAssets/Player/GeneratedV03/Production/03_generated-production.blend'
objects=[bpy.data.objects['SK_Tallow_Generated_Full'],bpy.data.objects['SK_Tallow_Generated_Partial']]
with bpy.data.libraries.load(SOURCE,link=False) as(a,b):b.objects=['SK_Tallow_Generated_Full','SK_Tallow_Generated_Partial']
report={}
for ob,src in zip(objects,b.objects):
 count=0
 for v,original in zip(ob.data.vertices,src.data.vertices):
  if not(abs(original.co.x)>.205 and .48<original.co.z<.606):continue
  v.co=original.co;side='r' if v.co.x>0 else 'l'
  def dist(name):
   bone=r.data.bones[name];d=bone.tail_local-bone.head_local;t=max(0,min(1,(v.co-bone.head_local).dot(d)/d.length_squared));return(v.co-bone.head_local-d*t).length
  digit=min(['thumb','index','middle','ring','pinky'],key=lambda d:min(dist(f'{d}_{j:02d}_{side}') for j in range(1,4)))
  candidates=[f'{digit}_{j:02d}_{side}' for j in range(1,4)]+['hand_'+side];nearest=sorted((dist(n),n) for n in candidates)[:2];w=[1/(d+.004)**2 for d,n in nearest];total=sum(w)
  for g in list(v.groups):ob.vertex_groups[g.group].remove([v.index])
  for (_,n),weight in zip(nearest,w):ob.vertex_groups[n].add([v.index],weight/total,'REPLACE')
  count+=1
 ob.data.update();report[ob.name]={'reweighted_hand_vertices':count}
 bpy.data.objects.remove(src,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'05_interaction-handfix.blend'))
print('RESULT',json.dumps(report))
