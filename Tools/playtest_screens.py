"""Rendered PIE playtest: escape the chair, pick up the candle, then screenshot from the player's eyes in a few rooms.
Run with rendering: UnrealEditor Tallow.uproject -RenderOffscreen -ExecCmds='py <abs path>'. Shots go to Tools/previews/playtest/."""
import unreal as u, os, time, traceback
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT=os.path.join(ROOT,'Tools','previews','playtest'); os.makedirs(OUT,exist_ok=True)
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
SPOTS=[('held_candle',(330,-450,-240),(-10,-60)),('basement_chair_room',(330,-480,-240),(-5,-100)),('basement_corridor',(780,-900,-240),(-5,180)),('ground_hall',(780,-300,110),(-5,-90)),('ground_room',(300,-1000,110),(-5,90)),('upper_room',(300,-1000,490),(-5,60))]
S={'start':time.monotonic(),'go':False,'t0':None,'i':0,'done':False}
def tick(dt):
    try:
        now=time.monotonic()
        if S['done']:
            if now-S['exit']>8: u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
            return
        if not S['go']:
            if now-S['start']>6: S['go']=True; ed.editor_request_begin_play(); S['pie']=now
            return
        if S['t0'] is None:
            if now-S['pie']<8: return
            gw=u.EditorLevelLibrary.get_game_world(); S['p']=u.GameplayStatics.get_player_character(gw,0); S['pc']=u.GameplayStatics.get_player_controller(gw,0)
            S['chair']=u.GameplayStatics.get_actor_of_class(gw,u.TallowChairEscape); S['pick']=u.GameplayStatics.get_actor_of_class(gw,u.TallowCandlePickup)
            S['t0']=now; return
        c=S['chair']
        if c.get_rock_count()<c.get_required_rocks():
            if c.is_at_rest() or c.is_timing_window_open(): c.attempt_rock()
            return
        if not S.get('lit'): S['pick'].on_interact(S['p']); S['lit']=True; S['next']=now+1.5; return
        if now<S['next']: return
        if S['i']<len(SPOTS):
            name,pos,rot=SPOTS[S['i']]
            if not S.get('placed'):
                S['p'].set_actor_location(u.Vector(*pos),False,True); S['pc'].set_control_rotation(u.Rotator(roll=0.0,pitch=float(rot[0]),yaw=float(rot[1]))); S['placed']=True; S['next']=now+2.5; return
            u.SystemLibrary.execute_console_command(S['p'],'HighResShot 1280x720 filename=%s'%os.path.join(OUT,name+'.png'))
            S['i']+=1; S['placed']=False; S['next']=now+4.0; return
        S['done']=True; ed.editor_request_end_play(); S['exit']=now
    except Exception:
        u.log_error('PLAYTEST '+traceback.format_exc()); S['done']=True; S['exit']=time.monotonic()
H=u.register_slate_post_tick_callback(tick)
