#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowHunterMotionComponent.generated.h"

class ATallowHunterCreature;
class UAnimSequence;
class USkeletalMesh;
class UPrimitiveComponent;

struct FTallowHunterHand
{
    FName Upper, Lower, Hand;
    FVector Target = FVector::ZeroVector;
    FVector Normal = FVector::UpVector;
    FVector Anchor = FVector::ZeroVector;
    FVector SwingFrom = FVector::ZeroVector;
    FVector SwingTo = FVector::ZeroVector;
    FVector SwingNormal = FVector::UpVector;
    FVector TipLocal = FVector::ZeroVector;
    FVector ContactLocal = FVector::ZeroVector;
    FVector NormalLocal = FVector::UpVector;
    FVector PalmLocal = FVector::UpVector;
    FVector Pole = FVector::ZeroVector;
    FVector ElbowPole = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FQuat SwingRotation = FQuat::Identity;
    FQuat LandRotation = FQuat::Identity;
    FQuat ContactRotationLocal = FQuat::Identity;
    TWeakObjectPtr<UPrimitiveComponent> SurfaceComponent;
    float Weight = 0.f;
    float SwingStarted = -1.f;
    float SwingDuration = .3f;
    float LandedAt = -1.f;
    float Reach = 100.f;
    float Grip = 0.f;
    bool bPlanted = false;
    bool bWall = false;
    bool bReaching = false;
    bool bAttackReach = false;
    bool bFolding = false;
    FName LastReleaseReason;
};

/** One part of the body found inside the world (the lab's clip audit). */
USTRUCT(BlueprintType)
struct FTallowClipHit
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Part;
    UPROPERTY(BlueprintReadOnly) FString Against;
    /** How deep it is in, cm. */
    UPROPERTY(BlueprintReadOnly) float Depth = 0.f;
};

