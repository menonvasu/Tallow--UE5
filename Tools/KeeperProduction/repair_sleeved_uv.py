"""Split only raster-confirmed overlapping UV triangles, preserve mesh and bake source UV."""
import bpy,json
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06';bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-tripo-clean-uv.blend'));o=bpy.data.objects['SK_Keeper_Tripo_Direct'];m=o.data;uv=m.uv_layers['ProductionUV'];bad=json.loads((OUT/'uv-overlap-faces.json').read_text())
for index,face in enumerate(bad):
 loops=m.polygons[face].loop_indices;center=sum((uv.data[i].uv for i in loops),Vector((0,0)))/3
 for i in loops:uv.data[i].uv+=Vector((3+index*.1,3))-center
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;m.uv_layers.active=uv;uv.active_render=True
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.select_all(action='SELECT');bpy.ops.uv.pack_islands(rotate=True,margin_method='FRACTION',margin=.0005);bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-tripo-clean-uv.blend'));print('UV_SPLIT_OVERLAP_FACES '+str(len(bad)))
