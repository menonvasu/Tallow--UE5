#include "Plush/TallowPlushMim.h"
#include "Plush/TallowPlushPerch.h"
#include "House/TallowHouseGraph.h"
#include "House/TallowHouseRooms.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "AI/Hunter/TallowHunterSearchAnchor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATallowPlushMim::ATallowPlushMim()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .1f;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body")); Body->SetupAttachment(Root);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Body->SetCanEverAffectNavigation(false); Body->SetGenerateOverlapEvents(false);
    SetActorHiddenInGame(true); SetActorEnableCollision(false);
}
ATallowPlushMim* ATallowPlushMim::FindOrSpawn(UWorld* World, TSubclassOf<ATallowPlushMim> Class)
{
    if (!World) return nullptr;
    TActorIterator<ATallowPlushMim> Existing(World);
    if (Existing) return *Existing;
    return World->SpawnActor<ATallowPlushMim>(Class ? Class.Get() : StaticClass());
}
void ATallowPlushMim::GatherPerches()
{
    Perches.Reset();
    for (TActorIterator<ATallowPlushPerch> It(GetWorld()); It; ++It) Perches.Add(*It);
    if (Perches.IsEmpty()) for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->IsA<ATallowHunterSearchAnchor>() || (It->IsA<ATargetPoint>() && It->ActorHasTag(TEXT("Tallow.Lurk")))) Perches.Add(*It);
    Perches.Sort([](const auto& A, const auto& B) { return A->GetName() < B->GetName(); });
}
void ATallowPlushMim::SetState(ETallowPlushState State)
{
    if (PlushState == State) return;
    const auto Old = PlushState; PlushState = State;
    OnPlushStateChanged(Old, State);
    UE_LOG(LogTemp, Display, TEXT("TALLOW_PLUSH %s->%s perch=%s"), *UEnum::GetValueAsString(Old), *UEnum::GetValueAsString(State), *GetNameSafe(CurrentPerch.Get()));
}
void ATallowPlushMim::BeginChapter(int32, const FTallowPlushRules& InRules, AActor* StoryObject, int32 Seed)
{
    Retire(); bChapterEnabled = true; Rules = InRules; Story = StoryObject; Random.Initialize(Seed);
    UnseenAge = Rules.UnseenSeconds; SeenAge = 0.f; RelocateAge = 0.f; Relocations = 0;
    RelocateAfter = Random.FRandRange(FMath::Max(.1f, Rules.RelocateMinSeconds), FMath::Max(Rules.RelocateMinSeconds, Rules.RelocateMaxSeconds));
    GatherPerches();
    Place(NAME_None, true);
}
bool ATallowPlushMim::IsSeenAt(const FVector& Position) const
{
    const auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetPawn()) return true;
    FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye, Rotation);
    const FVector Target = Position + FVector(0, 0, 22.f);
    if (FVector::DotProduct(Rotation.Vector(), (Target - Eye).GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(Rules.ViewConeDegrees))) return false;
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(PlushSight), false, this); Params.AddIgnoredActor(PC->GetPawn());
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
}
bool ATallowPlushMim::PerchUsable(AActor* Perch) const
{
    if (!IsValid(Perch)) return false;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Floor;
    if (!Nav || !Nav->ProjectPointToNavigation(Perch->GetActorLocation(), Floor, FVector(90, 90, 180))) return false;
    if (FVector::Dist2D(Floor.Location, Perch->GetActorLocation()) > 90.f) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PlushPerch), false, this); Params.AddIgnoredActor(Perch);
    return !GetWorld()->OverlapBlockingTestByChannel(Perch->GetActorLocation() + FVector(0, 0, 22.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeBox(FVector(18, 18, 20)), Params);
}
void ATallowPlushMim::BuildContext(FTallowPerchContext& Out) const
{
    Out.Rules = Rules;
    const auto* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    Out.Player = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
    Out.Story = Story.IsValid() ? Story->GetActorLocation() : Out.Player;
    if (Pawn && Story.IsValid())
    {
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); FNavLocation End;
        if (Nav && Nav->ProjectPointToNavigation(Out.Story, End, FVector(180, 180, 250)))
            if (auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Out.Player, End.Location, const_cast<APawn*>(Pawn)); Path && Path->IsValid() && !Path->IsPartial()) Out.Path = Path->PathPoints;
    }
    for (const auto& Edge : TallowHouseGraph::Get().Edges) Out.Doorways.Add(Edge.Doorway);
}
bool ATallowPlushMim::CanMove() const
{
    return (PlushState == ETallowPlushState::Hidden || PlushState == ETallowPlushState::Retired || !IsSeenAt(GetActorLocation()))
        && UnseenAge >= Rules.UnseenSeconds && PlushState != ETallowPlushState::Transforming && PlushState != ETallowPlushState::Scaring;
}
bool ATallowPlushMim::Place(FName Room, bool bStart)
{
    if (!CanMove()) return false;
    FTallowPerchContext Context; BuildContext(Context);
    const FName StartRoom = TallowHouseRooms::RoomAt(Context.Player);
    TArray<FName> Neighbours;
    for (const auto* Edge : TallowHouseGraph::Get().EdgesOf(StartRoom)) Neighbours.AddUnique(Edge->Other(StartRoom));
    TArray<FTallowPerchCandidate> Candidates;
    for (const auto& Ref : Perches)
    {
        auto* P = Ref.Get(); FTallowPerchCandidate Candidate;
        Candidate.bUsable = P && P != CurrentPerch.Get() && PerchUsable(P);
        if (P)
        {
            Candidate.Position = P->GetActorLocation(); Candidate.bSeen = IsSeenAt(Candidate.Position);
            const FName PerchRoom = Cast<ATallowPlushPerch>(P) ? Cast<ATallowPlushPerch>(P)->GetRoom() : TallowHouseRooms::RoomAt(Candidate.Position);
            if (!Room.IsNone()) Candidate.bUsable &= PerchRoom == Room;
            if (bStart && !Neighbours.IsEmpty()) Candidate.bUsable &= Neighbours.Contains(PerchRoom);
            if (Story.IsValid() && Context.Path.Num() > 1)
            {
                auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); FNavLocation Floor;
                if (Nav && Nav->ProjectPointToNavigation(Candidate.Position, Floor, FVector(90, 90, 180)))
                {
                    auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Floor.Location, Context.Path.Last());
                    Candidate.bUsable &= Path && Path->IsValid() && !Path->IsPartial();
                    if (Candidate.bUsable) Candidate.StoryPathDistance = Path->GetPathLength();
                }
                else Candidate.bUsable = false;
            }
            Candidate.bVisibleFromEntry = false;
            for (const auto& Edge : TallowHouseGraph::Get().Edges) if (Edge.Joins(PerchRoom))
            {
                FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(PlushEntrySight), false, this); Params.AddIgnoredActor(P);
                if (!GetWorld()->LineTraceSingleByChannel(Hit, Edge.Doorway + FVector(0, 0, 95), Candidate.Position + FVector(0, 0, 22), ECC_Visibility, Params)) { Candidate.bVisibleFromEntry = true; break; }
            }
        }
        Candidates.Add(Candidate);
    }
    const int32 Pick = TallowPlushRules::Choose(Candidates, Context);
    if (Pick == INDEX_NONE) return false;
    SetState(ETallowPlushState::Relocating);
    CurrentPerch = Perches[Pick]; SetActorLocation(Candidates[Pick].Position, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation((Context.Player - GetActorLocation()).Rotation());
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SetActorHiddenInGame(false); SetActorEnableCollision(true);
    ++Relocations; RelocateAge = 0.f; SeenAge = 0.f; UnseenAge = 0.f;
    RelocateAfter = Random.FRandRange(FMath::Max(.1f, Rules.RelocateMinSeconds), FMath::Max(Rules.RelocateMinSeconds, Rules.RelocateMaxSeconds));
    SetState(bStart ? ETallowPlushState::Perched : Story.IsValid() ? ETallowPlushState::Guiding : ETallowPlushState::Perched);
    OnPerched(CurrentPerch.Get());
    return true;
}
bool ATallowPlushMim::TryRelocate()
{
    if (RelocateAge < RelocateAfter) return false;
    return Place(PendingRoom, false);
}
void ATallowPlushMim::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bChapterEnabled || GetWorld()->IsPaused() || PlushState == ETallowPlushState::Retired || PlushState == ETallowPlushState::Transforming) return;
    if (const auto* Chapter = UTallowChapterSubsystem::GetActive(this))
    {
        const auto Phase = Chapter->GetEffectivePhase();
        if (Phase != ETallowChapterPhase::Exploration && Phase != ETallowChapterPhase::StoryInteraction && Phase != ETallowChapterPhase::Transformation && Phase != ETallowChapterPhase::InitialRestraint) return;
    }
    RelocateAge += Dt;
    const bool bSeen = PlushState != ETallowPlushState::Hidden && IsSeenAt(GetActorLocation());
    if (bSeen)
    {
        UnseenAge = 0.f; SeenAge += Dt;
        if (Body->WasRecentlyRendered(.3f) || !Body->GetSkeletalMeshAsset() || !FApp::CanEverRender())
        {
            if (PlushState != ETallowPlushState::Scaring) SetState(ETallowPlushState::Watching);
            OnSeenByPlayer(SeenAge);
        }
    }
    else { UnseenAge += Dt; SeenAge = 0.f; if (PlushState == ETallowPlushState::Watching) SetState(ETallowPlushState::Perched); }
    if (PlushState == ETallowPlushState::Scaring)
    {
        ScareAge += Dt; if (ScareAge >= 2.f) SetState(bSeen ? ETallowPlushState::Watching : ETallowPlushState::Perched);
        return;
    }
    if (!PendingRoom.IsNone() && !IsPreparedForTransformation(PendingRoom)) Place(PendingRoom, false);
    else if (PendingRoom.IsNone()) TryRelocate();
}
bool ATallowPlushMim::TryMinorScare(FName ScareId)
{
    if (!Rules.bMinorScares || !CurrentPerch.IsValid() || PlushState == ETallowPlushState::Scaring || PlushState == ETallowPlushState::Transforming || PlushState == ETallowPlushState::Hidden || PlushState == ETallowPlushState::Retired) return false;
    if (const auto* P = Cast<ATallowPlushPerch>(CurrentPerch.Get()); P && !P->bAllowScare) return false;
    if (!IsSeenAt(GetActorLocation())) return false;
    SetState(ETallowPlushState::Scaring); ScareAge = 0.f; OnMinorScare(ScareId); return true;
}
void ATallowPlushMim::PrepareForTransformation(FName ChairRoom)
{
    PendingRoom = ChairRoom;
    if (!IsPreparedForTransformation(ChairRoom)) Place(ChairRoom, false);
}
bool ATallowPlushMim::IsPreparedForTransformation(FName Room) const
{
    return CurrentPerch.IsValid() && TallowHouseRooms::RoomAt(GetActorLocation()) == Room && PlushState != ETallowPlushState::Hidden && PlushState != ETallowPlushState::Retired;
}
bool ATallowPlushMim::StageForTransformation(const FTransform& Where, bool bViewCovered)
{
    if (!TallowHouseRooms::Find(TallowHouseRooms::RoomAt(Where.GetLocation()))) return false;
    FTallowPerchContext Context; BuildContext(Context);
    FTallowPerchCandidate Candidate; Candidate.Position = Where.GetLocation(); Candidate.bSeen = IsSeenAt(Candidate.Position);
    if (!bViewCovered && (!CanMove() || !TallowPlushRules::IsSafe(Candidate, Context))) return false;
    for (const FVector& Door : Context.Doorways)
        if (FMath::Abs(Door.Z - Where.GetLocation().Z) < 180.f && FVector::Dist2D(Door, Where.GetLocation()) < 120.f) return false;
    SetActorTransform(Where, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorHiddenInGame(false); SetActorEnableCollision(true); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetState(ETallowPlushState::Transforming);
    return true;
}
FTransform ATallowPlushMim::BeginTransformation()
{
    SetState(ETallowPlushState::Transforming);
    FVector Position = GetActorLocation();
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation Projected;
        if (Nav->ProjectPointToNavigation(Position, Projected, FVector(100, 100, 200))) Position = Projected.Location;
    }
    const auto* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    return FTransform(Player ? FRotator(0, (Player->GetActorLocation() - Position).Rotation().Yaw, 0) : GetActorRotation(), Position);
}
void ATallowPlushMim::Retire()
{
    bChapterEnabled = false;
    SetActorHiddenInGame(true); SetActorEnableCollision(false); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CurrentPerch.Reset(); PendingRoom = NAME_None; SetState(ETallowPlushState::Hidden);
}
void ATallowPlushMim::Describe(TArray<FString>& Lines) const
{
    Lines.Add(FString::Printf(TEXT("TALLOW_PLUSH state=%s perch=%s room=%s seen=%d unseen=%.2f moves=%d pending=%s"), *UEnum::GetValueAsString(PlushState), *GetNameSafe(CurrentPerch.Get()), *TallowHouseRooms::RoomAt(GetActorLocation()).ToString(), IsSeenAt(GetActorLocation()), UnseenAge, Relocations, *PendingRoom.ToString()));
}
