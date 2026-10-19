"""Fit custom deform rig onto the retained V06 Tripo surface. No geometry replacement."""
import bpy,json,math,numpy as np
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-tripo-production-pbr.blend'))
o=bpy.data.objects['SK_Keeper_Tripo_Direct'];mesh=o.data;scene=bpy.context.scene
sample=np.load(OUT/'semantic-source.npz');co=sample['positions'];colors=sample['colors']
assert len(co)==len(mesh.vertices)
armdata=bpy.data.armatures.new('Keeper_Direct_Skeleton');rig=bpy.data.objects.new('KEEPER_PRODUCTION_RIG',armdata);scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
def bone(n,h,t,p=None):
 b=armdata.edit_bones.new(n);b.head=h;b.tail=t
 if p:b.parent=armdata.edit_bones[p]
 return b
bone('root',(0,0,0),(0,0,.2));bone('body',(0,0,1.4),(0,0,1.95),'root');bone('neck',(0,0,1.95),(0,-.02,2.15),'body');bone('head',(0,-.02,2.15),(0,-.03,2.39),'neck')
for i in range(3):bone('cloak_'+str(i),(0,.03,.15+i*.55),(0,.03,.70+i*.55),'body')
# Measured front geometry and depth cross sections, meters; generated mesh is mildly asymmetrical.
def point(x,y,z,sign):return Vector((sign*x+(-.08 if sign<0 else 0),y,z))
landmarks=[[(.18,.06,2.03),(.32,-.13,1.85),(.44,-.28,1.68),(.47,-.36,1.59)],[(.13,-.015,1.77),(.26,-.235,1.52),(.34,-.345,1.35),(.34,-.43,1.25)],[(.12,-.07,1.45),(.16,-.26,1.30),(.19,-.40,1.17),(.20,-.48,1.08)]]
tips=[[(.41,-.37,1.52),(.445,-.39,1.44),(.475,-.40,1.42),(.50,-.40,1.44),(.53,-.38,1.48)],[(.29,-.43,1.18),(.32,-.46,1.11),(.35,-.47,1.10),(.375,-.46,1.13),(.40,-.44,1.17)],[(.15,-.48,1.01),(.18,-.51,.95),(.205,-.52,.94),(.23,-.50,.97),(.25,-.48,1.01)]]
arms=[];segments=[]
for pair in range(1,4):
 for side,sign in [('L',1),('R',-1)]:
  prefix=f'arm_{pair}.{side}';sh,el,wr,pa=[point(*v,sign) for v in landmarks[pair-1]]
  for suffix,h,t,parent in [('upper',sh,el,'body'),('lower',el,wr,prefix+'_upper'),('hand',wr,pa,prefix+'_lower')]:
   name=prefix+'_'+suffix;bone(name,h,t,parent);segments.append((name,h,t,pair,side))
  for d,tip in enumerate(tips[pair-1]):
   end=point(*tip,sign);base=pa.lerp(end,.16)
   pieces=2 if d==0 else 3;previous=prefix+'_hand'
   for j in range(pieces):
    name=f'{prefix}_digit{d+1}_{"abc"[j]}';h=base.lerp(end,j/pieces);t=base.lerp(end,(j+1)/pieces)
    bone(name,h,t,previous);segments.append((name,h,t,pair,side));previous=name
  arms.append(dict(pair=pair,side=side,shoulder=list(sh),elbow=list(el),wrist=list(wr),upper=prefix+'_upper',lower=prefix+'_lower',hand=prefix+'_hand'))
bpy.ops.object.mode_set(mode='OBJECT')
# Assign arms using local capsule distances and foreground depth, not nearest-bone alone.
dist=[];proj=[]
for name,h,t,pair,side in segments:
 h=np.array(h);t=np.array(t);v=t-h;q=np.clip(((co-h)@v)/(v@v),0,1);p=h+q[:,None]*v
 dist.append(np.linalg.norm(co-p,axis=1));proj.append(p)
dist=np.array(dist).T;nearest=dist.argmin(1);mind=dist.min(1);nearpos=np.array([proj[j][i] for i,j in enumerate(nearest)])
# Beyond shoulders all arm surfaces sit forward of the shroud. Texture luminance is supporting evidence only.
# Surface geodesic segmentation keeps hanging sleeve fringe attached to its actual arm.
# Capsule-only weights cut across dangling fabric, so seed sleeve centre lines and torso separately.
import heapq
adj=[[] for _ in co]
for e in mesh.edges:
 aa,bb=e.vertices;length=float(np.linalg.norm(co[aa]-co[bb]));adj[aa].append((bb,length));adj[bb].append((aa,length))
region_ids=np.full(len(co),-1,dtype=int);cost=np.full(len(co),np.inf);queue=[]
def seed(index,label):
 cost[index]=0;region_ids[index]=label;heapq.heappush(queue,(0,int(index),label))
