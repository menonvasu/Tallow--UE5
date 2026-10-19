"""Architecture audit of Lvl_Tallow_Playable in PIE (full collision): gaps to the outside, ceiling heights, floor holes.
Samples each floor on a grid at three heights; a horizontal ray that leaves the footprint without hitting anything is a
leak (an unintended opening unless it lines up with a window). Never saves. Output: Saved/Integration/architecture-audit.json
"""
import json,math,time,traceback
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/Saved/Integration/architecture-audit.json'
MAP='/Game/Presentation/Playable/Lvl_Tallow_Playable';ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
FLOORS={'basement':-330,'ground':0,'upper':390};X0,X1,Y0,Y1=10,1565,-1290,-10;STEP=50;HEIGHTS=(40,110,220)
R={'leaks':[],'ceiling':{},'floor_holes':{},'errors':[]}

def trace(w,a,b,ignore):
    h=u.SystemLibrary.line_trace_single(w,a,b,u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,u.DrawDebugTrace.NONE,True)
    return h.to_tuple()[4] if h else None

def exit_point(p,d):
    # Where the ray leaves the footprint rectangle (axis-parallel rays only test the walls they can reach).
    ts=[]
    if abs(d.x)>1e-6:ts.append(((X0 if d.x<0 else X1)-p.x)/d.x)
    if abs(d.y)>1e-6:ts.append(((Y0 if d.y<0 else Y1)-p.y)/d.y)
    t=min([t for t in ts if t>0] or [0.]);e=[p.x+d.x*t,p.y+d.y*t]
    side='west' if abs(e[0]-X0)<1 else 'east' if abs(e[0]-X1)<1 else 'south' if abs(e[1]-Y0)<1 else 'north'
    return side,[round(e[0]),round(e[1])]

def main():
    assert ED.load_level(MAP);yield 3
    ED.editor_request_begin_play();yield 5
    w=u.EditorLevelLibrary.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0);k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature);ignore=[p,k]
    dirs=[u.Vector(math.cos(a),math.sin(a),0) for a in [i*math.tau/16 for i in range(16)]]
    for floor,z in FLOORS.items():
        ceil_rows=[];hole_rows=[]
        for y in range(Y1-20,Y0+19,-STEP):
            crow='';hrow=''
            for x in range(X0+20,X1-19,STEP):
                eye=u.Vector(x,y,z+110)
                if u.SystemLibrary.sphere_overlap_actors(w,eye,8,[],None,ignore):crow+='#';hrow+='#';continue
                down=trace(w,eye,u.Vector(x,y,z-60),ignore);hrow+='.' if down and abs(down.z-z)<40 else 'H' if not down else 's'
                up=trace(w,eye,u.Vector(x,y,z+800),ignore)
                if not up:crow+='O'
                else:
                    hgt=up.z-z;crow+=str(min(9,int(hgt//50))) if hgt<500 else '+'
                if not (down and abs(down.z-z)<40 and up):continue  # only real interior points: floor below, ceiling above
                for hz in HEIGHTS:
                    q=u.Vector(x,y,z+hz)
                    for d in dirs:
                        if trace(w,q,q+d*2600,ignore) is None:
                            side,e=exit_point(q,d);R['leaks'].append({'floor':floor,'from':[x,y,z+hz],'height':hz,'side':side,'exit':e})
            ceil_rows.append(crow);hole_rows.append(hrow)
        R['ceiling'][floor]=ceil_rows;R['floor_holes'][floor]=hole_rows
        u.log('ARCH_AUDIT floor done '+floor);yield .05

def done(e=None):
    if e:R['errors'].append(e);u.log_error(e)
    OUT.write_text(json.dumps(R,indent=1));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0;START=time.monotonic()
def tick(dt):
    global NEXT
    if time.monotonic()-START>900:done('900s timeout');return
    if time.monotonic()<NEXT:return
    try:NEXT=time.monotonic()+next(G)
    except StopIteration:done()
    except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
