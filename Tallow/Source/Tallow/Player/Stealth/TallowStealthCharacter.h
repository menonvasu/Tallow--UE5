#pragma once

#include "CoreMinimal.h"
#include "Player/TallowCharacter.h"
#include "TallowStealthCharacter.generated.h"

class UAnimSequence;
class UTallowPlayerPresentationComponent;
class UTallowStealthFeedbackWidget;
class UTallowNoiseSubsystem;
struct FMinimalViewInfo;

UENUM(BlueprintType)
enum class ETallowPlayerTraversalMode : uint8
{
    None,
    Mantling,
    Squeezing
};

/** Opt-in player movement and breath behaviour for the isolated hunter review. */
UCLASS()
class TALLOW_API ATallowStealthCharacter : public ATallowCharacter
{
    GENERATED_BODY()
public:
    ATallowStealthCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    UFUNCTION(BlueprintCallable, Category="Tallow|Stealth")
    bool BeginBreathHold();
    UFUNCTION(BlueprintCallable, Category="Tallow|Stealth")
    void EndBreathHold();
    UFUNCTION(BlueprintCallable, Category="Tallow|Stealth")
    void ResetStealthState();
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    bool IsHoldingBreath() const { return bIsHoldingStill; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    float GetBreathRemaining() const { return BreathFraction * BreathCapacitySeconds; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    float GetBreathFraction() const { return BreathFraction; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    float GetBreathStrain() const { return BreathStrain; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    bool IsBracedAgainstSurface() const { return bBracedAgainstSurface; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    float GetBracePoseAlpha() const { return BracePoseAlpha; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    int32 GetExhaustionReleaseCount() const { return ExhaustionReleaseCount; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    bool IsCandleSightRegistered() const { return bCandleSightRegistered; }
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    FText GetStealthPrompt() const;
    UFUNCTION(BlueprintPure, Category="Tallow|Stealth")
    UTallowNoiseSubsystem* GetNoiseSystem() const;

    /** Jump invokes this first. A solid ledge, clear upper route and safe landing are all required. */
    UFUNCTION(BlueprintCallable, Category="Tallow|Traversal")
    bool TryStartMantle();
    /** Crouch and walk toward a real narrow passage. Normal swept movement remains in control. */
    UFUNCTION(BlueprintCallable, Category="Tallow|Traversal")
    bool TryStartSqueeze();
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    bool CanStandHere() const;
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    ETallowPlayerTraversalMode GetStealthTraversalMode() const { return TraversalMode; }
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    float GetStealthTraversalProgress() const { return TraversalProgress; }
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    FVector GetTraversalHandTarget() const { return TraversalHandTarget; }
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    FVector GetTraversalHandSurfaceNormal() const { return TraversalHandSurfaceNormal; }
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    float GetTraversalHandWeight() const;
    UFUNCTION(BlueprintPure, Category="Tallow|Traversal")
    bool IsStandBlocked() const { return bStandBlocked; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth", meta=(ClampMin="3", ClampMax="20"))
    float BreathCapacitySeconds = 9.f;
    /** Set by the hunter each tick so the HUD can say plainly whether holding still is working. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tallow|Stealth")
    bool bSeenByHunter = false;
    /** How sure the Keeper is that it sees you (0..1), written by its controller every tick. */
    float HunterAwareness = 0.f;
    /** Time until which the HUD tells the player the hunter has locked onto their noise. */
    float HunterAlertUntil = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth")
    bool bKeepStoryTransformationEvent = true;
    /** 25 Oct: no on-screen notes ("IT HEARD YOU", "SEEN"). Being seen is felt instead (UpdateAwarenessFeedback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth")
    bool bShowStealthFeedback = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth")
    float HeldLookYawLimit = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth")
    float HeldLookPitchLimit = 24.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Traversal")
    float MaxMantleHeight = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Traversal")
    float SqueezeRadius = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Traversal")
    float SqueezeSpeed = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth|Animation")
    TObjectPtr<UAnimSequence> BraceClip;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Stealth|Animation")
    TObjectPtr<UAnimSequence> ReachClip;

protected:
    virtual void DoAim(float Yaw, float Pitch) override;
    virtual void DoMove(float Right, float Forward) override;
    virtual void DoRunStart() override;
    virtual void DoRunEnd() override;
    virtual void DoJumpStart() override;
    virtual void DoCrouch() override;
    virtual void DoCrouchEnd() override;
    virtual void DoCup() override;
    virtual void DoCupEnd() override;
    virtual void DoRelight() override;
    virtual void DoStillnessStart() override;
    virtual void DoStillnessEnd() override;
    virtual void DoInteractStart() override;
    virtual void DoDropItem() override;

private:
    UFUNCTION() void RefreshCandleSight(ECandleState State);
    UFUNCTION() void HandleCandleReset();
    void ReleaseBreath(bool bExhausted, bool bRestoreCandle = true);
    void UpdateBreath(float DeltaSeconds);
    void UpdatePresentation(float DeltaSeconds);
    void UpdateTraversal(float DeltaSeconds);
    void RestoreMovementSpeed();
    void FinishMantle(bool bCompleted);
    bool CapsuleFits(FVector Centre, float Radius, float HalfHeight) const;
    bool CapsulePathClear(FVector Start, FVector End, float Radius, float HalfHeight) const;
    bool FindSqueezeWalls(FVector Probe, FVector& Left, FVector& Right) const;
    FVector ConstrainEyeToClearSpace(const FVector& DesiredEye) const;

    UPROPERTY(Transient) TObjectPtr<UTallowStealthFeedbackWidget> Feedback;
    UPROPERTY(Transient) TObjectPtr<class UMaterialParameterCollection> LookParams;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Heartbeat;
    float WatchedLook = 0.f, BreathLook = 0.f;
    void UpdateAwarenessFeedback(float DeltaSeconds);
    TWeakObjectPtr<UTallowPlayerPresentationComponent> Presentation;
    FTransform BaseHandsTransform;
    FVector BaseBoomLocation = FVector::ZeroVector;
    FRotator BaseCameraRotation = FRotator::ZeroRotator;
    FRotator HeldViewRotation = FRotator::ZeroRotator;
    float BreathFraction = 1.f, BreathStrain = 0.f, BracePoseAlpha = 0.f;
    float RecoveryDelay = 0.f, ShieldReleaseDelay = 0.f, ExhaustionCueTime = 0.f;
    float NextBreathNoise = 3.6f;
    float IntroductionTime = 0.f, LessonCheckTime = 0.f, CameraCrouchOffset = 0.f;
    float MantleCameraOffset = 0.f, MantleCameraLean = 0.f;
    float DefaultRadius = 30.f, DefaultHalfHeight = 60.f;
    float TraversalProgress = 0.f, MantleElapsed = 0.f, MantleDuration = 1.f;
    float SqueezeTravel = 0.f;
    FVector MantleStart, MantleLift, MantleAcross, MantleEnd;
    FVector SqueezeStart, PreviousSqueezePosition, TraversalHandTarget;
    FVector TraversalHandSurfaceNormal = FVector::UpVector;
    ETallowPlayerTraversalMode TraversalMode = ETallowPlayerTraversalMode::None;
    bool bOwnsCup = false, bManualCup = false, bCrouchInput = false, bSprintInput = false;
    bool bBracedAgainstSurface = false, bOriginalControllerYaw = true;
    bool bCandleSightRegistered = false, bStandBlocked = false, bSeenBraceIntroduction = false;
    int32 ExhaustionReleaseCount = 0;
};
