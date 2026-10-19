"""Wire the existing game systems to the authored presentation, in the integration copy only.

TALLOW_STAGING=1 permits the earlier art while production imports are being reviewed.
The final pass refuses missing V07/V05/candle assets.
"""
import json
import os
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SAVED = ROOT/'Tallow/Saved/Integration'
SAVED.mkdir(parents=True, exist_ok=True)
E = u.EditorAssetLibrary
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
A = u.get_editor_subsystem(u.EditorActorSubsystem)
GAME = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
MENU = '/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation'
ENDING = '/Game/Presentation/Cinematics/Maps/Lvl_OutroPresentation'
STAGING = os.environ.get('TALLOW_STAGING') == '1'
ROUTE = json.loads((ROOT/'Tallow/SourceAssets/Production/HouseIntegrationV01/route-plan.json').read_text())
ASSETS = '/Game/Presentation/Environment/HouseIntegrationV01/Architecture/'
TAG = 'Tallow.Integration'

def asset(path, fallback=None):
    if E.does_asset_exist(path):
        result = u.load_asset(path)
        assert result, path
        return result
    assert STAGING and fallback, 'Required production asset missing: '+path
    result = u.load_asset(fallback)
    assert result, fallback
    return result

def spawn(cls, label, location=(0,0,0), yaw=0):
    a = A.spawn_actor_from_class(cls, u.Vector(*location), u.Rotator(pitch=0,yaw=yaw,roll=0))
    assert a, label
    a.set_actor_label(label)
    a.tags = [u.Name(TAG)]
    a.set_folder_path('Integration')
    return a

def component(actor, name):
    return next(c for c in actor.get_components_by_class(u.SceneComponent) if c.get_name() == name)

def director(scene):
    for a in A.get_all_level_actors():
        if isinstance(a, u.TallowPlayDirector): A.destroy_actor(a)
    d = spawn(u.TallowPlayDirector, 'TALLOW_PlayDirector')
    d.set_editor_property('scene',scene)
    for prop,path in [('game_map',GAME),('menu_map',MENU),('ending_map',ENDING)]:
        d.set_editor_property(prop,asset(path))
    d.set_editor_property('menu_sequence',asset('/Game/Presentation/Cinematics/LS_MainMenu_Stillness'))
    d.set_editor_property('intro_sequence',asset('/Game/Presentation/Cinematics/LS_Intro_Wake'))
    d.set_editor_property('outro_sequence',asset('/Game/Presentation/Cinematics/LS_Outro_LastFlame'))
    return d

assert ED.load_level(GAME)
for a in A.get_all_level_actors():
    if TAG in [str(t) for t in a.tags]: A.destroy_actor(a)
existing = {a.get_name():a for a in A.get_all_level_actors()}
d = director(u.TallowPlayScene.GAME)
d.set_editor_property('player_hands',asset('/Game/Presentation/PlayerInteractionV05/SK_PlayerV05_Partial', '/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Partial'))
for prop,new,old in [('hold_clip','Hold','Hold'),('rest_clip','Rest','Rest'),('cup_clip','Cup','Cup'),
                     ('reach_clip','Reach','Interact'),('relight_clip','Relight','Relight'),
                     ('walk_clip','Walk','Hold'),('sprint_clip','Sprint','Hold'),('crouch_clip','Crouch','Hold'),
                     ('tool_hold_clip','ToolHold','Hold'),('pickup_clip','Pickup','Hold'),
                     ('bound_clip','IntroBound','SeatedBoundIdle'),('escape_clip','ChairEscape','Rest')]:
    d.set_editor_property(prop, asset('/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_'+new,
        '/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_'+old))
for prop,name in [('crowbar_mesh','Crowbar'),('fuse_mesh','Fuse'),('match_mesh','Match')]:
    d.set_editor_property(prop,asset('/Game/Presentation/ToolProps/SM_ToolV02_'+name))
