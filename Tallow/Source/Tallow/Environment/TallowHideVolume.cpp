#include "Environment/TallowHideVolume.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/TallowCharacter.h"

ATallowHideVolume::ATallowHideVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	HideBox = CreateDefaultSubobject<UBoxComponent>("Hide Box");
	HideBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HideBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = HideBox;
	
	HidingViewPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Hiding View Point"));
	HidingViewPoint->SetupAttachment(RootComponent);

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Exit Point"));
	ExitPoint->SetupAttachment(RootComponent);
	ExitPoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
}

void ATallowHideVolume::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) return;
	APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn) return;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return;

	if (!bIsHidden)
	{
		EnterHiddenSpot(PC);
	}
	else
	{
		ExitHiddenSpot(PC);
	}
}

FText ATallowHideVolume::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) return FText::GetEmpty();
	
	if (!bIsHidden)
	{
		return FText::FromString("ENTER");
	}
	return FText::FromString("EXIT");
}

void ATallowHideVolume::EnterHiddenSpot(APlayerController* PC)
{
	if (!PC) return; 
	CachedPC = PC;

	bIsHidden = true;
	OnHide.Broadcast(bIsHidden);
	
	const FVector TargetLocation = HidingViewPoint->GetComponentLocation();
	const FRotator TargetRotation = HidingViewPoint->GetComponentRotation();
	
	// Camera clamps
	if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
	{
		// Read and save camera defaults
		CachedPitchMin = CamManager->ViewPitchMin;
		CachedPitchMax = CamManager->ViewPitchMax;
		CachedYawMin = CamManager->ViewYawMin;
		CachedYawMax = CamManager->ViewYawMax;

		// Set pitch clamps
		CamManager->ViewPitchMin = -30.0f;
		CamManager->ViewPitchMax = 30.0f;

		// Set yaw clamps relative to new rotation
		float TargetYawMin = FRotator::ClampAxis(TargetRotation.Yaw - 40.0f);
		float TargetYawMax = FRotator::ClampAxis(TargetRotation.Yaw + 40.0f);

		CamManager->ViewYawMin = TargetYawMin;
		CamManager->ViewYawMax = TargetYawMax + 360.0f;
	}

	ATallowCharacter* Player = Cast<ATallowCharacter>(PC->GetPawn());
	if (!Player) return;

	Player->SetCurrentHideVolume(this);

	// Set location and rotation to the viewpoint
	const float CameraLocalZ = Player->GetFirstPersonCameraComponent()->GetRelativeLocation().Z + Player->GetCameraBoom()->GetRelativeLocation().Z;
	FVector TargetCapsuleLocation = TargetLocation;
	TargetCapsuleLocation.Z -= CameraLocalZ;
	Player->SetActorLocationAndRotation(TargetCapsuleLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(TargetRotation);

	// Disable movement and collision
	if (Player->GetCharacterMovement())
	{
		Player->GetCharacterMovement()->DisableMovement();
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->bUseControllerRotationYaw = false;
		Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ATallowHideVolume::ExitHiddenSpot(APlayerController* PC)
{
	if (!PC) return;

	bIsHidden = false;
	OnHide.Broadcast(bIsHidden);

	if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
	{
		CamManager->ViewPitchMin = CachedPitchMin;
		CamManager->ViewPitchMax = CachedPitchMax;
		CamManager->ViewYawMin = CachedYawMin;
		CamManager->ViewYawMax = CachedYawMax;
	}

	// Re-enable movement
	FVector TargetLocation = ExitPoint->GetComponentLocation();
	ATallowCharacter* Player = Cast<ATallowCharacter>(PC->GetPawn());

	if (Player && Player->GetCharacterMovement())
	{
		Player->SetCurrentHideVolume(nullptr);

		// Set location and rotation to the viewpoint
		Player->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Player->bUseControllerRotationYaw = true;
		Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Player->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
}

void ATallowHideVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsHidden && CachedPC.IsValid())
	{
		if (APlayerCameraManager* CamManager = CachedPC->PlayerCameraManager)
		{
			CamManager->ViewPitchMin = CachedPitchMin;
			CamManager->ViewPitchMax = CachedPitchMax;
			CamManager->ViewYawMin = CachedYawMin;
			CamManager->ViewYawMax = CachedYawMax;
		}
	}
	Super::EndPlay(EndPlayReason);
}