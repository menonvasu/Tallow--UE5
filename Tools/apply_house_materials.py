"""House finish pass: CC0 Poly Haven textures on a world-aligned material, assigned per house mesh, plus boarded-up
windows (from Tools/house-openings.json). Re-runnable. Textures credited in SourceAssets/Textures/PolyHaven/CREDITS.json."""
import unreal as u, os, json, math
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC=os.path.join(ROOT,'Tallow/SourceAssets/Textures/PolyHaven'); DEST='/Game/Environment/HouseMaterials'
AT=u.AssetToolsHelpers.get_asset_tools(); MEL=u.MaterialEditingLibrary; EAL=u.EditorAssetLibrary
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
# 1. textures
tasks=[]
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.jpg'): continue
    name=f[:-4]
    if EAL.does_asset_exist(DEST+'/Textures/'+name): continue
    t=u.AssetImportTask(); t.filename=os.path.join(SRC,f); t.destination_path=DEST+'/Textures'; t.destination_name=name; t.automated=True; t.save=True; t.replace_existing=True
    tasks.append(t)
if tasks: AT.import_asset_tasks(tasks)
def tex(name):
    t=u.load_asset(DEST+'/Textures/'+name)
    if name.endswith('_nor') and t.get_editor_property('compression_settings')!=u.TextureCompressionSettings.TC_NORMALMAP:
        t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP); t.set_editor_property('srgb',False); EAL.save_loaded_asset(t)
    if name.endswith('_rough') and t.get_editor_property('srgb'):
        t.set_editor_property('srgb',False); EAL.save_loaded_asset(t)
    return t
# 2. master material (world aligned, so greybox UVs don't matter)
MP=DEST+'/M_TallowWorldAligned'
if EAL.does_asset_exist(MP): EAL.delete_asset(MP)
m=AT.create_asset('M_TallowWorldAligned',DEST,u.Material,u.MaterialFactoryNew())
m.set_editor_property('tangent_space_normal',False)
WAT=u.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture')
WAN=u.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal')
def node(cls,x,y): return MEL.create_material_expression(m,cls,x,y)
size=node(u.MaterialExpressionScalarParameter,-900,0); size.set_editor_property('parameter_name','TileSize'); size.set_editor_property('default_value',200.0)
def tparam(name,default,y,normal=False):
    p=node(u.MaterialExpressionTextureObjectParameter,-900,y); p.set_editor_property('parameter_name',name); p.set_editor_property('texture',default)
    if normal: p.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    elif name=='RoughTex': p.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    return p
bc=tparam('BaseColorTex',tex('T_decrepit_wallpaper_diff'),-300)
nr=tparam('NormalTex',tex('T_decrepit_wallpaper_nor'),200,True)
rg=tparam('RoughTex',tex('T_decrepit_wallpaper_rough'),500)
def wa(fn,texp,y):
    c=node(u.MaterialExpressionMaterialFunctionCall,-500,y); c.set_material_function(fn)
    MEL.connect_material_expressions(texp,'',c,'TextureObject'); MEL.connect_material_expressions(size,'',c,'TextureSize'); return c
