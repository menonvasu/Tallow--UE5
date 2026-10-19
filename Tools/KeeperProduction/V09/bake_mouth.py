"""V09 mouth, stage 4: our own texture set (one 2048 atlas shared by slots M_KeeperV07_Mouth and M_KeeperV07_Teeth).
Normal: Tripo C3 sculpt (teeth-free) + our object-space wrinkle/vein relief, baked selected-to-active onto our low flesh; teeth from
our own enamel bump. AO: high flesh with our teeth as occluders; teeth with our flesh as occluder. BaseColor/Roughness: our procedural
flesh (wet, bruised lips, inflamed gums, veins, blood at the sockets, darkening down the throat) and enamel (yellowed, blood-dark
roots, cracks) shaders driven by the maw's own attributes and the baked AO, baked through Emission. Run under the heavy-render flock."""
import bpy,sys,json,os,numpy as np
from pathlib import Path
sys.path.append('/home/tyron/.claude/skills/scenario-blender-uv-baking/scripts');import bx_uvbake as U
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';TX=O/'Textures';TMP=TX/'_bake';TMP.mkdir(parents=True,exist_ok=True)
RES=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 2048;MARGIN=6
bpy.ops.wm.open_mainfile(filepath=str(O/'mouth-v09-low.blend'))
fl,tm,st,fh,op=[bpy.data.objects[n] for n in ('MOUTH_Flesh','MOUTH_Teeth','MOUTH_Strands','TRIPO_C3_FleshHigh','TRIPO_C3_Opened')]
sc=bpy.context.scene;sc.render.engine='CYCLES';dev=U._use_gpu(sc);sc.cycles.use_denoising=False
for o in sc.objects:o.hide_render=o not in (fl,tm,st,fh,op);o.hide_viewport=False;o.hide_set(False)
def N(nt,t,**kw):
 n=nt.nodes.new(t)
 for k,v in sorted(kw.items(),key=lambda kv:kv[0].startswith('i_')):  # properties first (they create sockets), then inputs
  if k.startswith('i_'):n.inputs[k[2:].replace('_',' ')].default_value=v
  else:setattr(n,k,v)
 return n
def L(nt,a,b):nt.links.new(a,b)
def ramp(nt,src,stops,interp='LINEAR'):
 r=N(nt,'ShaderNodeValToRGB');r.color_ramp.interpolation=interp;E=r.color_ramp.elements
 while len(E)>len(stops):E.remove(E[-1])
 while len(E)<len(stops):E.new(0)
 for e,(p,c) in zip(E,stops):e.position=p;e.color=(c,c,c,1) if isinstance(c,(int,float)) else (*c,1)
 L(nt,src,r.inputs['Fac']);return r
def mix(nt,f,a,b,blend='MIX'):
 m=N(nt,'ShaderNodeMix',data_type='RGBA',blend_type=blend,clamp_result=True)
 for s,v in ((0,f),(6,a),(7,b)):
  if isinstance(v,bpy.types.NodeSocket):L(nt,v,m.inputs[s])
  else:m.inputs[s].default_value=v if s==0 else (*v,1)
 return m.outputs[2]
def math_(nt,op,a,b=None,c=None,clamp=False):
 m=N(nt,'ShaderNodeMath',operation=op,use_clamp=clamp)
 for i,v in enumerate((a,b,c)):
  if v is None:continue
  if isinstance(v,bpy.types.NodeSocket):L(nt,v,m.inputs[i])
  else:m.inputs[i].default_value=v
 return m.outputs[0]