/** World-space hand plants and clearance-driven posture for the preview rig. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowHunterMotionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTallowHunterMotionComponent();
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
    void InitializeMesh();
    UFUNCTION(BlueprintCallable, Category="Hunter") void ResetContacts();
    UFUNCTION(BlueprintPure, Category="Hunter") TArray<float> GetContactErrors() const;
    UFUNCTION(BlueprintPure, Category="Hunter") TArray<float> GetContactTipErrors() const;
    UFUNCTION(BlueprintPure, Category="Hunter") TArray<FVector> GetPlantedContactPoints() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Review") TArray<FString> GetContactStates() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Review") TArray<FString> GetSkinVertexInfluences(int32 VertexIndex) const;
    UFUNCTION(BlueprintPure, Category="Hunter|Review") FString GetSkinVertexMaterial(int32 VertexIndex) const;
    /** Review-only snapshot. Includes skinning/morphs, flushes rendering; never call every frame. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") TArray<FVector> GetSkinnedSurfaceWorld(int32 VertexStride = 1) const;
    /** Bone-skinned structural bounds for plane checks without a render-device readback. */
    UFUNCTION(BlueprintPure, Category="Hunter|Review") FBox GetPosedBodyWorldBounds(FName BoneName = NAME_None);
    void LookAt(const FVector& Point, float Duration);
    /** Eye contact. While it holds, nothing else may aim the head, and the head turns fast. */
    UFUNCTION(BlueprintCallable, Category="Hunter")
    void LockGaze(const FVector& Point, float Duration);
    bool IsGazeLocked() const;
    UFUNCTION(BlueprintCallable, Category="Hunter") void SetReachFocus(const FVector& Point, float Duration);
    UFUNCTION(BlueprintPure, Category="Hunter") bool HasBraceAgainstActor(AActor* Actor) const;
    int32 CountContactsOnSurface(const FVector& Normal) const;
    float GetSupportStrength() const;
    bool HasInitialized() const { return bInitialized; }
    FVector GetEyeLocation() const;
    FVector GetLookDirection() const { return LookDirection; }
    const TArray<FTallowHunterHand>& GetHands() const { return Hands; }
    float GetBodyOffset() const { return BodyOffset; }
    /** Slides the body out past its cover for a beat, holding eye contact with the focus. */
    UFUNCTION(BlueprintCallable, Category="Hunter") void BeginPeek(const FVector& Focus, float Seconds);
    float GetPeekAlpha() const { return PeekAlpha; }
    float GetIdleTime() const { return IdleTime; }
    FQuat GetNeckWorldRotation() const { return NeckWorldRotation; }
    /** World-axis rotations for DEF_body then DEF_spine_1..4. Together they bend the whole torso. */
    const FQuat& GetSpineWorldRotation(int32 Index) const { return SpineWorld[FMath::Clamp(Index, 0, 4)]; }
    float GetHunch() const { return Hunch; }
    FVector GetBodyTranslation() const { return BodyTranslation; }
    FQuat GetBodyWorldRotation() const { return BodyWorldRotation; }
    float GetSqueezeAmount() const { return SqueezeAmount; }
    bool HasNarrowPortal() const { return bNarrowPortal; }
    FVector GetPortalCenter() const { return PortalCenter; }
    FVector GetPortalForward() const { return PortalForward; }
    FVector GetPortalRight() const { return PortalRight; }
    float GetPortalHalfWidth() const { return PortalHalfWidth; }
    float GetAttackAlpha() const { return AttackAlpha; }
    UFUNCTION(BlueprintPure, Category="Hunter|Balance") FVector GetEstimatedMassCenter() const { return EstimatedMassCenter; }
    UFUNCTION(BlueprintPure, Category="Hunter|Balance") float GetSupportMargin() const { return SupportMargin; }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float CreatureScale = 2.f;
    /** -1 lets the hunter choose. 0 forces the sat-up pose, 1 forces the low one with the front arms as legs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float PostureOverride = -1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float LowAlpha = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 PlantedHands = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 WallBraces = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 TotalPlants = 0;
    /** Its steps sound when a hand or foot actually lands, where it lands (25 Oct: the distance timer played
     *  them far faster than they look, and read as clicking). */
    void PlayPlant(const FVector& Where, bool bWall, float Speed);
    UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> PlantSounds;
    UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> DragSounds;
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> PlantAttenuation;
    float LastPlantSound = -1.f, LastDragSound = -1.f;
    bool bSilencedDistanceSteps = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float HeadClearance = 1000.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") FString ActiveMotion;
    /** Plays any imported MIM clip by name, subject to the current action priority. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Animation") bool PlayMimAction(FName Name, int32 Priority = 2, bool bLoop = false);
    UFUNCTION(BlueprintPure, Category="Hunter|Animation") FName GetMimActionName() const { return MimActionName; }
    UFUNCTION(BlueprintPure, Category="Hunter|Animation") float GetMimActionAge() const { return MimActionAge; }
    UFUNCTION(BlueprintPure, Category="Hunter|Review") FName GetMimSearchActionForReview(const FVector& Focus) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Animation") FString MimActionReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Animation") int32 MimActionStarts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Animation") float MimActionPlaybackTime = 0.f;
    bool UsesMim() const { return bUsingMim; }
    void BeginMimCapture();
    float GetMimCaptureDuration() const { return MimCaptureDuration; }
    float GetMimCaptureStrikeTime() const { return MimCaptureStrikeTime; }
    UFUNCTION(BlueprintPure, Category="Hunter|Capture") bool ShouldKeepMimCaptureRoot() const
    { return bUsingMim && MimCaptureName == TEXT("MIM_Cap_Grab_FromCeiling"); }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float SmoothedSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float DoorwayWidth = 1000.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 ReachingHands = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") bool bUsingClothRig = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Balance") int32 SupportingHands = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Balance") int32 DeferredLifts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Balance") float SupportMargin = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Balance") float BalanceCorrection = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Balance") float ReachHeightCorrection = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Cloth") int32 CollisionAdjustedHemBones = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Cloth") float MaxHemContactOffset = 0.f;
private:
    UPROPERTY() TObjectPtr<ATallowHunterCreature> Creature;
    UPROPERTY() TObjectPtr<USkeletalMesh> KeeperMesh;
    UPROPERTY() TMap<FName, TObjectPtr<UAnimSequence>> Clips;
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> MimGaitClips;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UAnimSequence>> MimActionClips;
    TArray<FTallowHunterHand> Hands;
    FVector PreviousLocation = FVector::ZeroVector;
    FVector PreviousUp = FVector::UpVector;
    FVector PreviousVelocity = FVector::ZeroVector;
    FVector SmoothedVelocity = FVector::ZeroVector;
    FVector SmoothedAcceleration = FVector::ZeroVector;
    FVector LookTarget = FVector::ZeroVector;
    FVector ReachFocus = FVector::ZeroVector;
    FVector LookDirection = FVector::ForwardVector;
    FVector BodyTranslation = FVector::ZeroVector;
    FVector BodyVelocity = FVector::ZeroVector;
    FVector BodyAngles = FVector::ZeroVector;
    FVector BodyAngularVelocity = FVector::ZeroVector;
    FVector EstimatedMassCenter = FVector::ZeroVector;
    FVector PendingSupportShift = FVector::ZeroVector;
    FVector PortalCenter = FVector::ZeroVector;
    FVector PortalForward = FVector::ForwardVector;
    FVector PortalRight = FVector::RightVector;
    FQuat BodyWorldRotation = FQuat::Identity;
    FQuat NeckWorldRotation = FQuat::Identity;
    FQuat SpineWorld[5] = {FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity};
    float Hunch = 0.f, SpineYaw = 0.f, DoorLean = 0.f, IdleTime = 0.f;
    FVector FlowAngles = FVector::ZeroVector, FlowVelocity = FVector::ZeroVector;
    FVector PeekFocus = FVector::ZeroVector;
    float NextRearAt = 6.f, RearUntil = 0.f;
    float HeldLowTarget = 1.f, PostureHoldUntil = 0.f;
    float PeekUntil = 0.f, PeekAlpha = 0.f, PeekSide = 1.f, StanceDrop = 0.f, SurgePhase = 0.f;
    FName HeadBone;
    float LookUntil = 0.f, GazeLockUntil = 0.f;
    float ReachUntil = 0.f;
    float BodyOffset = 0.f;
    float NextPlant = 0.f;
    float LastClipChange = -1.f;
    float PreviousYaw = 0.f;
    float TurnVelocity = 0.f;
    float SqueezeAmount = 0.f;
    float SupportStrength = 0.f;
    float AttackAlpha = 0.f;
    float SmoothedMaxSpeed = 65.f;
    float DistanceSincePlant = 0.f;
    float PortalHalfWidth = 1000.f;
    int32 GaitIndex = 0;
    /** A predator's rhythm off the chase ("it creeps, looks, dips and turns, it's heavy, and sometimes it moves
     *  faster"): moving, a stop to look with a dip, or a short burst. */
    enum class ERhythm : uint8 { Move, Pause, Burst };
    ERhythm Rhythm = ERhythm::Move;
    float RhythmUntil = 0.f;
    FVector PauseLook = FVector::ZeroVector;
    void UpdateRhythm(float Now, uint8 Mode);
