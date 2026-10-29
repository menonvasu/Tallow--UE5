#include "AI/Hunter/TallowMimMovementContract.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
constexpr float BodyTolerance = 2.f;
constexpr float DepthTolerance = .1f;
struct FContact
{
    float Depth = 0.f;
    FHitResult Hit;
};
using FContacts = TMap<UPrimitiveComponent*, FContact>;
struct FBodyPart
{
    FVector Center;
    FQuat Rotation;
    FCollisionShape Shape;
    FName Name;
};

void ReadContacts(UWorld* World, const FVector& At, const FQuat& Rotation,
    const FCollisionShape& Shape, ECollisionChannel Channel, const FCollisionQueryParams& Query,
    const FCollisionResponseParams& Responses, FContacts& Contacts)
{
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByChannel(Overlaps, At, Rotation, Channel, Shape, Query, Responses);
    for (const auto& Overlap : Overlaps)
    {
        auto* Other = Overlap.GetComponent();
        if (!Overlap.bBlockingHit || !Other) continue;
        FMTDResult MTD;
        const bool bMeasured = Other->ComputePenetration(MTD, Shape, At, Rotation);
        FContact Contact;
        // An overlap without an MTD is a blocker, not proof of a tiny brush.
        // Negative Hit depth records that the penetration was not measurable.
        Contact.Depth = bMeasured ? MTD.Distance : BodyTolerance + 1.f;
        const FVector Normal = bMeasured ? FVector(MTD.Direction) : FVector::UpVector;
        Contact.Hit = FHitResult(Other->GetOwner(), Other, At, Normal);
        Contact.Hit.bBlockingHit = true;
        Contact.Hit.bStartPenetrating = true;
        Contact.Hit.PenetrationDepth = bMeasured ? Contact.Depth : -1.f;
        Contact.Hit.Location = At;
        Contact.Hit.ImpactPoint = At;
        Contacts.FindOrAdd(Other) = Contact;
    }
}

float MaximumDepth(const FContacts& Contacts)
{
    float Depth = 0.f;
    for (const auto& Contact : Contacts) Depth = FMath::Max(Depth, Contact.Value.Depth);
    return Depth;
}

float ReportedDepth(const FContacts& Contacts)
{
    for (const auto& Contact : Contacts)
        if (Contact.Value.Hit.PenetrationDepth < 0.f) return -1.f;
    return MaximumDepth(Contacts);
}

bool ContactsAllowed(const FContacts& Current, const FContacts& Initial,
    const FContacts& Previous, float Tolerance, FHitResult& Hit)
{
    for (const auto& Contact : Current)
    {
        const auto* Was = Initial.Find(Contact.Key);
        const auto* Last = Previous.Find(Contact.Key);
        const float Allowed = FMath::Max(Tolerance,
            Was && Last ? FMath::Min(Was->Depth, Last->Depth) + DepthTolerance : 0.f);
        if (Contact.Value.Depth > Allowed)
        { Hit = Contact.Value.Hit; return false; }
    }
    return true;
}

bool SweepAllowed(UWorld* World, const FVector& From, const FVector& To, const FQuat& Rotation,
    const FCollisionShape& Shape, ECollisionChannel Channel, const FCollisionQueryParams& Query,
    const FCollisionResponseParams& Responses, const FContacts& Initial, const FContacts& Current,
    float Tolerance, FHitResult& Blocking)
{
    if (From.Equals(To, .001f)) return true;
    TArray<FHitResult> Hits;
    World->SweepMultiByChannel(Hits, From, To, Rotation, Channel, Shape, Query, Responses);
    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.bBlockingHit) continue;
        auto* Part = Hit.GetComponent();
        const auto* Was = Initial.Find(Part);
        const auto* Now = Current.Find(Part);
        // Initial overlap is distinct from a fresh collision along the path.
        // Its depth is constrained by ContactsAllowed and must ultimately ease.
        if (Hit.bStartPenetrating && Was && (!Now || Now->Depth <= Was->Depth + DepthTolerance)) continue;
        if (Tolerance > 0.f && Hit.Time > .95f && Now && Now->Depth <= Tolerance) continue;
        Blocking = Hit;
        return false;
    }
    return true;
}

