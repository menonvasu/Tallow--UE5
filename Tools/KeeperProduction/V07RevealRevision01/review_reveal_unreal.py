"""Isolate the reveal tear in Unreal: morph alone (no clip), the real clip, and the clip with morphs off,
for the V07 mesh and the Reveal01 mesh. Head close-ups only. Uses the existing keeper review room; never
touches gameplay maps. Launch: UnrealEditor Tallow.uproject -RenderOffscreen ... -ExecCmds="py <this file>"
"""
import json,time,traceback
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/V07RevealRevision01/Unreal';OUT.mkdir(exist_ok=True)
ED=u.get_editor_subsystem(u.LevelEditorSubsystem);DEST='/Game/Presentation/KeeperRiggedV07/'
import os
MESHES=os.environ.get('TALLOW_REVEAL_MESHES','SK_KeeperRiggedV07,SK_KeeperRiggedV07_Reveal01').split(',')
CASES=[('morph-only',None,{'HOOD_OPEN':1.0,'FULL_HORROR':1.0},False),('clip',1.4,{},False),('clip-no-morphs',1.4,{},True)]
SAMPLES=[(m,c) for m in MESHES for c in CASES]
S={'phase':'load','time':time.monotonic(),'i':0,'observed':[],'errors':[]}

def finish(error=None):
    if error:S['errors'].append(error);u.log_error(error)
    (OUT/'review.json').write_text(json.dumps({'observed':S['observed'],'errors':S['errors']},indent=2))
    ED.editor_request_end_play();S.update(phase='finished',time=time.monotonic())

def pose(sc,mesh,case):
    name,at,morphs,no_morphs=case
    sc.set_skeletal_mesh_asset(u.load_asset(DEST+mesh))
    sc.set_editor_property('disable_morph_target',no_morphs)
    if at is None:
        sc.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE);sc.set_animation(None)
        for key,value in morphs.items():sc.set_morph_target(key,value,True)
    else:
        sc.clear_morph_targets();sc.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        sc.set_animation(u.load_asset(DEST+'AN_Keeper_JumpscareDeath'));sc.set_position(at,False);sc.set_play_rate(0)

def tick(dt):
    try:
        now=time.monotonic()
        if S['phase']=='finished':
            if now-S['time']>3:u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
        elif S['phase']=='load' and now-S['time']>8:
            assert ED.load_level('/Game/Presentation/Maps/Lvl_KeeperContactReview_v1')
            S.update(phase='warmup',time=now)
        elif S['phase']=='warmup' and now-S['time']>6:
            ED.editor_request_begin_play();S.update(phase='pie',time=now)
        elif S['phase']=='pie' and now-S['time']>6:
            w=u.EditorLevelLibrary.get_game_world();pc=u.GameplayStatics.get_player_controller(w,0);pawn=u.GameplayStatics.get_player_pawn(w,0)
            if pawn:pawn.set_actor_hidden_in_game(True)
            cam=u.GameplayStatics.get_all_actors_of_class(w,u.CameraActor)[0];pc.set_view_target_with_blend(cam,0)
            keeper=u.GameplayStatics.get_all_actors_of_class(w,u.SkeletalMeshActor)[0]
            S.update(world=w,pc=pc,cam=cam,keeper=keeper,phase='pose',time=now)
        elif S['phase']=='pose' and now-S['time']>1.5:
            if S['i']>=len(SAMPLES):finish();return
            mesh,case=SAMPLES[S['i']];sc=S['keeper'].skeletal_mesh_component;pose(sc,mesh,case)
            S.update(phase='aim',time=now)
        elif S['phase']=='aim' and now-S['time']>1.5:
            sc=S['keeper'].skeletal_mesh_component;head=sc.get_socket_location('head')
            # Keeper faces +Y in this room; frame the hood from the front, slightly above.
            eye=head+u.Vector(15,135,15);S['cam'].set_actor_location(eye,False,False)
            S['cam'].set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,head+u.Vector(0,0,-12)),False);S['cam'].camera_component.set_field_of_view(55)
            S.update(phase='capture',time=now)
        elif S['phase']=='capture' and now-S['time']>1.5:
            mesh,case=SAMPLES[S['i']];name=f"{S['i']:02d}-{mesh.split('_')[-1]}-{case[0]}.png";path=OUT/name;path.unlink(missing_ok=True)
            u.SystemLibrary.execute_console_command(S['pc'],'HighResShot 1280x720 filename='+str(path))
            sc=S['keeper'].skeletal_mesh_component
            S['pending']={'image':name,'mesh':sc.get_skeletal_mesh_asset().get_name(),'case':case[0],
                          'hood_open':sc.get_morph_target('HOOD_OPEN'),'head':str(sc.get_socket_location('head'))}
            S.update(phase='await',time=now)
        elif S['phase']=='await':
            path=OUT/S['pending']['image']
            if path.is_file() and path.stat().st_size>100:S['observed'].append(S['pending']);S.update(phase='pose',i=S['i']+1,time=now)
            elif now-S['time']>15:finish('Capture timed out: '+str(path))
    except Exception:finish(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
