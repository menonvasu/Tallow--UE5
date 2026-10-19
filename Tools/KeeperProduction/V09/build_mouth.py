"""V09 mouth, stage 2: our own game low poly. Flesh = 72 meridians of rings projected onto the teeth-free Tripo surface (lip crest,
gum rings, ribbed throat), our skirt that fills the hood's V split at HOOD_OPEN=1 and tucks behind its cut edges, and a hidden outer
sack closing the volume. Teeth = our own curved needle teeth at Tripo's tooth sites, re-aimed inward/back, plus our deep second ring.
Per-vertex attributes drive shading and the CHOMP morph. Output V09Mouth/mouth-v09-low.blend (MOUTH_Flesh, MOUTH_Teeth)."""
import bpy,bmesh,json,math,random,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix,noise
from mathutils.bvhtree import BVHTree

import sys;sys.path.append(str(Path(__file__).parent));import mouth_rig as MR
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';SV=json.loads((O/'tooth-survey.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(O/'mouth-v09-source.blend'))
for o in list(bpy.data.objects):
 if o.name.startswith('MOUTH_'):bpy.data.objects.remove(o)
CX,LY,LZ=SV['lip_centre'];C0=Vector((CX,LY,LZ));NA=72;NB=30;rng=random.Random(9);UP=Vector((0,1,0))
bm=bmesh.new();bm.from_mesh(bpy.data.objects['TRIPO_C3_Opened'].data);T=BVHTree.FromBMesh(bm);bm.free()
# Hood split at HOOD_OPEN=1 (front half), measured on the Revision02 game mesh: x of each half's inner edge per height.
with bpy.data.libraries.load(str(R/'Tallow/SourceAssets/Keeper/V07RevealRevision02/keeper-reveal-revision02-export.blend')) as (src,dst):dst.meshes=['KeeperV07_SeamedSurface.001']
km=dst.meshes[0];kb=km.shape_keys.key_blocks;nk=40603
def kco(n):a=np.empty(len(km.vertices)*3);kb[n].data.foreach_get('co',a);return a.reshape(-1,3)[:nk]
KB,KH=kco('Basis'),kco('HOOD_OPEN');dl=KH-KB;mov=np.linalg.norm(dl,axis=1)>1e-5;Lh=mov&(dl[:,0]<0)&(KH[:,1]<.03);Rh=mov&(dl[:,0]>0)&(KH[:,1]<.03)
GZ=np.arange(1.97,2.43,.01);GL=np.array([KH[Lh&(abs(KH[:,2]-z)<.008),0].max(initial=-np.inf) for z in GZ]);GR=np.array([KH[Rh&(abs(KH[:,2]-z)<.008),0].min(initial=np.inf) for z in GZ])
ok=np.isfinite(GL)&np.isfinite(GR);GL=np.interp(GZ,GZ[ok],GL[ok]);GR=np.interp(GZ,GZ[ok],GR[ok])
HF=[tuple(p.vertices) for p in km.polygons if p.material_index==0 and KB[list(p.vertices)][:,2].min()>1.85];HOODS=[(t,BVHTree.FromPolygons((KB+t*(KH-KB)).tolist(),HF)) for t in (.6,.7,.8,.9,1.)]
bpy.data.meshes.remove(km)
def inside_gap(x,z,margin):
 if z<GZ[0] or z>2.392:return False
 return np.interp(z,GZ,GL)+margin<x<np.interp(z,GZ,GR)-margin
# Cavity centre line by depth: y-slices, radial rays, refined; a slice counts only while its hits are front faces (centre in the air).
def ray(o,d,dist=.5):
 h=T.ray_cast(o,d.normalized(),dist);return h[0]
def in_air(c,n=24):
 hs=[(h[0],h[1],dv) for a in np.linspace(0,math.tau,n,endpoint=False) for dv in [Vector((math.cos(a),0,math.sin(a)))] for h in [T.ray_cast(c,dv,.2)] if h[0]]
 return hs if len(hs)>=n-2 and sum(h[1].dot(dv)<0 for h,dv in [((x[0],x[1]),x[2]) for x in hs])>=.9*len(hs) else None
