"""Public BlenderKit download API; no saved credentials or live Blender session."""
import json,urllib.request,uuid,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01/BlenderKitRaw';O.mkdir(parents=True,exist_ok=True)
results=json.loads((O/'source-catalog.json').read_text());records=[]
for a in results:
 name=a['production_name'];assert a['license']=='cc_zero' and a['canDownload'];folder=O/name;folder.mkdir(exist_ok=True);f=next((x for x in a['files'] if x['fileType']=='resolution_2K'),next(x for x in a['files'] if x['fileType']=='blend'))
 reply=json.load(urllib.request.urlopen(f['downloadUrl']+'?scene_uuid='+str(uuid.uuid4()),timeout=30));dest=folder/'source.blend';dest.write_bytes(urllib.request.urlopen(urllib.request.Request(reply['filePath'],headers={'User-Agent':'BlenderKit/3.20','Referer':'https://www.blenderkit.com/'}),timeout=90).read());(folder/'source-preview.png').write_bytes(urllib.request.urlopen(urllib.request.Request(a['thumbnailMiddleUrl'],headers={'User-Agent':'BlenderKit/3.20','Referer':'https://www.blenderkit.com/'}),timeout=30).read())
 record={'name':a['displayName'],'author':a['author']['fullName'],'license':a['license'],'source':'https://www.blenderkit.com/asset-gallery-detail/'+a['assetBaseId']+'/','asset_id':a['id'],'asset_base_id':a['assetBaseId'],'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'source_file':str(dest.relative_to(R))};records.append(record);(folder/'provenance.json').write_text(json.dumps(record,indent=2));print('FETCHED',name,round(dest.stat().st_size/1e6,2),'MB',flush=True)
(O/'attribution.json').write_text(json.dumps(records,indent=2))
