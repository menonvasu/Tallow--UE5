#include "TallowCharacter.h"
#include "Player/Map/TallowHouseMapComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interactable/IInteractable.h"
#include "Tasks/TallowFilmReelPickup.h"
#include "Engine/StaticMesh.h"
#include "Candle/TallowCandleComponent.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Noise/TallowFootstepNoiseComponent.h"
#include "Components/PointLightComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionSystem.h"
#include "Tasks/TallowChairEscape.h"
#include "Environment/TallowHideVolume.h"
#include "Interactable/TallowInteractionComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Items/TallowItemDefinition.h"
#include "Tasks/TallowLockpickGame.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Tallow.h"


ATallowCharacter::ATallowCharacter()
{
	// Ticks only for the held breath running out (UpdateHeldBreath).
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	constexpr float PlayerHalfHeight = 60.0f;
	GetCapsuleComponent()->InitCapsuleSize(30.f, PlayerHalfHeight);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -PlayerHalfHeight));

	// Create spring arm
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Glide camera smoothly
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->CameraLagSpeed = 15.0f;

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = false;

	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// Create first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetOnlyOwnerSee(true);

	// Configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// Configure character movement/crouch
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(PlayerHalfHeight / 2.0f);

	// Create candle and light component
	CandleComponent = CreateDefaultSubobject<UTallowCandleComponent>(TEXT("Candle Component"));
	CandleFlame = CreateDefaultSubobject<UTallowCandleFlameComponent>(TEXT("Candle Flame"));
	FootstepNoiseComponent = CreateDefaultSubobject<UTallowFootstepNoiseComponent>(TEXT("Footstep Noise Component"));
	CandleLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("CandleLight"));

	// Configure light
	CandleLightComponent->SetupAttachment(FirstPersonCameraComponent);
	CandleLightComponent->SetRelativeLocation(FVector(40.0f, 25.0f, -15.0f));
	CandleLightComponent->SetAttenuationRadius(800.0f);
	CandleLightComponent->SetIntensity(3000.0f);

	// Create stimulus
	StimulusSourceComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimulusSourceComponent"));
	if (StimulusSourceComponent)
	{
		StimulusSourceComponent->bAutoRegister = true;
	}
	
	// Interaction component
	InteractionComponent = CreateDefaultSubobject<UTallowInteractionComponent>(TEXT("Interaction Component"));
	HouseMapComponent = CreateDefaultSubobject<UTallowHouseMapComponent>(TEXT("House Map"));
	
	// Held Item Mesh
	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItemMesh"));
	HeldItemMesh->SetupAttachment(FirstPersonCameraComponent);
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetVisibility(false);
	HeldItemMesh->SetRelativeLocation(FVector(45.0f, 25.0f, -25.0f));
	
	// Lockpick component
	LockpickComponent = CreateDefaultSubobject<UTallowLockpickGame>(TEXT("LockpickComponent"));
}

void ATallowCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ATallowCharacter::ToggleHouseMap);	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ATallowCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoJumpEnd);

		// Crouching
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ATallowCharacter::DoCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoCrouchEnd);

		// Walking
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Triggered, this, &ATallowCharacter::MoveInput);

		// Running
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &ATallowCharacter::DoRunStart);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoRunEnd);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATallowCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ATallowCharacter::LookInput);
		
		// Hold - Interact
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATallowCharacter::DoInteractStart);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoInteractEnd);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ATallowCharacter::DoInteractEnd);
		}
		
		// Cupping candle (actions come from the candle MR; skip if the BP has not wired them yet)
		if (CupAction)
		{
			EnhancedInputComponent->BindAction(CupAction, ETriggerEvent::Started, this, &ATallowCharacter::DoCup);
			EnhancedInputComponent->BindAction(CupAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoCupEnd);
		}

		// Relight candle
		if (RelightAction)
		{
			EnhancedInputComponent->BindAction(RelightAction, ETriggerEvent::Started, this, &ATallowCharacter::DoRelight);
		}

		// Stillness
		if (StillnessAction)
		{
			EnhancedInputComponent->BindAction(StillnessAction, ETriggerEvent::Started, this, &ATallowCharacter::DoStillnessStart);
			EnhancedInputComponent->BindAction(StillnessAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoStillnessEnd);
			EnhancedInputComponent->BindAction(StillnessAction, ETriggerEvent::Canceled, this, &ATallowCharacter::DoStillnessEnd);
		}
		
		// Drop item
		if (DropAction)
		{
			EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &ATallowCharacter::DoDropItem);
		}
		
		// Tension action
		if (TensionAction)
		{
			EnhancedInputComponent->BindAction(TensionAction, ETriggerEvent::Started, this, &ATallowCharacter::DoTensionStart);
			EnhancedInputComponent->BindAction(TensionAction, ETriggerEvent::Completed, this, &ATallowCharacter::DoTensionEnd);
			EnhancedInputComponent->BindAction(TensionAction, ETriggerEvent::Canceled, this, &ATallowCharacter::DoTensionEnd);
		}
	}
	else
	{
		UE_LOG(LogTallow, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void ATallowCharacter::BeginPlay()
{
	Super::BeginPlay();

	StandingBoomZ = CameraBoom->GetRelativeLocation().Z;
	LastCapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Bind event to set light perception
	if (CandleComponent)
	{
		CandleComponent->OnCandleStateChanged.AddDynamic(this, &ATallowCharacter::SetLightPerception);
		SetLightPerception(CandleComponent->GetCandleState());
	}

	// Hide head mesh clipping
	if (FirstPersonMesh)
	{
		FirstPersonMesh->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);
	}
}

void ATallowCharacter::SetLightPerception(ECandleState NewState)
{
	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(GetWorld());
	if (!PerceptionSystem)
	{
		return;
	}

	if (bIsHoldingStill || NewState == ECandleState::Out)
	{
		PerceptionSystem->UnregisterSource(*this, UAISense_Sight::StaticClass());
	}
	else if (NewState == ECandleState::Lit || NewState == ECandleState::Cupped)
	{
		PerceptionSystem->RegisterSourceForSenseClass(UAISense_Sight::StaticClass(), *this);
	}
}

bool ATallowCharacter::IsHandOccupied() const
{
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped)
	{
		return true;
	}
	return false;
}

int32 ATallowCharacter::GetInventoryCount(ECarriedItemType Item) const
{
	return Item == ECarriedItemType::None ? 0 : PocketItems.FindRef(Item) + (CarriedItem == Item ? 1 : 0);
}

void ATallowCharacter::PrepareForInventory()
{
	if (InteractionComponent) InteractionComponent->CancelInteract();
	DoRunEnd();
	DoStillnessEnd();
	DoCupEnd();
	GetCharacterMovement()->StopMovementImmediately();
}

void ATallowCharacter::PrepareForHiding()
{
	if (InteractionComponent) InteractionComponent->CancelInteract();
	DoRunEnd();
	GetCharacterMovement()->StopMovementImmediately();
}

void ATallowCharacter::ToggleHouseMap()
{
	if(HouseMapComponent)HouseMapComponent->ToggleMap();
}

bool ATallowCharacter::CanChangeInventory() const
{
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATallowGameMode>() : nullptr;
	return !IsTiedToChair() && !bIsHoldingStill && !(GM && GM->IsCaptureInProgress())
		&& !(CandleComponent && CandleComponent->IsRelighting())
		&& !(InteractionComponent && InteractionComponent->IsInteracting())
		&& !(LockpickComponent && LockpickComponent->IsComponentTickEnabled());
}

