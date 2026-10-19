import bpy,json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'));cfg=json.loads((O/'dressing.json').read_text());cache={}
for row in cfg['props']:
 name=row['asset'].split('/')[-1]
 if name not in cache:
  if name.startswith('SM_House_'):p=O/'Furniture'/(name.removeprefix('SM_House_')+'.blend');obj=name
  elif 'Cabinet' in name:p=R/'Tallow/SourceAssets/Production/CabinetFinished/Cabinet_SageTimber.blend';obj='SM_Cabinet_Production'
  elif 'RestraintChair' in name:p=R/'Tallow/SourceAssets/Production/RestraintChair/RestraintChair_Production.blend';obj='SM_RestraintChair'
  else:p=R/'Tallow/SourceAssets/Production/CollectionDoll/CollectionDoll_Production.blend';obj='SM_CollectionDoll'
  with bpy.data.libraries.load(str(p),link=False) as(a,b):b.objects=[obj]
  cache[name]=b.objects[0]
 o=cache[name].copy();o.data=o.data.copy();bpy.context.scene.collection.objects.link(o);o.name='HI_'+row['label'];x,y,z=row['location'];o.location=(x/100,-y/100,z/100);o.rotation_mode='XYZ';o.rotation_euler=(0,0,math.radians(-row['yaw']));o.scale=row['scale']
bpy.context.view_layer.update();blocknames={r['mesh'] for r in json.loads((O/'Architecture/manifest.json').read_text()) if r['collision']=='complex'};objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and (o.name in blocknames or o.name.startswith('HI_') or 'PuzzleShelves' in o.name)];trees={}
for o in objects:
 verts=[o.matrix_world@v.co for v in o.data.vertices];trees[o.name]=BVHTree.FromPolygons(verts,[list(p.vertices) for p in o.data.polygons])
def ray(p,down=3):
 hits=[]
 for name,tree in trees.items():
  loc,n,i,d=tree.ray_cast(Vector(p),Vector((0,0,-1)),down)
  if loc is not None:hits.append((d,name,list(loc)))
 return sorted(hits)[0] if hits else None
plan=json.loads((O/'route-plan.json').read_text());surface=[]
for row in plan['crowbar_candidates']+plan['fuse_candidates']:
 x,y,z=row['location'];hit=ray((x/100,-y/100,z/100+.5),2);surface.append(dict(id=row['id'],candidate_cm=row['location'],support=hit[1] if hit else None,surface_z_cm=hit[2][2]*100 if hit else None,pivot_above_surface_cm=z-hit[2][2]*100 if hit else None))
def capsule(center):
 x,y,z=center;coll=[]
 for name,tree in trees.items():
  best=999
  for dz in [-.30,-.15,0,.15,.30]:
   p,n,i,d=tree.find_nearest(Vector((x/100,-y/100,z/100+dz)))
   if p is not None:best=min(best,d)
  if best<.299:coll.append(dict(mesh=name,nearest_cm=best*100))
 return coll
# Views remain within real standable pockets; exits are full capsule centers, not feet.
hides=[dict(id='Hide_ParlourSofaBack',center=[260,-44,60],view=[260,-44,108],exit=[430,-60,60],concealment='Behind north-wall sofa; approach around east arm'),dict(id='Hide_StudyCupboardCorner',center=[1480,-95,60],view=[1480,-95,108],exit=[1320,-100,60],concealment='Northeast corner behind study cupboard; approach from west'),dict(id='Hide_BedroomSofa',center=[1320,-245,450],view=[1320,-245,498],exit=[1120,-245,450],concealment='Behind bedroom sofa; approach around west arm')]
for row in hides:
 row['yaw']={'Hide_ParlourSofaBack':-90,'Hide_StudyCupboardCorner':180,'Hide_BedroomSofa':-90}[row['id']]
 row['exit_path']=[row['center'],[430,-44,60],row['exit']] if row['id']=='Hide_ParlourSofaBack' else [row['center'],row['exit']]
 row['path_obstructions']=[]
 for a,b in zip(row['exit_path'],row['exit_path'][1:]):
  for i in range(11):
   point=[a[j]+(b[j]-a[j])*i/10 for j in range(3)];hits=capsule(point)
   if hits:row['path_obstructions'].append(dict(point=point,hits=hits))
 row['center_obstructions']=capsule(row['center']);row['exit_obstructions']=capsule(row['exit']);x,y,z=row['center'];row['floor_support']=ray((x/100,-y/100,z/100),1)
escape=[]
for a,b in zip(plan['final_room']['escape_polyline'],plan['final_room']['escape_polyline'][1:]):
 for i in range(11):
  point=[a[j]+(b[j]-a[j])*i/10 for j in range(3)];point[2]+=60;hits=capsule(point)
  if hits:escape.append(dict(point=point,hits=hits))
result=dict(escape_path_obstructions=escape,item_surfaces=surface,hiding_contracts=hides,method='Actual source mesh BVH rays and nearest surface samples at five capsule-axis points, radius30cm halfheight60cm. This is a bounded static check, not runtime sweep/LOS proof.');(O/'item-hide-measurements.json').write_text(json.dumps(result,indent=2));print('RESULT',json.dumps(result))
