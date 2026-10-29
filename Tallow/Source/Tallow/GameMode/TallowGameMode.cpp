#include "TallowGameMode.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Environment/TallowHideVolume.h"
#include "Environment/TallowWaxPickup.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Tasks/TallowChairEscape.h"
#include "Player/TallowCharacter.h"
#include "AI/TallowCreature.h"
#include "AI/Hunter/TallowHunterAIController.h"
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
#include "Environment/TallowMatchbox.h"
#include "Tasks/TallowFilmReelPickup.h"
#include "Tasks/TallowProjector.h"
#include "Presentation/TallowEscapePoint.h"

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
	TArray<AActor*> FoundMatchboxes;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATallowMatchbox::StaticClass(), FoundMatchboxes);
	for (AActor* Actor : FoundMatchboxes)
	{
		if (ATallowMatchbox* Matchbox = Cast<ATallowMatchbox>(Actor))
		{
			Matchboxes.Add(Matchbox);
		}
	}
	
}

void ATallowGameMode::HandlePlayerCaptured()
{
	if (bCaptureInProgress) return;
	if (const auto* Chapters = UTallowChapterSubsystem::GetActive(this); Chapters && !Chapters->AllowsCapture()) return;
	if (Creature)
		if (const auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController()); Hunter && !Hunter->CanCapturePlayer()) return;
	bCaptureInProgress = true;
	if (Player && Player->GetCurrentHideVolume()) Player->GetCurrentHideVolume()->ReleaseForCapture();
	if (Creature) Creature->PrepareCapturePresentation();
	float PresentationDuration = 0.f;
	if (bShowAuthoredCapture && Creature)
	{
		PresentationDuration = Creature->GetCapturePresentationDuration();
	}
	// No authored clip fits the V13 Keeper: its grab is procedural and still needs time on screen.
	const bool bProceduralCapture = PresentationDuration <= 0.f;
	if (bProceduralCapture) PresentationDuration = ProceduralCaptureDuration;
	if (PresentationDuration > 0.f && Player && Creature)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			// The play director turns the view onto her face; snapping it here would skip the turn.
			PC->SetIgnoreLookInput(true);
			PC->SetIgnoreMoveInput(true);
			bCaptureLockedLook = true;
		}
	}
	// The procedural grab plays out in full before the fade starts.
	const float FadeDelay = bProceduralCapture ? PresentationDuration : FMath::Max(0.f, PresentationDuration - FadeOutTimer);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CAPTURE presentation=%.2f fadeout=%.2f delay=%.2f procedural=%d"),
		PresentationDuration, FadeOutTimer, FadeDelay, bProceduralCapture ? 1 : 0);
	if (FadeDelay > 0.f)
		GetWorld()->GetTimerManager().SetTimer(CaptureFadeTimerHandle, this, &ATallowGameMode::BeginCaptureFade, FadeDelay, false);
	else
		BeginCaptureFade();

	if (Player && Player->GetCharacterMovement())
	{
		CaptureMovementMode = Player->GetCharacterMovement()->MovementMode;
		CaptureCustomMovementMode = Player->GetCharacterMovement()->CustomMovementMode;
		bCaptureLockedMovement = true;
		Player->GetCharacterMovement()->DisableMovement();
	}

	for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It) It->StopProjection();
	OnCaught.Broadcast();
	
	if (Creature && Creature->GetCharacterMovement())
	{
		Creature->GetCharacterMovement()->DisableMovement();
	}

	GetWorld()->GetTimerManager().SetTimer(
		ResetTimerHandle,
		this,
		&ATallowGameMode::FinishCapturePresentation,
		FadeDelay + FadeOutTimer + BlackoutTimer,
		false
	);
}

void ATallowGameMode::FinishCapturePresentation()
{
	if (OnCapturePresented.IsBound()) OnCapturePresented.Broadcast();
	else ResetGame();
}

void ATallowGameMode::BeginCaptureFade()
{	
	if (!CameraManager) return;
	bCaptureFadeHeld = true;
	// A zero-length camera fade never updates its amount, so the hard cut is set directly;
	// the audio follows it to silence on the next camera update.
	if (FadeOutTimer <= 0.f) CameraManager->SetManualCameraFade(1.f, FColor::Black, true);
	else CameraManager->StartCameraFade(0.f, 1.f, FadeOutTimer, FColor::Black, true, true);
}

void ATallowGameMode::RegisterDroppedItem(AActor* DroppedItem)
{
	// just adds dropped item to the array
	if (!DroppedItem) { return; }
	RuntimeDroppedItems.Add(DroppedItem);
}

void ATallowGameMode::ClearCaptureState()
{
	if (Player && Player->GetCurrentHideVolume()) Player->GetCurrentHideVolume()->LeaveHidingSpot(true);
	bCaptureInProgress = false;
	GetWorld()->GetTimerManager().ClearTimer(CaptureFadeTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ResetTimerHandle);
	if (bCaptureLockedLook && Player)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
		}
	}
	bCaptureLockedLook = false;
	// Every way out of a capture comes through here (the retry, a chapter rebuild or its recovery): the picture
	// and the sound come back up together with the house, rather than staying on the cut's black.
	if (bCaptureFadeHeld && CameraManager) CameraManager->StartCameraFade(1.f, 0.f, FadeInTimer, FColor::Black, true, false);
	bCaptureFadeHeld = false;
	if (bCaptureLockedMovement && Player && !Player->IsTiedToChair() && Player->GetCharacterMovement())
		Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(CaptureMovementMode), CaptureCustomMovementMode);
	bCaptureLockedMovement = false;
}

void ATallowGameMode::ResetGame()
{
	ClearCaptureState();
	if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) { Chapters->RestartChapter(ETallowRestartReason::Caught); return; }
	for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) It->SetCodeVerified(false);
	for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It) It->ResetProjector();
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
	
	for(TActorIterator<ATallowWaxPickup> It(GetWorld());It;++It)It->ResetSupply();
	if (Player)
	{
		Player->ResetInventory();
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
	for (TActorIterator<ATallowFilmReelPickup> It(GetWorld()); It; ++It) if (!It->IsActorBeingDestroyed()) It->ResetReelPickup();
	
	for (ATallowDoor* Door : CachedDoors)
	{
		if (Door)
		{
			Door->ResetDoor();
		}
	}
	
	for (ATallowMatchbox* Matchbox : Matchboxes)
	{
		if (Matchbox)
		{
			Matchbox->ResetMatchbox();
		}
	}
}
