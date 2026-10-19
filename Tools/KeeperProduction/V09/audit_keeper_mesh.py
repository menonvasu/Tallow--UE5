"""Reusable Keeper mesh audit: open boundary loops, loose parts, non-manifold edges, zero-area UV faces, counts, seam split, HOOD_OPEN.
blender -b <file.blend> --factory-startup --python audit_keeper_mesh.py -- [--object NAME] [--json out.json]"""
import bpy,bmesh,json,sys,numpy as np
from pathlib import Path
from collections import defaultdict
R=Path(__file__).resolve().parents[3]
def audit(o,seam_groups=None):
 me=o.data;bm=bmesh.new();bm.from_mesh(me);bm.verts.ensure_lookup_table();bm.edges.ensure_lookup_table();bm.faces.ensure_lookup_table()
 uv=bm.loops.layers.uv.get('ProductionUV') or bm.loops.layers.uv.active
 bnd=[e for e in bm.edges if e.is_boundary];nonman=[e for e in bm.edges if len(e.link_faces)>2 or len(e.link_faces)==0]
 # boundary loops: walk boundary edges by shared boundary vertices
 seen=set();loops=[]
 adj=defaultdict(list)
 for e in bnd:
  for v in e.verts:adj[v.index].append(e.index)
 for e in bnd:
  if e.index in seen:continue
  stack=[e.index];comp=[]
  while stack:
   i=stack.pop()
   if i in seen:continue
   seen.add(i);comp.append(i);ed=bm.edges[i]
   for v in ed.verts:
    for j in adj[v.index]:
     if j not in seen:stack.append(j)
  vs={v.index for i in comp for v in bm.edges[i].verts};co=np.array([bm.verts[i].co[:] for i in vs])
  mats={f.material_index for i in comp for f in bm.edges[i].link_faces}
  loops.append({'edges':len(comp),'verts':len(vs),'centroid':co.mean(0).round(3).tolist(),'bbox_min':co.min(0).round(3).tolist(),'bbox_max':co.max(0).round(3).tolist(),'materials':sorted(mats),'vids':sorted(vs)})
 loops.sort(key=lambda l:-l['edges'])
 # loose parts
 seenv=np.zeros(len(bm.verts),bool);parts=[]
 for v in bm.verts:
  if seenv[v.index]:continue
  stack=[v];seenv[v.index]=True;comp=[]
  while stack:
   x=stack.pop();comp.append(x.index)
   for e in x.link_edges:
    w=e.other_vert(x)
    if not seenv[w.index]:seenv[w.index]=True;stack.append(w)
  co=np.array([bm.verts[i].co[:] for i in comp]);mats={f.material_index for i in comp for f in bm.verts[i].link_faces}
  parts.append({'verts':len(comp),'centroid':co.mean(0).round(3).tolist(),'bbox_min':co.min(0).round(3).tolist(),'bbox_max':co.max(0).round(3).tolist(),'materials':sorted(mats),'first_vid':min(comp)})
 parts.sort(key=lambda p:-p['verts'])
 zuv=[]
 for f in bm.faces:
  if uv is None:break
  p=[l[uv].uv for l in f.loops];a=0.
  for k in range(1,len(p)-1):a+=abs((p[k]-p[0]).cross(p[k+1]-p[0]))/2
  if a<1e-10:zuv.append(f.index)
 zero3d=sum(1 for f in bm.faces if f.calc_area()<1e-12)
 tris=sum(len(f.verts)-2 for f in bm.faces)
 r={'object':o.name,'vertices':len(bm.verts),'faces':len(bm.faces),'triangles':tris,'materials':[m.name if m else None for m in me.materials],
  'shape_keys':[k.name for k in me.shape_keys.key_blocks] if me.shape_keys else [],
  'open_boundary_loops':len(loops),'boundary_edges':len(bnd),'boundary_loop_sizes':[l['edges'] for l in loops],
  'loose_parts':len(parts),'loose_part_sizes':[p['verts'] for p in parts],'non_manifold_edges':len(nonman),'non_manifold_edge_ids':[e.index for e in nonman],
  'zero_area_uv_faces':len(zuv),'zero_area_uv_face_ids':zuv,'zero_area_3d_faces':zero3d,'loops':loops,'parts':parts}
 if me.shape_keys and 'HOOD_OPEN' in me.shape_keys.key_blocks:
  ks=me.shape_keys.key_blocks;n=len(me.vertices);B=np.empty(n*3);H=np.empty(n*3);ks['Basis'].data.foreach_get('co',B);ks['HOOD_OPEN'].data.foreach_get('co',H);B=B.reshape(-1,3);H=H.reshape(-1,3)
  co=np.empty(n*3);me.vertices.foreach_get('co',co);co=co.reshape(-1,3)
  r['mesh_vs_basis_max_m']=float(np.abs(co-B).max());r['hood_open_moving_vertices']=int((np.linalg.norm(H-B,axis=1)>1e-5).sum());r['hood_open_max_delta_m']=float(np.linalg.norm(H-B,axis=1).max())
 if seam_groups is not None:
  # split check by position: coincident-seam groups are identified by their Basis coordinates in Revision02
  ks=me.shape_keys.key_blocks;n=len(me.vertices);B=np.empty(n*3);ks['Basis'].data.foreach_get('co',B);B=B.reshape(-1,3)
  from mathutils.kdtree import KDTree
  kd=KDTree(n)
  for i,c in enumerate(B):kd.insert(c,i)
  kd.balance();split=0;welded=0;gaps=[]
  for g in seam_groups:
   c=np.array(g['co']);hits=kd.find_range(c,.0011)
   xs=sorted(B[h[1]][0] for h in hits)
   if len(hits)>=2 and xs[-1]-xs[0]>.0009:split+=1;gaps.append(xs[-1]-xs[0])
   else:welded+=1
  r['seam_groups']=len(seam_groups);r['seam_groups_split']=split;r['seam_groups_welded_or_missing']=welded;r['seam_min_gap_m']=float(min(gaps)) if gaps else None
 bm.free();return r
if any(a.endswith('audit_keeper_mesh.py') for a in sys.argv):  # CLI only when run as the --python script
 a=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
 name=a[a.index('--object')+1] if '--object' in a else None;out=a[a.index('--json')+1] if '--json' in a else None
 o=bpy.data.objects[name] if name else next(o for o in bpy.data.objects if o.type=='MESH' and o.data.shape_keys)
 sg=json.loads((R/'Tallow/SourceAssets/Keeper/V07RevealRevision01/coincident-seam-audit.json').read_text())
 r=audit(o,sg)
 if out:Path(out).write_text(json.dumps(r,indent=1))
 s={k:v for k,v in r.items() if k not in ('loops','parts','non_manifold_edge_ids','zero_area_uv_face_ids')}
 s['loops_top']=[{k:v for k,v in l.items() if k!='vids'} for l in r['loops'][:40]];s['parts_top']=r['parts'][:40]
 print('AUDIT '+json.dumps(s))
