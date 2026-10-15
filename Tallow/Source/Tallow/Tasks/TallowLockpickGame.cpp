#include "TallowLockpickGame.h"

#include "Player/TallowCharacter.h"
#include "Algo/RandomShuffle.h" 
#include "Interactable/IInteractable.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "UI/TallowLockpickWidget.h"


UTallowLockpickGame::UTallowLockpickGame()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTallowLockpickGame::BeginPlay()
{
	Super::BeginPlay();
}

void UTallowLockpickGame::GenerateLock(int32 PinCount)
{
	// reset chamber, binding sequence and progress
	PinChambers.Reset();
	BindingSequence.Reset();
	CurrentBindingStep = 0;
	
	// get random height, and add to arrays 
	for (int32 i = 0; i < PinCount; i++)
	{
		FPinChamber PinChamber;
		PinChamber.TargetHeight = FMath::RandRange(PinTargetHeightMin, PinTargetHeightMax);
		PinChamber.FalseGateHeight = FMath::Max(0.05f, PinChamber.TargetHeight - 0.10f);
		PinChambers.Add(PinChamber);
		BindingSequence.Add(i);
	}
	
	// shuffle sequence so random
	Algo::RandomShuffle(BindingSequence);
	
	// randomize tolerance
	CurrentTolerance = FMath::RandRange(0.03f, 0.06f);
	
	// randomize a pin to false gate
	PinChambers[FMath::RandRange(0, PinCount - 1)].bHasFalseGate = true;
	
	// set first pin to binding
	const int32 FirstPinIndex = BindingSequence[0];
	PinChambers[FirstPinIndex].PinState = EPinState::Binding;
	
}

void UTallowLockpickGame::StartLockpickSession(AActor* InTargetLockable, int32 PinCount)
{
	TargetLockable = InTargetLockable;
	GenerateLock(PinCount);
	
	ATallowCharacter* Player = Cast<ATallowCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	
	if (PC)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
	
	if (PC && LockpickWidgetClass)
	{
		ActiveWidgetInstance = CreateWidget<UTallowLockpickWidget>(this, LockpickWidgetClass);
		ActiveWidgetInstance->AddToViewport();
		ActiveWidgetInstance->InitializeLockpickWidget(this);
	}
	
	PickDepth = 0.0f; 
	PickLift = 0.0f; 
	TargetPickDepth = 0.0f;
	TargetPickLift = 0.0f;
	CurrentTorque = 0.0f;

	SetComponentTickEnabled(true); 
}

void UTallowLockpickGame::UpdatePickPosition(float DeltaDepth, float DeltaLift)
{
	// pick trapped in chamber
	if (TargetPickLift <= 0.15f)
	{
		TargetPickDepth = FMath::Clamp(TargetPickDepth + DeltaDepth, 0.0f, static_cast<float>(PinChambers.Num() - 1)); 
	}
	
	// pick hitting ceiling
	const int32 TargetPinIndex = FMath::RoundToInt(TargetPickDepth);
	const bool bIsUnderPin = FMath::Abs(TargetPickDepth - TargetPinIndex) < 0.15f;
	
	float MaxAllowedLift = bIsUnderPin ? 1.0f : 0.05f;
	TargetPickLift = FMath::Clamp(TargetPickLift + DeltaLift, 0.0f, MaxAllowedLift);
}

void UTallowLockpickGame::SetTensionTorque(float InTorque)
{
	if (BindingSequence.IsEmpty()) return;
	
	CurrentTorque = FMath::Clamp(InTorque, 0.0f, 1.0f);
		
	// tension dropped
	if (CurrentTorque <= 0.05f)
	{
		for (int32 i = 0; i < PinChambers.Num(); i++)
		{
			// reset pins
			PinChambers[i].PinState = EPinState::Free;
		}
		
		CurrentBindingStep = 0;
		PinChambers[BindingSequence[0]].PinState = EPinState::Binding;
		OnTensionReleased.Broadcast();
	}
}

