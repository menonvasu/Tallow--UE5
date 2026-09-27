"""Run in UE 5.8 Python commandlet. Creates an isolated M3 test map."""
import unreal as u,json,os
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVEL='/Game/Maps/Lvl_Tallow_M3_Greybox'
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
if u.EditorAssetLibrary.does_asset_exist(LEVEL):
 assert ed.load_level(LEVEL)
 for a in actors.get_all_level_actors():
  if a.get_class().get_name() not in ['WorldSettings','Brush']: actors.destroy_actor(a)
else: assert ed.new_level(LEVEL)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
def spawn(cls,label,xyz,rot=(0,0,0)):
 a=actors.spawn_actor_from_class(cls,u.Vector(*xyz),u.Rotator(*rot));assert a,label;a.set_actor_label(label);return a
def bp(path,label,xyz):return spawn(u.EditorAssetLibrary.load_blueprint_class(path),label,xyz)
def mesh_actor(mesh,label,xyz=(0,0,0),scale=(1,1,1)):
 a=spawn(u.StaticMeshActor,label,xyz);c=a.static_mesh_component;c.set_static_mesh(mesh);a.set_actor_scale3d(u.Vector(*scale));return a
cube=u.load_asset('/Engine/BasicShapes/Cube')
def box(label,x,y,z,sx,sy,sz):
 # design coordinates are Blender metres; Unreal flips Y and uses centimetres
 a=mesh_actor(cube,label,(x*100,-y*100,(z+sz/2)*100),(sx,sy,sz));a.set_folder_path('Greybox/Furniture');return a
# Existing imported shell has its room coordinates baked into each mesh.
for path in u.EditorAssetLibrary.list_assets('/Game/Environment/TallowHouse',recursive=False,include_folder=False):
 asset=u.load_asset(path)
 if isinstance(asset,u.StaticMesh):
  target='/Game/Environment/M3House/'+asset.get_name()
  asset=u.load_asset(target) if u.EditorAssetLibrary.does_asset_exist(target) else u.EditorAssetLibrary.duplicate_asset(path,target)
  asset.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  u.EditorAssetLibrary.save_loaded_asset(asset)
  a=mesh_actor(asset,'House_'+asset.get_name());a.set_folder_path('Greybox/House')
# Main route: chair -> workshop -> toy corridor -> boiler -> laundry stairs -> hall -> upper film room.
box('Workshop_Bench',2.5,10.4,-3.3,2.4,.8,1.05)
box('ToyShelf_West',6.2,5.5,-3.3,.45,3.0,2.5)
box('Boiler_SightBreak',13.6,3.2,-3.3,1.6,1.1,2.25)
# Side approach gives the basement stair foot room for the adult capsule.
box('Basement_StairApproach_Low',14.08,7.72,-3.3,.6,.84,.195)
box('Basement_StairApproach_High',13.48,7.72,-3.3,.6,.84,.39)
box('Laundry_Table_Top',14.4,10.5,-2.3,1.6,1.2,.12)
for x in [13.7,15.1]:
 for y in [10.0,11.0]:box('Laundry_Table_Leg',x,y,-3.3,.12,.12,1.0)
box('Dining_Table_Top',2.8,10.4,1.1,2.3,1.3,.12)
for x in [1.8,3.8]:
 for y in [9.9,10.9]:box('Dining_Table_Leg',x,y,0,.12,.12,1.1)
box('Study_Desk',13.3,3.5,0,1.7,.8,1.25)
box('Kitchen_Counter',14.9,11.3,0,.8,2.4,1.15)
box('ToyRoom_Shelf',1.0,3.8,3.9,.65,2.1,2.3)
box('Film_Projector_Table',2.5,10.2,3.9,1.6,.8,1.0)
box('Bedroom_Bed_Top',13.2,3.3,4.65,2.5,1.6,.18)
for x in [12.1,14.3]:
 for y in [2.65,3.95]:box('Bedroom_Bed_Leg',x,y,3.9,.15,.15,.75)
# Three separate ledge heights for the planned climb mechanic; none gates the main route.
climbs=[]
for name,x,y,z,height in [('Workshop',1.5,9,-3.3,.55),('Study',11.4,3.5,0,.70),('ToyRoom',2.1,3.8,3.9,.95)]:
 a=box('CLIMB_'+name+'_Ledge',x,y,z,.9,.85,height);a.tags=['Tallow.Climb.Test','HeightCm='+str(int(height*100))]
 climbs.append({'name':name,'height_cm':int(height*100),'approach_cm':[x*100,-(y-1)*100,z*100+60],'top_cm':[x*100,-y*100,(z+height)*100]})
 # low step allows route testing before climbing is implemented
 box('TEST_STEP_'+name,x,y-.75,z,.9,.55,height/2)
