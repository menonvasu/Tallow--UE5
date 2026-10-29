#include "Environment/TallowHouseRunLayout.h"
#include "Environment/TallowDoor.h"
#include "Environment/TallowOptionalBoards.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowPlayDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneComponent.h"
#include "Misc/Crc.h"

ATallowHouseRunLayout::ATallowHouseRunLayout()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Layout"));
}

TArray<bool> ATallowHouseRunLayout::PreviewStartingLocks(int32 Seed) const
{
    TArray<bool> Result; Result.Init(false,Candidates.Num());
    TArray<FName> Groups;
    for (const auto& Candidate : Candidates) Groups.AddUnique(Candidate.RouteGroup);
    for (FName Group : Groups)
    {
        TArray<int32> Members;
        for (int32 I=0; I<Candidates.Num(); ++I)
            if (Candidates[I].RouteGroup == Group && IsValid(Candidates[I].Door)) Members.Add(I);
        Members.Sort([&](int32 A,int32 B){ return Candidates[A].VariantIndex < Candidates[B].VariantIndex; });
        // No lock, first door, or second door. Never lock every entrance.
        FRandomStream Random(Seed ^ int32(FCrc::StrCrc32(*Group.ToString())) ^ 0x1974A6);
        const int32 Choice = Random.RandRange(0,Members.Num());
        if (Choice > 0) Result[Members[Choice-1]] = true;
    }
    // Every run meets the lockpick. A lock behind boards is never seen, so when the roll leaves no
    // unboarded door locked, one room's lock moves to its open entrance (its other door is boarded,
    // so the room stays reachable by prying, and no room is ever locked on both doors).
    auto Open = [&](int32 I) { return IsValid(Candidates[I].Door) && !(IsValid(Candidates[I].OptionalBoards) && Candidates[I].OptionalBoards->IsSelectedForSeed(Seed)); };
    bool bMet = false;
    TArray<int32> Entrances;
    for (int32 I=0; I<Candidates.Num(); ++I) if (Open(I)) { Entrances.Add(I); bMet |= Result[I]; }
    if (!bMet && Entrances.Num())
    {
        FRandomStream Random(Seed ^ 0x10C4D00);
        const int32 Chosen = Entrances[Random.RandRange(0,Entrances.Num()-1)];
        for (int32 I=0; I<Candidates.Num(); ++I) if (Candidates[I].RouteGroup == Candidates[Chosen].RouteGroup) Result[I] = false;
        Result[Chosen] = true;
    }
    return Result;
}

void ATallowHouseRunLayout::RestoreStartingLayout()
{
    if (!bLayoutReady) return;
    for (int32 I=0; I<Candidates.Num(); ++I)
        if (IsValid(Candidates[I].Door) && StartingLocks.IsValidIndex(I))
            Candidates[I].Door->SetLocked(StartingLocks[I]);
    OnRunDoorsChanged.Broadcast();
}

TArray<FTallowRunDoorState> ATallowHouseRunLayout::GetDoorStates() const
{
    TArray<FTallowRunDoorState> Result;
    for (int32 I=0; I<Candidates.Num(); ++I)
    {
        const auto& Candidate = Candidates[I];
        if (!IsValid(Candidate.Door)) continue;
        FTallowRunDoorState State;
        State.Door = Candidate.Door; State.RouteGroup = Candidate.RouteGroup;
        State.DoorwayPosition = Candidate.Door->GetRootComponent()->Bounds.Origin;
        State.DoorwayPosition.Z = Candidate.Door->GetActorLocation().Z;
        State.bLocked = Candidate.Door->bIsLocked;
        State.bBarricaded = IsValid(Candidate.OptionalBoards) && Candidate.OptionalBoards->IsRouteBlocked();
        State.bStartsLocked = StartingLocks.IsValidIndex(I) && StartingLocks[I];
        Result.Add(State);
    }
    return Result;
}

void ATallowHouseRunLayout::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this,ATallowPlayDirector::StaticClass()));
    auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!bChapterControlled && (!Director || !Director->ActiveRunSeed || !Player)) return;
    if (!bChapterControlled && (!bLayoutReady || LayoutSeed != Director->ActiveRunSeed))
    {
        LayoutSeed = Director->ActiveRunSeed;
        StartingLocks = PreviewStartingLocks(LayoutSeed);
        bLayoutReady = true;
        RestoreStartingLayout();
        bWasTied = Player->IsTiedToChair();
    }
    const bool bTied = Player && Player->IsTiedToChair();
    // The native game reset restores map defaults. Reapply this run's original
    // lock roll after that reset, preserving ordinary unlocks during play.
    if (!bChapterControlled && bTied && !bWasTied) RestoreStartingLayout();
    bWasTied = bTied;
    uint32 StateHash = 0;
    for (const auto& State : GetDoorStates())
        StateHash = HashCombine(StateHash,HashCombine(GetTypeHash(State.Door.Get()),uint32(State.bLocked) | (uint32(State.bBarricaded)<<1)));
    if (StateHash != PreviousStateHash)
    {
        PreviousStateHash = StateHash;
        OnRunDoorsChanged.Broadcast();
    }
}