FString HitName(const FHitResult& Hit)
{
    return GetNameSafe(Hit.GetActor()) + TEXT(".") + GetNameSafe(Hit.GetComponent());
}
}

void UTallowMimMovementContract::BuildCollisionPolicy(const UCapsuleComponent* Capsule,
    FCollisionQueryParams& Query, FCollisionResponseParams& Responses)
{
    if (!Capsule) return;
    Capsule->InitSweepCollisionParams(Query, Responses);
    Query.AddIgnoredActor(Capsule->GetOwner());
    Query.bFindInitialOverlaps = true;
}

FVector2D UTallowMimMovementContract::PostureSize(ETallowMimPosture Posture)
{
    return Posture == ETallowMimPosture::Compact ? FVector2D(24.f, 38.f) : FVector2D(34.f, 50.f);
}

FTallowMimConfiguration UTallowMimMovementContract::GetConfiguration(ACharacter* Character)
{
    FTallowMimConfiguration State;
    if (!Character) return State;
    State.Position = Character->GetActorLocation();
    State.Rotation = Character->GetActorRotation();
    if (const auto* Capsule = Character->GetCapsuleComponent())
        State.Posture = Capsule->GetUnscaledCapsuleRadius() <= 24.5f && Capsule->GetUnscaledCapsuleHalfHeight() <= 38.5f
            ? ETallowMimPosture::Compact : ETallowMimPosture::Normal;
    if (Character->GetMesh()) State.MeshOffset = Character->GetMesh()->GetRelativeLocation();
    return State;
}