DEP=np.arange(.05,.30,.005);CEN=[];c=Vector((CX,LY,LZ))
for d in DEP:
 c=Vector((c.x,LY+d,c.z));hs=None
 for it in range(3):
  hs=in_air(c)
  if not hs:break
  c=sum((h[0] for h in hs),Vector())/len(hs);c.y=LY+d
 if not hs:break
 CEN.append(c.copy())
DEP=DEP[:len(CEN)]
def cen(d):
 i=min(len(CEN)-2,max(0,int((d-DEP[0])/.005)));t=(d-DEP[i])/.005;return CEN[i].lerp(CEN[i+1],max(0.,min(1.,t)))
AX=(cen(.12)-cen(.05)).normalized();END=ray(cen(.06),AX,.4);dend=min(END.y-LY-.006,DEP[-1])
# Meridian profiles: front pivot rays for lips/disc, depth slices for the throat; resampled by arc length (front-weighted).
PF=C0+Vector((0,-.17,0))
def front_hits(th):
 e=Vector((math.cos(th),0,math.sin(th)));hits=[]
 for phi in np.arange(62,9,-.35):
  p=math.radians(phi);dv=UP*math.cos(p)+e*math.sin(p);h=T.ray_cast(PF,dv,.6)
  if h[0] and h[0].y-LY<.06:hits.append((h[0],h[1]))
 return hits
rad=lambda h:(Vector((h.x,0,h.z))-Vector((C0.x,0,C0.z))).length
FH=[front_hits(-math.pi/2+math.tau*j/NA) for j in range(NA)]
# Profile start = outer shoulder of the forward-facing disc (Tripo's outer gum ring), smoothed around the lip so the rim has no notches.
R0=np.array([rad(next(h for h,nn in hs if rad(h)<.156 and nn.y<-.25)) for hs in FH])
R0s=np.array([np.median(np.take(R0,range(j-3,j+4),mode='wrap')) for j in range(NA)]);R0s=np.convolve(np.concatenate([R0s[-2:],R0s,R0s[:2]]),np.ones(5)/5,'valid')
def profile(j):
 th=-math.pi/2+math.tau*j/NA;e=Vector((math.cos(th),0,math.sin(th)));hs=FH[j]
 k=min(range(len(hs)),key=lambda i:abs(rad(hs[i][0])-R0s[j])+(.02 if hs[i][1].y>-.1 else 0))
 crest=hs[k][0];disc=[h for h,nn in hs[k:]]
 pts=[crest]
 for h in disc[1:]:
  if (h-pts[-1]).length>.0015 and h.y-LY>=pts[-1].y-LY-.012:pts.append(h)
 d0=pts[-1].y-LY
 for d in np.arange(d0+.004,dend-.004,.004):
  c=cen(d);h=ray(c,e,.2)
  if h and (h-pts[-1]).length>.0015:pts.append(h)
 pts.append(END)
 P=np.array([p[:] for p in pts]);
 for it in range(2):P[1:-1]=.25*P[:-2]+.5*P[1:-1]+.25*P[2:]
 seg=np.linalg.norm(np.diff(P,axis=0),axis=1);s=np.concatenate([[0],np.cumsum(seg)]);s/=s[-1]
 u=np.linspace(0,1,NB);u=u+.22*np.sin(np.pi*u)*(1-u)  # denser rings over lips and gum rings than deep in the throat
 u=(u-u.min())/(u.max()-u.min())
 Q=np.stack([np.interp(u,s,P[:,k]) for k in range(3)],1)
 return [Vector(q) for q in Q],crest
rows=[];meta=[];PROF=[];RC=[];RB=[]
for j in range(NA):
 th=-math.pi/2+math.tau*j/NA;e=Vector((math.cos(th),0,math.sin(th)));prof,crest=profile(j);PROF.append((prof,crest))
 rc=(Vector((crest.x,0,crest.z))-Vector((C0.x,0,C0.z))).length;rb=rc
 while rb<rc+.035:  # fleshy collar: out to the hood's cut edge (+5 mm tuck), never more than 35 mm
  q=C0+e*rb
  if q.z<2.07 or not inside_gap(q.x,q.z,-.005):break
  rb+=.002
 RC.append(rc);RB.append(max(rb-rc,.004))
