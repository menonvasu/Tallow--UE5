"""V09 mouth, stage 5: replace the V07 disc/peg maw in the Keeper game mesh with the V09 maw, author its morphs, export, verify.
Basis (and mesh.vertices) = maw squeezed shut inside the closed hood; HOOD_OPEN = designed open maw; CHOMP = clench (mouth_rig.py);
FULL_HORROR/Blink_L/Blink_R leave the maw at Basis. Maw vertices ride 'head' at weight 1, like the V07 maw. Slots stay
[M_KeeperTripo_Production, M_KeeperV07_Teeth, M_KeeperV07_Mouth]; skeleton, other vertices, weights, UVs and shape offsets untouched.
FBX settings are Revision02's; the export is re-imported and verified by vertex index.
Usage: blender -b --python fit_keeper.py -- [keeper.blend]  (default: V09Mouth/keeper-v09-repaired.blend, else Revision02 export)"""
import bpy,bmesh,sys,json,hashlib,numpy as np
from pathlib import Path
from mathutils.kdtree import KDTree
from mathutils.bvhtree import BVHTree
sys.path.append(str(Path(__file__).parent));import mouth_rig as MR
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';(O/'Export').mkdir(exist_ok=True)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
SRC=Path(args[0]).resolve() if args else (O/'keeper-v09-repaired.blend' if (O/'keeper-v09-repaired.blend').exists() else R/'Tallow/SourceAssets/Keeper/V07RevealRevision02/keeper-reveal-revision02-export.blend')
bpy.ops.wm.open_mainfile(filepath=str(SRC))
r=next(x for x in bpy.data.objects if x.type=='ARMATURE');o=next(x for x in bpy.data.objects if x.type=='MESH' and x.data.shape_keys and 'HOOD_OPEN' in x.data.shape_keys.key_blocks)
assert len(r.data.bones)==109 and [m.name for m in o.data.materials]==['M_KeeperTripo_Production','M_KeeperV07_Teeth','M_KeeperV07_Mouth']
KEYS=[k.name for k in o.data.shape_keys.key_blocks];GROUPS=[g.name for g in o.vertex_groups]
def keyarr(me,name):a=np.empty(len(me.vertices)*3);me.shape_keys.key_blocks[name].data.foreach_get('co',a);return a.reshape(-1,3)
# 1) remove the V07 maw (every vertex used only by slot 1/2 faces)
me=o.data;mi=np.empty(len(me.polygons),dtype=np.int32);me.polygons.foreach_get('material_index',mi)
old=sorted({v for p in me.polygons if p.material_index>0 for v in p.vertices});body_used={v for p in me.polygons if p.material_index==0 for v in p.vertices}
assert not (set(old)&body_used),'old maw shares vertices with the body'
before={k:keyarr(me,k) for k in KEYS};keep=np.setdiff1d(np.arange(len(me.vertices)),old)
bm=bmesh.new();bm.from_mesh(me);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[bm.verts[i] for i in old],context='VERTS');bm.to_mesh(me);bm.free()
nb=len(me.vertices);assert nb==len(keep)
for k in KEYS:assert np.allclose(keyarr(me,k),before[k][keep],atol=1e-7),k
# 2) bring in the V09 maw (materials keep their exact names: retire the V07 ones first)
for m in o.data.materials[1:]:m.name=m.name+'_V07'
with bpy.data.libraries.load(str(O/'mouth-v09-low.blend')) as (src,dst):dst.objects=['MOUTH_Flesh','MOUTH_Teeth','MOUTH_Strands']
fl,tm,st=dst.objects;sc=bpy.context.scene
for x in (fl,tm,st):sc.collection.objects.link(x)
assert fl.data.materials[0].name=='M_KeeperV07_Mouth' and tm.data.materials[0].name=='M_KeeperV07_Teeth' and st.data.materials[0].name=='M_KeeperV07_Mouth'
def co(m):a=np.empty(len(m.vertices)*3);m.vertices.foreach_get('co',a);return a.reshape(-1,3)
def at(m,n,k=1):a=np.empty(len(m.vertices)*k,dtype=np.float32);m.attributes[n].data.foreach_get('value' if k==1 else 'vector',a);return a.reshape(-1,k) if k>1 else a
Pf,Pt,Ps=co(fl.data),co(tm.data),co(st.data);axf=at(fl.data,'axis_pt',3);kd=KDTree(len(Pf))
for i,p in enumerate(Pf):kd.insert(p,i)
kd.balance();tb=at(tm.data,'tooth_base',3);axt=np.array([axf[kd.find(b)[1]] for b in tb])
CHf=MR.flesh_chomp(Pf,axf,at(fl.data,'chomp_w'),at(fl.data,'mouth_part'),at(fl.data,'mouth_depth'))
CHt=MR.teeth_chomp(Pt,tb,at(tm.data,'tooth_dir',3),at(tm.data,'tooth_ring'),axt)
sa,sb,stt=at(st.data,'strand_a').astype(int),at(st.data,'strand_b').astype(int),at(st.data,'strand_t')[:,None]
CHs=(1-stt)*CHf[sa]+stt*CHf[sb]  # strands ride their two gum anchors
OPEN={1:Pf,2:Pt,3:Ps};CHOMPD={1:CHf,2:CHt,3:CHs}
for src_id,x in ((1,fl),(2,tm),(3,st)):
 for a in [a.name for a in x.data.attributes if a.name.startswith(('mouth_','tooth_','chomp_','axis_','uv_','strand_'))]:x.data.attributes.remove(x.data.attributes[a])
 n=len(x.data.vertices);x.data.attributes.new('v09_src','FLOAT','POINT').data.foreach_set('value',np.full(n,src_id,np.float32));x.data.attributes.new('v09_idx','FLOAT','POINT').data.foreach_set('value',np.arange(n,dtype=np.float32))
 x.data.vertices.foreach_set('co',(MR.closed(co(x.data))).ravel())  # Basis = closed
bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
for x in (fl,tm,st):x.select_set(True)
bpy.context.view_layer.objects.active=o;bpy.ops.object.join()
me=o.data;nm=len(me.vertices)-nb;assert nm==len(Pf)+len(Pt)+len(Ps)
src_=at(me,'v09_src')[nb:].astype(int);idx_=at(me,'v09_idx')[nb:].astype(int)
OPEN=np.array([OPEN[a][b] for a,b in zip(src_,idx_)]);CH=np.array([CHOMPD[a][b] for a,b in zip(src_,idx_)]);CL=MR.closed(OPEN)
for a in ('v09_src','v09_idx'):me.attributes.remove(me.attributes[a])
# 3) slots: new teeth/mouth into slots 1/2, drop extras and the retired V07 materials
names=[m.name for m in me.materials];tid,mid=names.index('M_KeeperV07_Teeth'),names.index('M_KeeperV07_Mouth')
mi=np.empty(len(me.polygons),dtype=np.int32);me.polygons.foreach_get('material_index',mi);mi=np.select([mi==tid,mi==mid],[1,2],mi)
me.materials[1]=bpy.data.materials['M_KeeperV07_Teeth'];me.materials[2]=bpy.data.materials['M_KeeperV07_Mouth']
while len(me.materials)>3:me.materials.pop(index=len(me.materials)-1)
me.polygons.foreach_set('material_index',mi.astype(np.int32));me.update()
for m in [m for m in bpy.data.materials if m.name.endswith('_V07')]:bpy.data.materials.remove(m)
# 4) morphs and weights for the maw
ks=me.shape_keys.key_blocks;rng=slice(nb,nb+nm)
for k in ks:
 a=keyarr(me,k.name);a[rng]=OPEN if k.name=='HOOD_OPEN' else CL+CH if k.name=='CHOMP' else CL;k.data.foreach_set('co',a.ravel())
a=co(me);a[rng]=CL;me.vertices.foreach_set('co',a.ravel());me.update()
hg=o.vertex_groups['head'];hg.add(list(range(nb,nb+nm)),1.,'REPLACE')
for p in me.polygons:
 if p.material_index>0:p.use_smooth=True
