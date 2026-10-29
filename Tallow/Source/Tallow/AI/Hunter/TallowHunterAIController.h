#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Noise/TallowNoiseTypes.h"
#include "AI/Hunter/TallowMimMovementContract.h"
#include "TallowHunterAIController.generated.h"

class UTallowHunterBrain;
class ATallowHunterCreature;
class ATallowCharacter;
class ATallowHideVolume;
class UTallowNoiseSubsystem;
class UTallowHunterDirector;
class ATallowHunterSearchAnchor;

UENUM(BlueprintType)
enum class ETallowHuntMode : uint8
{
    Roam, Investigate, Search, Inspect, Pursue, Withdraw, Captured, Listen, Ambush
};

/** How it answers a noise. It never ignores one (25 Oct: "it's intelligent, it wants to find the kid"); it varies
 *  how it comes, and never the same way twice running. */
UENUM(BlueprintType)
enum class ETallowNoiseResponse : uint8
{
    None, Hunt, Stalk, Flank, Ambush, Lurk, Rush,
    /** Stand off in the dark where she can be seen, and watch the place the sound came from. */
    Watch
};

/** What the brain wants the body to do this moment. The body (motion, animation, traversal) turns it into a gait,
 *  a stance and a speed; the brain keeps navigation, doors and the rules. */
UENUM(BlueprintType)
enum class ETallowKeeperIntent : uint8
{
    Idle, Wander, Stalk, Hunt, Chase, Listen, Search, Inspect, Lurk, Reveal, Lunge,
    /** Still as a harmless heap (the resting form), or hanging and waiting on a ceiling, in a crawl, on a cupboard. */
    Disguise, Hold
};

/** One-off moments the body plays an authored beat for: the call on seeing her, a sound heard, losing her, the grab. */
UENUM(BlueprintType)
enum class ETallowKeeperEvent : uint8
{
    None, Alert, Lost, Heard, Seen, Capture,
    /** Out of hiding: the heap unfolds, it drops from the ceiling, it bursts out of a crawl. */
    Reveal
};

USTRUCT(BlueprintType)
struct FTallowKeeperEvent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ETallowKeeperEvent Type = ETallowKeeperEvent::None;
    /** Goes up by one with every event, so a poller sees each once. */
    UPROPERTY(BlueprintReadOnly) int32 Count = 0;
    UPROPERTY(BlueprintReadOnly) float Time = -100.f;
};

