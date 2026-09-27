"""Author inert traversal anchors for the AI owner. Does not change movement logic."""
import unreal as u,json,os,math
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox');sub=u.get_editor_subsystem(u.EditorActorSubsystem)
for a in sub.get_all_level_actors():
 if a.get_actor_label().startswith(('HANDOFF_','GUIDE_')):sub.destroy_actor(a)
nodes=[
 ('B00','Floor',(800,-900,-330),(0,0,1)),
 ('B01','Wall',(952,-900,-230),(-1,0,0)),
 ('B02','Wall',(952,-900,-100),(-1,0,0)),
 ('B03','Ceiling',(850,-900,-30),(0,0,-1)),
 ('B04','Ceiling',(850,-500,-30),(0,0,-1)),
 ('B05','Wall',(952,-500,-100),(-1,0,0)),
 ('B06','Wall',(952,-500,-230),(-1,0,0)),
 ('B07','Floor',(800,-500,-330),(0,0,1)),
]
route=[]
for i,(name,surface,pos,normal) in enumerate(nodes):
 a=sub.spawn_actor_from_class(u.TargetPoint,u.Vector(*pos));a.set_actor_label('HANDOFF_Surface_'+name+'_'+surface);a.set_folder_path('Traversal Handoff/Surface Route');a.tags=['Tallow.Traversal.Anchor','Surface='+surface,'Normal='+','.join(map(str,normal)),'Next='+nodes[(i+1)%len(nodes)][0]]
 route.append({'id':name,'actor':a.get_actor_label(),'surface':surface,'contact_cm':pos,'outward_normal':normal,'next':nodes[(i+1)%len(nodes)][0]})
 # Guides are editor-only and noncolliding. Anchors stay available at runtime for teammate logic.
 if i:
  prev=u.Vector(*nodes[i-1][2]);cur=u.Vector(*pos);delta=cur-prev;mid=(cur+prev)*.5
  line=sub.spawn_actor_from_class(u.StaticMeshActor,mid);line.set_actor_label('GUIDE_Surface_'+name);line.set_folder_path('Traversal Handoff/Editor Guides');line.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'));line.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);line.set_actor_scale3d(u.Vector(delta.length()/100,.025,.025));line.set_actor_rotation(u.MathLibrary.find_look_at_rotation(prev,cur),False);line.set_editor_property('is_editor_only_actor',True)
squeezes=[]
for name,entry,exit in [('ParlourDoor',(680,-325,0),(490,-325,0)),('KitchenDoor',(890,-936,0),(1060,-936,0)),('FilmRoomDoor',(680,-1105,390),(490,-1105,390))]:
 for side,p in [('Entry',entry),('Exit',exit)]:
  a=sub.spawn_actor_from_class(u.TargetPoint,u.Vector(*p));a.set_actor_label('HANDOFF_Squeeze_'+name+'_'+side);a.set_folder_path('Traversal Handoff/Squeeze Doors');a.tags=['Tallow.Traversal.Squeeze',name,side]
 squeezes.append({'id':name,'entry_floor_cm':entry,'exit_floor_cm':exit,'opening_width_cm':110,'opening_height_cm':240,'proposed_visual_envelope_cm':{'width':90,'height':215},'notes':'Fold limbs and duck head; sweep collision through the opening. Do not teleport or turn collision off.'})
# Keep the imposing 230 cm Host proxy; the squeeze profile is a future pose, not global model scaling.
for a in sub.get_all_level_actors():
 if a.get_actor_label()=='M3_Keeper_Visual':a.tags=['Tallow.Prototype.StaticVisual','StandingHeightCm=230','FinalLegPoseNotIntegrated']
assert ed.save_current_level()
output={'status':'Route anchors only; teammate must implement traversal and squeeze movement/animation','units':'Unreal centimetres','surface_route':route,'squeeze_doors':squeezes,'implementation_notes':['Contact points lie on the surface, not at actor centres. Offset by the active collision shape support distance along outward_normal.','Ground StateTree MoveTo cannot follow these wall and ceiling anchors. Switch to an explicit traversal movement mode and then return to ground navigation.','Interpolate surface orientation at transitions and sweep the full body; validate feet/legs separately.','Keep perception, capture and reset active; on interruption choose a reachable ground exit.','Full extended spider legs are not integrated in this proxy. Fold or plant them against nearby surfaces before enabling these routes.']}
open(os.path.join(ROOT,'Tools/traversal-routes.json'),'w').write(json.dumps(output,indent=2))