def attr(nt,name):return N(nt,'ShaderNodeAttribute',attribute_name=name,attribute_type='GEOMETRY').outputs['Fac']
def veins(nt,tc,scale=22.,width=.035,seed=0.):
 """Shared vein network in object space: identical on the high (relief) and the low (colour)."""
 dn=N(nt,'ShaderNodeTexNoise',i_Scale=4.,i_Detail=4.,i_W=seed,noise_dimensions='4D');L(nt,tc.outputs['Object'],dn.inputs['Vector'])
 off=N(nt,'ShaderNodeVectorMath',operation='SCALE');L(nt,dn.outputs['Color'],off.inputs[0]);off.inputs['Scale'].default_value=.10
 add=N(nt,'ShaderNodeVectorMath',operation='ADD');L(nt,tc.outputs['Object'],add.inputs[0]);L(nt,off.outputs[0],add.inputs[1])
 vo=N(nt,'ShaderNodeTexVoronoi',feature='DISTANCE_TO_EDGE',i_Scale=scale);L(nt,add.outputs[0],vo.inputs['Vector'])
 mr=N(nt,'ShaderNodeMapRange',clamp=True);L(nt,vo.outputs['Distance'],mr.inputs['Value']);mr.inputs['From Min'].default_value=0;mr.inputs['From Max'].default_value=width;mr.inputs['To Min'].default_value=1;mr.inputs['To Max'].default_value=0
 gate=N(nt,'ShaderNodeTexNoise',i_Scale=3.2,i_Detail=2.,i_W=seed+11.,noise_dimensions='4D');L(nt,tc.outputs['Object'],gate.inputs['Vector'])
 return math_(nt,'MULTIPLY',mr.outputs['Result'],ramp(nt,gate.outputs['Fac'],[(.52,0.),(.68,1.)]).outputs['Color'])  # vessels come and go
def fresh(mat):
 nt=mat.node_tree;nt.nodes.clear();return nt,N(nt,'ShaderNodeTexCoord')
# ---- high relief material (Tripo sculpt + our wrinkles and raised veins) ----
hm=bpy.data.materials.new('V09_HighRelief');nt,tc=fresh(hm);out=N(nt,'ShaderNodeOutputMaterial');bs=N(nt,'ShaderNodeBsdfPrincipled');L(nt,bs.outputs[0],out.inputs['Surface'])
wr=N(nt,'ShaderNodeTexNoise',i_Scale=150.,i_Detail=6.,i_Roughness=.62);L(nt,tc.outputs['Object'],wr.inputs['Vector'])
fold=N(nt,'ShaderNodeTexNoise',i_Scale=38.,i_Detail=3.,noise_type='RIDGED_MULTIFRACTAL');L(nt,tc.outputs['Object'],fold.inputs['Vector'])
h=math_(nt,'ADD',math_(nt,'MULTIPLY',wr.outputs['Fac'],.55),math_(nt,'ADD',math_(nt,'MULTIPLY',veins(nt,tc),.22),math_(nt,'MULTIPLY',veins(nt,tc,60.,.05,3.),.10)))
h=math_(nt,'ADD',h,math_(nt,'MULTIPLY',fold.outputs['Fac'],.35))
bu=N(nt,'ShaderNodeBump',i_Distance=.0011,i_Strength=1.);L(nt,h,bu.inputs['Height']);L(nt,bu.outputs['Normal'],bs.inputs['Normal'])
for o in (fh,op):o.data.materials.clear();o.data.materials.append(hm)
rep={'device':dev,'res':RES}
# ---- flesh normal + AO (selected-to-active from the Tripo sculpt) ----
r=U.bake_high_to_low(fl,[fh,op],str(TMP),res=RES,maps=('NORMAL','AO'),occluders=(tm,st),ao_distance=.06,prefix='flesh',margin_px=MARGIN)
rep['flesh_hi']={k:r.get(k) for k in ('settings','adaptive','sanity','warnings','paths','device')}
# ---- own-object bakes (teeth normal/AO, colour/roughness through emission) ----
def target(obj,name,colorspace,float_buffer):
 img=bpy.data.images.new(name,RES,RES,alpha=False,float_buffer=float_buffer);img.colorspace_settings.name=colorspace
 for m in obj.data.materials:
  nt=m.node_tree;n=N(nt,'ShaderNodeTexImage',image=img,name='V09_TARGET')
  for x in nt.nodes:x.select=False
  n.select=True;nt.nodes.active=n
 return img
