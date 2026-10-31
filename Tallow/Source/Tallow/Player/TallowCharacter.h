#pragma once

#include "CoreMinimal.h"
#include "Candle/TallowCandleComponent.h"
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
class UCharacterMovementComponent;
class UTallowLockpickGame;
class UTallowItemDefinition;
class UTallowHouseMapComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStill, bool, bIsStill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransformation, int32, NewLevel);
/** She let her held breath go: by choice, or because she could not hold it any longer. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreathReleased, bool, bForced);

UENUM(BlueprintType)
enum class ECarriedItemType : uint8 
{
	None,
	Fuse,
	Crowbar,
	FilmReel
};

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTallowHouseMapComponent> HouseMapComponent;
	
	/** Pointer to hide volume */
	UPROPERTY(VisibleInstanceOnly, Category = "Hide")
	TObjectPtr<class ATallowHideVolume> CurrentHideVolume = nullptr;
	
	/** Spawnable pickups */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Spawns") 
	TSubclassOf<class ATallowFusePickup> FusePickupClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Spawns") 
	TSubclassOf<class ATallowCrowbarPickup> CrowbarPickupClass;
	
	/** Meshes assigned in BP class */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Visuals")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;
	
	/** Registry of items */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Registry")
	TMap<ECarriedItemType, TObjectPtr<UTallowItemDefinition>> ItemRegistry;
	
	/** Lockpick component */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick")
	TObjectPtr<UTallowLockpickGame> LockpickComponent;
	
	/** Transformation counter */
	int32 TransformationLevel = 0;

	/** Bool to check if player holding still */
	bool bIsHoldingStill = false;

	/** Must hold for at least 0.6s to pay the cost */
	double StillnessStartTime = 0.0;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Tuning")
	float MinStillnessHoldDuration = 0.6f; 

	/** How much breath she has left to hold (1 full, 0 she has to let go), and the pause before it comes back. */
	float HeldAir = 1.f;
	float BreathRecovery = 0.f;
	bool bBreathForced = false;
	void UpdateHeldBreath(float DeltaSeconds);
	/** The run that leaves her panting: how long she has sprinted, and until when she pants. */
	void UpdatePanting(float DeltaSeconds);
	float SprintedSeconds = 0.f;
	double PantUntil = -100.0, NextPantNoise = 0.0;
	/** Child-scale gait on the camera: a dip on each footfall and a slight roll from side to side. */
	void UpdateWalkBob(float DeltaSeconds);
	float WalkBobDistance = 0.f;
	float WalkBobAlpha = 0.f;
	float WalkBobRunAlpha = 0.f;

	/** Carried Item */
	ECarriedItemType CarriedItem = ECarriedItemType::None;
	
	/** Drop item raycast distance threshold */
	const float DropRayCastDistance = 220.0f;
	
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* DropAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* TensionAction;

