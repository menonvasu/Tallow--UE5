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
class UTallowCandleComponent;
class UPointLightComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A first person character
 */
UCLASS(abstract)
class ATallowCharacter : public ACharacter
{
	GENERATED_BODY()

protected:

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;
	
	/** First person mesh */
	UPROPERTY(VisibleAnywhere, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTallowCandleComponent> CandleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Candle|Light")
	TObjectPtr<UPointLightComponent> CandleLightComponent;

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
	
	/** Cupping Action */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* CupAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* RelightAction;

public:
	ATallowCharacter();

	virtual void Tick(float DeltaTime) override;

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Movement Speeds */
	UPROPERTY(EditAnywhere, Category = "Params")
	float RunSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Params")
	float Walkspeed = 250.0f;


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

	/** Handles cupping candle */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCup();

	/** Handles cupping end */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCupEnd();

	/** Handles relighting candle*/
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoRelight();

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

};

