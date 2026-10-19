"""Add detailed game-budget props to separate benchmark scenes; never touches Unreal/main map."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];PROD=ROOT/'Tallow/SourceAssets/Production';OUT=PROD/'HeroPropStaging';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(PROD/'AtticKit/AtticBenchmark.blend'));scene=bpy.context.scene;scene.render.threads_mode='FIXED';scene.render.threads=2;scene.cycles.device='CPU';scene.cycles.samples=24
assets={}
for prop in ['RestraintChair','CollectionDoll']:
 with bpy.data.libraries.load(str(PROD/prop/(prop+'_Production.blend')),link=False) as (a,b):b.objects=['SM_'+prop]
 o=b.objects[0];scene.collection.objects.link(o);o.hide_render=True;o.hide_set(True);assets[prop]=o
placements=[]
def instance(prop,label,loc,scale=1,yaw=0):
 source=assets[prop];o=source.copy();o.data=source.data;scene.collection.objects.link(o);o.name=label;o.location=loc;o.scale=(scale,)*3;o.rotation_mode='XYZ';o.rotation_euler=(0,0,math.radians(yaw));o.hide_render=False;o.hide_set(False);placements.append({'asset':prop,'label':label,'location_m':list(loc),'uniform_scale':scale,'yaw_degrees':yaw});return o
# Leave the child-size compartment empty. Collection faces away; one offset figure watches the aisle.
dolls=[instance('CollectionDoll','ShelfDollUpper',(-1.08,.83,.88),.44,180),instance('CollectionDoll','ShelfDollLower',(-1.05,.84,.49),.43,155),instance('CollectionDoll','CollectionSentinel',(.28,1.11,.06),1.12,-18)]
# Fine material detail is foreground evidence; empty compartment and clear escape opening remain.
scene.camera.location=(.65,-2.5,1.1);scene.camera.rotation_euler=(Vector((-.15,1.05,1.12))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=31
scene.render.filepath=str(OUT/'attic-detailed-WIP.png');bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AtticDetailedBenchmark.blend'))
# A second presentation study for the chair/menu, separate from the attic collection layout.
for o in dolls:o.hide_render=True;o.hide_set(True)
chair=instance('RestraintChair','MenuRestraintChair',(.45,.2,.04),1,0)
scene.camera.location=(-.4,-2.4,1.1);scene.camera.rotation_euler=(Vector((.25,.25,1.08))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=28
scene.render.filepath=str(OUT/'chair-corner-WIP.png');bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ChairCornerBenchmark.blend'))
(OUT/'hero-placements.json').write_text(json.dumps({'space':'isolated Blender metres; no main-map placement authorization','camera_height_m':1.1,'placements':placements,'attic_rule':'empty compartment stays empty; static dolls are environmental props, not player/Keeper replacements'},indent=2));print('RESULT twoisolatedbenchmarkscenes saved')
