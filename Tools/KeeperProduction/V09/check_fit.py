"""V09 maw vs hood clearance: BVH face-overlap between the maw (closed->open, with/without CHOMP) and the Keeper head shell at the same
HOOD_OPEN value. Usage: blender -b mouth-v09-low.blend --python check_fit.py -- [keeper.blend] [out.json]"""
import bpy,sys,json,numpy as np
from pathlib import Path
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
sys.path.append(str(Path(__file__).parent));import mouth_rig as MR
R=Path(__file__).resolve().parents[3];args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
KB=Path(args[0]) if args else R/'Tallow/SourceAssets/Keeper/V07RevealRevision02/keeper-reveal-revision02-export.blend'
OUT=Path(args[1]) if len(args)>1 else None
def arr(me,name,k,dom='value'):
 a=np.empty(len(me.vertices)*k,dtype=np.float32);me.attributes[name].data.foreach_get(dom,a);return a.reshape(-1,k) if k>1 else a
def co(me):a=np.empty(len(me.vertices)*3);me.vertices.foreach_get('co',a);return a.reshape(-1,3)
fl=bpy.data.objects['MOUTH_Flesh'].data;tm=bpy.data.objects['MOUTH_Teeth'].data;sm=bpy.data.objects['MOUTH_Strands'].data
Pf,Pt=co(fl),co(tm);ff=[tuple(p.vertices) for p in fl.polygons];tf=[tuple(p.vertices) for p in tm.polygons]
axf=arr(fl,'axis_pt',3,'vector');kd=KDTree(len(Pf))
for i,p in enumerate(Pf):kd.insert(p,i)
kd.balance();tb=arr(tm,'tooth_base',3,'vector');axt=np.array([axf[kd.find(b)[1]] for b in tb])
Df=MR.flesh_chomp(Pf,axf,arr(fl,'chomp_w',1),arr(fl,'mouth_part',1),arr(fl,'mouth_depth',1))
Dt=MR.teeth_chomp(Pt,tb,arr(tm,'tooth_dir',3,'vector'),arr(tm,'tooth_ring',1),axt)
Ps=co(sm);sf=[tuple(p.vertices) for p in sm.polygons];sa_,sb_,st_=arr(sm,'strand_a',1).astype(int),arr(sm,'strand_b',1).astype(int),arr(sm,'strand_t',1)[:,None];Ds=(1-st_)*Df[sa_]+st_*Df[sb_]
with bpy.data.libraries.load(str(KB)) as (src,dst):dst.meshes=[n for n in src.meshes if 'Seamed' in n or 'Repaired' in n or 'Keeper' in n][:1]
km=dst.meshes[0];kb=km.shape_keys.key_blocks
def kco(n):a=np.empty(len(km.vertices)*3);kb[n].data.foreach_get('co',a);return a.reshape(-1,3)
B,H=kco('Basis'),kco('HOOD_OPEN')
body=[tuple(p.vertices) for p in km.polygons if p.material_index==0 and B[list(p.vertices)][:,2].min()>1.85]
rep=[]
for chomp in [0.,1.]:
 for t in np.linspace(0,1,11):
  bt=BVHTree.FromPolygons((B+t*(H-B)).tolist(),body)
  for name,P,F,D in [('flesh',Pf,ff,Df),('teeth',Pt,tf,Dt),('strands',Ps,sf,Ds)]:
   C=MR.closed(P);M=C+t*(P-C)+chomp*D
   pairs=bt.overlap(BVHTree.FromPolygons(M.tolist(),F))
   rep.append({'hood_open':round(float(t),2),'chomp':chomp,'part':name,'overlap_pairs':len(pairs),'mouth_faces_hit':len({b for a,b in pairs})})
print('RESULT '+json.dumps([r for r in rep if r['overlap_pairs']]))
if OUT:OUT.write_text(json.dumps(rep,indent=1))
