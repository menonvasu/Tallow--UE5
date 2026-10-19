"""Read-only asset validation in a fresh Unreal process after import; root executes."""
import unreal as u,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P='/Game/Presentation/PlayerInteractionV05';C='/Game/Presentation/CandleV02';source=json.loads((ROOT/'Tallow/SourceAssets/Player/InteractionV05/animation-validation.json').read_text());report={}
full=u.load_asset(P+'/SK_PlayerV05_Full');partial=u.load_asset(P+'/SK_PlayerV05_Partial');assert full and partial
height=full.get_imported_bounds().box_extent.z*2;assert 110<height<130,('player height cm',height)
assert full.skeleton==partial.skeleton
for mesh in [full,partial]:
 mats=[slot.get_editor_property('material_interface') for slot in mesh.get_editor_property('materials')];assert mats and all(m and m.get_path_name().startswith(P+'/M_PlayerV05') for m in mats)
report['height_cm']=height;report['clips']={}
for name,expected in source['clips'].items():
 clip=u.load_asset(P+'/AN_PlayerV05_'+name);assert clip and clip.skeleton==full.skeleton
 duration=clip.get_play_length();assert abs(duration-expected['seconds'])<.04,(name,duration)
 lengths=[]
 for time in [0,duration*.5,max(0,duration-.001)]:
  # Engine implementation returns LOCAL transforms; hand_r translation must be a human forearm length in cm.
  pose=u.AnimationBlueprintLibrary.get_bone_pose_for_time(clip,'hand_r',time,False);v=pose.translation;length=math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z);assert 10<length<25,(name,time,length);lengths.append(length)
 report['clips'][name]={'duration':duration,'hand_parent_lengths_cm':lengths}
candle=u.load_asset(C+'/SM_CandleV02_Crooked');assert candle;b=candle.get_bounding_box();height=b.max.z-b.min.z;assert 24<height<27,height;report['candle_height_cm']=height
out=ROOT/'Tallow/Saved/Presentation/PlayerV05-fresh-import.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2));u.log('Player V05 fresh-process unit/material/clip validation passed')
