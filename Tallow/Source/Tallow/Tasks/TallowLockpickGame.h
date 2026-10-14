#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowLockpickGame.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTensionReleased);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLockpickSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSnap);

/** Per state for a pin */
UENUM(BlueprintType)
enum class EPinState : uint8
{
	Free,
	Binding, 
	Set,
	FalseGate,
	Overset
};

/** Holds editable details on a single pin */
USTRUCT(BlueprintType)
struct FPinChamber
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float TargetHeight = 0.6f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float CurrentHeight = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	EPinState PinState = EPinState::Free;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PinChamber")
	float FalseGateHeight = 0.5f;
};

/** Cylinder/lock mechanism as a whole */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowLockpickGame : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowLockpickGame();
	
	/** Start a session */
	void StartLockpickSession(AActor* InTargetLockable, int32 PinCount = 4);
	
	/** Session over */
	void EndLockpickSession(bool bWasSuccessful);
	
	/** Set torque */
	void SetTensionTorque(float InTorque);
	
	/** Update pick */
	void UpdatePickPosition(float DeltaDepth, float DeltaLift);
	
	/** Evaluate state of pin */
	void EvaluateLockState(float DeltaTime);
	
	/** Generate a new lock */
	void GenerateLock(int32 PinCount);
	
	/** Lock picking events */
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnTensionReleased OnTensionReleased;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnLockpickSet OnLockpickSet;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "LockPick|Events")
	FOnSnap OnSnap;

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
	float PinTargetHeightMax = 0.85f;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|Tunables")
	float PinTargetHeightMin = 0.25f;
	
	/** Current binding unlock level */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	int32 CurrentBindingStep = 0;
	
	/** Active pin */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	int32 ActivePinIndex = 0;
	
	/** Stress variables */
	float OversetStressTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float MaxStressDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Lockpick|Tunables")
	float LockpickSnapLoudness = 1.0f;
	
	/** Current Torque */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float CurrentTorque = 0.0f;
	
	/** Depth of pick */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float PickDepth = 0.0f;
	
	/** Lift level of pick */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Lockpick|State")
	float PickLift = 0.0f;
	
	/** Weak pointer to target actor */
	TWeakObjectPtr<AActor> TargetLockable = nullptr;
};