void UTallowLockpickGame::EvaluateLockState(float DeltaTime)
{
	PickDepth = FMath::FInterpTo(PickDepth, TargetPickDepth, DeltaTime, PickMoveSpeed);
	PickLift = FMath::FInterpTo(PickLift, TargetPickLift, DeltaTime, PickMoveSpeed);
	
	ActivePinIndex = FMath::RoundToInt(PickDepth);
	for (int32 i = 0; i < PinChambers.Num(); i++)
	{
		// drop if not active pin and free
		if ((PinChambers[i].PinState == EPinState::Free || PinChambers[i].PinState == EPinState::Binding) && ActivePinIndex != i)
		{
			PinChambers[i].CurrentHeight = FMath::FInterpTo(PinChambers[i].CurrentHeight, 0.0, DeltaTime, PinSpringDropSpeed);
		}
	}
	
	// actively applying torque
	if (CurrentTorque >= 0.15f)
	{
		const int32 BindingPinIndex = BindingSequence[CurrentBindingStep];
		
		// if index matches pin to bind
		if (BindingPinIndex == ActivePinIndex)
		{
			// if false gate 
			FPinChamber& Pin = PinChambers[ActivePinIndex];
			const bool bNearFalseGate = Pin.bHasFalseGate && !Pin.bFalseGateCleared && FMath::Abs(Pin.CurrentHeight - Pin.FalseGateHeight) <= CurrentTolerance;
			if (bNearFalseGate && CurrentTorque >= 0.4)
			{
				Pin.PinState = EPinState::FalseSet;
				OnFalseSet.Broadcast();
			}
			else if (bNearFalseGate)
			{
				Pin.bFalseGateCleared = true;
			}
			
			// if struck pin right!
			else if (FMath::Abs(PinChambers[ActivePinIndex].CurrentHeight - PinChambers[ActivePinIndex].TargetHeight) <= CurrentTolerance)
			{
				PinChambers[ActivePinIndex].PinState = EPinState::Set;
				
				CurrentBindingStep++;
			
				// if finished
				if (CurrentBindingStep >= BindingSequence.Num())
				{
					OnLockpickSet.Broadcast();
					EndLockpickSession(true);
				}
				else
				{
					// arm next pin in sequence
					const int32 NextPinIndex = BindingSequence[CurrentBindingStep];
					PinChambers[NextPinIndex].PinState = EPinState::Binding;
				}
			}
			else
			{
				// overset - pushed too high
				if (PinChambers[ActivePinIndex].CurrentHeight > (PinChambers[ActivePinIndex].TargetHeight + CurrentTolerance))
				{
					PinChambers[ActivePinIndex].PinState = EPinState::Overset;
				}
			}
		}
	}
	
	bool bHasOversetPin = false;
	for (const FPinChamber& Pin : PinChambers)
	{
		if (Pin.PinState == EPinState::Overset)
		{
			bHasOversetPin = true;
			break;
		}
	}
	
	if (bHasOversetPin && CurrentTorque > 0.75f)
	{
		OversetStressTimer += DeltaTime;
		
		if (OversetStressTimer >= MaxStressDuration)
		{
			// fire audio and animation
			OnSnap.Broadcast();
			
			if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
			{
				NoiseSubsystem->EmitNoise(ETallowNoiseType::LockSnap, GetOwner()->GetActorLocation(), GetOwner(), LockpickSnapLoudness, true);	
			}
			
			// lock collapse
			for (FPinChamber& Pin : PinChambers)
			{
				Pin.PinState = EPinState::Free;
			}
			CurrentBindingStep = 0;
			PinChambers[BindingSequence[0]].PinState = EPinState::Binding;
			OversetStressTimer = 0.0f;
		}
	}
	else
	{
		// tension released or no overset pins
		OversetStressTimer = 0.0f;
	}
}

void UTallowLockpickGame::EndLockpickSession(bool bSuccess)
{
	ATallowCharacter* Player = Cast<ATallowCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	
	// restore input
	if (PC)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);	
	}
	
	SetComponentTickEnabled(false);
	
	// remove widget 
	if (ActiveWidgetInstance)
	{
		ActiveWidgetInstance->RemoveFromParent();
		ActiveWidgetInstance = nullptr;
	}
	
	if (TargetLockable.IsValid())
	{
		if (bSuccess)
		{
			IInteractable::Execute_OnInteractComplete(TargetLockable.Get(), GetOwner());
		}
		else
		{
			IInteractable::Execute_OnInteractCancel(TargetLockable.Get(), GetOwner());
		}
	}
	TargetLockable = nullptr;
}

