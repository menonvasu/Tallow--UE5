#include "Chapter/TallowChapterSubsystem.h"
#include "Chapter/TallowChapterDefinition.h"
#include "Chapter/TallowChapterFlow.h"
#include "Chapter/TallowChapterResources.h"
#include "Chapter/TallowChairSpot.h"
#include "Chapter/TallowStoryObject.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/TallowCreature.h"
#include "BrainComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Candle/TallowCandleComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Environment/TallowCandlePickup.h"
#include "Environment/TallowHideVolume.h"
#include "Environment/TallowHouseMapPickup.h"
#include "Environment/TallowMatchbox.h"
#include "Environment/TallowPickupSpot.h"
#include "Environment/TallowWaxPickup.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/TallowGameMode.h"
#include "HAL/IConsoleManager.h"
#include "House/TallowBatteryLoopSubsystem.h"
#include "House/TallowDoorLayoutSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "House/TallowRoomSearchSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "PatrolPoint/TallowPatrolPoint.h"
#include "Player/TallowCharacter.h"
#include "Plush/TallowPlushMim.h"
#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowPlayDirector.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Save/TallowSaveSubsystem.h"
#include "Scare/TallowScareDirector.h"
#include "Tasks/TallowChairEscape.h"
#include "Tasks/TallowCrowbarPickup.h"
#include "Tasks/TallowFilmReelPickup.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowLockpickGame.h"
#include "Tasks/TallowProjector.h"
#include "UI/Presentation/TallowHouseMapWidget.h"
#include "UI/Presentation/TallowInventoryWidget.h"
#include "UI/Presentation/TallowJournalWidget.h"
namespace
{
    TAutoConsoleVariable<int32> CVarTallowChapters(TEXT("tallow.Chapters"), 0, TEXT("Enable the chapter structure."), ECVF_Default);
    TAutoConsoleVariable<int32> CVarReloadChapter(TEXT("tallow.Chapters.ReloadOnRetry"), 0, TEXT("Reload the playable map for a chapter retry."), ECVF_Cheat);
    FName StableName(const AActor* Actor)
    {
        if (!Actor) return NAME_None;
        for (FName Tag:Actor->Tags) if (Tag.ToString().StartsWith(TEXT("Tallow.Spot."))) return FName(*Tag.ToString().RightChop(12));
        return Actor->GetFName();
    }
    template<class T> T* One(const UObject* Context) { return Cast<T>(UGameplayStatics::GetActorOfClass(Context, T::StaticClass())); }
    template<class T> TArray<T*> Sorted(UWorld* World)
    {
        TArray<T*> Actors; for (TActorIterator<T> It(World); It; ++It) Actors.Add(*It);
        Actors.Sort([](const T& A, const T& B) { return A.GetName() < B.GetName(); }); return Actors;
    }
    // Walking distance recorded for a hunt start the player cannot yet be reached from.
    constexpr float UnreachableWalkCm = 100000.f;
    float WalkingDistance(UWorld* World, const FVector& From, const FVector& To)
    {
        auto* Nav = UNavigationSystemV1::GetCurrent(World); if (!Nav) return -1.f;
        FNavLocation A, B;
        if (!Nav->ProjectPointToNavigation(From, A, FVector(80,80,200)) || !Nav->ProjectPointToNavigation(To, B, FVector(80,80,200))) return -1.f;
        auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, A.Location, B.Location);
        return Path && Path->IsValid() && !Path->IsPartial() ? Path->GetPathLength() : -1.f;
    }
    FString TransformLine(const FTransform& Transform)
    {
        const FVector P = Transform.GetLocation(); const FRotator R = Transform.Rotator();
        return FString::Printf(TEXT("%.0f,%.0f,%.0f;%.0f,%.0f,%.0f"), P.X,P.Y,P.Z,R.Pitch,R.Yaw,R.Roll);
    }
}
bool UTallowChapterSubsystem::DoesSupportWorldType(const EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
TStatId UTallowChapterSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowChapterSubsystem, STATGROUP_Tickables); }
bool UTallowChapterSubsystem::ShouldRunChapters() { return CVarTallowChapters.GetValueOnGameThread() != 0 || FParse::Param(FCommandLine::Get(), TEXT("TallowChapters")); }
UTallowChapterSubsystem* UTallowChapterSubsystem::GetActive(const UObject* Context)
{
    const auto* World = Context ? Context->GetWorld() : nullptr;
    auto* Chapters = World ? World->GetSubsystem<UTallowChapterSubsystem>() : nullptr;
    return Chapters && Chapters->IsActive() ? Chapters : nullptr;
}
AActor* UTallowChapterSubsystem::FindNamed(FName Name) const
{
    if (Name.IsNone()) return nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->GetFName() == Name || StableName(*It) == Name) return *It;
    return nullptr;
}
void UTallowChapterSubsystem::Activate(ATallowPlayDirector* InDirector)
{
    if (bActive || !InDirector || !ShouldRunChapters()) return;
    Director = InDirector; CampaignDef = UTallowCampaignDefinition::LoadOrBuildDefault(this);
    Chair = One<ATallowChairEscape>(this);
    for (auto* Crowbar : Sorted<ATallowCrowbarPickup>(GetWorld())) AuthoredCrowbars.Add(Crowbar);
    if (!Director->GetPlayerCharacter() || !Chair || !CampaignDef || CampaignDef->Chapters.IsEmpty())
    { UE_LOG(LogTemp, Error, TEXT("TALLOW_CHAPTER missing required player, chair or campaign")); return; }
    ETallowPendingStart Pending = ETallowPendingStart::None;
    if (auto* Save = UTallowSaveSubsystem::Get(this)) Save->ConsumePendingStart(Pending, Campaign);
    ETallowRestartReason Reason = Pending == ETallowPendingStart::Continue ? ETallowRestartReason::Continue : ETallowRestartReason::NewCampaign;
    if (Reason == ETallowRestartReason::NewCampaign)
    {
        int32 Seed = Campaign.CampaignSeed;
#if !UE_BUILD_SHIPPING
        FParse::Value(FCommandLine::Get(), TEXT("TallowSeed="), Seed);
#endif
        if (Director->RunSeed != 0) Seed = Director->RunSeed;
        Campaign = {}; Campaign.CampaignSeed = Seed != 0 ? Seed : FMath::Rand();
        Campaign.History.UsedChairRooms.Add(TEXT("wake"));
        Campaign.Resources = FTallowChapterResources::Capture(Director->GetPlayerCharacter());
    }
#if !UE_BUILD_SHIPPING
    int32 Requested = Campaign.ChapterIndex;
    if (FParse::Value(FCommandLine::Get(), TEXT("TallowChapter="), Requested))
    {
        Campaign.ChapterIndex = FMath::Clamp(Requested, 0, CampaignDef->Chapters.Num()-1);
        Campaign.InstalledFuses = FMath::Min(Campaign.ChapterIndex, CampaignDef->GetBatteryChapterCount());
        Reason = ETallowRestartReason::NewCampaign;
    }
#endif
    Campaign.ChapterIndex = FMath::Clamp(Campaign.ChapterIndex, 0, CampaignDef->Chapters.Num()-1);
    if (Reason == ETallowRestartReason::NewCampaign || Campaign.ChapterSeed == 0)
        Campaign.ChapterSeed = FTallowChapterFlow::ChapterSeed(Campaign.CampaignSeed, Campaign.ChapterIndex);
    bActive = true;
    if (auto* Mode = Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode())) Mode->OnCaught.AddUniqueDynamic(this, &UTallowChapterSubsystem::OnCaught);
    if (auto* Story = GetWorld()->GetGameInstance()->GetSubsystem<UTallowStorySubsystem>())
    {
        Story->OnLoreRead.AddUObject(this, &UTallowChapterSubsystem::OnLoreRead);
        Story->RestoreRead(Campaign.LoreRead);
    }
    if (UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
    {
        bAwaitingNavigation=true; PendingBuildReason=Reason; NavigationWaitStarted=GetWorld()->GetTimeSeconds();
        SetMimForm(ETallowMimForm::Absent); SetBeatInputLocked(true);
        UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER waiting-for-navigation"));
    }
    else BuildRound(Reason);
}
ETallowChapterPhase UTallowChapterSubsystem::GetEffectivePhase() const { return bActive && UGameplayStatics::IsGamePaused(this) ? ETallowChapterPhase::Paused : Phase; }
const UTallowChapterDefinition* UTallowChapterSubsystem::GetChapterDefinition() const
{ return CampaignDef && CampaignDef->Chapters.IsValidIndex(Campaign.ChapterIndex) ? CampaignDef->Chapters[Campaign.ChapterIndex].Get() : nullptr; }
ETallowChapterKind UTallowChapterSubsystem::GetChapterKind() const { const auto* Def = GetChapterDefinition(); return Def ? Def->Kind : ETallowChapterKind::Hunt; }
bool UTallowChapterSubsystem::AllowsCapture() const
{ return bActive && !BeatState.IsActive() && (Phase == ETallowChapterPhase::ActiveHunt || Phase == ETallowChapterPhase::BatteryAcquired); }
void UTallowChapterSubsystem::CloseOverlays()
{
    auto* Player = Director ? Director->GetPlayerCharacter() : nullptr;
    if (Player)
    {
        Player->PrepareForInventory();
        if (auto* Lock = Player->GetLockpickComponent(); Lock && Lock->IsInMinigame()) Lock->EndLockpickSession(false);
        if (auto* Hide = Player->GetCurrentHideVolume()) Hide->LeaveHidingSpot(true);
        Player->StowCarriedItem();
    }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), false);
    for (auto* Widget : Widgets)
    {
        if (auto* Inventory = Cast<UTallowInventoryWidget>(Widget)) Inventory->Close();
        if (auto* Journal = Cast<UTallowJournalWidget>(Widget)) { Journal->SkipReading(); Journal->Close(); }
        if (auto* Map = Cast<UTallowHouseMapWidget>(Widget)) Map->Close(true);
    }
    for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) It->CloseKeypad();
    for (TActorIterator<ATallowStoryObject> It(GetWorld()); It; ++It) It->ResetForRound();
}
void UTallowChapterSubsystem::SetBeatInputLocked(bool bLocked)
{
    if (bLocked == bBeatInputLocked) return;
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
    { PC->SetIgnoreMoveInput(bLocked); PC->SetIgnoreLookInput(bLocked); }
    bBeatInputLocked = bLocked;
}
FTransform UTallowChapterSubsystem::GetChapterStart() const
{
    if (auto* Start = FindNamed(TEXT("Pass2_ChapterStart"))) return Start->GetActorTransform();
    if (auto* Box = One<ATallowFuseBox>(this))
    {
        FVector Point = Box->GetActorLocation() + Box->GetActorForwardVector() * 150.f;
        Point.Z=TallowHouseRooms::FloorZ(TallowHouseRooms::FloorAt(Box->GetActorLocation().Z))+10.f;
        if (auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        { FNavLocation OnNav; if (Nav->ProjectPointToNavigation(Point, OnNav, FVector(50,50,60))) Point = OnNav.Location + FVector(0,0,62); }
        return FTransform((Box->GetActorLocation()-Point).Rotation(), Point);
    }
    return Chair ? Chair->GetExitRelativeTransform() * Chair->GetStartTransform() : FTransform::Identity;
}
void UTallowChapterSubsystem::GatherFallbackSpots(FTallowSolverInput& Input) const
{
    if (!Input.Spots.IsEmpty()) return;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAPTER layout-input-defaults: gathering authored markers"));
    auto Add = [&Input](AActor* Actor, ETallowSpotKind Kind, FName Room, int32 Index = -1)
    { FTallowSpotCandidate Spot; Spot.Spot=StableName(Actor); Spot.Room=Room; Spot.Kind=Kind; Spot.Transform=Actor->GetActorTransform(); Spot.ChapterIndex=Index; Input.Spots.Add(Spot); };
    for (auto* Story : Sorted<ATallowStoryObject>(GetWorld())) Add(Story, ETallowSpotKind::Story, Story->GetRoom(), Story->ChapterIndex);
    for (auto* Spot : Sorted<ATallowChairSpot>(GetWorld())) if (Spot->bEnabled) Add(Spot, ETallowSpotKind::Chair, Spot->GetRoom());
    for (auto* Spot : Sorted<ATallowPickupSpot>(GetWorld()))
        if (Spot->IsUsable()) Add(Spot, Spot->Item == ETallowPickupSpotItem::Crowbar ? ETallowSpotKind::Crowbar : Spot->Item == ETallowPickupSpotItem::Matchbox ? ETallowSpotKind::Matchbox : ETallowSpotKind::Battery, TallowHouseRooms::RoomAt(Spot->GetActorLocation()));
    if (auto* Box = One<ATallowFuseBox>(this)) Input.FuseBoxRoom = TallowHouseRooms::RoomAt(Box->GetActorLocation());
}
bool UTallowChapterSubsystem::BuildRound(ETallowRestartReason Reason)
{
    const auto* Def = GetChapterDefinition(); if (!Def || bRebuilding) return false;
    TGuardValue<bool> Rebuilding(bRebuilding, true);
    // Chairs, stories and routes are measured on navigation, so finish the previous layout's rebuild first; the cut hides it.
    if (UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
        if (auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld()))
            if (auto* NavData=Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) NavData->EnsureBuildCompletion();
    auto* Player = Director->GetPlayerCharacter();
    auto Carried = FTallowChapterResources::Capture(Player);
    Carried.bHasCandle |= Campaign.Resources.bHasCandle; Carried.bHasMap |= Campaign.Resources.bHasMap;
    BeatState.Reset(); ++Serial; SetBeatInputLocked(false); CloseOverlays();
    HandledEvents.Reset(); bWaitingForRoom = false; bWritingDone = false;
    bHuntStartPending = false; bChairRoomCommitted = false; bChairUnavailable = false;
    SetMimForm(ETallowMimForm::Absent);
    auto* Doors = UTallowDoorLayoutSubsystem::Get(this);
    const FName StartRoom = TallowHouseRooms::RoomAt(Def->bStartsInChair ? Chair->GetStartTransform().GetLocation() : GetChapterStart().GetLocation());
    SolverInput = {}; if (Doors) Doors->GatherSolverInput(StartRoom, SolverInput);
    SolverInput.StartRoom = StartRoom;
    SolverInput.bPowerRestored = Campaign.InstalledFuses >= CampaignDef->GetBatteryChapterCount();
    GatherFallbackSpots(SolverInput);
    const auto Rules = Def->MakeRules(Campaign.ChapterIndex);
    Layout = FTallowLayoutSolver::Solve(SolverInput, Rules, Campaign.ChapterSeed, Campaign.History);
    TArray<FString> Problems;
    bool bValid=FTallowLayoutSolver::Validate(SolverInput, Rules, Layout, Campaign.History, Problems);
    bUsingContractLayout=Problems.Contains(TEXT("Chapter layout contract stub"));
    if (!bValid)
    {
        UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAPTER layout-fallback %s"), *FString::Join(Problems,TEXT("; ")));
        Layout = FTallowLayoutSolver::SafeFallback(SolverInput, Rules); Layout.bFallback = true;
        Problems.Reset(); bValid=FTallowLayoutSolver::Validate(SolverInput, Rules, Layout, Campaign.History, Problems);
    }
    if (!bValid && !bUsingContractLayout)
    {
        if (bHasCommittedRound) { Campaign=CommittedCampaign; Layout=CommittedLayout; }
        bBuildFailed=true; Phase=ETallowChapterPhase::Caught; PhaseStartedAt=GetWorld()->GetTimeSeconds(); ++Serial;
        UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER build-rejected checkpoint-preserved %s"),*FString::Join(Problems,TEXT("; ")));
        SetBeatInputLocked(false);
        Director->ShowChapterRecoveryMenu();
        return false;
    }
    bBuildFailed=false;
    if (bUsingContractLayout) UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER contract-layout simulation; checkpoint writes disabled"));
    Layout.Seed = Campaign.ChapterSeed;
    // The contract fallback may return its first chair without matching the story room.
    if (bUsingContractLayout && Layout.bFallback && !Layout.StoryRoom.IsNone() && Layout.ChairRoom != Layout.StoryRoom)
        for (const auto& Spot : SolverInput.Spots) if (Spot.Kind == ETallowSpotKind::Chair && Spot.Room == Layout.StoryRoom)
        { Layout.ChairSpot = Spot.Spot; Layout.ChairRoom = Spot.Room; break; }
    if (Layout.ChairRoom.IsNone()) Layout.ChairRoom = StartRoom;
    if (Doors) { Doors->SetChapterControl(true); Doors->ApplyLayout(Layout); }
    if (auto* Battery = UTallowBatteryLoopSubsystem::Get(this)) Battery->PrepareRound(Layout, Campaign.InstalledFuses, CampaignDef->GetBatteryChapterCount(), Def->BatteryNoun);
    if (Reason == ETallowRestartReason::NextChapter) Campaign.Resources = FTallowChapterResources::ForNewChapter(Carried, Def->Resources);
    else if (Reason == ETallowRestartReason::NewCampaign) Campaign.Resources = FTallowChapterResources::ForNewChapter(Campaign.Resources, Def->Resources);
    PlaceResources(Campaign.Resources);
    for (TActorIterator<ATallowStoryObject> It(GetWorld()); It; ++It)
    { It->ResetForRound(); It->SetAvailable(*It == FindNamed(Layout.StoryObject)); }
    Chair->ReleasePlayerQuietly();
    Chair->SetActorTransform(Chair->GetStartTransform(), false, nullptr, ETeleportType::TeleportPhysics);
    if (Def->bStartsInChair)
    {
        Chair->SetActorTransform(Chair->GetStartTransform(), false, nullptr, ETeleportType::TeleportPhysics);
        Chair->SetRequiredRocks(Def->FirstChairRocks); Chair->TiePlayerToChair(Player);
    }
    else
    {
        // Reuse the chair's existing reset, then release quietly; its fallen mesh must not drift between retries.
        Chair->TiePlayerToChair(Player); Chair->ReleasePlayerQuietly();
        const FTransform Start = GetChapterStart(); Player->TeleportTo(Start.GetLocation(), Start.Rotator(), false, true);
        if (auto* PC = Player->GetController<APlayerController>()) PC->SetControlRotation(Start.Rotator());
    }
    if (auto* Search = UTallowRoomSearchSubsystem::Get(this))
    {
        Search->Begin(CampaignDef->RoomSearch); Search->ResetRound();
        // Every rebuild starts from the chapter-start visited set, including an in-world retry.
        Search->RestoreVisited(Campaign.VisitedRooms);
    }
    if (auto* Scares = UTallowScareDirector::Get(this)) Scares->BeginChapter(Campaign.ChapterIndex, CampaignDef->ScareGroups, Def->ScareGroups, CampaignDef->Scares, Campaign.ChapterSeed);
    if (Def->bPlushPhase)
    {
        UClass* PlushClass = CampaignDef->PlushClass.IsNull() ? ATallowPlushMim::StaticClass() : CampaignDef->PlushClass.LoadSynchronous();
        if (!PlushClass || !PlushClass->IsChildOf(ATallowPlushMim::StaticClass())) PlushClass = ATallowPlushMim::StaticClass();
        Plush = ATallowPlushMim::FindOrSpawn(GetWorld(), PlushClass);
        if (Plush) Plush->BeginChapter(Campaign.ChapterIndex, CampaignDef->Plush, FindNamed(Layout.StoryObject), Campaign.ChapterSeed);
        SetMimForm(ETallowMimForm::Plush);
    }
    else if (Plush) Plush->Retire();
    Director->ResetPresentationForChapter();
    for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It) It->ResetProjector();
    // The reel belongs to the final chapter: out of reach during the battery rounds, back on its shelf when the final one begins or is restored.
    {
        const auto* Projector=One<ATallowProjector>(this);
        const bool bCarrying=Player && Player->GetInventoryCount(ECarriedItemType::FilmReel)>0;
        const bool bWanted=Def->Kind==ETallowChapterKind::Final && !bCarrying && !(Projector && Projector->IsReelLoaded());
        for (TActorIterator<ATallowFilmReelPickup> It(GetWorld()); It; ++It)
        {
            if (bWanted) It->ResetReelPickup();
            else { It->SetActorHiddenInGame(true); It->SetActorEnableCollision(false); }
        }
    }
    for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) It->SetCodeVerified(false);
    Director->ActiveRunSeed = Campaign.ChapterSeed; Director->bBoardsCleared = true;
    const auto* Box = One<ATallowFuseBox>(this);
    Director->bPowerRestored = Box && Box->HasPower();
    Director->SetPowerVisualsForChapter(Director->bPowerRestored); Director->SetKeeperAwakeForChapter(false);
    if (auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Pacing->ResetPressure();
    Campaign.Resources = FTallowChapterResources::Capture(Player);
    if (!bUsingContractLayout && (Reason == ETallowRestartReason::NewCampaign || Reason == ETallowRestartReason::NextChapter))
        if (auto* Save = UTallowSaveSubsystem::Get(this)) Save->SaveCampaign(Campaign);
    Phase = ETallowChapterPhase::None;
    if (Def->Kind == ETallowChapterKind::Final) RequestBeat(ETallowChapterBeat::FinalIntro);
    else SetPhase(Def->bStartsInChair ? ETallowChapterPhase::InitialRestraint : ETallowChapterPhase::Exploration, TEXT("BuildRound"));
    OnRoundReset.Broadcast(Campaign.ChapterIndex, Reason);
    OnChapterEvent.Broadcast(Reason == ETallowRestartReason::NewCampaign || Reason == ETallowRestartReason::NextChapter ? ETallowChapterEvent::ChapterStarted : ETallowChapterEvent::RoundRebuilt, Def->ChapterId);
    CommittedCampaign=Campaign; CommittedLayout=Layout; bHasCommittedRound=true;
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAPTER rebuilt ch=%d seed=%d reason=%d"), Campaign.ChapterIndex, Campaign.ChapterSeed, int32(Reason));
    return true;
}
void UTallowChapterSubsystem::PersistDiscoveries()
{
    if (!Director || !Director->GetPlayerCharacter() || Campaign.bComplete) return;
    const auto Live=FTallowChapterResources::Capture(Director->GetPlayerCharacter());
    bool Changed=false;
    if (Live.bHasCandle && !Campaign.Resources.bHasCandle) { Campaign.Resources.bHasCandle=true; Changed=true; }
    if (Live.bHasMap && !Campaign.Resources.bHasMap) { Campaign.Resources.bHasMap=true; Changed=true; }
    if (auto* Story=GetWorld()->GetGameInstance()->GetSubsystem<UTallowStorySubsystem>())
        for (const auto Key:Story->GetReadKeys()) if (!Campaign.LoreRead.Contains(Key)) { Campaign.LoreRead.Add(Key); Changed=true; }
    if (Changed)
    {
        if (bHasCommittedRound) { CommittedCampaign.Resources.bHasCandle=Campaign.Resources.bHasCandle; CommittedCampaign.Resources.bHasMap=Campaign.Resources.bHasMap; CommittedCampaign.LoreRead=Campaign.LoreRead; }
        if (!bUsingContractLayout) if (auto* Save=UTallowSaveSubsystem::Get(this)) Save->SaveCampaign(Campaign);
    }
}
void UTallowChapterSubsystem::PlaceResources(const FTallowResourceSnapshot& Resources)
{
    auto* Player = Director->GetPlayerCharacter(); FTallowChapterResources::Restore(Player, Resources);
    for (auto* Crowbar : Sorted<ATallowCrowbarPickup>(GetWorld()))
    {
        if (!AuthoredCrowbars.Contains(Crowbar)) { Crowbar->Destroy(); continue; }
        const bool bPlace = GetChapterDefinition()->Resources.bCrowbarInHouse && !Resources.bHasCrowbar && !Layout.CrowbarSpot.IsNone();
        if (bPlace)
        {
            Crowbar->ResetPickup();
            if (auto* Spot = FindNamed(Layout.CrowbarSpot))
            { if (auto* Seat = Cast<ATallowPickupSpot>(Spot)) Seat->SeatItem(Crowbar); else Crowbar->SetActorTransform(Spot->GetActorTransform()); }
        }
        Crowbar->SetActorHiddenInGame(!bPlace); Crowbar->SetActorEnableCollision(bPlace);
    }
    for (TActorIterator<ATallowCandlePickup> It(GetWorld()); It; ++It) It->SetTakenForChapter(Resources.bHasCandle);
    for (TActorIterator<ATallowHouseMapPickup> It(GetWorld()); It; ++It) { It->SetActorHiddenInGame(Resources.bHasMap); It->SetActorEnableCollision(!Resources.bHasMap); }
    int32 Index = 0;
    for (auto* Box : Sorted<ATallowMatchbox>(GetWorld()))
    {
        bool bPlace = Campaign.ChapterIndex == 0 && (Box->ActorHasTag(TEXT("Tallow.Matchbox.Start")) || Box->GetActorNameOrLabel()==TEXT("Matchbox_Start"));
        if (!bPlace && Index < Layout.MatchboxSpots.Num())
        {
            if (auto* Spot = FindNamed(Layout.MatchboxSpots[Index++]))
            { if (auto* Seat = Cast<ATallowPickupSpot>(Spot)) Seat->SeatItem(Box); else Box->SetActorTransform(Spot->GetActorTransform()); bPlace = true; }
        }
        Box->Matches = GetChapterDefinition()->Resources.MatchesPerBox;
        if (bPlace) Box->ResetMatchbox(); else { Box->SetActorHiddenInGame(true); Box->SetActorEnableCollision(false); }
    }
    Index = 0;
    for (auto* Wax : Sorted<ATallowWaxPickup>(GetWorld()))
    {
        auto* Spot = Index < Layout.WaxSpots.Num() ? FindNamed(Layout.WaxSpots[Index++]) : nullptr;
        if (Spot) { Wax->SetActorTransform(Spot->GetActorTransform()); Wax->ResetSupply(); }
        else { Wax->SetActorHiddenInGame(true); Wax->SetActorEnableCollision(false); }
    }
}
void UTallowChapterSubsystem::SetPhase(ETallowChapterPhase Next, FName Reason)
{
    if (Next == Phase) return;
    const auto Old = Phase;
    if (!FTallowChapterFlow::CanTransition(GetChapterKind(), Old, Next))
    { UE_LOG(LogTemp, Warning, TEXT("TALLOW_CHAPTER ignored transition=%d->%d reason=%s"), int32(Old),int32(Next),*Reason.ToString()); return; }
    Phase = Next; ++Serial; PhaseStartedAt = GetWorld()->GetTimeSeconds(); HandledEvents.Reset();
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAPTER ch=%d phase=%d->%d serial=%d reason=%s"), Campaign.ChapterIndex,int32(Old),int32(Next),Serial,*Reason.ToString());
    if (auto* Scares = UTallowScareDirector::Get(this)) Scares->SetPhase(Phase);
    OnPhaseChanged.Broadcast(Old, Phase);
}
void UTallowChapterSubsystem::NotifyEvent(ETallowChapterEvent Event, FName Detail)
{
    if (!bActive || bRebuilding || HandledEvents.Contains(Event)) return;
    using E=ETallowChapterEvent; using P=ETallowChapterPhase;
    if (Event == E::StoryClueDone && (Phase == P::Exploration || Phase == P::StoryInteraction))
    {
        if (bWritingDone || BeatState.Context.Beat == ETallowChapterBeat::BloodWriting) return;
        HandledEvents.Add(Event); OnChapterEvent.Broadcast(Event, Detail); RequestBeat(ETallowChapterBeat::BloodWriting); return;
    }
    const P Next = FTallowChapterFlow::OnEvent(GetChapterKind(), Phase, Event, true);
    if (Next == Phase)
    {
        // A duplicate input after its transition is quiet; catch-up loss while already hunting is valid too.
        const bool bDuplicate = (Event == E::StoryStarted && Phase == P::StoryInteraction)
            || (Event == E::ChairEscaped && (Phase == P::Exploration || Phase == P::ActiveHunt))
            || (Event == E::BatteryCollected && Phase == P::BatteryAcquired)
            || ((Event == E::BatteryDropped || Event == E::BatteryLost) && Phase == P::ActiveHunt)
            || (Event == E::FuseActivated && (Phase == P::FuseSequence || Phase == P::ChapterTransition))
            || (Event == E::Caught && Phase == P::Caught) || (Event == E::FinalReached && Phase == P::FinalSequence)
            || (Event == E::StoryClueDone && bWritingDone);
        if (!bDuplicate) UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAPTER ignored event=%d phase=%d"),int32(Event),int32(Phase));
        return;
    }
    if (Event == E::Caught) { BeatState.Reset(); SetBeatInputLocked(false); }
    SetPhase(Next, FName(*StaticEnum<ETallowChapterEvent>()->GetNameStringByValue(int64(Event))));
    HandledEvents.Add(Event); OnChapterEvent.Broadcast(Event, Detail);
    if (Next == P::ActiveHunt && Event == E::ChairEscaped) StartHunt();
    if (Next == P::FuseSequence) RequestBeat(ETallowChapterBeat::FuseEvent);
    if (Next == P::FinalSequence)
    { Campaign.bComplete=true; if (auto* Save=UTallowSaveSubsystem::Get(this)) Save->MarkCampaignComplete(); SetMimForm(ETallowMimForm::Absent); }
}
void UTallowChapterSubsystem::OnCaught() { NotifyEvent(ETallowChapterEvent::Caught); }
void UTallowChapterSubsystem::OnLoreRead(FName Key)
{
    const auto* Def=GetChapterDefinition();
    if (Def && Def->StoryObjectCandidates.IsEmpty() && Def->StoryFlag == Key) NotifyEvent(ETallowChapterEvent::StoryClueDone, Key);
}
void UTallowChapterSubsystem::RequestBeat(ETallowChapterBeat Beat)
{
    BeatState.Reset(); ++Serial;
    auto& Context=BeatState.Context; Context.Serial=Serial; Context.Beat=Beat; Context.ChapterIndex=Campaign.ChapterIndex; Context.Room=Layout.ChairRoom;
    const auto* Def=GetChapterDefinition(); if (!Def) return;
    Context.ChairTransform=ResolveChairTransform(); Context.Where=Context.ChairTransform;
    if (bChairUnavailable && (Beat==ETallowChapterBeat::Transformation || Beat==ETallowChapterBeat::Restraint))
    {
        BeatState.Reset(); SetBeatInputLocked(false);
        UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER chair-recovery beat=%d room=%s"),int32(Beat),*Layout.ChairRoom.ToString());
        Director->ShowChapterRecoveryMenu(); return;
    }
    switch (Beat)
    {
    case ETallowChapterBeat::BloodWriting:
        Context.MaxSeconds=Def->Beats.BloodWriting; Context.Text=Def->BloodWriting; Context.Subject=FindNamed(Layout.StoryObject);
        if (auto* Story=Cast<ATallowStoryObject>(Context.Subject)) Context.Where=Story->WritingAnchor->GetComponentTransform(); break;
    case ETallowChapterBeat::Transformation:
        Context.MaxSeconds=Def->Beats.Transformation; Context.Subject=Plush;
        if (Plush) { Plush->PrepareForTransformation(Layout.ChairRoom); Context.Where=Plush->BeginTransformation(); }
        {
            float Clearance; FString Problem; FVector Mim;
            if (ATallowChairSpot::ValidatePlacement(GetWorld(), Chair, Context.ChairTransform, true, Clearance, Problem, &Mim))
                Context.Where=FTransform((Context.ChairTransform.GetLocation()-Mim).Rotation(),Mim);
            else Context.Where=FTransform(Context.ChairTransform.Rotator(),Context.ChairTransform.GetLocation()+Context.ChairTransform.GetRotation().GetForwardVector()*220.f+FVector(0,0,100));
        }
        break;
    case ETallowChapterBeat::Restraint: Context.MaxSeconds=Def->Beats.Restraint; Context.Subject=Chair; break;
    case ETallowChapterBeat::HuntRelease: Context.MaxSeconds=Def->Beats.HuntRelease; Context.Subject=Director->GetKeeper(); if (auto* Start=FindNamed(Layout.MimHuntStart)) Context.Where=Start->GetActorTransform(); break;
    case ETallowChapterBeat::FuseEvent: Context.MaxSeconds=Def->Beats.FuseEvent; Context.Subject=One<ATallowFuseBox>(this); break;
    case ETallowChapterBeat::ChapterReset:
        Context.MaxSeconds=Def->Beats.ChapterReset;
        if (CampaignDef->Chapters.IsValidIndex(Campaign.ChapterIndex+1)) Context.Text=CampaignDef->Chapters[Campaign.ChapterIndex+1]->CardTitle;
        break;
    case ETallowChapterBeat::FinalIntro: Context.MaxSeconds=Def->Beats.FinalIntro; Context.Subject=One<ATallowFuseBox>(this); break;
    default: return;
    }
    BeatState.StartedAt=GetWorld()->GetTimeSeconds(); BeatState.RequestedFrame=GFrameCounter;
    SetBeatInputLocked(true);
    UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER beat=%d serial=%d"),int32(Beat),Serial);
    const FTallowBeatContext Copy=Context; OnBeatRequested.Broadcast(Beat,Copy);
}
bool UTallowChapterSubsystem::ClaimBeat(ETallowChapterBeat Beat, int32 BeatSerial) { return BeatState.Claim(Beat,BeatSerial,GFrameCounter); }
void UTallowChapterSubsystem::NotifyBeatCut(ETallowChapterBeat Beat, int32 BeatSerial)
{
    if (!BeatState.Matches(Beat,BeatSerial) || BeatState.bCut) return;
    BeatState.bCut=true; const auto Context=BeatState.Context; ApplyBeatCut(Context);
}
void UTallowChapterSubsystem::ApplyBeatCut(const FTallowBeatContext& Context)
{
    auto* Player=Director->GetPlayerCharacter();
    switch (Context.Beat)
    {
    case ETallowChapterBeat::Transformation:
        // The encounter and restraint share one actual room, even when she left during the writing.
        // Presenters signal this cut only with the view covered.
        bChairRoomCommitted=true;
        if (TallowHouseRooms::RoomAt(Player->GetActorLocation()) != Layout.ChairRoom)
        {
            const FTransform Exit=Chair->GetExitRelativeTransform()*Context.ChairTransform;
            Player->TeleportTo(Exit.GetLocation(),Exit.Rotator(),false,true);
            if (auto* PC=Player->GetController<APlayerController>()) PC->SetControlRotation((Context.Where.GetLocation()-Exit.GetLocation()).Rotation());
        }
        if (Plush)
        {
            Plush->StageForTransformation(Context.Where,true);
            Plush->BeginTransformation();
            Plush->Retire();
        }
        SetMimForm(ETallowMimForm::Full);
        if (auto* Keeper=Director->GetKeeper())
        { Keeper->SetActorLocation(Context.Where.GetLocation(),false,nullptr,ETeleportType::TeleportPhysics); Keeper->SetActorRotation((Player->GetActorLocation()-Keeper->GetActorLocation()).Rotation()); }
        break;
    case ETallowChapterBeat::Restraint:
    {
        CloseOverlays(); ChosenChairTransform=Context.ChairTransform; Chair->ReleasePlayerQuietly();
        Chair->SetActorTransform(ChosenChairTransform,false,nullptr,ETeleportType::TeleportPhysics);
        Chair->SetRequiredRocks(GetChapterDefinition()->RestraintChairRocks); Chair->TiePlayerToChair(Player);
        auto* Candle=Player->GetCandleComponent();
        Candle->RestoreForChapter(Candle->IsAcquired(),Candle->GetWaxFraction(),Candle->GetMatchCount(),ECandleState::Out);
        if (GetChapterDefinition()->RestraintCandle==ETallowRestraintCandle::PlaceBesideChair && Candle->IsAcquired())
        {
            if (auto* Pickup=One<ATallowCandlePickup>(this))
            {
                FVector Where=ChosenChairTransform.TransformPosition(FVector(40,65,0));
                if (auto* Spot=Cast<ATallowChairSpot>(FindNamed(Layout.ChairSpot))) Where=Spot->CandlePlace->GetComponentLocation();
                Pickup->SetActorLocation(Where); Pickup->SetTakenForChapter(false);
                Candle->RestoreForChapter(false,Candle->GetWaxFraction(),Candle->GetMatchCount(),ECandleState::Out);
            }
        }
        break;
    }
    case ETallowChapterBeat::HuntRelease: SetMimForm(ETallowMimForm::Absent); break;
    case ETallowChapterBeat::FuseEvent:
        SetMimForm(ETallowMimForm::Absent);
        if (auto* Battery=UTallowBatteryLoopSubsystem::Get(this)) Battery->RetireBattery(); break;
    case ETallowChapterBeat::ChapterReset: CompleteChapter(); break;
    case ETallowChapterBeat::FinalIntro: StartHunt(); break;
    default: break;
    }
}
void UTallowChapterSubsystem::NotifyBeatFinished(ETallowChapterBeat Beat, int32 BeatSerial)
{
    if (!BeatState.Matches(Beat,BeatSerial)) return;
    NotifyBeatCut(Beat,BeatSerial);
    // ChapterReset can synchronously replace the active beat while rebuilding the final chapter.
    if (!BeatState.Matches(Beat,BeatSerial)) return;
    BeatState.Reset(); SetBeatInputLocked(false);
    switch (Beat)
    {
    case ETallowChapterBeat::BloodWriting:
        bWritingDone=true; bWaitingForRoom=true; DeferredAt=GetWorld()->GetTimeSeconds();
        if (TallowHouseRooms::RoomAt(Director->GetPlayerCharacter()->GetActorLocation())==Layout.ChairRoom) BeginTransformation(false);
        break;
    case ETallowChapterBeat::Transformation:
        OnChapterEvent.Broadcast(ETallowChapterEvent::TransformationFinished,Layout.ChairRoom);
        SetPhase(ETallowChapterPhase::Restraint,TEXT("TransformationFinished")); RequestBeat(ETallowChapterBeat::Restraint); break;
    case ETallowChapterBeat::Restraint:
        SetPhase(ETallowChapterPhase::HuntPreparation,TEXT("RestraintFinished")); RequestBeat(ETallowChapterBeat::HuntRelease); break;
    case ETallowChapterBeat::FuseEvent:
        SetPhase(ETallowChapterPhase::ChapterTransition,TEXT("FuseEventFinished")); RequestBeat(ETallowChapterBeat::ChapterReset); break;
    case ETallowChapterBeat::FinalIntro: SetPhase(ETallowChapterPhase::ActiveHunt,TEXT("FinalIntroFinished")); break;
    default: break;
    }
}
void UTallowChapterSubsystem::BeginTransformation(bool bDisplaced)
{
    if (!bWaitingForRoom) return; bWaitingForRoom=false;
    if (bDisplaced) UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER transformation-displaced"));
    SetPhase(ETallowChapterPhase::Transformation,TEXT("WritingFinished"));
    SetMimForm(ETallowMimForm::Transforming);
    OnChapterEvent.Broadcast(ETallowChapterEvent::TransformationStarted,Layout.ChairRoom);
    RequestBeat(ETallowChapterBeat::Transformation);
}
void UTallowChapterSubsystem::SetMimForm(ETallowMimForm Form, bool bHunting)
{
    auto* Keeper=Director ? Director->GetKeeper() : nullptr;
    const bool bPresent=Form==ETallowMimForm::Full || Form==ETallowMimForm::Transforming;
    bHuntEnabled=bPresent && bHunting;
    if (Keeper)
    {
        if (auto* Hunter=Cast<ATallowHunterAIController>(Keeper->GetController())) Hunter->SetHuntEnabled(bHuntEnabled);
        Keeper->SetActorHiddenInGame(!bPresent); Keeper->SetActorEnableCollision(bPresent);
        if (bHuntEnabled) Keeper->GetCharacterMovement()->SetMovementMode(MOVE_Walking); else Keeper->GetCharacterMovement()->DisableMovement();
        if (!bPresent) Keeper->SetActorLocation(FVector(780,-650,-3000),false,nullptr,ETeleportType::TeleportPhysics);
    }
    if (Director) Director->SetKeeperAwakeForChapter(bHuntEnabled);
    if (Plush && (Form==ETallowMimForm::Absent || Form==ETallowMimForm::Full)) Plush->Retire();
    if (MimForm!=Form) { MimForm=Form; OnMimFormChanged.Broadcast(Form); }
}
void UTallowChapterSubsystem::StartHunt()
{
    auto* Player=Director->GetPlayerCharacter(); auto* Keeper=Director->GetKeeper(); if (!Keeper) return;
    const FVector From=GetChapterKind()==ETallowChapterKind::Final ? Player->GetActorLocation() : Chair->GetExitTransform().GetLocation();
    const FName PlayerRoom=TallowHouseRooms::RoomAt(Player->GetActorLocation());
    const float Required=GetChapterDefinition()->HuntStartMinWalkCm;
    auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld());
    const float Radius=Keeper->GetCapsuleComponent()->GetScaledCapsuleRadius(), Half=Keeper->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    float Best=-1.f; AActor* Chosen=nullptr; int32 Considered=0, SameRoom=0, Unreachable=0, Short=0, Blocked=0, Seen=0; float Farthest=-1.f;
    // Mim starts only where the whole body fits on navigation, in another room, far enough to walk and out of sight.
    // While she is still shut in (the workshop barricade in chapter one) nothing can walk to her yet; a start she cannot
    // reach counts as far enough when it connects to the chapter start by the fuse box she has to reach.
    const FVector Meeting=GetChapterStart().GetLocation();
    auto Consider=[&](AActor* Point, bool bAllowShutIn)
    {
        if (!Point || !Nav) return; ++Considered;
        const FName Room=TallowHouseRooms::RoomAt(Point->GetActorLocation());
        if (Room.IsNone() || Room==PlayerRoom) { ++SameRoom; return; }
        float Distance=WalkingDistance(GetWorld(),From,Point->GetActorLocation());
        if (Distance<0.f)
        {
            if (!bAllowShutIn || WalkingDistance(GetWorld(),Point->GetActorLocation(),Meeting)<0.f) { ++Unreachable; return; }
            Distance=UnreachableWalkCm;
        }
        else Farthest=FMath::Max(Farthest,Distance);
        if (Distance<Required) { ++Short; return; }
        if (Distance<=Best) return;
        FNavLocation OnNav; if (!Nav->ProjectPointToNavigation(Point->GetActorLocation(),OnNav,FVector(50,50,200))) { ++Unreachable; return; }
        const FVector Standing=OnNav.Location+FVector(0,0,Half);
        FCollisionQueryParams Params(TEXT("ChapterHuntSight"),true,Player); Params.AddIgnoredActor(Keeper);
        if (GetWorld()->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half-.5f),Params)) { ++Blocked; return; }
        const FVector Eye=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
        FHitResult Hit;
        for (const float Up : {0.f,Half*.8f})
            if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Standing+FVector(0,0,Up),ECC_Visibility,Params)) { ++Seen; return; }
        Best=Distance; Chosen=Point;
    };
    for (const bool bAllowShutIn : {false,true})
    {
        if (Chosen) break;
        Consider(FindNamed(Layout.MimHuntStart),bAllowShutIn);
        if (!Chosen)
            for (auto* Point : Sorted<ATallowPatrolPoint>(GetWorld())) Consider(Point,bAllowShutIn);
    }
    if (!Chosen)
    {
        // No fair start yet: Mim stays away, the watchdog retries, then the existing retry menu is offered.
        if (!bHuntStartPending)
        {
            bHuntStartPending=true; HuntPendingSince=GetWorld()->GetTimeSeconds();
            UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER hunt-start-deferred required=%.0f room=%s considered=%d same-room=%d unreachable=%d short=%d blocked=%d seen=%d farthest=%.0f"),
                Required,*PlayerRoom.ToString(),Considered,SameRoom,Unreachable,Short,Blocked,Seen,Farthest);
        }
        SetMimForm(ETallowMimForm::Absent);
        return;
    }
    bHuntStartPending=false;
    HuntTransform=Chosen->GetActorTransform(); HuntStartWalkingDistance=Best;
    if (auto* Creature=Cast<ATallowHunterCreature>(Keeper)) Creature->SetResetLocation(HuntTransform.GetLocation());
    Keeper->ResetToSpawn();
    if (auto* Hunter=Cast<ATallowHunterAIController>(Keeper->GetController())) Hunter->ResetHunt();
    SetMimForm(ETallowMimForm::Full,true);
    if (auto* Pacing=GetWorld()->GetSubsystem<UTallowHunterDirector>()) Pacing->ResetPressure();
    if (auto* Battery=UTallowBatteryLoopSubsystem::Get(this); Battery && GetChapterKind()==ETallowChapterKind::Hunt) Battery->ReleaseBattery();
    UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER hunt-start walk=%.0f required=%.0f actor=%s"),Best,Required,*GetNameSafe(Chosen));
}
FTransform UTallowChapterSubsystem::ResolveChairTransform()
{
    bChairUnavailable=false;
    auto UsableIn=[this](FName Room)->ATallowChairSpot*
    {
        for (auto* Spot : Sorted<ATallowChairSpot>(GetWorld())) if (Spot->GetRoom()==Room && Spot->IsUsable()) return Spot;
        return nullptr;
    };
    if (auto* Chosen=Cast<ATallowChairSpot>(FindNamed(Layout.ChairSpot)); Chosen && Chosen->GetRoom()==Layout.ChairRoom && Chosen->IsUsable()) return Chosen->GetActorTransform();
    if (auto* Spot=UsableIn(Layout.ChairRoom)) { Layout.ChairSpot=StableName(Spot); return Spot->GetActorTransform(); }
    // Until the transformation is staged, the encounter and its restraint can still move together to another measured room.
    if (!bChairRoomCommitted)
        for (const auto& Room : TallowHouseRooms::All())
        {
            if (!Room.bChairAllowed || Room.bPowerGated || Room.Id==Layout.ChairRoom || Room.Id==Layout.BatteryRoom || !UsableIn(Room.Id)) continue;
            auto Retargeted=Layout;
            if (!FTallowLayoutSolver::Retarget(SolverInput,GetChapterDefinition()->MakeRules(Campaign.ChapterIndex),Room.Id,Campaign.ChapterSeed,Campaign.History,Retargeted)) continue;
            if (auto* Spot=UsableIn(Retargeted.ChairRoom))
            {
                Layout=Retargeted; Layout.ChairSpot=StableName(Spot);
                UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER chair-retarget room=%s"),*Layout.ChairRoom.ToString());
                return Spot->GetActorTransform();
            }
        }
    bChairUnavailable=true;
    UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER chair-unavailable room=%s"),*Layout.ChairRoom.ToString());
    return Chair ? Chair->GetActorTransform() : FTransform::Identity;
}
void UTallowChapterSubsystem::CompleteChapter()
{
    PersistDiscoveries();
    const auto* Def=GetChapterDefinition(); Campaign.CompletedChapters.AddUnique(Def->ChapterId);
    if (!Def->StoryFlag.IsNone()) Campaign.StoryFlags.AddUnique(Def->StoryFlag);
    if (!Layout.CrowbarSpot.IsNone()) Campaign.History.UsedCrowbarSpots.AddUnique(Layout.CrowbarSpot);
    Campaign.History.UsedBatterySpots.AddUnique(Layout.BatterySpot); Campaign.History.UsedChairRooms.AddUnique(Layout.ChairRoom); Campaign.History.UsedStoryObjects.AddUnique(Layout.StoryObject);
    Campaign.History.PreviousDoors=Layout.Doors;
    Campaign.History.PreviousDoors.Sort([](const FTallowDoorAssignment& A,const FTallowDoorAssignment& B){return A.Edge.LexicalLess(B.Edge);});
    Campaign.History.PreviousDoorHash=Layout.DoorHash();
    if (auto* Story=GetWorld()->GetGameInstance()->GetSubsystem<UTallowStorySubsystem>()) Campaign.LoreRead=Story->GetReadKeys();
    if (auto* Search=UTallowRoomSearchSubsystem::Get(this)) Campaign.VisitedRooms=Search->GetVisited();
    Campaign.InstalledFuses=FMath::Max(Campaign.InstalledFuses+1,UTallowBatteryLoopSubsystem::Get(this)->GetInstalledFuses());
    Campaign.InstalledFuses=FMath::Min(Campaign.InstalledFuses,CampaignDef->GetBatteryChapterCount());
    const FName CompletedId=Def->ChapterId;
    if (Campaign.ChapterIndex+1>=CampaignDef->Chapters.Num()) return;
    ++Campaign.ChapterIndex; Campaign.Attempt=0; Campaign.ChapterSeed=FTallowChapterFlow::ChapterSeed(Campaign.CampaignSeed,Campaign.ChapterIndex);
    if (BuildRound(ETallowRestartReason::NextChapter)) OnChapterEvent.Broadcast(ETallowChapterEvent::ChapterCompleted,CompletedId);
}
void UTallowChapterSubsystem::RestartChapter(ETallowRestartReason Reason)
{
    if (!bActive) return;
    PersistDiscoveries();
    UGameplayStatics::SetGamePaused(this,false);
    if (auto* Mode=Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode())) Mode->ClearCaptureState();
    ++Campaign.Attempt;
    if (CVarReloadChapter.GetValueOnGameThread()!=0)
    {
        if (auto* Save=UTallowSaveSubsystem::Get(this); Save && Save->SaveCampaign(Campaign) && Save->PrepareContinue())
        { Director->ReloadGameMap(); return; }
        UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER reload-retry unavailable; rebuilding live"));
    }
    BuildRound(Reason);
}
bool UTallowChapterSubsystem::JumpToChapter(int32 Index, ETallowChapterPhase Target)
{
    if (!bActive || !CampaignDef->Chapters.IsValidIndex(Index)) return false;
    PersistDiscoveries();
    UGameplayStatics::SetGamePaused(this,false);
    if (auto* Mode=Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode())) Mode->ClearCaptureState();
    Campaign.ChapterIndex=Index; Campaign.ChapterSeed=FTallowChapterFlow::ChapterSeed(Campaign.CampaignSeed,Index); Campaign.Attempt=0;
    Campaign.InstalledFuses=FMath::Min(Index,CampaignDef->GetBatteryChapterCount());
    // A jump skips the chapter that would have recorded the doors to keep, so the jumped round rolls its own.
    Campaign.History.PreviousDoors.Reset(); Campaign.History.PreviousDoorHash=0;
    if (!BuildRound(ETallowRestartReason::Debug)) return false;
    if (Target!=ETallowChapterPhase::None && Target!=ETallowChapterPhase::Paused)
    {
        BeatState.Reset(); SetBeatInputLocked(false); Chair->ReleasePlayerQuietly();
        const auto Old=Phase; Phase=Target; ++Serial; PhaseStartedAt=GetWorld()->GetTimeSeconds();
        OnPhaseChanged.Broadcast(Old,Phase);
        if (Target==ETallowChapterPhase::ActiveHunt || Target==ETallowChapterPhase::BatteryAcquired) StartHunt();
        if (auto* Scares=UTallowScareDirector::Get(this)) Scares->SetPhase(Phase);
    }
    return true;
}
void UTallowChapterSubsystem::Tick(float DeltaTime)
{
    if (bAwaitingNavigation)
    {
        if (!UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
        { bAwaitingNavigation=false; bActive=true; BuildRound(PendingBuildReason); }
        else if (GetWorld()->GetTimeSeconds()-NavigationWaitStarted>15.f)
        { bAwaitingNavigation=false; SetBeatInputLocked(false); UE_LOG(LogTemp,Error,TEXT("TALLOW_CHAPTER build-failed navigation unavailable")); }
        return;
    }
    if (!bActive || bRebuilding || bBuildFailed || UGameplayStatics::IsGamePaused(this)) return;
    Campaign.PlaySeconds+=DeltaTime;
    const double Now=GetWorld()->GetTimeSeconds();
    if (BeatState.IsActive())
    {
        const auto Beat=BeatState.Context.Beat; const int32 BeatSerial=BeatState.Context.Serial;
        if (!BeatState.bClaimed && BeatState.FallbackAt<0.0 && GFrameCounter>BeatState.RequestedFrame)
        { BeatState.FallbackAt=Now; TallowChapterBeats::Fade(this,true,.6f); }
        if (BeatState.FallbackAt>=0.0)
        {
            const double Age=Now-BeatState.FallbackAt;
            if (Age>=.6 && !BeatState.bCut) NotifyBeatCut(Beat,BeatSerial);
            if (BeatState.Matches(Beat,BeatSerial) && Age>=1.0 && Age-DeltaTime<1.0) TallowChapterBeats::Fade(this,false,.8f);
            if (BeatState.Matches(Beat,BeatSerial) && Age>=1.8) NotifyBeatFinished(Beat,BeatSerial);
            else if (!BeatState.Matches(Beat,BeatSerial)) TallowChapterBeats::Fade(this,false,.8f);
        }
    }
    if (bWaitingForRoom && TallowHouseRooms::RoomAt(Director->GetPlayerCharacter()->GetActorLocation())==Layout.ChairRoom) BeginTransformation(false);
    if (Now>=NextWatchdog) { NextWatchdog=Now+1.0; RunWatchdogs(); }
}
void UTallowChapterSubsystem::RunWatchdogs()
{
    if (!bActive || bRebuilding || bBuildFailed) return;
    PersistDiscoveries();
    const double Now=GetWorld()->GetTimeSeconds(); auto* Player=Director->GetPlayerCharacter();
    if (BeatState.IsActive() && Now-BeatState.StartedAt>BeatState.Context.MaxSeconds)
    { UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER beat-timeout beat=%d"),int32(BeatState.Context.Beat)); SkipBeat(); }
    if (bWaitingForRoom && Now-DeferredAt>=GetChapterDefinition()->Beats.TransformationDeferSeconds)
    {
        const FName Room=TallowHouseRooms::RoomAt(Player->GetActorLocation()); const auto* Def=TallowHouseRooms::Find(Room);
        bool bUsable=false; for (auto* Spot : Sorted<ATallowChairSpot>(GetWorld())) if (Spot->GetRoom()==Room && Spot->IsUsable()) bUsable=true;
        if (Def && Def->bChairAllowed && !Def->bPowerGated && bUsable)
            FTallowLayoutSolver::Retarget(SolverInput,GetChapterDefinition()->MakeRules(Campaign.ChapterIndex),Room,Campaign.ChapterSeed,Campaign.History,Layout);
        BeginTransformation(true);
    }
    if (Phase==ETallowChapterPhase::HuntPreparation && !Player->IsTiedToChair() && !BeatState.IsActive()) NotifyEvent(ETallowChapterEvent::ChairEscaped);
    if ((Phase==ETallowChapterPhase::Exploration || Phase==ETallowChapterPhase::StoryInteraction) && Player->IsTiedToChair()) Chair->ReleasePlayerQuietly();
    if (Phase==ETallowChapterPhase::ActiveHunt && GetChapterKind()==ETallowChapterKind::Hunt)
        if (auto* Battery=UTallowBatteryLoopSubsystem::Get(this); Battery && !Battery->DoesBatteryExist() && !Battery->IsBatteryCarried())
        { Battery->EnsureBattery(); NotifyEvent(ETallowChapterEvent::BatteryLost); }
    const FVector P=Player->GetActorLocation();
    if (P.Z<-1500 || P.Z>1500 || P.X < -200 || P.X>1775 || P.Y < -1500 || P.Y>200)
    {
        const auto Safe=AllowsCapture() ? Chair->GetExitTransform() : GetChapterStart();
        Player->TeleportTo(Safe.GetLocation(),Safe.Rotator(),false,true);
        UE_LOG(LogTemp,Display,TEXT("TALLOW_CHAPTER player-recovered"));
    }
    if (Phase==ETallowChapterPhase::Exploration || Phase==ETallowChapterPhase::StoryInteraction)
        SetMimForm(GetChapterDefinition()->bPlushPhase ? ETallowMimForm::Plush : ETallowMimForm::Absent);
    else if (AllowsCapture())
    {
        if (!bHuntStartPending) SetMimForm(ETallowMimForm::Full,true);
        else if (Now-HuntPendingSince<10.0) StartHunt();
        else if (!Director->IsMenuOpen())
        { UE_LOG(LogTemp,Warning,TEXT("TALLOW_CHAPTER hunt-start-recovery")); Director->ShowChapterRecoveryMenu(); }
    }
    if (Phase==ETallowChapterPhase::Caught && Now-PhaseStartedAt>=10.0)
        if (auto* Mode=Cast<ATallowGameMode>(GetWorld()->GetAuthGameMode()); Mode && !Mode->IsCaptureInProgress() && !Director->IsMenuOpen()) RestartChapter(ETallowRestartReason::Recovery);
}
void UTallowChapterSubsystem::SkipBeat()
{
    if (!BeatState.IsActive()) return;
    const auto Context=BeatState.Context; NotifyBeatFinished(Context.Beat,Context.Serial); TallowChapterBeats::Fade(this,false,.8f);
}
ETallowStoryAct UTallowChapterSubsystem::GetPacingAct() const
{
    const auto* Def=GetChapterDefinition(); if (!Def) return ETallowStoryAct::Dormant;
    if (Def->Kind==ETallowChapterKind::Final)
    {
        if (const auto* Escape=One<ATallowEscapePoint>(this); Escape && Escape->IsCodeVerified()) return ETallowStoryAct::EndRun;
        const auto* Player=Director->GetPlayerCharacter(); const auto* Projector=One<ATallowProjector>(this);
        return (Player && Player->GetInventoryCount(ECarriedItemType::FilmReel)>0) || (Projector && Projector->IsReelLoaded()) ? ETallowStoryAct::Reel : ETallowStoryAct::HouseWakes;
    }
    if (Phase==ETallowChapterPhase::FuseSequence || Phase==ETallowChapterPhase::ChapterTransition) return ETallowStoryAct::Dormant;
    return Phase==ETallowChapterPhase::ActiveHunt || Phase==ETallowChapterPhase::BatteryAcquired ? Def->HuntAct : Def->ExplorationAct;
}
AActor* UTallowChapterSubsystem::GetGuideObjective(ETallowGuideKind& OutKind) const
{
    OutKind=ETallowGuideKind::None;
    if (Phase==ETallowChapterPhase::Exploration)
        if (auto* Story=Cast<ATallowStoryObject>(FindNamed(Layout.StoryObject)); Story && Story->IsAvailable() && !Story->IsPlaying())
        { OutKind=ETallowGuideKind::Tape; return Story; }
    if (Phase==ETallowChapterPhase::ActiveHunt && GetChapterDefinition()->bDraughtGuidesToBattery)
        if (auto* Battery=UTallowBatteryLoopSubsystem::Get(this))
            if (auto* Actor=Battery->GetBatteryActor(); Actor && !Actor->IsHidden() && !Battery->IsBatteryCarried()) { OutKind=ETallowGuideKind::Battery; return Actor; }
    if (Phase==ETallowChapterPhase::BatteryAcquired) if (auto* Box=One<ATallowFuseBox>(this)) { OutKind=ETallowGuideKind::FuseBox; return Box; }
    return nullptr;
}
FVector UTallowChapterSubsystem::GetGuideTarget(float* OutWalk) const
{
    if (OutWalk) *OutWalk=0.f;
    ETallowGuideKind Kind; const AActor* Objective=GetGuideObjective(Kind);
    return Objective ? NextCorner(Objective->GetActorLocation(),OutWalk) : FVector::ZeroVector;
}
FVector UTallowChapterSubsystem::NextCorner(const FVector& Target, float* OutWalk) const
{
    // The draught leans toward the next corner of her walk, so it turns at doors instead of pointing through walls.
    const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0); auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld());
    if (OutWalk && Player) *OutWalk=FVector::Dist(Player->GetActorLocation(),Target);
    if (!Player || !Nav) return Target;
    FNavLocation From,To;
    const FVector Feet=Player->GetActorLocation()-FVector(0,0,60);
    FVector Goal=Target; Goal.Z=TallowHouseRooms::FloorZ(TallowHouseRooms::FloorAt(Target.Z))+10;
    if (!Nav->ProjectPointToNavigation(Feet,From,FVector(60,60,120)) || !Nav->ProjectPointToNavigation(Goal,To,FVector(120,120,60))) return Target;
    const UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),From.Location,To.Location);
    if (!Path || !Path->IsValid() || Path->PathPoints.Num()<2) return Target;
    if (OutWalk) *OutWalk=Path->GetPathLength();
    // A corner she is already rounding is passed over: the lean has turned to the leg beyond it.
    for (int32 I=1;I<Path->PathPoints.Num()-1;++I)
        if (FVector::Dist2D(Path->PathPoints[I],From.Location)>=90.f) return Path->PathPoints[I]+FVector(0,0,60);
    return Target;
}
FText UTallowChapterSubsystem::GetObjectiveText() const { const auto* D=GetChapterDefinition(); const auto* T=D ? D->Objectives.Find(Phase) : nullptr; return T ? *T : FText::GetEmpty(); }
FText UTallowChapterSubsystem::GetMapClue() const { const auto* D=GetChapterDefinition(); const auto* T=D ? D->MapClues.Find(Phase) : nullptr; return T ? *T : FText::GetEmpty(); }
int32 UTallowChapterSubsystem::GetChapterCardIndex() const { return Campaign.ChapterIndex; }
FText UTallowChapterSubsystem::GetChapterCardNumber() const { const auto* D=GetChapterDefinition(); return D ? D->CardNumber : FText::GetEmpty(); }
FText UTallowChapterSubsystem::GetChapterCardTitle() const { const auto* D=GetChapterDefinition(); return D ? D->CardTitle : FText::GetEmpty(); }
FString UTallowChapterSubsystem::BuildRoundFingerprint() const
{
    TArray<FString> Lines;
    if (auto* Doors=UTallowDoorLayoutSubsystem::Get(this)) Doors->DescribeLive(Lines);
    if (auto* Battery=UTallowBatteryLoopSubsystem::Get(this))
    {
        auto* Actor=Battery->GetBatteryActor();
        Lines.Add(FString::Printf(TEXT("battery=%s hidden=%d at=%s"),*GetNameSafe(Actor),Actor ? Actor->IsHidden() : 1,Actor ? *TransformLine(Actor->GetActorTransform()) : TEXT("none")));
    }
    for (auto* Actor : Sorted<ATallowCrowbarPickup>(GetWorld())) Lines.Add(FString::Printf(TEXT("crowbar=%s hidden=%d at=%s"),*Actor->GetName(),Actor->IsHidden(),*TransformLine(Actor->GetActorTransform())));
    for (auto* Actor : Sorted<ATallowMatchbox>(GetWorld())) Lines.Add(FString::Printf(TEXT("matches=%s hidden=%d count=%d at=%s"),*Actor->GetName(),Actor->IsHidden(),Actor->Matches,*TransformLine(Actor->GetActorTransform())));
    const auto* Player=Director->GetPlayerCharacter(); const auto R=FTallowChapterResources::Capture(Player);
    Lines.Add(FString::Printf(TEXT("resources matches=%d wax=%.2f candle=%d map=%d crowbar=%d fuse=%d reel=%d"),R.Matches,R.Wax,R.bHasCandle,R.bHasMap,Player->GetInventoryCount(ECarriedItemType::Crowbar),Player->GetInventoryCount(ECarriedItemType::Fuse),Player->GetInventoryCount(ECarriedItemType::FilmReel)));
    const auto* Box=One<ATallowFuseBox>(this); Lines.Add(FString::Printf(TEXT("fusebox=%d"),Box ? Box->CurrentFuses : -1));
    const auto* Keeper=Director->GetKeeper(); const auto* Hunter=Keeper ? Cast<ATallowHunterAIController>(Keeper->GetController()) : nullptr;
    const bool bLiveHunt=Hunter && Hunter->GetBrainComponent() && !Hunter->GetBrainComponent()->IsPaused();
    Lines.Add(FString::Printf(TEXT("mim form=%d hidden=%d hunt=%d plush=%d"),int32(MimForm),Keeper ? Keeper->IsHidden() : 1,bLiveHunt,Plush ? int32(Plush->GetPlushState()) : -1));
    Lines.Add(FString::Printf(TEXT("chair=%s tied=%d phase=%d"),Chair ? *TransformLine(Chair->GetActorTransform()) : TEXT("none"),Player->IsTiedToChair(),int32(Phase)));
    if (auto* Search=UTallowRoomSearchSubsystem::Get(this)) for (const auto& Room : TallowHouseRooms::All())
    { const auto State=Search->GetRoomState(Room.Id); Lines.Add(FString::Printf(TEXT("room=%s searched=%d"),*Room.Id.ToString(),State.bSearched)); }
    if (auto* Scares=UTallowScareDirector::Get(this)) Lines.Add(FString::Printf(TEXT("scares=%d"),Scares->GetChapterEventCount()));
    Lines.Sort(); return FString::Join(Lines,TEXT("\n"));
}
FString UTallowChapterSubsystem::DescribeState() const
{
    const auto* Pacing=GetWorld()->GetSubsystem<UTallowHunterDirector>();
    return FString::Printf(TEXT("TALLOW_CHAPTER ch=%d phase=%d serial=%d campaign=%d seed=%d attempt=%d story=%s chair=%s battery=%s beat=%d mim=%d act=%d tension=%d aggression=%.3f walk=%.0f\n%s"),Campaign.ChapterIndex,int32(Phase),Serial,Campaign.CampaignSeed,Campaign.ChapterSeed,Campaign.Attempt,*Layout.StoryObject.ToString(),*Layout.ChairRoom.ToString(),*Layout.BatterySpot.ToString(),int32(BeatState.Context.Beat),int32(MimForm),Pacing ? int32(Pacing->GetAct()) : -1,Pacing ? int32(Pacing->GetPhase()) : -1,Pacing ? Pacing->GetAggression() : 0.f,Pacing ? Pacing->GetWalkingDistance() : 0.f,*BuildRoundFingerprint());
}
