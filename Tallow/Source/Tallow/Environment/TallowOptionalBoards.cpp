#include "Environment/TallowOptionalBoards.h"
#include "AI/TallowCreature.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Player/TallowCharacter.h"
#include "Tasks/TallowPlank.h"
#include "Presentation/TallowPlayDirector.h"

ATallowOptionalBoards::ATallowOptionalBoards()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .04f;
    Boards = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boards"));
    SetRootComponent(Boards);
    Boards->SetMobility(EComponentMobility::Movable);
    Boards->SetCollisionProfileName(TEXT("BlockAll"));
    Boards->SetCanEverAffectNavigation(true);
    Tags.Add(TEXT("Tallow.Route.OptionalBoards"));
}

void ATallowOptionalBoards::BeginPlay()
{
    Super::BeginPlay();
    // The boards are the root component, so a relative rotation is the world rotation: keep the authored one and wobble around it.
    AuthoredRotation = GetActorQuat();
    SpawnPlanks();
    // Wait one tick so the existing director has assigned its persistent run seed. Until the run (or the
    // chapter layout) says which set is boarded, none shows: a set left visible as placed read as a second
    // barricade on the neighbouring door.
    SetBoardVisibility();
}

void ATallowOptionalBoards::SetBoardVisibility()
{
    const bool bBlocked = IsRouteBlocked();
    Boards->SetVisibility(bBlocked);
    Boards->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Boards->SetCanEverAffectNavigation(bBlocked);
    if (Planks.Num() > 0)
    {
        Boards->SetVisibility(false);
        Boards->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        if (bSelected && !bRemoved)
        {
            for (ATallowPlank* Plank : Planks) if (IsValid(Plank) && Plank->IsBroken()) Plank->SetChapterRetired(false);
        }
        else if (!bSelected)
        {
            for (ATallowPlank* Plank : Planks) if (IsValid(Plank)) Plank->SetChapterRetired(true);
        }
    }
    SetWobble(0.f);
}

void ATallowOptionalBoards::SetWobble(float RollDegrees)
{
    SetActorRotation(AuthoredRotation * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(RollDegrees)));
}

void ATallowOptionalBoards::SpawnPlanks()
{
    if (!PlankClass || Planks.Num() > 0 || !GetWorld()) return;
    const FVector Centre = Boards->Bounds.Origin;
    const FVector Extent = Boards->Bounds.BoxExtent;
    // The long axis of the set runs across the opening; a plank's own long axis is its local X (161 cm).
    const float Span = 2.f * FMath::Max(Extent.X, Extent.Y);
    const float Step = Extent.Z * 2.f / PlankCount;
    const FRotator Yaw(0.f, AuthoredRotation.Rotator().Yaw, 0.f);
    for (int32 I = 0; I < PlankCount; ++I)
    {
        const FVector At(Centre.X, Centre.Y, Centre.Z - Extent.Z + Step * (I + .5f));
        const FRotator Rot(0.f, Yaw.Yaw, (I % 2) ? 81.f : 98.f);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = this;
        ATallowPlank* Plank = GetWorld()->SpawnActor<ATallowPlank>(PlankClass, FTransform(Rot, At, FVector(Span / 161.f, 1.f, 1.f)), Params);
        if (!Plank) continue;
        // A side route's plank never counts toward the workshop door (the director counts every plank still marked
        // required, and a set not chosen this run is retired as already broken, so the main route could never clear).
        Plank->bRequiredForEscape = false;
        Plank->OnPlankDestroyed.AddDynamic(this, &ATallowOptionalBoards::HandlePlankDestroyed);
        Planks.Add(Plank);
    }
}

int32 ATallowOptionalBoards::RemainingPlanks() const
{
    int32 N = 0;
    for (const ATallowPlank* Plank : Planks) N += IsValid(Plank) && !Plank->IsBroken();
    return N;
}

void ATallowOptionalBoards::HandlePlankDestroyed(bool bBroken)
{
    if (bRemoved || !IsRouteBlocked() || RemainingPlanks() > 0) return;
    RemoveBoards(false, nullptr);
}

bool ATallowOptionalBoards::HasPryTool_Implementation(AActor* Interactor) const
{
    const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
    return Player && (Player->HasCarriedItem(ECarriedItemType::Crowbar) || Player->ActorHasTag(TEXT("Tallow.Inventory.Crowbar")));
}

bool ATallowOptionalBoards::CanInteract_Implementation(AActor* Interactor) const
{
    return IsRouteBlocked() && HasPryTool(Interactor);
}

