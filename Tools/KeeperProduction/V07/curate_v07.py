"""Curate new V07 deliverables without old candidates or Blender backups."""
from pathlib import Path
import json,hashlib
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07'
files=[OUT/n for n in ['keeper-v07-animated.blend','keeper-v07-export.blend','integration-readme.txt','source-inspection.json','rig-audit.json','head-construction-audit.json','animation-contract.json','contact-generation.json','saved-source-validation.json','export-audit.json','fbx-roundtrip.json']]
files+=list((OUT/'Export').glob('*.fbx'))+list((OUT/'Textures').glob('*.png'))+list(OUT.glob('review-*.png'))+list(Path(__file__).parent.glob('*.py'))
assert len(list((OUT/'Export').glob('AN_Keeper_*.fbx')))==20
report={'asset':'KeeperRiggedV07','status':'new control rig/source motion, saved-frame QC, clean bake and FBX roundtrip passed; source PBR images inspected; Unreal integration and actual route/cadence acceptance root-owned pending','namespace':'/Game/Presentation/KeeperRiggedV07','immutable_dependencies':['Tallow/SourceAssets/Keeper/SleevedV06/keeper-direct-delivery.blend','Tallow/SourceAssets/Keeper/SleevedV06/production-audit.json','Tallow/SourceAssets/Keeper/SleevedV06/Textures'],'files':{str(p.relative_to(ROOT)):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files}}
(OUT/'curated-manifest.json').write_text(json.dumps(report,indent=2)+'\n');print('CURATED '+str(len(files))+' V07 files')
