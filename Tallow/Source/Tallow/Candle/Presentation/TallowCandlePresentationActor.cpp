#include "Candle/Presentation/TallowCandlePresentationActor.h"
#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATallowCandlePresentationActor::ATallowCandlePresentationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Candle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Candle"));
	Candle->SetupAttachment(RootComponent);
	Candle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Candle->SetGenerateOverlapEvents(false);
	Candle->SetCanEverAffectNavigation(false);
	Wick = CreateDefaultSubobject<USceneComponent>(TEXT("Wick"));
	Wick->SetupAttachment(Candle);
	Flame = CreateDefaultSubobject<UTallowFlameVisualComponent>(TEXT("Flame"));
	Flame->bStandalone = true;
	Flame->WickAnchor = Wick;
	Flame->bTickInEditor = true;
}

void ATallowCandlePresentationActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Flame->StandaloneState = FlameState;
	Flame->StandaloneExposure = FMath::Clamp(DraftExposure, 0.f, 1.f);
	if (Flame->FlameMesh && Flame->FlameMaterial) Flame->InitializeVisual();
}

void ATallowCandlePresentationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Flame->StandaloneState = FlameState;
	Flame->StandaloneExposure = FMath::Clamp(DraftExposure, 0.f, 1.f);
}