W=np.array(RB);W=np.convolve(np.concatenate([W[-3:],W,W[:3]]),np.ones(7)/7,'valid')  # smooth collar width around the lip
for j in range(NA):
 th=-math.pi/2+math.tau*j/NA;e=Vector((math.cos(th),0,math.sin(th)));prof,crest=PROF[j];rc=RC[j];rb=rc+float(W[j]);dc=crest.y-LY
 lump=lambda r,k:noise.noise(Vector((math.cos(th)*3,math.sin(th)*3,k)))*r
 at=lambda r,d,k=0.:Vector((C0.x,LY+d,C0.z))+e*(r+lump(.004,k))
 skirt=[at(rb-.016,dc+.05,1),at(rb,dc+.02,2),at(rc+.6*(rb-rc)+.002,dc+.008,3),at(rc+.28*(rb-rc)+.002,dc+.002,4)]
 sack=[]
 for f in [.25,.5,.75]:  # hidden outer wall, deep -> shallow: from behind the throat end up to the collar tuck, 2 cm outside the cavity
  d=(dc+.05)+(dend-dc-.05)*(1-f);c=cen(d);h=ray(c,e,.25);rw=((h-c).length if h else .04)+.02
  sack.append(c+e*(max(rw,rw+((rb-.016)-rw)*f)+lump(.004,5+f)))
 col=sack+skirt+prof
 rows.append(col);meta.append((th,len(sack),len(skirt)))
back=END+AX*.03
# Build flesh mesh: ring grid + two fans (back of the sack, end of the throat).
ns=len(rows[0]);verts=[v for col in rows for v in col]+[back];faces=[]
for j in range(NA):
 a,b=j*ns,((j+1)%NA)*ns
 for i in range(ns-2):faces.append((a+i,a+i+1,b+i+1,b+i))
 faces.append((a+ns-2,a+ns-1,b+ns-1,b+ns-2))
 faces.append((len(verts)-1,a,b))
me=bpy.data.meshes.new('MOUTH_Flesh');me.from_pydata([v[:] for v in verts],[],faces);me.update()
# Attributes on build order (before the end pole merge renumbers vertices): part 0 sack/1 skirt/2 lips+disc/3 throat, profile param, chomp weight.
nsk,nskirt=meta[0][1],meta[0][2];ring=[i for j in range(NA) for i in range(ns)]+[0]
cls=lambda i:0 if i<nsk else 1 if i<nsk+nskirt else 2 if i<nsk+nskirt+int(NB*.42) else 3
def attr(m,name,vals,kind='FLOAT'):
 a=m.attributes.new(name,kind,'POINT');a.data.foreach_set('value' if kind=='FLOAT' else 'vector',np.asarray(vals,dtype=np.float32).ravel())
attr(me,'mouth_part',[cls(i) for i in ring])
attr(me,'uv_ring',ring);attr(me,'uv_mer',[j for j in range(NA) for i in range(ns)]+[0])  # seam bookkeeping
CUTS=(nsk,nsk+nskirt+int(NB*.42))
attr(me,'mouth_prof',[(i-nsk-nskirt)/(NB-1) if i>=nsk+nskirt else -1+i/(nsk+nskirt) for i in ring])
attr(me,'chomp_w',[0. if i<=nsk+1 else min(1.,(i-nsk-1)/3.) for i in ring])  # skirt edge stays glued to the hood, lips move fully
bmf=bmesh.new();bmf.from_mesh(me);bmf.verts.ensure_lookup_table()
bmesh.ops.pointmerge(bmf,verts=[bmf.verts[j*ns+ns-1] for j in range(NA)],merge_co=END)  # throat end pole
bmesh.ops.recalc_face_normals(bmf,faces=bmf.faces);bmf.to_mesh(me);bmf.free()
fl=bpy.data.objects.new('MOUTH_Flesh',me);bpy.context.scene.collection.objects.link(fl)
# Orientation: the inner surface must face into the cavity (toward the axis).
me.update();mid=next(v for v in me.vertices if v.co.y-LY>.08 and abs(v.co.x-CX)>.03 and me.attributes['mouth_part'].data[v.index].value==3)
if mid.normal.dot(cen(mid.co.y-LY)-mid.co)<0:
 bmf=bmesh.new();bmf.from_mesh(me);bmesh.ops.reverse_faces(bmf,faces=bmf.faces);bmf.to_mesh(me);bmf.free();me.update()
