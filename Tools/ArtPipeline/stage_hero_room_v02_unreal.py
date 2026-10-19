"""Lead-run V02 review maps only. Uses new_level, never duplicate_asset(World)."""
from pathlib import Path
import json,gc
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SRC=ROOT/'Tallow/SourceAssets/Production/HeroRoomV02';BASE='/Game/Presentation/Environment';DEST=BASE+'/Maps/';E=u.EditorAssetLibrary;ed=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
rows=json.loads((SRC/'architecture-metrics.json').read_text());meshes={row['mesh']:u.load_asset(BASE+'/HeroRoomV02/Meshes/'+row['mesh']) for row in rows};assert all(meshes.values())
chair=u.load_asset(BASE+'/HeroProps/RestraintChair/SM_RestraintChair');doll=u.load_asset(BASE+'/HeroProps/CollectionDoll/SM_CollectionDoll');cab=u.load_asset(BASE+'/Cabinet/SM_Cabinet_SageTimber');assert chair and doll and cab
assert all(not E.does_asset_exist(DEST+n) for n in ['Lvl_AtticHeroRoomV02','Lvl_MenuHeroRoomV02'])
def vec(loc):return u.Vector(loc[0]*100,-loc[1]*100,loc[2]*100)
def spawn(mesh,label,loc=(0,0,0),scale=1,yaw=0):
 a=actors.spawn_actor_from_class(u.StaticMeshActor,vec(loc),u.Rotator(pitch=0,yaw=-yaw,roll=0));a.set_actor_label(label);a.static_mesh_component.set_static_mesh(mesh);a.set_actor_scale3d(u.Vector(scale,scale,scale));a.set_actor_enable_collision(False)
def light(name,loc,target,power,color,size):
 a=actors.spawn_actor_from_class(u.RectLight,vec(loc),u.MathLibrary.find_look_at_rotation(vec(loc),vec(target)));a.set_actor_label(name);c=a.get_component_by_class(u.RectLightComponent);c.set_editor_property('intensity',power);c.set_editor_property('source_width',size*100);c.set_editor_property('source_height',size*100);c.set_light_color(u.LinearColor(*color,1))
def camera(loc,target,fov):
 a=actors.spawn_actor_from_class(u.CameraActor,vec(loc),u.MathLibrary.find_look_at_rotation(vec(loc),vec(target)));a.set_actor_label('ReviewCamera_V02');c=a.camera_component;c.set_field_of_view(fov);s=c.get_editor_property('post_process_settings');s.set_editor_property('override_auto_exposure_method',True);s.set_editor_property('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL);s.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);s.set_editor_property('auto_exposure_apply_physical_camera_exposure',False);s.set_editor_property('override_auto_exposure_bias',True);s.set_editor_property('auto_exposure_bias',0);c.set_editor_property('post_process_settings',s);c.set_editor_property('post_process_blend_weight',1)
for menu,name in [(False,'Lvl_AtticHeroRoomV02'),(True,'Lvl_MenuHeroRoomV02')]:
 assert ed.new_level(DEST+name)
 u.EditorLevelLibrary.get_editor_world().get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
 for row in rows:
  if menu and row['mesh'].endswith(('CollectionBay','Exit')):continue
  if not menu and row.get('menu_only'):continue
  spawn(meshes[row['mesh']],row['mesh'])
 if menu:
  spawn(chair,'DetailedMenuChair',(.83,.85,.045),.70)
  light('MenuWarm',(1.6,-.9,.95),(.8,.8,1),46,(1,.58,.3),.65);light('MenuCold',(1.4,1.8,2.4),(.9,.8,1),70,(.39,.52,.78),.8);light('MenuFill',(-.6,-1.7,1.3),(.7,.8,1),7,(.5,.55,.68),2);camera((0,-3.45,1),(0,.7,1.05),53.13)
 else:
  spawn(cab,'CollectionCabinet',(-1.3,1.58,.04))
  for label,loc,scale,yaw in [('ShelfBack',(-1.3,1.49,.89),.50,180),('ShelfSide',(-1.28,1.50,.49),.42,155),('SilentWitness',(.15,1.68,.05),1.04,25)]:spawn(doll,label,loc,scale,yaw)
  light('ColdExit',(1.1,3,1.8),(-.7,.2,.8),190,(.48,.64,1),1.2);light('Amber',(-1.3,-1.1,.8),(-.3,1.8,1),32,(1,.57,.29),.35);light('RoomFill',(.4,-1.5,2),(-.5,1,1),26,(.43,.48,.58),2.8);camera((.75,-2.65,1.1),(-.35,1.6,1.25),65.47)
 assert ed.save_current_level();gc.collect()
u.log('V02 isolated review maps saved. Lead attaches existing generated player animation using player-fit.json; no gameplay hookup claimed.')
