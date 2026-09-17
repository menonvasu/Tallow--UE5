// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TallowCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A first person character
 */
UCLASS(abstract)
class ATallowCharacter : public ACharacter
{
	GENERATED_BODY()

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;
	
	/** First person mesh */
	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	USkeletalMeshComponent* FirstPersonMesh;

protected:
	/** Jump Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* JumpAction;

	/** Crouch Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* CrouchAction;

	/** Walk Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* WalkAction;

	/** Run Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* RunAction;

	/** Look Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MouseLookAction;

	/** Interaction Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* InteractAction;

	/** Movement Speeds */
	UPROPERTY(EditAnywhere, Category = "Params")
	float RunSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Params")
	float Walkspeed = 250.0f;
	
public:
	ATallowCharacter();

	virtual void Tick(float DeltaTime) override;

protected:

	/** Movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles run start inputs */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoRunStart();

	/** Handles run end inputs */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoRunEnd();

	/** Handles jump start inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoJumpEnd();

	/** Handles crouch start inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoCrouch();

	/** Handles crouch end inputs */
	UFUNCTION(BlueprintCallable, Category= "Input")
	virtual void DoCrouchEnd();

	/** Handles interact */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoInteract();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

