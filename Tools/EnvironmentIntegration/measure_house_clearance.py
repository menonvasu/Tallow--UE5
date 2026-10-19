"""Source-space vertical headroom samples; not a substitute for runtime capsule sweeps."""
import bpy,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'));rows=json.loads((O/'Architecture/manifest.json').read_text());blocking=[bpy.data.objects.get(r['mesh']) for r in rows if r['collision']=='complex'];blocking=[o for o in blocking if o];audit=json.loads((O/'source-house-audit.json').read_text())['objects'];samples=[]
for stair in audit:
 if stair['name'] not in ['MainStairs','BackStairs','BasementStairs']:continue
 for t in stair['horizontal_treads']:
  x=(t['min_xy'][0]+t['max_xy'][0])/2;y=(t['min_xy'][1]+t['max_xy'][1])/2;z=t['z'];origin=Vector((x,y,z+.025));hits=[]
  for o in blocking:
   hit,p,n,idx=o.ray_cast(o.matrix_world.inverted()@origin,o.matrix_world.inverted().to_3x3()@Vector((0,0,1)),distance=2.40)
   if hit:hits.append(dict(mesh=o.name,height_m=round(((o.matrix_world@p)-origin).length+.025,4)))
  samples.append(dict(stair=stair['name'],point_m=[x,y,z],headroom_obstacles_under_2_4m=hits))
floor=bpy.data.objects['SM_HouseV01_Upper_Floor'];checks=[]
for p in [(1,12.1,4.5),(3,10,4.5),(4.5,11.05,4.5),(6.6,4.3,4.5),(14.7,12.5,4.5)]:
 hit,loc,n,idx=floor.ray_cast(Vector(p),Vector((0,0,-1)),distance=1);checks.append(dict(point_m=p,solid_floor_hit=hit,z=loc.z if hit else None))
result=dict(stair_headroom_samples=samples,upper_floor_checks=checks,source_status='Measured vertical rays only; width, swept capsule, dynamic doors and navigation remain runtime gates',keeper_obstruction_samples=sum(bool(x['headroom_obstacles_under_2_4m']) for x in samples));(O/'source-clearance.json').write_text(json.dumps(result,indent=2));print('RESULT',result['keeper_obstruction_samples'],'blocked of',len(samples),'floor',checks)
