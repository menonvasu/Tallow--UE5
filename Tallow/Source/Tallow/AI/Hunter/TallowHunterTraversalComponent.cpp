#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowMimAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ScopeExit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"

namespace {
#include "TallowMimPounceEnvelope.inl"
}

UTallowHunterTraversalComponent::UTallowHunterTraversalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTallowHunterTraversalComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    Motion = GetOwner()->FindComponentByClass<UTallowHunterMotionComponent>();
    if (!Character.IsValid()) return;
    SavedHalfHeight = Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    SavedRadius = Character->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    // Traversal moves first, contacts then evaluate that transform, and finally the mesh poses.
    if (Motion.IsValid()) Motion->AddTickPrerequisiteComponent(this);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->IsEditorOnly()) Character->GetCapsuleComponent()->IgnoreActorWhenMoving(*It, true);
}

FString UTallowHunterTraversalComponent::TagValue(const AActor* Actor, const FString& Prefix)
{
    for (const FName& Tag : Actor->Tags)
        if (Tag.ToString().StartsWith(Prefix)) return Tag.ToString().RightChop(Prefix.Len());
    return FString();
}

FQuat UTallowHunterTraversalComponent::SurfaceRotation(const FVector& Forward, const FVector& Normal)
{
    FVector Tangent = FVector::VectorPlaneProject(Forward, Normal).GetSafeNormal();
    if (Tangent.IsNearlyZero()) Tangent = FVector::VectorPlaneProject(FVector::ForwardVector, Normal).GetSafeNormal();
    if (Tangent.IsNearlyZero()) Tangent = FVector::RightVector;
    return FRotationMatrix::MakeFromXZ(Tangent, Normal).ToQuat();
}

float UTallowHunterTraversalComponent::GetDirectCeilingWristStandOff(int32 HandIndex)
{
    // Measured flat-finger skin depth of the authored direct ceiling grips.
    static const float Depth[10] = {32.883f, 33.260f, 20.f, 20.f, 20.f, 20.f, 20.f, 20.f, 30.555f, 31.629f};
    return HandIndex >= 0 && HandIndex < 10 ? Depth[HandIndex] : 20.f;
}

float UTallowHunterTraversalComponent::RoutePoseAlpha(float Alpha, const FVector& FromNormal, const FVector& ToNormal)
{
    if (FVector::DotProduct(FromNormal, ToNormal) >= .82f) return Alpha;
    // The authored transfer has a front grip, chest pull and rear follow.
    // Motion samples Pull at clip .34 + .61 * Alpha after the Reach hold.
    const bool bCeiling = FromNormal.Z < -.5f || ToNormal.Z < -.5f;
    const float ClipAlpha = .34f + .61f * Alpha;
    return bCeiling ? FMath::SmoothStep(12.f / 35.f, 32.f / 35.f, ClipAlpha)
        : FMath::SmoothStep(12.f / 31.f, 28.f / 31.f, ClipAlpha);
}

bool UTallowHunterTraversalComponent::Trace(const FVector& From, const FVector& To, FHitResult& Hit) const
{
    if (!Character.IsValid()) return false;
    // Use the same actual blocking surfaces as the hands. A WorldStatic-channel
    // simple trace can choose a different hull/response from the visible ceiling
    // that the complex Visibility hand trace must eventually grip.
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterTraversalSurface), true, Character.Get());
    Query.AddIgnoredActors(Character->GetCapsuleComponent()->GetMoveIgnoreActors());
    TArray<AActor*> Attached;
    Character->GetAttachedActors(Attached);
    Query.AddIgnoredActors(Attached);
    return GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
}

bool UTallowHunterTraversalComponent::PoseClear(const FVector& Position, const FQuat& Rotation,
    float HalfHeight, FString* Blocker) const
{
    if (!Character.IsValid()) return false;
    const auto* Capsule = Character->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterTraversalPose), false, Character.Get());
    Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
    TArray<AActor*> Attached;
    Character->GetAttachedActors(Attached);
    Query.AddIgnoredActors(Attached);
    const float Height = HalfHeight > 0.f ? HalfHeight : Capsule->GetScaledCapsuleHalfHeight();
    const auto Shape = FCollisionShape::MakeCapsule(FMath::Min(PoseRadius > 0.f ? PoseRadius
        : Capsule->GetScaledCapsuleRadius(), Height), Height);
    if (!Blocker)
        return !GetWorld()->OverlapBlockingTestByProfile(Position, Rotation, Capsule->GetCollisionProfileName(), Shape, Query);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByProfile(Hits, Position, Rotation, Capsule->GetCollisionProfileName(), Shape, Query);
    for (const auto& Hit : Hits) if (Hit.bBlockingHit)
    {
        *Blocker = GetNameSafe(Hit.GetActor()) + TEXT(".") + GetNameSafe(Hit.GetComponent());
        return false;
    }
    return true;
}

bool UTallowHunterTraversalComponent::Reject(const FString& Reason)
{ LastRouteFailure = Reason; return false; }

bool UTallowHunterTraversalComponent::IsCurrentPoseClear() const
{
    return Character.IsValid() && PoseClear(Character->GetActorLocation(), Character->GetActorQuat());
}

bool UTallowHunterTraversalComponent::BeginRoute(TArray<FWaypoint>&& Points, bool bIsSqueeze, float HalfHeight, float Radius)
{
    if (bActive || bFrozen || !Character.IsValid() || Points.IsEmpty()) return Reject(TEXT("Route state unavailable"));
    const float TestHeight = HalfHeight > 0.f ? HalfHeight : Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float TestRadius = Radius > 0.f ? Radius : FMath::Min(SavedRadius, TestHeight);
    PoseRadius = TestRadius;
    ON_SCOPE_EXIT { PoseRadius = -1.f; };
    const FVector Original = Character->GetActorLocation();
    const FVector Start = Original - Character->GetActorUpVector()
        * (Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - TestHeight);
    for (int32 I = 0; I <= 12; ++I)
        if (!PoseClear(FMath::Lerp(Original, Start, I / 12.f), Character->GetActorQuat(), TestHeight))
            return Reject(TEXT("Cannot fold at the aperture entry"));
    FWaypoint Previous{Start, Character->GetActorUpVector(),
        Start - Character->GetActorUpVector() * TestHeight, Character->GetActorQuat()};
    if (bIsSqueeze)
    {
        const auto* Mim = Cast<UTallowMimAnimInstance>(Character->GetMesh()->GetAnimInstance());
        if (Mim && Mim->HasAdaptiveFitLayer())
        {
            RouteClearance = FVector2D(400.f, 400.f);
            FVector Last = Previous.SurfacePoint;
            for (const auto& Point : Points)
            {
                const FVector Direction = (Point.SurfacePoint - Last).GetSafeNormal2D();
                const int32 Checks = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Last, Point.SurfacePoint) / 12.f));
                for (int32 I = 0; I <= Checks; ++I)
                {
                    const FVector2D Space = PassageAt(FMath::Lerp(Last, Point.SurfacePoint, float(I) / Checks), Direction);
                    RouteClearance.X = FMath::Min(RouteClearance.X, Space.X);
                    RouteClearance.Y = FMath::Min(RouteClearance.Y, Space.Y);
                }
                Last = Point.SurfacePoint;
            }
            if (RouteClearance.X < 82.f || RouteClearance.Y < 85.f)
                return Reject(FString::Printf(TEXT("Adaptive body requires 82x85cm; measured %.1fx%.1fcm"),
                    RouteClearance.X, RouteClearance.Y));
        }
    }
    // Validate the entire rotational capsule path, including door heads and inside corners.
    int32 Segment = 0;
    for (const auto& Point : Points)
    {
        const int32 Samples = FMath::Max(12, FMath::CeilToInt(FVector::Dist(Previous.Position, Point.Position) / 14.f));
        for (int32 I = 1; I <= Samples; ++I)
        {
            const float T = float(I) / Samples;
            const bool bNewSurface = FVector::DotProduct(Previous.Normal, Point.Normal) < .82f;
            const float Travel = RoutePoseAlpha(T, Previous.Normal, Point.Normal);
            const float R = bNewSurface ? Travel : FMath::SmoothStep(0.f, 1.f, T);
            const FVector Position = FMath::Lerp(Previous.Position, Point.Position, Travel);
            const FQuat Rotation = FQuat::Slerp(Previous.Rotation, Point.Rotation, R).GetNormalized();
            if (!PoseClear(Position, Rotation, TestHeight))
            {
                FString Blocker;
                PoseClear(Position, Rotation, TestHeight, &Blocker);
                RejectedPose = Position; RejectedSegment = Segment;
                return Reject(FString::Printf(TEXT("Capsule path segment %d at %.3f blocked by %s"),
                    Segment, T, *Blocker));
            }
        }
        Previous = Point; ++Segment;
    }
    auto* Movement = Character->GetCharacterMovement();
    SavedGravity = Movement->GravityScale;
    bSavedOrient = Movement->bOrientRotationToMovement;
    bSavedDesired = Movement->bUseControllerDesiredRotation;
    bSavedYaw = Character->bUseControllerRotationYaw;
    if (auto* AI = Cast<AAIController>(Character->GetController())) AI->StopMovement();
    Movement->StopMovementImmediately();
    Movement->GravityScale = 0.f;
    Movement->bOrientRotationToMovement = Movement->bUseControllerDesiredRotation = false;
    Character->bUseControllerRotationYaw = false;
    Movement->SetMovementMode(MOVE_None);
    if (HalfHeight > 0.f)
    {
        Character->GetCapsuleComponent()->SetCapsuleSize(TestRadius, HalfHeight, true);
        Character->SetActorLocation(Start, false);
        bRestoreCapsulePending = true;
    }
    RouteClearance = FVector2D::ZeroVector;
    SqueezeSeverity = 0.f; RouteSpeedScale = 1.f;
    if (bIsSqueeze)
    {
        RouteClearance = FVector2D(400.f, 400.f);
        FVector PreviousFloor = Start - FVector::UpVector * (TestHeight + 4.f);
        for (const FWaypoint& Point : Points)
        {
            const FVector Forward = (Point.SurfacePoint - PreviousFloor).GetSafeNormal2D();
            const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(PreviousFloor, Point.SurfacePoint) / 12.f));
            for (int32 I = 0; I <= Samples; ++I)
            {
                const FVector2D Space = PassageAt(FMath::Lerp(PreviousFloor, Point.SurfacePoint, float(I) / Samples), Forward);
                RouteClearance.X = FMath::Min(RouteClearance.X, Space.X);
                RouteClearance.Y = FMath::Min(RouteClearance.Y, Space.Y);
            }
            PreviousFloor = Point.SurfacePoint;
        }
        const float HeightFold = FMath::Clamp((180.f - RouteClearance.Y) / 122.f, 0.f, 1.f);
        const float WidthFold = FMath::Clamp((160.f - RouteClearance.X) / 102.f, 0.f, 1.f);
        SqueezeSeverity = FMath::Max(HeightFold, WidthFold);
        RouteSpeedScale = FMath::Lerp(1.f, .16f, SqueezeSeverity);
    }
    Waypoints = MoveTemp(Points);
    WaypointIndex = 0;
    bActive = true;
    bSqueeze = bIsSqueeze;
    const auto* FitAnimation = Cast<UTallowMimAnimInstance>(Character->GetMesh()->GetAnimInstance());
    bAwaitingAdaptiveFit = bIsSqueeze && FitAnimation && FitAnimation->HasAdaptiveFitLayer();
    AdaptiveFitStartEvaluation = FitAnimation ? FitAnimation->ContactEvaluationSerial : 0;
    AdaptiveFitWaitAge = 0.f;
    RouteKind = bIsSqueeze ? TEXT("Crawl") : TEXT("Surface");
    bReturning = false;
    TravelSpeed = ActualSpeed = 0.f;
    History.Reset();
    History.Add({Character->GetActorLocation(), FVector::UpVector,
        Character->GetActorLocation() - FVector::UpVector * TestHeight, Character->GetActorQuat()});
    BeginSegment();
    return true;
}

