"""V09 mouth, stage 3: our own UVs ('ProductionUV', one shared 2048 atlas for flesh + teeth).
Seams: one meridian under the chin (least seen) + two ring cuts + 3 more meridians on the visible parts -> hidden outer wall | 4 collar/lips/disc sectors | 4 throat sectors; each tooth
cut once along its back. Texel density weights: lips/disc/teeth 1.0, throat 0.75 (falls into darkness), hidden outer wall 0.3."""
import bpy,bmesh,json,sys,numpy as np
from pathlib import Path
sys.path.append('/home/tyron/.claude/skills/scenario-blender-uv-baking/scripts');import bx_uvbake as U
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';RES=2048
bpy.ops.wm.open_mainfile(filepath=str(O/'mouth-v09-low.blend'))
fl=bpy.data.objects['MOUTH_Flesh'];tm=bpy.data.objects['MOUTH_Teeth'];st=bpy.data.objects['MOUTH_Strands'];cuts=list(fl['uv_cuts'])
def attr(me,n):a=np.empty(len(me.vertices));me.attributes[n].data.foreach_get('value',a);return a
for o in (fl,tm,st):
 while o.data.uv_layers:o.data.uv_layers.remove(o.data.uv_layers[0])
 o.data.uv_layers.new(name='ProductionUV')
ring,mer,part=attr(fl.data,'uv_ring'),attr(fl.data,'uv_mer'),attr(fl.data,'mouth_part')
for e in fl.data.edges:
 a,b=e.vertices;pole=ring.max();sec=(mer[a]%18==0 and part[a]>=1 and part[b]>=1 and ring[a]>=cuts[0] and ring[b]>=cuts[0]) or (pole in (ring[a],ring[b]) and min(ring[a],ring[b])!=pole and mer[a if ring[b]==pole else b]%18==0)  # 4 sectors on collar/lips/disc and throat (through the throat-end pole)
 e.use_seam=bool(((mer[a]==0 or sec) and (mer[a]==mer[b] or pole in (ring[a],ring[b])) and ring[a]!=ring[b]) or (ring[a]==ring[b] and ring[a] in cuts and mer[a]!=mer[b]))
tk,tid=attr(tm.data,'tooth_k'),attr(tm.data,'tooth_id')
for e in tm.data.edges:
 a,b=e.vertices;e.use_seam=bool(tk[a]==0 and tk[b]==0)
rep={}
sk=attr(st.data,'strand_k')
for e in st.data.edges:
 a,b=e.vertices;e.use_seam=bool(sk[a]==0 and sk[b]==0)
for o in (fl,tm,st):rep[o.name+'_unwrap']={k:v for k,v in U.unwrap(o,method='BEST').items() if k in ('unsolved_faces','folding_islands','methods','islands')}
fw=[]
for p in fl.data.polygons:
 pp=part[list(p.vertices)].min();fw.append(.3 if pp==0 else .75 if pp==3 else 1.)
nd=U.normalize_texel_density(fl,None,RES,weights=np.array(fw));U.normalize_texel_density(tm,nd['target_px_per_m'],RES);U.normalize_texel_density(st,nd['target_px_per_m']*.6,RES);rep['density_norm']=nd
rep['pack']=U.pack([fl,tm,st],tex_res=RES,allow_any=True)
for o in (fl,tm,st):rep[o.name+'_qa']={k:v for k,v in U.uv_qa(o,RES).items() if not isinstance(v,(list,dict))};rep[o.name+'_density']=U.texel_density(o,RES)['px_per_m']
U.uv_layout_image(fl,str(O/'Review/uv-flesh.png'),res=1024);U.uv_layout_image(tm,str(O/'Review/uv-teeth.png'),res=1024)
bpy.ops.wm.save_as_mainfile(filepath=str(O/'mouth-v09-low.blend'))
(O/'uv-audit.json').write_text(json.dumps(rep,indent=1,default=str));print('RESULT '+json.dumps(rep,default=str)[:3000])