public:
    /** Stop, dip and look at a point for a beat (at a doorway, before a reveal). */
    void PauseAndLook(const FVector& Where, float Seconds);
private:
    /** Where its route goes next, a couple of corners ahead: the head looks there before the body turns. */
    FVector PathAhead = FVector::ZeroVector;
    float PathAheadValidUntil = 0.f;
    float RhythmScale() const { return Rhythm == ERhythm::Pause ? .05f : Rhythm == ERhythm::Burst ? 1.45f : 1.f; }
    int32 PoseWarmupFrames = 0;
    bool bInitialized = false;
    bool bProcedural = false;
    bool bUsingMim = false;
    float MimGaitPhase = 0.f;
    bool bMimWasMoving = false;
    float MimMeshBottom = 0.f;
    FVector MimFitScale = FVector::OneVector;
    FVector MimFitClearance = FVector(800.f, 800.f, 350.f);
    FVector MimFitPoseClearance = FVector(800.f, 800.f, 350.f);
    float MimFitCompression = 0.f;
    bool bMimFitPassable = true;
    bool bMimTallDoorFit = false;
    FVector MimPosedFitSize = FVector::ZeroVector;
    FVector MimFitAxisWeights = FVector::ZeroVector;
    FVector MimFitAxisTargetScale = FVector::OneVector;
    FVector MimFitLatchedClearance = FVector(800.f, 800.f, 350.f);
    FVector MimFitReleaseAt = FVector::ZeroVector;
    FVector MimFitFootprintClearance = FVector(800.f, 800.f, 350.f);
    FVector MimFitMassClearance = FVector(800.f, 800.f, 350.f);
    FBox MimFitMassBounds = FBox(ForceInit);
    FVector MimMassWidthProbe = FVector::ZeroVector, MimMassHeightProbe = FVector::ZeroVector;
    FName MimMassWidthLeft = NAME_None, MimMassWidthRight = NAME_None, MimMassRoof = NAME_None;
    FVector MimFitFootprintAt = FVector::ZeroVector;
    float MimNextFootprintSample = 0.f;
    float MimFitBlockedSince = -1.f;
    float MimRecoveryStepSince = -1.f, MimRecoveryStepAt = -100.f;
    FString MimFitBlockReason;
    struct FMimFitSkinGroup
    {
        int32 Bones[4] = {0, 0, 0, 0};
        float Weights[4] = {0.f, 0.f, 0.f, 0.f};
        TArray<FVector3f> Positions;
        TArray<FVector3f> BodyPositions;
        bool bMass = false;
    };
    TArray<FMimFitSkinGroup> MimFitSkinGroups;
    float MimFitMeasureMilliseconds = 0.f;
    FBox MeasureMimPoseBounds();
    FName MimActionName;
    float MimActionAge = 0.f;
    int32 MimActionPriority = 0;
    int32 MimLastEventCount = -1;
    bool bMimActionLoop = false;
    bool bMimWasMound = false;
    FName MimCaptureName;
    float MimCaptureDuration = 0.f, MimCaptureStrikeTime = 0.f;
    FName MimPreviousRouteKind, MimPreviousDecision, MimLastMovingGait = TEXT("Walk");
    FString MimPreviousTraversalPhase;
    FVector MimPreviousRouteNormal = FVector::UpVector;
    FVector MimPreviousRoutingLocation = FVector::ZeroVector, MimPreviousSearchFocus = FVector::ZeroVector;
    float MimPreviousRoutingYaw = 0.f, MimPreviousSpeed = 0.f, MimStationaryAge = 0.f;
    float MimNextContextAt = 0.f, MimNextIdleAt = 0.f, MimNextTurnAt = 0.f, MimTurnAccum = 0.f;
    float MimDoorWaitStarted = -1.f, MimStairUntil = 0.f, MimTransferAge = 0.f;
    float MimActionTimeOverride = -1.f;
    float MimSurfacePhase = 0.f;
    int32 MimPreviousHuntMode = INDEX_NONE, MimPreviousIntent = INDEX_NONE, MimIdleChoice = 0;
    bool bMimRoutingStarted = false, bMimWasTraversing = false, bMimWasSqueezing = false;
    bool bMimRoutingMoving = false;
    bool bMimWasWaitingForDoor = false, bMimWasFalling = false, bMimWasTransferring = false;
    bool bMimActionTracksTransfer = false, bMimStairsUp = true;
    TMap<FName, float> MimActionCooldowns;
    /** The box stand-in, used by the behavior lab when explicitly requested. */
    bool bProxy = false;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> ProxyBody;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> ProxyNose;
    void InitializeProxy();
    bool InitializeMimMesh();
    void UpdateMimLocomotion(float DeltaSeconds, float Speed);
    void UpdateMimActions(float DeltaSeconds, float Speed);
    bool RequestMimAction(FName Name, int32 Priority, float Cooldown, const FString& Reason, bool bLoop = false);
    FName SelectMimSearchAction(const FVector& Focus) const;
    void UpdateMimFit(float DeltaSeconds);
    void UpdateProxy(float DeltaSeconds);
    bool bProxyWasFalling = false;
    float ProxySquash = 0.f;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ProxyPaint;
    bool bMound = false, bMoundFast = false;
    float MoundAlpha = 0.f, ProxyStride = 0.f;
    void UpdateFit(float DeltaSeconds);
    FVector FitSize = FVector(160.f, 160.f, 140.f), FitOffset = FVector::ZeroVector;
    float FitLift = 40.f, FitSpeed = 1.f, SqueezeBeatUntil = 0.f;
