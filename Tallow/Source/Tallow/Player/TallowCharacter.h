#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TallowCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
struct FInputActionValue;
class UTallowCandleComponent;
class UTallowCandleFlameComponent;
class UTallowFootstepNoiseComponent;
class UPointLightComponent;
class ATallowChairEscape;
class UAIPerceptionStimuliSourceComponent;
class UTallowInteractionComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStill, bool, bIsStill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransformation, int32, NewLevel);
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

	/** Camera boom */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	/** First person mesh */
	UPROPERTY(VisibleAnywhere, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTallowCandleComponent> CandleComponent;

	/** Flame flicker and guttering on the candle's light */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Candle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTallowCandleFlameComponent> CandleFlame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Candle|Light")
	TObjectPtr<UPointLightComponent> CandleLightComponent;

	/** Reference to track if player is in chair */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<ATallowChairEscape> CurrentTiedChair;

	/** Emits movement and landing noise events for the AI hearing system. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTallowFootstepNoiseComponent> FootstepNoiseComponent;

	/** Stimulus component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perception")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> StimulusSourceComponent;
	
	/** Interaction component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTallowInteractionComponent> InteractionComponent;
	
	/** Transformation counter */
	int32 TransformationLevel = 0;

	/** Bool to check if player holding still */
	bool bIsHoldingStill = false;

	/** Must hold for at least 0.6s to pay the cost */
	double StillnessStartTime = 0.0;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Tuning")
	float MinStillnessHoldDuration = 0.6f; 

	/** Holds current fuse */
	bool bHasFuse = false;
	
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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* StillnessAction;

public:
	ATallowCharacter();

	virtual void BeginPlay() override;

	/** Sets the AI to only perceive player on light */
	UFUNCTION()
	void SetLightPerception(ECandleState NewState);

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Movement Speeds */
	UPROPERTY(EditAnywhere, Category = "Params")
	float RunSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Params")
	float Walkspeed = 250.0f;

	/** Sets player tied to chair */
	void SetTiedChair(ATallowChairEscape* InChair) { CurrentTiedChair = InChair; }

	/** Get if player tied to chair */
	UFUNCTION(BlueprintPure, Category = "Player|State")
	bool IsTiedToChair() const { return CurrentTiedChair != nullptr; }

	/** Gets candle component */
	UFUNCTION(BlueprintCallable, Category = "Components")
	UTallowCandleComponent* GetCandleComponent() const { return CandleComponent; }

	/** Get if player holding still*/
	UFUNCTION(BlueprintPure, Category = "Player|State")
	bool IsHoldingStill() const { return bIsHoldingStill; }

	/** Pointer to hide volume */
	UPROPERTY(VisibleInstanceOnly, Category = "Hide")
	TObjectPtr<class ATallowHideVolume> CurrentHideVolume = nullptr;

	/** Set and check hide volume */
	void SetCurrentHideVolume(ATallowHideVolume* InVolume) { CurrentHideVolume = InVolume; }
	
	bool IsHiddenInSpot() const {
		return CurrentHideVolume != nullptr;
	}
	
	/** Gets & Sets Carried Fuses */
	UFUNCTION(BlueprintPure, Category = "Player|Components")
	bool ConsumeFuse()
	{
		if (bHasFuse)
		{
			bHasFuse = false;
			return true;
		}
		return false;
	}
	
	UFUNCTION(BlueprintPure, Category = "Player|Components")
	bool HasFuse() const { return bHasFuse; }
	
	UFUNCTION(BlueprintCallable, Category = "Player|Components")
	void GiveFuse()  { bHasFuse = true;  }
	
	/** Player Events */
	UPROPERTY(BlueprintAssignable, Category = "Player|Event")
	FOnStill OnStill;
	
	UPROPERTY(BlueprintCallable, Category = "Player|Event")
	FOnTransformation OnTransformation;
	
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
	
	/** Handles cupping candle */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCup();

	/** Handles cupping end */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoCupEnd();

	/** Handles relighting candle */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoRelight();

	/** Handles stillness mechanic */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoStillnessStart();

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoStillnessEnd();

	/** Handles interact */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoInteractStart();
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoInteractEnd();
	
	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
};
