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
	SetDwellTimer = 0.0f;
	
	// randomize tolerance
	CurrentTolerance = FMath::RandRange(0.03f, 0.045f);
	
	// get random heights, set false gates and add to arrays 
	for (int32 i = 0; i < PinCount; i++)
	{
		FPinChamber PinChamber;
		PinChamber.TargetHeight = FMath::RandRange(PinTargetHeightMin, PinTargetHeightMax);
		const float FalseHeight = FMath::Max(0.05f, PinChamber.TargetHeight - (CurrentTolerance * 2.0f + 0.06f));
		PinChamber.FalseGateHeight = FalseHeight;
		PinChambers.Add(PinChamber);
		BindingSequence.Add(i);
		if (FMath::FRand() <= FalseGateChance)
		{
			PinChambers[i].bHasFalseGate = true;
		}
	}
	
	// at least 1: randomize a pin to false gate
	PinChambers[FMath::RandRange(0, PinCount - 1)].bHasFalseGate = true;
	
	// shuffle sequence so random
	Algo::RandomShuffle(BindingSequence);
	
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
		ActiveWidgetInstance = CreateWidget<UTallowLockpickWidget>(PC, LockpickWidgetClass);
		ActiveWidgetInstance->AddToViewport();
		ActiveWidgetInstance->InitializeLockpickWidget(this);
	}
	
	PickDepth = 0.0f; 
	PickLift = 0.0f; 
	TargetPickDepth = 0.0f;
	TargetPickLift = 0.0f;
	TargetTorque = 0.0f;
	CurrentTorque = 0.0f;
	bTensionHeld = false;
	bTensionResetDone = true;
	bCompleting = false;

	SetComponentTickEnabled(true); 
}

void UTallowLockpickGame::UpdatePickPosition(float DeltaDepth, float DeltaLift)
{
	if (bCompleting) { return; }
	
	// pick trapped in chamber
	if (TargetPickLift <= 0.15f)
	{
		TargetPickDepth = FMath::Clamp(TargetPickDepth + DeltaDepth, 0.0f, static_cast<float>(PinChambers.Num() - 1)); 
	}
	
	// pick hitting ceiling
	const int32 TargetPinIndex = FMath::RoundToInt(TargetPickDepth);
	const bool bIsUnderPin = FMath::Abs(TargetPickDepth - TargetPinIndex) < 0.15f;
	
	float MaxAllowedLift = bIsUnderPin ? 1.0f : 0.05f;
	
	// if falseset, don't allow further push
	if (PinChambers.IsValidIndex(TargetPinIndex) && PinChambers[TargetPinIndex].PinState == EPinState::FalseSet)
	{
		MaxAllowedLift = FMath::Min(MaxAllowedLift, PinChambers[TargetPinIndex].FalseGateHeight);
	}
	
	// more resistance on binding pin 
	const bool bOnBindingPin = BindingSequence.IsValidIndex(CurrentBindingStep) && TargetPinIndex == BindingSequence[CurrentBindingStep];
	if (DeltaLift > 0.0f && bIsUnderPin && bOnBindingPin)
	{
		DeltaLift *= FMath::Lerp(1.0f, BindingResistanceMin, FMath::Max(CurrentTorque, 0.3f));
	}
	
	TargetPickLift = FMath::Clamp(TargetPickLift + DeltaLift, 0.0f, MaxAllowedLift);
}

void UTallowLockpickGame::UpdateSmoothing(float DeltaTime)
{
	TargetTorque = FMath::FInterpConstantTo(TargetTorque, bTensionHeld ? 1.0f : 0.0f, DeltaTime, bTensionHeld ? TorqueRiseRate : TorqueFallRate);
	CurrentTorque = TargetTorque;
	PickDepth = FMath::FInterpTo(PickDepth, TargetPickDepth, DeltaTime, PickMoveSpeed);
	PickLift = FMath::FInterpTo(PickLift, TargetPickLift, DeltaTime, PickMoveSpeed);
	ActivePinIndex = FMath::RoundToInt(PickDepth);
	
	FPinChamber& Active = PinChambers[ActivePinIndex];
	// couple pin height w pick if not set
	if (Active.PinState != EPinState::Set && FMath::Abs(PickDepth - ActivePinIndex) <= 0.2f)
	{
		Active.CurrentHeight = PickLift;
	}
}