/** Evidence-based hunter. All pursuit locations originate in sight or audible noise. */
UCLASS()
class TALLOW_API ATallowHunterAIController : public AAIController
{
    GENERATED_BODY()
public:
    ATallowHunterAIController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Hunter") void ResetHunt();
    UFUNCTION(BlueprintCallable, Category="Hunter") void SetHuntEnabled(bool bEnabled);
    /** Stage locomotion reviews only while autonomous decisions are paused. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool SetReviewMovementMode(ETallowHuntMode Mode);
    UFUNCTION(BlueprintCallable, Category="Hunter") void EnableSecondPhase();
    UFUNCTION(BlueprintPure, Category="Hunter") bool HasVisiblePlayer() const { return bVisible; }
    /** Fresh sight only: gaze must not follow a hidden player during perception grace. */
    UFUNCTION(BlueprintPure, Category="Hunter|Gaze") bool HasDirectPlayerSight() const { return bDirectPlayerSight; }
    UFUNCTION(BlueprintPure, Category="Hunter|Gaze") FVector GetVisiblePlayerGazeTarget() const { return VisiblePlayerGazeTarget; }
    UFUNCTION(BlueprintPure, Category="Hunter") bool CanCapturePlayer() const;
    /** Evidence authorizing a physical grab now; None means capture is forbidden. */
    UFUNCTION(BlueprintPure, Category="Hunter|Capture") FName GetCaptureEvidence() const;
    UFUNCTION(BlueprintCallable, Category="Hunter") bool TryCapturePlayer();
    UFUNCTION(BlueprintPure, Category="Hunter") FVector GetBeliefLocation() const { return EvidenceLocation; }
    UFUNCTION(BlueprintPure, Category="Hunter") FVector GetSearchGoal() const { return Goal; }
    /** A point a little way along its current route (the second corner ahead, or the goal), for the head to lead. */
    bool GetRouteAhead(FVector& Out) const;
    UFUNCTION(BlueprintPure, Category="Hunter|Body") ETallowKeeperIntent GetIntent() const;
    /** Where its attention is: the sound, the spot it waits by, the place it inspects, or where it is going. */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") FVector GetIntentFocus() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsWaitingForDoor() const { return bWaitingForDoor; }
    UFUNCTION(BlueprintPure, Category="Hunter|Body") FTallowKeeperEvent GetIntentEvent() const { return IntentEvent; }
    UFUNCTION(BlueprintPure, Category="Hunter") UTallowNoiseSubsystem* GetNoiseSystem() const;
    UFUNCTION(BlueprintPure, Category="Hunter") UTallowHunterDirector* GetHuntDirector() const;
    UFUNCTION(BlueprintPure, Category="Hunter") float GetAttackWindupAlpha() const { return AttackWindupAlpha; }
    UFUNCTION(BlueprintPure, Category="Hunter") float GetEvidenceStrength() const { return EvidenceStrength; }
    UFUNCTION(BlueprintPure, Category="Hunter") float GetPursuitFatigue() const { return FMath::Clamp((ContinuousPursuit - 12.f) / 10.f, 0.f, 1.f); }
    UFUNCTION(BlueprintPure, Category="Hunter") int32 GetMemoryCount() const { return Observations.Num(); }
    UFUNCTION(BlueprintCallable, Category="Hunter") bool RequestAmbushFromEvidence();
    UFUNCTION(BlueprintCallable, Category="Hunter") void SetRandomSeed(int32 Seed) { Random.Initialize(Seed); }
    /** Walking pace on top of the mode's own speed: a slow, deliberate approach or a quiet lurk. */
    UFUNCTION(BlueprintPure, Category="Hunter") float GetPaceScale() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") ETallowNoiseResponse LastNoiseResponse = ETallowNoiseResponse::None;
    /** Reviews only: answer every loud noise this way (None restores the weighted choice). */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") void ForceNoiseResponseForReview(ETallowNoiseResponse Response) { ForcedNoiseResponse = Response; }
    ETallowNoiseResponse ForcedNoiseResponse = ETallowNoiseResponse::None;
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsDisguised() const { return bDisguised; }
    /** Lying in wait and keeping still: the heap, a hold, a corner brace, a covered ambush, or watching her. The
     *  house must not announce it (no search lines, no "too close", no hunted drone); only its breath this close. */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsLurking() const;
    /** Coming in silently (29 Oct: "she made herself known every time, so it was never a surprise"): a creep in on a
     *  sound, or a hunt that does not announce itself. No heard beat, no lines, no search scratching; the first sign is
     *  seeing her, or her breath right beside you. The brain mirrors it into the voice's SetCreeping every tick (the
     *  voice, her steps and the soundscape read that), and IsLurking() is true too, so the run widget's signs hush.
     *  Anything else that makes her heard (body foley, drag) should go quiet while this is true. */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsCreeping() const;
    /** The director sent it to search the room she is keeping still in (the close call). */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsCloseCall() const { return bCloseCall; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 Creeps = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 CloseCallsStarted = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 CloseCallsLeft = 0;
    /** Closest it came to her (cm) in the current or last close call. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") float CloseCallClosest = 0.f;
    /** Standing off in the dark watching her (the stalk): eyes catch what light there is. */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") bool IsWatching() const { return bWatching; }
    /** 0..1 while it watches, eased in and out, for the eye glints. */
    UFUNCTION(BlueprintPure, Category="Hunter|Body") float GetWatchGlint() const { return WatchGlint; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 WatchesStarted = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 WatchesHeld = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 WatchSlips = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 CornerProbes = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") int32 VoluntarySurfaceRoutes = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Tactics") FString LastCornerProbe;
    /** Reviews only (the Keeper Lab reels): settle the heap here now; hang from the ceiling over a spot; take a link
     *  (walks to its start first). */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool ReviewDisguiseAt(const FVector& Where);
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool ReviewPerchOver(const FVector& Where);
    /** Reviews only: stand watching her from here now; bHold keeps it there (no slipping away, no time limit). */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool ReviewWatchAt(const FVector& Where, bool bHold);
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool ReviewTakeLink(int32 Index, bool bReverse);
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") FString DescribeLinks();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") ETallowHuntMode HuntMode = ETallowHuntMode::Roam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") float SightConfidence = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 SightingCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 HeardNoiseCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 InspectedSpots = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 CapturesWithoutEvidence = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 ListeningPauses = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 AmbushesStarted = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") int32 AbortedGrabs = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoveryAttempts = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoverySuccesses = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") TArray<FString> RecoveryLog;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FString GeometryRouteCosts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FVector GeometryPassage = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Routes") bool bUseMovementContract = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FTallowMimTransitionResult MovementContractResult;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FString MovementContractStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") bool ContractReviewReached = false;
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") bool ReviewContractMoveTo(const FVector& Destination);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Routes") bool bAllowRecoveryFitAssist = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") bool bRecoveryFitAssist = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoveryFitAssistCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FString RecoveryPlannerStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FString RecoveryPlannerSourcePose;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FString RecoveryPlannerFailureSummary;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoveryPlannerExpansions = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoveryPlannerEdgeChecks = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 RecoveryPlannerFailedEdges = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") float RecoveryPlannerCPUms = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") float RecoveryPlannerSliceMs = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") float RecoveryPlannerWallMs = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") float RecoveryPlannerExecutorMs = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") float RecoveryPlannerSeconds = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") FName TacticalPhase = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") int32 TacticalRetreats = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Routes") TArray<FString> TacticalLog;
    UFUNCTION(BlueprintPure, Category="Hunter|Light") float GetLightExposureAt(const FVector& WorldPoint);
    UFUNCTION(BlueprintPure, Category="Hunter|Light") float GetShadowLevelAt(const FVector& WorldPoint);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Light") FName ShadowRouteReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Light") int32 ShadowDetours = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Light") int32 ShadowDirectCommits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Light") float ShadowDirectExposure = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Light") float ShadowChosenExposure = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Routes", meta=(ClampMin="0", ClampMax="1")) float StruggleRetreatChance = .38f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") FName LastDecision = TEXT("Roam");
    /** Its recent decisions, oldest first, each with the game time it made it ("12.3 CeilingDrop: dropped on her"). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") TArray<FString> DecisionLog;
    bool bCatchUp = false;
    float LastClearAt = -1000.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter") FName LastEvidenceType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") bool bAllowCapture = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") bool bUseDirector = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") bool bUseSurfaceRoutes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") bool bUseAmbushes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float LitSightRange = 1700.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float CuppedSightRange = 430.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter") float CaptureWindupSeconds = .55f;
protected:
    UFUNCTION() void HearNoise(const FTallowNoiseEvent& Event);
    /** On a fresh trail (it saw her or answered a sound a little while ago) a new sound is followed at once. */
    bool IsOnTrail(float Now) const;
    /** Follow a new sound on the trail straight away: no listening beat, no new plan. A loud one ends a creep. */
    void CloseInOn(const FVector& Where, bool bLoud);
    UFUNCTION() void OnCaptured();
