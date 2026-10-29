#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TallowMimAnimInstance.generated.h"

class UAnimSequence;

/** Shared-phase locomotion with inertialized authored action changes. */
UCLASS(Transient)
class TALLOW_API UTallowMimAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UTallowMimAnimInstance();
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitA;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitB;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ActionClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> FitNeutralClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> FitTallClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> FitUprightClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> FitLowClip;
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    bool HasAdaptiveFitLayer() const { return FitNeutralClip && FitTallClip && FitLowClip; }
    UPROPERTY(Transient, BlueprintReadOnly) bool AdaptiveFitApplied = false;
    UPROPERTY(Transient, BlueprintReadOnly) bool AdaptiveFitWithinLimits = true;
    UPROPERTY(Transient, BlueprintReadOnly) float AdaptiveFitUprightWeight = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) FVector AdaptiveFitWeights = FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly) FVector AdaptiveFitBodyScale = FVector::OneVector;
    UPROPERTY(Transient, BlueprintReadOnly) int32 AdaptiveFitReplants = 0;
    UPROPERTY(Transient, BlueprintReadOnly) float AdaptiveFitMilliseconds = 0.f;
    float GaitTimeA = 0.f, GaitTimeB = 0.f, GaitAlpha = 0.f;
    float ActionTime = 0.f, ActionAlpha = 0.f;
    /** Bumped when the gait pair changes so the proxy can blend from the last evaluated pose. */
    uint32 BaseSerial = 0;
    float BlendAge = 0.f;
    float BlendSeconds = .2f;
    /** Review telemetry from the evaluated pose, in arm01_l/r ... arm05_l/r order. */
    UPROPERTY(Transient, BlueprintReadOnly) TArray<float> ContactWeights;
    UPROPERTY(Transient, BlueprintReadOnly) TArray<bool> ContactPinned;
    UPROPERTY(Transient, BlueprintReadOnly) TArray<FVector> ContactAnchorsWorld;
    UPROPERTY(Transient, BlueprintReadOnly) TArray<FVector> ContactNormalsWorld;
    UPROPERTY(Transient, BlueprintReadOnly) TArray<float> ContactAnchorErrorCm;
    UPROPERTY(Transient, BlueprintReadOnly) int32 ContactReleaseCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly) int32 ContactPlantEventCount = 0;
    UPROPERTY(Transient, BlueprintReadOnly) int32 ContactEvaluationSerial = 0;
    UPROPERTY(Transient, BlueprintReadOnly) FVector ContactEvaluationComponentLocation = FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly) float RootTranslationRemovedCm = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) float RootRotationRemovedDeg = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) FVector HeadLookDirection = FVector::ForwardVector;
    UPROPERTY(Transient, BlueprintReadOnly) FVector HeadDesiredDirection = FVector::ForwardVector;
    UPROPERTY(Transient, BlueprintReadOnly) FVector HeadAuthoredDirection = FVector::ForwardVector;
    UPROPERTY(Transient, BlueprintReadOnly) float HeadLookWeight = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) float Eyeshine = .0175f;
    UFUNCTION(BlueprintCallable)
    void SetLocomotion(UAnimSequence* A, UAnimSequence* B, float Blend, float TimeA, float TimeB);
    UFUNCTION(BlueprintCallable)
    void SetAction(UAnimSequence* Clip, float Time, float Weight);
    void BlendFromCurrentPose();
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativePostEvaluateAnimation() override;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
private:
    float NextFitReferenceLoad = 0.f;
    float NextEyeshineSample = 0.f, EyeshineTarget = .0175f;
    /** Eye glint strength while it stands off watching her in the dark (the resting dark glint is .0245). */
    UPROPERTY(EditAnywhere, Category="Mim|Eyes") float WatchEyeshine = .35f;
    friend struct FTallowMimAnimProxy;
};
