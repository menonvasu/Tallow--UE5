"""Original Keeper materials. Blender projects cracked plaster / hessian by position (its UVs overlap), so this does
a triplanar projection from the PRE-SKINNED (rest pose) position and normal, which stays glued to him while he
animates, tinted per slot with the colours from the Blender export. Falls back to flat colours if the graph fails.
Saves with only_if_is_dirty=False: setting SK materials does not dirty the package, so a normal save is skipped."""
import unreal as u, os, traceback
TEX=os.path.expanduser('~/Projects/the-keeper/textures'); DEST='/Game/Characters/Keeper/Surface'
at=u.AssetToolsHelpers.get_asset_tools(); mel=u.MaterialEditingLibrary; EAL=u.EditorAssetLibrary
def fresh(name,cls,factory):
    p=DEST+'/'+name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    return at.create_asset(name,DEST,cls,factory)
def imp(path):
    t=u.AssetImportTask(); t.filename=path; t.destination_path=DEST; t.automated=True; t.replace_existing=True; t.save=True
    at.import_asset_tasks([t]); return u.load_asset(t.imported_object_paths[0])
SPEC={'R3___Woven_soot_umber':((0.05,0.055,0.06),'H',0.9,0.9,3.0),'SCULPT___weathered_ivory_mask':((0.46,0.44,0.4),'P',0.45,0.8,1.2),
'R3___Dark_moist_eyeballs':((0.012,0.018,0.013),None,0.05,0,1),'HYBRID___blended_enamel_and_gum':((0.48,0.4,0.27),'P',0.3,0.3,3),
'HYBRID___blended_gum_and_enamel':((0.085,0.009,0.008),None,0.25,0,1),'R3___Ashen_sinew':((0.3,0.26,0.185),'P',0.6,0.6,2),
'R3___Striated_carapace':((0.06,0.046,0.03),'H',0.7,0.6,3),'R8___oxidized_joint_ferrules':((0.115,0.069,0.028),'P',0.5,0.5,3),
'SCULPT___rooted_ivory_teeth':((0.48,0.4,0.27),'P',0.3,0.3,3),'R7___living_mouth_interior':((0.055,0.009,0.015),None,0.2,0,1),
'R6___deep_red_iris':((0.32,0.004,0.003),None,0.1,0,1),'R3___Deep_lining':((0.02,0.018,0.015),'H',0.9,0.7,3),
'R3___Flax_stitch':((0.22,0.17,0.105),'H',0.8,0.6,4),'R3___Horn_edges':((0.2,0.15,0.085),'P',0.5,0.5,3),
'R3___Socket_darkness':((0.002,0.001,0.001),None,0.9,0,1),'R3___Aged_porcelain':((0.58,0.55,0.48),'P',0.45,0.8,1.2),
'R3___Stained_enamel':((0.54,0.49,0.33),'P',0.35,0.5,3),'R5___cloth_fray_fibres':((0.11,0.084,0.05),'H',0.9,0.7,4)}
def flags(m): m.set_editor_property('used_with_skeletal_mesh',True); m.set_editor_property('used_with_morph_targets',True)
def build_triplanar(PL):
    m=fresh('M_KeeperSurface',u.Material,u.MaterialFactoryNew()); flags(m)
    def C(a,ao,b,bi):
        ok=mel.connect_material_expressions(a,ao,b,bi)
        if not ok: raise RuntimeError('link failed %s.%s -> %s.%s'%(a.get_class().get_name(),ao,b.get_class().get_name(),bi))
        return ok
    def E(cls,x,y,**kw):
        e=mel.create_material_expression(m,cls,x,y)
        for k,v in kw.items(): e.set_editor_property(k,v)
        return e
    # Pre-skinned position/normal only exist in the vertex shader: carry them to the pixel shader via interpolators
    pvs=E(u.MaterialExpressionPreSkinnedPosition,-1900,0); nvs=E(u.MaterialExpressionPreSkinnedNormal,-1900,400)
    pos=E(u.MaterialExpressionVertexInterpolator,-1750,0); nrm=E(u.MaterialExpressionVertexInterpolator,-1750,400)
    for a,b in ((pvs,pos),(nvs,nrm)):
        C(a,'',b,'VS')
    tl=E(u.MaterialExpressionScalarParameter,-1600,200,parameter_name='Tiling',default_value=1.0)
    sp=E(u.MaterialExpressionMultiply,-1400,0); C(pos,'',sp,'A'); C(tl,'',sp,'B')
    tex=E(u.MaterialExpressionTextureObjectParameter,-1400,250,parameter_name='Detail',texture=PL)
    samples=[]
    for i,(a,b) in enumerate((('g','b'),('r','b'),('r','g'))):
        ca=E(u.MaterialExpressionComponentMask,-1250,i*200-250,**{a:True}); C(sp,'',ca,'')
        cb=E(u.MaterialExpressionComponentMask,-1250,i*200-150,**{b:True}); C(sp,'',cb,'')
        uv=E(u.MaterialExpressionAppendVector,-1100,i*200-200); C(ca,'',uv,'A'); C(cb,'',uv,'B')
        s=E(u.MaterialExpressionTextureSample,-900,i*200-200); C(tex,'',s,'Tex'); C(uv,'',s,'UVs'); samples.append(s)
    ab=E(u.MaterialExpressionAbs,-1400,450); C(nrm,'',ab,'')
    sq=E(u.MaterialExpressionMultiply,-1250,450); C(ab,'',sq,'A'); C(ab,'',sq,'B')
    q=E(u.MaterialExpressionMultiply,-1150,450); C(sq,'',q,'A'); C(sq,'',q,'B')          # |n|^4 blend weights
    w=[E(u.MaterialExpressionComponentMask,-1000,400+i*60,**{'rgb'[i]:True}) for i in range(3)]
    for x in w: C(q,'',x,'')
    s01=E(u.MaterialExpressionAdd,-850,420); C(w[0],'',s01,'A'); C(w[1],'',s01,'B')
    tot=E(u.MaterialExpressionAdd,-750,450); C(s01,'',tot,'A'); C(w[2],'',tot,'B')
    acc=None
    for i in range(3):
        mu=E(u.MaterialExpressionMultiply,-700,i*200-200); C(samples[i],'R',mu,'A'); C(w[i],'',mu,'B')
        if acc is None: acc=mu
        else: ad=E(u.MaterialExpressionAdd,-550,i*150-150); C(acc,'',ad,'A'); C(mu,'',ad,'B'); acc=ad
    lum=E(u.MaterialExpressionDivide,-450,0); C(acc,'',lum,'A'); C(tot,'',lum,'B')
    l2=E(u.MaterialExpressionMultiply,-300,0,const_b=2.0); C(lum,'',l2,'A')
    amt=E(u.MaterialExpressionScalarParameter,-320,120,parameter_name='DetailAmount',default_value=0.7)
    one=E(u.MaterialExpressionConstant,-320,180,r=1.0)
    lp=E(u.MaterialExpressionLinearInterpolate,-80,60); C(one,'',lp,'A'); C(l2,'',lp,'B'); C(amt,'',lp,'Alpha')
    col=E(u.MaterialExpressionVectorParameter,-80,-150,parameter_name='Colour',default_value=u.LinearColor(0.5,0.5,0.5,1))
    bc=E(u.MaterialExpressionMultiply,100,-50); C(col,'',bc,'A'); C(lp,'',bc,'B')
    mel.connect_material_property(bc,'',u.MaterialProperty.MP_BASE_COLOR)
    ri=E(u.MaterialExpressionScalarParameter,-80,300,parameter_name='Roughness',default_value=0.6); mel.connect_material_property(ri,'',u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); EAL.save_loaded_asset(m,False); return m
