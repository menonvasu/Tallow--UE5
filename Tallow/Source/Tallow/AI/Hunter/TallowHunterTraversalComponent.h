#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowHunterTraversalComponent.generated.h"

class ACharacter;
class UTallowHunterMotionComponent;

/** Preview traversal. A hand establishes the new surface before the torso transfers weight. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowHunterTraversalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTallowHunterTraversalComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* Tick) override;

    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartSurfaceRoute();
    /** Floor, wall, across the ceiling toward the target and down the far wall. With bPerch it stops on the ceiling
     *  and hangs there (the ambush over a door) until DropFromCeiling or a ground exit. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartChaseSurfaceRoute(const FVector& TargetLocation, bool bPerch = false);
    /** Rise from the floor, acquire the reachable ceiling, then use the checked ceiling route. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartDirectCeilingRoute(const FVector& TargetLocation);
    /** Let go of the ceiling and fall, landing upright and heavy. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") void DropFromCeiling() { PounceToward(FVector::ZeroVector); }
    /** Let go wherever it holds (ceiling, wall, the top of a cupboard) and come down at the target, if one is given. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") void PounceToward(const FVector& Target);
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsPouncing() const { return bPounceActive; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetPounceClipAlpha() const
    { return FMath::Clamp(PounceAge / (33.f / 24.f), 0.f, 1.f); }
    /** A checked climb into a real two-wall corner, then a braced hold. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal")
    bool StartCornerAmbushRoute(const FVector& Corner, const FVector& WallNormalA, const FVector& WallNormalB);
    /** Waiting at the end of a held route: hanging from the ceiling, in a crawl mouth, on top of a cupboard. */
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsHolding() const { return bActive && Phase == TEXT("Perch"); }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsPerched() const { return IsHolding(); }
    /** Along a marked climb or crawl link: surface points and their outward normals, in order. A squeeze runs the
     *  whole way at HalfHeight (the capsule narrows to match); bHold stops at the last point and waits there. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal")
    bool StartLinkRoute(const TArray<FVector>& Surface, const TArray<FVector>& Normals, bool bIsSqueeze,
        float HalfHeight = -1.f, bool bHold = false);
    /** Carry on from a hold through these surface points (out of a crawl mouth). */
    bool ResumeHeld(const TArray<FVector>& Surface, const TArray<FVector>& Normals);
    /** Over the furniture in its way (a bed, a table, a chest): up its near side, across its top, down the far side.
     *  A body this big goes over a cluttered floor, not round it. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartOverRoute(const FVector& Toward, bool bTryPounce = false);
    /** Up onto the wall beside it and along it at shoulder height toward the target, then back down: the skitter. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartWallWalk(const FVector& Toward);
    /** Floor clearance above a point (cm), for fitting the capsule into crawl spaces. */
    float ClearanceAt(const FVector& Point) const;
    float GetStandingHalfHeight() const { return SavedHalfHeight; }
    float GetStandingRadius() const { return SavedRadius; }
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartSqueeze(AActor* Entry, AActor* Exit);
    /** Probe a nearby aperture toward the same evidence; every capsule pose must fit. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") bool StartSqueezeToward(const FVector& Toward);
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") void RequestGroundExit();
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") void ResetTraversal();
    UFUNCTION(BlueprintCallable, Category="Hunter|Traversal") void FreezeForCapture();
    void SetAttackPaused(bool bPaused) { bAttackPaused = bPaused; }
    void SetTacticalPaused(bool bPaused) { bTacticalPaused = bPaused; }
    FVector GetRouteStartLocation() const { return History.IsEmpty() ? FVector::ZeroVector : History[0].Position; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsTraversing() const { return bActive; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsSqueezing() const { return bActive && bSqueeze; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsChaseRoute() const { return bActive; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") FName GetRouteKind() const { return RouteKind; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsCurrentPoseClear() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") FVector GetSurfaceNormal() const { return SurfaceNormal; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetTraversalSpeed() const { return ActualSpeed; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetSqueezeAlpha() const { return SqueezeAlpha; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") FVector2D GetRouteClearance() const { return RouteClearance; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetSqueezeSeverity() const { return SqueezeSeverity; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetRouteSpeedScale() const { return RouteSpeedScale; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") float GetTransferAlpha() const { return SegmentAlpha; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsTransferring() const { return bActive && bTransfer; }
    bool IsDirectCeilingTransfer() const
    { return bActive && bTransfer && RouteKind == TEXT("DirectCeiling")
        && SegmentStart.Normal.Z > .5f && GetPendingSurfaceNormal().Z < -.5f; }
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") FVector GetPendingSurfaceNormal() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") FVector GetTravelDirection() const;
    UFUNCTION(BlueprintPure, Category="Hunter|Traversal") bool IsCornerBracing() const
    { return bActive && !bReturning && RouteKind == TEXT("Corner") && !Waypoints.IsEmpty()
        && (Phase == TEXT("Perch") || WaypointIndex == Waypoints.Num() - 1); }
    /** Total authored corner wrist-to-wall distance, including finger clearance, in world centimetres. */
    float GetCornerWristStandOff(int32 HandIndex) const;
    static float GetDirectCeilingWristStandOff(int32 HandIndex);
    /** Returns a traced, reachable candidate on the upcoming support surface. */
    bool GetApproachContact(int32 HandIndex, const FVector& Shoulder, FVector& Point, FVector& Normal) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Traversal") float CrawlSpeed = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Traversal") float CeilingSpeed = 125.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter|Traversal") float SqueezeSpeed = 80.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") FString Phase = TEXT("Ground");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") int32 CompletedTransfers = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") int32 AbortedTransfers = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") int32 TransferGripCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") FString LastRouteFailure;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") FVector RejectedPose = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter|Traversal") int32 RejectedSegment = INDEX_NONE;
