"""PIE: finds where the Keeper can really walk (complete navmesh paths from its spawn) on each storey, picks
well-spread patrol points per storey and writes Tools/patrol-plan.json. extend_keeper_patrol.py then places them."""
import unreal as u, json, os, time, math
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
FLOORS={'ground':10,'upper':400}
S={'s':time.monotonic(),'go':False}
def tick(dt):
    now=time.monotonic()
    if not S['go']:
        if now-S['s']>5: S['go']=True; ed.editor_request_begin_play(); S['p']=now
        return
    if now-S['p']<6: return
    gw=u.EditorLevelLibrary.get_game_world(); k=u.GameplayStatics.get_actor_of_class(gw,u.TallowCreature); start=k.get_actor_location()
    plan={}
    for fl,z in FLOORS.items():
        ok=[]
        for x in range(80,1520,100):
            for y in range(-1240,-60,100):
                path=u.NavigationSystemV1.find_path_to_location_synchronously(gw,start,u.Vector(x,y,z),k)
                if path and path.is_valid() and not path.is_partial():
                    end=path.get_editor_property('path_points')[-1]
                    if abs(end.z-z)<60: ok.append((x,y,round(end.z)))
        # farthest-point sampling: 4 well-spread points
        pick=[]
        if ok:
            pick.append(max(ok,key=lambda p:(p[0]-780)**2+(p[1]+650)**2))
            while len(pick)<min(4,len(ok)):
                pick.append(max(ok,key=lambda p:min((p[0]-q[0])**2+(p[1]-q[1])**2 for q in pick)))
        plan[fl]={'reachable_cells':len(ok),'points':pick}
        u.log('PLAN %s reachable=%d pick=%s'%(fl,len(ok),pick))
    json.dump(plan,open(os.path.join(ROOT,'Tools','patrol-plan.json'),'w'),indent=1)
    ed.editor_request_end_play(); u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
H=u.register_slate_post_tick_callback(tick)