public:
    static bool IsProxyMode();
    /** True when locomotion is not using the legacy Keeper hand-contact solver. */
    bool IsProxy() const { return bProxy || bUsingMim; }
    /** Body size and measured free length, width and height, in actor axes and cm. */
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FVector GetFitSize() const { return FitSize; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    float GetFitSpeed() const { return FitSpeed; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FVector GetFitClearance() const { return MimFitClearance; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    float GetFitCompression() const { return MimFitCompression; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    bool IsFitPassable() const { return bMimFitPassable; }
    bool TryCheckedRecoveryStep(const FVector& Toward, FString& Reason, bool bTurnOnly = false);
    /** Current evaluated body, current heading only; no actor or contact mutation.
     *  Initial local escape edges are limited to120cm and checked continuously. */
    bool CanCheckedRecoveryTranslate(const FVector& Destination, FString& Reason) const;
    /** Pure next8cm ground-step check using the current heading and evaluated body. */
    bool CanCheckedGroundAdvance(const FVector& Toward, FString& Reason) const;
    /** Pure translation of the current evaluated skin-audit shapes in WORLD axes.
     *  No capsule/floor policy; shared by mesh-offset and grounded escape checks. */
    bool CanCheckedBodyTranslate(const FVector& WorldDelta, FString& Reason, bool bTrunkOnly = false) const;
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    bool IsTallDoorwayFit() const { return bMimTallDoorFit; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FVector GetPosedFitSize() const { return MimPosedFitSize; }
    /** Stable geometry pressure: actor X forward, Y lateral, Z height; 0 open to 1 folded. */
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FVector GetFitAxisWeights() const { return MimFitAxisWeights; }
    /** Requested body-envelope scale per actor axis, before authored pose deformation. */
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FVector GetFitAxisTargetScale() const { return MimFitAxisTargetScale; }
    /** Shape against the real body floor band during a blocked remote-tail recovery;
     *  GetFitClearance and IsFitPassable retain the stricter whole-route gate. */
    FVector GetFitPoseClearance() const { return MimFitPoseClearance; }
    /** Exact cached linear-skin positions for an evaluated mesh-index pose; safe after first fit update. */
    FBox MeasureAdaptiveFitBounds(const TArray<FTransform>& MeshBoneCS, int32 BoneA = INDEX_NONE,
        int32 BoneB = INDEX_NONE, bool bMassOnly = false, bool bIncludeDescendants = true,
        bool bIncludeLooseFur = true) const;
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    FString GetFitBlockReason() const { return MimFitBlockReason; }
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    float GetFitBlockedSeconds() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Fit")
    float GetFitMeasureMilliseconds() const { return MimFitMeasureMilliseconds; }
    /** Hauling itself through a doorway frame: slow for a moment, the body squeezed to the opening. */
    void BeginSqueezeBeat(float Seconds);
    /** After a teleport: fit the body to where it now is at once, instead of blending from where it was. */
    void SnapFit() { if (bProxy && bInitialized) UpdateProxy(1.f); else if (bUsingMim && bInitialized) UpdateMimFit(1.f); }
    /** Every body part swept against world collision now: anything in deeper than MinDepth (no clipping, ever). */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") TArray<FTallowClipHit> AuditClipping(float MinDepth = 2.f) const;
    /** The resting form: a still, harmless-looking heap (the disguise ambush). bFast springs it back up on reveal. */
    void SetMound(bool bOn, bool bFast = false);
    bool IsMound() const { return bMound; }
    float GetMoundAlpha() const { return MoundAlpha; }
private:
    bool bNarrowPortal = false;
    FName SelectedClip;
    float EvaluateSupport(int32 ExcludedHand, FVector& Correction, int32& Count) const;
    FQuat ContactRotation(const FTallowHunterHand& Hand, const FVector& Normal) const;
    FVector FloorContactGoal(int32 Index, float Spread = 1.f) const;
    bool ProbeHand(int32 Index, FVector& Point, FVector& Normal, bool& bWall,
        UPrimitiveComponent*& Surface) const;
    void BeginReach(int32 Index, const FVector& Point, const FVector& Normal, bool bWall,
        UPrimitiveComponent* Surface, float Speed);
    void UpdatePosture(float DeltaSeconds, float Speed);
    /** Procedural grab: no authored capture clip fits the V13 skeleton. */
    void UpdateCapture(float DeltaSeconds);
    float CaptureStartedAt = -1.f;
    bool bCaptureCrackPlayed = false;
    void UpdateHands(float DeltaSeconds, float Speed);
    void UpdatePlayback(float DeltaSeconds, float Speed);
};