void UTallowHunterTraversalComponent::BeginSegment()
{
    if (!Waypoints.IsValidIndex(WaypointIndex))
    {
        if (bPerchRoute && !bReturning) { Phase = TEXT("Perch"); TravelSpeed = ActualSpeed = 0.f; return; }
        FinishRoute(); return;
    }
    const auto& Target = Waypoints[WaypointIndex];
    SegmentStart = {Character->GetActorLocation(), Character->GetActorUpVector(),
        Character->GetActorLocation() - Character->GetActorUpVector()
        * Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Character->GetActorQuat()};
    SegmentAlpha = ReachAge = SettleAge = BlockedAge = 0.f;
    const float Angle = SegmentStart.Rotation.AngularDistance(Target.Rotation);
    SegmentMetric = FMath::Max(1.f, FMath::Max(FVector::Dist(SegmentStart.Position, Target.Position), Angle * 72.f));
    bTransfer = FVector::DotProduct(SegmentStart.Normal, Target.Normal) < .82f;
    Phase = bTransfer ? TEXT("Reach") : TEXT("Travel");
    if (bTransfer) TravelSpeed = 0.f;
}

FVector UTallowHunterTraversalComponent::GetPendingSurfaceNormal() const
{
    return bActive && Waypoints.IsValidIndex(WaypointIndex) ? Waypoints[WaypointIndex].Normal : SurfaceNormal;
}

FVector UTallowHunterTraversalComponent::GetTravelDirection() const
{
    return bActive && Character.IsValid() && Waypoints.IsValidIndex(WaypointIndex)
        ? (Waypoints[WaypointIndex].Position - Character->GetActorLocation()).GetSafeNormal()
        : FVector::ZeroVector;
}

float UTallowHunterTraversalComponent::GetCornerWristStandOff(int32 HandIndex) const
{
    // Exact locked-rig palm depth + normal plant allowance + measured skin padding.
    static const float StandOff[10] = {32.3830f, 32.7596f, 12.2674f, 11.9525f, 19.4770f,
        20.8979f, 27.8215f, 27.8150f, 29.5553f, 30.6291f};
    if (HandIndex < 0 || HandIndex >= 10) return 0.f;
    const FVector Scale = Character.IsValid() && Character->GetMesh()
        ? Character->GetMesh()->GetRelativeScale3D().GetAbs() : FVector::OneVector;
    return StandOff[HandIndex] * .5f * (Scale.Y + Scale.Z);
}

bool UTallowHunterTraversalComponent::GetApproachContact(
    int32 HandIndex, const FVector& Shoulder, FVector& Point, FVector& Normal) const
{
    if (IsCornerBracing())
    {
        const int32 Pair = HandIndex / 2;
        if (HandIndex < 0 || HandIndex >= 10 || (Pair != 0 && Pair != 3 && Pair != 4)) return false;
        const auto& Target = Waypoints.Last();
        const auto* Mesh = Character->GetMesh();
        const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
        if (!Asset) return false;
        const auto& Skeleton = Asset->GetRefSkeleton();
        const auto& Ref = Skeleton.GetRefBonePose();
        auto ReferencePoint = [&](FName Bone)
        {
            int32 Index = Skeleton.FindBoneIndex(Bone);
            if (Index == INDEX_NONE) return FVector::ZeroVector;
            FTransform At = Ref[Index];
            for (int32 Parent = Skeleton.GetParentIndex(Index); Parent != INDEX_NONE;
                Parent = Skeleton.GetParentIndex(Parent)) At = At * Ref[Parent];
            return At.GetLocation();
        };
        // Stable anatomical sides survive both the import's handedness and
        // an old pinned pose. Animated shoulders can both sit near wall A
        // during the approach, which must not assign every hand to that wall.
        const FVector ReferenceLeft = ReferencePoint(TEXT("arm01_upper_l")) - ReferencePoint(TEXT("arm01_upper_r"));
        // Corner alone uses the same imported +Y -> authored +X root yaw
        // normalization as AnimInstance, before acquiring either real wall.
        const FVector Left = Mesh->GetComponentTransform().TransformVectorNoScale(
            FQuat(FVector::UpVector, -PI * .5f).RotateVector(ReferenceLeft));
        const bool bLeftOnA = FVector::DotProduct(Left, CornerWallA) < FVector::DotProduct(Left, CornerWallB);
        Normal = ((HandIndex % 2 == 0) == bLeftOnA) ? CornerWallA : CornerWallB;
        const FVector Tangent = FVector::VectorPlaneProject(Target.Normal, Normal).GetSafeNormal();
        // The six source grips, in metres before the pure12cm up-wall reframe.
        static const float Span[10] = {1.02f, .90f, 0.f, 0.f, 0.f, 0.f, 1.20f, 1.08f, .80f, .75f};
        static const float Height[10] = {.82f, .72f, 0.f, 0.f, 0.f, 0.f, -.73f, -.88f, -.88f, -.98f};
        const FVector Scale = Mesh->GetRelativeScale3D().GetAbs();
        const FVector Desired = CornerLine + Tangent * (Span[HandIndex] * 100.f * FMath::Sqrt(2.f)
            * .5f * (Scale.Y + Scale.Z)) + FVector::UpVector * (Height[HandIndex] * 100.f * Scale.X);
        FHitResult Hit, Occluder;
        if (!Trace(Desired + Normal * 45.f, Desired - Normal * 35.f, Hit)
            || FVector::DotProduct(Hit.ImpactNormal, Normal) < .9f) return false;
        if (Trace(Shoulder, Hit.ImpactPoint + Normal * 3.f, Occluder)
            && FVector::Dist(Occluder.ImpactPoint, Hit.ImpactPoint) > 12.f) return false;
        Point = Hit.ImpactPoint + Hit.ImpactNormal * GetCornerWristStandOff(HandIndex);
        Normal = Hit.ImpactNormal; return true;
    }
    if (!bActive || !bTransfer || !Waypoints.IsValidIndex(WaypointIndex)) return false;
    // The direct reach replants its front pair at the end of the roll and
    // finishes the rear hand-off on its final frame. Keep those real roof
    // targets through settlement; every plant still needs its authored curve.
    if (SegmentAlpha > .90f && !IsDirectCeilingTransfer()) return false;
    const auto& Target = Waypoints[WaypointIndex];
    // Returning to a lower surface starts with the rear supports. A furniture
    // ledge still uses the front hands even though its normal turns upward.
    const bool bReverse = RouteKind != TEXT("Furniture") && RouteKind != TEXT("Climb")
        && Target.Normal.Z > SegmentStart.Normal.Z + .5f;
    const int32 Pair = HandIndex / 2, Lead = bReverse ? 4 : 0, Follow = bReverse ? 3 : 1;
    if (HandIndex < 0 || HandIndex >= 10) return false;
    if (RouteKind == TEXT("DirectCeiling") && SegmentStart.Normal.Z > .5f && Target.Normal.Z < -.5f)
    {
        // Rear hands take the ceiling before the front pair changes sides
        // during the roll. The authored release can then uncross those arms.
        if (Pair == 0)
        {
            if (SegmentAlpha > .72f && SegmentAlpha < .88f) return false;
        }
        // Begin the rear reach before its plant so low ceilings can redirect
        // the free fingers. Contact timing still controls when they bear weight.
        else if (Pair != 4 || SegmentAlpha < .35f) return false;
    }
    else if ((Pair != Lead && Pair != Follow)
        || (Pair == Follow && SegmentAlpha < .22f
            && !((RouteKind == TEXT("DirectCeiling") || RouteKind == TEXT("WallWalk"))
                && FMath::Abs(SegmentStart.Normal.Z) < .5f
                && Target.Normal.Z > .5f))) return false;
    if (RouteKind == TEXT("Furniture") && FMath::Abs(Target.Normal.Z) < .1f
        && Waypoints.IsValidIndex(WaypointIndex - 1)
        && Waypoints[WaypointIndex - 1].Normal.Z > .5f && Waypoints[WaypointIndex - 1].Normal.Z < .9f)
    {
        // During the far-edge roll the shoulders are still above the top.
        // Their horizontal projection misses the finite wall below it. Take
        // a real grip just under that measured ledge before lowering the mass.
        const FVector Wall = Target.Normal.GetSafeNormal2D();
        const FVector AlongEdge = FVector::CrossProduct(FVector::UpVector, Wall);
        FVector Desired = Target.SurfacePoint
            + AlongEdge * FVector::DotProduct(Shoulder - Target.SurfacePoint, AlongEdge);
        Desired.Z = Waypoints[WaypointIndex - 1].SurfacePoint.Z - 8.f;
        FHitResult Face, Occluder;
        if (Trace(Desired + Wall * 45.f, Desired - Wall * 35.f, Face)
            && FVector::DotProduct(Face.ImpactNormal, Wall) > .9f)
        {
            if (Trace(Shoulder, Face.ImpactPoint + Face.ImpactNormal * 3.f, Occluder)
                && FVector::Dist(Occluder.ImpactPoint, Face.ImpactPoint) > 12.f) return false;
            Point = Face.ImpactPoint + Face.ImpactNormal * 1.5f;
            Normal = Face.ImpactNormal;
            return true;
        }
    }
    if (RouteKind == TEXT("Furniture") && Target.Normal.Z > .5f && Target.Normal.Z < .9f)
    {
        // The actor rounds the ledge on a bisector; the hand grips its actual
        // horizontal top. Projecting onto the bisector can miss both faces or
        // keep an old wall pin when a new top support must take the weight.
        const FVector Wall = Target.Normal.GetSafeNormal2D();
        const FVector AlongEdge = FVector::CrossProduct(FVector::UpVector, Wall);
        const FVector Desired = Target.SurfacePoint - Wall * 3.f
            + AlongEdge * FVector::DotProduct(Shoulder - Target.SurfacePoint, AlongEdge);
        FHitResult Top, Occluder;
        if (Trace(Desired + FVector::UpVector * 45.f, Desired - FVector::UpVector * 35.f, Top)
            && Top.ImpactNormal.Z > .9f)
        {
            if (Trace(Shoulder, Top.ImpactPoint + Top.ImpactNormal * 3.f, Occluder)
                && FVector::Dist(Occluder.ImpactPoint, Top.ImpactPoint) > 12.f) return false;
            Point = Top.ImpactPoint + Top.ImpactNormal * 1.5f;
            Normal = Top.ImpactNormal;
            return true;
        }
    }
    Normal = Target.Normal;
    const FVector Forward = Target.Rotation.GetForwardVector();
    const FVector Projection = Shoulder - Normal * FVector::DotProduct(Shoulder - Target.SurfacePoint, Normal);
    const FVector Desired = Projection + Forward * (HandIndex % 2 == 0 ? 18.f : 7.f);
    FHitResult Hit;
    if (!Trace(Desired + Normal * 45.f, Desired - Normal * 35.f, Hit)
        || FVector::DotProduct(Hit.ImpactNormal, Normal) < .7f) return false;
    // A wrist may not grab the far side of an obstructing wall or prop.
    FHitResult Occluder;
    if (Trace(Shoulder, Hit.ImpactPoint + Normal * 3.f, Occluder)
        && FVector::Dist(Occluder.ImpactPoint, Hit.ImpactPoint) > 12.f) return false;
    const bool bDirectGrip = RouteKind == TEXT("DirectCeiling")
        && SegmentStart.Normal.Z > .5f && Target.Normal.Z < -.5f;
    Point = Hit.ImpactPoint + Hit.ImpactNormal * (bDirectGrip
        ? GetDirectCeilingWristStandOff(HandIndex) - 6.5f : 1.5f);
    Normal = Hit.ImpactNormal;
    return true;
}