public:
	ATallowCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

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
	
	/** Gets if player cupping flame */
	UFUNCTION(BlueprintCallable, Category = "Player|State")
	bool IsHandOccupied() const;
	
	/** Get if player holding still*/
	UFUNCTION(BlueprintPure, Category = "Player|State")
	bool IsHoldingStill() const { return bIsHoldingStill; }

	/** The "you are seen" signal lives in the world (25 Oct): the hunter writes how hard the Keeper is looking at
	 *  her and when it called out on seeing her; the candle light, the flame and the soundscape answer it. */
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") float KeeperSight = 0.f;
	/** True only while the Keeper has a live, clear look at her this tick. The HUD eye reads this,
	 *  never the chase state: hunting a last-known spot is not being watched. */
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") bool bSeenByKeeper = false;
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") FVector KeeperSeenFrom = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") float SeenCallAt = -100.f;
	/** Where the house draws her next: the flame leans slowly toward it like a draught (zero when nothing to point at). */
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") FVector GuideTarget = FVector::ZeroVector;
	/** How far she still has to walk to what the draught leads to (cm, along the navmesh): the closer, the harder it pulls. */
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") float GuideWalk = 0.f;
	/** Leans the draught harder for a moment (0..1), while Nell first notices it (UTallowNellNotesComponent). */
	UPROPERTY(BlueprintReadOnly, Category = "Player|State") float GuideEmphasis = 0.f;

	/** Holding her breath (F) silences her: the Keeper cannot hear her breathe and her steps make no sound, so in the
	 *  dark she can creep past it. It does nothing for the light. 28 Oct: it needed a limit you can feel. */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathHoldSeconds = 14.f;
	/** When the strain starts to show: the heartbeat, the trembling flame, a small sound she cannot keep in. */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathStrainSeconds = 8.f;
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathRecoverSeconds = 6.f;
	/** Under the Keeper's 140 cm/s "she is silent" test, so creeping with a held breath stays silent. */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float HeldBreathCreepSpeed = 110.f;
	/** The gasp when she cannot hold it any longer is a real sound, heard only close by. */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathGaspLoudness = .3f;
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathGaspRange = 450.f;
	UFUNCTION(BlueprintPure, Category = "Player|Breath") float GetHeldAir() const { return HeldAir; }
	/** Seconds into the current hold (0 when she is breathing). */
	UFUNCTION(BlueprintPure, Category = "Player|Breath") float GetHeldSeconds() const;
	/** 0 until the strain begins, 1 at the moment she has to let go. */
	UFUNCTION(BlueprintPure, Category = "Player|Breath") float GetHeldStrain() const;
	UPROPERTY(BlueprintAssignable, Category = "Player|Event") FOnBreathReleased OnBreathReleased;
	/** 29 Oct: with Mim this close the held breath runs out faster (up to this many times quicker, right on her). */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float BreathCloseDrain = 1.5f;
	/** Out of breath after a run: a few seconds after she stops sprinting she pants, louder than her breathing, and
	 *  the Keeper can hear it close by. Holding her breath (F) stops it. */
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float PantAfterSprintSeconds = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float PantLoudness = .35f;
	UPROPERTY(EditAnywhere, Category = "Player|Breath") float PantRange = 700.f;
	UFUNCTION(BlueprintPure, Category = "Player|Breath") bool IsPanting() const;
	/** How far the Keeper is (cm), written by its brain every tick it thinks; stale after half a second. */
	float KeeperDistance = 1.e6f;
	double KeeperDistanceAt = -100.0;
	/** A round restart, rebuild or checkpoint (30 Oct): forget being seen or heard, so nothing she sees or hears carries
	 *  the last hunt's "it sees you" into the new one (the cold light, the flame's bow, the heartbeat, the out of breath). */
	virtual void ClearKeeperAwareness();

	/** Set and check hide volume */
	void SetCurrentHideVolume(ATallowHideVolume* InVolume) { CurrentHideVolume = InVolume; }
	
	/** Get current hide volume */
	ATallowHideVolume* GetCurrentHideVolume() const { return CurrentHideVolume; }
	
	bool IsHiddenInSpot() const {
		return CurrentHideVolume != nullptr;
	}
	
	/** Gets & Sets Carried Fuses */
	UFUNCTION(BlueprintPure, Category = "Player|Components")
	bool ConsumeFuse()
	{
		if (CarriedItem == ECarriedItemType::Fuse)
		{
			ClearCarriedItem();
			if (PocketItems.FindRef(ECarriedItemType::Fuse) > 0) EquipInventoryItem(ECarriedItemType::Fuse);
			return true;
		}
		return false;
	}
	
	UFUNCTION(BlueprintPure, Category = "Player|Components")
	ECarriedItemType GetCarriedItem() const {  return CarriedItem; }
	
	UFUNCTION(BlueprintPure, Category = "Player|Components")
	bool HasCarriedItem(const ECarriedItemType ItemType) const { return CarriedItem == ItemType; }
	
	UFUNCTION(BlueprintCallable, Category = "Player|Components")
	void SetCarriedItem(const ECarriedItemType NewCarriedItem) { CarriedItem = NewCarriedItem; UpdateHeldItemVisual(); }
	
	UFUNCTION(BlueprintCallable, Category = "Player|Components")
	void ClearCarriedItem() { CarriedItem = ECarriedItemType::None; UpdateHeldItemVisual();	}

	/** One tool in hand; pockets hold the crowbar and up to three fuses. */
	UFUNCTION(BlueprintPure, Category="Player|Inventory")
	int32 GetInventoryCount(ECarriedItemType Item) const;
	UFUNCTION(BlueprintPure, Category="Player|Inventory")
	bool CanCollectItem(ECarriedItemType Item) const;
	UFUNCTION(BlueprintCallable, Category="Player|Inventory")
	bool CollectItem(ECarriedItemType Item);
	UFUNCTION(BlueprintCallable, Category="Player|Inventory")
	bool EquipInventoryItem(ECarriedItemType Item);
	UFUNCTION(BlueprintCallable, Category="Player|Inventory")
	void StowCarriedItem();
	UFUNCTION(BlueprintCallable, Category="Player|Inventory")
	void ResetInventory();
	UFUNCTION(BlueprintPure, Category="Player|Inventory")
	bool CanChangeInventory() const;
	void PrepareForInventory();
	/** Stop interaction/running while retaining held breath and candle shielding. */
	void PrepareForHiding();
	void ToggleHouseMap();

private:
	UPROPERTY(Transient) TMap<ECarriedItemType, int32> PocketItems;
	/** Boom height with the capsule standing, taken at BeginPlay; the crouch eased eye is built from it. */
	float StandingBoomZ = 0.f;
	float LastCapsuleHalfHeight = 0.f;
	void UpdateCrouchEye(float DeltaSeconds);
public:
	/** Crouched eye height below the top of the crouched capsule. The crawl gaps are 68 cm clear, the crouched capsule 60. */
	UPROPERTY(EditAnywhere, Category = "Params")
	float CrouchEyeBelowCapsuleTop = 8.f;
	
	UFUNCTION(BlueprintCallable, Category = "Player|Visuals	")
	void UpdateHeldItemVisual();
	
	UFUNCTION(BlueprintCallable, Category = "Player|Components")
	UTallowLockpickGame* GetLockpickComponent() const { return LockpickComponent; }
	
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
	
	/** Handles drop */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoDropItem();
	
	/** Handles tension */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoTensionStart();
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoTensionEnd();
	
	/** Helper to instantiate an item pickup cleanly on the floor */
	AActor* SpawnItem(TSubclassOf<AActor> ItemPickupClass, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	
	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
};