for prop,name in [('crowbar_pry_clip','CrowbarPry'),('fuse_insert_clip','FuseInsert'),('cup_clip','CupShield_03'),('reach_clip','Reach_01'),
                  ('walk_clip','Walk_01'),('sprint_clip','Sprint_01'),('crouch_clip','Crouch_01'),
                  ('tool_hold_clip','ToolHold_01'),('crowbar_pry_clip','CrowbarPry_01'),('fuse_insert_clip','FuseInsert_01'),('pickup_clip','Pickup_01')]:
    d.set_editor_property(prop,asset('/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_'+name))
d.set_editor_property('relight_clip',asset('/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_RelightMatch_02'))
match=json.loads((ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision02/tool-match-contract.json').read_text())['match']
# Crowbar and fuse ride hand_l from the ToolHold_01 grip; the contract places them in camera space at its first frame.
tools=json.loads((ROOT/'Tallow/SourceAssets/Player/InteractionV05/ToolRevision01/tool-hold-contract.json').read_text())['tools']
for prop,name in [('crowbar_camera_transform','SM_ToolV02_Crowbar'),('fuse_camera_transform','SM_ToolV02_Fuse')]:
    t=tools[name];d.set_editor_property(prop,u.Transform(location=u.Vector(*t['camera_location_cm']),rotation=u.MathLibrary.quat_rotator(u.Quat(*t['camera_quaternion_xyzw'])),scale=u.Vector(1,1,1)))
d.set_editor_property('match_camera_transform',u.Transform(location=u.Vector(*match['camera_location_cm']),rotation=u.MathLibrary.quat_rotator(u.Quat(*match['camera_quaternion_xyzw'])),scale=u.Vector(1,1,1)))
d.set_editor_property('candle_mesh',asset('/Game/Presentation/CandleV02/SM_CandleV02_Crooked','/Game/Props/Candle/wooden_candlestick_1k'))
d.set_editor_property('flame_mesh',asset('/Game/Presentation/Flame/SM_TallowFlame'))
d.set_editor_property('flame_material',asset('/Game/Presentation/Flame/M_TallowFlame'))
d.set_editor_property('wick_position',u.Vector(.4,0,25.5))
d.set_editor_property('candle_camera_transform',u.Transform(location=u.Vector(44.5,22.5,-27.5),rotation=u.Rotator(pitch=0,yaw=90,roll=0),scale=u.Vector(1,1,1)))

# Author candidate pools once; the runtime owner shuffles within reachable floor groups.
for row in ROUTE['crowbar_candidates']:
    a = spawn(u.TargetPoint, row['id'], row['location'])
    a.tags = [u.Name(TAG),u.Name('Tallow.Spawn.Crowbar')]
    a.set_editor_property('is_editor_only_actor',False)
for row in ROUTE['fuse_candidates']:
    a = spawn(u.TargetPoint,row['id'],row['location'])
    a.tags = [u.Name(TAG),u.Name('Tallow.Spawn.Fuse.'+row['floor'].title())]
    a.set_editor_property('is_editor_only_actor',False)

# Keep the frame opening hollow for collision (a convex hull would invisibly block the doorway).
for suffix in ['Wide','Narrow']:
    frame_mesh=asset(ASSETS+'SM_HouseV01_GateFrame_'+suffix)
    frame_mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    for i in range(frame_mesh.get_num_sections(0)):
        u.get_editor_subsystem(u.StaticMeshEditorSubsystem).enable_section_collision(frame_mesh,True,0,i)
    E.save_loaded_asset(frame_mesh,False)

# Move, reskin and reuse Vasu's working door/plank actors and their existing references.
door = existing['BP_TallowDoor_C_3']
door.set_actor_scale3d(u.Vector(1,1,1))
door.set_actor_location_and_rotation(u.Vector(585,-1120,-330),u.Rotator(pitch=0,yaw=90,roll=0),False,True)
frame = component(door,'DoorFrameMesh'); frame.set_static_mesh(asset(ASSETS+'SM_HouseV01_GateFrame_Wide'))
hinge = component(door,'HingePivot'); hinge.set_relative_location(u.Vector(0,0,0),False,False)
leaf = component(door,'DoorMesh'); leaf.set_static_mesh(asset(ASSETS+'SM_HouseV01_GateDoor_Wide'))
leaf.set_relative_scale3d(u.Vector(1,1,1)); leaf.set_relative_location(u.Vector(0,0,0),False,False)
door.set_editor_property('open_angle',100)
planks = sorted([a for a in existing.values() if isinstance(a,u.TallowPlank)],key=lambda a:a.get_name())
for i,p in enumerate(planks):
    p.set_actor_location_and_rotation(u.Vector(578,-1040,-280+i*45),u.Rotator(pitch=0,yaw=90,roll=(-9 if i%2 else 8)),False,True)
    pc = p.get_component_by_class(u.StaticMeshComponent)
    # A centred timber plank now spans the full arch rather than the tiny testing cube.
    pc.set_static_mesh(asset(ASSETS+'SM_HouseV01_SplinteredBoard','/Engine/BasicShapes/Cube'))
    pc.set_relative_scale3d(u.Vector(1,1,1))
    pc.set_material(0,asset('/Game/Presentation/Environment/HeroRoomV02/Materials/M_HeroRoomV02_BlackenedTimber'))
    p.set_editor_property('target_door',door)
door.set_editor_property('barricade_planks',planks)
# The redundant old test door stays in the copy as hidden data, with no collision or AI trigger.
old = existing['BP_TallowDoor_C_1']; old.set_actor_hidden_in_game(True); old.set_actor_enable_collision(False)
# Retire the M3 greybox climb/step test blocks the same way: never wired to a mechanic (Tools/README.md: remove them then).
# The two stair-approach steps stay: the basement stairs start 48cm from the hall wall, so these 20/39cm side steps are
# the only way onto them. They get the basement concrete so they read as built steps, not greybox.
for a in A.get_all_level_actors():
    if a.get_actor_label() in ('CLIMB_Workshop_Ledge','TEST_STEP_Workshop','CLIMB_Study_Ledge','TEST_STEP_Study','CLIMB_ToyRoom_Ledge','TEST_STEP_ToyRoom'):
        a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
    # Vasu's child-gap areas mark spaces a child fits but the phase-one Keeper must not path into. M3_ChildGap_0 sat on the
    # old house's opening and now clipped the dining-to-stairs passage, splitting the ground floor in two for the Keeper.
    # Its real job in this house is under the main stair half-landing (~190cm headroom): you fit, the Keeper wedges.
    if a.get_actor_label() == 'M3_ChildGap_0':
        a.set_actor_location(u.Vector(800,-800,90), False, False)
        a.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(150,100,90))
    if a.get_actor_label() in ('Basement_StairApproach_Low','Basement_StairApproach_High'):
        a.set_actor_hidden_in_game(False); a.set_actor_enable_collision(True)
        a.static_mesh_component.set_material(0,asset('/Game/Environment/HouseMaterials/MI_BasementConcrete'))
        # Run them back to the hall wall (south face y=-672): the 58cm slot left in front of the doorway wedged
        # both capsules (Keeper 68cm, player 60cm). You now step straight up out of the doorway.
        o,e = a.get_actor_bounds(False); s = a.get_actor_scale3d()
        a.set_actor_scale3d(u.Vector(s.x, s.y*(814-673)/(2*e.y), s.z))
        o,e = a.get_actor_bounds(False); a.add_actor_world_offset(u.Vector(0, -743.5-o.y, 0), False, False)
for row in ROUTE['native_gates']:
    gate = spawn(u.TallowRouteGate,row['label'],row['hinge_location'],row['yaw'])
    gate.requires_power = row['requires_power']
    gate.door.set_static_mesh(asset(row['door_mesh']))
    gate.frame.set_static_mesh(asset(row['frame_mesh']))
    gate.door.set_relative_scale3d(u.Vector(*row.get('mesh_scale',[1,1,1])))
    gate.frame.set_relative_scale3d(u.Vector(*row.get('mesh_scale',[1,1,1])))
    gate.set_editor_property('open_angle',row.get('open_angle',100))

# Close the openings Tools/PlayIntegration/audit_architecture.py found: the front bay window (x 1180-1420) had no glass,
# walls or boards on either floor, an open black void you could walk out through, and the front door was two boards
# with no door behind them. Board the bay over like every other window and hang a shut door behind the boards.
def fit(mesh,label,lo,hi,roll=0.):
    a = spawn(u.StaticMeshActor,label,[(lo[i]+hi[i])/2 for i in range(3)])
    c = a.static_mesh_component; c.set_static_mesh(mesh); c.set_collision_profile_name('BlockAll')
    b = mesh.get_bounding_box(); size = [b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z]
    a.set_actor_scale3d(u.Vector(*[(hi[i]-lo[i])/max(size[i],.01) for i in range(3)]))
    o,e = a.get_actor_bounds(False); a.add_actor_world_offset(u.Vector(*[(lo[i]+hi[i])/2-v for i,v in enumerate((o.x,o.y,o.z))]),False,False)
    if roll: a.add_actor_local_rotation(u.Rotator(pitch=0,yaw=0,roll=roll),False,False)
    return a
planking = asset('/Game/Environment/HouseMaterials/MI_Boards'); timber = asset('/Game/Presentation/Environment/HeroRoomV02/Materials/M_HeroRoomV02_BlackenedTimber')
for floor,z in [('Ground',0),('Upper',390)]:
    fit(asset('/Engine/BasicShapes/Cube'),'BayBoarded_'+floor+'_Backing',[1150,-2,z+38],[1450,6,z+352]).static_mesh_component.set_material(0,planking)
    for i,(h,roll) in enumerate([(95,-3),(150,2.5),(215,-1.5),(285,3)]):
        board = fit(asset(ASSETS+'SM_HouseV01_SplinteredBoard','/Engine/BasicShapes/Cube'),'BayBoarded_%s_Board%d'%(floor,i),[1160,-14,z+h-9],[1440,-8,z+h+9],roll)
        board.static_mesh_component.set_material(0,timber)
fit(asset(ASSETS+'SM_HouseV01_GateDoor_Wide'),'FrontDoor_Leaf',[700,-6,0],[860,6,240])
# Ty's playtest: the basement stairs start 48 cm behind the hall wall, so you could barely get onto them. Open the wall in
# front of the flight and its side steps (x 1170-1470) so the stairs start straight off the hall; rebuild the rest around it.
for a in A.get_all_level_actors():
    if a.get_actor_label().startswith('HI_SM_HouseV01_Basement_Wall_x6p50'):
        wall_material = a.static_mesh_component.get_material(0); a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
        for i,(x0,x1,z0,z1) in enumerate([(952,1170,-330,-30),(1470,1582,-330,-30),(1170,1470,-95,-30)]):
            fit(asset('/Engine/BasicShapes/Cube'),'StairHallWall_%d'%i,[x0,-672,z0],[x1,-627,z1]).static_mesh_component.set_material(0,wall_material)

box = existing['BP_TallowFuseBox_C_1']
# Mount the authored switchboard on the east utility wall, facing into the room.
box.set_actor_location_and_rotation(u.Vector(1532,-320,-210),u.Rotator(pitch=0,yaw=90,roll=0),False,True)
box.set_actor_scale3d(u.Vector(1,1,1))
component(box,'BoxMesh').set_static_mesh(asset('/Game/Presentation/Environment/HouseIntegrationV01/TaskProps/SM_HouseTask_Switchboard'))
component(box,'BoxMesh').set_editor_property('override_materials',[])
component(box,'BoxMesh').set_relative_scale3d(u.Vector(1,1,1))
# Existing fuse count/events still own these three visible insertions.
for i,x in enumerate([-13,0,13],1):
    slot=component(box,'FuseMesh'+str(i))
    slot.set_relative_location(u.Vector(x,13.7,0),False,False)
    if E.does_asset_exist('/Game/Presentation/ToolProps/SM_ToolV02_Fuse'):
        slot.set_static_mesh(asset('/Game/Presentation/ToolProps/SM_ToolV02_Fuse'))
        slot.set_relative_scale3d(u.Vector(1,1,1))

# Weak electric practicals are the visible reward; moonlit navigation remains available before power.
for a in A.get_all_level_actors():
    if a.get_actor_label() in ['HI_DiningPractical','HI_ToyShelfFill']:
        a.tags=list(a.tags)+[u.Name('Tallow.PowerLight')]


# Fit the real chair around the established seat and escape location.
chair = existing['BP_TallowChairEscape_C_4']
seat = component(chair,'Seat Attach Point'); seat_world = seat.get_world_transform()
chair_mesh = component(chair,'Chair Mesh')
chair_mesh.set_static_mesh(asset('/Game/Presentation/Environment/HeroProps/RestraintChair/SM_RestraintChair'))
chair_mesh.set_editor_property('override_materials',[])
chair_mesh.set_relative_scale3d(u.Vector(.7,.7,.7))
chair_mesh.set_relative_location_and_rotation(u.Vector(25,25,0),u.Rotator(pitch=0,yaw=90,roll=0),False,False)
seat.set_world_transform(seat_world,False,False)
pickup = existing['BP_TallowCandlePickup_C_0']
pickup.get_component_by_class(u.StaticMeshComponent).set_static_mesh(d.get_editor_property('candle_mesh'))
light = pickup.get_component_by_class(u.PointLightComponent)
light.set_relative_location(d.get_editor_property('wick_position'),False,False)
light.set_editor_property('intensity_units',u.LightUnits.LUMENS);light.set_intensity(d.get_editor_property('candle_intensity_lumens'))

# Native draft / hiding mechanics get real locations in this map.
for row in ROUTE.get('draft_candidates',[]):
    a = spawn(u.TallowDraftVolume,row['id'],row['location'])
    a.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(*row['extent']))