def build_flat():
    m=fresh('M_KeeperFlat',u.Material,u.MaterialFactoryNew()); flags(m)
    col=mel.create_material_expression(m,u.MaterialExpressionVectorParameter,-300,0); col.set_editor_property('parameter_name','Colour')
    mel.connect_material_property(col,'',u.MaterialProperty.MP_BASE_COLOR)
    r=mel.create_material_expression(m,u.MaterialExpressionScalarParameter,-300,200); r.set_editor_property('parameter_name','Roughness'); r.set_editor_property('default_value',0.6)
    mel.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m); EAL.save_loaded_asset(m,False); return m
TEXS={}
try:
    TEXS={'P':imp(f'{TEX}/worn_cracked_plaster/worn_cracked_plaster_diff_2k.jpg'),'H':imp(f'{TEX}/hessian_230/hessian_230_diff_2k.jpg')}
    parent=build_flat(); mode="flat"   # triplanar graph fails to compile on SM6; flat colours until fixed
except Exception:
    u.log_warning('SURF triplanar failed: '+traceback.format_exc().replace('\n',' | ')); parent=build_flat(); mode='flat'
sk=u.load_asset('/Game/Characters/Keeper/Animated/SK_Keeper'); mats=sk.materials; n=0
for s in mats:
    key=str(s.material_slot_name)[len('Keeper_'):]
    c,t,rough,amt,tile=SPEC.get(key,((0.3,0.3,0.3),None,0.6,0,1))
    mi=fresh('MI_Keeper_'+key,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew()); mel.set_material_instance_parent(mi,parent)
    mel.set_material_instance_vector_parameter_value(mi,'Colour',u.LinearColor(c[0],c[1],c[2],1)); mel.set_material_instance_scalar_parameter_value(mi,'Roughness',rough)
    if mode=='triplanar':
        mel.set_material_instance_scalar_parameter_value(mi,'DetailAmount',amt); mel.set_material_instance_scalar_parameter_value(mi,'Tiling',float(tile))
        if t: mel.set_material_instance_texture_parameter_value(mi,'Detail',TEXS[t])
    EAL.save_loaded_asset(mi,False); s.material_interface=mi; n+=1
sk.modify(); sk.set_editor_property('materials',mats)
ok=EAL.save_loaded_asset(sk,False)
check=[str(x.material_interface.get_name()) if x.material_interface else None for x in u.load_asset('/Game/Characters/Keeper/Animated/SK_Keeper').materials]
u.log_warning('SURF mode=%s slots=%d saved=%s check=%s'%(mode,n,ok,check[:4]))
u.SystemLibrary.quit_editor()
