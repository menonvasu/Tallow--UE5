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


    void SetRequiredRocks(int32 Rocks);
    void ReleasePlayerQuietly();
    /** 30 Oct: the chair game after the transformation shows nothing and takes no rocks until Mim has gone; then its
     *  meter and key fade in. Ready by default (the opening chair), and again after every release. */
    void SetMinigameReady(bool bReady);
    bool IsMinigameReady() const { return bMinigameReady; }
    /** 0..1: how far the meter has faded in since the game became ready (the widget reads it). */
    float GetOverlayAlpha() const;
    UPROPERTY(EditAnywhere, Category = "Chair|UI") float OverlayFadeSeconds = .9f;
    FTransform GetStartTransform() const { return StartTransform; }
    UStaticMeshComponent* GetChairMesh() const { return ChairMesh; }
    FTransform GetExitTransform() const;
    FTransform GetExitRelativeTransform() const;
    FTransform GetSeatRelativeTransform() const;
    FTransform GetStandingMeshRelativeTransform() const;

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void TryAutoTiePlayer();

	/** After the fall: freeze where it landed and block pawns again once nobody stands inside it. */
	void SettleChair(FTransform Stood);
	bool IsCharacterInsideChair() const;

	/** Tips the fallen chair onto its back (or a side) on the floor, away from where she steps out. */
	void SnapChairToKnockedOverPose(const FTransform& Stood);
	bool IsChairPoseClear() const;
	bool IsChairLyingOnFloor(float& OutUpZ) const;

	float ChairSettleElapsed = 0.f;
	float ChairRestTime = 0.f;

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

	FTimerHandle ChairSettleHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chair")
	TObjectPtr<ATallowCharacter> TiedPlayerCharacter;

	FTransform StartTransform;
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

	/** Yaw of the chair mesh's front relative to the mesh's +X. SM_RestraintChair's front is its +Y. */
	UPROPERTY(EditAnywhere, Category = "Chair|Tuning")
	float MeshFrontYaw = 90.0f;

	/** Tied view: starting pitch, facing the way the chair faces, and how far either side it can turn. */
	UPROPERTY(EditAnywhere, Category = "Chair|Tuning")
	float TiedStartPitch = -38.0f;

	UPROPERTY(EditAnywhere, Category = "Chair|Tuning")
	float TiedYawRange = 70.0f;

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

	bool bMinigameReady = true;
	double OverlayShownAt = -100.0;
	void ShowOverlay(APlayerController* PC);

};