bool UTallowHunterTraversalComponent::Advance(float DeltaTime)
{
    if (!Waypoints.IsValidIndex(WaypointIndex)) return true;
    if (bSqueeze && !bReturning && Motion.IsValid() && Motion->UsesMim())
    {
        const auto* Mim = Cast<UTallowMimAnimInstance>(Character->GetMesh()->GetAnimInstance());
        if (Mim && Mim->HasAdaptiveFitLayer())
        {
            const bool bFreshPose = !bAwaitingAdaptiveFit
                || Mim->ContactEvaluationSerial > AdaptiveFitStartEvaluation + 1;
            if (bFreshPose) bAwaitingAdaptiveFit = false;
            const bool bNeedsFold = !Motion->GetFitAxisWeights().IsNearlyZero(.01f);
            if (!bFreshPose || (bNeedsFold && !Mim->AdaptiveFitApplied)
                || !Mim->AdaptiveFitWithinLimits || !Motion->IsFitPassable())
            {
                // Route movement owns the capsule, so stopping the AI alone
                // is insufficient. Wait for the new route's evaluated shape,
                // with its gait/contact clocks still running at the entrance.
                ActualSpeed = TravelSpeed = 0.f;
                AdaptiveFitWaitAge += DeltaTime;
                if (AdaptiveFitWaitAge < .85f) return true;
                return Reject(TEXT("Adaptive pose cannot clear the complete squeeze footprint"));
            }
            AdaptiveFitWaitAge = 0.f;
        }
    }
    const auto& Target = Waypoints[WaypointIndex];
    const FVector Before = Character->GetActorLocation();
    const FQuat BeforeRotation = Character->GetActorQuat();
    TransferGripCount = Motion.IsValid() ? Motion->CountContactsOnSurface(Target.Normal) : 0;
    if (Phase == TEXT("Reach"))
    {
        ReachAge += DeltaTime;
        ActualSpeed = 0.f;
        const bool bNoHands = Motion.IsValid() && Motion->IsProxy() && !Motion->UsesMim();
        const bool bDirectCeiling = RouteKind == TEXT("DirectCeiling")
            && SegmentStart.Normal.Z > .5f && Target.Normal.Z < -.5f;
        const int32 NeededGrips = bDirectCeiling ? 2 : 1;
        if ((bNoHands && ReachAge >= .12f)
            || (ReachAge >= (bDirectCeiling ? .6f : .28f) && TransferGripCount >= NeededGrips))
        {
            Phase = TEXT("Pull");
            ++CompletedTransfers;
        }
        else if (ReachAge > 2.2f)
        {
            return Reject(FString::Printf(TEXT("No physical new-surface grip on segment %d"), WaypointIndex));
        }
        else return true;
    }
    const float Fit = Motion.IsValid() ? Motion->GetFitSpeed() : 1.f;
    const float SurfaceSpeed = (bSqueeze ? SqueezeSpeed : Target.Normal.Z < -.5f ? CeilingSpeed : CrawlSpeed)
        * FMath::Min(RouteSpeedScale, Fit);
    const float Support = Motion.IsValid() && (!Motion->IsProxy() || Motion->UsesMim()) ? Motion->GetSupportStrength() : 1.f;
    const float Pull = FMath::Lerp(.52f, 1.f, Support);
    const float Remaining = (1.f - SegmentAlpha) * SegmentMetric;
    const bool bStopAhead = !Waypoints.IsValidIndex(WaypointIndex + 1)
        || FVector::DotProduct(Target.Normal, Waypoints[WaypointIndex + 1].Normal) < .82f;
    const float Brake = bStopAhead ? FMath::Sqrt(FMath::Max(0.f, 2.f * 520.f * Remaining)) : MAX_flt;
    const float DesiredSpeed = FMath::Min(SurfaceSpeed * Pull, Brake);
    TravelSpeed = FMath::FInterpConstantTo(TravelSpeed, DesiredSpeed, DeltaTime, FMath::Max(280.f, SurfaceSpeed * 1.6f));
    SegmentAlpha = FMath::Min(1.f, SegmentAlpha + FMath::Max(TravelSpeed, 6.f) * DeltaTime / SegmentMetric);
    const float Travel = RoutePoseAlpha(SegmentAlpha, SegmentStart.Normal, Target.Normal);
    const FVector Position = FMath::Lerp(SegmentStart.Position, Target.Position, Travel);
    const FQuat Rotation = FQuat::Slerp(SegmentStart.Rotation, Target.Rotation,
        bTransfer ? Travel : FMath::SmoothStep(0.f, 1.f, SegmentAlpha)).GetNormalized();
    for (int32 I = 1; I <= 4; ++I)
        if (!PoseClear(FMath::Lerp(Before, Position, I / 4.f),
            FQuat::Slerp(BeforeRotation, Rotation, I / 4.f).GetNormalized()))
            return Reject(FString::Printf(TEXT("Dynamic capsule obstruction on segment %d"), WaypointIndex));
    FHitResult Hit;
    Character->GetCapsuleComponent()->MoveComponent(Position - Before, Rotation, true, &Hit);
    if (Hit.bBlockingHit) return Reject(TEXT("Sweep blocked by ") + GetNameSafe(Hit.GetActor()));
    ActualSpeed = FVector::Dist(Before, Character->GetActorLocation()) / FMath::Max(DeltaTime, SMALL_NUMBER);
    SurfaceNormal = Character->GetActorUpVector();
    if (!bReturning && (History.IsEmpty() || FVector::Dist(History.Last().Position, Position) > 18.f))
        History.Add({Position, SurfaceNormal, Position - SurfaceNormal
            * Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Rotation});
    if (SegmentAlpha >= 1.f)
    {
        // Let the new supports take the load before continuing along another surface.
        SettleAge += DeltaTime;
        Phase = TEXT("Settle");
        if (SettleAge >= (bTransfer ? .16f : 0.f))
        {
            ++WaypointIndex;
            BeginSegment();
        }
    }
    return true;
}

bool UTallowHunterTraversalComponent::StartCornerAmbushRoute(
    const FVector& Corner, const FVector& WallNormalA, const FVector& WallNormalB)
{
    if (bActive || bFrozen || !Character.IsValid()) return Reject(TEXT("Corner route state unavailable"));
    const FVector A = WallNormalA.GetSafeNormal2D(), B = WallNormalB.GetSafeNormal2D();
    if (A.IsNearlyZero() || B.IsNearlyZero() || FVector::DotProduct(A, B) < -.005f
        || FVector::DotProduct(A, B) > .12f) return Reject(TEXT("Corner needs two nearly perpendicular walls"));
    const FVector Inward = (A + B).GetSafeNormal(), Along = FVector::VectorPlaneProject(Inward, A).GetSafeNormal();
    FHitResult Floor;
    if (!Trace(Corner + Inward * 150.f, Corner + Inward * 150.f - FVector::UpVector * 550.f, Floor)
        || Floor.ImpactNormal.Z < .8f || Corner.Z - Floor.ImpactPoint.Z < 175.f)
        return Reject(TEXT("Corner lacks a supporting approach floor"));
    FHitResult Roof;
    if (Trace(Corner + Inward * 140.f, Corner + Inward * 140.f + FVector::UpVector * 500.f, Roof)
        && Roof.ImpactPoint.Z - Corner.Z < 110.f)
        return Reject(TEXT("Corner lacks head and front-hand clearance"));
    for (const FVector& Normal : {A, B})
    {
        const FVector Tangent = FVector::VectorPlaneProject(Inward, Normal).GetSafeNormal();
        for (float Span : {45.f, 110.f, 175.f}) for (float Z : {-145.f, -45.f, 75.f})
        {
            const FVector Surface = Corner + Tangent * Span + FVector::UpVector * Z;
            FHitResult Brace;
            if (!Trace(Surface + Normal * 35.f, Surface - Normal * 25.f, Brace)
                || FVector::DotProduct(Brace.ImpactNormal, Normal) < .9f
                || FVector::Dist(Brace.ImpactPoint, Surface) > 10.f)
                return Reject(TEXT("Corner lacks a continuous two-wall brace"));
        }
    }
    const FVector Across = FVector::CrossProduct(Inward, FVector::UpVector);
    for (float Z : {-145.f, -95.f, -45.f, 5.f, 55.f, 95.f}) for (float Side : {-55.f, 0.f, 55.f})
    {
        FHitResult Room;
        const FVector Slice = Corner + FVector::UpVector * Z + Across * Side;
        if (Trace(Slice + Inward * (FMath::Abs(Side) + 12.f), Slice + Inward * 350.f, Room))
            return Reject(TEXT("Corner has no room for the hanging body and free arms"));
    }
    const float HH = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Base(Corner.X, Corner.Y, Floor.ImpactPoint.Z), Up = FVector::UpVector;
    const FVector WallBase = Base + Along * 160.f;
    TArray<FWaypoint> Points;
    auto Add = [&](const FVector& Surface, const FVector& Normal, const FVector& Forward, float Offset)
    { Points.Add({Surface + Normal * Offset, Normal, Surface, SurfaceRotation(Forward, Normal)}); };
    Add(WallBase + A * (HH + 28.f), Up, -A, HH + 4.f);
    Add(WallBase + Up * 150.f, A, Up, HH + 10.f);
    Add(FVector(WallBase.X, WallBase.Y, Corner.Z), A, Up, HH + 10.f);
    // A diagonal capsule needs extra radial clearance from both walls. The
    // hands still grip the two traced planes; the bisector is never a wall.
    const float DiagonalOffset = HH + Radius * (FMath::Sqrt(2.f) - 1.f) + 6.f;
    Add(Corner, Inward, Up, DiagonalOffset);
    if (!BeginRoute(MoveTemp(Points), false)) return false;
    CornerWallA = A; CornerWallB = B; CornerLine = Corner;
    RouteKind = TEXT("Corner"); bPerchRoute = true;
    return true;
}

void UTallowHunterTraversalComponent::ClearPounce()
{
    if (bPounceOwnsMeshTransform && Character.IsValid() && Character->GetMesh())
        Character->GetMesh()->SetRelativeTransform(PouncePreviousMeshRelative);
    bPounceOwnsMeshTransform = false;
    if (bPounceIgnoredPlayer && Character.IsValid() && PouncePlayer.IsValid())
        Character->GetCapsuleComponent()->IgnoreActorWhenMoving(PouncePlayer.Get(), false);
    bPounceActive = bPounceIgnoredPlayer = false;
    PouncePlayer.Reset();
}

void UTallowHunterTraversalComponent::ClearCornerMeshTransform()
{
    if (bCornerOwnsMeshTransform && Character.IsValid() && Character->GetMesh())
        Character->GetMesh()->SetRelativeTransform(CornerPreviousMeshRelative);
    bCornerOwnsMeshTransform = false;
}