bodyseeds=(co[:,2]<.85)|(np.abs(co[:,0]+.04)<.045)|((co[:,1]>.09)&(np.abs(co[:,0]+.04)<.14))|(co[:,2]>2.11)
for i in np.where(bodyseeds)[0]:seed(i,0)
for ai,arm in enumerate(arms,1):
 for h,t in [(Vector(arm['shoulder']),Vector(arm['elbow'])),(Vector(arm['elbow']),Vector(arm['wrist']))]:
  for q in np.linspace(.18,1,20):
   point=h.lerp(t,float(q));ds=np.linalg.norm(co-np.array(point),axis=1)
   for i in np.argpartition(ds,4)[:4]:seed(int(i),ai)
 hand=rig.data.bones[arm['hand']]
 for q in np.linspace(0,2,10):
  point=hand.head_local.lerp(hand.tail_local,float(q));ds=np.linalg.norm(co-np.array(point),axis=1)
  for i in np.argpartition(ds,4)[:4]:seed(int(i),ai)
while queue:
 distance,i,label=heapq.heappop(queue)
 if distance>cost[i]+1e-12 or region_ids[i]!=label:continue
 for j,length in adj[i]:
  new=distance+length
  if new<cost[j]-1e-12:cost[j]=new;region_ids[j]=label;heapq.heappush(queue,(new,j,label))
armmask=region_ids>0
hand_regions={};skin_surface=np.zeros(len(co),dtype=bool)
for ai,arm in enumerate(arms,1):
 candidates=[j for j,seg in enumerate(segments) if seg[3:]==(arm['pair'],arm['side'])]
 ids=np.where(region_ids==ai)[0];nearest[ids]=np.array(candidates)[dist[ids][:,candidates].argmin(1)]
 # Separate actual flesh from cloth by surface connectivity, not per-vertex brightness.
 # Dark fingertips inherit the flesh label; dark hanging cuff fringe remains sleeve.
 local_cost=np.full(len(co),np.inf);local_label=np.zeros(len(co),dtype=int);q=[]
 for i in ids:
  is_skin=co[i,2]<arm['wrist'][2]-.02 and colors[i].mean()>.48
  is_sleeve=co[i,2]>arm['wrist'][2]+.025
  if is_skin or is_sleeve:
   lab=1 if is_skin else 0;local_cost[i]=0;local_label[i]=lab;heapq.heappush(q,(0,int(i),lab))
 while q:
  dd,i,lab=heapq.heappop(q)
  if dd>local_cost[i]+1e-12:continue
  for j,length in adj[i]:
   if region_ids[j]!=ai:continue
   nd=dd+length*(1+8*abs(float(colors[i].mean()-colors[j].mean())))
   if nd<local_cost[j]-1e-12:local_cost[j]=nd;local_label[j]=lab;heapq.heappush(q,(nd,j,lab))
 skin_surface[ids]=local_label[ids]==1
 hand_regions[arm['hand']]=(region_ids==ai)&skin_surface
cloth=~armmask;headmask=(co[:,2]>2.12);neckmask=(co[:,2]>1.98)&~headmask
names=[b.name for b in armdata.bones]
for n in names:o.vertex_groups.new(name=n)
labels=[];parts=[]
weights=np.zeros((len(co),len(names)));ni={n:i for i,n in enumerate(names)}
for i,v in enumerate(mesh.vertices):
 if armmask[i]:
  j=int(nearest[i]);name,h,t,pair,side=segments[j]
  eligible=[k for k,seg in enumerate(segments) if seg[3:]==(pair,side) and (('_hand' in seg[0] or '_digit' in seg[0]) if skin_surface[i] else seg[0].endswith(('_upper','_lower')))]
  local_min=min(dist[i,k] for k in eligible)
  choices=[k for k in eligible if dist[i,k]<local_min+(.018 if skin_surface[i] else .075)]
  choices=sorted(choices,key=lambda k:dist[i,k])[:2]
  ws=np.array([1/max(.006,dist[i,k])**(4 if skin_surface[i] else 2) for k in choices]);ws/=ws.sum()
  for k,w in zip(choices,ws):weights[i,ni[segments[k][0]]]=w
  labels.append(name)
 else:
  name='head' if headmask[i] else 'neck' if neckmask[i] else 'body';weights[i,ni[name]]=1;labels.append(name)
# Diffuse across actual mesh edges, smoothing fused cloth boundaries and avoiding isolated needle vertices.
edges=np.array([e.vertices[:] for e in mesh.edges]);degree=np.bincount(edges.flatten(),minlength=len(co))
for iteration in range(8):
 summed=np.zeros_like(weights);np.add.at(summed,edges[:,0],weights[edges[:,1]]);np.add.at(summed,edges[:,1],weights[edges[:,0]])
 weights=.65*weights+.35*summed/np.maximum(1,degree[:,None])
# Human fingers keep independent deform chains. Palm/finger weights remain locally smooth.
# Four influence export cap with normalized weights; cloth morph shares the same continuous transition.
# Cut only generated fusion seams: duplicate boundary vertices without removing or moving any face.
# Each original triangle retains its UV and surface, but cloth can gather independently of sleeved arms.
def region_of(name):
 if not name.startswith('arm_'):return 'cloth'
 base=name.split('_')[0]+'_'+name.split('_')[1]
 return base+('::sleeve' if name.endswith(('_upper','_lower')) else '::skin')
