#include "Presentation/TallowRouteGate.h"
#include "Presentation/TallowPlayDirector.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
ATallowRouteGate::ATallowRouteGate()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); RootComponent = SceneRoot;
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame")); Frame->SetupAttachment(SceneRoot);
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge")); Hinge->SetupAttachment(SceneRoot);
	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door")); Door->SetupAttachment(Hinge);
	Door->SetMobility(EComponentMobility::Movable); Door->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void ATallowRouteGate::BeginPlay()
{
	Super::BeginPlay();
	Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()));
}
void ATallowRouteGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Director) return;
	const bool bReleased = bRequiresPower ? Director->bPowerRestored : Director->bBoardsCleared;
	CurrentAngle = FMath::FInterpConstantTo(CurrentAngle, bReleased ? OpenAngle : 0.f, DeltaSeconds, 65.f);
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
}
