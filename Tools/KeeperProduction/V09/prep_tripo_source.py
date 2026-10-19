"""V09 mouth, stage 1: Tripo C3 geometry -> Keeper mouth frame, tooth survey, teeth-free bake/projection sources.
Tripo supplies raw sculpted geometry only (texture:false, pbr:false). Outputs V09Mouth/mouth-v09-source.blend + tooth-survey.json.
Frame: rest-pose Keeper mesh space; the maw opens toward -Y, lip centre LIP, throat bent down toward the neck.
Teeth are fused into Tripo's surface, so a morphological SDF opening (erode 4.5 mm, dilate 4.5 mm) gives the gums without them;
High vertices standing >2 mm proud of that surface are teeth, split into individual teeth by connectivity."""
import bpy,bmesh,json,math,time,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth'
GLB=next((O/'TripoRaw/C3_text_disc/tripo-out').glob('*/model.glb'))
CX=-.04;LIP=Vector((CX,-.085,2.250));RIM=.128;BEND=.75;OPEN_R=.0045;VOX=.0015;PROUD=.002;t0=time.time()
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(GLB))
ms=[o for o in bpy.data.objects if o.type=='MESH'];bm=bmesh.new()
for o in ms:me=o.data.copy();me.transform(o.matrix_world);bm.from_mesh(me);bpy.data.meshes.remove(me)
for o in list(bpy.data.objects):bpy.data.objects.remove(o)
bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5);bmesh.ops.triangulate(bm,faces=[f for f in bm.faces if len(f.verts)>3])
# Cavity axis: centroid of first wall hits of radial rays from a provisional +X axis, refined three times, then a line fit.
tree=BVHTree.FromBMesh(bm);cy=cz=0.;pts=[]
for it in range(3):
 pts=[]
 for x in np.linspace(-.12,.22,9):
  hits=[h[0] for a in np.linspace(0,math.tau,48,endpoint=False) for h in [tree.ray_cast(Vector((x,cy,cz)),Vector((0,math.cos(a),math.sin(a))),.6)] if h[0]]
  if len(hits)>40:c=sum(hits,Vector())/len(hits);pts.append((x,c.y,c.z))
 P=np.array(pts);cy,cz=float(P[:,1].mean()),float(P[:,2].mean())
P=np.array(pts);ky=np.polyfit(P[:,0],P[:,1],1);kz=np.polyfit(P[:,0],P[:,2],1)
axis=Vector((1,ky[0],kz[0])).normalized();a0=Vector((0,ky[1],kz[1]))
co=np.array([v.co[:] for v in bm.verts]);rel=co-np.array(a0);ax=np.array(axis);along=rel@ax;radial=np.linalg.norm(rel-np.outer(along,ax),axis=1)
lipx=float(np.percentile(along[radial<.5],99.0));rim_r=float(np.percentile(radial[(along>lipx-.08)&(along<lipx)],97))  # outer lip of the disc
end=tree.ray_cast(a0+axis*.05,-axis,2.)[0];depth_units=lipx-(end-a0).dot(axis);s=RIM/rim_r
# Tripo -> Keeper: mouth axis -> -Y tilted 8 deg up (capture pose pitches the head 34 deg down), uniform scale, 7 deg twist of the oval; then egg/V taper, crown squash, chin recession, throat bend.
rot=axis.to_track_quat('X','Z').to_matrix().to_4x4()
M=Matrix.Translation(LIP)@Matrix.Rotation(math.radians(-8),4,'X')@Matrix.Rotation(math.radians(-90),4,'Z')@Matrix.Rotation(math.radians(7),4,'X')@Matrix.Diagonal((s,s,s,1))@rot.inverted()@Matrix.Translation(-(a0+axis*lipx))
bm.transform(M)
for v in bm.verts:
 d=v.co.y-LIP.y;u=(v.co.z-LIP.z)/RIM
 v.co.x=CX+(v.co.x-CX)*(.97-.22*max(0.,min(1.,-u)))*(1+.04*u)  # egg: narrower toward the bottom of the hood's V
 rc=2.6*(v.co.z-LIP.z)**2 if u<0 else 0.  # lower lip recedes behind the hood's chin
 if u>0:v.co.z=LIP.z+(v.co.z-LIP.z)*.93  # stay under the hood crown
 if d>0:t=min(1.,max(0.,(d-.02)/.07));d2=d+.55*(d-.02)*t*t*(3-2*t) if d>.02 else d;v.co.y=LIP.y+d2;v.co.z-=BEND*d2*d2;v.co.x+=.08*d2*d2  # deeper throat, sinking toward the neck
 v.co.y+=rc