void UTallowHunterTraversalComponent::UpdateCornerMeshTransform()
{
    if (!IsCornerBracing() || !IsHolding())
    {
        ClearCornerMeshTransform();
        return;
    }
    auto* Mesh = Character->GetMesh();
    if (!bCornerOwnsMeshTransform)
    {
        CornerPreviousMeshRelative = Mesh->GetRelativeTransform();
        bCornerOwnsMeshTransform = true;
    }
    // This separately checked pose spans both real walls at full authored size.
    // Its origin is the corner line, not the capsule's diagonal wall standoff.
    Mesh->SetRelativeTransform(FTransform(FQuat::Identity,
        Character->GetActorTransform().InverseTransformPosition(CornerLine), FVector::OneVector));
}

bool UTallowHunterTraversalComponent::PouncePoseClear(float Alpha, FVector* Position, FString* Blocker) const
{
    if (!Character.IsValid()) return false;
    const float Frame = FMath::Clamp(Alpha, 0.f, 1.f) * 33.f;
    const int32 A = FMath::FloorToInt(Frame), B = FMath::Min(33, A + 1);
    const float U = Frame - A;
    FVector Delta = FMath::Lerp(MimPounceFrames[A].Delta, MimPounceFrames[B].Delta, U);
    Delta.X *= PounceDistanceScale; Delta.Y *= FMath::Abs(PounceMeshRelative.GetScale3D().Y);
    Delta.Z *= PounceHeightScale;
    const FVector At = PounceStart + PounceRotation.RotateVector(Delta);
    if (Position) *Position = At;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimPounceBody), false, Character.Get());
    Query.AddIgnoredActors(Character->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (PouncePlayer.IsValid()) Query.AddIgnoredActor(PouncePlayer.Get()); // capture owns player contact
    TArray<AActor*> Attached; Character->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    auto Clear = [&](const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape)
    {
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByProfile(Hits, Center, Rotation,
            Character->GetCapsuleComponent()->GetCollisionProfileName(), Shape, Query);
        for (const auto& Hit : Hits) if (Hit.bBlockingHit)
        {
            if (Blocker) *Blocker = GetNameSafe(Hit.GetActor()) + TEXT(".") + GetNameSafe(Hit.GetComponent());
            return false;
        }
        return true;
    };
    const auto* Capsule = Character->GetCapsuleComponent();
    if (!Clear(At, PounceRotation, FCollisionShape::MakeCapsule(
        Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()))) return false;
    const FTransform Actor(PounceRotation, At);
    const FVector Scale = PounceMeshRelative.GetScale3D().GetAbs();
    const FQuat Rotation = PounceRotation * PounceMeshRelative.GetRotation();
    for (int32 I = 0; I < 6; ++I)
    {
        const FVector Min = FMath::Lerp(MimPounceFrames[A].Body[I].Min, MimPounceFrames[B].Body[I].Min, U);
        const FVector Max = FMath::Lerp(MimPounceFrames[A].Body[I].Max, MimPounceFrames[B].Body[I].Max, U);
        // Imported +Y forward is normalized -90 degrees for this action.
        // Our canonical source-forward boxes therefore mirror their side Y.
        FVector CenterLocal = (Min + Max) * .5f; CenterLocal.Y *= -1.f;
        const FVector Center = Actor.TransformPosition(PounceMeshRelative.TransformPosition(CenterLocal));
        if (!Clear(Center, Rotation, FCollisionShape::MakeBox((Max - Min) * .5f * Scale))) return false;
    }
    return true;
}

bool UTallowHunterTraversalComponent::AdvancePounce(float DeltaTime)
{
    // The checked source pose uses a floor origin. Rest-mesh fitting is not
    // part of this authored trajectory and must not shift a hand into its ledge.
    Character->GetMesh()->SetRelativeTransform(PounceMeshRelative);
    constexpr float Duration = 33.f / 24.f;
    const float Next = FMath::Min(Duration, PounceAge + DeltaTime);
    FVector At; FString Blocker;
    if (!PouncePoseClear(Next / Duration, &At, &Blocker))
    {
        LastRouteFailure = TEXT("Pounce dynamically blocked by ") + Blocker;
        // Remain at the last checked pose. Before release the climb history is
        // still valid; during flight only a verified clear vertical exit may run.
        const bool bBeforeRelease = PounceAge < 7.f / 24.f;
        ClearPounce(); ++AbortedTransfers;
        if (bBeforeRelease) RequestGroundExit();
        else
        {
            Phase = TEXT("Blocked");
            if (!TryBlockedGroundExit()) { bPerchRoute = false; FinishRoute(); }
        }
        return false;
    }
    const FVector Before = Character->GetActorLocation();
    FHitResult Hit;
    Character->GetCapsuleComponent()->MoveComponent(At - Before, PounceRotation, true, &Hit);
    if (Hit.bBlockingHit)
    {
        LastRouteFailure = TEXT("Pounce sweep blocked by ") + GetNameSafe(Hit.GetActor());
        ClearPounce(); ++AbortedTransfers; Phase = TEXT("Blocked");
        if (!TryBlockedGroundExit()) { bPerchRoute = false; FinishRoute(); }
        return false;
    }
    PounceAge = Next;
    ActualSpeed = FVector::Dist(Before, At) / FMath::Max(DeltaTime, SMALL_NUMBER);
    TravelSpeed = ActualSpeed;
    Phase = PounceAge < 7.f / 24.f ? TEXT("PouncePrepare")
        : PounceAge < 25.f / 24.f ? TEXT("PounceFlight") : TEXT("PounceCatch");
    SurfaceNormal = FVector::UpVector;
    if (PounceAge >= Duration)
    {
        FinishRoute();
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    return true;
}

void UTallowHunterTraversalComponent::PounceToward(const FVector& Target)
{
    if (!bActive || bFrozen || bPounceActive || !Character.IsValid()) return;
    const bool bHanging = Character->GetActorUpVector().Z < -.5f;
    FVector Facing = Target.IsZero() ? Character->GetActorForwardVector() : Target - Character->GetActorLocation();
    Facing.Z = 0.f;
    if (Facing.IsNearlyZero()) Facing = FVector::ForwardVector;
    const float Distance = Facing.Size(); Facing /= Distance;
    if (bHanging)
    {
        // The ceiling drop retains its separate validated descent/capture path.
        bPerchRoute = false; FinishRoute(); RouteKind = TEXT("Drop");
        Character->SetActorRotation(FRotator(0.f, Facing.Rotation().Yaw, 0.f));
        const float Out = Target.IsZero() ? 0.f : FMath::Clamp(Distance * 2.f, 0.f, 300.f);
        Character->LaunchCharacter(Facing * Out + FVector(0.f, 0.f, -150.f), true, true);
        return;
    }
    auto BackDown = [&](const FString& Reason)
    { LastRouteFailure = Reason + TEXT("; using the checked climb-down route"); RequestGroundExit(); };
    if (Character->GetActorUpVector().Z < .85f)
    { BackDown(TEXT("Pounce requires an upright departure ledge")); return; }
    if (FVector::DotProduct(Facing, Character->GetActorForwardVector().GetSafeNormal2D()) < .94f)
    { BackDown(TEXT("Pounce target is outside the prepared forward corridor")); return; }
    PounceStart = Character->GetActorLocation();
    PounceRotation = FRotator(0.f, Facing.Rotation().Yaw, 0.f).Quaternion();
    PouncePreviousMeshRelative = Character->GetMesh()->GetRelativeTransform();
    PounceMeshRelative = PouncePreviousMeshRelative;
    // Three centimetres preserve the imported thumb skin's ledge clearance.
    PounceMeshRelative.SetTranslation(FVector(0.f, 0.f,
        3.f - Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
    PounceDistanceScale = FMath::Abs(PounceMeshRelative.GetScale3D().X);
    PouncePlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimPounceFloor), true, Character.Get());
    Query.AddIgnoredActors(Character->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (PouncePlayer.IsValid()) Query.AddIgnoredActor(PouncePlayer.Get());
    TArray<AActor*> Attached; Character->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    auto Floor = [&](const FVector& From, const FVector& To, FHitResult& Hit)
    { return GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query) && Hit.ImpactNormal.Z > .8f; };
    const float HH = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Top;
    if (!Floor(PounceStart, PounceStart - FVector::UpVector * (HH + 50.f), Top))
    { BackDown(TEXT("Pounce has no departure platform")); return; }
    const float WantedEdge = 95.f * PounceDistanceScale;
    float Edge = -1.f;
    for (float S = 10.f; S <= 260.f * PounceDistanceScale; S += 10.f)
    {
        const FVector P = Top.ImpactPoint + Facing * S;
        FHitResult Support;
        if (!Floor(P + FVector::UpVector * 25.f, P - FVector::UpVector * 25.f, Support)
            || FMath::Abs(Support.ImpactPoint.Z - Top.ImpactPoint.Z) > 10.f)
        { Edge = S - 5.f; break; }
    }
    if (Edge < 0.f || FMath::Abs(Edge - WantedEdge) > 22.f * PounceDistanceScale)
    { BackDown(TEXT("Pounce is not prepared at the platform edge")); return; }
    const FVector EndXY = Top.ImpactPoint + Facing * (460.f * PounceDistanceScale);
    FHitResult Landing;
    if (!Floor(EndXY + FVector::UpVector * 25.f, EndXY - FVector::UpVector * 320.f, Landing))
    { BackDown(TEXT("Pounce has no supporting landing floor")); return; }
    const float Drop = Top.ImpactPoint.Z - Landing.ImpactPoint.Z;
    if (Drop < 45.f || Drop > 220.f)
    { BackDown(TEXT("Pounce landing height is outside the authored descent")); return; }
    PounceHeightScale = Drop / 155.f;
    for (int32 I = 0; I <= 132; ++I)
    {
        FString Blocker;
        if (!PouncePoseClear(float(I) / 132.f, nullptr, &Blocker))
        { BackDown(TEXT("Pounce body corridor blocked by ") + Blocker); return; }
    }
    bPerchRoute = false; bTransfer = bSqueeze = false;
    bPounceActive = true; PounceAge = 0.f; ActualSpeed = TravelSpeed = 0.f;
    bPounceOwnsMeshTransform = true;
    Character->GetMesh()->SetRelativeTransform(PounceMeshRelative);
    RouteKind = TEXT("Pounce"); Phase = TEXT("PouncePrepare");
    bPounceIgnoredPlayer = PouncePlayer.IsValid()
        && !Character->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(PouncePlayer.Get());
    if (bPounceIgnoredPlayer) Character->GetCapsuleComponent()->IgnoreActorWhenMoving(PouncePlayer.Get(), true);
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Character->SetActorRotation(PounceRotation);
}

float UTallowHunterTraversalComponent::ClearanceAt(const FVector& Point) const
{
    FHitResult Floor, Roof;
    if (!Trace(Point + FVector::UpVector * 40.f, Point - FVector::UpVector * 120.f, Floor)) return 0.f;
    return Trace(Floor.ImpactPoint + FVector::UpVector * 3.f, Floor.ImpactPoint + FVector::UpVector * 400.f, Roof)
        ? Roof.ImpactPoint.Z - Floor.ImpactPoint.Z : 400.f;
}

FVector2D UTallowHunterTraversalComponent::PassageAt(const FVector& Floor, const FVector& Forward) const
{
    FHitResult Roof;
    const float Height = Trace(Floor + FVector::UpVector * 3.f, Floor + FVector::UpVector * 400.f, Roof)
        ? FMath::Max(0.f, Roof.ImpactPoint.Z - Floor.Z) : 400.f;
    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward.GetSafeNormal2D());
    if (Right.IsNearlyZero()) Right = Character.IsValid() ? Character->GetActorRightVector() : FVector::RightVector;
    float Width = 400.f;
    for (const float Fraction : {.25f, .5f, .75f})
    {
        const FVector Probe = Floor + FVector::UpVector * FMath::Clamp(Height * Fraction, 10.f, 85.f);
        float Span = 0.f;
        for (const float Side : {-1.f, 1.f})
        {
            FHitResult Wall;
            Span += Trace(Probe, Probe + Right * Side * 200.f, Wall) ? Wall.Distance : 200.f;
        }
        Width = FMath::Min(Width, Span);
    }
    return FVector2D(Width, Height);
}

