"""Extends the Keeper's patrol from the basement into the whole house: basement loop, then up the basement stairs,
across the ground floor, up the main stairs, round the upper floor and back down the back stairs. Re-runnable."""
import unreal as u
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
acts=eas.get_all_level_actors()
for a in acts:
    if a.get_actor_label().startswith('M3_PatrolUp_'): eas.destroy_actor(a)
base=sorted([a for a in eas.get_all_level_actors() if a.get_actor_label().startswith('M3_Patrol_')],key=lambda a:a.get_actor_label())
cls=u.load_class(None,'/Script/Tallow.TallowPatrolPoint')
import json, os
plan=json.load(open('/home/tyron/Projects/cgra252-team9/Tools/patrol-plan.json'))
def order(pts,first_near):
    pts=list(pts); out=[min(pts,key=lambda p:(p[0]-first_near[0])**2+(p[1]-first_near[1])**2)]; pts.remove(out[0])
    while pts:
        nxt=min(pts,key=lambda p:(p[0]-out[-1][0])**2+(p[1]-out[-1][1])**2); out.append(nxt); pts.remove(nxt)
    return out
g=order(plan['ground']['points'],(1250,-1150)); up=order(plan['upper']['points'],(g[-1][0],g[-1][1]))
UP=[('G%d'%i,(p[0],p[1],p[2]+10)) for i,p in enumerate(g)]+[('U%d'%i,(p[0],p[1],p[2]+10)) for i,p in enumerate(up)]
ups=[]
for name,p in UP:
    a=eas.spawn_actor_from_class(cls,u.Vector(*p)); a.set_actor_label('M3_PatrolUp_'+name); a.set_folder_path('AI/Patrol'); ups.append(a)
loop=base+ups
for i,a in enumerate(loop): a.set_editor_property('next_points',[loop[(i+1)%len(loop)]])
assert ed.save_current_level(); u.log('PATROL loop %d points'%len(loop))