cb=wa(WAT,bc,-300); cn=wa(WAN,nr,200); cr=wa(WAT,rg,500)
tint=node(u.MaterialExpressionVectorParameter,-500,-500); tint.set_editor_property('parameter_name','Tint'); tint.set_editor_property('default_value',u.LinearColor(0.75,0.72,0.68,1))
mul=node(u.MaterialExpressionMultiply,-250,-350)
MEL.connect_material_expressions(cb,'XYZ Texture',mul,'A'); MEL.connect_material_expressions(tint,'',mul,'B')
mask=node(u.MaterialExpressionComponentMask,-250,500); mask.set_editor_property('r',True); mask.set_editor_property('g',False); mask.set_editor_property('b',False); mask.set_editor_property('a',False)
MEL.connect_material_expressions(cr,'XYZ Texture',mask,'')
MEL.connect_material_property(mul,'',u.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(cn,'XYZ Texture',u.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(mask,'',u.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(m); EAL.save_loaded_asset(m)
# 3. instances
SURF={'Wallpaper':('decrepit_wallpaper',180,(0.72,0.66,0.6)),'FloorWood':('old_wooden_floor_02',220,(0.7,0.65,0.6)),
      'BasementBrick':('worn_brick_wall',220,(0.6,0.58,0.55)),'BasementConcrete':('concrete_floor_worn_001',300,(0.6,0.6,0.6)),
      'Planks':('worn_planks',160,(0.7,0.65,0.6)),'Boards':('weathered_planks',140,(0.6,0.55,0.5)),
      'Plaster':('rough_plaster_broken',200,(0.7,0.68,0.64)),'Furniture':('wood_table_worn',120,(0.7,0.62,0.55))}
MI={}
for key,(tid,ts,tn) in SURF.items():
    path=DEST+'/MI_'+key
    mi=u.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset('MI_'+key,DEST,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi,m)
    MEL.set_material_instance_texture_parameter_value(mi,'BaseColorTex',tex('T_%s_diff'%tid))
    MEL.set_material_instance_texture_parameter_value(mi,'NormalTex',tex('T_%s_nor'%tid))
    MEL.set_material_instance_texture_parameter_value(mi,'RoughTex',tex('T_%s_rough'%tid))
    MEL.set_material_instance_scalar_parameter_value(mi,'TileSize',float(ts))
    MEL.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*tn,1))
    EAL.save_loaded_asset(mi); MI[key]=mi
def surface_for(n):
    if n.startswith('Basement_Floor'): return 'BasementConcrete'
    if n.startswith('Basement_Wall'): return 'BasementBrick'
    if 'Floor' in n: return 'FloorWood'
    if any(k in n for k in ('Stairs','Balustrade','Cupboard','FrontDoor','Porch','Stoop','Ladder','WindowSeat','Plinth')): return 'Planks'
    if any(k in n for k in ('Flue','Chimney','LaundryChute')): return 'Plaster'
    if any(k in n for k in ('Roof','Gable','Dormer','Cresting','Tower_Cap')): return 'Boards'
    return 'Wallpaper'
assigned={}
for path in EAL.list_assets('/Game/Environment/M3House',recursive=False,include_folder=False):
    sm=u.load_asset(path)
    if not isinstance(sm,u.StaticMesh): continue
    k=surface_for(sm.get_name()); sm.set_material(0,MI[k]); EAL.save_loaded_asset(sm); assigned[sm.get_name()]=k
for a in eas.get_all_level_actors():
    if str(a.get_folder_path()).startswith('Greybox/Furniture') and isinstance(a,u.StaticMeshActor):
        a.static_mesh_component.set_material(0,MI['Furniture'])
# 4. board up the windows
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Board_'): eas.destroy_actor(a)
cube=u.load_asset('/Engine/BasicShapes/Cube')
openings=json.load(open(os.path.join(ROOT,'Tools','house-openings.json')))['wall_openings']
INSIDE={'front':(0,-1),'back':(0,1),'west':(1,0),'east':(-1,0)}
n=0
for o in openings:
    w=o['u1']-o['u0']; h=o['z1']-o['z0']
    if w>150 or h<50: continue                           # bay/tower links and door slits are not windows
    if o['z0']<=10 and h>300: continue
    ix,iy=INSIDE[o['wall']]; half=24 if o['storey']=='basement' else 12
    count=max(3,int(h/45)); gap=h/count
    for k in range(count):
        z=o['z0']+gap*(k+0.5); uc=(o['u0']+o['u1'])/2
        tilt=((k*37+int(uc))%7-3)*1.2                   # fixed, hand-nailed crookedness
        L=w+24; T=gap*0.72
        if o['axis']=='y': loc=u.Vector(uc,o['plane']+iy*(half+3),z); scale=u.Vector(L/100,0.04,T/100); rot=u.Rotator(0,tilt,0)
        else: loc=u.Vector(o['plane']+ix*(half+3),uc,z); scale=u.Vector(0.04,L/100,T/100); rot=u.Rotator(tilt,0,0)
        b=eas.spawn_actor_from_class(u.StaticMeshActor,loc,rot); b.set_actor_label('Board_%s_%s_%d_%d'%(o['storey'],o['wall'],int(uc),k))
        b.static_mesh_component.set_static_mesh(cube); b.set_actor_scale3d(scale); b.static_mesh_component.set_material(0,MI['Boards'])
        b.set_folder_path('Greybox/Boards'); n+=1
assert ed.save_current_level()
u.log('MATERIALS assigned %s; boards %d'%(json.dumps({k:sum(1 for v in assigned.values() if v==k) for k in SURF}),n))
