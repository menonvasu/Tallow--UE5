"""V09 Keeper body repair. The 'holes' are not missing cloth: the Tripo surface was cut into body / six sleeve-arm pieces / six hands /
one cuff piece along coincident but UNWELDED seams whose two sides carry different skin weights (DEF_body 1.0 vs DEF_arm 1.0) and
different FULL_HORROR offsets (up to 0.56 m), so the seams tear open in poses and show the hollow interior. Repair:
 1. delete 5 sub-mm flap triangles (+10 orphan vertices) left on the hood seam boundary by the V07 clip (the zero-area UV faces);
 2. resolve the 2-edge non-manifold pinch on the mask forehead (a closed 4-face tetrahedron glued into the mask crack: delete it);
 3. weld every coincident seam cluster (never the 1 mm hood split, never the old maw/teeth slots); across each seam the skin weights
    and the shape-key offsets meet at the average of both sides, the corrections fading out geodesically (weights 5 cm, shape keys
    22 cm) on each side, so the joins deform as one surface instead of tearing. FULL_HORROR (on in every crawl/chase clip) lifts the
    lower mantle up to 0.56 m at the lower sleeve roots while the sleeves stayed put: those were the see-through gaps.
No new vertices are created, so no new UV islands or texels are needed: existing UVs keep their seams, T_KeeperTripo_* maps stay valid.
Outputs V09Mouth/keeper-v09-repaired.blend and repair-index-map.json (old index -> new index; welded-away vertices map to their survivor)."""
import bpy,bmesh,json,heapq,math,numpy as np
from pathlib import Path
from collections import defaultdict
from mathutils.kdtree import KDTree
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';O.mkdir(exist_ok=True)
SRC=R/'Tallow/SourceAssets/Keeper/V07RevealRevision02/keeper-reveal-revision02-export.blend';WW,WK=.05,.22
bpy.ops.wm.open_mainfile(filepath=str(SRC))
o=bpy.data.objects['SK_KeeperRiggedV07_Export'];me=o.data;rig=bpy.data.objects['KeeperV07'];assert len(rig.data.bones)==109
G=[g.name for g in o.vertex_groups];KN=[k.name for k in me.shape_keys.key_blocks];N0=len(me.vertices)
def snapshot(m):
 n=len(m.vertices);K={};W=np.zeros((n,len(G)))
 for kb in m.shape_keys.key_blocks:a=np.empty(n*3);kb.data.foreach_get('co',a);K[kb.name]=a.reshape(-1,3)
 for v in m.vertices:
  for g in v.groups:W[v.index,g.group]=g.weight
 return K,W
K0,W0=snapshot(me)
import sys;sys.path.insert(0,str(Path(__file__).parent));import audit_keeper_mesh as AK
SEAM=json.loads((R/'Tallow/SourceAssets/Keeper/V07RevealRevision01/coincident-seam-audit.json').read_text())
short=lambda a:{k:v for k,v in a.items() if k not in ('loops','parts','non_manifold_edge_ids','zero_area_uv_face_ids')}|{'loops':[{k:v for k,v in l.items() if k!='vids'} for l in a['loops']]}
before=short(AK.audit(o,SEAM))
bm=bmesh.new();bm.from_mesh(me);bm.verts.ensure_lookup_table();bm.faces.ensure_lookup_table()
dl=bm.verts.layers.deform.active;SH={k:bm.verts.layers.shape[k] for k in KN};uvl=bm.loops.layers.uv['ProductionUV'];orig=bm.verts.layers.int.new('v09_orig')
for v in bm.verts:v[orig]=v.index
maw={v.index for f in bm.faces if f.material_index in (1,2) for v in f.verts}  # parent replaces these: untouched
rep={'input':str(SRC.relative_to(R))}
# 1. sub-mm flaps on the hood seam boundary: zero-area UV triangles whose two outer vertices have no other face
flaps=[f for f in bm.faces if f.calc_area()<1e-7 and sum(len(v.link_faces)==1 for v in f.verts)==2]
orph=[v for f in flaps for v in f.verts if len(v.link_faces)==1];rep['flap_faces_deleted']=len(flaps);rep['flap_vertices_deleted']=len(orph)
bmesh.ops.delete(bm,geom=flaps,context='FACES_ONLY');bmesh.ops.delete(bm,geom=orph,context='VERTS')
# 2. non-manifold pinch on the mask forehead: a closed 4-face tetrahedron glued into the bottom of the mask's sculpted crack.
#    Deleting exactly those four faces leaves both shared edges with two faces each; no vertex is lost and no hole opens.
import itertools
nm=[e for e in bm.edges if len(e.link_faces)>2];rep['non_manifold_edges_before']=len(nm)
pinch={v for e in nm for v in e.verts};cand={w for f in {f for e in nm for f in e.link_faces} for w in f.verts}
tris={frozenset(f.verts):f for v in cand for f in v.link_faces if len(f.verts)==3}
tet=set()
for q in itertools.combinations(cand,4):
 fs=[tris.get(frozenset(t)) for t in itertools.combinations(q,3)]
 if all(fs) and any(v in pinch for v in q):tet|=set(fs)
