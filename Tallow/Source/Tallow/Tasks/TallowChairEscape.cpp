#include "Tasks/TallowChairEscape.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Player/TallowCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"

// Sets default values
ATallowChairEscape::ATallowChairEscape()
{
	PrimaryActorTick.bCanEverTick = true;

	ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chair Mesh"));
	RootComponent = ChairMesh;

	SeatAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatAttachPoint"));
	SeatAttachPoint->SetupAttachment(ChairMesh);
}

// Called when the game starts or when spawned
void ATallowChairEscape::BeginPlay()
{
	Super::BeginPlay();
	Stiffness = FMath::Square(RockingFrequency * 2.0f * UE_PI);

	if ((TiedPlayerCharacter = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))))
	{
		TiePlayerToChair(TiedPlayerCharacter);
	}
}

// Called every frame
void ATallowChairEscape::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// calculate how much the spring and friction are pulling right now
	float AngularAcceleration = (-Stiffness * CurrentSwingAngle) - (Damping * AngularVelocity);
	// integrate Velocity
	AngularVelocity += AngularAcceleration * DeltaTime;
	// integrate Angle
	CurrentSwingAngle += AngularVelocity * DeltaTime;
	// clamp to ensure no clipping
	CurrentSwingAngle = FMath::Clamp(CurrentSwingAngle, -MaxSwingAngle, MaxSwingAngle);

	if (CurrentSwingAngle >= MaxSwingAngle && AngularVelocity > 0.0f)
	{
		AngularVelocity = 0.0f; // inelastic collision
	}
	else if (CurrentSwingAngle <= -MaxSwingAngle && AngularVelocity < 0.0f) 
	{
		AngularVelocity = 0.0f; // inelastic collision
	}

	ChairMesh->SetRelativeRotation(FRotator(-CurrentSwingAngle, 0.0f, 0.0f));

	// reset count if at rest
	const bool bIsFlatOnFloor = FMath::IsNearlyZero(CurrentSwingAngle, 1.0f);
	const bool bIsStationary = FMath::IsNearlyZero(AngularVelocity, 1.0f);
	if (bIsFlatOnFloor && bIsStationary && CurrentRockCount > 0)
	{
		CurrentRockCount = 0;
		OnRockProgressChanged.Broadcast(0, RequiredRocks);
	}
}


void ATallowChairEscape::AttemptRock()
{ 
	if (!bIsTied) return;

	// initial impulse
	const bool bIsAtRest = FMath::IsNearlyZero(AngularVelocity, 1.0f) && FMath::IsNearlyZero(CurrentSwingAngle, 1.0f);
	if (bIsAtRest)
	{
		AngularVelocity += RockImpulse;
	}
	// amplitude amplified
	else if (IsTimingWindowOpen())
	{
		CurrentRockCount++;
		OnRockProgressChanged.Broadcast(CurrentRockCount, RequiredRocks);
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), RhythmicCreakLoudness, this, 0.0f, FName("Successful rock"));
		AngularVelocity += RockImpulse;
	}
	// amplitude cancels out - reset
	else 
	{
		CurrentRockCount = 0;
		OnRockProgressChanged.Broadcast(0, RequiredRocks);
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), MistimedNoiseLoudness, this, 0.0f, FName("Chair rock failed"));
		AngularVelocity = 0.0f;
	}

	// breakout
	if (CurrentRockCount >= RequiredRocks)
	{
		OnChairEscaped.Broadcast();
		bIsTied = false;
		SetActorTickEnabled(false);

		if (TiedPlayerCharacter)
		{
			TiedPlayerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			TiedPlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			TiedPlayerCharacter->SetTiedChair(nullptr);
			TiedPlayerCharacter = nullptr;
		}

		if (ActiveOverlayWidget)
		{
			ActiveOverlayWidget->RemoveFromParent();
			ActiveOverlayWidget = nullptr;
		}
	}
}

void ATallowChairEscape::TiePlayerToChair(ATallowCharacter* InPlayer)
{
	if (!InPlayer) return;

	APlayerController* PC = InPlayer->GetController<APlayerController>();
	if (ChairOverlayWidgetClass && PC && !ActiveOverlayWidget)
	{
		ActiveOverlayWidget = CreateWidget<UUserWidget>(PC, ChairOverlayWidgetClass);
		ActiveOverlayWidget->AddToViewport();
	}

	// set properties back to defaults
	TiedPlayerCharacter = InPlayer;
	bIsTied = true;
	CurrentRockCount = 0;
	OnRockProgressChanged.Broadcast(0, RequiredRocks);
	CurrentSwingAngle = 0.0f;
	AngularVelocity = 0.0f;
	ChairMesh->SetRelativeRotation(FRotator::ZeroRotator);
	
	// attach player to component and disable movement, re-enable tick
	InPlayer->AttachToComponent(SeatAttachPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InPlayer->GetCharacterMovement()->DisableMovement();
	InPlayer->SetTiedChair(this);
	SetActorTickEnabled(true);
}

// Helper: return correct window
bool ATallowChairEscape::IsTimingWindowOpen() const
{
	const bool bIsNearApex = CurrentSwingAngle < -3.0f; 
	const bool bIsMovingForward = AngularVelocity > 0.0f;

	return bIsNearApex && bIsMovingForward;
}

// Helper for UI
float ATallowChairEscape::GetNormalizedSwing() const
{
	if (MaxSwingAngle == 0.0f) return 0.0f;

	float NormalisedSwing = CurrentSwingAngle / MaxSwingAngle;
	
	return FMath::Clamp(NormalisedSwing, -1.0f, 1.0f);
}

void ATallowChairEscape::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveOverlayWidget)
	{
		ActiveOverlayWidget->RemoveFromParent();
		ActiveOverlayWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}