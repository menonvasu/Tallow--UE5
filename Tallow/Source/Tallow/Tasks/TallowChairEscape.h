#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowChairEscape.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class ATallowCharacter;
class UUserWidget;
class UCameraShakeBase;

// escape delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChairEscaped);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRockProgressChanged, int32, CurrentRocks, int32, RequiredRocks);

UCLASS()
class TALLOW_API ATallowChairEscape : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATallowChairEscape();

	UFUNCTION(BlueprintCallable, Category = "Chair")
	void AttemptRock();

	UFUNCTION(BlueprintCallable, Category = "Chair")
	void TiePlayerToChair(ATallowCharacter* InPlayer);

	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	bool IsTimingWindowOpen() const;

	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	float GetNormalizedSwing() const;

	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	int32 GetRockCount() const { return CurrentRockCount; }

	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	int32 GetRequiredRocks() const { return RequiredRocks; }

	/** Normalised swing below which the timing window can open (the back of the swing). */
	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	float GetWindowThreshold() const { return MaxSwingAngle > 0.0f ? WindowAngle / MaxSwingAngle : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Chair|UI")
	bool IsAtRest() const;


protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void TryAutoTiePlayer();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Mesh, Seat and Player attachment pointers */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<USceneComponent> RockingPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<UStaticMeshComponent> ChairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<USceneComponent> SeatAttachPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<USceneComponent> EscapeExitPoint;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<ATallowCharacter> TiedPlayerCharacter;

	FTransform InitialChairRelativeTransform;

	/** Event properties */
	UPROPERTY(BlueprintAssignable, Category = "Chair|Events")
	FOnChairEscaped OnChairEscaped;

	UPROPERTY(BlueprintAssignable, Category = "Chair|Events")
	FOnRockProgressChanged OnRockProgressChanged;

	/** Is tied? */
	bool bIsTied = true;

	/** Rocking rythm parameters */
	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float RockingFrequency = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float MistimedNoiseLoudness = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float RhythmicCreakLoudness = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	int32 RequiredRocks = 3;

	/** Angular velocity and stiffness */
	float AngularVelocity = 0.0f;
	float Stiffness = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float RockImpulse = 90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float Damping = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float CrashForce = 1000.0f;

	/** Tracks success */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair|State")
	int32 CurrentRockCount = 0;

	/** Tracks Swing */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair|State")
	float CurrentSwingAngle = 0.0f; 

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float MaxSwingAngle = 25.0f;

	/** How far back (degrees) the chair must be tipped for a rock to count */
	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float WindowAngle = 3.0f;

	/** Camera Shake */
	UPROPERTY(EditDefaultsOnly, Category = "Chair|Effects")
	TSubclassOf<UCameraShakeBase> FloorSlamCameraShakeClass;

	/** Camera limits */
	float CachedPitchMin = 0.0f;
	float CachedPitchMax = 0.0f;
	float CachedYawMin = 0.0f;
	float CachedYawMax = 0.0f;

	/** Widget properties */
	UPROPERTY(EditDefaultsOnly, Category = "Chair|UI")
	TSubclassOf<UUserWidget> ChairOverlayWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|UI")
	UUserWidget* ActiveOverlayWidget;

};