for p in me.polygons:p.use_smooth=True
# Collision relax: collar/outer-wall vertices that touch the opened hood (inward mask folds at the lower V) step toward the maw axis.
pt=np.zeros(len(me.vertices));me.attributes['mouth_part'].data.foreach_get('value',pt);relaxed=set()
for it in range(15):
 P=np.empty(len(me.vertices)*3);me.vertices.foreach_get('co',P);P=P.reshape(-1,3);FF=[tuple(q.vertices) for q in me.polygons]
 Q=MR.closed(P);hit={b for t,H in HOODS for a,b in H.overlap(BVHTree.FromPolygons((Q+t*(P-Q)).tolist(),FF))}
 if not hit:break
 for f in hit:
  for v in FF[f]:
   if pt[v]<=1:c=cen(max(.05,min(P[v,1]-LY,dend)));r=Vector((P[v,0]-c.x,0,P[v,2]-c.z));P[v]-=np.array(r.normalized()*.004);relaxed.add(v)
 me.vertices.foreach_set('co',P.ravel());me.update()
depth=np.array([v.co.y-LY for v in me.vertices]);attr(me,'mouth_depth',depth);attr(me,'axis_pt',[cen(max(0,min(d,dend)))[:] for d in depth],'FLOAT_VECTOR')
# Teeth: Tripo sites -> our curved needles. Base sunk 4 mm under our flesh, aim = gum normal + inward + back, hooked tip.
bmf=bmesh.new();bmf.from_mesh(me);bmf.normal_update();FT=BVHTree.FromBMesh(bmf);bmf.free()
tv=[];tf=[];tooth_attr=[];bases=[]
def tooth(base,n,inward,L,r0,hook,tid,ring,blunt=False):
 back=1.0 if ring in (0,2) else .45  # throat-ring fangs rake back down the throat and leave the centre black
 dirv=(n*.8+inward*.8+UP*back);dirv.normalize()
 if dirv.dot(n)<.3:dirv=(dirv+n*(.3-dirv.dot(n))*1.5).normalized()
 q=Matrix.Rotation(math.radians(rng.uniform(-11,11)),3,n.cross(dirv).normalized() if n.cross(dirv).length>1e-4 else Vector((1,0,0)))@Matrix.Rotation(math.radians(rng.uniform(-14,14)),3,n)
 dirv=(q@dirv).normalized();hk=(inward*.7+UP*.7);hk=(hk-dirv*hk.dot(dirv));hk=hk.normalized() if hk.length>1e-4 else Vector((0,0,0))
 side=dirv.cross(hk).normalized() if hk.length else dirv.orthogonal().normalized();up2=side.cross(dirv).normalized()
 start=len(tv);hs=[0,.3,.58,.82];rs=[1,.74,.47,.22]
 if blunt:hs=[0,.3,.58,.78];rs=[1,.8,.62,.5];L*=.62
 sp=lambda t:base+dirv*(L*t)+hk*(L*hook*t*t)
 for h,rr in zip(hs,rs):
  c=sp(h);tan=(sp(h+.02)-sp(h)).normalized();s2=(side-tan*side.dot(tan)).normalized();u2=tan.cross(s2)
  for k in range(6):
   a=math.tau*k/6+.3;tv.append((c+s2*math.cos(a)*r0*rr+u2*math.sin(a)*r0*rr*.72)[:]);tooth_attr.append((tid,h,ring,*base,*dirv,*hk,k))
 tip=sp(1.)if not blunt else sp(.86);tv.append(tip[:]);tooth_attr.append((tid,1.,ring,*base,*dirv,*hk,0))
 for r in range(3):
  for k in range(6):tf.append((start+r*6+k,start+r*6+(k+1)%6,start+(r+1)*6+(k+1)%6,start+(r+1)*6+k))
 for k in range(6):tf.append((start+18+k,start+18+(k+1)%6,start+24))
