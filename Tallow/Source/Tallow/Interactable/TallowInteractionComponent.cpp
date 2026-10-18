#include "TallowInteractionComponent.h"
#include "UI/TallowInteractWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "IInteractable.h"
#include "Player/TallowCharacter.h"
#include "Environment/TallowHideVolume.h"

UTallowInteractionComponent::UTallowInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UTallowInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// instantiate instance and timer
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		PC = Cast<APlayerController>(Pawn->GetController());
		if (PC.IsValid())
		{
			HoldProgressWidgetInstance = CreateWidget<UTallowInteractWidget>(PC.Get(), HoldProgressWidgetClass);
			if (HoldProgressWidgetInstance)
			{
				HoldProgressWidgetInstance->AddToViewport();
			}
			GetWorld()->GetTimerManager().SetTimer(HoverCheckTimerHandle, this, &UTallowInteractionComponent::DoHoverCheck, HoverCheckInterval, true);
		}
	}
}

void UTallowInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	// clear timers
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoverCheckTimerHandle);
		World->GetTimerManager().ClearTimer(HoldTimerHandle);	
	}
	
	//clear widget instance
	if (IsValid(HoldProgressWidgetInstance))
	{
		HoldProgressWidgetInstance->RemoveFromParent();
		HoldProgressWidgetInstance = nullptr;
	}
	
	ResetHoldState();
}

// Helper returns owning camera
UCameraComponent* UTallowInteractionComponent::GetOwnerCamera() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
}

// Helper to run a raycast on item
AActor* UTallowInteractionComponent::RunRaycast() const
{
	const UCameraComponent* Camera = GetOwnerCamera();
	if (!Camera) return nullptr;
	
	const FVector StartLocation = Camera->GetComponentLocation();
	const FVector EndLocation = StartLocation + (Camera->GetForwardVector() * InteractionDistance);
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	
	// run raycast
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility,
		QueryParams
	);		
	
	// if hit - return actor
	if (bHit)
	{
		return HitResult.GetActor();
	}
	
	return nullptr;
}

// called to check if player hovers on item
void UTallowInteractionComponent::DoHoverCheck()
{
	// if hidden - call hidden prompt without raycast
	ATallowCharacter* Player = Cast<ATallowCharacter>(GetOwner());
	if (Player && Player->IsHiddenInSpot())
	{
		if (ATallowHideVolume* HideVolume = Player->GetCurrentHideVolume())
		{
			if (HideVolume == CurrentHoverActor.Get()) return;
			CurrentHoverActor = HideVolume;
			FText Prompt = IInteractable::Execute_GetInteractionPromptText(HideVolume, GetOwner());
			if (HoldProgressWidgetInstance)
			{
				HoldProgressWidgetInstance->ShowPrompt(Prompt, false, true);	
			}
			return;
		}
	}
	
	AActor* HitActor = RunRaycast();
	if (HitActor && HitActor->Implements<UInteractable>())
	{
		if (HitActor == CurrentHoverActor.Get()) return;
		CurrentHoverActor = HitActor;
		
		bCanInteract = IInteractable::Execute_CanInteract(HitActor, GetOwner());
		
		const float HoldDuration = IInteractable::Execute_GetInteractionDuration(HitActor);
		bIsHoldType = HoldDuration > 0.0f;
		
		FText Prompt = IInteractable::Execute_GetInteractionPromptText(HitActor, GetOwner());
	
		// force interact to false if player cupping flame
		if (Player)
		{
			if (Player->IsHandOccupied())
			{
				bCanInteract = false;
				bIsHoldType = false;
				Prompt = FText::FromString(TEXT("HAND OCCUPIED"));	
			}
		}
		
		if (HoldProgressWidgetInstance)
		{
			HoldProgressWidgetInstance->ShowPrompt(Prompt, bIsHoldType, bCanInteract);
		}
	}
	else
	{
		// clear if nothing hit and actor valid
		if (CurrentHoverActor.IsValid())
		{
			CurrentHoverActor = nullptr;
		}
		
		if (HoldProgressWidgetInstance)
		{
			HoldProgressWidgetInstance->HidePrompt();
		}
	}
}

