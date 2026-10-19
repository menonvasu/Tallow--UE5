import bpy,json,numpy as np
from pathlib import Path
p=Path('/home/tyron/Projects/tallow-workstreams/keeper/Tallow/SourceAssets/Keeper/SleevedV06')
bpy.ops.wm.open_mainfile(filepath=str(p/'keeper-tripo-clean-uv.blend'))
o=bpy.data.objects['SK_Keeper_Tripo_Direct'];m=o.data
for mat in m.materials:
 print('NODES',[(n.name,n.type,n.image.name if n.type=='TEX_IMAGE' else '') for n in mat.node_tree.nodes])
bs=next(n for n in m.materials[0].node_tree.nodes if n.type=='BSDF_PRINCIPLED');im=bs.inputs['Base Color'].links[0].from_node.image
px=np.array(im.pixels[:]).reshape(im.size[1],im.size[0],4);col=np.zeros((len(m.vertices),3));ct=np.zeros(len(m.vertices))
for loop,uv in zip(m.loops,m.uv_layers['TripoSourceUV'].data):
 x=int(uv.uv.x*im.size[0])%im.size[0];y=int(uv.uv.y*im.size[1])%im.size[1];col[loop.vertex_index]+=px[y,x,:3];ct[loop.vertex_index]+=1
col/=np.maximum(ct[:,None],1)
co=np.array([v.co[:] for v in m.vertices]);np.savez(p/'semantic-source.npz',positions=co,colors=col)
for x,z in [(.15,1.95),(.32,1.63),(.52,1.42),(.14,1.68),(.24,1.36),(.35,1.10),(.125,1.34),(.18,1.10),(.17,.85)]:
 sel=(abs(co[:,0]-x)<.03)&(abs(co[:,2]-z)<.03);print('LANDMARK',x,z,co[sel].min(0).tolist(),co[sel].max(0).tolist())
print('COLOR',col.min(0),col.max(0),np.percentile(col.mean(1),[10,25,50,75,90]))

uv=np.array([x.uv[:] for x in m.uv_layers['ProductionUV'].data]);np.save(p/'uv-triangles.npy',np.array([uv[list(f.loop_indices)] for f in m.polygons]))