bm.normal_update()
def obj(name,me):
 for p in me.polygons:p.use_smooth=True
 o=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(o);return o
me=bpy.data.meshes.new('TRIPO_C3_High');bm.to_mesh(me);bm.free();hi=obj('TRIPO_C3_High',me)
# Morphological opening on an SDF grid (Geometry Nodes): removes thin protrusions (teeth), keeps gum rings, lips, throat ribs.
ng=bpy.data.node_groups.new('V09_SDFOpen','GeometryNodeTree');ng.interface.new_socket('Geometry',in_out='INPUT',socket_type='NodeSocketGeometry');ng.interface.new_socket('Geometry',in_out='OUTPUT',socket_type='NodeSocketGeometry')
N=ng.nodes;gi=N.new('NodeGroupInput');go=N.new('NodeGroupOutput');m2s=N.new('GeometryNodeMeshToSDFGrid');e=N.new('GeometryNodeSDFGridOffset');d_=N.new('GeometryNodeSDFGridOffset');g2m=N.new('GeometryNodeGridToMesh')
m2s.inputs['Voxel Size'].default_value=VOX;m2s.inputs['Band Width'].default_value=int(OPEN_R/VOX)+5;e.inputs['Distance'].default_value=-OPEN_R;d_.inputs['Distance'].default_value=OPEN_R;g2m.inputs['Threshold'].default_value=0
for x,y in [(gi.outputs[0],m2s.inputs['Mesh']),(m2s.outputs[0],e.inputs[0]),(e.outputs[0],d_.inputs[0]),(d_.outputs[0],g2m.inputs[0]),(g2m.outputs[0],go.inputs[0])]:ng.links.new(x,y)
tmp=bpy.data.objects.new('tmp',me);bpy.context.scene.collection.objects.link(tmp);tmp.modifiers.new('open','NODES').node_group=ng
dg=bpy.context.evaluated_depsgraph_get();om=bpy.data.meshes.new_from_object(tmp.evaluated_get(dg));om.name='TRIPO_C3_Opened';bpy.data.objects.remove(tmp);bpy.data.node_groups.remove(ng)
opened=obj('TRIPO_C3_Opened',om);bmo=bmesh.new();bmo.from_mesh(om);ot=BVHTree.FromBMesh(bmo);bmo.free()
n=len(me.vertices);V=np.empty(n*3);me.vertices.foreach_get('co',V);V=V.reshape(-1,3);sd=np.empty(n)
for i,v in enumerate(V):
 p,nn,_,dd=ot.find_nearest(Vector(v));sd[i]=dd if (Vector(v)-p).dot(nn)>0 else -dd
mask=sd>PROUD
E=np.empty(len(me.edges)*2,dtype=np.int64);me.edges.foreach_get('vertices',E);E=E.reshape(-1,2);k=mask[E[:,0]]&mask[E[:,1]];a,b=E[k,0],E[k,1];lab=np.arange(n)
while True:
 m=np.minimum(lab[a],lab[b]);old=lab.copy();np.minimum.at(lab,a,m);np.minimum.at(lab,b,m);lab=lab[lab];lab=lab[lab]
 if np.array_equal(old,lab):break