pinch_orig={w[orig] for f in tet for w in f.verts};removed=len(tet);fill=[]
bmesh.ops.delete(bm,geom=list(tet),context='FACES_ONLY');bmesh.ops.delete(bm,geom=[e for e in bm.edges if not e.link_faces],context='EDGES')
rep['pinch_tetra_faces_removed']=removed;rep['non_manifold_edges_after_pinch']=sum(len(e.link_faces)>2 for e in bm.edges)
# 3. coincident seam clusters (boundary vertices at identical positions on different parts), excluding hood split and old maw
bm.verts.ensure_lookup_table();bm.edges.ensure_lookup_table();n=len(bm.verts)
part=-np.ones(n,int);k=0
for v in bm.verts:
 if part[v.index]>=0:continue
 st=[v];part[v.index]=k
 while st:
  x=st.pop()
  for e in x.link_edges:
   w=e.other_vert(x)
   if part[w.index]<0:part[w.index]=k;st.append(w)
 k+=1
bnd=[v for v in bm.verts if v.is_boundary and v.index not in maw];kd=KDTree(len(bnd))
for i,v in enumerate(bnd):kd.insert(v.co,i)
kd.balance();clus=[];seen=set()
for i,v in enumerate(bnd):
 if v.index in seen:continue
 m=[bnd[j] for _,j,_ in kd.find_range(v.co,1e-6)]
 if len({part[x.index] for x in m})<2:continue
 for x in m:seen.add(x.index)
 clus.append(sorted(m,key=lambda x:x.index))
rep['seam_clusters_welded']=len(clus);rep['seam_vertices_merged_away']=sum(len(c)-1 for c in clus)
def wvec(v):
 a=np.zeros(len(G))
 for g,w in v[dl].items():a[g]=w
 return a
def kvec(v):return np.array([(v[SH[k]]-v[SH['Basis']])[:] for k in KN[1:]])
# targets per cluster; per-member corrections
corrW={};corrK={}
for c in clus:
 tw=np.mean([wvec(x)/max(wvec(x).sum(),1e-9) for x in c],axis=0)
 K=[kvec(x) for x in c];tk=np.mean(K,axis=0)  # both sides share the jump
 for x,Kx in zip(c,K):corrW[x.index]=tw-wvec(x);corrK[x.index]=tk-Kx
# geodesic spread inside each original part (pre-weld topology), nearest seam vertex wins
def spread(src,limit):
 best={};pq=[(0.,i,i) for i in src]
 while pq:
  d,i,s=heapq.heappop(pq)
  if i in best:continue
  best[i]=(d,s);v=bm.verts[i]
  for e in v.link_edges:
   w=e.other_vert(v);nd=d+e.calc_length()
   if nd<limit and w.index not in best and part[w.index]==part[i] and w.index not in maw:heapq.heappush(pq,(nd,w.index,s))
 return best
ss=lambda t:t*t*(3-2*t)
bw=spread(list(corrW),WW);bk=spread(list(corrK),WK);rep['weight_band_vertices']=len(bw);rep['shape_band_vertices']=len(bk)
for i,(d,s) in bw.items():
 v=bm.verts[i];w=wvec(v)+corrW[s]*(1-ss(min(1.,d/WW)));w=np.clip(w,0,None);top=np.argsort(-w)[:4];top=[t for t in top if w[t]>1e-4];tot=w[top].sum()
 dv=v[dl];dv.clear()
 for t in top:dv[int(t)]=float(w[t]/tot)
for i,(d,s) in bk.items():
 v=bm.verts[i];f=1-ss(min(1.,d/WK));base=v[SH['Basis']]
 for ki,kn in enumerate(KN[1:]):
  c=corrK[s][ki]
  if np.abs(c).max()>0:v[SH[kn]]=base+type(base)(kvec(v)[ki]+c*f)