void UTallowLockpickGame::SimulatePinGravity(float DeltaTime)
{
	// tension dropped
	const bool bTensionDropped = (CurrentTorque <= 0.05f && TargetTorque <= 0.05f);
	if (bTensionDropped)
	{
		if (!bTensionResetDone)
		{
			bTensionResetDone = true;
			const bool bHadProgress = CurrentBindingStep > 0;

			for (int32 i = 0; i < PinChambers.Num(); i++)
			{
				// reset pins
				PinChambers[i].PinState = EPinState::Free;
			}
			CurrentBindingStep = 0;
			PinChambers[BindingSequence[0]].PinState = EPinState::Binding;
			SetDwellTimer = 0.0f;
			if (bHadProgress)
			{
				OnTensionReleased.Broadcast();		
			}
		}
	}
	else
	{
		bTensionResetDone = false;
	}
	
	// drop if not active pin and set
	for (int32 i = 0; i < PinChambers.Num(); i++)
	{
		FPinChamber& Pin = PinChambers[i];
		if (Pin.PinState == EPinState::Set || ActivePinIndex == i) { continue; }
		
		// pick left this pin, so a stressed or false set pin recovers
		if (Pin.PinState == EPinState::Overset || Pin.PinState == EPinState::FalseSet)
		{
			Pin.PinState = EPinState::Binding;
		}
		
		Pin.CurrentHeight = FMath::FInterpTo(Pin.CurrentHeight, 0.0f, DeltaTime, PinSpringDropSpeed);
	}
}

void UTallowLockpickGame::EvaluateActiveChamber(float DeltaTime)
{
	// run if only applying torque
	if (!(CurrentTorque >= 0.15f && BindingSequence.IsValidIndex(CurrentBindingStep)))
	{
		SetDwellTimer = 0.0f;
		return;
	}
	
	const int32 BindingPinIndex = BindingSequence[CurrentBindingStep];
	if (ActivePinIndex != BindingPinIndex)
	{
		SetDwellTimer = 0.0f;
		return; // exit if not poking active 
	}
	
	FPinChamber& Pin = PinChambers[ActivePinIndex]; // active pin 
	
	// if falseset and low torque - allow
	if (Pin.PinState == EPinState::FalseSet)
	{
		SetDwellTimer = 0.0f;
		if (CurrentTorque < 0.4f)
		{
			Pin.PinState = EPinState::Binding;
		}
		return;
	}
	
	const bool bInsideFalseGate = Pin.bHasFalseGate && FMath::Abs(Pin.CurrentHeight - Pin.FalseGateHeight) <= CurrentTolerance;
	const bool bInsideSetWindow = FMath::Abs(Pin.CurrentHeight - Pin.TargetHeight) <= CurrentTolerance;
	// if false gate
	if (bInsideFalseGate)
	{
		SetDwellTimer = 0.0f;
		// if player using heavy tension
		if (CurrentTorque >= 0.4)
		{
			Pin.PinState = EPinState::FalseSet;
			OnFalseSet.Broadcast();
		}
	}
	// if struck pin right!
	else if (bInsideSetWindow)
	{	
		//ignore if under duration
		SetDwellTimer += DeltaTime;
		if (SetDwellTimer < SetDwellDuration) { return; }
		
		SetDwellTimer = 0.0f;
		Pin.PinState = EPinState::Set;
		CurrentBindingStep++;
		OnPinSet.Broadcast();
				
		// if finished
		if (CurrentBindingStep >= BindingSequence.Num())
		{
			bCompleting = true;
			OnLockpickSet.Broadcast();
			GetWorld()->GetTimerManager().SetTimer(CompleteTimerHandle, this, &UTallowLockpickGame::FinishSuccess, UnlockDelay, false);
		}
		else
		{
			// arm next pin in sequence
			PinChambers[BindingSequence[CurrentBindingStep]].PinState = EPinState::Binding;
		}
	}
	else
	{
		SetDwellTimer = 0.0f;
		// over set - pushed too high
		if (Pin.CurrentHeight > (Pin.TargetHeight + CurrentTolerance + OversetMargin))
		{
			if (Pin.PinState != EPinState::Overset)
			{
				Pin.PinState = EPinState::Overset;
				OnOverset.Broadcast();
			}
		}
		else if (Pin.PinState == EPinState::Overset)
		{
			Pin.PinState = EPinState::Binding; // came back down, recovered
		}
	}
}

void UTallowLockpickGame::EvaluatePickStress(float DeltaTime)
{
	// scans overset pins
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
			SetDwellTimer = 0.0f;
		}
	}
	else
	{
		// tension released or no overset pins
		OversetStressTimer = 0.0f;
	}	
}

void UTallowLockpickGame::EvaluateLockState(float DeltaTime)
{
	if (PinChambers.IsEmpty()) return;
	
	UpdateSmoothing(DeltaTime);
	if (bCompleting) { return; } 
	
	SimulatePinGravity(DeltaTime);
	EvaluateActiveChamber(DeltaTime);
	EvaluatePickStress(DeltaTime);
}

void UTallowLockpickGame::EndLockpickSession(bool bWasSuccessful)
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
	
	// clear timer
	GetWorld()->GetTimerManager().ClearTimer(CompleteTimerHandle);
	bCompleting = false;
	
	if (TargetLockable.IsValid())
	{
		if (bWasSuccessful)
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

void UTallowLockpickGame::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EvaluateLockState(DeltaTime);
} 
