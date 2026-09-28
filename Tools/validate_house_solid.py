"""PIE check that each storey is solid: player placed on each floor must not fall through, and in-game traces
must hit the outer walls. Writes Tools/house-solid.json."""
import unreal as u, json, os, time, traceback
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT=os.path.join(ROOT,'Tools','house-solid.json')
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
S={'start':time.monotonic(),'playing':False,'gw':None,'done':False,'res':[],'i':0}
DROPS=[('basement',(300,-900,-250)),('ground',(300,-900,100)),('ground hall',(780,-300,100)),('upper',(300,-900,480)),('upper east',(1300,-900,480))]
TRACES=[('ground front wall',(300,-900,150),(300,500,150)),('ground floor slab',(300,-900,60),(300,-900,-100)),('upper front wall',(300,-900,550),(300,500,550)),('upper floor slab',(300,-900,440),(300,-900,300))]
def fin():
    S['done']=True; json.dump(S['res'],open(OUT,'w'),indent=1); ed.editor_request_end_play(); S['exit']=time.monotonic()
def tick(dt):
    try:
        now=time.monotonic()
        if S['done']:
            if now-S['exit']>2: u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
            return
        if not S['playing']:
            if now-S['start']>5: S['playing']=True; ed.editor_request_begin_play(); S['pie']=now
            return
        if S['gw'] is None:
            if now-S['pie']<4: return
            S['gw']=u.EditorLevelLibrary.get_game_world(); S['p']=u.GameplayStatics.get_player_character(S['gw'],0)
            ch=u.GameplayStatics.get_actor_of_class(S['gw'],u.TallowChairEscape)
            while ch.get_rock_count()<ch.get_required_rocks(): break
            for lab,a,b in TRACES:
                h=u.SystemLibrary.line_trace_single(S['gw'],u.Vector(*a),u.Vector(*b),u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[S['p']],u.DrawDebugTrace.NONE,True)
                r={'trace':lab,'hit':h is not None and h.to_tuple()[0]}; S['res'].append(r); u.log('SOLID %s'%r)
            S['t']=now; return
        if S['i']<len(DROPS):
            lab,pos=DROPS[S['i']]
            if 'placed' not in S:
                p=S['p']; p.detach_from_actor(); cm=p.character_movement; cm.set_movement_mode(u.MovementMode.MOVE_FALLING)
                p.set_actor_location(u.Vector(*pos),False,True); S['placed']=now; return
            if now-S['placed']>2.5:
                z=S['p'].get_actor_location().z; r={'drop':lab,'start_z':pos[2],'end_z':round(z),'fell_through':z<pos[2]-250}
                S['res'].append(r); u.log('SOLID %s'%r); S['i']+=1; S.pop('placed')
            return
        fin()
    except Exception:
        u.log_error('SOLID '+traceback.format_exc()); fin()
H=u.register_slate_post_tick_callback(tick)