bool UTallowHunterTraversalComponent::StartLinkRoute(const TArray<FVector>& Surface, const TArray<FVector>& Normals,
    bool bIsSqueeze, float HalfHeight, bool bHold)
{
    LastRouteFailure.Reset(); RejectedSegment = INDEX_NONE; RejectedPose = FVector::ZeroVector;
    if (!Character.IsValid() || bActive || bFrozen) return Reject(TEXT("Character missing, already traversing, or frozen"));
    if (Surface.Num() < 2 || Surface.Num() != Normals.Num()) return Reject(TEXT("Link has no usable points"));
    const float InitialHH = HalfHeight > 0.f ? HalfHeight : Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (bIsSqueeze)
    {
        const auto* Mim = Cast<UTallowMimAnimInstance>(Character->GetMesh()->GetAnimInstance());
        if (Mim && Mim->HasAdaptiveFitLayer())
        {
            RouteClearance = FVector2D(400.f, 400.f);
            for (int32 P = 1; P < Surface.Num(); ++P)
            {
                const FVector Direction = (Surface[P] - Surface[P - 1]).GetSafeNormal2D();
                const int32 Checks = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Surface[P - 1], Surface[P]) / 12.f));
                for (int32 I = 0; I <= Checks; ++I)
                {
                    const FVector2D Space = PassageAt(FMath::Lerp(Surface[P - 1], Surface[P], float(I) / Checks), Direction);
                    RouteClearance.X = FMath::Min(RouteClearance.X, Space.X);
                    RouteClearance.Y = FMath::Min(RouteClearance.Y, Space.Y);
                }
            }
            if (RouteClearance.X < 82.f || RouteClearance.Y < 85.f)
                return Reject(FString::Printf(TEXT("Adaptive body requires 82x85cm; measured %.1fx%.1fcm"),
                    RouteClearance.X, RouteClearance.Y));
        }
    }
    // Fit the complete passage, including its approach. Width and height are
    // independent: narrowing a shoulder does not require shortening the torso.
    for (float HH = InitialHH; HH >= 26.f - KINDA_SMALL_NUMBER; HH -= 2.f)
    {
        for (float Radius = FMath::Min(SavedRadius, HH); Radius >= 26.f - KINDA_SMALL_NUMBER; Radius -= 2.f)
        {
            TArray<FWaypoint> Points;
            FVector LastForward = (Surface[1] - Surface[0]).GetSafeNormal();
            bool bPassageFits = true;
            for (int32 I = 0; I < Surface.Num(); ++I)
            {
                const FVector N = Normals[I].GetSafeNormal();
                if (N.IsNearlyZero()) return Reject(TEXT("Link has no support normal"));
                const FVector Next = I + 1 < Surface.Num() ? Surface[I + 1] : Surface[I] + (Surface[I] - Surface[I - 1]);
                FVector Forward = FVector::VectorPlaneProject(Next - Surface[I], N).GetSafeNormal();
                if (Forward.IsNearlyZero()) Forward = FVector::VectorPlaneProject(LastForward, N).GetSafeNormal();
                if (Forward.IsNearlyZero()) Forward = FVector::CrossProduct(N, FVector::RightVector).GetSafeNormal();
                LastForward = Forward;
                const float Lift = HH + (FMath::Abs(N.Z) > .9f ? 4.f : FMath::Abs(N.Z) < .1f ? 8.f : 20.f);
                Points.Add({Surface[I] + N * Lift, N, Surface[I], SurfaceRotation(Forward, N)});
                if (bIsSqueeze && N.Z > .9f)
                {
                    const FVector From = I > 0 ? Surface[I - 1] : Surface[I];
                    const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(From, Surface[I]) / 12.f));
                    for (int32 S = 0; S <= Samples; ++S)
                    {
                        const FVector2D Space = PassageAt(FMath::Lerp(From, Surface[I], float(S) / Samples), Forward);
                        if (Space.X < Radius * 2.f + 6.f || Space.Y < HH * 2.f + 6.f) bPassageFits = false;
                    }
                }
            }
            if (FVector::Dist2D(Character->GetActorLocation(), Points[0].Position) > 220.f) return Reject(TEXT("Not at the link entry"));
            if (bPassageFits && BeginRoute(MoveTemp(Points), bIsSqueeze,
                bIsSqueeze || HalfHeight > 0.f ? HH : -1.f, Radius))
            {
                bPerchRoute = bHold;
                RouteKind = bIsSqueeze ? TEXT("Crawl") : TEXT("Climb");
                return true;
            }
            if (!bIsSqueeze) break;
        }
        if (!bIsSqueeze) break;
    }
    return Reject(TEXT("No capsule fits the complete passage with clearance"));
}

bool UTallowHunterTraversalComponent::ResumeHeld(const TArray<FVector>& Surface, const TArray<FVector>& Normals)
{
    if (!IsHolding() || !Character.IsValid() || Surface.IsEmpty() || Surface.Num() != Normals.Num()) return false;
    const float HH = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector From = Character->GetActorLocation(), LastForward = Character->GetActorForwardVector();
    for (int32 I = 0; I < Surface.Num(); ++I)
    {
        const FVector N = Normals[I].GetSafeNormal();
        FVector Forward = FVector::VectorPlaneProject(Surface[I] - From, N).GetSafeNormal();
        if (Forward.IsNearlyZero()) Forward = LastForward;
        LastForward = Forward; From = Surface[I];
        Waypoints.Add({Surface[I] + N * (HH + 4.f), N, Surface[I], SurfaceRotation(Forward, N)});
    }
    bPerchRoute = false;
    BeginSegment();
    return true;
}