FTallowMimTransitionResult UTallowMimMovementContract::ValidateTransition(ACharacter* Character,
    const FTallowMimConfiguration& From, const FTallowMimConfiguration& To, bool bRequireFloor)
{
    FTallowMimTransitionResult Result;
    Result.TargetPosition = To.Position;
    const auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
    auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    auto* Mesh = Character ? Character->GetMesh() : nullptr;
    if (!Capsule || !Movement || !Mesh || !Character->GetWorld()
        || From.Position.ContainsNaN() || To.Position.ContainsNaN()
        || From.Rotation.ContainsNaN() || To.Rotation.ContainsNaN())
    {
        Result.bCapsuleClear = Result.bBodyClear = Result.bFloorSupported = false;
        Result.FailureKind = TEXT("InvalidConfiguration"); Result.Reason = TEXT("missing evaluated character/configuration");
        return Result;
    }
    UWorld* World = Character->GetWorld();
    const float Scale = Capsule->GetShapeScale();
    const FVector2D StartSize = PostureSize(From.Posture) * Scale, EndSize = PostureSize(To.Posture) * Scale;
    Result.Radius = EndSize.X; Result.HalfHeight = EndSize.Y;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimMovementContract), false, Character);
    FCollisionResponseParams Responses;
    BuildCollisionPolicy(Capsule, Query, Responses);
    const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
    const FTransform ActualFrame = Character->GetActorTransform();
    const FVector ActualOffset = Mesh->GetRelativeLocation();
    const FTransform MeshFrame = Mesh->GetRelativeTransform();
    const auto& BonePose = Mesh->GetComponentSpaceTransforms();
    TArray<FBodyPart, TInlineAllocator<2>> Parts;
    const int32 Chest = Mesh->GetBoneIndex(TEXT("chest")), Abdomen = Mesh->GetBoneIndex(TEXT("abdomen_02")),
        Head = Mesh->GetBoneIndex(TEXT("head"));
    if (Chest == INDEX_NONE || Abdomen == INDEX_NONE || Head == INDEX_NONE
        || !BonePose.IsValidIndex(Chest) || !BonePose.IsValidIndex(Abdomen) || !BonePose.IsValidIndex(Head))
    {
        Result.bBodyClear = false;
        Result.FailureKind = TEXT("MissingBodyProxy"); Result.Reason = TEXT("torso/head bones have not evaluated");
    }
    else
    {
        // Component-space pose plus the relative mesh transform remains valid
        // while CharacterMovement defers child transforms during StepUp.
        auto InActor = [&](int32 Bone)
        { return MeshFrame.TransformPosition(BonePose[Bone].GetLocation()) * ActualFrame.GetScale3D(); };
        const FVector A = InActor(Chest), B = InActor(Abdomen);
        const FVector Along = B - A;
        Parts.Add({(A + B) * .5f, Along.Size() > .01f ? FRotationMatrix::MakeFromZ(Along).ToQuat() : FQuat::Identity,
            FCollisionShape::MakeCapsule(26.f * Scale, Along.Size() * .5f + 26.f * Scale), TEXT("Torso")});
        Parts.Add({InActor(Head), FQuat::Identity,
            FCollisionShape::MakeSphere(24.f * Scale), TEXT("Head")});
    }
    const FQuat StartRotation = From.Rotation.Quaternion(), EndRotation = To.Rotation.Quaternion();
    float Reach = StartSize.Y;
    for (const auto& Part : Parts) Reach = FMath::Max(Reach, float(Part.Center.Size() + Part.Shape.GetExtent().Size()));
    const float Angle = StartRotation.AngularDistance(EndRotation);
    const float Distance = FVector::Dist(From.Position, To.Position)
        + FVector::Dist(From.MeshOffset, To.MeshOffset) + Angle * Reach;
    const int32 Steps = FMath::Clamp(FMath::Max(FMath::CeilToInt(Distance / 2.f),
        FMath::Max(FMath::CeilToInt(FMath::RadiansToDegrees(Angle) / 3.f),
            FMath::CeilToInt(float((EndSize - StartSize).GetAbsMax()) / .5f))), 1, 1024);
    FContacts InitialCapsule, PreviousCapsule;
    TArray<FContacts, TInlineAllocator<2>> InitialBody, PreviousBody;
    InitialBody.SetNum(Parts.Num()); PreviousBody.SetNum(Parts.Num());
    FHitResult CapsuleBlock, BodyBlock, FloorBlock;
    FVector PreviousPosition = From.Position;
    FVector2D PreviousSize = StartSize;
    FQuat PreviousRotation = StartRotation;
    TArray<FVector, TInlineAllocator<2>> PreviousPartCenters;
    PreviousPartCenters.SetNum(Parts.Num());
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        const float Alpha = float(Step) / Steps;
        const FVector At = FMath::Lerp(From.Position, To.Position, Alpha);
        const FQuat Rotation = FQuat::Slerp(StartRotation, EndRotation, Alpha).GetNormalized();
        const FVector2D Size = FMath::Lerp(StartSize, EndSize, Alpha);
        const auto Shape = FCollisionShape::MakeCapsule(Size.X, Size.Y);
        FContacts Current;
        ReadContacts(World, At, Rotation, Shape, Channel, Query, Responses, Current);
        if (Step == 0)
        {
            InitialCapsule = PreviousCapsule = Current;
            Result.InitialDepth = ReportedDepth(Current);
        }
        else
        {
            if (!ContactsAllowed(Current, InitialCapsule, PreviousCapsule, 0.f, CapsuleBlock)) Result.bCapsuleClear = false;
            const auto SweptShape = FCollisionShape::MakeCapsule(FMath::Min(PreviousSize.X, Size.X), FMath::Min(PreviousSize.Y, Size.Y));
            if (!SweepAllowed(World, PreviousPosition, At, PreviousRotation, SweptShape, Channel, Query,
                Responses, InitialCapsule, Current, 0.f, CapsuleBlock)) Result.bCapsuleClear = false;
        }
        PreviousCapsule = Current;
        if (bRequireFloor)
        {
            FHitResult Floor;
            const FVector Base = At - FVector::UpVector * Size.Y;
            if (!World->LineTraceSingleByChannel(Floor, Base + FVector::UpVector * (Movement->MaxStepHeight + 4.f),
                Base - FVector::UpVector * 120.f, Channel, Query, Responses) || !Movement->IsWalkable(Floor)
                || Floor.ImpactPoint.Z > Base.Z + Movement->MaxStepHeight + .1f
                || Base.Z - Floor.ImpactPoint.Z > Movement->MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST)
            { Result.bFloorSupported = false; FloorBlock = Floor; }
        }
        for (int32 I = 0; I < Parts.Num(); ++I)
        {
            const auto& Part = Parts[I];
            const FVector PartAt = At + Rotation.RotateVector(Part.Center
                + (FMath::Lerp(From.MeshOffset, To.MeshOffset, Alpha) - ActualOffset) * ActualFrame.GetScale3D());
            const FQuat PartRotation = Rotation * Part.Rotation;
            FContacts Contacts;
            ReadContacts(World, PartAt, PartRotation, Part.Shape, Channel, Query, Responses, Contacts);
            if (Step == 0)
            {
                InitialBody[I] = PreviousBody[I] = Contacts;
                const float Depth = ReportedDepth(Contacts);
                Result.InitialDepth = Result.InitialDepth < 0.f || Depth < 0.f ? -1.f : FMath::Max(Result.InitialDepth, Depth);
            }
            else
            {
                if (!ContactsAllowed(Contacts, InitialBody[I], PreviousBody[I], BodyTolerance, BodyBlock)) Result.bBodyClear = false;
                if (!SweepAllowed(World, PreviousPartCenters[I], PartAt, PreviousRotation * Part.Rotation,
                    Part.Shape, Channel, Query, Responses, InitialBody[I], Contacts, BodyTolerance, BodyBlock)) Result.bBodyClear = false;
            }
            PreviousBody[I] = MoveTemp(Contacts); PreviousPartCenters[I] = PartAt;
        }
        PreviousPosition = At; PreviousRotation = Rotation; PreviousSize = Size;
    }
    // Remaining initial penetration must improve, even if the path contains
    // no fresh hit. A stationary overlapping endpoint is never called clear.
    if (!InitialCapsule.IsEmpty() && MaximumDepth(PreviousCapsule) >= MaximumDepth(InitialCapsule) - .01f)
    {
        Result.bCapsuleClear = false;
        const auto Contact = PreviousCapsule.CreateConstIterator();
        if (Contact) CapsuleBlock = Contact.Value().Hit;
    }
    for (int32 I = 0; I < Parts.Num(); ++I)
        if (MaximumDepth(InitialBody[I]) > BodyTolerance
            && MaximumDepth(PreviousBody[I]) >= MaximumDepth(InitialBody[I]) - .01f)
        {
            Result.bBodyClear = false;
            const auto Contact = PreviousBody[I].CreateConstIterator();
            if (Contact) BodyBlock = Contact.Value().Hit;
        }
    Result.bValid = Result.bCapsuleClear && Result.bBodyClear && Result.bFloorSupported;
    if (!Result.bBodyClear)
    { Result.FailureKind = TEXT("TorsoHeadCollision"); Result.Hit = BodyBlock; }
    else if (!Result.bCapsuleClear)
    { Result.FailureKind = TEXT("CapsuleCollision"); Result.Hit = CapsuleBlock; }
    else if (!Result.bFloorSupported)
    { Result.FailureKind = TEXT("FloorSupport"); Result.Hit = FloorBlock; }
    Result.Blocker = Result.bValid ? FString() : HitName(Result.Hit);
    const FString InitialDescription = Result.InitialDepth < 0.f ? TEXT("initial depth unavailable")
        : FString::Printf(TEXT("initial depth%.2fcm"), Result.InitialDepth);
    Result.Reason = Result.bValid ? TEXT("capsule, torso/head and transition clear")
        : FString::Printf(TEXT("%s %s; %s %.1f/%.1fcm, %s"),
            *Result.FailureKind.ToString(), *Result.Blocker,
            To.Posture == ETallowMimPosture::Compact ? TEXT("Compact") : TEXT("Normal"),
            Result.Radius, Result.HalfHeight, *InitialDescription);
    return Result;
}