bool ATallowCharacter::CanCollectItem(ECarriedItemType Item) const
{
	return CanChangeInventory() && (Item == ECarriedItemType::Crowbar || Item == ECarriedItemType::Fuse || Item == ECarriedItemType::FilmReel)
		&& GetInventoryCount(Item) < (Item == ECarriedItemType::Fuse ? 3 : 1);
}

bool ATallowCharacter::CollectItem(ECarriedItemType Item)
{
	if (!CanCollectItem(Item)) return false;
	StowCarriedItem();
	SetCarriedItem(Item);
	return true;
}

void ATallowCharacter::StowCarriedItem()
{
	if (!CanChangeInventory()) return;
	if (CarriedItem != ECarriedItemType::None) ++PocketItems.FindOrAdd(CarriedItem);
	ClearCarriedItem();
}

bool ATallowCharacter::EquipInventoryItem(ECarriedItemType Item)
{
	if (!CanChangeInventory()) return false;
	if (Item == CarriedItem) return true;
	if (Item == ECarriedItemType::None) { StowCarriedItem(); return true; }
	if (PocketItems.FindRef(Item) <= 0) return false;
	StowCarriedItem();
	--PocketItems.FindChecked(Item);
	SetCarriedItem(Item);
	return true;
}

void ATallowCharacter::ResetInventory()
{
	PocketItems.Reset();
	ClearCarriedItem();
}

void ATallowCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ATallowCharacter::LookInput(const FInputActionValue& Value)
{
	// if in lock pick
	if (LockpickComponent && LockpickComponent->IsComponentTickEnabled())
	{
		FVector2D LookAxisVector = Value.Get<FVector2D>();
		LockpickComponent->UpdatePickPosition(LookAxisVector.X * 0.08f, -LookAxisVector.Y * 0.05f); // sens scalar
		return;
	}
	
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ATallowCharacter::DoAim(float Yaw, float Pitch)
{

	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ATallowCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ATallowCharacter::DoRunStart()
{
	// return early (cannot sprint while cupping, or while holding her breath)
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped) return;
	if (bIsHoldingStill) return;

	// pass run speed to maxwalkspeed and call sprint start in candle component to extinguish flame
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	if (CandleComponent)
	{
		CandleComponent->HandleSprintStarted();
	}
}

void ATallowCharacter::DoRunEnd()
{
	if (CandleComponent)
	{
		CandleComponent->HandleSprintEnded();
	}
	
	GetCharacterMovement()->MaxWalkSpeed = Walkspeed;
}

void ATallowCharacter::DoJumpStart()
{
	// if tied rock, else jump
	if (CurrentTiedChair)
	{
		CurrentTiedChair->AttemptRock();
		return;
	}
	Jump();
}

void ATallowCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void ATallowCharacter::DoCrouch()
{
	// pass Crouch to character
	Crouch();
}

void ATallowCharacter::DoCrouchEnd()
{
	// pass Uncrouch to character
	UnCrouch();
}

void ATallowCharacter::DoInteractStart()
{
	// if player is cupping, interact hand is occupied
	if (IsHandOccupied()) return;

	// if hidden, call exit
	if (CurrentHideVolume)
	{
		IInteractable::Execute_OnInteract(CurrentHideVolume, this);
		return;
	}
	
	// if in lock pick minigame, call exit
	if (LockpickComponent && LockpickComponent->IsComponentTickEnabled() && LockpickComponent->GetIsCompleting() != true)
	{
		LockpickComponent->EndLockpickSession(false);
		return;
	}
	
	// if free run interaction
	if (InteractionComponent)
	{
		InteractionComponent->StartInteract();	
	}
}

void ATallowCharacter::DoInteractEnd()
{
	if (UAnimInstance* AnimInst = FirstPersonMesh->GetAnimInstance())
	{
		AnimInst->Montage_Stop(0.2f); 
	}
	if (InteractionComponent)
	{
		InteractionComponent->CancelInteract();
	}
}

void ATallowCharacter::DoCup()
{
	// set state to cupped only if lit
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Lit)
	{
		CandleComponent->SetCandleState(ECandleState::Cupped);
	}
}

