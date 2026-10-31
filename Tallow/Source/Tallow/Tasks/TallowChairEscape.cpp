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
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"

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
    StartTransform = GetActorTransform();
	Stiffness = FMath::Square(RockingFrequency * 2.0f * UE_PI);

	if (ChairMesh)
	{
		InitialChairRelativeTransform = ChairMesh->GetRelativeTransform();
	}

	TryAutoTiePlayer();
}

void ATallowChairEscape::TryAutoTiePlayer()
{
	ACharacter* RawChar = UGameplayStatics::GetPlayerCharacter(this, 0);

	if (ATallowCharacter* FoundPlayer = Cast<ATallowCharacter>(RawChar))
	{
		TiePlayerToChair(FoundPlayer);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ATallowChairEscape::TryAutoTiePlayer);
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
			// 25 Oct: "the chair flies off the map once you get up". She is teleported to the exit point the
			// same frame, right where it falls, and her capsule shoved it out at any speed. It ignores pawns while it
			// falls, separates from anything gently, and a stray one is put back where it stood. Once it has settled
			// it freezes and blocks pawns again (SettleChair), so it is solid furniture for her and for Mim.
			ChairMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			if (FBodyInstance* Body = ChairMesh->GetBodyInstance()) Body->SetMaxDepenetrationVelocity(60.f);
			const FTransform Stood = ChairMesh->GetComponentTransform();
			ChairMesh->SetSimulatePhysics(true);
			ChairMesh->AddImpulse(GetActorForwardVector() * CrashForce);
			ChairSettleElapsed = 0.f;
			ChairRestTime = 0.f;
			GetWorldTimerManager().SetTimer(ChairSettleHandle, FTimerDelegate::CreateUObject(this, &ATallowChairEscape::SettleChair, Stood), 0.1f, false);
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

	if (!bIsTied || !bMinigameReady) return;

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
    if (TiedPlayerCharacter) ReleasePlayerQuietly();
	
	// Reattach chair mesh if not already
	RockingPivot->SetRelativeRotation(FRotator::ZeroRotator);
	if (ChairMesh)
	{
		ChairMesh->SetSimulatePhysics(false);
		ChairMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetWorldTimerManager().ClearTimer(ChairSettleHandle);
		if (RockingPivot && ChairMesh->GetAttachParent() != RockingPivot)
		{
			ChairMesh->AttachToComponent(RockingPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		ChairMesh->SetRelativeTransform(InitialChairRelativeTransform);
	}

	// The way the seated player faces: the mesh's yaw plus where its front points in mesh space.
	const float ChairYaw = (ChairMesh ? ChairMesh->GetComponentRotation().Yaw : GetActorRotation().Yaw) + MeshFrontYaw;
	
	APlayerController* PC = InPlayer->GetController<APlayerController>();
	if (bMinigameReady) ShowOverlay(PC);

	
	if (PC && PC->PlayerCameraManager)
	{
		APlayerCameraManager* CamManager = PC->PlayerCameraManager;
		// Read and save camera defaults
		CachedPitchMin = CamManager->ViewPitchMin;
		CachedPitchMax = CamManager->ViewPitchMax;
		CachedYawMin = CamManager->ViewYawMin;
		CachedYawMax = CamManager->ViewYawMax;

		// Calculate clamps
		const float TargetYawMin = ChairYaw - TiedYawRange;
		const float TargetYawMax = ChairYaw + TiedYawRange;

		CamManager->ViewPitchMin = -60.0f;
		CamManager->ViewPitchMax = 30.0f;
		CamManager->ViewYawMin = FRotator::ClampAxis(TargetYawMin);
		CamManager->ViewYawMax = FRotator::ClampAxis(TargetYawMax);

		PC->SetControlRotation(FRotator(TiedStartPitch, ChairYaw, 0.0f));

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
	InPlayer->bUseControllerRotationYaw = false;
	InPlayer->AttachToComponent(SeatAttachPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InPlayer->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	InPlayer->GetCharacterMovement()->DisableMovement();
	InPlayer->SetTiedChair(this);
	SetActorTickEnabled(true);
}

void ATallowChairEscape::SettleChair(FTransform Stood)
{
	// Re-tied after a capture: TiePlayerToChair owns the chair again.
	if (!ChairMesh || ChairMesh->GetAttachParent() == RockingPivot) return;
	const float Step = 0.1f;
	if (ChairMesh->IsSimulatingPhysics())
	{
		ChairSettleElapsed += Step;
		const FVector Now = ChairMesh->GetComponentLocation();
		const bool bStray = FVector::Dist2D(Now, Stood.GetLocation()) > 250.f || Now.Z < Stood.GetLocation().Z - 80.f;
		const bool bStill = ChairMesh->GetPhysicsLinearVelocity().SizeSquared() < 4.f * 4.f
			&& ChairMesh->GetPhysicsAngularVelocityInDegrees().SizeSquared() < 5.f * 5.f;
		ChairRestTime = bStill ? ChairRestTime + Step : 0.f;
		const bool bRested = ChairRestTime >= 0.3f;
		const bool bTimedOut = ChairSettleElapsed >= 6.f;
		float UpZ = 1.f;
		// Lying down on the floor and not across her feet.
		const bool bFlat = IsChairLyingOnFloor(UpZ) && !IsCharacterInsideChair();
		if (!bStray && !bTimedOut && !(bRested && bFlat))
		{
			// Still tumbling, or sleeping while propped on something: keep watching.
			if (!bRested)
			{
				GetWorldTimerManager().SetTimer(ChairSettleHandle, FTimerDelegate::CreateUObject(this, &ATallowChairEscape::SettleChair, Stood), Step, false);
				return;
			}
		}
		ChairMesh->SetSimulatePhysics(false);
		if (bStray)
		{
			ChairMesh->SetWorldTransform(Stood, false, nullptr, ETeleportType::ResetPhysics);
			SnapChairToKnockedOverPose(Stood);
		}
		else if (!(bRested && bFlat))
		{
			// Timed out, or came to rest tilted against something: lay it down properly.
			SnapChairToKnockedOverPose(Stood);
		}
	}
	// She lands right where it falls: wait until no capsule stands inside it, or blocking would shove her out.
	if (IsCharacterInsideChair())
	{
		GetWorldTimerManager().SetTimer(ChairSettleHandle, FTimerDelegate::CreateUObject(this, &ATallowChairEscape::SettleChair, Stood), .25f, false);
		return;
	}
	// Blocking pawns also makes it navigation relevant again, so the dynamic navmesh paths Mim around it.
	ChairMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAIR settled at %s; blocking pawns"), *ChairMesh->GetComponentLocation().ToCompactString());
}

bool ATallowChairEscape::IsChairLyingOnFloor(float& OutUpZ) const
{
	OutUpZ = ChairMesh->GetUpVector().Z;
	// Down on its back or side. Upright (it did not go over) or anything in between (propped on something) is not.
	const bool bOrientationOk = FMath::Abs(OutUpZ) < 0.3f;
	const FBoxSphereBounds Bounds = ChairMesh->Bounds;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChairFloor), false, this);
	const FVector Start = Bounds.Origin;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, Bounds.BoxExtent.Z + 300.f), ECC_WorldStatic, Params);
	const float Gap = bHit ? (Bounds.Origin.Z - Bounds.BoxExtent.Z) - Hit.ImpactPoint.Z : 1e6f;
	return bOrientationOk && Gap < 8.f;
}

