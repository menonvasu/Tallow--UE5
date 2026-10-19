import json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseStoryV02';rows=[]
def add(label,mesh,loc,scale=(1,1,1),yaw=0,tilt=0):rows.append(dict(label=label,mesh='SM_Story_'+mesh,location_cm=loc,scale=scale,yaw=yaw,tilt=tilt,collision='NoCollision'))
add('WorkshopFamily','FamilyPortrait',[445,-1273,-165],[.72,.72,.72],tilt=-1.5)
add('WakeAbsentFrames','EmptyFrameCluster',[460,-626,-160],[.9,.9,.9],tilt=2)
add('WakeEmptyPegs','EmptyPegRail',[130,-568,-165],[.8,.8,.8])
add('WakeRunner','TatteredRunner',[286,-493,-329.5],[1,.95,1])
add('StairFamily','FamilyPortrait',[963,-560,225],[.8,.8,.8],90,-2)
add('UpperLandingRunner','TatteredRunner',[780,-295,390.5],[1,.92,1])
add('CollectionAbsentFrames','EmptyFrameCluster',[12,-1130,640],[1,1,1],-90,2)
add('CollectionEmptyPegs','EmptyPegRail',[12,-870,605],[1,1,1],-90)
add('CollectionRunner','TatteredRunner',[250,-985,390.5],[1.4,.85,1],-2)
m=json.loads((O/'manifest.json').read_text());tris={r['mesh']:r['triangles'] for r in m['meshes']};total=sum(tris[r['mesh']] for r in rows);assert total<=10000,total
(O/'placement.json').write_text(json.dumps(dict(placements=rows,placed_triangles=total,unique_triangles=sum(tris.values()),map='/Game/Presentation/Playable/Lvl_Tallow_Playable',contracts='No changes to item/hiding/gates; no collision. Runners lie within solid floor and below1.2cm surface height.',compositions=['Wake/workshop: empty hooks and frames lead to the remaining family portrait.','Stair/landing: the same portrait watches the ascent; runner gives domestic scale.','Collection: frames and hooks have no occupants; worn runner ends at empty restraint chair.']),indent=2));print('RESULT placed triangles',total)