bool UTallowHunterTraversalComponent::StartOverRoute(const FVector& Toward, bool bTryPounce)
{
    LastRouteFailure.Reset(); RejectedSegment = INDEX_NONE; RejectedPose = FVector::ZeroVector;
    if (!Character.IsValid() || bActive || bFrozen) return Reject(TEXT("Character missing, already traversing, or frozen"));
    bFurniturePouncePending = false; FurniturePounceExit.Reset();
    const FVector Start = Character->GetActorLocation(), Up = FVector::UpVector;
    FVector Direction = Toward - Start;
    Direction.Z = 0.f;
    const float Distance = Direction.Size();
    if (Distance < 200.f) return Reject(TEXT("Over route target too close"));
    Direction /= Distance;
    const float HH = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Floor;
    if (!Trace(Start, Start - Up * (HH + 160.f), Floor) || Floor.ImpactNormal.Z < .7f) return Reject(TEXT("No floor at over route start"));
    const float FloorZ = Floor.ImpactPoint.Z;
    // Something low in the way, close ahead.
    FHitResult Face;
    const FVector Knee(Start.X, Start.Y, FloorZ + 30.f);
    if (!Trace(Knee, Knee + Direction * FMath::Min(Distance, 260.f), Face) || FMath::Abs(Face.ImpactNormal.Z) > .3f)
        return Reject(TEXT("Nothing low in the way"));
    const FVector N = Face.ImpactNormal.GetSafeNormal2D();
    FHitResult Top;
    const FVector In = Face.ImpactPoint + Direction * 25.f;
    if (!Trace(FVector(In.X, In.Y, FloorZ + 220.f), FVector(In.X, In.Y, FloorZ + 5.f), Top) || Top.ImpactNormal.Z < .8f)
        return Reject(TEXT("No top to climb onto"));
    const float TopZ = Top.ImpactPoint.Z, Height = TopZ - FloorZ;
    if (Height < 30.f || Height > 160.f) return Reject(TEXT("Too low or too tall to go over"));
    // Across its top until the top ends.
    float Depth = 25.f, OutsideDepth = -1.f;
    for (float S = 45.f; S <= 340.f; S += 20.f)
    {
        const FVector P = Face.ImpactPoint + Direction * S;
        FHitResult Hit;
        if (!Trace(FVector(P.X, P.Y, TopZ + 60.f), FVector(P.X, P.Y, TopZ - 25.f), Hit) || FMath::Abs(Hit.ImpactPoint.Z - TopZ) > 12.f)
        { OutsideDepth = S; break; }
        Depth = S;
    }
    if (OutsideDepth < 0.f) return Reject(TEXT("Its top runs on too far"));
    // A coarse midpoint can lie inside a solid box, making an otherwise
    // reachable far-side turn collide. Locate the real support edge first.
    for (int32 I = 0; I < 7; ++I)
    {
        const float S = (Depth + OutsideDepth) * .5f;
        const FVector P = Face.ImpactPoint + Direction * S;
        FHitResult Hit;
        if (Trace(FVector(P.X, P.Y, TopZ + 60.f), FVector(P.X, P.Y, TopZ - 25.f), Hit)
            && FMath::Abs(Hit.ImpactPoint.Z - TopZ) <= 12.f) Depth = S;
        else OutsideDepth = S;
    }
    Depth = (Depth + OutsideDepth) * .5f;
    const FVector NearEdge(Face.ImpactPoint.X, Face.ImpactPoint.Y, TopZ);
    const FVector FarEdge = NearEdge + Direction * Depth;
    FHitResult FarFloor;
    const FVector Beyond = FarEdge + Direction * (HH + 30.f);
    if (!Trace(FVector(Beyond.X, Beyond.Y, TopZ + 40.f), FVector(Beyond.X, Beyond.Y, FloorZ - 40.f), FarFloor)
        || FarFloor.ImpactNormal.Z < .8f || FMath::Abs(FarFloor.ImpactPoint.Z - FloorZ) > 25.f) return Reject(TEXT("No floor beyond it"));
    // The room above its top sets how flat it goes over.
    const float Gap = ClearanceAt(NearEdge + Direction * FMath::Min(Depth * .5f, 60.f) + Up * 2.f);
    if (Gap < 56.f) return Reject(TEXT("No room over its top"));
    const float Fit = Gap < SavedHalfHeight * 2.f + 8.f ? FMath::Max(26.f, Gap * .5f - 5.f) : -1.f;
    const float H = Fit > 0.f ? Fit : HH, R = FMath::Min(SavedRadius, H);
    // The existing pounce starts on a real ledge. Check its exact authored
    // corridor before choosing a held climb instead of the full over/down.
    const float LaunchEdge = 95.f * FMath::Abs(Character->GetMesh()->GetRelativeScale3D().X);
    const FVector LaunchSurface = FarEdge - Direction * LaunchEdge;
    bool bLaunchClear = false;
    if (bTryPounce && Height <= 150.f && Fit < 0.f
        && Depth > LaunchEdge + R + 20.f
        && FVector::DotProduct((Toward - LaunchSurface).GetSafeNormal2D(), Direction) > .94f)
    {
        const FVector End = LaunchSurface + Direction * (460.f * FMath::Abs(Character->GetMesh()->GetRelativeScale3D().X));
        FHitResult Landing;
        if (Trace(End + Up * 25.f, End - Up * 320.f, Landing) && Landing.ImpactNormal.Z > .8f
            && TopZ - Landing.ImpactPoint.Z >= 45.f && TopZ - Landing.ImpactPoint.Z <= 220.f
            && FVector::Dist2D(Landing.ImpactPoint, Toward) + 50.f < FVector::Dist2D(LaunchSurface, Toward))
        {
            const FVector PreviousStart = PounceStart;
            const FQuat PreviousRotation = PounceRotation;
            const FTransform PreviousMesh = PounceMeshRelative;
            const float PreviousDistance = PounceDistanceScale, PreviousHeight = PounceHeightScale;
            const TWeakObjectPtr<AActor> PreviousPlayer = PouncePlayer;
            PounceStart = LaunchSurface + Up * (HH + 4.f);
            PounceRotation = FRotator(0.f, Direction.Rotation().Yaw, 0.f).Quaternion();
            PounceMeshRelative = Character->GetMesh()->GetRelativeTransform();
            PounceMeshRelative.SetTranslation(FVector(0.f, 0.f, 3.f - Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
            PounceDistanceScale = FMath::Abs(PounceMeshRelative.GetScale3D().X);
            PounceHeightScale = (TopZ - Landing.ImpactPoint.Z) / 155.f;
            PouncePlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
            bLaunchClear = true;
            for (int32 I = 0; I <= 132; ++I)
                if (!PouncePoseClear(float(I) / 132.f)) { bLaunchClear = false; break; }
            PounceStart = PreviousStart; PounceRotation = PreviousRotation; PounceMeshRelative = PreviousMesh;
            PounceDistanceScale = PreviousDistance; PounceHeightScale = PreviousHeight; PouncePlayer = PreviousPlayer;
        }
    }
    TArray<FVector> Surface, Normals;
    int32 LaunchIndex = INDEX_NONE;
    auto Add = [&](const FVector& P, const FVector& Normal) { Surface.Add(P); Normals.Add(Normal); };
    Add(FVector(NearEdge.X, NearEdge.Y, FloorZ) + N * (H + 28.f), Up);
    if (Height > H + 40.f) { Add(FVector(NearEdge.X, NearEdge.Y, FloorZ + H + 15.f), N); Add(NearEdge - Up * 40.f, N); }
    Add(NearEdge, (N + Up).GetSafeNormal());
    if (Depth > (R + 15.f) * 2.f)
    {
        Add(NearEdge + Direction * (R + 15.f), Up);
        if (bLaunchClear) { Add(LaunchSurface, Up); LaunchIndex = Surface.Num() - 1; }
        Add(FarEdge - Direction * (R + 15.f), Up);
    }
    else Add((NearEdge + FarEdge) * .5f, Up);
    Add(FarEdge, (Direction + Up).GetSafeNormal());
    if (Height > H + 40.f) { Add(FarEdge - Up * 40.f, Direction); Add(FVector(FarEdge.X, FarEdge.Y, FloorZ + H + 15.f), Direction); }
    Add(FVector(Beyond.X, Beyond.Y, FarFloor.ImpactPoint.Z), Up);
    const bool bStarted = StartLinkRoute(Surface, Normals, Fit > 0.f, Fit, false);
    if (bStarted)
    {
        RouteKind = TEXT("Furniture");
        if (Waypoints.IsValidIndex(LaunchIndex))
        {
            FurniturePounceExit.Append(Waypoints.GetData() + LaunchIndex + 1, Waypoints.Num() - LaunchIndex - 1);
            FurniturePounceTarget = Toward; bFurniturePouncePending = true;
            Waypoints.SetNum(LaunchIndex + 1); bPerchRoute = true;
        }
    }
    return bStarted;
}

bool UTallowHunterTraversalComponent::StartWallWalk(const FVector& Toward)
{
    LastRouteFailure.Reset(); RejectedSegment = INDEX_NONE; RejectedPose = FVector::ZeroVector;
    if (!Character.IsValid() || bActive || bFrozen) return Reject(TEXT("Character missing, already traversing, or frozen"));
    const FVector Start = Character->GetActorLocation();
    FVector Direction = Toward - Start;
    Direction.Z = 0.f;
    const float Distance = Direction.Size();
    if (Distance < 320.f) return Reject(TEXT("Wall walk target too close"));
    Direction /= Distance;
    const auto* Capsule = Character->GetCapsuleComponent();
    const float HH = Capsule->GetScaledCapsuleHalfHeight(), Radius = Capsule->GetScaledCapsuleRadius(), Offset = HH + 10.f;
    FHitResult Floor;
    if (!Trace(Start, Start - FVector::UpVector * (HH + 160.f), Floor) || Floor.ImpactNormal.Z < .7f)
        return Reject(TEXT("No floor at wall walk start"));
    const float FloorZ = Floor.ImpactPoint.Z;
    // Shoulder height on the wall: high enough to read as on the wall, under every door head and picture rail.
    const float Height = 145.f;
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
    for (const FVector& Axis : {Side, -Side, (Side + Direction).GetSafeNormal(), (-Side + Direction).GetSafeNormal()})
    {
        FHitResult Hit;
        if (!Trace(Start, Start + Axis * 260.f, Hit) || FMath::Abs(Hit.ImpactNormal.Z) > .25f || Hit.Distance <= Radius) continue;
        const FVector N = Hit.ImpactNormal.GetSafeNormal2D();
        const FVector Along = FVector::VectorPlaneProject(Direction, N).GetSafeNormal2D();
        if (Along.IsNearlyZero() || FVector::DotProduct(Along, Direction) < .5f) continue;
        // How far the same wall runs unbroken (no doorway, no turn) at the travel height and low down.
        const FVector Base(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, FloorZ);
        const float Want = FMath::Clamp(Distance - 80.f, 260.f, 700.f);
        float Run = 0.f;
        for (float S = 40.f; S <= Want; S += 40.f)
        {
            bool bWall = true;
            for (const float Z : {HH + 15.f, Height})
            {
                const FVector P = Base + Along * S + FVector::UpVector * Z;
                FHitResult W;
                if (!Trace(P + N * 40.f, P - N * 30.f, W) || FVector::DotProduct(W.ImpactNormal, N) < .9f
                    || FMath::Abs(FVector::DotProduct(W.ImpactPoint - Base, N)) > 8.f) { bWall = false; break; }
            }
            if (!bWall) break;
            Run = S;
        }
        if (Run < 260.f) continue;
        const FVector End = Base + Along * Run;
        FHitResult EndFloor;
        const FVector FootProbe = End + N * (Offset + 24.f) + FVector::UpVector * 60.f;
        if (!Trace(FootProbe, FootProbe - FVector::UpVector * 220.f, EndFloor) || EndFloor.ImpactNormal.Z < .7f) continue;
        TArray<FWaypoint> Points;
        auto Add = [&](const FVector& Surface, const FVector& Normal, const FVector& Forward, float Lift)
        { Points.Add({Surface + Normal * Lift, Normal, Surface, SurfaceRotation(Forward, Normal)}); };
        const FVector Up = FVector::UpVector;
        Add(Base + N * (Offset + 18.f), Up, -N, HH + 4.f);
        Add(Base + Up * (HH + 15.f), N, (Up + Along * .4f).GetSafeNormal(), Offset);
        Add(Base + Along * 110.f + Up * Height, N, Along, Offset);
        Add(End - Along * 110.f + Up * Height, N, Along, Offset);
        Add(End + Up * (HH + 15.f), N, (Along - Up * .6f).GetSafeNormal(), Offset);
        Add(EndFloor.ImpactPoint, Up, Along, HH + 4.f);
        if (BeginRoute(MoveTemp(Points), false)) { RouteKind = TEXT("WallWalk"); return true; }
    }
    if (LastRouteFailure.IsEmpty()) Reject(TEXT("No straight wall beside it toward the target"));
    return false;
}

void UTallowHunterTraversalComponent::RequestGroundExit()
{
    ClearCornerMeshTransform();
    bPerchRoute = false;
    if (!bActive || bReturning) return;
    bReturning = true;
    Waypoints.Reset();
    for (int32 I = History.Num() - 1; I >= 0; --I) Waypoints.Add(History[I]);
    WaypointIndex = 0;
    BeginSegment();
}

void UTallowHunterTraversalComponent::RestoreCapsule()
{
    if (!Character.IsValid() || !bRestoreCapsulePending) return;
    auto* Capsule = Character->GetCapsuleComponent();
    const float Rise = SavedHalfHeight - Capsule->GetUnscaledCapsuleHalfHeight();
    const FVector NewLocation = Character->GetActorLocation() + FVector::UpVector * FMath::Max(0.f, Rise);
    PoseRadius = SavedRadius;
    ON_SCOPE_EXIT { PoseRadius = -1.f; };
    if (!PoseClear(NewLocation, Character->GetActorQuat(), SavedHalfHeight)) return;
    Capsule->SetCapsuleSize(SavedRadius, SavedHalfHeight, true);
    Character->SetActorLocation(NewLocation, false);
    bRestoreCapsulePending = false;
    RouteClearance = FVector2D::ZeroVector;
    SqueezeSeverity = 0.f; RouteSpeedScale = 1.f;
}

void UTallowHunterTraversalComponent::FinishRoute()
{
    ClearCornerMeshTransform();
    ClearPounce();
    if (!Character.IsValid()) return;
    auto* Movement = Character->GetCharacterMovement();
    Movement->StopMovementImmediately();
    Movement->GravityScale = SavedGravity;
    Movement->bOrientRotationToMovement = bSavedOrient;
    Movement->bUseControllerDesiredRotation = bSavedDesired;
    Character->bUseControllerRotationYaw = bSavedYaw;
    Movement->SetMovementMode(MOVE_Falling);
    bActive = bSqueeze = bTransfer = false;
    bAwaitingAdaptiveFit = false; AdaptiveFitWaitAge = 0.f;
    RouteKind = NAME_None;
    ActualSpeed = TravelSpeed = 0.f;
    SurfaceNormal = FVector::UpVector;
    Phase = TEXT("Ground");
    RestoreCapsule();
}

void UTallowHunterTraversalComponent::ResetTraversal()
{
    ClearCornerMeshTransform();
    ClearPounce();
    bFrozen = false;
    bAttackPaused = false;
    bTacticalPaused = false;
    RouteKind = NAME_None;
    bPerchRoute = false;
    if (bActive) FinishRoute();
    if (Character.IsValid())
        Character->GetCapsuleComponent()->SetCapsuleSize(SavedRadius, SavedHalfHeight, true);
    bRestoreCapsulePending = false;
    RouteClearance = FVector2D::ZeroVector;
    SqueezeSeverity = 0.f; RouteSpeedScale = 1.f;
    Waypoints.Reset();
    History.Reset();
    SqueezeAlpha = SegmentAlpha = 0.f;
}

void UTallowHunterTraversalComponent::FreezeForCapture()
{
    ClearCornerMeshTransform();
    bFrozen = true;
    ActualSpeed = 0.f;
    if (Character.IsValid()) Character->GetCharacterMovement()->StopMovementImmediately();
}

bool UTallowHunterTraversalComponent::TryBlockedGroundExit()
{
    if (!Character.IsValid()) return false;
    const FVector Here = Character->GetActorLocation();
    const auto* Capsule = Character->GetCapsuleComponent();
    const float HH = Capsule->GetScaledCapsuleHalfHeight();
    const FQuat Upright = FRotator(0.f, Character->GetActorForwardVector().Rotation().Yaw, 0.f).Quaternion();
    if (!PoseClear(Here, Upright)) return false;
    FHitResult Floor;
    if (!Trace(Here, Here - FVector::UpVector * 1000.f, Floor) || Floor.ImpactNormal.Z < .7f) return false;
    const FVector Landing = Floor.ImpactPoint + FVector::UpVector * (HH + 4.f);
    if (Landing.Z > Here.Z + 5.f || !PoseClear(Landing, Upright)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterBlockedGroundExit), false, Character.Get());
    Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
    TArray<AActor*> Attached;
    Character->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    if (GetWorld()->SweepTestByProfile(Here, Landing, Upright, Capsule->GetCollisionProfileName(),
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HH), Query)) return false;
    // A moving door can close across the return route after validation. Once
    // the upright capsule and the complete fall are clear, gravity is a real
    // local exit. Keeping MOVE_None here used to suspend the brain forever.
    const FString Reason = LastRouteFailure;
    Character->SetActorRotation(Upright);
    bPerchRoute = false;
    FinishRoute();
    RouteKind = TEXT("Drop");
    LastRouteFailure = Reason + TEXT("; released to a clear floor below");
    return true;
}

void UTallowHunterTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime, TickType, Tick);
    if (!Character.IsValid() || bFrozen) return;
    UpdateCornerMeshTransform();
    if (bAttackPaused || bTacticalPaused) { ActualSpeed = TravelSpeed = 0.f; return; }
    if (bPounceActive)
    {
        float Remaining = FMath::Min(DeltaTime, .12f);
        while (Remaining > SMALL_NUMBER && bPounceActive)
        {
            const float Step = FMath::Min(Remaining, 1.f / 60.f); Remaining -= Step;
            if (!AdvancePounce(Step)) break;
        }
        return;
    }
    SqueezeAlpha = FMath::FInterpTo(SqueezeAlpha, bSqueeze || bRestoreCapsulePending ? 1.f : 0.f, DeltaTime, 3.5f);
    if (!bActive)
    {
        RestoreCapsule();
        if ((RouteKind == TEXT("Drop") || RouteKind == TEXT("Pounce"))
            && !Character->GetCharacterMovement()->IsFalling()) RouteKind = NAME_None;
        return;
    }
    if (Phase == TEXT("Perch"))
    {
        if (bFurniturePouncePending && RouteKind == TEXT("Furniture") && !bReturning)
        {
            bFurniturePouncePending = false;
            PounceToward(FurniturePounceTarget);
            if (!bPounceActive && bActive && !FurniturePounceExit.IsEmpty())
            {
                // The prospective launch was checked before climbing; a newly
                // blocked jump continues the checked far-side exit instead.
                bReturning = bPerchRoute = false;
                Waypoints = MoveTemp(FurniturePounceExit); WaypointIndex = 0;
                BeginSegment();
            }
            else FurniturePounceExit.Reset();
        }
        return;
    }
    if (const auto* Hunter = Cast<ATallowHunterAIController>(Character->GetController()))
    {
        const ETallowKeeperIntent Intent = Hunter->GetIntent();
        const float Floor = Intent == ETallowKeeperIntent::Chase ? 335.f
            : (Intent == ETallowKeeperIntent::Hunt || Intent == ETallowKeeperIntent::Reveal) ? 230.f
            : Intent == ETallowKeeperIntent::Stalk ? 150.f : 190.f;
        CrawlSpeed = Floor; CeilingSpeed = Floor * .95f;
        SqueezeSpeed = Floor;
    }
    // Fixed maximum steps keep collision and the reach gate consistent under render hitches.
    float Remaining = FMath::Min(DeltaTime, .12f);
    while (Remaining > SMALL_NUMBER && bActive && Waypoints.IsValidIndex(WaypointIndex))
    {
        const float Step = FMath::Min(Remaining, 1.f / 60.f);
        Remaining -= Step;
        if (!Advance(Step))
        {
            if (!bReturning) { ++AbortedTransfers; RequestGroundExit(); }
            else
            {
                TravelSpeed = ActualSpeed = 0.f; Phase = TEXT("Blocked");
                BlockedAge += DeltaTime;
                if (BlockedAge > 1.2f) TryBlockedGroundExit();
            }
            break;
        }
        BlockedAge = 0.f;
    }
    UpdateCornerMeshTransform();
}

bool UTallowHunterTraversalComponent::StartSurfaceRoute()
{
    if (!Character.IsValid()) return false;
    TArray<AActor*> Anchors;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("Tallow.Traversal.Anchor"))) Anchors.Add(*It);
    Anchors.Sort([](const AActor& A, const AActor& B) {
        return TagValue(&A, TEXT("Next=")) < TagValue(&B, TEXT("Next="));
    });
    const int32 Start = Anchors.IndexOfByPredicate([](const AActor* A) {
        return TagValue(A, TEXT("Next=")) == TEXT("B01");
    });
    if (Anchors.Num() < 2 || Start == INDEX_NONE) return false;
    TArray<FWaypoint> Points;
    const float Offset = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.f;
    for (int32 I = 0; I < Anchors.Num(); ++I)
    {
        const AActor* A = Anchors[(Start + I) % Anchors.Num()];
        TArray<FString> N;
        TagValue(A, TEXT("Normal=")).ParseIntoArray(N, TEXT(","));
        if (N.Num() != 3) return false;
        const FVector Normal = FVector(FCString::Atof(*N[0]), FCString::Atof(*N[1]), FCString::Atof(*N[2])).GetSafeNormal();
        if (Normal.IsNearlyZero()) return false;
        Points.Add({A->GetActorLocation() + Normal * Offset, Normal, A->GetActorLocation(),
            SurfaceRotation(FVector::RightVector, Normal)});
    }
    if (FVector::Dist(Character->GetActorLocation(), Points[0].Position) > 250.f) return false;
    return BeginRoute(MoveTemp(Points), false);
}

bool UTallowHunterTraversalComponent::StartSqueeze(AActor* Entry, AActor* Exit)
{
    if (!Character.IsValid() || !IsValid(Entry) || !IsValid(Exit)) return false;
    const FVector Direction = (Exit->GetActorLocation() - Entry->GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return false;
    TArray<FVector> Surface, Normals;
    const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(Entry->GetActorLocation(), Exit->GetActorLocation()) / 20.f));
    float LastZ = Entry->GetActorLocation().Z;
    for (int32 I = 0; I <= Samples; ++I)
    {
        FVector Point = FMath::Lerp(Entry->GetActorLocation(), Exit->GetActorLocation(), float(I) / Samples);
        Point.Z = LastZ;
        FHitResult Floor;
        if (!Trace(Point + FVector::UpVector * 40.f, Point - FVector::UpVector * 120.f, Floor)
            || Floor.ImpactNormal.Z < .7f || (I > 0 && FMath::Abs(Floor.ImpactPoint.Z - LastZ) > 35.f))
            return Reject(TEXT("Crawl lacks a continuous supporting floor"));
        LastZ = Floor.ImpactPoint.Z;
        Surface.Add(Floor.ImpactPoint); Normals.Add(FVector::UpVector);
    }
    return StartLinkRoute(Surface, Normals, true);
}

bool UTallowHunterTraversalComponent::StartSqueezeToward(const FVector& Toward)
{
    if (!Character.IsValid() || bActive || bFrozen) return Reject(TEXT("Squeeze state unavailable"));
    const FVector Here = Character->GetActorLocation();
    const FVector Direction = (Toward - Here).GetSafeNormal2D();
    const float Distance = FVector::Dist2D(Here, Toward);
    if (Direction.IsNearlyZero() || Distance < 135.f) return Reject(TEXT("No aperture approach"));
    const float Travel = FMath::Min(420.f, Distance - 110.f);
    float LastZ = Here.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    TArray<FVector> Surface, Normals;
    bool bConstrained = false;
    const int32 Samples = FMath::Max(2, FMath::CeilToInt(Travel / 20.f));
    for (int32 I = 0; I <= Samples; ++I)
    {
        FVector Point = Here + Direction * (Travel * I / Samples);
        Point.Z = LastZ;
        FHitResult Floor;
        if (!Trace(Point + FVector::UpVector * 40.f, Point - FVector::UpVector * 120.f, Floor)
            || Floor.ImpactNormal.Z < .7f || FMath::Abs(Floor.ImpactPoint.Z - LastZ) > 35.f)
            return Reject(TEXT("Automatic crawl lacks a continuous floor"));
        LastZ = Floor.ImpactPoint.Z;
        const FVector2D Space = PassageAt(Floor.ImpactPoint, Direction);
        if (Space.X < 58.f || Space.Y < 58.f) return Reject(TEXT("Aperture below folded body clearance"));
        bConstrained |= Space.X < SavedRadius * 2.f + 8.f || Space.Y < SavedHalfHeight * 2.f + 8.f;
        Surface.Add(Floor.ImpactPoint); Normals.Add(FVector::UpVector);
    }
    if (!bConstrained) return Reject(TEXT("No constrained aperture on this approach"));
    return StartLinkRoute(Surface, Normals, true);
}

bool UTallowHunterTraversalComponent::StartChaseSurfaceRoute(const FVector& TargetLocation, bool bPerch)
{
    return BuildChaseCeilingRoute(TargetLocation, bPerch, false);
}

bool UTallowHunterTraversalComponent::StartDirectCeilingRoute(const FVector& TargetLocation)
{
    return BuildChaseCeilingRoute(TargetLocation, false, true);
}