void ATallowCharacter::DoCupEnd()
{
	// set state to lit only if cupped
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped)
	{
		CandleComponent->SetCandleState(ECandleState::Lit);
	}
}

void ATallowCharacter::DoRelight()
{
	// Tied to the chair her hands are bound: no match (a strike here flared with no match in view).
	if (!CandleComponent || !CandleComponent->IsAcquired() || IsTiedToChair()) return;
	// R toggles: blow the candle out to hide from the Keeper, or strike a match to relight it
	if (CandleComponent->GetCandleState() == ECandleState::Out) CandleComponent->StartRelighting();
	else CandleComponent->SetCandleState(ECandleState::Out);
}

void ATallowCharacter::DoStillnessStart()
{
	if (IsTiedToChair() || bIsHoldingStill) return;
	// Out of breath: she has to breathe again before she can hold it.
	if (HeldAir < .25f || BreathRecovery > 0.f) return;

	StillnessStartTime = GetWorld()->GetTimeSeconds();

	bIsHoldingStill = true;
	bBreathForced = false;
	OnStill.Broadcast(bIsHoldingStill);

	// Ignore player capsule
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// 25 and 28 Oct: holding your breath silences you, it doesn't freeze you. She can still look round and
	// creep (UpdateHeldBreath keeps her under HeldBreathCreepSpeed); her steps make no sound while she holds it.
	DoRunEnd();
	GetCharacterMovement()->MaxWalkSpeed = FMath::Min(GetCharacterMovement()->MaxWalkSpeed, HeldBreathCreepSpeed);

	// Make player immune
	if (StimulusSourceComponent)
	{
		StimulusSourceComponent->UnregisterFromSense(UAISense_Sight::StaticClass());
	}

	// RVO Avoidance
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceWeight = 1.0f;
}

void ATallowCharacter::DoStillnessEnd()
{
	if (!bIsHoldingStill) return;
	const double HoldDuration = GetWorld()->GetTimeSeconds() - StillnessStartTime;
	const bool bForced = bBreathForced;
	bBreathForced = false;

	bIsHoldingStill = false;
	OnStill.Broadcast(bIsHoldingStill);

	// only transform if reasonably held 
	if (HoldDuration >= MinStillnessHoldDuration)
	{
		TransformationLevel++;
		OnTransformation.Broadcast(TransformationLevel);
	}

	// Restore collision 
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// Back to her walking pace for the candle she is holding (shielding it still slows her).
	if (CandleComponent) CandleComponent->HandleSprintEnded();
	else GetCharacterMovement()->MaxWalkSpeed = Walkspeed;

	// Restore sight
	if (CandleComponent)
	{
		SetLightPerception(GetCandleComponent()->GetCandleState());
	}

	GetCharacterMovement()->AvoidanceWeight = 0.0f;
	// A long hold leaves her needing a moment before the breath comes back; letting go at the limit, longer.
	BreathRecovery = bForced ? 1.4f : HoldDuration > 2.0 ? .5f : 0.f;
	OnBreathReleased.Broadcast(bForced);
}

void ATallowCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeldBreath(DeltaSeconds);
	UpdatePanting(DeltaSeconds);
	UpdateCrouchEye(DeltaSeconds);
	UpdateWalkBob(DeltaSeconds);
}

void ATallowCharacter::UpdateCrouchEye(float DeltaSeconds)
{
	// The capsule shrinks when she crouches but the eye rode on it, so a crouched eye still sat 109 cm up, above
	// the 68 cm crawl gaps. Keep the eye where it was through the change, then ease it down to just under the
	// crouched capsule's top; standing returns to the authored boom height.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Boom = CameraBoom->GetRelativeLocation();
	Boom.Z += LastCapsuleHalfHeight - HalfHeight;
	LastCapsuleHalfHeight = HalfHeight;
	const float EyeAboveBoom = FirstPersonCameraComponent->GetComponentLocation().Z - CameraBoom->GetComponentLocation().Z;
	const float Target = bIsCrouched ? HalfHeight - CrouchEyeBelowCapsuleTop - EyeAboveBoom : StandingBoomZ;
	Boom.Z = FMath::FInterpTo(Boom.Z, Target, DeltaSeconds, 9.f);
	CameraBoom->SetRelativeLocation(Boom);
}

