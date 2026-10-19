import bpy,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];bpy.ops.wm.open_mainfile(filepath=str(R/'Tallow/SourceAssets/Production/CabinetFinished/Cabinet_SageTimber.blend'));o=bpy.data.objects['SM_Cabinet_Production'];out=[]
for y in [-.16,-.10,0,.1]:
 for z in [.55,.85,1.10,1.25]:
  h,p,n,i=o.ray_cast(Vector((0,y,z)),Vector((0,0,-1)),distance=.6)
  if h:out.append({'local_y':y,'start_z':z,'hit':list(p)})
print('SHELVES',json.dumps(out))