FTallowMimTransitionResult UTallowMimMovementContract::ValidateGroundMove(ACharacter* Character,
    const FVector& Toward, float DesiredYaw, ETallowMimPosture Posture, float MaxDistance)
{
    FTallowMimConfiguration From = GetConfiguration(Character), To = From;
    if (!Character || !Character->GetCharacterMovement()) return ValidateTransition(Character, From, To, true);
    auto* Movement = Character->GetCharacterMovement();
    const float Scale = Character->GetCapsuleComponent()->GetShapeScale();
    const FVector2D NewSize = PostureSize(Posture) * Scale;
    To.Posture = Posture; To.Rotation = FRotator(0.f, DesiredYaw, 0.f);
    const float HeightChange = NewSize.Y - PostureSize(From.Posture).Y * Scale;
    To.Position.Z += HeightChange; To.MeshOffset.Z -= HeightChange / FMath::Max(Scale, .01f);
    const FVector Delta = (Toward - From.Position).GetSafeNormal2D() * FMath::Min(
        FVector::Dist2D(From.Position, Toward), FMath::Clamp(MaxDistance, 0.f, 64.f));
    To.Position += Delta;
    FFindFloorResult CurrentFloor;
    Movement->FindFloor(From.Position, CurrentFloor, false);
    if (!CurrentFloor.IsWalkableFloor())
    {
        auto Result = ValidateTransition(Character, From, To, false);
        Result.bValid = Result.bFloorSupported = false; Result.FailureKind = TEXT("FloorSupport");
        Result.Reason = TEXT("current capsule has no walkable support"); return Result;
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimGroundContract), false, Character);
    FCollisionResponseParams Responses; BuildCollisionPolicy(Character->GetCapsuleComponent(), Query, Responses);
    const auto Channel = Character->GetCapsuleComponent()->GetCollisionObjectType();
    const auto ProposedShape = FCollisionShape::MakeCapsule(NewSize.X, NewSize.Y);
    const float StepLift = FMath::Max(0.f, Movement->MaxStepHeight - CurrentFloor.GetDistanceToFloor());
    const float SearchDistance = StepLift + Movement->MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST * 2.f;
    FFindFloorResult NextFloor;
    auto FindProposedFloor = [&](const FVector& Probe)
    {
        FHitResult Hit;
        if (!Movement->FloorSweepTest(Hit, Probe, Probe - FVector::UpVector * SearchDistance,
            Channel, ProposedShape, Query, Responses) || !Movement->IsWalkable(Hit)
            || !Movement->IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, NewSize.X)) return false;
        NextFloor.SetFromSweep(Hit, float(Probe.Z - Hit.Location.Z), true);
        return true;
    };
    FVector Probe = To.Position;
    bool bFoundFloor = FindProposedFloor(Probe);
    if (!bFoundFloor)
    {
        // A leading capsule hemisphere can overlap the next riser before
        // its centre reaches that tread. Probe from the same raised corridor
        // used by StepUp; a centreline ray would select the lower tread.
        const FVector RaiseFrom = From.Position + FVector::UpVector * HeightChange;
        FHitResult UpHit;
        float AvailableLift = StepLift;
        if (Character->GetWorld()->SweepSingleByChannel(UpHit, RaiseFrom,
            RaiseFrom + FVector::UpVector * StepLift, From.Rotation.Quaternion(),
            Channel, ProposedShape, Query, Responses))
            AvailableLift = UpHit.bStartPenetrating ? 0.f : FMath::Max(0.f, StepLift * UpHit.Time - .1f);
        Probe.Z += AvailableLift;
        bFoundFloor = AvailableLift > .01f && FindProposedFloor(Probe);
    }
    const float Gap = FMath::Clamp(CurrentFloor.GetDistanceToFloor(), UCharacterMovementComponent::MIN_FLOOR_DIST,
        UCharacterMovementComponent::MAX_FLOOR_DIST);
    const float CurrentSupportBase = From.Position.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
        - CurrentFloor.GetDistanceToFloor();
    const float NextSupportBase = bFoundFloor ? Probe.Z - NextFloor.GetDistanceToFloor() - NewSize.Y : CurrentSupportBase;
    const float Rise = NextSupportBase - CurrentSupportBase;
    if (!bFoundFloor || FMath::Abs(Rise) > Movement->MaxStepHeight + .1f)
    {
        auto Result = ValidateTransition(Character, From, To, false);
        Result.bValid = Result.bFloorSupported = false; Result.FailureKind = TEXT("FloorSupport");
        Result.Reason = TEXT("proposed capsule sweep lacks a walkable step"); Result.Hit = NextFloor.HitResult; return Result;
    }
    To.Position.Z = Probe.Z - NextFloor.GetDistanceToFloor() + Gap;
    auto Direct = ValidateTransition(Character, From, To, true);
    if (Direct.bValid || Direct.bCapsuleClear) return Direct;
    if (Rise <= 0.f || !Movement->CanStepUp(Direct.Hit)) return Direct;
    FTallowMimConfiguration Raised = From;
    Raised.Position.Z += FMath::Max(0.f, Movement->MaxStepHeight - CurrentFloor.GetDistanceToFloor());
    FTallowMimConfiguration Across = To; Across.Position.Z = Raised.Position.Z + HeightChange;
    const auto Up = ValidateTransition(Character, From, Raised, false);
    if (!Up.bValid) return Up;
    const auto Forward = ValidateTransition(Character, Raised, Across, false);
    if (!Forward.bValid) return Forward;
    auto Down = ValidateTransition(Character, Across, To, false);
    Down.bFloorSupported = true; Down.TargetPosition = To.Position;
    return Down;
}

