"""Poly Haven CC0 model intake for HouseFurnishV03 (system python, no bpy). glTF + DX normal per asset, credits.json."""
import json,urllib.request,hashlib,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseFurnishV03';W=O/'Raw';W.mkdir(parents=True,exist_ok=True);UA={'User-Agent':'TallowHouseFurnishV03/1.0 (uni project; CC0 intake)'}
def get(url,n=4):
 for i in range(n):
  try:return urllib.request.urlopen(urllib.request.Request(url,headers=UA),timeout=120).read()
  except Exception as e:
   if i==n-1:raise
   print('RETRY',url,e,flush=True)
# production name -> (Poly Haven id, texture res). 2k for furniture/landmarks, 1k for hand props.
A={'Armchair':('ArmChair_01','2k'),'GreenChair':('GreenChair_01','2k'),'GothicBed':('GothicBed_01','2k'),'GothicCabinet':('GothicCabinet_01','2k'),'Commode':('GothicCommode_01','2k'),
'HallConsole':('ClassicConsole_01','2k'),'Nightstand':('ClassicNightstand_01','1k'),'TeaTable':('gothic_coffee_table','2k'),'RockingChair':('Rockingchair_01','2k'),'IronBed':('old_bed_frame','2k'),
'Settle':('painted_wooden_bench','1k'),'KitchenCupboard':('painted_wooden_cabinet','2k'),'Dresser':('painted_wooden_cabinet_02','2k'),'NurseryChair':('painted_wooden_chair_01','1k'),'KitchenChair':('painted_wooden_chair_02','1k'),
'SideTable':('painted_wooden_nightstand','1k'),'HangingShelf':('painted_wooden_shelves','1k'),'Stool':('painted_wooden_stool','1k'),'TrestleTable':('painted_wooden_table','1k'),'PedestalTable':('round_wooden_table_01','2k'),
'PlantStand':('side_table_tall_01','1k'),'WritingTable':('small_wooden_table_01','2k'),'Chesterfield':('sofa_02','2k'),'SpinningWheel':('spinning_wheel_01','2k'),'ChinaHutch':('vintage_cabinet_01','2k'),
'CardCatalogue':('vintage_wooden_drawer_01','2k'),'Bookcase':('wooden_bookshelf_worn','2k'),'GothicHighChair':('WoodenChair_01','2k'),'MilkingStool':('wooden_stool_01','1k'),'LowBench':('WoodenTable_01','1k'),
'CarvedArmchair':('gallinera_chair','2k'),'CarvedChest':('gallinera_table','2k'),
'GrandfatherClock':('vintage_grandfather_clock_01','2k'),'MantelClock':('mantel_clock_01','1k'),'AlarmClock':('alarm_clock_01','1k'),'Mirror':('ornate_mirror_01','1k'),'PaintingLandscape':('fancy_picture_frame_01','1k'),
'PaintingTown':('fancy_picture_frame_02','1k'),'OvalPortrait':('hanging_picture_frame_03','1k'),'Bust':('marble_bust_01','1k'),'BookSet':('book_encyclopedia_set_01','1k'),'ChessSet':('chess_set','1k'),'TeaSet':('tea_set_01','1k'),
'Cake':('carrot_cake','1k'),'Candelabra':('brass_candleholders','1k'),'Candlestick':('wooden_candlestick','1k'),'OilLamp':('vintage_oil_lamp','1k'),'Lantern':('Lantern_01','1k'),'Chandelier':('Chandelier_02','1k'),
'BareBulb':('pull_chain_light_socket','1k'),'Suitcases':('vintage_suitcase','1k'),'LaundryBasket':('wicker_basket_01','1k'),'SewingBasket':('wicker_basket_02','1k'),'Bucket':('wooden_bucket_01','1k'),'WashTub':('wooden_bucket_02','1k'),
'MilkChurn':('metal_jug','1k'),'WashJug':('jug_01','1k'),'CopperPot':('brass_pot_01','1k'),'CopperPan':('brass_pan_01','1k'),'Crock':('ceramic_pot','1k'),'Goblets':('brass_goblets','1k'),'WineBottles':('wine_bottles_01','1k'),
'WineBarrel':('wine_barrel_01','1k'),'Crate':('wooden_crate_01','1k'),'CrateLidded':('wooden_crate_02','1k'),'BenchVice':('bench_vice_01','1k'),'HandPlane':('hand_plane_no4','1k'),'Handsaw':('handsaw_wood','1k'),
'HandDrill':('vintage_hand_drill','1k'),'Magnifier':('magnifying_glass_01','1k'),'PipeWrench':('pipe_wrench','1k'),'OilCan':('small_oil_can_01','1k'),'Mousetrap':('mousetrap','1k'),'ToyElephant':('carved_wooden_elephant','1k'),
'Chalkboard':('standing_chalkboard_01','1k'),'Stepladder':('wooden_ladder','1k'),'Broom':('wooden_broom','1k'),'WateringCan':('watering_can_metal_01','1k'),'AshBin':('metal_trash_can','1k'),'BlueVase':('antique_ceramic_vase_01','1k'),
'Spectacles':('round_spectacles','1k'),'PocketWatch':('vintage_pocket_watch','1k'),'Trunk':('treasure_chest','1k'),'Apple':('food_apple_01','1k'),'Bowl':('wooden_bowl_01','1k'),'Blowtorch':('brass_blowtorch','1k'),
'RustedTin':('can_rusted','1k'),'OilTin':('oil_tin','1k'),'PorcelainHorse':('horse_statue_01','1k')}
only=set(sys.argv[1:]);credits=json.loads((O/'credits.json').read_text()) if (O/'credits.json').exists() else {}
for name,(pid,res) in A.items():
 if only and name not in only:continue
 d=W/pid;done=d/'provenance.json'
 if done.exists():credits[name]=json.loads(done.read_text());continue
 d.mkdir(exist_ok=True);info=json.loads(get('https://api.polyhaven.com/info/'+pid));files=json.loads(get('https://api.polyhaven.com/files/'+pid));g=files['gltf'][res]['gltf'];got={}
 for rel,f in [(Path(g['url']).name,g)]+list(g['include'].items()):
  p=d/rel;p.parent.mkdir(parents=True,exist_ok=True);b=get(f['url']);assert hashlib.md5(b).hexdigest()==f['md5'],rel;p.write_bytes(b);got[rel]=f['md5']
 # glTF ships an OpenGL normal; Unreal wants DirectX, which Poly Haven also publishes per map (multi-map models keep their own names).
 for key in [k for k in files if 'nor_dx' in k.lower()]:
  f=files[key][res].get('jpg') or files[key][res].get('png');p=d/'textures_dx'/Path(f['url']).name;p.parent.mkdir(exist_ok=True);p.write_bytes(get(f['url']));got[str(p.relative_to(d))]=f['md5']
 rec=dict(name=name,polyhaven_id=pid,title=info.get('name'),authors=info.get('authors'),license='CC0 1.0 (Poly Haven)',source='https://polyhaven.com/a/'+pid,resolution=res,files=got,download_api='https://api.polyhaven.com/files/'+pid)
 done.write_text(json.dumps(rec,indent=1));credits[name]=rec;print('FETCHED',name,pid,flush=True)
