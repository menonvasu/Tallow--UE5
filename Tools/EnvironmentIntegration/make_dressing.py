import json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';P='/Game/Presentation/Environment/';rows=[]
def prop(name,asset,loc,scale=(1,1,1),yaw=0):rows.append(dict(label=name,asset=asset,location=loc,scale=scale,yaw=yaw))
f=P+'HouseIntegrationV01/Furniture/SM_House_';cab=P+'Cabinet/SM_Cabinet_SageTimber';doll=P+'HeroProps/CollectionDoll/SM_CollectionDoll'
prop('WorkshopBench',f+'GothicTable',[250,-1040,-330],[1.65,.55,1.77]);prop('WorkshopChair',f+'PaintedChair',[260,-1180,-330],yaw=10)
prop('LaundryTable',f+'GothicTable',[1420,-1080,-330],[1.1,.70,1.83]);prop('StudyDesk',f+'GothicTable',[1330,-350,0],[1.16,.55,1.73]);prop('StudyChair',f+'PaintedChair',[1310,-475,0],yaw=8)
prop('DiningTable',f+'GothicTable',[280,-1040,0],[1.59,.90,2.10])
for i,(x,y,yaw) in enumerate([(145,-1030,-90),(410,-1040,90),(270,-1190,0)]):prop('DiningChair'+str(i),f+'PaintedChair',[x,y,0],yaw=yaw)
prop('ParlourSofa',f+'Sofa',[260,-120,0],yaw=180);prop('ParlourTable',f+'GothicTable',[260,-300,0],[.8,.65,.9]);prop('StudyCabinet',cab,[1460,-160,0],yaw=180)
prop('KitchenCupboard',cab,[1460,-1120,0],[1.3,1.4,1.35],90);prop('BedroomSofa',f+'Sofa',[1320,-330,390],yaw=180);prop('BedroomChair',f+'PaintedChair',[1100,-180,390],yaw=-15)
prop('ToyShelfCabinet',cab,[110,-350,390],[1.3,1.2,1.30],-90);prop('ToyRoomChair',f+'PaintedChair',[430,-470,390],yaw=-35)
for i,(x,y,yaw) in enumerate([(80,-850,-90),(80,-1030,-90),(480,-870,90),(440,-1210,0)]):
 prop('CollectionCabinet'+str(i),cab,[x,y,390],[1.1,1.1,1.2],yaw)
 prop('CollectionDoll'+str(i),doll,[x,y,555],[.8,.8,.8],yaw+180)
prop('EmptyCollectionChair',f+'PaintedChair',[240,-970,390],[.8,.8,.8],-8)
# Declared marker ledges, not loose floating items. Existing shelves are authored in architecture.
prop('UtilityCupboard',cab,[1420,-200,-330],[1.1,1.1,1.3],180)
remove=list(range(70,92)) # exact prototype furnishing replacement; no invisible boiler blocking item/panel
lights=[]
for name,loc,power,color,radius in [('WakeFill',[220,-380,-100],55,[.75,.59,.37],500),('WorkshopTask',[250,-1010,-80],65,[1,.64,.32],480),('BasementStairLandmark',[1260,-900,-50],80,[.40,.57,1],600),('GroundStairLandmark',[780,-620,250],110,[.45,.62,1],700),('DiningPractical',[280,-1040,280],65,[1,.65,.32],550),('StudyWindow',[1400,-300,240],65,[.4,.58,1],550),('UpperLanding',[780,-380,640],90,[.42,.59,1],650),('CollectionWindow',[120,-1200,620],55,[.40,.55,1],550),('BedroomWindow',[1300,-150,620],45,[.45,.59,1],500),('ToyShelfFill',[180,-380,620],40,[.78,.66,.44],500)]:lights.append(dict(label=name,location=loc,lumens=power,color=color,radius=radius))
(O/'dressing.json').write_text(json.dumps(dict(props=rows,replace_prototype_actor_names=['StaticMeshActor_'+str(i) for i in remove],lights=lights,visual_status='WIP: source and Unreal visual/runtime gates required',exposure=dict(ev100=1.7,physical_camera_exposure=False)),indent=2))