private:
    UPROPERTY() TObjectPtr<UTallowHunterBrain> HunterBrain;
    UPROPERTY() TObjectPtr<ATallowHunterCreature> Creature;
    UPROPERTY() TObjectPtr<ATallowCharacter> Player;
    UPROPERTY() TObjectPtr<ATallowHideVolume> InspectionSpot;
    UPROPERTY() TObjectPtr<ATallowHunterSearchAnchor> SearchAnchor;
    TMap<TWeakObjectPtr<AActor>, float> SpotVisits;
    TArray<FVector> RecentSearchPoints;
    struct FObservation
    {
        FVector Location;
        float Time;
        float Strength;
        bool bVisual;
    };
    TArray<FObservation> Observations;
    TMap<TWeakObjectPtr<AActor>, float> AnchorVisits;
    TMap<FIntVector, int32> LearnedAreas;
    FVector EvidenceLocation = FVector::ZeroVector;
    FVector EvidenceHeading = FVector::ZeroVector;
    FVector PreviousVisualLocation = FVector::ZeroVector;
    FVector AmbushFocus = FVector::ZeroVector;
    FVector Goal = FVector::ZeroVector;
    FVector LastPursuitGoal = FVector::ZeroVector;
    FVector ProgressAnchor = FVector::ZeroVector;
    FVector RecoveryFinalGoal = FVector::ZeroVector, RecoveryStartedAt = FVector::ZeroVector;
    TArray<FVector> RecoveryFloorQueue;
    FName RecoveryMode = NAME_None;
    FString ActiveRecoveryKey;
    TMap<FString, float> RecoveryCooldowns;
    TMap<int32, float> LinkRetryAfter;
    float NextRecoveryTry = 0.f, RecoveryInitialDistance = 0.f;
    float RecoveryFitClearSince = -1.f, NextRecoveryTurnTry = 0.f;
    int32 RecoveryTraversalAborts = 0;
    bool bRecoveryDetour = false, bRecoveryTraversal = false, bRecoveryWaiting = false, bRecoveryGoalRequest = false;
    FString RecoveryKey(FName Mode, const FVector& Here, const FVector& FinalGoal) const;
    void NoteRecovery(const FString& Message);
    TArray<FName> RankRecoveryRoutes(const FVector& FinalGoal);
    bool TryRecoverMove(float Now, const FVector& FinalGoal);
    struct FRecoverySearchNode
    {
        int32 Point = 0, Parent = INDEX_NONE, Heading = 0;
        float Yaw = 0.f, Cost = 0.f, Estimate = 0.f;
        FName Mode = NAME_None, Posture = NAME_None;
        bool bClosed = false, bKeepHeading = false;
    };
    TArray<FVector> RecoverySearchPoints;
    TArray<FRecoverySearchNode> RecoverySearchNodes;
    TMap<FIntVector, int32> RecoverySearchLookup;
    TMap<FString, FString> RecoveryFailedEdges;
    TMap<FString, int32> RecoveryFailureReasons;
    FBox RecoverySearchMass = FBox(ForceInit);
    FVector RecoverySearchOrigin = FVector::ZeroVector, RecoverySearchTarget = FVector::ZeroVector;
    FVector RecoverySearchPoseSize = FVector::ZeroVector;
    FVector RecoverySearchClearance = FVector::ZeroVector;
    TArray<FVector> RecoverySearchArms;
    float RecoverySearchYaw = 0.f;
    double RecoverySearchBegan = 0.;
    uint32 RecoverySceneSignature = 0;
    uint32 RecoveryWorldSignature = 0;
    int32 RecoveryExpanding = INDEX_NONE, RecoveryNeighbor = 0, RecoveryBestPartial = INDEX_NONE;
    bool bRecoverySearchActive = false, bRecoverySearchFinished = false;
    bool bRecoveryCheckedTranslation = false;
    void UpdateContractMovement(float Now);
    bool bContractReview = false;
    float ContractBlockedSince = -1.f, NextContractRestoreTry = 0.f;
    FVector ContractCompactAt = FVector::ZeroVector;
    // Wedge escape: last spot she demonstrably reached, and the reverse-out state.
    FVector ContractProgressAnchor = FVector::ZeroVector;
    float ContractProgressAt = -1.f, ContractBacktrackUntil = -1.f, NextContractBacktrackTry = 0.f;
    FVector ContractBacktrackTarget = FVector::ZeroVector;
    void UpdateRecoveryFitAssist(float Now);
    FVector RecoveryAssistAnchor = FVector::ZeroVector, RecoveryAssistOrigin = FVector::ZeroVector;
    float RecoveryAssistStalledSince = -1.f, RecoveryAssistStarted = 0.f;
    float RecoveryAssistRadius = 34.f, RecoveryAssistHalfHeight = 50.f;
    int32 RecoveryAssistAttemptsAtProgress = 0;
    bool UpdateRecoveryPlanner(float Now, const FVector& Destination);
    void ResetRecoveryPlanner(bool bForgetFailures = false);
    void RejectRecoveryConnection(const FString& Reason);
    FString RecoveryConnectionKey(FName Mode, const FVector& From, const FVector& To, float Heading) const;
    bool CheckRecoveryFloorEdge(const FVector& From, float FromYaw, const FVector& To,
        float& Seconds, FName& Posture, FString& Reason, bool bKeepHeading = false);
    bool ExecuteRecoveryPlan(float Now, int32 NodeIndex, bool bPartial);
    bool UpdateTacticalRetreat(float Now, float DeltaTime);
    bool PlanTacticalIntercept(float Now);
    bool TacticalPathClear(const FVector& Destination) const;
    void NoteTactical(const FString& Message);
    FVector TacticalMemory = FVector::ZeroVector, TacticalStruggleAt = FVector::ZeroVector, TacticalBackout = FVector::ZeroVector;
    float StruggleSince = -1.f, LastStruggleAt = -1000.f, StruggleClosest = MAX_flt, NextTacticalThought = 0.f, TacticalUntil = 0.f;
    bool bTacticalBackoutGoal = false;
    bool UpdateCornerTactics(float Now);
    float NextCornerProbe = 0.f, CornerHoldUntil = 0.f;
    FVector CornerAmbushAnchor = FVector::ZeroVector;
    /** Corners whose brace put the body into a wall (measured): never taken again. */
    TArray<FVector> ClippedCorners;
    float NextCornerClipAudit = 0.f;
    bool bCornerAmbush = false, bCornerAttackPending = false;
    bool SelectShadowGoal(const FVector& Desired, FVector& Routed);
    void ResetShadowRouting();
    bool ResumeShadowDestination(float Now);
    TArray<TWeakObjectPtr<class ULightComponent>> ShadowLights;
    struct FShadowSample { FVector Point; float At = 0.f, Lux = 0.f; };
    TMap<FIntVector, FShadowSample> ShadowSamples;
    FVector ShadowDestination = FVector::ZeroVector, ShadowWaypoint = FVector::ZeroVector;
    float NextShadowLights = 0.f, NextShadowPlan = 0.f, ShadowWaypointUntil = 0.f, ShadowDirectUntil = 0.f;
    bool bShadowWaypointActive = false, bShadowInternalGoal = false;
    float LastSeenAt = -1000.f;
    float LastHeardAt = -1000.f;
    float NextDecision = 0.f;
    float NextAreaDecision = 0.f;
    float NextMoveRequest = 0.f;
    float NextPursuitGoalTry = 0.f;
    /** Doors in the house: it opens one in its way and waits for the leaf to swing clear (25 Oct). */
    struct FDoorWatch { TWeakObjectPtr<class ATallowDoor> Door; TWeakObjectPtr<USceneComponent> Hinge; float ClosedYaw = 0.f; FVector Center = FVector::ZeroVector; float AskedAt = -100.f; float WaitingSince = -1.f; float IgnoreUntil = 0.f; float LockNoticeUntil = 0.f; };
    TArray<FDoorWatch> Doors;
    bool bDoorsCached = false;
    bool bLeaveBoardsAfterStare = false;
    FVector BoardedDoorAt = FVector::ZeroVector;
    void CacheDoors();
    /** A boarded or shut door lies on the line between these points: it cannot see or reach you through it
     *  (the door leaf does not block the Visibility channel). */
    bool BoardsBetween(const FVector& From, const FVector& To);
    float DoorOpenness(const FDoorWatch& Watch) const;
    /** A navmesh route that runs through a boarded door or a shut gate. */
    bool PathCrossesShut(const TArray<FVector>& Points);
    bool bWaitingForDoor = false;
    FVector LastHeading = FVector::ForwardVector;
    /** A door it cannot use now: boarded, or locked. */
    static bool IsBarred(const class ATallowDoor* Door);
    /** 0 clear, 1 a door it is opening (wait), 2 a boarded door (a wall: it stops and stares at it), 3 a locked door
     *  (a wall until she picks it: it looks and goes round). */
    int32 DoorInTheWay(float Now, FVector& DoorAt);
    /** Applies DoorInTheWay: waits for a door it is opening, or treats a boarded one as a wall. True while holding. */
    bool HoldForDoor(float Now);
    float SearchUntil = 0.f;
    // 25 Oct: if it hasn't found you in an area for a while it moves on, and quiet sounds from
    // the area it just gave up on don't drag it straight back.
    FVector AreaCenter = FVector::ZeroVector, QuietAreaCenter = FVector::ZeroVector;
    float AreaSearchStartedAt = -1.f, QuietUntil = 0.f;
    // Echolocation in the dark (25 Oct).
    float NextEchoAt = 0.f;
    UPROPERTY(EditAnywhere, Category="Hunter|Senses") float EchoRange = 700.f;
    /** In the dark it hears you breathe this close (cm), unless you hold your breath (25 Oct). */
    UPROPERTY(EditAnywhere, Category="Hunter|Senses") float BreathHearing = 550.f;
    /** In the dark it can take you from this far if it hears you; holding your breath, only if it walks into you. */
    UPROPERTY(EditAnywhere, Category="Hunter|Senses") float DarkGrabReach = 230.f;
    UPROPERTY(EditAnywhere, Category="Hunter|Senses") float BumpReach = 95.f;
    UPROPERTY(Transient) TObjectPtr<class USoundBase> EchoClick;
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> AlertAttenuation;
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> EchoAttenuation;
    float InspectUntil = 0.f;
    float ListenUntil = 0.f;
    float AmbushUntil = 0.f;
    float NextAmbushAt = 0.f;
    float NextNoiseReaction = 0.f;
    float AttackStartedAt = -1.f;
    float AttackWindupAlpha = 0.f;
    float EvidenceStrength = 0.f;
    float PreviousVisualAt = -1000.f;
    float SearchStartedAt = 0.f;
    float ContinuousPursuit = 0.f;
    int32 SearchSteps = 0;
    bool bTravellingToAmbush = false;
    float NextSurfaceTry = 0.f, NextVoluntaryTry = 0.f;
    float GoalDeadline = 0.f;
    float ProgressAt = 0.f;
    bool bVisible = false;
    bool bDirectPlayerSight = false;
    FVector VisiblePlayerGazeTarget = FVector::ZeroVector;
    bool bHadVisible = false;
    TWeakObjectPtr<ATallowHideVolume> KnownOccupiedHide;
    float KnownHideEvidenceAt = -1000.f;
    bool bHasEvidence = false;
    bool bHasGoal = false;
    bool bSecondPhase = false;
    FRandomStream Random{90210};
    bool SensePlayer(float DeltaSeconds);
    void EnterMode(ETallowHuntMode Mode);
    void BeginSearch();
    bool ChooseSearchPoint();
    void RandomiseStart(bool bTeleport);
    /** Sprinting and landing give your position away outright. */
    void Rush(const FVector& Where, float Seconds);
    /** Prying a door open: one of the noise responses below, weighted for the door. */
    void ReactToPry(const FVector& Where);
    /** bLoud: her running, a landing, a door she slammed. Those it comes for, it does not wait for. */
    ETallowNoiseResponse ChooseNoiseResponse(const FVector& Where, bool bPry, bool bLoud = false);
    void RespondToNoise(const FVector& Where, bool bPry, bool bLoud = false);
    float LastNoiseResponseAt = -1000.f;
    /** The answer chosen at the listening beat comes silently (no heard beat, no line, then a creep). */
    bool bListenSilent = false;
    bool bCreeping = false;
    float CreepUntil = 0.f;
    void BeginCreep(float Seconds, float Pace);
    void EndCreep();
    // The close call (29 Oct): dark, still and holding her breath, the director sends it to search her room for
    // real. It is told the room, never where she is in it; its own senses decide whether it finds her.
    bool TryCloseCall(float Now);
    bool UpdateCloseCall(float Now);
    void EndCloseCall(float Now, bool bLeave, const TCHAR* Why);
    bool bCloseCall = false, bCloseCallPausing = false;
    FName CloseCallRoom;
    TArray<FVector> CloseCallPoints;
    TArray<TWeakObjectPtr<ATallowHideVolume>> CloseCallSpots;
    FVector CloseCallLookB = FVector::ZeroVector;
    float CloseCallStartedAt = 0.f, CloseCallLookSwapAt = -1.f, NextCloseCallTry = 0.f;
    /** What the end of a listening pause turns into (set by RespondToNoise; a plain listen investigates). */
    ETallowNoiseResponse ListenOutcome = ETallowNoiseResponse::Hunt;
    void FinishListening(float Now);
    bool LurkNear(const FVector& Where);
    FVector ListenedTo = FVector::ZeroVector;
    float ListenedLoudness = 0.f;
    /** The tactics. Each sets a goal from its own knowledge (the sound, the rooms, the doors), never her position. */
    bool FlankTo(const FVector& Where);
    bool AmbushExit(const FVector& Where);
    /** Doors and gates it knows, for exits and for stopping to look through. */
    TMap<TWeakObjectPtr<AActor>, float> DoorPauses;
    void ThinkAtDoorways(float Now);
    /** The jump-out: stalk unseen to a hidden spot near where it believes she is, wait, then step out at her. */
    bool bBooPending = false;
    float BooRevealAt = -1.f;
    FVector BooTarget = FVector::ZeroVector;
    bool TryBoo(float Now);
    void RevealBoo(float Now);
    FTallowKeeperEvent IntentEvent;
    void FireEvent(ETallowKeeperEvent Type);
    float PaceScale = 1.f, PaceUntil = 0.f;
    void SetPace(float Scale, float Seconds);
    /** The beat after it first sees her: it stops, calls, and only then comes (time to react). */
    float SpotFreezeUntil = 0.f;
    float LastSightingStart = -1000.f;
    /** Shut route gates (power-locked) are walls to it until they swing open. */
    struct FGateWatch { TWeakObjectPtr<class ATallowRouteGate> Gate; FVector Center = FVector::ZeroVector; };
    TArray<FGateWatch> Gates;
    float GateOpenness(const FGateWatch& Watch) const;
    /** Candle out: how close it is to you in the dark, and whether it can hear your breathing. */
    bool IsDark() const;
    bool HearsBreathing(float Distance) const;
    /** How far it hears her breathe: BreathHearing, further while she pants after a run. */
    float BreathRadius() const;
    float NextBreathGoal = 0.f;
    float NextWanderClimb = 15.f;
    void AlertPlayer();
    bool SetGoalNearest(const FVector& Location);
    float NextDebugLog = 0.f;
    float NextPeekAt = 0.f;
    float RushUntil = 0.f, NextRushRefresh = 0.f, NextAlertCue = 0.f, NextPryReaction = 0.f;
    bool bPlayerWasHiding = false;
    bool SetGoal(const FVector& Location);
    void UpdateMove(float Now);
    void Observe(const FVector& Location, float Strength, bool bVisual);
    void BeginListening(float Duration, bool bVoice = true);
    void UpdateAttack(float Now);
    bool HasClearTrace(const FVector& From, const FVector& To, AActor* ExtraIgnored = nullptr) const;
    bool HasAmbushCover(const FVector& Location) const;
    float FamiliarityAt(const FVector& Point) const;
    bool IsReachable(const FVector& Location, FVector& Projected, float* OutPathLength = nullptr) const;
    /** Its eyes and ears: the box's front, or the rigged body's head. */
    FVector HeadLocation() const;
    void TickBrain(float DeltaSeconds);
    void NoteDecision();
    FName LoggedDecision;
    /** Presentation only (a scare must be in front of her, a disguise must never form in her view): her camera. */
    bool PlayerLooksAt(const FVector& Point, float MinDot, bool bNeedClear = true) const;
    /** Close enough to notice her by its own senses: sight, her footsteps, or her breathing in the dark. */
    bool SensesPlayerClose(float Distance) const;
    void CancelSpiderPlans();
    /** The mound and the holds run the body themselves while active. True while they do. */
    bool UpdateSpiderAmbush(float Now, float DeltaSeconds);
    bool HandleNoiseInAmbush(const FVector& Where, float Perceived, bool bLoud = false);
    // The disguise ambush: it goes still as a harmless heap on her likely route and unfolds when she comes close
    // or turns her back (my resting form: "soft, fluffy, harmless").
    bool bDisguisePending = false, bDisguiseArrived = false, bDisguised = false;
    FVector LastDisguiseSpot = FVector(1.e7f);
    float DisguiseUntil = 0.f, DisguiseSettleBy = 0.f, StaredFor = 0.f, ClosestWhileDisguised = MAX_flt;
    bool TryDisguise(float Now);
    bool FindMoundSpot(FVector& Spot, FVector& Facing);
    void SettleDisguise(float Now);
    void Unfurl(float Now, bool bAttack, const TCHAR* Why);
    // Holds: hanging from the ceiling over a way through, in the mouth of a crawl, on top of a cupboard.
    enum class EHold : uint8 { None, Ceiling, Crawl, Top };
    EHold PlannedHold = EHold::None;
    /** Walking to where the climb starts (or to where a ceiling route can start). */
    bool bHoldPending = false;
    FVector HoldTarget = FVector::ZeroVector;
    int32 HoldLink = INDEX_NONE;
    bool bHoldLinkReverse = false, bBurstOut = false;
    float HoldUntil = 0.f;
    bool PerchOver(const FVector& Where, const FVector& Focus, const TCHAR* Why);
    bool LurkInLink(const FVector& Where);
    bool TrySpiderAmbush(const FVector& Where);
    bool StartHold();
    void ReleaseHold(float Now, const TCHAR* Why, bool bAttack);
    // Climb and crawl links (Tallow.Climb and Tallow.Crawl markers, shared with the player's routes).
    struct FLink
    {
        bool bCrawl = false, bExplicit = false;
        /** Shared player portal; Mim must fit the whole passage before following. */
        bool bPlayerCrawl = false;
        TWeakObjectPtr<class ATallowRouteGate> Gate;
        TArray<FVector> Points, Normals;
        float Length = 0.f;
        float EstimatedSpeedScale = 1.f;
    };
    TArray<FLink> Links;
    bool bLinksCached = false;
    void CacheLinks();
    bool BuildLinkRoute(int32 Index, bool bReverse, bool bHold);
    int32 PendingLink = INDEX_NONE;
    bool bPendingLinkReverse = false, bOnLink = false, bLinkFinalGoal = false;
    FVector LinkFinalGoal = FVector::ZeroVector;
    float NextLinkThought = 0.f, NextDisguiseTry = 0.f, NextScratch = 0.f;
    float StalkSightUntil = 0.f, PreviousAmbushDistance = -1.f, AmbushClosing = 0.f, NextOverTry = 0.f;
    TMap<TWeakObjectPtr<AActor>, float> DoorSqueezes;
    void SqueezeAtDoorways(float Now);
    float UpdateClosing(float Distance, float DeltaSeconds);
    /** Marked lurk spots: ceilings just inside doorways, and floor spots outside the far mouth of her crawls. */
    struct FLurk { FVector Location = FVector::ZeroVector; bool bCeiling = false; FVector Watch = FVector::ZeroVector; };
    TArray<FLurk> Lurks;
    bool LurkAtMarker(const FVector& Where);
    bool LinkOpen(int32 Index) const;
    bool HeadForLink(int32 Index, bool bReverse, const TCHAR* Why);
    bool TryLinkShortcut(float Now, bool bRecovery = false);
    bool TryLinkEscape(float Now);
    /** At the start of a link it was heading for, or off the end of one: take it, or carry on to the real goal. */
    bool UpdateLinks(float Now);
    /** Shortcuts, over the furniture, and up the walls and over the ceiling when it chooses to (not only when the
     *  floor fails). Shared by the plain path follower and the movement contract. True when a route started. */
    bool TryVoluntaryRoutes(float Now, float Distance);
    /** Plans that were only on their way (a boo, a heap, a hold, a watch) end when it changes its mind. */
    void DropPendingPlans();
    FName LoggedTactic;

    // The stalk (28 Oct: "I want to see Mim stalk"). In the build it stands off in the dark a room away where she
    // can see it, holds still and watches, slips out of sight when she turns the light on it, and comes back closer.
    // It never pursues from a watch on area knowledge alone: only her lit candle, a sound, or her breath close by.
    bool TryWatch(float Now, const FVector& Area, bool bFromNoise);
    bool FindWatchPost(const FVector& Area, float MinPath, float MaxPath, FVector& Post, float LightLimit);
    void BeginWatch(float Now);
    void UpdateWatch(float Now, float DeltaSeconds);
    void EndWatch(float Now, bool bSlip, const TCHAR* Why);
    bool bWatchPending = false, bWatching = false, bWatchStaring = false;
    FVector WatchArea = FVector::ZeroVector, WatchPost = FVector::ZeroVector, LastWatchPost = FVector(1.e7f);
    float WatchUntil = 0.f, WatchStareUntil = 0.f, WatchLookedAt = 0.f, NextWatchGaze = 0.f;
    float WatchGlint = 0.f, SlipGiggleAt = -1.f, NextWatchTry = 0.f;
    bool bWatchReviewHold = false;
    /** The post is framed in a doorway (the body holds the wait-outside lean there). */
    bool bWatchFromDoor = false;
    /** Just slipped away: a moment of her still in its eyes does not turn it round into a chase. */
    float WatchSlipUntil = 0.f;
    int32 WatchStage = 0;
};
