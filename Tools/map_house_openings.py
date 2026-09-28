"""(Runs inside PIE: the headless editor world lacks upper-storey collision.) Maps every opening in the M3 house: window/door rectangles in the four outer walls (per storey) and holes in
the floor slabs. Writes Tools/house-openings.json. Editor world, no PIE."""
import unreal as u, json, os
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
import time
def scan(w):
    def hit(a,b):
        h=u.SystemLibrary.line_trace_single(w,u.Vector(*a),u.Vector(*b),u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],u.DrawDebugTrace.NONE,True)
        return h is not None and h.to_tuple()[0]
    STOREYS={'basement':(-330,-30),'ground':(0,360),'upper':(390,720)}
    # wall planes: (name, axis fixed, value, other-axis range, outward sign)
    WALLS=[('front','y',0,(0,1560),1),('back','y',-1300,(0,1560),-1),('west','x',0,(-1300,0),-1),('east','x',1560,(-1300,0),1)]
    G=10
    openings=[]
    for st,(z0,z1) in STOREYS.items():
        for name,ax,val,(a0,a1),sgn in WALLS:
            cols=list(range(a0+G,a1,G)); rows=list(range(z0+G,z1,G))
            empty=set()
            for i,a in enumerate(cols):
                for j,z in enumerate(rows):
                    if ax=='y': p0=(a,val-sgn*60,z); p1=(a,val+sgn*60,z)
                    else: p0=(val-sgn*60,a,z); p1=(val+sgn*60,a,z)
                    if not hit(p0,p1): empty.add((i,j))
            # connected components -> rectangles
            seen=set()
            for c in empty:
                if c in seen: continue
                st_=[c]; comp=[]; seen.add(c)
                while st_:
                    q=st_.pop(); comp.append(q)
                    for d in ((1,0),(-1,0),(0,1),(0,-1)):
                        n=(q[0]+d[0],q[1]+d[1])
                        if n in empty and n not in seen: seen.add(n); st_.append(n)
                if len(comp)<4: continue
                ii=[q[0] for q in comp]; jj=[q[1] for q in comp]
                openings.append({'storey':st,'wall':name,'axis':ax,'plane':val,'u0':cols[min(ii)]-G/2,'u1':cols[max(ii)]+G/2,'z0':rows[min(jj)]-G/2,'z1':rows[max(jj)]+G/2,'cells':len(comp)})
    holes=[]
    for slab,(zt) in (('ground_floor',0),('upper_floor',390)):
        empty=set()
        for x in range(10,1560,20):
            for y in range(-1290,0,20):
                if not hit((x,y,zt+15),(x,y,zt-45)): empty.add((x,y))
        seen=set()
        for c in empty:
            if c in seen: continue
            st_=[c]; comp=[]; seen.add(c)
            while st_:
                q=st_.pop(); comp.append(q)
                for d in ((20,0),(-20,0),(0,20),(0,-20)):
                    n=(q[0]+d[0],q[1]+d[1])
                    if n in empty and n not in seen: seen.add(n); st_.append(n)
            xs=[q[0] for q in comp]; ys=[q[1] for q in comp]
            holes.append({'slab':slab,'x0':min(xs)-10,'x1':max(xs)+10,'y0':min(ys)-10,'y1':max(ys)+10,'cells':len(comp)})
    json.dump({'wall_openings':openings,'slab_holes':holes},open(os.path.join(ROOT,'Tools','house-openings.json'),'w'),indent=1)
    for o in openings: u.log('OPEN %s %s u %d..%d z %d..%d'%(o['storey'],o['wall'],o['u0'],o['u1'],o['z0'],o['z1']))
    for h in holes: u.log('HOLE %s x %d..%d y %d..%d cells %d'%(h['slab'],h['x0'],h['x1'],h['y0'],h['y1'],h['cells']))
    
S={'start':time.monotonic(),'go':False,'done':False}
def tick(dt):
    now=time.monotonic()
    if S['done']:
        if now-S['exit']>2: u.unregister_slate_post_tick_callback(H); u.SystemLibrary.quit_editor()
        return
    if not S['go']:
        if now-S['start']>5: S['go']=True; ed.editor_request_begin_play(); S['pie']=now
        return
    if now-S['pie']<4: return
    try: scan(u.EditorLevelLibrary.get_game_world())
    except Exception as e: u.log_error('OPEN_FAIL %s'%e)
    S['done']=True; ed.editor_request_end_play(); S['exit']=time.monotonic()
H=u.register_slate_post_tick_callback(tick)
