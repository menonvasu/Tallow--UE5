"""PIE candle check: pickup exists, escaping the chair then interacting lights the candle, and the flame flickers.
Run: UnrealEditor Tallow.uproject -nullrhi -ExecCmds='py <abs path>'. Writes Tools/candle-validation.json."""
import unreal as u, json, os, time, traceback
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT=os.path.join(ROOT,'Tools','candle-validation.json')
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
S={'start':time.monotonic(),'playing':False,'t0':None,'done':False,'tests':[],'errors':[],'intens':[]}
def check(n,ok,**d): S['tests'].append(dict(name=n,passed=bool(ok),**d)); u.log('CANDLE_TEST %s: %s %s'%(n,bool(ok),d))
def finish():
    if S['done']: return
    S['done']=True; json.dump({'passed':all(t['passed'] for t in S['tests']) and not S['errors'],'tests':S['tests'],'errors':S['errors']},open(OUT,'w'),indent=1)
    ed.editor_request_end_play(); S['exit']=time.monotonic()
def tick(dt):
    try:
        now=time.monotonic()
        if S['done']:
            if now-S['exit']>2: u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
            return
        if not S['playing']:
            if now-S['start']>5: S['playing']=True; ed.editor_request_begin_play(); S['pie']=now
            return
        if S['t0'] is None:
            if now-S['pie']<4: return
            gw=u.EditorLevelLibrary.get_game_world()
            S['p']=u.GameplayStatics.get_player_character(gw,0); S['chair']=u.GameplayStatics.get_actor_of_class(gw,u.TallowChairEscape)
            S['pick']=u.GameplayStatics.get_actor_of_class(gw,u.TallowCandlePickup)
            check('candle pickup spawned in the level',S['pick'] is not None,location=str(S['pick'].get_actor_location()) if S['pick'] else None)
            S['cand']=S['p'].get_component_by_class(u.TallowCandleComponent); S['light']=S['p'].get_component_by_class(u.PointLightComponent)
            check('player has candle component and light',S['cand'] is not None and S['light'] is not None)
            check('candle starts out','OUT' in str(S['cand'].get_candle_state()),state=str(S['cand'].get_candle_state()))
            S['t0']=now; return
        t=now-S['t0']; c=S['chair']; p=S['p']
        if c.get_rock_count()<c.get_required_rocks():
            if c.is_at_rest() or c.is_timing_window_open(): c.attempt_rock()
            return
        if not S.get('picked'):
            S['picked']=True
            pl=S['pick'].get_actor_location(); p.set_actor_location(u.Vector(pl.x-80,pl.y,pl.z+60),False,True)
            S['pick'].on_interact(p); S['tp']=now; return
        if now-S['tp']<1.0: return
        st=str(S['cand'].get_candle_state())
        if not S.get('lit_checked'):
            S['lit_checked']=True
            check('picking up the candle lights it','LIT' in st,state=st)
            check('candle light is on and visible',S['light'].is_visible() and S['light'].get_editor_property('intensity')>0,intensity=S['light'].get_editor_property('intensity'),radius=S['light'].get_editor_property('attenuation_radius'))
        S['intens'].append(S['light'].get_editor_property('intensity'))
        if len(S['intens'])>40:
            v=S['intens']; check('flame flickers (intensity varies)',max(v)-min(v)>50,min=round(min(v)),max=round(max(v)))
            finish()
    except Exception:
        S['errors'].append(traceback.format_exc()); u.log_error('CANDLE_TEST '+S['errors'][-1]); finish()
H=u.register_slate_post_tick_callback(tick)