void ATallowCharacter::UpdateWalkBob(float DeltaSeconds)
{
	if (!CameraBoom || !FirstPersonCameraComponent || DeltaSeconds <= 0.f) return;
	const float Speed = GetVelocity().Size2D();
	const bool bWalking = GetCharacterMovement()->IsMovingOnGround() && !IsTiedToChair() && !bIsHoldingStill && Speed > 25.f;
	const float StrideLength = FootstepNoiseComponent ? FootstepNoiseComponent->GetStrideLength() : 60.f;
	// One footfall per stride: the phase turns half a cycle per step, the same distance the footstep sound uses.
	if (bWalking) WalkBobDistance += Speed * DeltaSeconds / FMath::Max(StrideLength, 10.f) * PI;
	WalkBobAlpha = FMath::FInterpTo(WalkBobAlpha, bWalking ? 1.f : 0.f, DeltaSeconds, bWalking ? 6.f : 9.f);
	WalkBobRunAlpha = FMath::FInterpTo(WalkBobRunAlpha, bWalking ? FMath::GetMappedRangeValueClamped(FVector2D(120.f, 380.f), FVector2D(0.f, 1.f), Speed) : 0.f, DeltaSeconds, 5.f);
	const float Strength = WalkBobAlpha * FMath::Lerp(.7f, 1.6f, WalkBobRunAlpha);
	WalkBobDistance = FMath::Fmod(WalkBobDistance, 2.f * PI * 1000.f);
	const float Dip = -.9f * Strength * FMath::Cos(2.f * WalkBobDistance);
	const float Roll = .35f * Strength * FMath::Sin(WalkBobDistance);
	FVector Offset = CameraBoom->SocketOffset; Offset.Z = Dip; CameraBoom->SocketOffset = Offset;
	FirstPersonCameraComponent->ClearAdditiveOffset();
	if (Strength > KINDA_SMALL_NUMBER)
		FirstPersonCameraComponent->AddAdditiveOffset(FTransform(FRotator(0.f, 0.f, Roll)), 0.f);
}

void ATallowCharacter::UpdateHeldBreath(float DeltaSeconds)
{
	// Back in the chair (the start, or a capture reset): she breathes again, full.
	if (IsTiedToChair())
	{
		if (bIsHoldingStill) DoStillnessEnd();
		HeldAir = 1.f; BreathRecovery = 0.f;
		return;
	}
	if (!bIsHoldingStill)
	{
		BreathRecovery = FMath::Max(0.f, BreathRecovery - DeltaSeconds);
		if (BreathRecovery <= 0.f) HeldAir = FMath::Min(1.f, HeldAir + DeltaSeconds / FMath::Max(BreathRecoverSeconds, .5f));
		return;
	}
	// Mim within a few metres: her chest burns sooner (29 Oct: a close call should be a real struggle).
	const bool bKeeperFresh = GetWorld() && GetWorld()->GetTimeSeconds() - KeeperDistanceAt < .5;
	const float Close = bKeeperFresh ? FMath::GetMappedRangeValueClamped(FVector2D(150.f, 450.f), FVector2D(1.f, 0.f), KeeperDistance) : 0.f;
	HeldAir = FMath::Max(0.f, HeldAir - DeltaSeconds * FMath::Lerp(1.f, BreathCloseDrain, Close) / FMath::Max(BreathHoldSeconds, 1.f));
	// Shielding the candle resets her pace; holding her breath she only ever creeps.
	GetCharacterMovement()->MaxWalkSpeed = FMath::Min(GetCharacterMovement()->MaxWalkSpeed, HeldBreathCreepSpeed);
	if (HeldAir > 0.f) return;
	// She can't hold it any longer: one gasp for air, a real sound the Keeper can hear if it is close.
	if (auto* Noise = UTallowNoiseSubsystem::Get(this))
		Noise->EmitCustomNoise(FirstPersonCameraComponent->GetComponentLocation(), this, BreathGaspLoudness, BreathGaspRange);
	bBreathForced = true;
	DoStillnessEnd();
}