for row in ROUTE.get('measured_hiding_contracts',[]):
    a = spawn(u.TallowHideVolume,row['id'],row['center'])
    hide_box=a.get_component_by_class(u.BoxComponent)
    hide_box.set_box_extent(u.Vector(*row.get('extent',[45,35,55])))
    hide_box.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
    hide_box.set_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY,u.CollisionResponseType.ECR_BLOCK)
    component(a,'Hiding View Point').set_world_location_and_rotation(u.Vector(*row['view']),u.Rotator(pitch=0,yaw=row.get('yaw',0),roll=0),False,False)
    component(a,'Exit Point').set_world_location(u.Vector(*row['exit']),False,False)
    a.set_editor_property('hiding_spot_type',u.TallowHidingSpotType.UNDER_BED)

point = ROUTE['final_room']['exit_threshold']
escape = spawn(u.TallowEscapePoint,'Collection_ReleaseLatch',[100,-1250,480])
escape.latch.set_static_mesh(asset('/Game/Presentation/Environment/HouseIntegrationV01/TaskProps/SM_HouseTask_EscapeLatch'))
window = spawn(u.StaticMeshActor,'Collection_EscapeWindow',[100,-1254,510])
window.static_mesh_component.set_static_mesh(asset('/Game/Presentation/Environment/HouseIntegrationV01/TaskProps/SM_HouseTask_EscapeWindowFrame'))

