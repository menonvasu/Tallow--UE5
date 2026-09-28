"""Finds openings in the M3 house: from interior sample points, rays that escape the house footprint into the void.
Writes Tools/house-leaks.json with clustered exit points per floor. Editor world, no PIE."""
import unreal as u, json, math, os
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
X0,X1,Y0,Y1=-10,1570,-1310,10          # house shell footprint (cm)
FLOORS={'basement':-180,'ground':150,'upper':550,'attic':900}
leaks=[]
def trace(a,b):
    hit=u.SystemLibrary.line_trace_single(w,a,b,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,True)
    return hit
for name,z in FLOORS.items():
    for x in range(60,1540,90):
        for y in range(-1260,-20,90):
            p=u.Vector(x,y,z)
            # skip points buried in geometry: a short trace from above must not start inside
            for k in range(16):
                a=2*math.pi*k/16; d=u.Vector(math.cos(a),math.sin(a),0)
                end=p+d*2500
                h=trace(p,end)
                ok=h and h.to_tuple()[0]
                if not ok:
                    # escaped: where did it leave the footprint?
                    tx=[(t) for t in ((X0-x)/d.x if d.x<0 else (X1-x)/d.x if d.x>0 else 1e9,(Y0-y)/d.y if d.y<0 else (Y1-y)/d.y if d.y>0 else 1e9)]
                    t=min(tx); q=p+d*t
                    leaks.append({'floor':name,'from':[x,y,z],'exit':[round(q.x),round(q.y),round(q.z)]})
            for dz,lab in ((1,'up'),(-1,'down')):
                h=trace(p,p+u.Vector(0,0,dz*3000))
                if not (h and h.to_tuple()[0]): leaks.append({'floor':name,'from':[x,y,z],'exit':[x,y,z],'vertical':lab})
# cluster exits on a 60cm grid
cl={}
for L in leaks:
    key=(L['floor'],L.get('vertical',''),round(L['exit'][0]/60)*60,round(L['exit'][1]/60)*60)
    cl.setdefault(key,0); cl[key]+=1
out=[{'floor':k[0],'vertical':k[1],'x':k[2],'y':k[3],'rays':n} for k,n in sorted(cl.items(),key=lambda kv:-kv[1])]
json.dump({'total_escaping_rays':len(leaks),'clusters':out},open(os.path.join(ROOT,'Tools','house-leaks.json'),'w'),indent=1)
u.log('LEAKS total=%d clusters=%d'%(len(leaks),len(out)))
for c in out[:40]: u.log('LEAK %s'%c)
u.SystemLibrary.quit_editor()