assert [k.name for k in ks]==KEYS and [g.name for g in o.vertex_groups]==GROUPS and [m.name for m in me.materials]==['M_KeeperTripo_Production','M_KeeperV07_Teeth','M_KeeperV07_Mouth']
o.data.name='KeeperV09_Surface';bpy.ops.object.select_all(action='DESELECT')
# 5) export exactly like Revision02
r.select_set(True);o.select_set(True);bpy.context.view_layer.objects.active=r
if r.animation_data:r.animation_data.action=None
if me.shape_keys.animation_data:me.shape_keys.animation_data.action=None
r.data.pose_position='REST'
for k in ks:k.value=0
fbx=O/'Export/SK_KeeperMouthV09.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE',bake_anim=False)
bpy.ops.file.make_paths_relative()
bpy.ops.wm.save_as_mainfile(filepath=str(O/'keeper-v09-mouth-export.blend'))
# 6) verify the FBX by vertex index (fresh import keeps control-point order)
W=o.matrix_world.copy();SRCK={k.name:keyarr(me,k.name) for k in ks};B=SRCK['Basis'];h=B[:,2].max()-B[:,2].min()
nrm=np.array([len(v.groups) for v in me.vertices]);wsum=np.array([sum(g.weight for g in v.groups) for v in me.vertices])
tris=sum(len(p.vertices)-2 for p in me.polygons);mouth_tris=sum(len(p.vertices)-2 for p in me.polygons if p.material_index==2);teeth_tris=sum(len(p.vertices)-2 for p in me.polygons if p.material_index==1)
src_info={'vertices':len(me.vertices),'triangles':tris,'mouth_triangles':mouth_tris,'teeth_triangles':teeth_tris,'max_influences':int(nrm.max()),'unweighted':int((nrm==0).sum()),'max_weight_sum_dev':float(np.abs(wsum-1).max())}
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(fbx))
m=next(x for x in bpy.data.objects if x.type=='MESH');ar=next(x for x in bpy.data.objects if x.type=='ARMATURE');mk=m.data.shape_keys.key_blocks
M=np.array(m.matrix_world);imp={k.name:keyarr(m.data,k.name)@M[:3,:3].T+M[:3,3] for k in mk}
s=h/(imp['Basis'][:,2].max()-imp['Basis'][:,2].min());z0=B[:,2].min()-s*imp['Basis'][:,2].min()
pos=lambda X:X*s+np.array([0,0,z0])
assert len(m.data.vertices)==len(B),(len(m.data.vertices),len(B))
rep={'source_blend':str(SRC.relative_to(R)),'fbx':str(fbx.relative_to(R)),'fbx_sha256':hashlib.sha256(fbx.read_bytes()).hexdigest(),'source':src_info,
 'fbx_vertices':len(m.data.vertices),'fbx_bones':len(ar.data.bones),'fbx_morphs':[k.name for k in mk],'fbx_slots':[x.name for x in m.data.materials],
 'fbx_basis_max_err_m':float(np.linalg.norm(pos(imp['Basis'])-B,axis=1).max()),
 'fbx_morph_delta_max_err_m':{k:float(np.linalg.norm((imp[k]-imp['Basis'])*s-(SRCK[k]-B),axis=1).max()) for k in SRCK if k!='Basis'},
 'body_vertices_untouched':int(nb),'maw_vertices':int(nm)}
seam=json.loads((R/'Tallow/SourceAssets/Keeper/V07RevealRevision01/coincident-seam-audit.json').read_text())
mp=json.loads((O/'repair-index-map.json').read_text())['old_to_new'] if (O/'repair-index-map.json').exists() and 'repaired' in SRC.name else None
ib=pos(imp['Basis']);io=pos(imp['HOOD_OPEN']);gaps=[]
for g in seam:
 ids=[mp[i] if mp else i for i in g['ids']];ids=[i for i in ids if i>=0]
 sg={i:np.sign(io[i,0]-ib[i,0]) for i in ids};gaps.append(min((np.linalg.norm(ib[a]-ib[b]) for a in ids for b in ids if sg[a]!=sg[b]),default=np.inf))
rep['seam_groups']=len(seam);rep['min_cross_half_gap_in_fbx_m']=float(min(gaps))
assert rep['fbx_basis_max_err_m']<1e-5 and max(rep['fbx_morph_delta_max_err_m'].values())<1e-4 and rep['min_cross_half_gap_in_fbx_m']>.0005 and rep['fbx_bones']==109,rep
(O/'export-verification.json').write_text(json.dumps(rep,indent=2));print('RESULT '+json.dumps(rep))