ids=np.flatnonzero(mask);ids=ids[np.argsort(lab[ids],kind='stable')];comps=np.split(ids,np.flatnonzero(np.diff(lab[ids]))+1)
teeth=[];rejected=0
for c in comps:
 if len(c)<40:rejected+=1;continue
 P=V[c];mu=P.mean(0);X=P-mu;w_,U=np.linalg.eigh(X.T@X/len(c));ax_=U[:,2];h=X@ax_
 if w_[2]<3*w_[1]:rejected+=1;continue  # round lumps are flesh, not teeth
 if sd[c[np.argmax(h)]]<sd[c[np.argmin(h)]]:ax_=-ax_;h=-h  # tip is the end standing furthest proud
 lo,hi_=h.min(),h.max();L=float(hi_-lo)
 if L<.006:rejected+=1;continue
 t=(h-lo)/L;spine=[];radii=[]
 for q0,q1 in [(0,.15),(.2,.4),(.45,.65),(.7,.88)]:
  sel=P[(t>=q0)&(t<q1)]
  if len(sel)<3:continue
  cc=sel.mean(0);rr=sel-cc;rr-=np.outer(rr@ax_,ax_);spine.append(cc.tolist());radii.append(float(np.median(np.linalg.norm(rr,axis=1))))
 if len(spine)<3:rejected+=1;continue
 tip=P[np.argmax(h)];base=np.array(spine[0]);hit=ot.find_nearest(Vector(base))
 teeth.append({'base':base.tolist(),'tip':tip.tolist(),'length':L,'base_radius':radii[0],'spine':spine+[tip.tolist()],'radii':radii,'verts':int(len(c)),
  'gum_point':list(hit[0]),'gum_normal':list(hit[1]),'depth_m':float(base[1]-LIP.y),'radial_m':float(math.hypot(base[0]-CX,base[2]-LIP.z))})
# Bake source: High minus every tooth vertex (one ring dilated); holes are backed by the opened surface during the bake.
dil=mask.copy();dil[E[mask[E[:,0]],1]]=True;dil[E[mask[E[:,1]],0]]=True
bmf=bmesh.new();bmf.from_mesh(me);bmf.verts.ensure_lookup_table();bmesh.ops.delete(bmf,geom=[bmf.verts[i] for i in np.flatnonzero(dil)],context='VERTS')
fm=bpy.data.meshes.new('TRIPO_C3_FleshHigh');bmf.to_mesh(fm);bmf.free();obj('TRIPO_C3_FleshHigh',fm)
bmt=bmesh.new();bmt.from_mesh(me);bmt.verts.ensure_lookup_table();bmesh.ops.delete(bmt,geom=[bmt.verts[i] for i in np.flatnonzero(~mask)],context='VERTS')
tm=bpy.data.meshes.new('TRIPO_C3_TeethRef');bmt.to_mesh(tm);bmt.free();obj('TRIPO_C3_TeethRef',tm)
bpy.ops.wm.save_as_mainfile(filepath=str(O/'mouth-v09-source.blend'))
survey={'source_glb':str(GLB.relative_to(R)),'tripo_task':'89543399-fda8-4b7b-957f-25fb2181984a','scale_m_per_unit':s,'tripo_axis':list(axis),'tripo_rim_radius_units':rim_r,'tripo_cavity_depth_units':depth_units,
 'lip_centre':list(LIP),'rim_radius_m':RIM,'throat_depth_stretch':1.55,'throat_bend':BEND,'sdf_opening_radius_m':OPEN_R,'sdf_voxel_m':VOX,'tooth_proud_threshold_m':PROUD,'high_vertices':n,'high_triangles':len(me.polygons),
 'opened_vertices':len(om.vertices),'flesh_high_triangles':len(fm.polygons),'tooth_vertices':int(mask.sum()),'components_rejected':rejected,'teeth_found':len(teeth),'teeth':teeth,'seconds':time.time()-t0}
(O/'tooth-survey.json').write_text(json.dumps(survey,indent=1))
print('RESULT '+json.dumps({k:v for k,v in survey.items() if k!='teeth'}))
