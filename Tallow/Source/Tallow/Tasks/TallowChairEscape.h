#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowChairEscape.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class ATallowCharacter;
class UUserWidget;

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

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Mesh, Seat and Player attachment pointers */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<UStaticMeshComponent> ChairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<USceneComponent> SeatAttachPoint;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<ATallowCharacter> TiedPlayerCharacter;

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
	float RockImpulse = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float Damping = 2.0f;

	/** Tracks success */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair|State")
	int32 CurrentRockCount = 0;

	/** Tracks Swing */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair|State")
	float CurrentSwingAngle = 0.0f; 

	UPROPERTY(EditDefaultsOnly, Category = "Chair|Tuning")
	float MaxSwingAngle = 25.0f;


	/** Widget properties */
	UPROPERTY(EditDefaultsOnly, Category = "Chair|UI")
	TSubclassOf<UUserWidget> ChairOverlayWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Chair|UI")
	UUserWidget* ActiveOverlayWidget;

};