kept=0;skipped=0;why={'far':0,'outer':0,'facing':0,'dup':0}
for t in sorted(SV['teeth'],key=lambda t:-t['length']):
 b=Vector(t['base']);h=FT.find_nearest(b)
 if h[0] is None or h[3]>.03:skipped+=1;why['far']+=1;continue
 p,n=h[0],h[1].normalized();c=cen(max(0,min(p.y-LY,dend)));inward=Vector((c.x-p.x,0,c.z-p.z));inward=inward.normalized() if inward.length>1e-5 else Vector((0,0,0))
 th_t=math.atan2(p.z-C0.z,p.x-C0.x);jj=int(round((th_t+math.pi/2)/math.tau*NA))%NA;rad_t=math.hypot(p.x-C0.x,p.z-C0.z)
 if rad_t>RC[jj]-.006 or n.dot(-inward)>.35:skipped+=1;why['outer' if rad_t>RC[jj]-.006 else 'facing']+=1;continue  # Tripo's outer-face needles: lamprey teeth sit on the inner disc only
 if any((p-q).length<.0045 for q in bases):skipped+=1;why['dup']+=1;continue
 L=t['length']*1.1*rng.uniform(.8,1.25);L=min(max(L,.009),.045)
 r0=min(max(t['base_radius']*1.3,L*.15),.0075)  # cones with body, not straws
 bases.append(p);ring=0 if t['radial_m']<.09 else 1;tooth(p-n*.004,n,inward,L,r0,.30 if ring==0 else .18,kept,ring,blunt=rng.random()<.08);kept+=1
# Our rasp rows: short conical teeth scattered over the gum rings between the lip shoulder and the throat (lamprey disc), >=8 mm apart.
mp=np.zeros(len(me.vertices));me.attributes['mouth_prof'].data.foreach_get('value',mp);pp=np.zeros(len(me.vertices));me.attributes['mouth_part'].data.foreach_get('value',pp)
cand=[i for i in range(len(me.vertices)) if pp[i]==2 and .05<mp[i]<.34];rng.shuffle(cand);rasp=0
for vi in cand:
 v=me.vertices[vi];n=v.normal.copy();tg=n.orthogonal().normalized();tg=Matrix.Rotation(rng.uniform(0,math.tau),3,n)@tg
 h=FT.find_nearest(v.co+tg*rng.uniform(0,.004))
 if h[0] is None:continue
 p,n=h[0],h[1].normalized()
 if any((p-q).length<.008 for q in bases):continue
 c=cen(max(.05,min(p.y-LY,dend)));inward=Vector((c.x-p.x,0,c.z-p.z))
 if inward.length<1e-4:continue
 inward.normalize();L=rng.uniform(.006,.015);bases.append(p);tooth(p-n*.002,n,inward,L,max(.0022,L*.24),.12,kept,3);kept+=1;rasp+=1
 if rasp>=80:break
# Our deep second ring of hooked fangs, 11 around the throat ~12 cm in.
dr=.12;c=cen(dr);deep=0
for k in range(11):
 a=math.tau*k/11+rng.uniform(-.18,.18);e=Vector((math.cos(a),0,math.sin(a)));h=FT.ray_cast(c,e,.2)
 if not h[0]:continue
 p,n=h[0],h[1].normalized()
 if n.dot(c-p)<0:n=-n
 inward=(c-p);inward.y=0;inward.normalize();L=rng.uniform(.022,.036);tooth(p-n*.004,n,inward,L,min(.0065,L*.17),.35,kept,2);kept+=1;deep+=1
tm=bpy.data.meshes.new('MOUTH_Teeth');tm.from_pydata(tv,[],tf);tm.update()
bmt=bmesh.new();bmt.from_mesh(tm);bmesh.ops.recalc_face_normals(bmt,faces=bmt.faces);bmt.to_mesh(tm);bmt.free()
for p in tm.polygons:p.use_smooth=True
TA=np.array(tooth_attr,dtype=np.float32)
for name,cols,kind in [('tooth_id',[0],'FLOAT'),('tooth_h',[1],'FLOAT'),('tooth_ring',[2],'FLOAT'),('tooth_base',[3,4,5],'FLOAT_VECTOR'),('tooth_dir',[6,7,8],'FLOAT_VECTOR'),('tooth_hook',[9,10,11],'FLOAT_VECTOR'),('tooth_k',[12],'FLOAT')]:
 a=tm.attributes.new(name,kind,'POINT');a.data.foreach_set('value' if kind=='FLOAT' else 'vector',TA[:,cols].ravel())
