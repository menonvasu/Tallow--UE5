"""Workbench front-3/4 thumbnails of every prepared asset (camera on the asset's front, Blender -Y) -> Review/AssetSheet.png via ImageMagick montage."""
import bpy,json,math,subprocess,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseFurnishV03';L=O/'Work/Assets';Th=O/'Work/Thumbs';Th.mkdir(parents=True,exist_ok=True);only=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
M=json.loads((O/'Export/manifest.json').read_text())
for name in M:
 if only and name not in only:continue
 bpy.ops.wm.open_mainfile(filepath=str(L/(name+'.blend')));s=bpy.context.scene;o=bpy.data.objects['SM_Furnish_'+name]
 for x in s.objects:x.hide_render=x.name.startswith('UCX_')
 bb=[o.matrix_world@Vector(c) for c in o.bound_box];c=sum(bb,Vector())/8;r=max((v-c).length for v in bb)
 cam=bpy.data.objects.new('C',bpy.data.cameras.new('C'));s.collection.objects.link(cam);s.camera=cam;cam.data.lens=50
 d=Vector((-.45,-1,.45)).normalized();cam.location=c+d*r*3.3;cam.rotation_euler=(c-cam.location).to_track_quat('-Z','Y').to_euler()
 s.render.engine='BLENDER_WORKBENCH';s.display.shading.light='STUDIO';s.display.shading.color_type='TEXTURE';s.display.shading.show_shadows=True;s.render.resolution_x=s.render.resolution_y=260;s.render.film_transparent=False
 s.world=s.world or bpy.data.worlds.new('W');s.render.filepath=str(Th/(name+'.png'));bpy.ops.render.render(write_still=True)
names=sorted(M);args=['montage']+sum([['-label','%s\\n%dx%dx%d'%(n,*M[n]['dims_cm']),str(Th/(n+'.png'))] for n in names if (Th/(n+'.png')).exists()],[])+['-tile','10x','-geometry','200x200+3+3','-pointsize','13',str(O/'Review/AssetSheet.png')]
subprocess.run(args,check=True);print('RESULT sheet',len(names))