def bake(obj,kind,img,samples,others_visible=()):
 for o in sc.objects:o.hide_render=o not in (obj,*others_visible);o.select_set(False)
 obj.select_set(True);bpy.context.view_layer.objects.active=obj;sc.cycles.samples=samples
 assert bpy.ops.object.bake(type=kind,use_selected_to_active=False,margin=MARGIN,margin_type='EXTEND',use_clear=True,target='IMAGE_TEXTURES',normal_space='TANGENT',normal_r='POS_X',normal_g='POS_Y',normal_b='POS_Z')=={'FINISHED'}
 return img
def load(path,cs):i=bpy.data.images.load(path,check_existing=False);i.colorspace_settings.name=cs;return i
def uvtex(nt,img):
 n=N(nt,'ShaderNodeTexImage',image=img);u=N(nt,'ShaderNodeUVMap',uv_map='ProductionUV');L(nt,u.outputs[0],n.inputs['Vector']);return n.outputs['Color']
tmat=tm.data.materials[0];fmat=fl.data.materials[0]
# teeth relief: fine growth ridges along the tooth + hairline cracks
nt,tc=fresh(tmat);out=N(nt,'ShaderNodeOutputMaterial');bs=N(nt,'ShaderNodeBsdfPrincipled');L(nt,bs.outputs[0],out.inputs['Surface'])
th=attr(nt,'tooth_h');rid=N(nt,'ShaderNodeTexWave',wave_type='RINGS',i_Scale=2.,i_Distortion=3.,i_Detail=2.);L(nt,tc.outputs['Object'],rid.inputs['Vector'])
crk=N(nt,'ShaderNodeTexNoise',i_Scale=260.,i_Detail=4.,noise_type='RIDGED_MULTIFRACTAL');L(nt,tc.outputs['Object'],crk.inputs['Vector'])
hh=math_(nt,'ADD',math_(nt,'MULTIPLY',rid.outputs['Fac'],.25),math_(nt,'MULTIPLY',crk.outputs['Fac'],.5))
bu=N(nt,'ShaderNodeBump',i_Distance=.0009,i_Strength=1.);L(nt,hh,bu.inputs['Height']);L(nt,bu.outputs['Normal'],bs.inputs['Normal'])
tn=bake(tm,'NORMAL',target(tm,'teeth_normal','Non-Color',True),16)
if not sc.world:sc.world=bpy.data.worlds.new('World')
sc.world.light_settings.distance=.03;tao=bake(tm,'AO',target(tm,'teeth_ao','Non-Color',True),128,(fl,st))
fao=load(rep['flesh_hi']['paths']['AO'],'Non-Color')
# flesh colour / roughness
def flesh_tree(mode):
 nt,tc=fresh(fmat);out=N(nt,'ShaderNodeOutputMaterial');em=N(nt,'ShaderNodeEmission',i_Strength=1.);L(nt,em.outputs[0],out.inputs['Surface'])
 prof=attr(nt,'mouth_prof');ao=uvtex(nt,fao)
 if mode=='color':
  base=ramp(nt,math_(nt,'MULTIPLY_ADD',prof,.5,.5),[(0.,(.10,.04,.05)),(.20,(.18,.09,.10)),(.38,(.30,.19,.20)),(.50,(.34,.13,.14)),(.56,(.40,.07,.08)),(.65,(.30,.04,.05)),(.71,(.10,.012,.016)),(.78,(.025,.004,.006)),(1.,(.004,.001,.0015))]).outputs['Color']
  mo=N(nt,'ShaderNodeTexNoise',i_Scale=16.,i_Detail=4.);L(nt,tc.outputs['Object'],mo.inputs['Vector'])
  mott=ramp(nt,mo.outputs['Fac'],[(.3,(.78,.78,.78)),(.7,(1.16,1.16,1.16))]).outputs['Color']
  c=mix(nt,1.,base,mott,'MULTIPLY')
  br=N(nt,'ShaderNodeTexNoise',i_Scale=5.,i_Detail=2.,i_W=7.,noise_dimensions='4D');L(nt,tc.outputs['Object'],br.inputs['Vector'])
  bruise=math_(nt,'MULTIPLY',ramp(nt,br.outputs['Fac'],[(.5,0.),(.68,1.)]).outputs['Color'],ramp(nt,prof,[(-.8,.0),(-.4,.9),(.05,.6),(.2,.0)]).outputs['Color'])
  c=mix(nt,bruise,c,(.20,.07,.16))
  vs=ramp(nt,math_(nt,'MULTIPLY_ADD',prof,.5,.5),[(.15,.25),(.35,.9),(.5,.7),(.65,.45),(.8,.15)]).outputs['Color']
  c=mix(nt,math_(nt,'MULTIPLY',veins(nt,tc),math_(nt,'MULTIPLY',vs,.75)),c,(.12,.03,.10))
  c=mix(nt,math_(nt,'MULTIPLY',veins(nt,tc,60.,.05,3.),math_(nt,'MULTIPLY',vs,.55)),c,(.30,.02,.04))
  sock=math_(nt,'MULTIPLY',ramp(nt,ao,[(.35,1.),(.75,0.)]).outputs['Color'],ramp(nt,prof,[(-.05,0.),(.05,1.),(.45,1.),(.6,0.)]).outputs['Color'])
  c=mix(nt,sock,c,(.20,.012,.012))  # blood pooled in the tooth sockets and gum creases
  pus=N(nt,'ShaderNodeTexVoronoi',i_Scale=95.);L(nt,tc.outputs['Object'],pus.inputs['Vector'])
  pm=math_(nt,'MULTIPLY',ramp(nt,pus.outputs['Distance'],[(.0,1.),(.14,0.)]).outputs['Color'],ramp(nt,prof,[(.0,0.),(.1,.55),(.35,.35),(.45,0.)]).outputs['Color'])
  c=mix(nt,pm,c,(.48,.36,.17))  # yellowed mucus beads on the gum ridges
  c=mix(nt,1.,c,ramp(nt,ao,[(0.,(.05,.05,.05)),(1.,(1,1,1))]).outputs['Color'],'MULTIPLY')  # cavity/throat occlusion into albedo
  L(nt,c,em.inputs['Color'])
 else:
  rr=ramp(nt,prof,[(-1.,.5),(-.45,.40),(-.12,.26),(0.,.16),(.45,.10),(1.,.07)]).outputs['Color']
  rn=N(nt,'ShaderNodeTexNoise',i_Scale=34.,i_Detail=3.);L(nt,tc.outputs['Object'],rn.inputs['Vector'])
  v=math_(nt,'ADD',rr,math_(nt,'MULTIPLY',math_(nt,'SUBTRACT',rn.outputs['Fac'],.5),.16))
  v=math_(nt,'SUBTRACT',v,math_(nt,'MULTIPLY',math_(nt,'SUBTRACT',1.,ao),.10),clamp=True)
  L(nt,v,em.inputs['Color'])