bool ATallowChairEscape::IsChairPoseClear() const
{
	FComponentQueryParams Params(SCENE_QUERY_STAT(ChairPose), this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->ComponentOverlapMultiByChannel(Overlaps, ChairMesh, ChairMesh->GetComponentLocation(), ChairMesh->GetComponentQuat(), ECC_WorldStatic, Params);
	for (const FOverlapResult& O : Overlaps)
	{
		if (O.Component.IsValid() && O.Component->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block)
		{
			UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAIR pose blocked by %s"), *GetNameSafe(O.Component.Get()));
			return false;
		}
	}
	return !IsCharacterInsideChair();
}

void ATallowChairEscape::SnapChairToKnockedOverPose(const FTransform& Stood)
{
	// Fall away from where she steps out, onto its back; failing that onto either side; failing that upright where it stood.
	const FVector Pivot = Stood.GetLocation();
	FVector Away = (Pivot - EscapeExitPoint->GetComponentLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = -GetActorForwardVector().GetSafeNormal2D();
	const FVector Candidates[3] = { Away, Away.RotateAngleAxis(90.f, FVector::UpVector), Away.RotateAngleAxis(-90.f, FVector::UpVector) };
	for (int32 i = 0; i < 4; ++i)
	{
		FTransform Pose = Stood;
		if (i < 3)
		{
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, Candidates[i]).GetSafeNormal();
			Pose.SetRotation(FQuat(Axis, UE_HALF_PI) * Stood.GetRotation());
		}
		ChairMesh->SetWorldTransform(Pose, false, nullptr, ETeleportType::ResetPhysics);
		// Lower it onto the floor under it.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ChairKnock), false, this);
		const FBoxSphereBounds B = ChairMesh->Bounds;
		const FVector Start(B.Origin.X, B.Origin.Y, Stood.GetLocation().Z + B.BoxExtent.Z + 40.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 600.f), ECC_WorldStatic, Params))
		{
			const float Lift = Hit.ImpactPoint.Z + 0.5f - (B.Origin.Z - B.BoxExtent.Z);
			ChairMesh->SetWorldLocation(ChairMesh->GetComponentLocation() + FVector(0, 0, Lift), false, nullptr, ETeleportType::ResetPhysics);
		}
		if (i == 3 || IsChairPoseClear()) break;
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAIR knocked-over pose at %s up.z=%.2f"), *ChairMesh->GetComponentLocation().ToCompactString(), ChairMesh->GetUpVector().Z);
}

