#include "TallowCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interactable/IInteractable.h"
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
#include "Tallow.h"


ATallowCharacter::ATallowCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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

}

void ATallowCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
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
	}
	else
	{
		UE_LOG(LogTallow, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void ATallowCharacter::BeginPlay()
{
	Super::BeginPlay();

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

void ATallowCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ATallowCharacter::LookInput(const FInputActionValue& Value)
{
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
	// return early (cannot sprint while cupping)
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped) return;

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
	if (!CandleComponent || !CandleComponent->IsAcquired()) return;
	// R toggles: blow the candle out to hide from the Keeper, or strike a match to relight it
	if (CandleComponent->GetCandleState() == ECandleState::Out) CandleComponent->StartRelighting();
	else CandleComponent->SetCandleState(ECandleState::Out);
}

void ATallowCharacter::DoStillnessStart()
{
	if (IsTiedToChair() || bIsHoldingStill) return;

	StillnessStartTime = GetWorld()->GetTimeSeconds();

	bIsHoldingStill = true;
	OnStill.Broadcast(bIsHoldingStill);

	// Ignore player capsule
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Disable movement
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}

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

	// Re-enable movement
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}

	// Restore sight
	if (CandleComponent)
	{
		SetLightPerception(GetCandleComponent()->GetCandleState());
	}

	GetCharacterMovement()->AvoidanceWeight = 0.0f;
}

bool ATallowCharacter::IsHandOccupied() const
{
	if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped)
	{
		return true;
	}
	return false;
}