def teeth_tree(mode):
 nt,tc=fresh(tmat);out=N(nt,'ShaderNodeOutputMaterial');em=N(nt,'ShaderNodeEmission',i_Strength=1.);L(nt,em.outputs[0],out.inputs['Surface'])
 th=attr(nt,'tooth_h');tid=attr(nt,'tooth_id');ao=uvtex(nt,tao)
 wn=N(nt,'ShaderNodeTexWhiteNoise',noise_dimensions='1D');L(nt,tid,wn.inputs['W'])
 crk=N(nt,'ShaderNodeTexNoise',i_Scale=260.,i_Detail=4.,noise_type='RIDGED_MULTIFRACTAL');L(nt,tc.outputs['Object'],crk.inputs['Vector'])
 if mode=='color':
  c=ramp(nt,th,[(0.,(.10,.035,.025)),(.12,(.24,.11,.06)),(.35,(.46,.34,.17)),(.75,(.60,.50,.30)),(1.,(.70,.62,.44))]).outputs['Color']
  c=mix(nt,1.,c,ramp(nt,wn.outputs['Value'],[(0.,(.55,.47,.40)),(.5,(.85,.82,.76)),(1.,(1.,.98,.94))]).outputs['Color'],'MULTIPLY')
  c=mix(nt,ramp(nt,crk.outputs['Fac'],[(.62,0.),(.85,.7)]).outputs['Color'],c,(.22,.13,.07))
  c=mix(nt,1.,c,ramp(nt,ao,[(0.,(.18,.18,.18)),(1.,(1,1,1))]).outputs['Color'],'MULTIPLY')
  L(nt,c,em.inputs['Color'])
 else:
  v=ramp(nt,th,[(0.,.62),(.25,.48),(.8,.30),(1.,.24)]).outputs['Color']
  v=math_(nt,'ADD',v,math_(nt,'MULTIPLY',crk.outputs['Fac'],.12))
  L(nt,v,em.inputs['Color'])