bool UTallowHunterTraversalComponent::BuildChaseCeilingRoute(const FVector& TargetLocation, bool bPerch, bool bDirectReach)
{
    LastRouteFailure.Reset(); RejectedSegment = INDEX_NONE; RejectedPose = FVector::ZeroVector;
    if (!Character.IsValid() || bActive || bFrozen) return Reject(TEXT("Character missing, already traversing, or frozen"));
    const FVector Start = Character->GetActorLocation();
    FVector Direction = TargetLocation - Start;
    const float HeightDifference = Direction.Z;
    Direction.Z = 0.f;
    const float Distance = Direction.Size();
    if (Distance < (bDirectReach ? 110.f : 250.f) || Distance > 1500.f || FMath::Abs(HeightDifference) > 200.f)
        return Reject(TEXT("Target outside route distance or storey bounds"));
    Direction /= Distance;
    const auto* Capsule = Character->GetCapsuleComponent();
    const float HH = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius(), Offset = HH + 10.f;
    FHitResult Ceiling, Floor, Wall;
    if (!Trace(Start, Start + FVector::UpVector * 700.f, Ceiling) || Ceiling.ImpactNormal.Z > -.7f
        || !Trace(Start, Start - FVector::UpVector * (HH + 160.f), Floor) || Floor.ImpactNormal.Z < .7f)
        return Reject(TEXT("No usable floor and ceiling at route start"));
    const float CeilingZ = Ceiling.ImpactPoint.Z, FloorZ = Floor.ImpactPoint.Z;
    if (CeilingZ - FloorZ < Offset * 2.f + 40.f) return Reject(TEXT("Insufficient storey height"));
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
    TArray<FWaypoint> Points;
    auto Add = [&](const FVector& Position, const FVector& Normal, const FVector& Surface, const FVector& Forward) {
        Points.Add({Position, Normal, Surface, SurfaceRotation(Forward, Normal)});
    };
    FVector CeilingEntry;
    if (bDirectReach)
    {
        if (!Motion.IsValid() || !Motion->UsesMim() || Character->GetActorUpVector().Z < .9f)
            return Reject(TEXT("Direct ceiling reach needs a grounded upright body"));
        if (!LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Characters/Mim/Anims/MIM_Surf_FloorToCeiling.MIM_Surf_FloorToCeiling"),
            nullptr, LOAD_NoWarn | LOAD_Quiet))
            return Reject(TEXT("Direct ceiling reach animation unavailable"));
        if (CeilingZ - FloorZ > 360.f || CeilingZ - FloorZ < 220.f)
            return Reject(TEXT("Ceiling outside the standing reach envelope"));
        // Both leading hands must have continuous overhead support. The
        // live Reach phase still waits for their finite chains to grip it.
        for (const float Across : {-65.f, 65.f}) for (const float Ahead : {-30.f, 70.f})
        {
            const FVector At = Start + Side * Across + Direction * Ahead;
            FHitResult Support;
            if (!Trace(FVector(At.X, At.Y, CeilingZ - 45.f), FVector(At.X, At.Y, CeilingZ + 25.f), Support)
                || Support.ImpactNormal.Z > -.85f || FMath::Abs(Support.ImpactPoint.Z - CeilingZ) > 10.f)
                return Reject(TEXT("Direct ceiling reach lacks two clear overhead supports"));
        }
        CeilingEntry = FVector(Start.X, Start.Y, CeilingZ - Offset);
        Add(Start, FVector::UpVector, Floor.ImpactPoint, Direction);
    }
    else
    {
    float Nearest = TNumericLimits<float>::Max();
    bool FoundWall = false;
    const float ClimbTopZ = CeilingZ - HH - 30.f;
    for (const FVector& Axis : {Side, -Side, (Side + Direction).GetSafeNormal(), (-Side + Direction).GetSafeNormal(), -Direction})
    {
        FHitResult Hit, Upper;
        if (!Trace(Start, Start + Axis * 350.f, Hit) || FMath::Abs(Hit.ImpactNormal.Z) >= .25f
            || Hit.Distance <= Radius || Hit.Distance >= Nearest) continue;
        // Only a wall that is still there at the transfer height can be climbed; a wall with a
        // doorway in it at this spot is not one.
        const FVector N0 = Hit.ImpactNormal.GetSafeNormal2D();
        const FVector Up(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, ClimbTopZ);
        if (!Trace(Up + N0 * 30.f, Up - N0 * 35.f, Upper)) continue;
        Wall = Hit; Nearest = Hit.Distance; FoundWall = true;
    }
    if (!FoundWall) return Reject(TEXT("No reachable starting wall"));
    const FVector N = Wall.ImpactNormal.GetSafeNormal();
    const FVector WallBase(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, FloorZ);
    // Begin turning while the full capsule and leading body still fit below the
    // ceiling. The previous radius+20 offset intersected the ceiling mid-slerp.
    const float TopZ = CeilingZ - HH - 30.f;
    Add(WallBase + N * (Offset + 18.f) + FVector::UpVector * (HH + 4.f),
        FVector::UpVector, WallBase, -N);
    Add(WallBase + N * Offset + FVector::UpVector * (HH + 15.f), N,
        WallBase + FVector::UpVector * (HH + 15.f), FVector::UpVector);
    const FVector Top(WallBase.X, WallBase.Y, TopZ);
    FHitResult CheckWall;
    if (!Trace(Top + N * 30.f, Top - N * 35.f, CheckWall)) return Reject(TEXT("Starting wall ends below transfer"));
    Add(Top + N * Offset, N, CheckWall.ImpactPoint, FVector::UpVector);
    CeilingEntry = Top + N * (Offset + Radius + 45.f);
    }
    FVector Across = TargetLocation - CeilingEntry;
    Across.Z = 0.f;
    Across.Normalize();
    if (Across.IsNearlyZero()) return Reject(TEXT("No ceiling travel direction"));
    const float Travel = FMath::Clamp(FVector::Dist2D(TargetLocation, CeilingEntry) - (bPerch ? 0.f : 150.f), 120.f, 900.f);
    const int32 CeilingSamples = FMath::Max(2, FMath::CeilToInt(Travel / 20.f));
    // Follow the real ceiling: it ducks under beams and door headers and rises again after them,
    // instead of rejecting every span that is not one flat plane (which was every span after the
    // V04 joinery, and every doorway).
    TArray<FVector> Profile;
    int32 GapSamples = 0;
    for (int32 Sample = 0; Sample <= CeilingSamples; ++Sample)
    {
        const float T = Travel * Sample / CeilingSamples;
        const FVector XY = CeilingEntry + Across * T;
        const FVector From(XY.X, XY.Y, FloorZ + HH * 2.f + 25.f), To(XY.X, XY.Y, CeilingZ + 60.f);
        FHitResult Hit;
        bool Found = Trace(From, To, Hit);
        // A slab seam or the edge of a small opening returns a sideways normal; look either side of it.
        for (const float Nudge : {6.f, -6.f, 14.f, -14.f})
        {
            if (Found && Hit.ImpactNormal.Z <= -.7f) break;
            FHitResult Retry;
            const FVector Shift = Across * Nudge;
            if (Trace(From + Shift, To + Shift, Retry) && Retry.ImpactNormal.Z <= -.7f) { Hit = Retry; Found = true; }
        }
        const bool bGap = !Found || Hit.ImpactNormal.Z > -.7f || Hit.ImpactPoint.Z > CeilingZ + 40.f;
        if (bGap && !Profile.IsEmpty() && ++GapSamples <= 3)
        {
            // Up to 60 cm with nothing to hold: carried across at the last ceiling height.
            Profile.Add(FVector(XY.X, XY.Y, Profile.Last().Z));
            continue;
        }
        if (!bGap) GapSamples = 0;
        if (bGap || Hit.ImpactPoint.Z - FloorZ < HH * 2.f + 30.f)
        {
            RejectedPose = From;
            return Reject(FString::Printf(
                TEXT("Ceiling span sample %d/%d from %s: hit=%d %s.%s at %s normal=%s; start ceiling %s at %.2f"),
                Sample, CeilingSamples, *From.ToCompactString(), Found, *GetNameSafe(Hit.GetActor()),
                *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToCompactString(),
                *Hit.ImpactNormal.ToCompactString(), *GetNameSafe(Ceiling.GetActor()), CeilingZ));
        }
        Profile.Add(Hit.ImpactPoint);
    }
    // Duck a full body-width early and stay low until past: each sample takes the lowest ceiling
    // within the capsule's radius plus a margin either side, so the drop never brushes the face
    // of a beam or the wall above a door.
    {
        const float Step = Travel / CeilingSamples;
        const int32 Reach = FMath::CeilToInt((Radius + 14.f) / FMath::Max(Step, 1.f));
        TArray<float> Low; Low.SetNum(Profile.Num());
        for (int32 I = 0; I < Profile.Num(); ++I)
        {
            Low[I] = Profile[I].Z;
            for (int32 J = FMath::Max(0, I - Reach); J <= FMath::Min(Profile.Num() - 1, I + Reach); ++J)
                Low[I] = FMath::Min(Low[I], Profile[J].Z);
        }
        for (int32 I = 0; I < Profile.Num(); ++I) Profile[I].Z = Low[I];
    }
    // Waypoints at the ends and either side of every step in the (widened) ceiling height.
    float LastZ = Profile[0].Z;
    Add(Profile[0] - FVector::UpVector * Offset, -FVector::UpVector, Profile[0], Across);
    for (int32 I = 1; I < Profile.Num(); ++I)
    {
        const float Z = Profile[I].Z;
        if (FMath::Abs(Z - LastZ) > 8.f || I == Profile.Num() - 1)
        {
            const float Low = FMath::Min(Z, LastZ);
            const FVector Before(Profile[I - 1].X, Profile[I - 1].Y, Low), After(Profile[I].X, Profile[I].Y, Low);
            if (I < Profile.Num() - 1) Add(Before - FVector::UpVector * Offset, -FVector::UpVector, Before, Across);
            Add(FVector(Profile[I].X, Profile[I].Y, Z) - FVector::UpVector * Offset, -FVector::UpVector, Profile[I], Across);
            LastZ = Z;
        }
    }
    // A perch ends here, hanging from the ceiling short of the target.
    if (bPerch)
    {
        const bool bStarted = BeginRoute(MoveTemp(Points), false);
        bPerchRoute = bStarted;
        return bStarted;
    }
    // Return down an actual wall. Never rotate a hanging creature into an unsupported floor drop.
    const FVector End = Points.Last().Position;
    FHitResult DownWall;
    float Nearest = TNumericLimits<float>::Max();
    bool FoundWall = false;
    for (const FVector& Axis : {Side, -Side, Direction, -Direction})
    {
        FHitResult Hit;
        const FVector Probe(End.X, End.Y, Profile.Last().Z - 140.f);
        if (Trace(Probe, Probe + Axis * 360.f, Hit) && FMath::Abs(Hit.ImpactNormal.Z) < .25f
            && Hit.Distance > Radius && Hit.Distance < Nearest)
        { DownWall = Hit; Nearest = Hit.Distance; FoundWall = true; }
    }
    if (!FoundWall) return Reject(TEXT("No wall for controlled descent"));
    const FVector DN = DownWall.ImpactNormal;
    // The far room's ceiling may differ from the start room's: descend from the height actually reached.
    const FVector DownTop(DownWall.ImpactPoint.X, DownWall.ImpactPoint.Y, Profile.Last().Z - HH - 30.f);
    const bool bMimDescent = Motion.IsValid() && Motion->UsesMim();
    // The authored reverse transfer reaches behind the hanging body with05.
    // Facing into the wall placed those shoulders another150–190cm away,
    // beyond their171cm chains. The reverse hang keeps the root112cm from
    // the wall, with the shoulders inside finite grasp range. Face away first;
    // BeginRoute checks the complete turn/translation capsule sweep, and the
    // usual traced finite-chain grasp gate still has to acquire the real wall.
    const float DescentRoom = bMimDescent ? FMath::Max(112.f, Offset + Radius + 18.f)
        : Offset + Radius + 45.f;
    const FVector CeilingExit = DownTop + DN * DescentRoom;
    Add(FVector(CeilingExit.X, CeilingExit.Y, Profile.Last().Z - Offset),
        -FVector::UpVector, FVector(CeilingExit.X, CeilingExit.Y, CeilingZ), bMimDescent ? DN : -DN);
    Add(DownTop + DN * Offset, DN, DownTop, -FVector::UpVector);
    FHitResult DownFloor;
    const FVector FootProbe = DownTop + DN * (Offset + 24.f);
    if (!Trace(FootProbe, FootProbe - FVector::UpVector * (CeilingZ - FloorZ + 180.f), DownFloor)
        || DownFloor.ImpactNormal.Z < .7f) return Reject(TEXT("No floor at descent exit"));
    const FVector DownBase(DownTop.X, DownTop.Y, DownFloor.ImpactPoint.Z);
    Add(DownBase + DN * Offset + FVector::UpVector * (HH + 15.f), DN,
        DownBase + FVector::UpVector * (HH + 15.f), -FVector::UpVector);
    FVector Facing = TargetLocation - DownFloor.ImpactPoint;
    Facing.Z = 0.f;
    Add(DownFloor.ImpactPoint + FVector::UpVector * (HH + 4.f), FVector::UpVector,
        DownFloor.ImpactPoint, Facing.GetSafeNormal());
    const bool bStarted = BeginRoute(MoveTemp(Points), false);
    if (bStarted && bDirectReach) RouteKind = TEXT("DirectCeiling");
    return bStarted;
}
