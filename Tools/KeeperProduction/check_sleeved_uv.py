"""Triangle area and conservative raster overlap audit of fresh ProductionUV."""
import bpy,numpy as np,json
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06';bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-tripo-clean-uv.blend'));m=bpy.data.objects['SK_Keeper_Tripo_Direct'].data
uv=np.array([u.uv[:] for u in m.uv_layers['ProductionUV'].data]);tri=np.array([uv[list(p.loop_indices)] for p in m.polygons]);aa=tri[:,1]-tri[:,0];bb=tri[:,2]-tri[:,0];area=np.abs(aa[:,0]*bb[:,1]-aa[:,1]*bb[:,0])*.5
np.save(OUT/'uv-triangles.npy',tri)
assert np.isfinite(uv).all() and np.count_nonzero(area<1e-14)==0
# Pixel centres strictly inside two triangles reveal overlap without counting shared edges.
res=2048;grid=np.zeros((res,res),dtype=np.uint16);owner=np.full((res,res),-1,dtype=np.int32);bad=set()
for face_id,t in enumerate(tri):
 low=np.maximum(0,np.floor(t.min(0)*res-.5).astype(int));high=np.minimum(res-1,np.ceil(t.max(0)*res-.5).astype(int))
 if np.any(high<low):continue
 xs=(np.arange(low[0],high[0]+1)+.5)/res;ys=(np.arange(low[1],high[1]+1)+.5)/res;x,y=np.meshgrid(xs,ys);pts=np.stack((x,y),axis=-1)
 a,b,c=t;v0=b-a;v1=c-a;v2=pts-a;den=v0[0]*v1[1]-v1[0]*v0[1]
 u=(v2[...,0]*v1[1]-v1[0]*v2[...,1])/den;v=(v0[0]*v2[...,1]-v2[...,0]*v0[1])/den
 mask=(u>1e-5)&(v>1e-5)&(u+v<1-1e-5);view=grid[low[1]:high[1]+1,low[0]:high[0]+1];owners=owner[low[1]:high[1]+1,low[0]:high[0]+1];over=mask&(view>0)
 if over.any():bad.add(face_id);bad.update(int(i) for i in owners[over] if i>=0)
 view+=mask;owners[mask]=face_id
report=dict(method='fresh 55 degree projection and independent normalized pack',triangles=len(tri),collapsed_triangles=int((area<1e-14).sum()),minimum_triangle_area=float(area.min()),coverage_fraction=float((grid>0).mean()),overlapping_pixels=int((grid>1).sum()),raster_resolution=res,overlap_check_limit='subpixel overlaps below 2048px raster resolution are not detected')
(OUT/'uv-overlap-faces.json').write_text(json.dumps(sorted(bad)))
(OUT/'uv-validation.json').write_text(json.dumps(report,indent=2));assert report['overlapping_pixels']==0,report;print('UV_VALIDATION '+json.dumps(report))