keeper = existing['BP_TallowCreature_C_0']
keeper.get_component_by_class(u.CapsuleComponent).set_capsule_size(34,96,False)  # no wider than the 35cm navmesh agent, or it snags on jambs the path hugs
animation = keeper.get_component_by_class(u.TallowCreatureAnimationComponent)
# Reveal02 is V07 with the hood seam split so the importer cannot weld the two halves together.
# V09: repaired body (seams welded, no tears in crawl poses) and the new maw, on the V07 skeleton.
animation.keeper_mesh = asset('/Game/Presentation/KeeperRiggedV07/SK_KeeperRiggedV07_Mouth09','/Game/Presentation/KeeperRiggedV07/SK_KeeperRiggedV07_Reveal02')
animation.use_presentation_rig = True
animation.keeper_mesh_scale = 1.15  # ~2.75 m standing: crouched and crawling most of the time, 2.4 m did not tower over the child
animation.keeper_mesh_yaw = -90
animation.keeper_mesh_offset = u.Vector(0,0,-96)
animation.host_bob_height = 0
animation.chase_speed = 320
animation.patrol_speed = 65
animation.investigate_speed = 65
animation.walk_reference_speed = 36.666667
animation.chase_reference_speed = 224
animation.wall_reference_speed = 13.235294
animation.ceiling_reference_speed = 5.833333
animation.sneak_reference_speed = 13.333333
for prop,name in [('host_walk_animation','HostWalk'),('investigation_walk_animation','Walk'),('aware_idle_animation','Listen'),('reset_animation','Reset'),('wall_enter_animation','WallEnter'),('wall_exit_animation','WallExit'),('ceiling_enter_animation','CeilingEnter'),('ceiling_exit_animation','CeilingExit')]:
    animation.set_editor_property(prop,asset('/Game/Presentation/KeeperV08/AN_Keeper_'+name,'/Game/Presentation/KeeperRiggedV07/AN_Keeper_'+name))
