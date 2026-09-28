"""PIE hunt check: does the Keeper react to noise and close on the player? Logs state/position every 0.5 s.
Run: UnrealEditor Tallow.uproject -nullrhi -ExecCmds='py <abs path>'. Writes Tools/keeper-hunt-report.json."""
import unreal as u, json, os, time, traceback
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); OUT=os.path.join(ROOT,'Tools','keeper-hunt-report.json')
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
S={'start':time.monotonic(),'playing':False,'t0':None,'done':False,'samples':[],'noise_at':[8.0,14.0,20.0],'emitted':0,'errors':[]}
def finish():
    if S['done']: return
    S['done']=True; json.dump({k:S[k] for k in ('samples','errors')},open(OUT,'w'),indent=1)
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
            gw=u.EditorLevelLibrary.get_game_world(); S['gw']=gw
            S['k']=u.GameplayStatics.get_actor_of_class(gw,u.TallowCreature); S['p']=u.GameplayStatics.get_player_character(gw,0)
            wp=gw.get_path_name()
            S['noise']=next((o for o in (u.find_object(None,wp+':TallowNoiseSubsystem_%d'%i) for i in range(4)) if o),None)
            u.log('KEEPER_HUNT noise subsystem found: %s'%bool(S['noise']))
            S['chair']=u.GameplayStatics.get_actor_of_class(gw,u.TallowChairEscape)
            u.SystemLibrary.execute_console_command(gw,'Log LogStateTree Verbose')
            S['t0']=now; return
        t=now-S['t0']; k=S['k']; p=S['p']
        c=S['chair']
        if c and c.get_rock_count()<c.get_required_rocks() and not S.get('free'):
            if c.is_at_rest() or c.is_timing_window_open(): c.attempt_rock()
            if c.get_rock_count()>=c.get_required_rocks(): S['free']=True; u.log('KEEPER_HUNT escaped chair at t=%.1f'%t)
        while S['emitted']<len(S['noise_at']) and t>=S['noise_at'][S['emitted']]:
            loc=p.get_actor_location()+u.Vector(0,-120.0*S['emitted'],0)
            if S['noise']: S['noise'].emit_noise(u.TallowNoiseType.CHAIR_ROCK,loc,p,1.5,True)
            else: u.AISense_Hearing.report_noise_event(S['gw'],p.get_actor_location(),1.0,p,3000.0,'')
            S['emitted']+=1
            u.log('KEEPER_HUNT noise emitted at t=%.1f'%t)
        if not S['samples'] or t-S['samples'][-1]['t']>=0.5:
            kl=k.get_actor_location(); pl=p.get_actor_location()
            smp={'t':round(t,1),'state':str(k.get_editor_property('current_state')).split('.')[-1].rstrip('>').split(':')[0],'dist':round((kl-pl).length()),'keeper':[round(kl.x),round(kl.y),round(kl.z)],'speed':round(k.get_velocity().length()),'heard':[round(x) for x in (lambda v:(v.x,v.y,v.z))(k.get_controller().get_editor_property('last_heard_noise_location'))]}
            S['samples'].append(smp); u.log('KEEPER_HUNT '+json.dumps(smp))
        if t>=24 and not S.get('close'):
            S['close']=True; kl=k.get_actor_location()
            p.set_actor_location(u.Vector(kl.x+200,kl.y,kl.z+10),False,True); u.log('KEEPER_HUNT player placed 2.1m from keeper in the dark at t=%.1f'%t)
        if t>34: finish()
    except Exception:
        S['errors'].append(traceback.format_exc()); u.log_error('KEEPER_HUNT '+S['errors'][-1]); finish()
H=u.register_slate_post_tick_callback(tick)