th_=bpy.data.objects.new('MOUTH_Teeth',tm);bpy.context.scene.collection.objects.link(th_)
# Saliva strands (ours): stretched mucus strings between upper and lower gums, sagging; every vertex is a blend of its two flesh anchors
# (strand_a/strand_b/strand_t) so the fit can carry them through HOOD_OPEN and CHOMP exactly like the lips they hang from.
FV=me.vertices;up_=[i for i in range(len(FV)) if pp[i]==2 and .08<mp[i]<.30 and FV[i].co.z>LZ+.035];lo_=[i for i in range(len(FV)) if pp[i]==2 and .08<mp[i]<.30 and FV[i].co.z<LZ-.035]
sv=[];sfc=[];sat=[];used=[]
for k in range(5):
 a_=rng.choice(up_);A=FV[a_].co
 cand=[i for i in lo_ if abs(FV[i].co.x-A.x)<.07 and all(abs(FV[i].co.x-FV[j].co.x)>.012 for j in used)] or lo_
 b_=rng.choice(cand);used.append(b_);B=FV[b_].co;Lb=(B-A).length
 sag=Vector((rng.uniform(-.2,.2),rng.uniform(.0,.35),-1)).normalized()*Lb*rng.uniform(.12,.28);re=rng.uniform(.0032,.0048);rm=re*rng.uniform(.28,.45);segs=12;start=len(sv)
 Ai=A-FV[a_].normal*.003;Bi=B-FV[b_].normal*.003
 for si in range(segs+1):
  t=si/segs;c=Ai.lerp(Bi,t)+sag*4*t*(1-t);tan=((Bi-Ai)+sag*4*(1-2*t)).normalized();s1=tan.orthogonal().normalized();s2=tan.cross(s1)
  rad=rm+(re-rm)*abs(2*t-1)**1.6*(1+.5*abs(2*t-1)**6)
  for q in range(5):
   ang=math.tau*q/5;sv.append((c+(s1*math.cos(ang)+s2*math.sin(ang))*rad)[:]);sat.append((a_,b_,t,q,k))
 for si in range(segs):
  for q in range(5):sfc.append((start+si*5+q,start+si*5+(q+1)%5,start+(si+1)*5+(q+1)%5,start+(si+1)*5+q))
sm=bpy.data.meshes.new('MOUTH_Strands');sm.from_pydata(sv,[],sfc);sm.update()
bms=bmesh.new();bms.from_mesh(sm);bmesh.ops.recalc_face_normals(bms,faces=bms.faces);bms.to_mesh(sm);bms.free()
for p_ in sm.polygons:p_.use_smooth=True
SA=np.array(sat,dtype=np.float32)
for name,col in [('strand_a',0),('strand_b',1),('strand_t',2),('strand_k',3),('strand_id',4)]:
 a2=sm.attributes.new(name,'FLOAT','POINT');a2.data.foreach_set('value',SA[:,col])
st_=bpy.data.objects.new('MOUTH_Strands',sm);bpy.context.scene.collection.objects.link(st_)
for o,mn in [(fl,'M_KeeperV07_Mouth'),(th_,'M_KeeperV07_Teeth'),(st_,'M_KeeperV07_Mouth')]:
 m=bpy.data.materials.get(mn) or bpy.data.materials.new(mn);o.data.materials.append(m)
fl.data.calc_loop_triangles();tm.calc_loop_triangles()
for o in bpy.data.objects:o.hide_render=o.hide_viewport=not o.name.startswith('MOUTH_')
fl['uv_cuts']=list(CUTS)
bpy.ops.wm.save_as_mainfile(filepath=str(O/'mouth-v09-low.blend'))
rep={'flesh_vertices':len(me.vertices),'flesh_triangles':len(me.loop_triangles),'teeth':kept,'teeth_from_tripo_sites':kept-deep-rasp,'rasp_teeth':rasp,'deep_ring':deep,'tripo_sites_skipped':skipped,'skip_reasons':why,
 'teeth_triangles':len(tm.loop_triangles),'strand_triangles':sum(len(p_.vertices)-2 for p_ in sm.polygons),'total_triangles':len(me.loop_triangles)+len(tm.loop_triangles)+sum(len(p_.vertices)-2 for p_ in sm.polygons),'collision_relaxed_vertices':len(relaxed),'collision_relax_iterations':it,'meridians':NA,'profile_rings':NB,'throat_depth_m':dend,'rings_per_meridian':ns}
(O/'build-audit.json').write_text(json.dumps(rep,indent=2));print('RESULT '+json.dumps(rep))