void ATallowCharacter::ClearKeeperAwareness()
{
	KeeperSight = 0.f; bSeenByKeeper = false; KeeperSeenFrom = FVector::ZeroVector; SeenCallAt = -100.f;
	KeeperDistance = 1.e6f; KeeperDistanceAt = -100.0;
	SprintedSeconds = 0.f; PantUntil = -100.0;
	if (auto* Flame = FindComponentByClass<UTallowCandleFlameComponent>()) Flame->ClearSeen();
}

void ATallowCharacter::UpdatePanting(float DeltaSeconds)
{
	if (IsTiedToChair()) { SprintedSeconds = 0.f; PantUntil = -100.0; return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bSprinting = GetCharacterMovement()->IsMovingOnGround() && GetVelocity().Size2D() > 350.f;
	if (bSprinting) SprintedSeconds += DeltaSeconds;
	else if (SprintedSeconds > 0.f)
	{
		// A real run (not a two-step burst) leaves her out of breath for a little longer the longer she ran.
		if (SprintedSeconds >= PantAfterSprintSeconds) PantUntil = FMath::Max(PantUntil, Now + FMath::Clamp(SprintedSeconds * .6f, 3.f, 7.f));
		SprintedSeconds = FMath::Max(0.f, SprintedSeconds - DeltaSeconds * 4.f);
	}
	if (!IsPanting() || Now < NextPantNoise) return;
	// Each gulp of air is a real sound, about as loud as a step, heard only close by and dulled by the walls.
	NextPantNoise = Now + FMath::FRandRange(.8f, 1.1f);
	if (auto* Noise = UTallowNoiseSubsystem::Get(this))
		Noise->EmitCustomNoise(FirstPersonCameraComponent->GetComponentLocation(), this, PantLoudness, PantRange);
}

bool ATallowCharacter::IsPanting() const
{
	return !bIsHoldingStill && GetWorld() && GetWorld()->GetTimeSeconds() < PantUntil
		&& GetVelocity().Size2D() < 350.f;
}

float ATallowCharacter::GetHeldSeconds() const
{
	return bIsHoldingStill && GetWorld() ? float(GetWorld()->GetTimeSeconds() - StillnessStartTime) : 0.f;
}

float ATallowCharacter::GetHeldStrain() const
{
	if (!bIsHoldingStill) return 0.f;
	const float Held = (1.f - HeldAir) * BreathHoldSeconds;
	return FMath::Clamp((Held - BreathStrainSeconds) / FMath::Max(BreathHoldSeconds - BreathStrainSeconds, .1f), 0.f, 1.f);
}

void ATallowCharacter::DoDropItem()
{
	bool bFoundValidLocation = false;
	if (!CanChangeInventory() || CarriedItem == ECarriedItemType::None || !FirstPersonCameraComponent) { return; }
	UTallowItemDefinition* ItemDef = ItemRegistry.FindRef(CarriedItem);
	const TSubclassOf<AActor> PickupClass = ItemDef && ItemDef->PickupClass ? ItemDef->PickupClass.Get()
		: CarriedItem == ECarriedItemType::FilmReel ? ATallowFilmReelPickup::StaticClass() : nullptr;
	if (!PickupClass) return;
	
	const FVector StartLocation = FirstPersonCameraComponent->GetComponentLocation() + (GetActorForwardVector() * 40.0f);
	const FVector EndLocation = StartLocation + (FirstPersonCameraComponent->GetForwardVector() * 220.0f);
	const FVector DowncastEndLocation = StartLocation - FVector(0.0f, 0.0f, 150.0f);
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	
	// run initial raycast
	bool bHitSurface = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility,
		QueryParams
	);
	
	// if surface valid (surface angle < 45), save location
	if (bHitSurface && HitResult.ImpactNormal.Z >= 0.7f)
	{
		bFoundValidLocation = true;
		SpawnLocation = HitResult.Location + (HitResult.ImpactNormal * 2.0f);
		SpawnRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f );
	}
	else
	{
		// run downward raycast
		bool bHitFloor = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			StartLocation,
			DowncastEndLocation,
			ECollisionChannel::ECC_Visibility,
			QueryParams
		);	
		
		// if hit floor, save location
		if (bHitFloor && HitResult.ImpactNormal.Z >= 0.7f)
		{
			bFoundValidLocation = true;
			SpawnLocation = HitResult.Location + (HitResult.ImpactNormal * 2.0f);
			SpawnRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f );
		}
	}
	
	// return if no valid surface found 
	if (!bFoundValidLocation) { return; }
	// check item - spawn and clear 
	if (SpawnItem(PickupClass, SpawnLocation + (CarriedItem == ECarriedItemType::FilmReel ? FVector(0,0,11.5f) : FVector::ZeroVector), SpawnRotation))
	{
		ClearCarriedItem();
	}
}

