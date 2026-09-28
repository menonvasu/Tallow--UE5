"""PIE: every leg of the Keeper patrol loop must have a complete navmesh path. Writes Tools/patrol-paths.json."""
import unreal as u, json, os, time
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
S={'s':time.monotonic(),'go':False}
def tick(dt):
    now=time.monotonic()
    if not S['go']:
        if now-S['s']>5: S['go']=True; ed.editor_request_begin_play(); S['p']=now
        return
    if now-S['p']<6: return
    gw=u.EditorLevelLibrary.get_game_world(); k=u.GameplayStatics.get_actor_of_class(gw,u.TallowCreature)
    pts=[a for a in u.GameplayStatics.get_all_actors_of_class(gw,u.TallowPatrolPoint)]
    res=[]
    for a in pts:
        for b in a.get_editor_property('next_points'):
            path=u.NavigationSystemV1.find_path_to_location_synchronously(gw,a.get_actor_location(),b.get_actor_location(),k)
            ok=bool(path) and path.is_valid() and not path.is_partial()
            res.append({'from':a.get_actor_label(),'to':b.get_actor_label(),'ok':ok,'points':len(path.get_editor_property('path_points')) if path else 0})
    json.dump(res,open(os.path.join(ROOT,'Tools','patrol-paths.json'),'w'),indent=1)
    for r in res: u.log('PATH %s'%r)
    ed.editor_request_end_play(); u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
H=u.register_slate_post_tick_callback(tick)
