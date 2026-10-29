#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowLockpickGame.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTensionReleased);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPinSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLockpickSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBinding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSnap);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFalseSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOverset);

/** Per state for a pin */
UENUM(BlueprintType)
enum class EPinState : uint8
{
	Free,
	Binding, 
	Set,
	FalseSet,
	Overset
};

/** Helper enum for hint to widget */
UENUM(BlueprintType)
enum class ELockpickHint : uint8 { None, ApplyTension, EaseTension, LowerPick, FindBindingPin };

/** Holds editable details on a single pin */
USTRUCT(BlueprintType)
struct FPinChamber
{
	GENERATED_BODY()
	
public:
	bool bHasFalseGate = false;
	bool bFalseGateCleared = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float TargetHeight = 0.6f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float CurrentHeight = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	EPinState PinState = EPinState::Free;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float FalseGateHeight = 0.5f;
};

class UTallowLockpickWidget;

/** Cylinder/lock mechanism as a whole */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowLockpickGame : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowLockpickGame();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** Start a session */
	void StartLockpickSession(AActor* InTargetLockable, int32 PinCount = 3);
	
	/** Session over */
	void EndLockpickSession(bool bWasSuccessful);
	
	/** Set torque */
	void SetTensionHeld(bool bHeld) { bTensionHeld = bHeld; }
	
	/** Update pick */
	void UpdatePickPosition(float DeltaDepth, float DeltaLift);
	
	/** Evaluate state of pin per tick */
	void EvaluateLockState(float DeltaTime);
	
	/** Generate a new lock */
	void GenerateLock(int32 PinCount);
	
	/** Methods exposed for widget */
	ELockpickHint GetHint() const;
	
	float GetPickDepth() const { return PickDepth; }
	
	float GetPickLift() const { return PickLift; }
	
	float GetTargetPickLift() const { return TargetPickLift; }
	
	float GetCurrentTorque() const { return CurrentTorque; }
	
	float GetMinTorque() const { return MinimumTorque; }
	
	TArray<FPinChamber> GetPinChambers() const { return PinChambers; }
	
	bool GetIsCompleting() const { return bCompleting; }
	
	float GetSetDwellProgress() const
	{
		return SetDwellDuration > 0.0f ? FMath::Clamp(SetDwellTimer / SetDwellDuration, 0.0f, 1.0f) : 0.0f;
	}
	
	int32 GetCurrentBindingPinIndex() const
	{
		return BindingSequence.IsValidIndex(CurrentBindingStep) ? BindingSequence[CurrentBindingStep] : INDEX_NONE;	
	}
	
	bool IsInMinigame() const { return IsComponentTickEnabled(); } 
	
	/** Lock picking events */
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnTensionReleased OnTensionReleased;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnLockpickSet OnLockpickSet;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnSnap OnSnap;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnFalseSet OnFalseSet;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnBinding OnBinding;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnPinSet OnPinSet;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnOverset OnOverset;

protected:
	virtual void BeginPlay() override;
	
	/** No. of pins */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	TArray<FPinChamber> PinChambers = {};
	
	/** Binding order */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	TArray<int32> BindingSequence = {};
	
	/** Current tolerance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	float CurrentTolerance = 0.0f;
	
	/** Pin Height Ranges */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	float PinTargetHeightMax = 0.95f;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	float PinTargetHeightMin = 0.20f;
	
	/** Drop-speed of pin on release */
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float PinSpringDropSpeed = 25.0f; 
	
	/** Current binding unlock level */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	int32 CurrentBindingStep = 0;
	
	/** Active pin */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	int32 ActivePinIndex = 0;
	
	/** Stress variables */
	float OversetStressTimer = 0.0f;
	
	UPROPERTY(EditAnywhere, Category="Lockpick|Tuning") float OversetMargin = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float MaxStressDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float LockpickSnapLoudness = 1.0f;
	
	/** Current Torque */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float CurrentTorque = 0.0f;
	
	/** Min Torque */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float MinimumTorque = 0.15f;
	
	/** Max false set Torque */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float MaxFalseTorque = 0.55f;
	
	/** Depth of pick */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float PickDepth = 0.0f;
	
	/** Pick select threshold */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float PinWidthThreshold = 0.3f;
	
	/** Lift level of pick */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float PickLift = 0.0f;
	
	/** Variables for player input pick movement */
	float TargetPickDepth = 0.0f;
	float TargetPickLift = 0.0f;
	
	/** Variables for player input torque */
	float TargetTorque = 0.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float TorqueRiseRate = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float TorqueFallRate = 0.5f;
	bool bTensionHeld = false;
	bool bTensionResetDone = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float PickMoveSpeed = 7.0f;
	
	/** Binding Resistance */
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float BindingResistanceMin = 0.75f;
	
	/** Binding Resistance */
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float FalseGateChance = 0.1f;
	
	/** Dwell timers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lockpick|Tunables")
	float SetDwellDuration = 0.06f;

	float SetDwellTimer = 0.0f;
	
	/** Timers for hint */
	float TimeSinceProgress = 0.0f;
		
	float HintDelay = 5.0f;
	
	/** Weak pointer to target actor */
	TWeakObjectPtr<AActor> TargetLockable = nullptr;
	
	/** Completion */
	UPROPERTY(EditAnywhere, Category = "Lockpick|Tunables")
	float UnlockDelay = 0.6f;
	bool bCompleting = false;
	double SessionStartTime = 0.0;
	FTimerHandle CompleteTimerHandle;
	UFUNCTION()
	void FinishSuccess() { EndLockpickSession(true); }
	
	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|UI")
	TSubclassOf<UTallowLockpickWidget> LockpickWidgetClass;
	
	UPROPERTY(Transient)
	TObjectPtr<UTallowLockpickWidget> ActiveWidgetInstance;

private:
	/** Smoothes raw player inputs towards physical positions */
	void UpdateSmoothing(float DeltaTime);
	
	/** Resolves spring return on unsupported pins and handles tension drop collapse */
	void SimulatePinGravity(float DeltaTime);
	
	/** Evaluates shear line alignment, false gates, and oversets in active chamber */
	void EvaluateActiveChamber(float DeltaTime);
	
	/** Accumulates stress on jammed pins and executes the pick snap */
	void EvaluatePickStress(float DeltaTime);
};
