"""Targeted composition update; preserves route candidates and all hiding contracts."""
import json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';p=O/'dressing.json';cfg=json.loads(p.read_text());old={r['label']:r for r in cfg['props']};remove={f'CollectionDoll{i}' for i in range(4)}|{'EmptyCollectionChair'};cfg['props']=[r for r in cfg['props'] if r['label'] not in remove and not r['label'].startswith('CollectionWitness') and r['label']!='CollectionEmptyRestraintChair'];cab='/Game/Presentation/Environment/Cabinet/SM_Cabinet_SageTimber';doll='/Game/Presentation/Environment/HeroProps/CollectionDoll/SM_CollectionDoll'
updates={'CollectionCabinet0':([75,-840,390],[1.1,1.1,1.2],-90),'CollectionCabinet1':([65,-1030,390],[.95,1.05,1.4],-90),'CollectionCabinet2':([485,-880,390],[1,1.1,.95],90),'CollectionCabinet3':([440,-1210,390],[1.1,1.1,1.2],0)}
for row in cfg['props']:
 if row['label'] in updates:row['location'],row['scale'],row['yaw']=updates[row['label']]
def add(name,asset,loc,scale,yaw):cfg['props'].append(dict(label=name,asset=asset,location=loc,scale=scale,yaw=yaw))
add('CollectionEmptyRestraintChair','/Game/Presentation/Environment/HeroProps/RestraintChair/SM_RestraintChair',[250,-960,390],[.65,.65,.65],-115)
# Shelf contacts come from actual source raycasts: lower .476844m; upper .854219m.
add('CollectionWitnessLower',doll,[75,-840,390+.476844*120],[.5,.5,.5],90)
add('CollectionWitnessUpper',doll,[65,-1030,390+.854219*140],[.55,.55,.55],-82)
add('CollectionWitnessEast',doll,[485,-880,390+.476844*95],[.42,.42,.42],-90)
# A small wall-facing pair behind the focal chair, outside the south escape corridor.
add('CollectionWitnessWallA',doll,[45,-980,390],[.72,.72,.72],90)
add('CollectionWitnessWallB',doll,[48,-1125,390],[.51,.51,.51],97)
cfg['lights']=[r for r in cfg['lights'] if r['label']!='CollectionChairFocus'];cfg['lights'].append(dict(label='CollectionChairFocus',location=[250,-960,590],lumens=22,color=[.88,.61,.36],radius=270));cfg['collection_composition']='Empty restraint chair focal point; varied-height cabinets, small dolls in measured compartments, wall-facing witnesses; south exit lane retained. WIP pending UE view.';p.write_text(json.dumps(cfg,indent=2))
(O/'collection-refinement.json').write_text(json.dumps(dict(changed_labels=list(updates)+['CollectionEmptyRestraintChair','CollectionWitnessLower','CollectionWitnessUpper','CollectionWitnessEast','CollectionWitnessWallA','CollectionWitnessWallB'],removed_labels=sorted(remove),unchanged_contracts=['All crowbar/fuse candidate locations','All measured hiding centers/views/exits','Final escape polyline and gate locations'],source_shelf_heights_m=[.476844,.854219],status='Awaiting source swept samples and root UE visual review'),indent=2));print('COLLECTION_REFINED')