bool ATallowChairEscape::IsCharacterInsideChair() const
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
		if (!Capsule || !Capsule->IsCollisionEnabled()) continue;
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() + 2.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
		if (ChairMesh->OverlapComponent(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), Shape)) return true;
	}
	return false;
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
void ATallowChairEscape::SetRequiredRocks(int32 Rocks)
{
    RequiredRocks = FMath::Clamp(Rocks, 1, 6);
    OnRockProgressChanged.Broadcast(CurrentRockCount, RequiredRocks);
}
void ATallowChairEscape::ReleasePlayerQuietly()
{
    if (TiedPlayerCharacter)
    {
        if (auto* PC = TiedPlayerCharacter->GetController<APlayerController>(); PC && PC->PlayerCameraManager)
        {
            auto* Camera = PC->PlayerCameraManager.Get();
            Camera->ViewPitchMin = CachedPitchMin; Camera->ViewPitchMax = CachedPitchMax;
            Camera->ViewYawMin = CachedYawMin; Camera->ViewYawMax = CachedYawMax;
        }
        TiedPlayerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        TiedPlayerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        TiedPlayerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        TiedPlayerCharacter->SetTiedChair(nullptr);
        TiedPlayerCharacter->bUseControllerRotationYaw = true;
        TiedPlayerCharacter = nullptr;
    }
    if (ActiveOverlayWidget) { ActiveOverlayWidget->RemoveFromParent(); ActiveOverlayWidget = nullptr; }
    bMinigameReady = true;
    bIsTied = false; CurrentRockCount = 0; CurrentSwingAngle = 0.f; AngularVelocity = 0.f;
    RockingPivot->SetRelativeRotation(FRotator::ZeroRotator);
    SetActorTickEnabled(false);
}
FTransform ATallowChairEscape::GetExitTransform() const { return EscapeExitPoint->GetComponentTransform(); }
FTransform ATallowChairEscape::GetExitRelativeTransform() const { return EscapeExitPoint->GetRelativeTransform(); }
FTransform ATallowChairEscape::GetStandingMeshRelativeTransform() const { return InitialChairRelativeTransform; }
FTransform ATallowChairEscape::GetSeatRelativeTransform() const { return SeatAttachPoint->GetRelativeTransform() * InitialChairRelativeTransform; }

void ATallowChairEscape::ShowOverlay(APlayerController* PC)
{
	if (!ChairOverlayWidgetClass || !PC || ActiveOverlayWidget) return;
	ActiveOverlayWidget = CreateWidget<UUserWidget>(PC, ChairOverlayWidgetClass);
	if (UTallowChairEscapeWidget* Meter = Cast<UTallowChairEscapeWidget>(ActiveOverlayWidget)) Meter->Chair = this;
	ActiveOverlayWidget->AddToViewport();
	OverlayShownAt = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAIR meter fades in over %.1fs"), OverlayFadeSeconds);
}

void ATallowChairEscape::SetMinigameReady(bool bReady)
{
	if (bReady == bMinigameReady) return;
	bMinigameReady = bReady;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CHAIR minigame %s tied=%d"), bReady ? TEXT("ready") : TEXT("waiting for Mim to go"), bIsTied ? 1 : 0);
	if (!bReady)
	{
		if (ActiveOverlayWidget) { ActiveOverlayWidget->RemoveFromParent(); ActiveOverlayWidget = nullptr; }
		CurrentRockCount = 0;
		return;
	}
	if (bIsTied && TiedPlayerCharacter) ShowOverlay(TiedPlayerCharacter->GetController<APlayerController>());
}

float ATallowChairEscape::GetOverlayAlpha() const
{
	if (!GetWorld()) return 1.f;
	return FMath::SmoothStep(0.f, 1.f, float(GetWorld()->GetTimeSeconds() - OverlayShownAt) / FMath::Max(OverlayFadeSeconds, .01f));
}