FTallowMimTransitionResult UTallowMimMovementContract::TryChangePosture(ACharacter* Character, ETallowMimPosture Posture)
{
    const auto From = GetConfiguration(Character);
    auto To = From;
    if (!Character || !Character->GetCapsuleComponent()) return ValidateTransition(Character, From, To, true);
    auto* Capsule = Character->GetCapsuleComponent(); auto* Movement = Character->GetCharacterMovement();
    const FVector2D Size = PostureSize(Posture);
    const float Scale = Capsule->GetShapeScale();
    const float HeightChange = Size.Y * Scale - Capsule->GetScaledCapsuleHalfHeight();
    To.Posture = Posture; To.Position.Z += HeightChange; To.MeshOffset.Z -= HeightChange / FMath::Max(Scale, .01f);
    auto Result = ValidateTransition(Character, From, To, true);
    const float OldRadius = Capsule->GetUnscaledCapsuleRadius(), OldHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    const bool bShrink = Size.Y < OldHeight || Size.X < OldRadius;
    // Travel policy (see header): shrinking is always attempted -- smaller cannot
    // fit worse, and the swept execution below verifies it (restoring on mismatch).
    // Growing back up stays strict so she never expands into geometry.
    if (!Result.bValid && !bShrink) return Result;
    const bool bShrinkWithBrush = !Result.bValid;
    if (bShrink) Capsule->SetCapsuleSize(Size.X, Size.Y, true);
    FHitResult Hit;
    Capsule->MoveComponent(To.Position - From.Position, To.Rotation.Quaternion(), true, &Hit);
    if (Hit.bBlockingHit || !Capsule->GetComponentLocation().Equals(To.Position, .1f))
    {
        Capsule->SetWorldLocation(From.Position, false, nullptr, ETeleportType::TeleportPhysics);
        Capsule->SetCapsuleSize(OldRadius, OldHeight, true);
        Result.bValid = Result.bCapsuleClear = false; Result.FailureKind = TEXT("ExecutionMismatch");
        Result.Hit = Hit; Result.Blocker = HitName(Hit); Result.Reason = TEXT("movement component rejected validated posture change; restored previous state");
        return Result;
    }
    if (!bShrink) Capsule->SetCapsuleSize(Size.X, Size.Y, true);
    Character->GetMesh()->SetRelativeLocation(To.MeshOffset);
    Movement->bForceNextFloorCheck = true;
    if (bShrinkWithBrush)
    {
        // Executed, so report valid; bBodyClear stays false for diagnostics.
        Result.bValid = true;
        Result.Reason = TEXT("compact executed with torso brush; capsule and floor clear: ") + Result.Reason;
    }
    return Result;
}
