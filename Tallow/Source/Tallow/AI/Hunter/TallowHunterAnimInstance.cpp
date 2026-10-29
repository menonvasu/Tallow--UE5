#include "AI/Hunter/TallowHunterAnimInstance.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Misc/MemStack.h"
#include "TwoBoneIK.h"

bool UTallowHunterAnimInstance::PlayMotionClip(UAnimSequence* Clip, bool bLoop,
    float Rate, float BlendSeconds, bool bMatchPhase)
{
    if (!Clip) return false;
    const bool Changed = GetActiveClip() != Clip;
    UAnimSequence* Previous = GetActiveClip();
    const float PreviousTime = GetPlaybackPosition();
    if (!PlayClip(Clip, bLoop, Rate, BlendSeconds)) return false;
    if (Changed)
    {
        bInterrupted = TransitionAge < TransitionDuration;
        OutgoingClip = Previous; OutgoingTime = PreviousTime;
        OutgoingRate = MotionRate; bOutgoingLoop = bMotionLoop;
        TransitionAge = 0.f; TransitionDuration = FMath::Max(.001f, BlendSeconds);
        ++MotionGeneration;
        if (bMatchPhase && Previous && Previous->GetPlayLength() > UE_SMALL_NUMBER)
            SetPlaybackPosition((PreviousTime / Previous->GetPlayLength()) * Clip->GetPlayLength());
    }
    MotionClip = Clip; MotionRate = Rate; bMotionLoop = bLoop;
    return true;
}

void UTallowHunterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    TransitionAge += FMath::Max(0.f, DeltaSeconds);
    if (OutgoingClip)
    {
        const float Length = OutgoingClip->GetPlayLength();
        OutgoingTime += FMath::Max(0.f, DeltaSeconds) * OutgoingRate;
        OutgoingTime = bOutgoingLoop && Length > UE_SMALL_NUMBER
            ? FMath::Fmod(OutgoingTime, Length) : FMath::Clamp(OutgoingTime, 0.f, Length);
        if (TransitionAge >= TransitionDuration) OutgoingClip = nullptr;
    }
}

