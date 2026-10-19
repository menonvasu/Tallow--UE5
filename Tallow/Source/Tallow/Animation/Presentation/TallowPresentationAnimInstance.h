#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TallowPresentationAnimInstance.generated.h"

class UAnimSequence;
struct FTallowPresentationAnimProxy;

/** Visual-only sequence playback. Blends from the last rendered pose, including on interrupted transitions.
 * Never extracts movement or sends gameplay notifies; the existing actors retain those responsibilities. */
UCLASS(Transient)
class TALLOW_API UTallowPresentationAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UTallowPresentationAnimInstance();
	UFUNCTION(BlueprintCallable, Category="Tallow|Animation")
	bool PlayClip(UAnimSequence* Clip, bool bLoop = true, float PlayRate = 1.f, float BlendSeconds = .18f, bool bRestart = false);
	UFUNCTION(BlueprintCallable, Category="Tallow|Animation")
	void StopPlayback();
	UFUNCTION(BlueprintCallable, Category="Tallow|Animation")
	void SetPlaybackPosition(float Seconds);
	UFUNCTION(BlueprintPure, Category="Tallow|Animation")
	UAnimSequence* GetActiveClip() const { return ActiveClip; }
	UFUNCTION(BlueprintPure, Category="Tallow|Animation")
	float GetPlaybackPosition() const { return PlaybackPosition; }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
private:
	friend struct FTallowPresentationAnimProxy;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActiveClip;
	float PlaybackPosition = 0.f;
	float Rate = 1.f;
	float BlendDuration = 0.f;
	float BlendElapsed = 0.f;
	uint32 Generation = 0;
	bool bLooping = true;
	bool bPlaying = false;
};
