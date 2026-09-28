#include "Tasks/TallowChairEscape.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Player/TallowCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "UI/TallowChairEscapeWidget.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Noise/TallowNoiseTypes.h"

// Sets default values
ATallowChairEscape::ATallowChairEscape()
{
	PrimaryActorTick.bCanEverTick = true;
	ChairOverlayWidgetClass = UTallowChairEscapeWidget::StaticClass();

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
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	bool bNoiseSystem = (NoiseSubsystem != nullptr);

	if (!bIsTied) return;

	// initial impulse
	if (IsAtRest())
	{
		AngularVelocity += RockImpulse;
	}
	// amplitude amplified
	else if (IsTimingWindowOpen())
	{
		CurrentRockCount++;
		OnRockProgressChanged.Broadcast(CurrentRockCount, RequiredRocks);

		if (bNoiseSystem)
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::ChairRock, GetActorLocation(), this, RhythmicCreakLoudness, true);
		}

		AngularVelocity += RockImpulse;
	}
	// amplitude cancels out - reset
	else 
	{
		CurrentRockCount = 0;
		OnRockProgressChanged.Broadcast(0, RequiredRocks);
		
		if (bNoiseSystem)
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::ChairRock, GetActorLocation(), this, MistimedNoiseLoudness, true);
		}

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
			TiedPlayerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			TiedPlayerCharacter->bUseControllerRotationYaw = true;
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
		if (UTallowChairEscapeWidget* Meter = Cast<UTallowChairEscapeWidget>(ActiveOverlayWidget)) Meter->Chair = this;
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
	
	// attach player to component, ignore pawn, disable movement, re-enable tick
	InPlayer->AttachToComponent(SeatAttachPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InPlayer->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	InPlayer->GetCharacterMovement()->DisableMovement();
	InPlayer->SetTiedChair(this);
	InPlayer->bUseControllerRotationYaw = false;
	SetActorTickEnabled(true);
}

// Helper: return correct window
bool ATallowChairEscape::IsTimingWindowOpen() const
{
	const bool bIsNearApex = CurrentSwingAngle < -WindowAngle;
	const bool bIsMovingForward = AngularVelocity > 0.0f;

	return bIsNearApex && bIsMovingForward;
}

bool ATallowChairEscape::IsAtRest() const
{
	return FMath::IsNearlyZero(AngularVelocity, 1.0f) && FMath::IsNearlyZero(CurrentSwingAngle, 1.0f);
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