for prop,name in [('idle_animation','Idle'),('walk_animation','Walk'),('chase_animation','RunChase'),
                  ('investigate_animation','SearchUnder'),('capture_animation','JumpscareDeath'),('wall_animation','WallCrawl'),
                  ('ceiling_animation','CeilingCrawl'),('squeeze_animation','Squeeze'),('reveal_animation','Reveal'),
                  ('sneak_animation','Sneak'),('search_under_animation','SearchUnder'),('search_over_animation','SearchOver')]:
    animation.set_editor_property(prop,asset('/Game/Presentation/KeeperV08/AN_Keeper_'+name,
        '/Game/Presentation/KeeperRiggedV07/AN_Keeper_'+name))
# V08 jumpscares: drop-from-above when caught on the ceiling, hands-first when hiding; strike times from its contract.
capture=json.loads((ROOT/'Tallow/SourceAssets/Keeper/V08Animation/animation-contract.json').read_text())['capture']['variants']
animation.set_editor_property('ceiling_capture_animation',asset('/Game/Presentation/KeeperV08/AN_Keeper_Jumpscare_Ceiling'))
animation.set_editor_property('grab_capture_animation',asset('/Game/Presentation/KeeperV08/AN_Keeper_Jumpscare_Grab'))
for prop,name in [('capture_strike_time','JumpscareDeath'),('ceiling_capture_strike_time','Jumpscare_Ceiling'),('grab_capture_strike_time','Jumpscare_Grab')]:
    animation.set_editor_property(prop,capture[name]['strike'])
stepped = keeper.get_component_by_class(u.TallowSteppedAnimationComponent)
if stepped: stepped.set_editor_property('stepped',False)
assert ED.save_current_level()

for path,scene in [(MENU,u.TallowPlayScene.MAIN_MENU),(ENDING,u.TallowPlayScene.ENDING)]:
    assert ED.load_level(path)
    director(scene)
    assert ED.save_current_level()

# Audit controls from actual known contexts; registry class enumeration can miss unloaded plugins.
controls = {}
for path in ['/Game/Input/IMC_Default','/Game/Input/IMC_MouseLook']:
    context = u.load_asset(path)
    controls[path] = [{'action':m.action.get_name(),'key':str(u.InputLibrary.key_get_display_name(m.key))} for m in context.get_editor_property('default_key_mappings').get_editor_property('mappings') if m.action]
(SAVED/'configured-flow.json').write_text(json.dumps({'staging':STAGING,'maps':[MENU,GAME,ENDING],'controls':controls},indent=2))
u.log('TALLOW_FLOW_CONFIGURED '+str({'staging':STAGING,'maps':[MENU,GAME,ENDING]}))