imgs={}
for mode,cs,fb in (('color','sRGB',False),('rough','Non-Color',False)):
 flesh_tree(mode);imgs['flesh_'+mode]=bake(fl,'EMIT',target(fl,'flesh_'+mode,cs,fb),4)
 teeth_tree(mode);imgs['teeth_'+mode]=bake(tm,'EMIT',target(tm,'teeth_'+mode,cs,fb),4)
imgs['flesh_normal']=load(rep['flesh_hi']['paths']['NORMAL'],'Non-Color');imgs['flesh_ao']=fao;imgs['teeth_normal']=tn;imgs['teeth_ao']=tao
# strands: own bake material (glossy, pale, blood-tinged where they leave the gums), then back to the mouth slot
smat=bpy.data.materials.new('V09_StrandBake');st.data.materials[0]=smat;nt,tc=fresh(smat);out=N(nt,'ShaderNodeOutputMaterial');bs=N(nt,'ShaderNodeBsdfPrincipled');L(nt,bs.outputs[0],out.inputs['Surface'])
sn=N(nt,'ShaderNodeTexNoise',i_Scale=900.,i_Detail=2.);L(nt,tc.outputs['Object'],sn.inputs['Vector']);bu=N(nt,'ShaderNodeBump',i_Distance=.0002,i_Strength=.5);L(nt,sn.outputs['Fac'],bu.inputs['Height']);L(nt,bu.outputs['Normal'],bs.inputs['Normal'])
imgs['strand_normal']=bake(st,'NORMAL',target(st,'strand_normal','Non-Color',True),16)
sc.world.light_settings.distance=.03;imgs['strand_ao']=bake(st,'AO',target(st,'strand_ao','Non-Color',True),64,(fl,tm))
for mode,cs in (('color','sRGB'),('rough','Non-Color')):
 nt,tc=fresh(smat);out=N(nt,'ShaderNodeOutputMaterial');em=N(nt,'ShaderNodeEmission',i_Strength=1.);L(nt,em.outputs[0],out.inputs['Surface'])
 t_=attr(nt,'strand_t');edge=ramp(nt,math_(nt,'ABSOLUTE',math_(nt,'SUBTRACT',t_,.5)),[(.30,0.),(.48,1.)]).outputs['Color']
 if mode=='color':c=mix(nt,edge,(.30,.22,.21),(.26,.06,.06));c=mix(nt,1.,c,ramp(nt,uvtex(nt,imgs['strand_ao']),[(0.,(.3,.3,.3)),(1.,(1,1,1))]).outputs['Color'],'MULTIPLY')
 else:c=math_(nt,'ADD',.04,math_(nt,'MULTIPLY',edge,.06))
 L(nt,c,em.inputs['Color']);imgs['strand_'+mode]=bake(st,'EMIT',target(st,'strand_'+mode,cs,False),4)