# Working gameplay classes from MR !11.
start=spawn(u.PlayerStart,'M3_PlayerStart',(286,-500,-260))
chair=bp('/Game/Tasks/BP_TallowChairEscape','M3_ChairEscape',(286,-325,-330))
candle=bp('/Game/Environment/BP_TallowCandlePickup','M3_CandlePickup',(365,-325,-275))
creature=bp('/Game/AI/Blueprints/BP_TallowCreature','M3_Keeper',(1100,-300,-234))
points=[]
for i,p in enumerate([(1100,-300,-325),(1430,-650,-325),(1430,-800,-325),(830,-1040,-325),(825,-325,-325)]):
 points.append(spawn(u.load_class(None,'/Script/Tallow.TallowPatrolPoint'),'M3_Patrol_%02d'%i,p))
for i,p in enumerate(points):p.set_editor_property('next_points',[points[(i+1)%len(points)]])
creature.set_editor_property('starting_patrol_point',points[0])
# Reuse the existing game mode, player and input setup.
world.get_world_settings().set_editor_property('default_game_mode',u.EditorAssetLibrary.load_blueprint_class('/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode'))
# Prototype visual follows the working AI actor; final skeletal animation remains separate.
task=u.AssetImportTask();task.filename=os.path.join(ROOT,'Tallow/SourceAssets/Keeper/SM_Keeper_Host_Proxy.fbx');task.destination_path='/Game/Characters/Keeper';task.automated=True;task.save=True;task.replace_existing=True
opts=u.FbxImportUI();opts.import_mesh=True;opts.import_as_skeletal=False;opts.import_materials=True;opts.import_textures=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False;task.options=opts
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=next((u.load_asset(p) for p in task.imported_object_paths if isinstance(u.load_asset(p),u.StaticMesh)),None);assert mesh,'Keeper import failed'
proxy=mesh_actor(mesh,'M3_Keeper_Visual',(1100,-300,-330));proxy.set_actor_scale3d(u.Vector(.58,.8,1));proxy.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE);proxy.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
proxy.attach_to_actor(creature,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
creature.get_component_by_class(u.SkeletalMeshComponent).set_visibility(False,True)
for i,(x,y,z) in enumerate([(5.85,9.1,0),(5.85,12.48,3.9),(9.75,12.48,3.9)]):
 gap=spawn(u.load_class(None,'/Script/Tallow.TallowChildGapVolume'),'M3_ChildGap_%d'%i,(x*100,-y*100,z*100+40));gap.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(40,50,45))
# Broad coverage for all floors; factory creates the default volume brush.
nav=spawn(u.NavMeshBoundsVolume,'M3_NavigationBounds',(780,-650,360));nav.set_actor_scale3d(u.Vector(10,9,10))
# Readable warm/cold lighting for playtesting.
sun=spawn(u.DirectionalLight,'M3_Moon',(0,0,1800),(-50,-25,0));sun.light_component.set_intensity(.5)
for i,(x,y,z) in enumerate([(2.8,3.2,-1.0),(2.5,10,-1),(8,4,-1),(13,9,-1),(3,3,2.7),(8,2,2.7),(13,10,2.7),(3,10,6.5),(8,4,6.5),(13,3,6.5)]):
 l=spawn(u.PointLight,'M3_WorkLight_%02d'%i,(x*100,-y*100,z*100));l.light_component.set_intensity(2500);l.light_component.set_attenuation_radius(850)
# Markers are editor-only, for task implementation rather than fake playable tasks.
for name,xyz in [('FuseBox',(1450,-350,-190)),('Crowbar',(250,-1040,-210)),('Projector',(250,-1020,510)),('Escape',(780,30,60))]:
 a=spawn(u.TargetPoint,'TODO_Task_'+name,xyz);a.set_editor_property('is_editor_only_actor',True)
u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
assert ed.save_current_level()
u.EditorAssetLibrary.save_directory('/Game/Characters/Keeper',only_if_is_dirty=True,recursive=True)
report={'map':LEVEL,'actors':len(actors.get_all_level_actors()),'climb_tests':climbs,'nav_bounds':str(nav.get_actor_bounds(False)),'keeper_bounds':str(mesh.get_bounding_box()),'note':'Climb and three task markers are layout only. Requires in-editor playtest.'}
open(os.path.join(ROOT,'Tools/m3-layout.json'),'w').write(json.dumps(report,indent=2))