void ATallowCharacter::DoTensionStart()
{
	if (LockpickComponent && LockpickComponent->IsComponentTickEnabled())
	{
		LockpickComponent->SetTensionHeld(true);
	}
}

void ATallowCharacter::DoTensionEnd()
{
	if (LockpickComponent)
	{
		LockpickComponent->SetTensionHeld(false);
	}
}

AActor* ATallowCharacter::SpawnItem(TSubclassOf<AActor> ItemPickupClass, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!ItemPickupClass) { return nullptr; }
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AActor* SpawnedItem = GetWorld()->SpawnActor<AActor>(ItemPickupClass, SpawnLocation, SpawnRotation, SpawnParams);
	
	if (ATallowGameMode* Gm = GetWorld()->GetAuthGameMode<ATallowGameMode>())
	{
		Gm->RegisterDroppedItem(SpawnedItem);
	}
	
	return SpawnedItem;
}	

void ATallowCharacter::UpdateHeldItemVisual()
{
	if (!HeldItemMesh) { return; }
	if (CarriedItem == ECarriedItemType::None)
	{
		HeldItemMesh->SetStaticMesh(nullptr);
		HeldItemMesh->SetVisibility(false);
		return;
	}
	
	if (CarriedItem == ECarriedItemType::FilmReel && !ItemRegistry.FindRef(CarriedItem))
	{
		HeldItemMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/ProjectorFinal/SM_FilmReel.SM_FilmReel")));
		HeldItemMesh->SetRelativeTransform(FTransform(FRotator(0,-65,5), FVector(44.5f,-15.f,-17.f), FVector(.7f)));
		HeldItemMesh->SetVisibility(HeldItemMesh->GetStaticMesh() != nullptr);
		return;
	}
	// Checks the carried item and if it has a mesh, set it with correct transform  
	if (const UTallowItemDefinition* ItemDef = ItemRegistry.FindRef(CarriedItem))
	{
		if (ItemDef && ItemDef->HeldMesh)
		{
			HeldItemMesh->SetStaticMesh(ItemDef->HeldMesh);
			HeldItemMesh->SetRelativeTransform(ItemDef->HeldRelativeTransform);
			HeldItemMesh->SetVisibility(true);
		}
		else
		{
			HeldItemMesh->SetStaticMesh(nullptr);
			HeldItemMesh->SetVisibility(false);
		}
	}
}
