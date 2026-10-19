"""Measures rooms/openings/fixtures of HouseArchitecture.blend in Unreal cm (x, -y, z). Read-only on the house."""
import bpy,bmesh,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
R=Path(__file__).resolve().parents[2];H=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';O=R/'Tallow/SourceAssets/Production/HouseFurnishV03';bpy.ops.wm.open_mainfile(filepath=str(H/'HouseArchitecture.blend'))
def ue(v):return [round(v[0]*100,1),round(-v[1]*100,1),round(v[2]*100,1)]
def tree(o):return BVHTree.FromPolygons([o.matrix_world@v.co for v in o.data.vertices],[list(p.vertices) for p in o.data.polygons])
def islands(o):
 bm=bmesh.new();bm.from_mesh(o.data);bm.transform(o.matrix_world);bm.verts.ensure_lookup_table();seen=set();out=[]
 for v in bm.verts:
  if v.index in seen:continue
  stack=[v];grp=[]
  while stack:
   a=stack.pop()
   if a.index in seen:continue
   seen.add(a.index);grp.append(a.co.copy());stack+=[e.other_vert(a) for e in a.link_edges]
  lo=[min(p[i] for p in grp) for i in range(3)];hi=[max(p[i] for p in grp) for i in range(3)];out.append(dict(min=[round(lo[0]*100,1),round(-hi[1]*100,1),round(lo[2]*100,1)],max=[round(hi[0]*100,1),round(-lo[1]*100,1),round(hi[2]*100,1)]))
 bm.free();return out
floors={'Basement':-330,'Ground':0,'Upper':390};walls=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH' or '_Wall_' not in o.name:continue
 fl=o.name.split('_')[2];z0=floors[fl];t=tree(o);bb=[o.matrix_world@Vector(c) for c in o.bound_box];lo=Vector([min(v[i] for v in bb) for i in range(3)]);hi=Vector([max(v[i] for v in bb) for i in range(3)])
 along=0 if (hi.x-lo.x)>(hi.y-lo.y) else 1;across=1-along;mid=(lo[across]+hi[across])/2;rows={}
 for h in [15,60,120,200,260]:
  solid=[];s=lo[along]
  while s<=hi[along]:
   p=Vector((0,0,(z0+h)/100));p[along]=s;p[across]=mid-1;d=Vector((0,0,0));d[across]=1
   a=t.ray_cast(p,d,2.0);p2=p.copy();p2[across]=mid+1;b=t.ray_cast(p2,-d,2.0);solid.append(a[0] is not None or b[0] is not None or t.find_nearest(Vector((p.x if along==0 else mid,p.y if along==1 else mid,p.z)),0.02)[0] is not None);s+=.05
  gaps=[];start=None
  for i,ok in enumerate(solid+[True]):
   if not ok and start is None:start=i
   if ok and start is not None:gaps.append([round((lo[along]+start*.05)*100),round((lo[along]+(i-1)*.05)*100)]);start=None
  rows[h]=gaps
 axis='x' if along==0 else 'y';line=round((-mid if across==1 else mid)*100,1)
 conv=(lambda g:g) if along==0 else (lambda g:[-g[1],-g[0]])
 walls.append(dict(wall=o.name,floor=fl,runs_along=axis,at=('y=' if across==1 else 'x=')+str(line),thickness_cm=round((hi[across]-lo[across])*100,1),span=[round(lo[along]*100),round(hi[along]*100)] if along==0 else [round(-hi[along]*100),round(-lo[along]*100)],gaps_by_height={h:[conv(g) for g in gs] for h,gs in rows.items()}))
fix={}
for n in ['SM_HouseV01_PuzzleShelves','SM_HouseV01_DraftCloths','SM_HouseV01_WakeTimberScreen','SM_HouseV01_UnderStairCupboard','SM_HouseV01_FinishedStairJoinery','SM_HouseV01_Flue_Basement','SM_HouseV01_Flue_Ground','SM_HouseV01_LaundryChute_Basement','SM_HouseV01_LaundryChute_Ground','SM_HouseV01_LaundryChute_Upper','SM_HouseV01_Chimneys','SM_HouseV01_MainStairs','SM_HouseV01_BackStairs','SM_HouseV01_BasementStairs','SM_HouseV01_Balustrade','SM_HouseV01_WakeScreen','SM_HouseV01_WakeScreenDado','SM_HouseV01_CollectionRevealScreen','SM_HouseV01_Bay_WindowSeat_Ground','SM_HouseV01_Bay_WindowSeat_Upper']:
 o=bpy.data.objects.get(n)
 if o:isl=islands(o);fix[n]=isl if len(isl)<=80 else dict(count=len(isl),note='too many islands',sample=isl[:10])
(O/'architecture-survey.json').write_text(json.dumps(dict(units='Unreal cm; walls sampled every 5cm at heights above floor',walls=walls,fixtures=fix),indent=1));print('RESULT walls',len(walls))
