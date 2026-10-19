"""Write V08Animation/animation-contract.json from the authored contact samples, events and the saved-source validation."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/V08Animation'
src=json.loads((OUT/'keeper-v08-animated.contacts.json').read_text());val={r['clip']:r for r in json.loads((OUT/'validation-v08.json').read_text())['clips']}
GAME=['Idle','HostWalk','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl','SearchUnder','SearchOver','Reveal','JumpscareDeath','Squeeze','Listen','Alert','Reset','WallEnter','WallExit','CeilingEnter','CeilingExit']
V07={'Walk':25,'JumpscareDeath':73}
clips=[]
for c in src:
    n=c['name'];runs={}
    for s in c['contacts']:
        for h in s['support']:runs.setdefault(h['hand'],[]).append(h['stance'])
    crawl={h:v for h,v in runs.items() if any(v)}
    duty=round(sum(sum(v)/len(v) for v in crawl.values())/len(crawl),3) if crawl else None
    lens=[]
    for v in crawl.values():
        if c['loop']:v=v[:-1];i0=v.index(False) if False in v else 0;v=v[i0:]+v[:i0]   # unwrap the loop seam
        n_=0
        for x in v+[False]:
            if x:n_+=1
            elif n_:lens.append(n_);n_=0
    v=c['reference_speed_cm_s']/100;stride=round(v*(sum(lens)/len(lens)-1)/24,3) if v and lens else 0.   # stroke per stance (frames between plant and lift)
    row={'name':n,'status':'final V08 (no V07 stand-ins remain)','set':'game' if n in GAME else 'variant','frames':c['frames'],'seconds':round(c['seconds'],4),'loop':c['loop'],'reference_speed_cm_s':c['reference_speed_cm_s'],
         'stride_m':stride,'stance_duty':duty,'contact_plane':'+Z local floor; runtime rotates surface','max_contact_error_m':val[n]['max_contact_error_m'],'events_seconds':c['events_seconds'],'note':c['note']}
    if n in V07 and c['frames']!=V07[n]:row['length_changed_from_v07_frames']=V07[n]
    clips.append(row)
order={n:i for i,n in enumerate(GAME)};clips.sort(key=lambda r:(r['set']!='game',order.get(r['name'],99),r['name']))
contract={'clips':clips,'root_motion':False,'fps':24,'source_forward':'-Y','engine_legacy_forward':'+Y','engine_mesh_yaw':-90,'mesh_scale':1,'head_bone':'head',
 'morph_curves':['FULL_HORROR','HOOD_OPEN','CHOMP','Blink_L','Blink_R'],
 'capture':{'note':'Jumpscares are staged for the capture camera: player eye 1.1 m, Keeper root ~1.25 m in front facing it (grab sphere 85 cm + capsules). The game fade (FadeOutTimer 0.8 s) starts at clip_end-0.8 s: every strike lands just before it. Proximity only holds near 1.25 m: at 1.0 m the head reaches the lens, beyond ~1.6 m the maw is under 30% of frame height. Recommended: on capture, place the Keeper root 1.25 m from the camera along the look line before playing the clip.',
            'variants':{n:src_c['events_seconds'] for src_c in src for n in [src_c['name']] if n.startswith('Jumpscare')}},
 'turns':'Turn_L90/Turn_R90: the ACTOR must yaw 90 deg linearly over the clip (actor_yaw_rate_deg_s); planted hands are world-fixed under that rotation.',
 'runtime_owner':'Vasu AI unchanged; playback adapter selects/crossfades only','quality_status':'V08: saved-source validation + clean bake + FBX roundtrip; Unreal import pending'}
(OUT/'animation-contract.json').write_text(json.dumps(contract,indent=2));print('CONTRACT',len(clips),'clips')
