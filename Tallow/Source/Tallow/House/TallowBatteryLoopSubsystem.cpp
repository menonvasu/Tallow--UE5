#include "House/TallowBatteryLoopSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Environment/TallowPickupSpot.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowFusePickup.h"
#include "Player/TallowCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

bool UTallowBatteryLoopSubsystem::DoesSupportWorldType(const EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
UTallowBatteryLoopSubsystem* UTallowBatteryLoopSubsystem::Get(const UObject* Context) { const auto* W=Context?Context->GetWorld():nullptr;return W?W->GetSubsystem<UTallowBatteryLoopSubsystem>():nullptr; }
void UTallowBatteryLoopSubsystem::Deinitialize()
{
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(InventoryTimer);
    if(Box.IsValid()) Box->OnFuseInserted.RemoveDynamic(this,&UTallowBatteryLoopSubsystem::OnFuseInserted);
    Super::Deinitialize();
}
ATallowFuseBox* UTallowBatteryLoopSubsystem::GetFuseBox() const
{
    if(Box.IsValid()) return Box.Get();
    TActorIterator<ATallowFuseBox> I(GetWorld()); if(I) return *I;
    return nullptr;
}
void UTallowBatteryLoopSubsystem::PrepareRound(const FTallowChapterLayout& L,int32 Installed,int32 Required,const FText& Noun)
{
    bReleased=false; BatteryNoun=Noun; Box=GetFuseBox();
    if(!bCapturedOriginals)
    {
        TArray<ATallowFusePickup*> Fuses;for(TActorIterator<ATallowFusePickup> I(GetWorld());I;++I) Fuses.Add(*I);
        Fuses.Sort([](const auto& A,const auto& B){return A.GetFName().LexicalLess(B.GetFName());});
        for(auto* F:Fuses) OriginalFuses.Add(F); bCapturedOriginals=true;
    }
    for(TActorIterator<ATallowFusePickup> I(GetWorld());I;++I)
    {
        if(!OriginalFuses.Contains(*I)) I->Destroy();
        else {I->SetDormant(true);I->SetChapterNoun(Noun);}
    }
    Battery=Installed<Required&&OriginalFuses.IsValidIndex(Installed)?OriginalFuses[Installed]:nullptr;
    BatterySpot.Reset();
    for(TActorIterator<AActor> I(GetWorld());I;++I) {
        bool Match=I->GetFName()==L.BatterySpot;
        for(FName Tag:I->Tags) if(Tag.ToString()==TEXT("Tallow.Spot.")+L.BatterySpot.ToString()) Match=true;
        if(Match) {BatterySpot=*I;BatteryTransform=I->GetActorTransform();break;}
    }
    if(L.BatterySpot.IsNone()) Battery.Reset();
    SeatBattery();
    if(Box.IsValid())
    {
        Box->RequiredFuses=FMath::Clamp(Required,1,3); Box->RestoreInstalledFuses(Installed); Box->SetChapterNoun(Noun);
        Box->OnFuseInserted.AddUniqueDynamic(this,&UTallowBatteryLoopSubsystem::OnFuseInserted);
    }
    const auto* Player=Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); PreviousCarried=Player?Player->GetInventoryCount(ECarriedItemType::Fuse):0;
    GetWorld()->GetTimerManager().SetTimer(InventoryTimer,this,&UTallowBatteryLoopSubsystem::PollInventory,.2f,true);
    UE_LOG(LogTemp,Display,TEXT("TALLOW_BATTERY prepared installed=%d actor=%s spot=%s"),Installed,*GetNameSafe(Battery.Get()),*L.BatterySpot.ToString());
}
void UTallowBatteryLoopSubsystem::SeatBattery()
{
    if(!Battery.IsValid()) return;
    Battery->SetActorTransform(BatteryTransform);
    if(const auto* Spot=Cast<ATallowPickupSpot>(BatterySpot.Get())) Spot->SeatItem(Battery.Get());
    Battery->SetChapterNoun(BatteryNoun);Battery->SetDormant(!bReleased);
}
void UTallowBatteryLoopSubsystem::ReleaseBattery() {bReleased=true; if(Battery.IsValid()) Battery->SetDormant(false);}
void UTallowBatteryLoopSubsystem::RetireBattery()
{
    bReleased=false;
    for(TActorIterator<ATallowFusePickup> I(GetWorld());I;++I) I->SetDormant(true);
}
bool UTallowBatteryLoopSubsystem::IsBatteryCarried() const
{
    const auto* Player=Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); return Player&&Player->GetInventoryCount(ECarriedItemType::Fuse)>0;
}
bool UTallowBatteryLoopSubsystem::DoesBatteryExist() const
{
    if(IsBatteryCarried()) return true;
    for(TActorIterator<ATallowFusePickup> I(GetWorld());I;++I)
        if(!I->IsHidden()&&I->GetActorEnableCollision()&&!TallowHouseRooms::RoomAt(I->GetActorLocation()).IsNone()&&I->GetActorLocation().Z>-600&&I->GetActorLocation().Z<1000) return true;
    return false;
}
void UTallowBatteryLoopSubsystem::EnsureBattery()
{
    if(!bReleased||DoesBatteryExist()||!BatterySpot.IsValid()||!GetFuseBox()||GetFuseBox()->HasPower()) return;
    if(!Battery.IsValid())
    {
        UClass* Class=ATallowFusePickup::StaticClass();for(const auto& F:OriginalFuses) if(F.IsValid()) {Class=F->GetClass();break;}
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Battery=GetWorld()->SpawnActor<ATallowFusePickup>(Class,BatteryTransform,Params);
    }
    SeatBattery();Signal(ETallowChapterEvent::BatteryLost);
}
void UTallowBatteryLoopSubsystem::Signal(ETallowChapterEvent Event)
{
    OnBatteryEvent.Broadcast(Event);
    if(auto* Chapters=UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(Event);
    UE_LOG(LogTemp,Display,TEXT("TALLOW_BATTERY event=%d installed=%d"),int32(Event),GetInstalledFuses());
}
void UTallowBatteryLoopSubsystem::PollInventory()
{
    const auto* Player=Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this,0));const int32 Count=Player?Player->GetInventoryCount(ECarriedItemType::Fuse):0;
    if(bReleased&&Count>PreviousCarried) Signal(ETallowChapterEvent::BatteryCollected);
    else if(bReleased&&Count<PreviousCarried) Signal(ETallowChapterEvent::BatteryDropped);
    PreviousCarried=Count;
}
void UTallowBatteryLoopSubsystem::OnFuseInserted()
{
    PreviousCarried=0;RetireBattery();Signal(ETallowChapterEvent::FuseActivated);
}
int32 UTallowBatteryLoopSubsystem::GetInstalledFuses() const {const auto* F=GetFuseBox();return F?F->CurrentFuses:0;}
FName UTallowBatteryLoopSubsystem::GetFuseBoxRoom() const {const auto* F=GetFuseBox();return F?TallowHouseRooms::RoomAt(F->GetActorLocation()):NAME_None;}
AActor* UTallowBatteryLoopSubsystem::GetBatteryActor() const {return Battery.Get();}
