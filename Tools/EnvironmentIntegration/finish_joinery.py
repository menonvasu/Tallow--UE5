"""Authored own-UV joinery, draft signals and removable board source."""
import bpy,json,math,random
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';E=O/'Architecture';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'));mats={m.name.removeprefix('M_HeroRoomV02_'):m for m in bpy.data.materials if m.name.startswith('M_HeroRoomV02_')};groups={};rng=random.Random(252)
def mesh(name,verts,faces,mat='BlackenedTimber'):
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();ob=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(ob);me.materials.append(mats[mat]);groups.setdefault(name,[]).append(ob);return ob
def box(group,lo,hi,mat='BlackenedTimber'):
 return mesh(group,[(x,y,z) for z in [lo[2],hi[2]] for y in [lo[1],hi[1]] for x in [lo[0],hi[0]]],[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)],mat)
def beam(g,a,b,w=.045):
 a,b=Vector(a),Vector(b);o=box(g,(-w/2,-w/2,0),(w/2,w/2,(b-a).length));o.location=a;o.rotation_mode='QUATERNION';o.rotation_quaternion=(b-a).to_track_quat('Z','Y');return o
def spindle(g,x,y,z,h=.84):
 profile=[(0,.034),(.08,.034),(.10,.021),(.32,.022),(.38,.038),(.44,.020),(.74,.021),(.78,.032),(.84,.032)];v=[];f=[]
 for dz,r in profile:
  for k in range(8):a=k*math.tau/8;v.append((x+r*math.cos(a),y+r*math.sin(a),z+dz*h/.84))
 for j in range(len(profile)-1):
  for k in range(8):f.append((j*8+k,j*8+(k+1)%8,(j+1)*8+(k+1)%8,(j+1)*8+k))
 f.extend([tuple(reversed(range(8))),tuple((len(profile)-1)*8+k for k in range(8))]);return mesh(g,v,f)
# Slender lathed balusters/continuous handrails, clear walking lanes remain >=1.30m.
flights=[(8.28,4.2,6.72,.195,1.95),(7.32,6.72,4.2,2.145,3.9),(11.82,7.2,11.68,-3.106,0),(14.10,7.0,12.32,.195,3.9)]
for x,ya,yb,za,zb in flights:
 beam('FinishedStairJoinery',(x,ya,za+.86),(x,yb,zb+.86),.065)
 n=round(abs(yb-ya)/.28)
 for i in range(n+1):t=i/n;spindle('FinishedStairJoinery',x,ya+(yb-ya)*t,za+(zb-za)*t)
# Small cornice under stair-storage face breaks its broad slab into a cupboard story.
for y in [4.55,5.10,5.66,6.10]:beam('UnderStairPanelTrim',(7.405,y,.1),(7.405,y,1.65),.028)
beam('UnderStairPanelTrim',(7.405,4.55,1.65),(7.405,6.1,1.65),.035)
# Timber wake enclosure: interrupted uprights and crooked headers; no passage below 2.4m.
for x,y in [(0.6,5.7),(2.06,5.7),(0.6,2.2)]:box('WakeTimberScreen',(x-.055,y-.05,-3.3),(x+.055,y+.05,-.65))
beam('WakeTimberScreen',(.55,5.7,-.62),(2.15,5.7,-.68),.14)
# Soft draped cloth signals attached to walls, never random floor clutter.
for j,(x,y,z,axis) in enumerate([(.30,2.0,-.45,0),(15.30,3.0,2.80,0),(.30,2.2,6.65,0)]):
 v=[];f=[]
 for row in range(12):
  for col in range(6):
   u=col/5;t=row/11;v.append((x+.10*math.sin(u*math.pi*6+t)+.08*t,y+(u-.5)*.65,z-t*1.10+.025*math.sin(u*9)))
 for a in range(11):
  for b in range(5):k=a*6+b;f.append((k,k+1,k+7,k+6))
 ob=mesh('DraftCloths',v,f,'LimePlaster');sol=ob.modifiers.new('Cloth thickness','SOLIDIFY');sol.thickness=.004;bpy.context.view_layer.objects.active=ob;bpy.ops.object.modifier_apply(modifier=sol.name)
# Fine physical plank joints; authored per-floor and skipped over actual upper stair holes.
for z in [-3.3,0,3.9]:
 for i in range(1,51):
  y=i*.25
  for x0,x1 in [(0,5.94),(5.94,9.66),(9.66,14.005),(14.005,15.505),(15.505,15.6)]:
   if z==0 and x0==9.66 and 7.0<y<12.0:continue
   if z==3.9 and ((x0==5.94 and 4.5<y<8.8) or (x0==14.005 and 6.9<y<12.32)):continue
   box('FloorBoardJoints',(x0,y-.002,z+.001),(x1,y+.002,z+.002))
# A genuinely splintered removable board: local X long axis, pivot centered.
poly=[(-.8,-.075),(-.74,-.025),(-.81,.02),(-.76,.08),(.73,.065),(.81,.025),(.75,.008),(.8,-.07)];v=[(x,y,z) for z in [-.025,.025] for x,y in poly];n=len(poly);f=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)];mesh('SplinteredBoard',v,f,'AshTimber')
rows=json.loads((E/'manifest.json').read_text());new=[]
for group,obs in groups.items():
 bpy.ops.object.select_all(action='DESELECT')
 for ob in obs:ob.select_set(True)
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_HouseV01_'+group;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 if group not in ['FloorBoardJoints','DraftCloths']:
  mod=o.modifiers.new('Soft hand-finished edges','BEVEL');mod.width=.006;mod.segments=2;bpy.ops.object.modifier_apply(modifier=mod.name)
 uv=o.data.uv_layers.new(name='ProductionUV')
 for f in o.data.polygons:
  ax=max(range(3),key=lambda i:abs(f.normal[i]));a,b=([1,2] if ax==0 else [0,2] if ax==1 else [0,1])
  for li in f.loop_indices:v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v[a]*.65,v[b]*.65)
 bpy.ops.export_scene.fbx(filepath=str(E/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False);o.data.calc_loop_triangles();row=dict(mesh=o.name,original_actor_label=None,triangles=len(o.data.loop_triangles),materials=[m.name for m in o.data.materials],geometry='Original authored finishing joinery with new ProductionUV',collision='none');rows=[r for r in rows if r['mesh']!=o.name];rows.append(row);new.append(row)
 if group=='SplinteredBoard':o.hide_render=True
(E/'manifest.json').write_text(json.dumps(rows,indent=2));(O/'joinery-manifest.json').write_text(json.dumps(new,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseArchitecture.blend'));print('RESULT joinery',[(r['mesh'],r['triangles']) for r in new])