private:
    struct FWaypoint
    {
        FVector Position, Normal, SurfacePoint;
        FQuat Rotation;
    };
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<UTallowHunterMotionComponent> Motion;
    TArray<FWaypoint> Waypoints, History;
    FWaypoint SegmentStart;
    TArray<FWaypoint> FurniturePounceExit;
    FVector FurniturePounceTarget = FVector::ZeroVector;
    bool bFurniturePouncePending = false;
    int32 WaypointIndex = 0;
    bool bPerchRoute = false;
    /** The radius a squeezed route is validated at (the capsule narrows with its height). */
    float PoseRadius = -1.f;
    float SegmentAlpha = 0.f, SegmentMetric = 1.f, ReachAge = 0.f, SettleAge = 0.f, BlockedAge = 0.f;
    float TravelSpeed = 0.f, ActualSpeed = 0.f, SqueezeAlpha = 0.f;
    float SavedHalfHeight = 96.f, SavedRadius = 34.f, SavedGravity = 1.f;
    FVector2D RouteClearance = FVector2D::ZeroVector;
    float SqueezeSeverity = 0.f, RouteSpeedScale = 1.f;
    FVector SurfaceNormal = FVector::UpVector;
    FName RouteKind = NAME_None;
    bool bActive = false, bSqueeze = false, bReturning = false, bFrozen = false;
    bool bAttackPaused = false;
    bool bAwaitingAdaptiveFit = false;
    int32 AdaptiveFitStartEvaluation = 0;
    float AdaptiveFitWaitAge = 0.f;
    bool bTacticalPaused = false;
    bool bTransfer = false, bSavedOrient = false, bSavedDesired = false, bSavedYaw = false;
    bool bRestoreCapsulePending = false;
    bool bPounceActive = false, bPounceIgnoredPlayer = false;
    float PounceAge = 0.f, PounceHeightScale = 1.f, PounceDistanceScale = 1.f;
    FVector PounceStart = FVector::ZeroVector;
    FQuat PounceRotation = FQuat::Identity;
    FTransform PounceMeshRelative;
    FTransform PouncePreviousMeshRelative;
    bool bPounceOwnsMeshTransform = false;
    TWeakObjectPtr<AActor> PouncePlayer;
    FVector CornerWallA = FVector::ZeroVector, CornerWallB = FVector::ZeroVector;
    FVector CornerLine = FVector::ZeroVector;
    FTransform CornerPreviousMeshRelative;
    bool bCornerOwnsMeshTransform = false;
    void UpdateCornerMeshTransform();
    void ClearCornerMeshTransform();
    bool PouncePoseClear(float Alpha, FVector* Position = nullptr, FString* Blocker = nullptr) const;
    bool AdvancePounce(float DeltaTime);
    void ClearPounce();
    bool Trace(const FVector& From, const FVector& To, FHitResult& Hit) const;
    bool PoseClear(const FVector& Position, const FQuat& Rotation, float HalfHeight = -1.f, FString* Blocker = nullptr) const;
    bool Reject(const FString& Reason);
    bool BeginRoute(TArray<FWaypoint>&& Points, bool bIsSqueeze, float HalfHeight = -1.f, float Radius = -1.f);
    bool BuildChaseCeilingRoute(const FVector& TargetLocation, bool bPerch, bool bDirectReach);
    FVector2D PassageAt(const FVector& Floor, const FVector& Forward) const;
    void BeginSegment();
    bool Advance(float DeltaTime);
    bool TryBlockedGroundExit();
    void FinishRoute();
    void RestoreCapsule();
    static FQuat SurfaceRotation(const FVector& Forward, const FVector& Normal);
    static float RoutePoseAlpha(float Alpha, const FVector& FromNormal, const FVector& ToNormal);
    static FString TagValue(const AActor* Actor, const FString& Prefix);
};