float ATallowOptionalBoards::GetInteractionDuration_Implementation() const { return PrySeconds; }

FText ATallowOptionalBoards::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
    if (!IsRouteBlocked()) return FText::GetEmpty();
    return FText::FromString(HasPryTool(Interactor) ? TEXT("PRY BOARDS") : TEXT("REQUIRES CROWBAR / FIND ANOTHER WAY"));
}

void ATallowOptionalBoards::OnInteractTick_Implementation(AActor* Interactor, float DeltaSeconds, float Progress)
{
    if (!CanInteract_Implementation(Interactor)) return;
    NoiseAge += DeltaSeconds;
    SetWobble(FMath::Sin(Progress * 47.f) * .45f);
    if (NoiseAge >= .5f)
    {
        NoiseAge = 0.f;
        if (UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this))
            Noise->EmitNoise(ETallowNoiseType::Pry, GetActorLocation() + FVector(0,0,100), Interactor, .8f, true);
    }
}

void ATallowOptionalBoards::RemoveBoards(bool bByMim, AActor* Responsible)
{
    if (!IsRouteBlocked()) return;
    bRemoved = true;
    // A breach by Mim takes whatever planks are left.
    for (ATallowPlank* Plank : Planks) if (IsValid(Plank) && !Plank->IsBroken()) Plank->OnInteractComplete_Implementation(Responsible);
    SetBoardVisibility();
    if (UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this))
        Noise->EmitNoise(ETallowNoiseType::PropFall, GetActorLocation() + FVector(0,0,100), Responsible, 1.f, !bByMim);
    OnOptionalBoardsOpened.Broadcast(bByMim);
}

void ATallowOptionalBoards::OnInteractComplete_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor)) return;
    // With planks the player pries each plank itself (the set ignores the visibility trace); here one call takes one plank.
    if (Planks.Num() > 0)
    {
        for (ATallowPlank* Plank : Planks) if (IsValid(Plank) && !Plank->IsBroken()) { Plank->OnInteractComplete_Implementation(Interactor); break; }
        return;
    }
    RemoveBoards(false, Interactor);
}

void ATallowOptionalBoards::OnInteractCancel_Implementation(AActor* Interactor)
{
    NoiseAge = 0.f;
    SetWobble(0.f);
}

bool ATallowOptionalBoards::IsSelectedForSeed(int32 Seed) const
{
    FRandomStream Run(Seed ^ int32(GetTypeHash(RouteGroup)));
    return VariantIndex == Run.RandRange(0, FMath::Max(1, VariantCount)-1);
}

void ATallowOptionalBoards::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bChapterControlled && !bInitialized)
    {
        int32 Seed = 1974;
        if (const ATallowPlayDirector* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass())))
        {
            if (!Director->ActiveRunSeed) return;
            Seed = Director->ActiveRunSeed;
        }
        bSelected = IsSelectedForSeed(Seed);
        bInitialized = true;
        SetBoardVisibility();
    }
    if (const ATallowCharacter* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    {
        const bool bTied = Player->IsTiedToChair();
        if (!bChapterControlled && bTied && !bLastTied) { bRemoved = false; BreachAge = 0.f; SetBoardVisibility(); }
        bLastTied = bTied;
    }
    if (!IsRouteBlocked() || !bAllowMimBreach) return;
    ScanAge += DeltaSeconds;
    if (ScanAge > .35f)
    {
        ScanAge = 0.f;
        NearbyMim.Reset();
        for (TActorIterator<ATallowCreature> It(GetWorld()); It; ++It)
        {
            if (!It->GetActorEnableCollision()) continue;
            const FVector Offset = It->GetActorLocation() - GetActorLocation();
            if (Offset.Size2D() < 135.f && FMath::Abs(Offset.Z) < 180.f) { NearbyMim = *It; break; }
        }
    }
    if (NearbyMim.IsValid())
    {
        BreachAge += DeltaSeconds;
        SetWobble(FMath::Sin(BreachAge*14.f)*FMath::Clamp(BreachAge*.18f,0.f,1.f));
        if (BreachAge >= MimBreachSeconds) RemoveBoards(true, NearbyMim.Get());
    }
    else BreachAge = FMath::Max(0.f, BreachAge - DeltaSeconds);
}

void ATallowOptionalBoards::ApplyChapterBlocked(bool bBlocked)
{
    bSelected = bBlocked;
    bRemoved = false;
    bInitialized = true;
    BreachAge = NoiseAge = 0.f;
    SetBoardVisibility();
}
