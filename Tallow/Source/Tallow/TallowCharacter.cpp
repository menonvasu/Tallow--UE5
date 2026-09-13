// Copyright Epic Games, Inc. All Rights Reserved.

#include "TallowCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "IInteractable.h"
#include "Tallow.h"

ATallowCharacter::ATallowCharacter()
{
	// Set size for collision capsule
	constexpr float PlayerHalfHeight = 60.0f;
	GetCapsuleComponent()->InitCapsuleSize(30.f, PlayerHalfHeight);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -PlayerHalfHeight));

	// Create spring arm
	USpringArmComponent* CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Glide camera smoothly
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->CameraLagSpeed = 15.0f;

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 0.f), FRotator(0.0f, 0.0f, -90.0f));
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

		// Interact
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATallowCharacter::DoInteract);
	}
	else
	{
		UE_LOG(LogTallow, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
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
	// pass run speed to maxwalkspeed
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
}

void ATallowCharacter::DoRunEnd()
{
	// pass walk speed to maxwalkspeed
	GetCharacterMovement()->MaxWalkSpeed = Walkspeed;
}

void ATallowCharacter::DoJumpStart()
{
	// pass Jump to the character
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

void ATallowCharacter::DoInteract()
{
	FHitResult HitResult;
	FVector StartLocation = FirstPersonCameraComponent->GetComponentLocation();
	FVector EndLocation = StartLocation + (FirstPersonCameraComponent->GetForwardVector() * 200.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // ignore self

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility,
		QueryParams
	);

	if (bHit && HitResult.GetActor()->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_OnInteract(HitResult.GetActor(), this);
	}
}

void ATallowCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}