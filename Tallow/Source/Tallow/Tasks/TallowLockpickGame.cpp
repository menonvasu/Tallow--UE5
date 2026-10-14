#include "TallowLockpickGame.h"

#include "Player/TallowCharacter.h"
	#include "Algo/RandomShuffle.h" 
#include "Noise/TallowNoiseSubsystem.h"


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
	
	PickDepth = 0.0f; 
	PickLift = 0.0f; 
	CurrentTorque = 0.0f;

	SetComponentTickEnabled(true); 
}

void UTallowLockpickGame::UpdatePickPosition(float DeltaDepth, float DeltaLift)
{
	PickDepth = FMath::Clamp(PickDepth + DeltaDepth , 0.0f, static_cast<float>(PinChambers.Num() - 1));
	PickLift = FMath::Clamp(PickLift + DeltaLift, 0.0f, 1.0f);
	
	ActivePinIndex = FMath::RoundToInt(PickDepth);
	// aligned under chamber!
	if (FMath::Abs(PickDepth - ActivePinIndex) <= 0.2f)
	{
		PinChambers[ActivePinIndex].CurrentHeight = PickLift;
	}
}

void UTallowLockpickGame::SetTensionTorque(float InTorque)
{
	CurrentTorque = FMath::Clamp(InTorque, 0.0f, 1.0f);
	
	// tension dropped
	if (CurrentTorque <= 0.05f)
	{
		for (int32 i = 0; i < PinChambers.Num(); i++)
		{
			// reset pins
			PinChambers[i].CurrentHeight = (i == ActivePinIndex) ? PickLift : 0.0f;
			PinChambers[i].PinState = EPinState::Free;
		}
		
		CurrentBindingStep = 0;
		PinChambers[BindingSequence[0]].PinState = EPinState::Binding;
		OnTensionReleased.Broadcast();
	}
}

void UTallowLockpickGame::EvaluateLockState(float DeltaTime)
{
	if (!PinChambers.IsValidIndex(ActivePinIndex)) { return;} 
	
	if(FMath::Abs(PickDepth - ActivePinIndex) <= 0.2f)
	{
		PinChambers[ActivePinIndex].CurrentHeight = PickLift;
	}
	for (int32 i = 0; i < PinChambers.Num(); i++)
	{
		// drop if not active pin and free
		if (PinChambers[i].PinState == EPinState::Free && ActivePinIndex != i)
		{
			PinChambers[i].CurrentHeight = 0.0f;	
		}
	}
	
	// actively applying torque
	if (CurrentTorque >= 0.15f)
	{
		const int32 BindingPinIndex = BindingSequence[CurrentBindingStep];
		
		// if index matches pin to bind
		if (BindingPinIndex == ActivePinIndex)
		{
			// if struck pin right!
			if (FMath::Abs(PinChambers[ActivePinIndex].CurrentHeight - PinChambers[ActivePinIndex].TargetHeight) <= CurrentTolerance)
			{
				PinChambers[ActivePinIndex].PinState = EPinState::Set;
				OnLockpickSet.Broadcast();
				CurrentBindingStep++;
			
				// if finished
				if (CurrentBindingStep >= BindingSequence.Num())
				{
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
				Pin.CurrentHeight = 0.0f;
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

void UTallowLockpickGame::EndLockpickSession(bool bSuccess) {}

