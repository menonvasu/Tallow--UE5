import bpy,json,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
# Open ToolRevision01/11_tools-rev01.blend. In-game camera stills with the tool attached rigidly to hand_l per tool-hold-contract.json (as the game does), 3/4 views, one sheet per clip.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/ToolRevision01';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];bpy.data.objects['SK_Tallow_Generated_Full'].hide_render=True;part.hide_render=False
PCT=int(os.environ.get('REVIEW_PCT','100'));DIR=Path(os.environ.get('REVIEW_DIR',str(OUT)));DIR.mkdir(parents=True,exist_ok=True);ONLY=os.environ.get('REVIEW_CLIPS','')
C=json.loads((OUT/'tool-hold-contract.json').read_text())['tools'];REL={k:Matrix(v['hand_l_relative_matrix_blender']) for k,v in C.items()};TOOL={k:bpy.data.objects[k] for k in C}
for o in TOOL.values():o.hide_viewport=False;o.hide_render=True;o.constraints.clear()
SHOTS={'ToolHold':[('crowbar',1),('crowbar',16),('crowbar',31),('crowbar',46),('fuse',1),('fuse',31)],'CrowbarPry':[('crowbar',f) for f in (1,6,11,16,19,24,36)],'FuseInsert':[('fuse',f) for f in (1,6,12,17,22,24,34)],'Pickup':[(None,f) for f in (1,10,20,28,36,46)]}
VIEW34={'ToolHold':('crowbar',1,Vector((-.30,.25,.12))),'CrowbarPry':('crowbar',16,Vector((-.32,.10,.10))),'FuseInsert':('fuse',22,Vector((-.25,-.05,.20))),'Pickup':(None,30,Vector((.30,.15,.05)))}
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=PCT;cam=s.camera
def pose(name,f,tool):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);bpy.context.view_layer.update()
 for k,o in TOOL.items():
  o.hide_render=not(tool and k.endswith(tool.title()))
  if not o.hide_render:o.matrix_world=r.matrix_world@r.pose.bones['hand_l'].matrix@REL[k]
 bpy.context.view_layer.update()
def game():cam.data.lens=16;cam.location=(0,-.20,1.10);cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler()
for name,shots in SHOTS.items():
 if ONLY and name not in ONLY.split(','):continue
 files=[]
 for tool,f in shots:
  pose(name,f,tool);game();s.render.filepath=str(DIR/f'FINAL-FP-{name}{"-"+tool if name=="ToolHold" else ""}-f{f:02d}.png');bpy.ops.render.render(write_still=True);files.append(s.render.filepath)
 tool,f,off=VIEW34[name];pose(name,f,tool);sd='r' if name=='Pickup' else 'l';tgt=r.matrix_world@r.pose.bones['hand_'+sd].tail;eye=tgt+off;fw=(tgt-eye).normalized();x=fw.cross(Vector((0,0,1))).normalized();cam.matrix_world=Matrix.Translation(eye)@Matrix((x,x.cross(fw),-fw)).transposed().to_4x4();cam.data.lens=30
 s.render.filepath=str(DIR/f'FINAL-34-{name}-f{f:02d}.png');bpy.ops.render.render(write_still=True)
 ims=[bpy.data.images.load(p) for p in files];w,h=ims[0].size;px=[np.array(im.pixels[:],dtype=np.float32).reshape(h,w,4)[::2,::2] for im in ims];cols=4;rows=(len(px)+cols-1)//cols;sheet=np.zeros((rows*(h//2),cols*(w//2),4),np.float32);sheet[...,3]=1
 for i,p in enumerate(px):R0=(rows-1-i//cols)*(h//2);sheet[R0:R0+p.shape[0],(i%cols)*(w//2):(i%cols)*(w//2)+p.shape[1]]=p
 out=bpy.data.images.new('sheet_'+name,cols*(w//2),rows*(h//2),alpha=True);out.pixels.foreach_set(sheet.ravel());out.filepath_raw=str(DIR/f'FINAL-FP-{name}-sheet.png');out.file_format='PNG';out.save()
 for im in ims:bpy.data.images.remove(im)
print('RESULT',json.dumps({'dir':str(DIR)}))