st.data.materials[0]=fmat
# ---- composite into one atlas (teeth islands never overlap flesh islands), pad, save ----
mf,mt,ms_=U.island_mask(fl,RES),U.island_mask(tm,RES),U.island_mask(st,RES)
def px(img):a=np.empty(RES*RES*4,dtype=np.float32);img.pixels.foreach_get(a);return a.reshape(RES,RES,4)
def dilate(m,n):
 m=m.copy()
 for i in range(n):m|=np.roll(m,1,0)|np.roll(m,-1,0)|np.roll(m,1,1)|np.roll(m,-1,1)
 return m
mt_d=dilate(mt,MARGIN)&~mf&~ms_;ms_d=dilate(ms_,MARGIN)&~mf&~mt
final={}
for kind,cs,fb,name in (('color','sRGB',False,'BaseColor'),('rough','Non-Color',False,'Roughness'),('normal','Non-Color',True,'Normal'),('ao','Non-Color',False,'AO')):
 a=px(imgs['flesh_'+kind]);b=px(imgs['teeth_'+kind]);c_=px(imgs['strand_'+kind]);a[mt_d]=b[mt_d];a[ms_d]=c_[ms_d];a[...,3]=1
 img=bpy.data.images.new('T_KeeperMouthV09_'+name,RES,RES,alpha=False,float_buffer=fb);img.colorspace_settings.name=cs;img.pixels.foreach_set(a.ravel())
 p=TX/f'T_KeeperMouthV09_{name}.png';img.filepath_raw=str(p);img.file_format='PNG';img.save();final[name]=img
 rep.setdefault('maps',{})[name]={'path':str(p.relative_to(R)),'bit_depth':16 if fb else 8,'colorspace':cs}
rep['teeth_normal_sanity']=U.bake_sanity(tn,'NORMAL',mt);rep['coverage_flesh']=float(mf.mean());rep['coverage_teeth']=float(mt.mean());rep['coverage_strands']=float(ms_.mean())
# ---- final game materials (same slot names) read the baked set; relative paths from V09Mouth/ ----
for mat,emis in ((fmat,0),(tmat,1)):
 nt,tc=fresh(mat);out=N(nt,'ShaderNodeOutputMaterial');bs=N(nt,'ShaderNodeBsdfPrincipled');L(nt,bs.outputs[0],out.inputs['Surface'])
 for name in ('BaseColor','Roughness','Normal'):
  img=bpy.data.images.load(str(TX/f'T_KeeperMouthV09_{name}.png'),check_existing=True);img.colorspace_settings.name='sRGB' if name=='BaseColor' else 'Non-Color'
  t=N(nt,'ShaderNodeTexImage',image=img,label=name);u=N(nt,'ShaderNodeUVMap',uv_map='ProductionUV');L(nt,u.outputs[0],t.inputs['Vector'])
  if name=='BaseColor':L(nt,t.outputs['Color'],bs.inputs['Base Color'])
  elif name=='Roughness':L(nt,t.outputs['Color'],bs.inputs['Roughness'])
  else:nm=N(nt,'ShaderNodeNormalMap',uv_map='ProductionUV');L(nt,t.outputs['Color'],nm.inputs['Color']);L(nt,nm.outputs['Normal'],bs.inputs['Normal'])
 bs.inputs['Subsurface Weight'].default_value=.0 if emis else .25;bs.inputs['Subsurface Radius'].default_value=(1.,.25,.12);bs.inputs['Subsurface Scale'].default_value=.006
 bs.inputs['Specular IOR Level'].default_value=.5;mat.diffuse_color=(.4,.08,.08,1) if not emis else (.7,.6,.4,1)
for i in list(bpy.data.images):
 if i.users==0:bpy.data.images.remove(i)
bpy.ops.file.make_paths_relative()
bpy.ops.wm.save_as_mainfile(filepath=str(O/'mouth-v09-low.blend'))
(O/'bake-audit.json').write_text(json.dumps(rep,indent=1,default=str));print('RESULT '+json.dumps(rep,default=str)[:2500])