void UTallowInteractionComponent::StartInteract()
{
	AActor* HitActor = RunRaycast();
	if (HitActor && HitActor->Implements<UInteractable>())
	{
		if (!IInteractable::Execute_CanInteract(HitActor, GetOwner())) return;

		const float Duration = IInteractable::Execute_GetInteractionDuration(HitActor);
		// if < 0.0s run tap interact
		if (Duration <= 0.0f)
		{
			IInteractable::Execute_OnInteract(HitActor, GetOwner());	
		}
		// hold interact
		else
		{
			IInteractable::Execute_OnInteractStart(HitActor, GetOwner());
			
			CurrentInteractTarget = HitActor;
			TargetHoldDuration = Duration;
			CurrentHoldTime = 0.0f;
			bIsHoldingInteract = true;
			
			GetWorld()->GetTimerManager().SetTimer(HoldTimerHandle, this, &UTallowInteractionComponent::UpdateHoldStep, HoldTickInterval, true);
		}
	}
}

void UTallowInteractionComponent::UpdateHoldStep()
{
	// if not valid or distance too far, cancel
	if (!CurrentInteractTarget.IsValid())
	{
		CancelInteract();
		return;
	}
	
	// distance and player direction check
	if (AActor* RawActor = CurrentInteractTarget.Get())
	{
		if (PC.IsValid() && PC->PlayerCameraManager)
		{
			const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
			const FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

			// Get normalized vector
			const FVector TargetLoc = RawActor->GetActorLocation();
			const FVector DirToTarget = (TargetLoc - CameraLoc).GetSafeNormal();

			// Calculate Dot Product
			float DotProduct = FVector::DotProduct(CameraForward, DirToTarget);

			if (DotProduct <= HoldLookThreshold)
			{
				CancelInteract();
				return;
			}
		}
		if (GetOwner()->GetDistanceTo(RawActor) > InteractionDistance)
		{
			CancelInteract();
			return;
		}
	}
	
	// Update UI
	if (HoldProgressWidgetInstance)
	{
		const float Alpha = CurrentHoldTime / TargetHoldDuration;
		HoldProgressWidgetInstance->SetHoldProgress(Alpha);
	}
	
	// notify target actor of elapsed time
	IInteractable::Execute_OnInteractTick(CurrentInteractTarget.Get(), GetOwner(), HoldTickInterval, CurrentHoldTime / TargetHoldDuration);
	
	// Update hold time and perform check
	CurrentHoldTime += HoldTickInterval;
	if (CurrentHoldTime >= TargetHoldDuration)
	{
		AActor* CompletedTarget = CurrentInteractTarget.Get();
		ResetHoldState(); 
		if (CompletedTarget)
		{
			IInteractable::Execute_OnInteractComplete(CompletedTarget, GetOwner());
				
			// force an immediate hover re-evaluation 
			CurrentHoverActor = nullptr; // Invalidate the cache
			DoHoverCheck();
		}
		return;
	}
}

void UTallowInteractionComponent::CancelInteract()
{
	// if holding - stop
	if (bIsHoldingInteract)
	{
		AActor* CanceledTarget = CurrentInteractTarget.Get();
		ResetHoldState();
		
		if (CanceledTarget)
		{
			IInteractable::Execute_OnInteractCancel(CanceledTarget, GetOwner());
		}
	}
}

//clear states
void UTallowInteractionComponent::ResetHoldState()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);	
	}
	CurrentHoldTime = 0.0f;
	TargetHoldDuration = 0.0f;
	bIsHoldingInteract = false;
	CurrentInteractTarget = nullptr;
	if (HoldProgressWidgetInstance)
	{
		HoldProgressWidgetInstance->SetHoldProgress(0.0f);
	}
}

