#include "Player/Map/TallowHouseMapComponent.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "House/TallowRoomSearchSubsystem.h"
#include "Player/TallowCharacter.h"
#include "UI/Presentation/TallowHouseMapWidget.h"
#include "GameMode/TallowGameMode.h"
#include "Presentation/TallowPlayDirector.h"
#include "Environment/TallowDoor.h"
#include "Environment/TallowHouseRunLayout.h"
#include "Environment/TallowOptionalBoards.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowProjector.h"
#include "Presentation/TallowEscapePoint.h"
#include "Tasks/TallowPlank.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
UTallowHouseMapComponent::UTallowHouseMapComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.3f;}
void UTallowHouseMapComponent::BeginPlay()
{
 Super::BeginPlay();Player=Cast<ATallowCharacter>(GetOwner());Discovered.Add(TEXT("wake"));
 if(auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>())GM->OnCaught.AddUniqueDynamic(this,&UTallowHouseMapComponent::OnCaught);
}
void UTallowHouseMapComponent::EndPlay(const EEndPlayReason::Type Reason)
{if(Widget)Widget->Close(true);Super::EndPlay(Reason);}
const TArray<FTallowMapRoom>& UTallowHouseMapComponent::Rooms()
{return TallowHouseRooms::All();}
int32 UTallowHouseMapComponent::GetCurrentFloor()const
{return Player.IsValid() ? TallowHouseRooms::FloorAt(Player->GetActorLocation().Z) : 0;}
bool UTallowHouseMapComponent::KnowsRoom(FName Id)const
{
 if(const auto* Search=UTallowRoomSearchSubsystem::Get(this);Search&&Search->IsRunning())return Search->GetRoomState(Id).bVisited;
 return Discovered.Contains(Id);
}
int32 UTallowHouseMapComponent::GetDiscoveredRoomCount()const
{
 if(const auto* Search=UTallowRoomSearchSubsystem::Get(this);Search&&Search->IsRunning())return Search->GetVisited().Num();
 return Discovered.Num();
}
FName UTallowHouseMapComponent::GetCurrentRoom()const
{
 if(const auto* Search=UTallowRoomSearchSubsystem::Get(this);Search&&Search->IsRunning())return Search->GetCurrentRoom();
 if(Player.IsValid())for(const auto& R:Rooms())if(R.Floor==GetCurrentFloor()&&R.Contains(Player->GetActorLocation()))return R.Id;
 return NAME_None;
}
void UTallowHouseMapComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* F)
{
 Super::TickComponent(Dt,T,F);if(!Player.IsValid())return;
 const FName Here=GetCurrentRoom();if(!Here.IsNone())Discovered.Add(Here);
 if(const FName Mark=GetMarkedRoom();Mark!=LoggedMark)
 {LoggedMark=Mark;UE_LOG(LogTemp,Display,TEXT("TALLOW_MAP marked room=%s"),Mark.IsNone()?TEXT("none"):*Mark.ToString());}
 const FVector Eye=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseMapDoor),false,Player.Get());
 for(TActorIterator<ATallowDoor>It(GetWorld());It;++It)
 {
  if(KnownDoors.Contains(*It)||FVector::Dist(Eye,It->GetActorLocation())>280.f)continue;
  FHitResult Hit;
  bool SawDoor=!GetWorld()->LineTraceSingleByChannel(Hit,Eye,It->GetActorLocation()+FVector(0,0,55),ECC_Visibility,Q)||Hit.GetActor()==*It;
  if(!SawDoor&&Hit.GetActor())for(TActorIterator<ATallowHouseRunLayout> Layout(GetWorld());Layout;++Layout)
   for(const auto& Candidate:Layout->Candidates)if(Candidate.Door==*It&&Candidate.OptionalBoards==Hit.GetActor())SawDoor=true;
  if(SawDoor)KnownDoors.Add(*It);
 }
}
bool UTallowHouseMapComponent::CollectMap()
{if(bCollected||!Player.IsValid()||!Player->CanChangeInventory())return false;bCollected=true;ToggleMap();return true;}
bool UTallowHouseMapComponent::IsMapOpen()const{return Widget&&Widget->IsInViewport();}
void UTallowHouseMapComponent::ToggleMap()
{
 if(IsMapOpen()){Widget->Close();return;}
 if(!bCollected||!Player.IsValid())return;
 // Reaching for the map puts the picks away.
 {TActorIterator<ATallowPlayDirector>It(GetWorld());if(It)It->StopLockpicking();}
 if(!Player->CanChangeInventory())return;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return;
 Player->PrepareForInventory();PlayGesture(false);
 if(!Widget)Widget=CreateWidget<UTallowHouseMapWidget>(PC);
 if(Widget)Widget->Open(this,PC);
}
void UTallowHouseMapComponent::OnCaught(){if(Widget)Widget->Close(true);}
void UTallowHouseMapComponent::PlayGesture(bool bPuttingAway)
{TActorIterator<ATallowPlayDirector>It(GetWorld());if(It)It->PlayMapGesture(bPuttingAway);}
FName UTallowHouseMapComponent::GetMarkedRoom()const
{
 const auto* Chapters=UTallowChapterSubsystem::GetActive(this);if(!Chapters)return NAME_None;
 ETallowGuideKind Kind;const AActor* Objective=Chapters->GetGuideObjective(Kind);
 return Objective?TallowHouseRooms::RoomAt(Objective->GetActorLocation()):NAME_None;
}
FString UTallowHouseMapComponent::GetClue()const
{
 if (const auto* Chapters = UTallowChapterSubsystem::GetActive(this))
 {
  const FText Clue = Chapters->GetMapClue(); if (!Clue.IsEmpty()) return Clue.ToString();
  // 31 Oct: the map names the room, never the spot in it. It knows the room too (UTallowHunterDirector::TakeGuardHint).
  ETallowGuideKind Kind; const AActor* Objective = Chapters->GetGuideObjective(Kind);
  if (const FTallowRoomDef* Room = Objective ? TallowHouseRooms::Find(TallowHouseRooms::RoomAt(Objective->GetActorLocation())) : nullptr)
  {
   const TCHAR* Floors[] = {TEXT("in the cellar"), TEXT("on the ground floor"), TEXT("upstairs")};
   const FString Where = FString::Printf(TEXT("%s,\n%s."), *Room->Label, Floors[FMath::Clamp(Room->Floor, 0, 2)]);
   if (Kind == ETallowGuideKind::Tape) return TEXT("something is hissing.\na tape, waiting for me.\n\n") + Where;
   if (Kind == ETallowGuideKind::Battery) return TEXT("the battery hums.\ni circled where.\n\n") + Where + TEXT("\n\nit goes there too.\nwait for it to pass.");
   if (Kind == ETallowGuideKind::FuseBox) return TEXT("take it to the humming box.\n\n") + Where + TEXT("\n\nit knows where i am going.");
  }
 }
 TActorIterator<ATallowFuseBox>Box(GetWorld());const int32 Fitted=Box?Box->CurrentFuses:0;
 if(Fitted>=3)
 {
  TActorIterator<ATallowProjector> It(GetWorld());
  if(It)
  {
   if(!It->IsReelLoaded())return Player.IsValid()&&Player->GetInventoryCount(ECarriedItemType::FilmReel)>0?TEXT("the metal reel fits the projector.\ntake it in hand from my pockets.\n\nthen tap E to thread the film."):TEXT("the release wants five numbers.\n\na film reel was left in the house.\nfind it. take it to the projector.");
   if(It->GetProjectorState()==EProjectorState::Empty)return TEXT("the reel is fitted.\n\ntap E, again and again,\nto thread the film.\ndon't let it slip back.");
   if(!It->HasCompletedProjection())return TEXT("start the projector.\nwatch the light on the wall.\n\nfive numbers. in order.\nit will be loud.");
   for(TActorIterator<ATallowEscapePoint> Exit(GetWorld());Exit;++Exit)if(Exit->IsCodeVerified())return TEXT("the release accepted the numbers.\n\nhold E to pull its lever.\n\nthe way out.");
   FString Code;for(int32 D:It->GetCodes()){if(!Code.IsEmpty())Code+=TEXT("  ");Code+=FString::FromInt(D);}
   return TEXT("i wrote what the film showed:\n\n")+Code+TEXT("\n\nenter them at the attic release.\ni can watch the film again.");
  }
  return TEXT("the box is working.\ntry the latch upstairs.\n\nmaisie's place is there too.");
 }
 bool BoardsRemain=false;for(TActorIterator<ATallowPlank>It(GetWorld());It;++It)BoardsRemain|=It->bRequiredForEscape&&!It->IsBroken();
 if(BoardsRemain)
 {
  if(Player.IsValid()&&Player->GetInventoryCount(ECarriedItemType::Crowbar)>0)
   return TEXT("the way past the tools\nis boarded shut.\n\nthe iron can lift the nails.\nwait until it has gone.\nit will be loud.");
  return TEXT("something long & heavy\nwas left with the tools.\n\ntry the room below this one?");
 }
 return FString::Printf(TEXT("three little glass fuses.\none on each floor.\n\ncupboards? shelves?\nthey aren't always in the same place.\n\n%d / 3 fitted in the humming box."),Fitted);
}
