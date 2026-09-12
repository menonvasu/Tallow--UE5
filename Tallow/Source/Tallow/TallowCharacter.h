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

	/** Pawn mesh: first person view */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Crouch Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* CrouchAction;

	/** Walk Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* WalkAction;

	/** Run Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* RunAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MouseLookAction;
	
public:
	ATallowCharacter();

protected:

	/** Movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles crouch start inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCrouch();

	/** Handles crouch end inputs */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCrouchEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