# CC0 texture sets for our own models and the Tripo bakes (box/UV mapped by us).
TEX=['wood_cabinet_worn_long','dark_wood','wood_peeling_paint_weathered','rusty_metal_02','metal_plate','brown_leather','rough_linen','hessian_230','red_brick','black_painted_planks','dirty_carpet','floral_jacquard','waffle_pique_cotton','worn_planks','velour_velvet']
for pid in TEX:
 d=W/'textures'/pid;done=d/'provenance.json'
 if only and pid not in only:continue
 if done.exists():credits['tex:'+pid]=json.loads(done.read_text());continue
 d.mkdir(parents=True,exist_ok=True);info=json.loads(get('https://api.polyhaven.com/info/'+pid));files=json.loads(get('https://api.polyhaven.com/files/'+pid));got={}
 for key in ['Diffuse','nor_gl','nor_dx','Rough','arm']:
  if key not in files:continue
  f=files[key]['2k']['jpg'];p=d/Path(f['url']).name;b=get(f['url']);assert hashlib.md5(b).hexdigest()==f['md5'],p;p.write_bytes(b);got[p.name]=f['md5']
 rec=dict(name='tex:'+pid,polyhaven_id=pid,title=info.get('name'),authors=info.get('authors'),license='CC0 1.0 (Poly Haven)',source='https://polyhaven.com/a/'+pid,resolution='2k',files=got)
 done.write_text(json.dumps(rec,indent=1));credits['tex:'+pid]=rec;print('FETCHED texture',pid,flush=True)
(O/'credits.json').write_text(json.dumps(credits,indent=1));print('RESULT',len(credits),'credited Poly Haven assets')
