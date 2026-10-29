#include "House/TallowRoomSearchSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Chapter/TallowStoryObject.h"
#include "Environment/TallowPickupSpot.h"
#include "Environment/TallowHideVolume.h"
#include "Environment/TallowJournal.h"
#include "Environment/TallowMatchbox.h"
#include "Candle/TallowCandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UTallowRoomSearchSubsystem::DoesSupportWorldType(const EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
TStatId UTallowRoomSearchSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowRoomSearchSubsystem, STATGROUP_Tickables); }
UTallowRoomSearchSubsystem* UTallowRoomSearchSubsystem::Get(const UObject* Context) { const auto* World = Context ? Context->GetWorld() : nullptr; return World ? World->GetSubsystem<UTallowRoomSearchSubsystem>() : nullptr; }
void UTallowRoomSearchSubsystem::Begin(const FTallowRoomSearchRules& Rules)
{
    Tracker.Begin(Rules); bRunning = true; SampleAge = 0.f; SearchActors.Reset();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->IsA<ATallowPickupSpot>() || It->IsA<ATallowHideVolume>() || It->IsA<ATallowJournal>() || It->IsA<ATallowStoryObject>() || It->IsA<ATallowMatchbox>()) SearchActors.Add(*It);
    if (auto* Chapter = GetWorld()->GetSubsystem<UTallowChapterSubsystem>())
    {
        BoundChapter = Chapter;
        Chapter->OnRoundReset.AddUniqueDynamic(this, &UTallowRoomSearchSubsystem::OnRoundReset);
    }
}
void UTallowRoomSearchSubsystem::OnRoundReset(int32, ETallowRestartReason) { ResetRound(); }
void UTallowRoomSearchSubsystem::ResetRound()
{
    Tracker.ResetRound(); SampleAge = 0.f;
    for (const auto& Room : TallowHouseRooms::All()) OnRoomStateChanged.Broadcast(Room.Id);
}
void UTallowRoomSearchSubsystem::RestoreVisited(const TArray<FName>& Rooms)
{
    TArray<FName> Valid;
    for (FName Room : Rooms) if (TallowHouseRooms::Find(Room)) Valid.Add(Room);
    Tracker.RestoreVisited(Valid);
    for (FName Room : Valid) OnRoomStateChanged.Broadcast(Room);
}
TArray<FName> UTallowRoomSearchSubsystem::GetVisited() const { return Tracker.GetVisited(); }
FName UTallowRoomSearchSubsystem::GetCurrentRoom() const
{
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    return Pawn ? TallowHouseRooms::RoomAt(Pawn->GetActorLocation()) : NAME_None;
}
FTallowRoomSearchState UTallowRoomSearchSubsystem::GetRoomState(FName Room) const
{
    FTallowRoomSearchState State; State.Room = Room;
    const auto& S = Tracker.Get(Room);
    State.bVisited = S.bVisited; State.bSearched = S.bSearched; State.Progress = S.Progress; State.bHere = Room == GetCurrentRoom();
    return State;
}
void UTallowRoomSearchSubsystem::GatherSearchPoints(FName Room, TArray<AActor*>& Out) const
{
    Out.Reset();
    for (const auto& Ref : SearchActors) if (AActor* Actor = Ref.Get())
        if (TallowHouseRooms::RoomAt(Actor->GetActorLocation()) == Room) Out.Add(Actor);
    Out.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
}
void UTallowRoomSearchSubsystem::Tick(float Dt)
{
    if (!bRunning || GetWorld()->IsPaused()) return;
    SampleAge += Dt;
    if (SampleAge < .1f) return;
    const float Step = SampleAge; SampleAge = 0.f;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetPawn()) return;
    const FName Room = GetCurrentRoom();
    FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye, Rotation);
    const auto* Candle = PC->GetPawn()->FindComponentByClass<UTallowCandleComponent>();
    const bool bLit = Candle && Candle->IsAcquired() && Candle->GetCandleState() != ECandleState::Out;
    TArray<AActor*> Actors; GatherSearchPoints(Room, Actors);
    TArray<FTallowSearchPoint> Points;
    for (AActor* Actor : Actors)
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RoomSearch), false, PC->GetPawn());
        const FVector Target = Actor->GetActorLocation();
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
        Points.Add({ Actor->GetFName(), Target, !bBlocked || Hit.GetActor() == Actor });
    }
    const auto Before = Tracker.Get(Room);
    Tracker.Step(Room, Eye, Rotation.Vector(), bLit, Step, Points);
    const auto& After = Tracker.Get(Room);
    if (LastRoom != Room)
    {
        if (!LastRoom.IsNone()) OnRoomStateChanged.Broadcast(LastRoom);
        LastRoom = Room; OnRoomStateChanged.Broadcast(Room);
    }
    if (Before.bVisited != After.bVisited || Before.bSearched != After.bSearched || !FMath::IsNearlyEqual(Before.Progress, After.Progress))
    {
        OnRoomStateChanged.Broadcast(Room);
        if (Before.bVisited != After.bVisited || Before.bSearched != After.bSearched)
            UE_LOG(LogTemp, Display, TEXT("TALLOW_SEARCH room=%s visited=%d searched=%d"), *Room.ToString(), After.bVisited, After.bSearched);
    }
}
void UTallowRoomSearchSubsystem::Draw(float Seconds) const
{
#if !UE_BUILD_SHIPPING
    for (const auto& Room : TallowHouseRooms::All())
    {
        const auto S = GetRoomState(Room.Id);
        DrawDebugString(GetWorld(), Room.Anchor(), FString::Printf(TEXT("%s v=%d s=%d %.0f%%"), *Room.Id.ToString(), S.bVisited, S.bSearched, S.Progress * 100.f), nullptr, S.bSearched ? FColor::Green : FColor::White, Seconds);
    }
#endif
}
