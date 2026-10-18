#include "TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Tasks/TallowChairEscape.h"
#include "Player/TallowCharacter.h"
#include "AI/TallowCreature.h"
#include "Environment/TallowCandlePickup.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Tasks/TallowCrowbarPickup.h"
#include "Tasks/TallowFusePickup.h"
#include "Tasks/TallowFuseBox.h"
#include "Environment/TallowDoor.h"

ATallowGameMode::ATallowGameMode()
{
}

void ATallowGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Cast to all pointers once for performance
	Player = Cast<ATallowCharacter>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCharacter::StaticClass()));
	Chair = Cast<ATallowChairEscape>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowChairEscape::StaticClass()));
	Creature = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCreature::StaticClass()));
	CandlePickup = Cast<ATallowCandlePickup>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCandlePickup::StaticClass()));
	FuseBox = Cast<ATallowFuseBox>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowFuseBox::StaticClass()));
	TArray<AActor*> FoundFuseActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATallowFusePickup::StaticClass(), FoundFuseActors);
	for (AActor* Actor : FoundFuseActors)
	{
		if (ATallowFusePickup* Pickup = Cast<ATallowFusePickup>(Actor))
		{
			FusePickups.Add(Pickup);
		}
	}
	CrowbarPickup = Cast<ATallowCrowbarPickup>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCrowbarPickup::StaticClass()));
	
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		CameraManager = PC->PlayerCameraManager;
	}
	TArray<AActor*> FoundDoors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATallowDoor::StaticClass(), FoundDoors);
	for (AActor* Actor : FoundDoors)
	{
		if (ATallowDoor* Door = Cast<ATallowDoor>(Actor))
		{
			CachedDoors.Add(Door);
		}
	}
	
}

void ATallowGameMode::HandlePlayerCaptured()
{
	if (bCaptureInProgress) return;
	bCaptureInProgress = true;
	float PresentationDuration = 0.f;
	if (bShowAuthoredCapture && Creature)
	{
		if (const auto* Animation = Creature->FindComponentByClass<UTallowCreatureAnimationComponent>())
		{
			PresentationDuration = Animation->GetCaptureDuration();
		}
	}
	if (PresentationDuration > 0.f && Player)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			FVector FaceLocation = Creature->GetActorLocation() + FVector(0, 0, 85.f);
			if (Creature->GetMesh()->DoesSocketExist(TEXT("head")))
				FaceLocation = Creature->GetMesh()->GetSocketLocation(TEXT("head"));
			PC->SetControlRotation((FaceLocation - Player->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
			PC->SetIgnoreLookInput(true);
			bCaptureLockedLook = true;
		}
	}
	const float FadeDelay = FMath::Max(0.f, PresentationDuration - FadeOutTimer);
	if (FadeDelay > 0.f)
		GetWorld()->GetTimerManager().SetTimer(CaptureFadeTimerHandle, this, &ATallowGameMode::BeginCaptureFade, FadeDelay, false);
	else
		BeginCaptureFade();

	if (Player && Player->GetCharacterMovement())
	{
		Player->GetCharacterMovement()->DisableMovement();
	}

	OnCaught.Broadcast();
	
	if (Creature && Creature->GetCharacterMovement())
	{
		Creature->GetCharacterMovement()->DisableMovement();
	}

	GetWorld()->GetTimerManager().SetTimer(
		ResetTimerHandle,
		this,
		&ATallowGameMode::ResetGame,
		FadeDelay + FadeOutTimer + BlackoutTimer,
		false
	);
}

void ATallowGameMode::BeginCaptureFade()
{	
	if (CameraManager)
		CameraManager->StartCameraFade(0.f, 1.f, FadeOutTimer, FColor::Black, true, true);
}

void ATallowGameMode::RegisterDroppedItem(AActor* DroppedItem)
{
	// just adds dropped item to the array
	if (!DroppedItem) { return; }
	RuntimeDroppedItems.Add(DroppedItem);
}

void ATallowGameMode::ResetGame()
{
	bCaptureInProgress = false;
	GetWorld()->GetTimerManager().ClearTimer(CaptureFadeTimerHandle);
	if (bCaptureLockedLook && Player)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController())) PC->SetIgnoreLookInput(false);
	}
	bCaptureLockedLook = false;
	if (Creature)
	{
		Creature->ResetToSpawn();
	}	
	if (Chair)
	{
		Chair->TiePlayerToChair(Player);
	}
	if (CandlePickup)
	{
		CandlePickup->ResetPickup(Player);
	}
	
	if (CameraManager)
	{
		CameraManager->StartCameraFade(1.0f, 0.0f, FadeInTimer, FColor::Black, false, false);
	}
	
	if (FuseBox)
	{
		FuseBox->ResetVisual();
	}
	
	for (ATallowFusePickup* Pickup : FusePickups)
	{
		if (Pickup)
		{
			Pickup->ResetPickup();
		}
	}
	
	if (CrowbarPickup)
	{
		CrowbarPickup->ResetPickup();
	}
	
	if (Player)
	{
		Player->ClearCarriedItem();
		Player->UpdateHeldItemVisual();
	}
	
	for (TWeakObjectPtr<AActor> DroppedItem : RuntimeDroppedItems)
	{
		if (DroppedItem.IsValid())
		{
			DroppedItem->Destroy();
		}
	}
	RuntimeDroppedItems.Empty();
	
	for (ATallowDoor* Door : CachedDoors)
	{
		if (Door)
		{
			Door->ResetDoor();
		}
	}
}
