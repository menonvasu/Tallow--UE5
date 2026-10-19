#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowCreatureAnimationComponent.generated.h"

class ATallowCreature;
class ATallowGameMode;
class UAnimSequence;
class USkeletalMesh;

/** Optional playback bridge for the Keeper's authored clips. Character movement owns translation.
 *  The presentation rig blends authored poses; legacy rigs retain single-node playback. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowCreatureAnimationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTallowCreatureAnimationComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	bool bEnableAuthoredPlayback = true;
	/** Opt-in for the new rig in the integrated map. Legacy maps keep their original playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	bool bUsePresentationRig = false;
	/** V07 locomotion and transition clips. Legacy playback ignores these. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> HostWalkAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> InvestigationWalkAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> AwareIdleAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> ResetAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> WallEnterAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> WallExitAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> CeilingEnterAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation") TObjectPtr<UAnimSequence> CeilingExitAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation", meta=(ClampMin="1")) float HostReferenceSpeed = 65.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation", meta=(ClampMin="1")) float InvestigationReferenceSpeed = 36.666667f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation", meta=(ClampMin="1")) float SqueezeReferenceSpeed = 5.555556f;
	/** Upper cadence for planted modern locomotion; movement is limited to its matching stride speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Presentation", meta=(ClampMin="0.1", ClampMax="4")) float PresentationMaxLocomotionRate = 2.f;
	/** Optional stationary transition hook. Does not pause or move the traversal controller.
	 * Call only during an owner-controlled hold; movement cancels the visual transition. */
	UFUNCTION(BlueprintCallable, Category="Keeper|Presentation") bool PlaySurfaceTransition(bool bCeiling, bool bEntering);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<USkeletalMesh> KeeperMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> ChaseAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> InvestigateAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> CaptureAnimation;
	/** Jumpscare variants: dropping from above when caught on the ceiling, hands-first when the player is hiding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> CeilingCaptureAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> GrabCaptureAnimation;
	/** Seconds into each jumpscare where it strikes the lens; sound, light and camera hit on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	float CaptureStrikeTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	float CeilingCaptureStrikeTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	float GrabCaptureStrikeTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> WallAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> CeilingAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> SqueezeAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> RevealAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> SneakAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> SearchUnderAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	TObjectPtr<UAnimSequence> SearchOverAnimation;
	UFUNCTION(BlueprintPure, Category="Keeper|Animation")
	float GetCaptureDuration() const;
	UFUNCTION(BlueprintPure, Category="Keeper|Animation")
	float GetCaptureStrikeTime() const;

	/** Speeds represented by the in-place cycles, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="1"))
	float WalkReferenceSpeed = 40.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="1"))
	float ChaseReferenceSpeed = 211.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="1"))
	float WallReferenceSpeed = 40.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="1"))
	float CeilingReferenceSpeed = 7.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="1"))
	float SneakReferenceSpeed = 7.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float PatrolSpeed = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float InvestigateSpeed = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float ChaseSpeed = 400.f;   // faster than the player's walk (250), slower than their sprint (450)
	/** Turn rates in degrees per second. The host turns slowly; the chase turns faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float PatrolTurnRate = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float ChaseTurnRate = 320.f;
	/** Mesh placement on the capsule. The v6 export arrives at true size (feet at the origin, facing -Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	FVector KeeperMeshOffset = FVector(0.f, 0.f, -96.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	float KeeperMeshYaw = -90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0.01"))
	float KeeperMeshScale = 43.f;   // original Keeper (imported at 5.6 cm) to about 2.4 m
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0.1", ClampMax="1"))
	float SqueezeWidthScale = .60f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0.1", ClampMax="1"))
	float SqueezeHeightScale = .92f;
	/** Vertical drift (cm) while the unaware host floats with its legs tucked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation")
	float HostBobHeight = 6.f;
	/** Seconds the legs stay out after he stops hunting, before tucking back into the floating host. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0"))
	float LegsOutHold = 10.f;

	/** Overrides locomotion until cleared. Used by wall, ceiling, squeeze and emergence controllers. */
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void PlayTraversalAnimation(UAnimSequence* Animation, bool bLoop = true, float PlayRate = 1.f);
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void ClearTraversalAnimation();
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void ResetPlayback();
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void PlayCaptureAnimation();
	/** The game mode asks for the duration before it broadcasts the capture, so the variant is chosen there once. */
	UAnimSequence* ChooseCaptureAnimation() const;
	mutable UAnimSequence* ChosenCapture = nullptr;
	mutable UAnimSequence* LastCapture = nullptr;

private:
	void PlayClip(UAnimSequence* Animation, bool bLoop, float PlayRate);
	void TickPresentation(float DeltaTime);
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> PresentationTransition;
	float PresentationTransitionRemaining = 0.f;
	bool bPresentationFolded = true;
	UPROPERTY(Transient) TObjectPtr<ATallowCreature> Creature;
	UPROPERTY(Transient) TObjectPtr<ATallowGameMode> GameMode;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActiveClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> TraversalClip;
	bool bActive = false;
	bool bCaptured = false;
	bool bTraversalLoop = true;
	float TraversalRate = 1.f;
	bool bWasChasing = false;
	bool bLegsOut = false;
	float HostTime = 0.f;
	float LegsOutUntil = -1.f;
	bool bWasTraversing = false;
	float RevealTimeRemaining = 0.f;
	bool bWasStationarySearch = false;
	bool bNextSearchOver = false;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> StationarySearchClip;
	FVector BaseMeshScale = FVector::OneVector;
};
