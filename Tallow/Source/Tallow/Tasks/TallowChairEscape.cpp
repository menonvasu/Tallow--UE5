#include "Tasks/TallowChairEscape.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Player/TallowCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/TallowChairEscapeWidget.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Noise/TallowNoiseTypes.h"

// Sets default values
ATallowChairEscape::ATallowChairEscape()
{
	PrimaryActorTick.bCanEverTick = true;
	ChairOverlayWidgetClass = UTallowChairEscapeWidget::StaticClass();

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Chair Root"));
	RootComponent = SceneRoot;

	RockingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Chair Pivot"));
	RockingPivot->SetupAttachment(SceneRoot);

	ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chair Mesh"));
	ChairMesh->SetupAttachment(RockingPivot);

	SeatAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Seat Attach Point"));
	SeatAttachPoint->SetupAttachment(ChairMesh);

	EscapeExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Escape Exit Point"));
	EscapeExitPoint->SetupAttachment(SceneRoot);
	EscapeExitPoint->SetRelativeLocation(FVector(75.0f, 0.0f, 60.0f));
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

	if (ChairMesh)
	{
		InitialChairRelativeTransform = ChairMesh->GetRelativeTransform();
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

	RockingPivot->SetRelativeRotation(FRotator(-CurrentSwingAngle, 0.0f, 0.0f));

	// reset count if at rest
	const bool bIsFlatOnFloor = FMath::IsNearlyZero(CurrentSwingAngle, 1.0f);
	const bool bIsStationary = FMath::IsNearlyZero(AngularVelocity, 1.0f);
	if (bIsFlatOnFloor && bIsStationary && CurrentRockCount > 0)
	{
		CurrentRockCount = 0;
		OnRockProgressChanged.Broadcast(0, RequiredRocks);
	}

	// breakout 
	if (CurrentSwingAngle >= 0.0f && CurrentRockCount >= RequiredRocks && AngularVelocity > 0.0f)
	{
		OnChairEscaped.Broadcast();
		bIsTied = false;
		
		APlayerController* PC = TiedPlayerCharacter ? TiedPlayerCharacter->GetController<APlayerController>() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			APlayerCameraManager* CamManager = PC->PlayerCameraManager;

			CamManager->ViewPitchMin = CachedPitchMin;
			CamManager->ViewPitchMax = CachedPitchMax;
			CamManager->ViewYawMin = CachedYawMin;
			CamManager->ViewYawMax = CachedYawMax;
			
			if (FloorSlamCameraShakeClass)
			{
				PC->PlayerCameraManager->StartCameraShake(FloorSlamCameraShakeClass, 1.0f);
			}
		}

		if (ChairMesh)
		{
			ChairMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			ChairMesh->SetSimulatePhysics(true);
			ChairMesh->AddImpulse(GetActorForwardVector() * CrashForce);
		}

		if (ActiveOverlayWidget)
		{
			ActiveOverlayWidget->RemoveFromParent();
			ActiveOverlayWidget = nullptr;
		}

		if (TiedPlayerCharacter)
		{
			TiedPlayerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			TiedPlayerCharacter->TeleportTo(EscapeExitPoint->GetComponentLocation(), EscapeExitPoint->GetComponentRotation(), false, false);
			TiedPlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			TiedPlayerCharacter->SetTiedChair(nullptr);
			TiedPlayerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			TiedPlayerCharacter->bUseControllerRotationYaw = true;
			TiedPlayerCharacter = nullptr;
		}

		SetActorTickEnabled(false);
	}
}


void ATallowChairEscape::AttemptRock()
{ 
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	bool bNoiseSystem = (NoiseSubsystem != nullptr);

	if (!bIsTied) return;

	APlayerController* PC = TiedPlayerCharacter ? TiedPlayerCharacter->GetController<APlayerController>() : nullptr;

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
		
		if (PC && PC->PlayerCameraManager && FloorSlamCameraShakeClass)
		{
			PC->PlayerCameraManager->StartCameraShake(FloorSlamCameraShakeClass, 0.7f);
		}

		if (bNoiseSystem)
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::ChairRock, GetActorLocation(), this, MistimedNoiseLoudness, true);
		}

		AngularVelocity = 0.0f;
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

	if (PC && PC->PlayerCameraManager)
	{
		APlayerCameraManager* CamManager = PC->PlayerCameraManager;
		// Read and save camera defaults
		CachedPitchMin = CamManager->ViewPitchMin;
		CachedPitchMax = CamManager->ViewPitchMax;
		CachedYawMin = CamManager->ViewYawMin;
		CachedYawMax = CamManager->ViewYawMax;

		// Calculate clamps
		const float ChairYaw = ChairMesh ? ChairMesh->GetComponentRotation().Yaw : GetActorRotation().Yaw;
		const float TargetYawMin = ChairYaw - 40.0f;
		const float TargetYawMax = ChairYaw + 40.0f;

		CamManager->ViewPitchMin = -60.0f;
		CamManager->ViewPitchMax = 30.0f;
		CamManager->ViewYawMin = FRotator::ClampAxis(TargetYawMin);
		CamManager->ViewYawMax = FRotator::ClampAxis(TargetYawMax);

		PC->SetControlRotation(FRotator(-60.0f, ChairYaw, 0.0f));

	}

	// set properties back to defaults
	TiedPlayerCharacter = InPlayer;
	bIsTied = true;
	CurrentRockCount = 0;
	OnRockProgressChanged.Broadcast(0, RequiredRocks);
	CurrentSwingAngle = 0.0f;
	AngularVelocity = 0.0f;
	RockingPivot->SetRelativeRotation(FRotator::ZeroRotator);

	// attach player to component, ignore pawn, disable movement, re-enable tick
	InPlayer->AttachToComponent(SeatAttachPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InPlayer->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	InPlayer->GetCharacterMovement()->DisableMovement();
	InPlayer->SetTiedChair(this);
	InPlayer->bUseControllerRotationYaw = false;
	SetActorTickEnabled(true);

	// Reattach chair mesh if not already
	if (ChairMesh)
	{
		ChairMesh->SetSimulatePhysics(false);
		if (RockingPivot && ChairMesh->GetAttachParent() != RockingPivot)
		{
			ChairMesh->AttachToComponent(RockingPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ChairMesh->SetRelativeTransform(InitialChairRelativeTransform);
		}
	}

	// Set Widget
	if (ChairOverlayWidgetClass && PC && !ActiveOverlayWidget)
	{
		ActiveOverlayWidget = CreateWidget<UUserWidget>(PC, ChairOverlayWidgetClass);
		ActiveOverlayWidget->AddToViewport();
	}
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