band_orig={bm.verts[i][orig] for i in set(bw)|set(bk)}|{x[orig] for c in clus for x in c};tmap={x:c[0] for c in clus for x in c[1:]};corig=[[x[orig] for x in c] for c in clus]
bmesh.ops.weld_verts(bm,targetmap=tmap)
for v in bm.verts:v.co=v[SH['Basis']]
bm.normal_update();bm.to_mesh(me);bm.free();me.update()
# index map from the carried original index; welded-away vertices map to their survivor, deleted flap vertices to -1
neworig=np.empty(len(me.vertices),dtype=np.int64);me.attributes['v09_orig'].data.foreach_get('value',neworig);me.attributes.remove(me.attributes['v09_orig'])
assert np.all(np.diff(neworig)>0),'surviving vertices must keep their original order'
o2n=-np.ones(N0,dtype=np.int64);o2n[neworig]=np.arange(len(neworig))
for c in corig:
 for x in c[1:]:o2n[x]=o2n[c[0]]
assert all(o2n[i]==i-(N0-len(me.vertices)) for i in range(40603,N0)),'old maw/teeth vertices must survive untouched (shifted only)'
(O/'repair-index-map.json').write_text(json.dumps({'source':str(SRC.relative_to(R)),'old_to_new':o2n.tolist(),'new_vertices':0,'welded_away':int(sum(len(c)-1 for c in corig)),
 'deleted':int((o2n<0).sum()),'note':'no vertices are appended; welded-away vertices map to their surviving partner; -1 = deleted seam flap vertex'}))
rep['vertices_before']=N0;rep['vertices_after']=len(me.vertices);me.calc_loop_triangles();rep['triangles_after']=len(me.loop_triangles)
# verification against the input, by index map
K1,W1=snapshot(me);surv=np.flatnonzero(o2n>=0);surv=surv[np.isin(surv,neworig)];nn=o2n[surv]
wchg=np.abs(W1[nn]-W0[surv]).max(1)>1e-6;kchg=np.max([np.linalg.norm(K1[k][nn]-K0[k][surv],axis=1) for k in KN],axis=0)>1e-7
inf=(W1>1e-6).sum(1);rep['verify']={'survivors':int(len(surv)),'weights_changed':int(wchg.sum()),'shape_offsets_changed':int(kchg.sum()),
 'changed_outside_seam_bands':int(sum(1 for i,c in zip(surv,wchg|kchg) if c and i not in band_orig and i not in pinch_orig)),
 'max_influences':int(inf.max()),'unweighted_vertices':int((inf==0).sum()),'max_weight_sum_error':float(np.abs(W1.sum(1)-1)[inf>0].max()),
 'hood_open_delta_max_change_m':float(np.linalg.norm((K1['HOOD_OPEN'][nn]-K1['Basis'][nn])-(K0['HOOD_OPEN'][surv]-K0['Basis'][surv]),axis=1).max()),
 'basis_max_change_m':float(np.linalg.norm(K1['Basis'][nn]-K0['Basis'][surv],axis=1).max()),
 'per_key_max_offset_change_m':{k:float(np.linalg.norm((K1[k][nn]-K1['Basis'][nn])-(K0[k][surv]-K0['Basis'][surv]),axis=1).max()) for k in KN[1:]},
 'old_maw_vertices_identical':bool(all(np.allclose(K1[k][o2n[40603:N0]],K0[k][40603:N0]) for k in KN) and np.allclose(W1[o2n[40603:N0]],W0[40603:N0]))}
bpy.ops.wm.save_as_mainfile(filepath=str(O/'keeper-v09-repaired.blend'))
after=short(AK.audit(o,SEAM))
left={'hood_split':'1475-edge loop plus 4 small end slits at x~-0.04, z 1.96-2.02: the intentional 1 mm Revision02 split (HOOD_OPEN parts it); not closed',
 'old_maw':'24-edge rim of the old slot-2 mouth bowl plus the old disc/teeth loose parts: left untouched for the V09 maw replacement'}
(O/'repair-audit.json').write_text(json.dumps({'before':before,'after':after,'repair':rep,'deliberately_open':left,
 'uv_and_texture':'no vertices or faces added (welds, flap and tetrahedron deletions only); existing UV islands and T_KeeperTripo_* maps unchanged (no patch texels needed)'},indent=1))
print('RESULT '+json.dumps(rep))
