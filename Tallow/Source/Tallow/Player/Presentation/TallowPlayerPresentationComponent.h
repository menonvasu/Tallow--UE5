#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/Presentation/TallowFirstPersonProjectionState.h"
#include "TallowPlayerPresentationComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class USceneComponent;

/** Explicit opt-in visual adapter for the generated partial player. No gameplay state or timers. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowPlayerPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTallowPlayerPresentationComponent();

	/** Call after the camera and actual held candle/flame primitives are registered.
	 * All references must belong to this component's owner; no name-based discovery or asset loading. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	bool EnablePresentation(UCameraComponent* Camera, USkeletalMesh* PartialMesh,
		UPrimitiveComponent* HeldCandle, UPrimitiveComponent* VisibleFlame);

	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	void DisablePresentation();

	/** Freeze the requested clip at an explicit time. Does not play notifies, advance gameplay or run a timer.
	 * Caller may sample successive times to animate, or use 17/30 seconds for exported frame18. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	bool SetPresentationPose(UAnimSequence* Clip, float TimeSeconds = 0.566667f);

	/** Visual playback only; shared native instance crossfades poses, never emits gameplay notifies. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	bool PlayPresentationClip(UAnimSequence* Clip, bool bLoop = true, float PlayRate = 1.f, float BlendSeconds = 0.18f, bool bRestart = false);

	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	void StopPresentationPlayback();

	/** Scales every clip's rate: the hands go still while she holds her breath (UTallowBreathFeedbackComponent). */
	UPROPERTY(Transient, BlueprintReadWrite, Category="Tallow|Player Presentation")
	float PlaybackRateScale = 1.f;

	/** Sample the calibrated Hold pose first. Place prop in camera space, then retain that transform on the hand. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	bool AttachHeldCandleToHand(FName HandBone, const FTransform& CandleRelativeToCamera);

	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	void RestoreHeldCandleAttachment();

	UFUNCTION(BlueprintCallable, Category="Tallow|Player Presentation")
	bool UpdateHandsTransform(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category="Tallow|Player Presentation")
	bool IsPresentationEnabled() const { return bPresentationEnabled; }

	UFUNCTION(BlueprintPure, Category="Tallow|Player Presentation")
	USkeletalMeshComponent* GetPresentationHands() const { return Hands.Get(); }

	/** Camera-relative centimetres. Verified imported asset faces -Y, so yaw +90 aligns it with camera +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Player Presentation")
	FTransform HandsRelativeTransform = FTransform(FRotator(0.f, 90.f, 0.f), FVector(20.f, 0.f, -110.f));

	/** Applies on next EnablePresentation. World camera FieldOfView remains untouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Player Presentation", meta=(ClampMin="5", ClampMax="170"))
	float PresentationFieldOfView = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Player Presentation", meta=(ClampMin="0.001", ClampMax="1"))
	float PresentationScale = 0.6f;

	/** Imported partial bounds end below animated wrists. Conservative culling envelope for these five clips.
	 * Applies only to this owned, shadowless mesh on next EnablePresentation; does not scale geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Player Presentation", meta=(ClampMin="1", ClampMax="10"))
	float HandsBoundsScale = 3.f;

	virtual void Deactivate() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Hands;
	TWeakObjectPtr<UCameraComponent> PresentationCamera;
	TWeakObjectPtr<UPrimitiveComponent> PresentationCandle;
	TWeakObjectPtr<USceneComponent> OriginalCandleParent;
	FName OriginalCandleSocket;
	FTransform OriginalCandleRelative;
	bool bCandleAttachmentOwned = false;
	FTallowFirstPersonProjectionState ProjectionState;
	bool bPresentationEnabled = false;
};