struct FTallowHunterAnimProxy : FAnimInstanceProxy
{
    explicit FTallowHunterAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    struct FContact { int32 Upper, Lower, Hand; FVector Target, Pole; FQuat Rotation; float Alpha; TArray<int32> Digits; TArray<FQuat> DigitRest; float Flat; };
    struct FClothFollow { int32 Bone; float Weight; FVector Offset; FQuat Rotation; };
    UAnimSequence* Clip = nullptr;
    UAnimSequence* Outgoing = nullptr;
    float Time = 0.f, OutgoingTime = 0.f, Blend = 1.f, InterruptBlend = 1.f;
    uint32 Generation = 0;
    int32 HeadIndex = INDEX_NONE, BodyIndex = INDEX_NONE, NeckIndex = INDEX_NONE;
    FQuat NeckRotation = FQuat::Identity;
    float IdleClock = 0.f;
    FVector Look = FVector(0, 1, 0), BodyTranslation = FVector::ZeroVector;
    FQuat BodyRotation = FQuat::Identity;
    FQuat SpineRot[5] = {FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity, FQuat::Identity};
    int32 SpineIndex[5] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
    bool bContactLayer = true, bInterrupted = false;
    bool bNarrowPortal = false;
    FVector PortalCenter, PortalRight, PortalForward;
    float PortalHalfWidth = 1000.f, Squeeze = 0.f, LastClothUpdateTime = -1.f;
    TArray<FContact> Contacts;
    TArray<FClothFollow> Cloth;
    TArray<FTransform> LastPose, SourcePose;
    TArray<FVector> PreviousClothOffsets;
    TArray<FVector> ClothPos, ClothPrev;
    TArray<float> ClothRest, ClothRing;
    FVector ClothAnchor = FVector::ZeroVector;
    TArray<FVector> HemTailAxes;
    TMap<int32, FQuat> ContactFingerRotations;
    // Liquid output layer: what the step logic asks of each hand is followed, never jumped to.
    TArray<FVector> SmoothTarget, SmoothTargetVel, SmoothPole, SmoothPoleVel;
    TArray<FQuat> SmoothRot;
    TArray<float> SmoothWeight;
    FVector LastActorWorld = FVector::ZeroVector;
    static void Follow(FVector& X, FVector& V, const FVector& Goal, float Frequency, float Delta)
    {
        // Critically damped: it arrives as fast as it can without overshooting or ringing.
        float Remaining = FMath::Min(Delta, .1f);
        while (Remaining > SMALL_NUMBER)
        {
            const float Step = FMath::Min(Remaining, 1.f / 120.f);
            V += ((Goal - X) * Frequency * Frequency - V * (2.f * Frequency)) * Step;
            X += V * Step; Remaining -= Step;
        }
    }
    // Hood chain behind the neck (V12 rig, DEF_hood_1..3): hangs from the spine and swings as cloth.
    int32 HoodIndex[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
    FVector HoodPos[3], HoodPrev[3];
    bool bHoodLive = false;
    FTransform CompToWorld = FTransform::Identity;
    float HoodDt = 1.f / 60.f;
    FBlendedHeapCurve LastCurve, SourceCurve;

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const auto* State = CastChecked<UTallowHunterAnimInstance>(Instance);
        if (Generation != State->MotionGeneration)
        {
            SourcePose = LastPose; SourceCurve.CopyFrom(LastCurve);
            Generation = State->MotionGeneration;
        }
        Clip = State->GetActiveClip(); Time = State->GetPlaybackPosition();
        Outgoing = State->OutgoingClip; OutgoingTime = State->OutgoingTime;
        Blend = FMath::SmoothStep(0.f, State->TransitionDuration, State->TransitionAge);
        bInterrupted = State->bInterrupted;
        InterruptBlend = FMath::SmoothStep(0.f, .12f, State->TransitionAge);
        // The presentation component owns capture playback. Its approved opening is
        // evaluated directly, without the locomotion crossfade or procedural offsets.
        if (Clip != State->MotionClip) { Outgoing = nullptr; Blend = 1.f; }
        Contacts.Reset(); Cloth.Reset(); BodyTranslation = FVector::ZeroVector; BodyRotation = FQuat::Identity;
        auto* Mesh = Instance->GetSkelMeshComponent();
        auto* Motion = Mesh && Mesh->GetOwner() ? Mesh->GetOwner()->FindComponentByClass<UTallowHunterMotionComponent>() : nullptr;
        const auto* Pawn = Mesh ? Cast<APawn>(Mesh->GetOwner()) : nullptr;
        const auto* Hunter = Pawn ? Cast<ATallowHunterAIController>(Pawn->GetController()) : nullptr;
        (void)Hunter;
        // The capture is procedural now (UpdateCapture): the contact layer carries the grab too.
        bContactLayer = true;
        if (!Motion || !bContactLayer) return;
        const FTransform ToWorld = Mesh->GetComponentTransform();
        const auto& Hands = Motion->GetHands();
        const float Dt = FMath::Clamp(DeltaSeconds, 1.f / 240.f, 1.f / 15.f);
        const bool bJumped = FVector::Dist(LastActorWorld, ToWorld.GetLocation()) > 120.f;
        LastActorWorld = ToWorld.GetLocation();
        if (SmoothTarget.Num() != Hands.Num() || bJumped)
        {
            SmoothTarget.SetNum(Hands.Num()); SmoothTargetVel.SetNum(Hands.Num()); SmoothPole.SetNum(Hands.Num());
            SmoothPoleVel.SetNum(Hands.Num()); SmoothRot.SetNum(Hands.Num()); SmoothWeight.SetNum(Hands.Num());
            for (int32 I = 0; I < Hands.Num(); ++I)
            {
                SmoothTarget[I] = Hands[I].Target; SmoothPole[I] = Hands[I].Pole; SmoothRot[I] = Hands[I].Rotation;
                SmoothTargetVel[I] = SmoothPoleVel[I] = FVector::ZeroVector; SmoothWeight[I] = Hands[I].Weight;
            }
        }
        for (int32 HandIndex = 0; HandIndex < Hands.Num(); ++HandIndex)
        {
            const auto& H = Hands[HandIndex];
            // World-space follow: a planted hand's target is fixed in the world, so it stays planted.
            Follow(SmoothTarget[HandIndex], SmoothTargetVel[HandIndex], H.Target, 11.f, Dt);
            Follow(SmoothPole[HandIndex], SmoothPoleVel[HandIndex], H.Pole, 7.f, Dt);
            SmoothRot[HandIndex] = FQuat::Slerp(SmoothRot[HandIndex], H.Rotation, 1.f - FMath::Exp(-9.f * Dt)).GetNormalized();
            // Arms ease between planted and free over about a quarter second instead of snapping.
            SmoothWeight[HandIndex] = FMath::FInterpTo(SmoothWeight[HandIndex], H.Weight, Dt, H.Weight > SmoothWeight[HandIndex] ? 6.f : 3.5f);
            FContact Contact{Mesh->GetBoneIndex(H.Upper), Mesh->GetBoneIndex(H.Lower), Mesh->GetBoneIndex(H.Hand),
                ToWorld.InverseTransformPosition(SmoothTarget[HandIndex]), ToWorld.InverseTransformPosition(SmoothPole[HandIndex]),
                (ToWorld.GetRotation().Inverse() * SmoothRot[HandIndex]).GetNormalized(),
                FMath::SmoothStep(0.f, 1.f, SmoothWeight[HandIndex]), {}};
            const FString Prefix = H.Hand.ToString().LeftChop(4);
            for (int32 Digit = 1; Digit <= 5; ++Digit) for (const TCHAR* Segment : {TEXT("a"), TEXT("b"), TEXT("c")})
            {
                const int32 DigitIndex = Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("%sdigit%d_%s"), *Prefix, Digit, Segment)));
                Contact.Digits.Add(DigitIndex);
                const auto& Reference = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
                Contact.DigitRest.Add(DigitIndex != INDEX_NONE ? Reference.GetRefBonePose()[DigitIndex].GetRotation() : FQuat::Identity);
            }
            Contacts.Add(MoveTemp(Contact));
        }
        bNarrowPortal = Motion->HasNarrowPortal();
        PortalCenter = ToWorld.InverseTransformPosition(Motion->GetPortalCenter());
        PortalRight = ToWorld.InverseTransformVectorNoScale(Motion->GetPortalRight()).GetSafeNormal();
        PortalForward = ToWorld.InverseTransformVectorNoScale(Motion->GetPortalForward()).GetSafeNormal();
        PortalHalfWidth = Motion->GetPortalHalfWidth() / ToWorld.GetScale3D().GetAbsMax();
        Squeeze = Motion->GetSqueezeAmount();
        HeadIndex = Mesh->GetBoneIndex(TEXT("head")); BodyIndex = Mesh->GetBoneIndex(TEXT("DEF_body"));
        Look = ToWorld.InverseTransformVectorNoScale(Motion->GetLookDirection()).GetSafeNormal();
        IdleClock = Motion->GetIdleTime();
        NeckIndex = Mesh->GetBoneIndex(TEXT("DEF_neck"));
        for (int32 K = 0; K < 3; ++K) HoodIndex[K] = Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("DEF_hood_%d"), K + 1)));
        CompToWorld = ToWorld; HoodDt = FMath::Clamp(DeltaSeconds, 1.f / 240.f, 1.f / 20.f);
        NeckRotation = (ToWorld.GetRotation().Inverse() * Motion->GetNeckWorldRotation() * ToWorld.GetRotation()).GetNormalized();
        BodyTranslation = ToWorld.InverseTransformVector(Motion->GetBodyTranslation());
        BodyRotation = (ToWorld.GetRotation().Inverse() * Motion->GetBodyWorldRotation() * ToWorld.GetRotation()).GetNormalized();
        for (int32 I = 0; I < 5; ++I)
        {
            SpineRot[I] = (ToWorld.GetRotation().Inverse() * Motion->GetSpineWorldRotation(I) * ToWorld.GetRotation()).GetNormalized();
            SpineIndex[I] = I == 0 ? BodyIndex : Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("DEF_spine_%d"), I)));
        }
        // Cloth03 hem bones are root siblings. Follow the torso only near the collar,
        // leaving the lower folded material and planted fringe near their baked surface.
        const float Follow[] = {1.f, 1.f, .72f, .38f, .13f, 0.f, 0.f};
        for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 0; Segment < 7; ++Segment)
        {
            const int32 Bone = Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("DEF_hem_%02d_%d"), Strip, Segment)));
            if (Bone != INDEX_NONE) Cloth.Add({Bone, Follow[Segment], FVector::ZeroVector, FQuat::Identity});
        }
        const int32 Underside = Mesh->GetBoneIndex(TEXT("DEF_hem_underside"));
        if (Underside != INDEX_NONE) Cloth.Add({Underside, 0.f, FVector::ZeroVector, FQuat::Identity});
        UpdateClothContacts(Mesh, Motion, DeltaSeconds);
    }

    void EvaluateBase(FPoseContext& Output)
    {
        Output.ResetToRefPose();
        if (!Clip) return;
        FAnimationPoseData Target(Output);
        FAnimExtractContext Context(static_cast<double>(Time), false);
        Clip->GetAnimationPose(Target, Context);
        if (Outgoing && Blend < 1.f)
        {
            FPoseContext Previous(this); Previous.ResetToRefPose();
            FAnimationPoseData Prior(Previous);
            FAnimExtractContext Earlier(static_cast<double>(OutgoingTime), false);
            Outgoing->GetAnimationPose(Prior, Earlier);
            if (bInterrupted && InterruptBlend < 1.f && SourcePose.Num() == Output.Pose.GetNumBones())
            {
                FPoseContext Snapshot(this); Snapshot.ResetToRefPose();
                for (const FCompactPoseBoneIndex Index : Snapshot.Pose.ForEachBoneIndex()) Snapshot.Pose[Index] = SourcePose[Index.GetInt()];
                Snapshot.Curve.CopyFrom(SourceCurve);
                FAnimationPoseData Frozen(Snapshot);
                FAnimationRuntime::BlendTwoPosesTogetherInPlace(Prior, Frozen, InterruptBlend);
            }
            FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, Prior, Blend);
        }
    }

    template <typename FRebuild>
    void SimulateHood(FPoseContext& Output, TArray<FTransform>& CS, FRebuild& Rebuild)
    {
        // Each hood bone's tip is a verlet particle in world space: it trails the body when it
        // accelerates or turns, sags a little under gravity and is pulled back to its held shape.
        const auto& Bones = Output.Pose.GetBoneContainer();
        FCompactPoseBoneIndex Index[3] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
        for (int32 K = 0; K < 3; ++K)
        {
            if (HoodIndex[K] == INDEX_NONE) { bHoodLive = false; return; }
            Index[K] = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HoodIndex[K]));
            if (Index[K] == INDEX_NONE) { bHoodLive = false; return; }
        }
        const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
        FVector TipLocal[3];
        TipLocal[0] = Ref.GetRefBonePose()[HoodIndex[1]].GetLocation();
        TipLocal[1] = Ref.GetRefBonePose()[HoodIndex[2]].GetLocation();
        TipLocal[2] = TipLocal[1] * .9f;
        for (int32 K = 0; K < 3; ++K)
        {
            const FTransform& Bone = CS[Index[K].GetInt()];
            const FVector Base = CompToWorld.TransformPosition(Bone.GetLocation());
            const FVector Held = CompToWorld.TransformPosition(Bone.TransformPosition(TipLocal[K]));
            const float Length = FVector::Dist(Base, Held);
            if (!bHoodLive || FVector::Dist(HoodPos[K], Held) > 120.f) { HoodPos[K] = HoodPrev[K] = Held; continue; }
            const FVector Velocity = (HoodPos[K] - HoodPrev[K]) * .9f;
            HoodPrev[K] = HoodPos[K];
            FVector Next = HoodPos[K] + Velocity + (Held - HoodPos[K]) * (.22f - K * .05f)
                + FVector(0, 0, -980.f) * HoodDt * HoodDt * .3f;
            // Keep the bone length and never swing more than 55 degrees off the held drape.
            FVector Dir = (Next - Base).GetSafeNormal();
            const FVector HeldDir = (Held - Base).GetSafeNormal();
            const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, HeldDir), -1.f, 1.f));
            const float Limit = FMath::DegreesToRadians(55.f);
            if (Angle > Limit) Dir = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(HeldDir, Dir), Limit / Angle).RotateVector(HeldDir);
            HoodPos[K] = Base + Dir * Length;
            const FVector CurrentCS = (Bone.TransformPosition(TipLocal[K]) - Bone.GetLocation()).GetSafeNormal();
            const FVector WantedCS = CompToWorld.InverseTransformVectorNoScale(Dir).GetSafeNormal();
            FTransform Adjusted = Bone;
            Adjusted.SetRotation((FQuat::FindBetweenNormals(CurrentCS, WantedCS) * Adjusted.GetRotation()).GetNormalized());
            const auto Parent = Bones.GetParentBoneIndex(Index[K]);
            if (Parent != INDEX_NONE) Output.Pose[Index[K]] = Adjusted.GetRelativeTransform(CS[Parent.GetInt()]);
            Rebuild();
        }
        bHoodLive = true;
    }

    void UpdateClothContacts(USkeletalMeshComponent* Mesh, UTallowHunterMotionComponent* Motion, float DeltaSeconds)
    {
        Motion->CollisionAdjustedHemBones = 0;
        Motion->MaxHemContactOffset = 0.f;
        if (Cloth.Num() != 85 || !Clip || !Mesh->GetWorld()) return;
        FMemMark PoseMemory(FMemStack::Get());
        FPoseContext Base(this);
        EvaluateBase(Base);
        const auto& Bones = Base.Pose.GetBoneContainer();
        TArray<FTransform> CS;
        CS.SetNum(Base.Pose.GetNumBones());
        for (const FCompactPoseBoneIndex Index : Base.Pose.ForEachBoneIndex())
        {
            const auto Parent = Bones.GetParentBoneIndex(Index);
            CS[Index.GetInt()] = Parent != INDEX_NONE ? Base.Pose[Index] * CS[Parent.GetInt()] : Base.Pose[Index];
        }
        const auto Body = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(BodyIndex));
        if (Body == INDEX_NONE) return;
        const FVector Pivot = CS[Body.GetInt()].GetLocation();
        const FTransform World = Mesh->GetComponentTransform();
        const FVector Up = Mesh->GetOwner()->GetActorUpVector();
        const FVector Forward = Mesh->GetOwner()->GetActorForwardVector();
        const FVector Origin = Mesh->GetComponentLocation();
        const FVector PortalWorld = Motion->GetPortalCenter();
        const FVector PortalSide = Motion->GetPortalRight(), PortalAxis = Motion->GetPortalForward();
        // Reserve the transverse skin span around each cage strip as well as
        // the node itself. Motion04 cleared the nodes but left up to 15 cm of
        // weighted fabric outside the jamb.
        const float SafeHalfWidth = FMath::Max(16.f, Motion->GetPortalHalfWidth() - 30.f);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterHemJambs), false, Mesh->GetOwner());
        if (const auto* Character = Cast<ACharacter>(Mesh->GetOwner()))
            Query.AddIgnoredActors(Character->GetCapsuleComponent()->GetMoveIgnoreActors());
        // Exact source material lengths, measured from the saved original rest rig.
        // The virtual eighth point is essential: the final bone controls a radial
        // 28-43 cm fringe at runtime scale, not another vertical skirt segment.
        static const float TailLengthsCm[] = {16.87842f, 15.62975f, 15.90379f, 18.83770f,
            21.51529f, 14.14788f, 21.00635f, 17.00286f, 14.93611f, 16.55993f, 16.11076f, 15.52489f};
        if (HemTailAxes.Num() != 12)
        {
            const auto& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
            TArray<FTransform> RefCS;
            RefCS.SetNum(Ref.GetNum());
            for (int32 I = 0; I < Ref.GetNum(); ++I)
            {
                const int32 Parent = Ref.GetParentIndex(I);
                RefCS[I] = Parent != INDEX_NONE ? Ref.GetRefBonePose()[I] * RefCS[Parent] : Ref.GetRefBonePose()[I];
            }
            HemTailAxes.Reset();
            for (int32 Strip = 0; Strip < 12; ++Strip)
            {
                const int32 Last = Cloth[Strip * 7 + 6].Bone, Previous = Cloth[Strip * 7 + 5].Bone;
                const FVector Radial = (RefCS[Last].GetLocation() - RefCS[Previous].GetLocation()).GetSafeNormal();
                FVector Axis = FVector::YAxisVector; float Best = -1.f;
                for (const FVector& Candidate : {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector})
                {
                    const float Dot = FVector::DotProduct(RefCS[Last].TransformVectorNoScale(Candidate), Radial);
                    if (FMath::Abs(Dot) > Best) { Best = FMath::Abs(Dot); Axis = Candidate * FMath::Sign(Dot); }
                }
                HemTailAxes.Add(Axis);
            }
        }
        TArray<FVector> Original, Points;
        Original.SetNum(96);
        for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 0; Segment < 7; ++Segment)
        {
            const auto& Follow = Cloth[Strip * 7 + Segment];
            const auto Bone = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Follow.Bone));
            if (Bone == INDEX_NONE) return;
            const FQuat Delta = FQuat::Slerp(FQuat::Identity, BodyRotation, Follow.Weight).GetNormalized();
            const FVector Head = Pivot + Delta.RotateVector(CS[Bone.GetInt()].GetLocation() - Pivot)
                + BodyTranslation * Follow.Weight;
            Original[Strip * 8 + Segment] = World.TransformPosition(Head);
            if (Segment == 6)
                Original[Strip * 8 + 7] = World.TransformPosition(Head
                    + (Delta * CS[Bone.GetInt()].GetRotation()).RotateVector(HemTailAxes[Strip] * TailLengthsCm[Strip]));
        }
        Points = Original;
        auto ProjectContact = [&](FVector& Point, bool Anticipate) {
            if (bNarrowPortal)
            {
                const float Along = FMath::Abs(FVector::DotProduct(Point - PortalWorld, PortalAxis));
                const float Weight = Anticipate ? 1.f - FMath::SmoothStep(35.f, 105.f, Along)
                    : (Along < 35.f ? 1.f : 0.f);
                const float Lateral = FVector::DotProduct(Point - PortalWorld, PortalSide);
                Point += PortalSide * (FMath::Clamp(Lateral, -SafeHalfWidth, SafeHalfWidth) - Lateral) * Weight;
            }
            const float Height = FVector::DotProduct(Point - Origin, Up);
            // Trace across the opening at this fabric point's longitudinal station.
            // A diagonal ray from the actor centre catches the front wall instead
            // of the jamb and incorrectly pulls trailing cloth through that wall.
            const FVector Safe = Origin + Forward * FVector::DotProduct(Point - Origin, Forward)
                + Up * FMath::Max(8.f, Height);
            const FVector Probe = Point + Up * FMath::Max(0.f, 8.f - Height);
            FHitResult Hit;
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, Safe, Probe, ECC_Visibility, Query)
                && FMath::Abs(FVector::DotProduct(Hit.ImpactNormal, Up)) < .6f)
                Point += Hit.ImpactPoint + Hit.ImpactNormal * 7.f - Probe;
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, Point + Up * 22.f, Point - Up * 18.f,
                ECC_Visibility, Query) && FVector::DotProduct(Hit.ImpactNormal, Up) > .55f)
            {
                const float Clearance = FVector::DotProduct(Point - Hit.ImpactPoint, Hit.ImpactNormal);
                if (Clearance < 3.5f) Point += Hit.ImpactNormal * (3.5f - Clearance);
            }
            // On a ceiling the hanging fabric can reach the opposite floor.
            if (Up.Z < .4f && Mesh->GetWorld()->LineTraceSingleByChannel(Hit,
                Point + FVector::UpVector * 120.f, Point - FVector::UpVector * 18.f,
                ECC_Visibility, Query) && Hit.ImpactNormal.Z > .55f)
            {
                const float Clearance = FVector::DotProduct(Point - Hit.ImpactPoint, Hit.ImpactNormal);
                if (Clearance < 3.5f) Point += Hit.ImpactNormal * (3.5f - Clearance);
            }
        };
        // Real cloth. The hem is a set of verlet strips hung from the collar: they fall under
        // gravity, drag and pile on the floor, and are pushed out by the head, torso and arms.
        const int32 Count = 96;
        const float WorldScale = FMath::Max(.01f, float(World.GetScale3D().GetAbsMax()));
        if (ClothPos.Num() != Count || FVector::DistSquared(ClothAnchor, Original[1]) > 160000.f)
        {
            ClothPos = Original; ClothPrev = Original;
            ClothRest.SetNum(Count); ClothRing.SetNum(Count);
            for (int32 I = 0; I < Count; ++I)
            {
                ClothRest[I] = I % 8 == 0 ? 0.f : FVector::Dist(Original[I], Original[I - 1]);
                ClothRing[I] = FVector::Dist(Original[I], Original[(I + 8) % Count]);
            }
        }
        ClothAnchor = Original[1];
        const float Dt = FMath::Clamp(LastClothUpdateTime < 0.f ? DeltaSeconds : FMath::Clamp(
            Mesh->GetWorld()->GetTimeSeconds() - LastClothUpdateTime, 0.f, .1f), 1.f / 240.f, .05f);
        LastClothUpdateTime = Mesh->GetWorld()->GetTimeSeconds();
        const FQuat ToGravity = FQuat::FindBetweenNormals(Up, FVector::UpVector);
        // Soft pull toward the authored flare, held in a gravity-aligned frame so it still hangs
        // correctly when the creature is on a wall or the ceiling.
        TArray<FVector> Target; Target.SetNum(Count);
        for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 0; Segment < 8; ++Segment)
        {
            const int32 I = Strip * 8 + Segment;
            Target[I] = Original[Strip * 8 + 1] + ToGravity.RotateVector(Original[I] - Original[Strip * 8 + 1]);
        }
        // Obstacles: torso, neck, head, and each arm.
        struct FCapsule { FVector A, B; float R; };
        TArray<FCapsule> Obstacles;
        auto Bone = [&](const TCHAR* Name) { return Mesh->GetBoneLocation(FName(Name)); };
        const FVector BodyAt = Bone(TEXT("DEF_body")), NeckAt = Bone(TEXT("DEF_neck")), HeadAt = Bone(TEXT("head"));
        Obstacles.Add({BodyAt - Up * 30.f * WorldScale / 2.f, NeckAt, 62.f});
        Obstacles.Add({NeckAt, HeadAt + (HeadAt - NeckAt) * .5f, 40.f});
        for (int32 Row = 1; Row <= 3; ++Row) for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
        {
            const FString Upper = FString::Printf(TEXT("DEF_arm_%d_%s_upper"), Row, Side);
            const FString Lower = FString::Printf(TEXT("DEF_arm_%d_%s_lower"), Row, Side);
            const FString Hand = FString::Printf(TEXT("DEF_arm_%d_%s_hand"), Row, Side);
            Obstacles.Add({Mesh->GetBoneLocation(FName(*Upper)), Mesh->GetBoneLocation(FName(*Lower)), 20.f});
            Obstacles.Add({Mesh->GetBoneLocation(FName(*Lower)), Mesh->GetBoneLocation(FName(*Hand)), 17.f});
        }
        static const float Shape[] = {0.f, 0.f, .06f, .04f, .03f, .022f, .016f, .012f};
        const int32 Sub = FMath::Clamp(FMath::CeilToInt(Dt * 60.f), 1, 3);
        const float H = Dt / Sub;
        for (int32 Step = 0; Step < Sub; ++Step)
        {
            for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 0; Segment < 8; ++Segment)
            {
                const int32 I = Strip * 8 + Segment;
                if (Segment <= 1) { ClothPrev[I] = ClothPos[I]; ClothPos[I] = Original[I]; continue; }
                const FVector Velocity = (ClothPos[I] - ClothPrev[I]) * .985f;
                ClothPrev[I] = ClothPos[I];
                ClothPos[I] += Velocity + FVector(0, 0, -1050.f) * H * H;
                ClothPos[I] += (Target[I] - ClothPos[I]) * Shape[Segment];
            }
            for (int32 Pass = 0; Pass < 4; ++Pass)
            {
                for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 2; Segment < 8; ++Segment)
                {
                    const int32 I = Strip * 8 + Segment, Prior = I - 1;
                    const FVector Edge = ClothPos[I] - ClothPos[Prior];
                    const float Length = Edge.Size();
                    if (Length < UE_SMALL_NUMBER) continue;
                    const FVector Fix = Edge * ((Length - ClothRest[I]) / Length);
                    if (Segment == 2) ClothPos[I] -= Fix;
                    else { ClothPos[I] -= Fix * .5f; ClothPos[Prior] += Fix * .5f; }
                    // Keep the ring of strips from tearing apart or folding through itself.
                    const int32 Next = Strip * 8 + Segment + 8 < Count ? I + 8 : Segment;
                    const FVector Side = ClothPos[Next] - ClothPos[I];
                    const float SideLength = Side.Size();
                    if (SideLength > UE_SMALL_NUMBER)
                    {
                        const FVector Pull = Side * ((SideLength - ClothRing[I]) / SideLength) * .12f;
                        ClothPos[I] += Pull; ClothPos[Next] -= Pull;
                    }
                }
                for (int32 I = 0; I < Count; ++I)
                {
                    if (I % 8 <= 1) continue;
                    for (const auto& Capsule : Obstacles)
                    {
                        const FVector Segment = Capsule.B - Capsule.A;
                        const float T = FMath::Clamp(FVector::DotProduct(ClothPos[I] - Capsule.A, Segment)
                            / FMath::Max(Segment.SizeSquared(), 1.f), 0.f, 1.f);
                        const FVector Closest = Capsule.A + Segment * T;
                        const FVector Out = ClothPos[I] - Closest;
                        const float Distance = Out.Size();
                        if (Distance < Capsule.R && Distance > UE_SMALL_NUMBER)
                            ClothPos[I] = Closest + Out / Distance * Capsule.R;
                    }
                }
            }
        }
        // World contact, once per frame: the floor holds and drags the hem, walls stop it.
        for (int32 I = 0; I < Count; ++I)
        {
            if (I % 8 <= 1) continue;
            FHitResult Hit;
            const FVector Before = ClothPrev[I];
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, Before + FVector(0, 0, 6.f), ClothPos[I],
                ECC_Visibility, Query) && Hit.bBlockingHit)
            {
                ClothPos[I] = Hit.ImpactPoint + Hit.ImpactNormal * 4.f;
                ClothPrev[I] = ClothPos[I];
            }
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, ClothPos[I] + FVector(0, 0, 40.f),
                ClothPos[I] - FVector(0, 0, 8.f), ECC_Visibility, Query) && Hit.ImpactNormal.Z > .5f)
            {
                const float Floor = Hit.ImpactPoint.Z + 3.f;
                if (ClothPos[I].Z < Floor)
                {
                    ClothPos[I].Z = Floor;
                    // Weight: the cloth on the ground resists sliding.
                    const FVector Slide = ClothPos[I] - ClothPrev[I];
                    ClothPrev[I] = ClothPos[I] - FVector(Slide.X, Slide.Y, 0.f) * .55f;
                }
            }
        }
        for (int32 I = 0; I < Count; ++I) Points[I] = ClothPos[I];
        if (PreviousClothOffsets.Num() != Points.Num()) PreviousClothOffsets.Init(FVector::ZeroVector, Points.Num());
        for (int32 Strip = 0; Strip < 12; ++Strip) for (int32 Segment = 0; Segment < 7; ++Segment)
        {
            const int32 I = Strip * 8 + Segment, BoneIndex = Strip * 7 + Segment;
            const FVector Offset = Points[I] - Original[I];
            Cloth[BoneIndex].Offset = World.InverseTransformVector(Offset);
            const FVector Before = (Original[I + 1] - Original[I]).GetSafeNormal();
            const FVector After = (Points[I + 1] - Points[I]).GetSafeNormal();
            const FQuat Rotation = FQuat::FindBetweenNormals(Before, After);
            Cloth[BoneIndex].Rotation = (World.GetRotation().Inverse() * Rotation * World.GetRotation()).GetNormalized();
            if (Offset.SizeSquared() > 1.f) ++Motion->CollisionAdjustedHemBones;
            Motion->MaxHemContactOffset = FMath::Max(Motion->MaxHemContactOffset, float(Offset.Size()));
        }
        // The small inner underside is separately weighted in Cloth03.
        const auto Under = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Cloth[84].Bone));
        if (Under != INDEX_NONE)
        {
            const FVector Before = World.TransformPosition(CS[Under.GetInt()].GetLocation());
            FVector After = Before;
            ProjectContact(After, true);
            Cloth[84].Offset = World.InverseTransformVector(After - Before);
        }
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        EvaluateBase(Output);
        if (!Clip) return true;
        // Store the moving blended base pose, before any procedural correction.
        LastPose.SetNum(Output.Pose.GetNumBones());
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex()) LastPose[Index.GetInt()] = Output.Pose[Index];
        LastCurve.CopyFrom(Output.Curve);
        if (!bContactLayer) { Output.Pose.NormalizeRotations(); return true; }
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        auto Compact = [&](int32 MeshIndex) {
            return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
                : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
        };
        for (const auto& Contact : Contacts) for (int32 K = 0; K < Contact.Digits.Num(); ++K)
        {
            const auto Digit = Compact(Contact.Digits[K]);
            if (Digit == INDEX_NONE) continue;
            // A planted hand opens and lays its fingers along the ground, pads down. A free hand
            // keeps the curl of the held pose.
            const float Curl = FMath::Lerp(1.f, .22f, FMath::Clamp(Contact.Alpha, 0.f, 1.f));
            Output.Pose[Digit].SetRotation(FQuat::Slerp(Contact.DigitRest[K], Output.Pose[Digit].GetRotation(), Curl).GetNormalized());
        }
        TArray<FTransform> CS;
        auto Rebuild = [&]() {
            CS.SetNum(Output.Pose.GetNumBones());
            for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
            {
                const auto Parent = Bones.GetParentBoneIndex(Index);
                CS[Index.GetInt()] = Parent != INDEX_NONE ? Output.Pose[Index] * CS[Parent.GetInt()] : Output.Pose[Index];
            }
        };
        Rebuild();
        const auto Body = Compact(BodyIndex);
        if (Body != INDEX_NONE)
        {
            const FVector Pivot = CS[Body.GetInt()].GetLocation();
            // Component-space offsets avoid the FBX unit-conversion parent's 100x scale.
            auto FollowBody = [&](FCompactPoseBoneIndex Bone, float Weight) {
                if (Bone == INDEX_NONE) return;
                FTransform Adjusted = CS[Bone.GetInt()];
                const FQuat Delta = FQuat::Slerp(FQuat::Identity, BodyRotation, Weight).GetNormalized();
                Adjusted.SetLocation(Pivot + Delta.RotateVector(Adjusted.GetLocation() - Pivot) + BodyTranslation * Weight);
                Adjusted.SetRotation((Delta * Adjusted.GetRotation()).GetNormalized());
                const auto Parent = Bones.GetParentBoneIndex(Bone);
                Output.Pose[Bone] = Parent != INDEX_NONE ? Adjusted.GetRelativeTransform(CS[Parent.GetInt()]) : Adjusted;
            };
            // The body bone carries the translation and its share of the bend. The torso then
            // flexes bone by bone, each turning about its own base, so it curves like a spine.
            {
                FTransform Adjusted = CS[Body.GetInt()];
                Adjusted.SetLocation(Pivot + SpineRot[0].RotateVector(Adjusted.GetLocation() - Pivot) + BodyTranslation);
                Adjusted.SetRotation((SpineRot[0] * Adjusted.GetRotation()).GetNormalized());
                const auto Parent = Bones.GetParentBoneIndex(Body);
                Output.Pose[Body] = Parent != INDEX_NONE ? Adjusted.GetRelativeTransform(CS[Parent.GetInt()]) : Adjusted;
            }
            for (const auto& Follow : Cloth) FollowBody(Compact(Follow.Bone), Follow.Weight);
            Rebuild();
            for (int32 I = 1; I < 5; ++I)
            {
                const auto Spine = Compact(SpineIndex[I]);
                if (Spine == INDEX_NONE) continue;
                FTransform Bent = CS[Spine.GetInt()];
                Bent.SetRotation((SpineRot[I] * Bent.GetRotation()).GetNormalized());
                const auto Parent = Bones.GetParentBoneIndex(Spine);
                if (Parent != INDEX_NONE) Output.Pose[Spine] = Bent.GetRelativeTransform(CS[Parent.GetInt()]);
                Rebuild();
            }
        }
        for (const auto& Follow : Cloth)
        {
            const auto Bone = Compact(Follow.Bone);
            if (Bone == INDEX_NONE) continue;
            FTransform Adjusted = CS[Bone.GetInt()];
            Adjusted.AddToTranslation(Follow.Offset);
            Adjusted.SetRotation((Follow.Rotation * Adjusted.GetRotation()).GetNormalized());
            const auto Parent = Bones.GetParentBoneIndex(Bone);
            Output.Pose[Bone] = Parent != INDEX_NONE ? Adjusted.GetRelativeTransform(CS[Parent.GetInt()]) : Adjusted;
        }
        if (!Cloth.IsEmpty()) Rebuild();
        if (const auto Neck = Compact(NeckIndex); Neck != INDEX_NONE)
        {
            FTransform N = CS[Neck.GetInt()];
            N.SetRotation((NeckRotation * N.GetRotation()).GetNormalized());
            const auto Parent = Bones.GetParentBoneIndex(Neck);
            if (Parent != INDEX_NONE) Output.Pose[Neck] = N.GetRelativeTransform(CS[Parent.GetInt()]);
            Rebuild();
        }
        for (const auto& Contact : Contacts)
        {
            const auto Upper = Compact(Contact.Upper), Lower = Compact(Contact.Lower), Hand = Compact(Contact.Hand);
            if (Upper != INDEX_NONE && Lower != INDEX_NONE && Contact.Alpha < .98f)
            {
                // A free arm is alive: it drifts and flexes a little, out of phase with the others.
                const float Phase = IdleClock * (.9f + .13f * (Contact.Upper % 5)) + Contact.Upper * .7f;
                const float Free = 1.f - Contact.Alpha;
                Output.Pose[Upper].SetRotation((Output.Pose[Upper].GetRotation()
                    * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(FMath::Sin(Phase) * 5.f * Free))
                    * FQuat(FVector(0, 0, 1), FMath::DegreesToRadians(FMath::Sin(Phase * .7f + 1.f) * 4.f * Free))).GetNormalized());
                Output.Pose[Lower].SetRotation((Output.Pose[Lower].GetRotation()
                    * FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(FMath::Sin(Phase * 1.3f + .5f) * 6.f * Free))).GetNormalized());
                Rebuild();
            }
            if (Contact.Alpha < .001f || Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE) continue;
            FTransform A = CS[Upper.GetInt()], B = CS[Lower.GetInt()], C = CS[Hand.GetInt()];
            if (bNarrowPortal && Squeeze > .1f)
            {
                const float Side = FVector::DotProduct(A.GetLocation() - PortalCenter, PortalRight);
                const float Half = FMath::Max(8.f, PortalHalfWidth - 15.f);
                // Protract each shoulder inward by at most 25 world centimetres,
                // reserving clearance for the sleeve around the joint itself.
                // The two limb segments retain their authored lengths.
                const float Shift = FMath::Clamp(FMath::Clamp(Side, -Half, Half) - Side, -12.5f, 12.5f)
                    * FMath::Clamp(Squeeze * 1.3f, 0.f, 1.f);
                const FVector Offset = PortalRight * Shift;
                A.AddToTranslation(Offset); B.AddToTranslation(Offset); C.AddToTranslation(Offset);
            }
            const FVector Effector = FMath::Lerp(C.GetLocation(), Contact.Target, Contact.Alpha);
            FVector Pole = Contact.Pole;
            const FVector Axis = (Effector - A.GetLocation()).GetSafeNormal();
            if (FVector::VectorPlaneProject(Pole - A.GetLocation(), Axis).SizeSquared() < 4.f)
                Pole = B.GetLocation() + (B.GetLocation() - (A.GetLocation() + C.GetLocation()) * .5f) * 2.f;
            const FQuat Wrist = FQuat::Slerp(C.GetRotation(), Contact.Rotation, Contact.Alpha).GetNormalized();
            AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Effector, false, 1.f, 1.f);
            C.SetRotation(Wrist);
            const auto Parent = Bones.GetParentBoneIndex(Upper);
            Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
            Output.Pose[Lower] = B.GetRelativeTransform(A); Output.Pose[Hand] = C.GetRelativeTransform(B);
            Rebuild();
        }
        const auto Head = Compact(HeadIndex);
        if (Head != INDEX_NONE)
        {
            FTransform H = CS[Head.GetInt()];
            FQuat Torso = FQuat::Identity;
            for (int32 I = 4; I >= 0; --I) Torso = Torso * SpineRot[I];
            const FQuat Aim = FQuat::FindBetweenNormals(Torso.RotateVector(FVector(0, 1, 0)), Look);
            H.SetRotation((FQuat::Slerp(FQuat::Identity, Aim, .58f) * H.GetRotation()).GetNormalized());
            const auto Parent = Bones.GetParentBoneIndex(Head);
            if (Parent != INDEX_NONE) Output.Pose[Head] = H.GetRelativeTransform(CS[Parent.GetInt()]);
        }
        SimulateHood(Output, CS, Rebuild);
        Output.Pose.NormalizeRotations(); return true;
    }
};
FAnimInstanceProxy* UTallowHunterAnimInstance::CreateAnimInstanceProxy() { return new FTallowHunterAnimProxy(this); }
void UTallowHunterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