regions=[region_of(names[int(np.argmax(row))]) for row in weights]
oldmesh=mesh;verts=[];faces=[];uvs=[];mapping={};oldids=[];newregions=[]
for poly in oldmesh.polygons:
 rs=[regions[i] for i in poly.vertices];region=max(set(rs),key=rs.count);face=[]
 for li in poly.loop_indices:
  vi=oldmesh.loops[li].vertex_index;k=(vi,region)
  if k not in mapping:mapping[k]=len(verts);verts.append(co[vi]);oldids.append(vi);newregions.append(region)
  face.append(mapping[k]);uvs.append(oldmesh.uv_layers['ProductionUV'].data[li].uv[:])
 faces.append(face)
mesh=bpy.data.meshes.new('Keeper_Tripo_SeparatedSeams');mesh.from_pydata(verts,[],faces);mesh.update()
for mat in oldmesh.materials:mesh.materials.append(mat)
uv=mesh.uv_layers.new(name='ProductionUV')
for i,p in enumerate(uvs):uv.data[i].uv=p
for poly in mesh.polygons:poly.use_smooth=True
o.data=mesh
for n in names:
 if n not in o.vertex_groups:o.vertex_groups.new(name=n)
co=co[oldids];colors=colors[oldids];weights=weights[oldids];headmask=headmask[oldids];cloth=cloth[oldids];armmask=armmask[oldids];labels=[labels[i] for i in oldids]
hand_regions={n:v[oldids] for n,v in hand_regions.items()}
for i,region in enumerate(newregions):
 if region=='cloth':
  weights[i]=0;weights[i,ni['head' if headmask[i] else 'neck' if co[i,2]>1.98 else 'body']]=1
 else:
  base,kind=region.split('::')
  for j,n in enumerate(names):
   allowed=n.startswith(base+'_') and ((n.endswith(('_upper','_lower'))) if kind=='sleeve' else ('_hand' in n or '_digit' in n))
   if not allowed:weights[i,j]=0
  if weights[i].sum()<1e-8:weights[i,ni[base+('_lower' if kind=='sleeve' else '_hand')]]=1
  weights[i]/=weights[i].sum()
keep=np.argsort(weights,axis=1)[:,-4:];filtered=np.zeros_like(weights)
for i in range(len(co)):
 filtered[i,keep[i]]=weights[i,keep[i]];filtered[i]/=filtered[i].sum()
 for j in keep[i]:
  if filtered[i,j]>1e-5:o.vertex_groups[names[j]].add([i],float(filtered[i,j]),'REPLACE')
cloth_weight=filtered[:,ni['body']]
# Morph keeps every original vertex; gathers only the cloth, leaving six original arms intact.
o.shape_key_add(name='Basis')
for n in ['FULL_HORROR','CHOMP','Blink_L','Blink_R']:o.shape_key_add(name=n)
horror=o.data.shape_keys.key_blocks['FULL_HORROR']
for i,v in enumerate(mesh.vertices):
 if cloth_weight[i]>.00001 and v.co.z<1.98:
  t=max(0,min(1,v.co.z/1.98));p=horror.data[i].co;p.z=1.20+v.co.z*.394;p.y+=.22*(1-t)**1.5;p.x*=1-.25*(1-t);p[:]=v.co.lerp(p, float(cloth_weight[i]))[:]
 # Head/hood untouched. Mouth and centre seam opening are explicitly deferred.
mod=o.modifiers.new('Keeper six arm deformation','ARMATURE');mod.object=rig;o.parent=rig
# Vertex lists support actual fingertip contact measurements after IK.
for a in arms:
 prefix=f"arm_{a['pair']}.{a['side']}";ids=np.where(hand_regions[a['hand']] & (co[:,2]<np.array(a['wrist'])[2]-.08))[0].tolist()
 parts.append({'name':prefix+'_digit_region','indices':ids})
report=dict(arms=arms,parts=parts,vertices=len(mesh.vertices),triangles=len(mesh.polygons),bones=len(armdata.bones),max_influences=max(len(v.groups) for v in mesh.vertices),unweighted=sum(not v.groups for v in mesh.vertices),arm_vertices=int(armmask.sum()),cloth_vertices=int(cloth.sum()),source_surface='All V06 faces retained at identical bind coordinates; fused arm/cloth seams split by duplicating shared vertices',status='sleeved human-hand bind candidate; stress visual review required',fingers_per_hand=5,head_opening='deferred issue44; no head topology/morph changes')
(OUT/'production-audit.json').write_text(json.dumps(report,indent=2));np.save(OUT/'vertex-region-labels.npy',np.array(labels))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-direct-rigged.blend'));print('DIRECT_RIG '+json.dumps({k:v for k,v in report.items() if k not in ['arms','parts']}))
