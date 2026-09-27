#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowCreatureAnimationComponent.generated.h"

class ATallowCreature;
class ATallowGameMode;
class UAnimSequence;
class USkeletalMesh;

/** Optional playback bridge for the Keeper's authored clips. Character movement owns translation.
 *  This prototype uses single-node playback, so transitions cut between poses; an AnimBP can replace it.
 */
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
	float PatrolSpeed = 65.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float InvestigateSpeed = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Movement", meta=(ClampMin="1"))
	float ChaseSpeed = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0.1", ClampMax="1"))
	float SqueezeWidthScale = .60f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Keeper|Animation", meta=(ClampMin="0.1", ClampMax="1"))
	float SqueezeHeightScale = .92f;

	/** Overrides locomotion until cleared. Used by wall, ceiling, squeeze and emergence controllers. */
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void PlayTraversalAnimation(UAnimSequence* Animation, bool bLoop = true, float PlayRate = 1.f);
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void ClearTraversalAnimation();
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void ResetPlayback();
	UFUNCTION(BlueprintCallable, Category="Keeper|Animation")
	void PlayCaptureAnimation();

private:
	void PlayClip(UAnimSequence* Animation, bool bLoop, float PlayRate);
	UPROPERTY(Transient) TObjectPtr<ATallowCreature> Creature;
	UPROPERTY(Transient) TObjectPtr<ATallowGameMode> GameMode;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActiveClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> TraversalClip;
	bool bActive = false;
	bool bCaptured = false;
	bool bTraversalLoop = true;
	float TraversalRate = 1.f;
	bool bWasChasing = false;
	bool bWasTraversing = false;
	float RevealTimeRemaining = 0.f;
	bool bWasStationarySearch = false;
	bool bNextSearchOver = false;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> StationarySearchClip;
	FVector BaseMeshScale = FVector::OneVector;
};
