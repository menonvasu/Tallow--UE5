#include "Player/Stealth/TallowStealthAnimInstance.h"
#include "Player/Stealth/TallowStealthCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

struct FTallowStealthAnimProxy : FAnimInstanceProxy
{
    explicit FTallowStealthAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* Clip = nullptr;
    float Time = 0.f, BlendAge = 1.f, HandWeight = 0.f;
    FVector HandTarget = FVector::ZeroVector;
    FVector HandNormal = FVector::UpVector, HandForward = FVector::ForwardVector;
    bool bPalmContact = false;
    int32 UpperIndex = INDEX_NONE, LowerIndex = INDEX_NONE, HandIndex = INDEX_NONE;
    int32 IndexIndex = INDEX_NONE, MiddleIndex = INDEX_NONE, PinkyIndex = INDEX_NONE;
    TArray<FTransform> LastPose, SourcePose;
    FBlendedHeapCurve LastCurve, SourceCurve;

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        auto* State = CastChecked<UTallowStealthAnimInstance>(Instance);
        auto* Mesh = Instance->GetSkelMeshComponent();
        auto* Player = Mesh ? Cast<ATallowStealthCharacter>(Mesh->GetOwner()) : nullptr;
        UAnimSequence* Next = State->GetActiveClip();
        float NextTime = State->GetPlaybackPosition();
        HandWeight = 0.f;
        bPalmContact = false;
        if (Player)
        {
            const auto Mode = Player->GetStealthTraversalMode();
            if (Mode == ETallowPlayerTraversalMode::Mantling && Player->ReachClip)
            {
                Next = Player->ReachClip;
                const float Progress = Player->GetStealthTraversalProgress();
                // Use the authored open palm for the plant, a slight finger curl
                // while bearing weight, then the authored release. Sampling the
                // whole grab clip uniformly closed the fist around empty air.
                const float Phase = Progress < .16f
                    ? FMath::Lerp(.12f, .365f, FMath::SmoothStep(0.f, .16f, Progress))
                    : Progress < .58f ? FMath::Lerp(.365f, .375f, FMath::SmoothStep(.16f, .58f, Progress))
                    : Progress < .70f ? FMath::Lerp(.375f, .82f, FMath::SmoothStep(.58f, .70f, Progress))
                    : FMath::Lerp(.82f, 1.f, FMath::SmoothStep(.70f, 1.f, Progress));
                NextTime = Next->GetPlayLength() * Phase;
            }
            else if ((Player->IsHoldingBreath() || Mode == ETallowPlayerTraversalMode::Squeezing)
                && Player->BraceClip && Player->GetCandleComponent()->IsAcquired())
            {
                Next = Player->BraceClip;
                // Hold the authored protection pose; the physical tuck, heartbeat
                // tremor and recovery breathing are applied by the player.
                NextTime = FMath::Min(.6f, Next->GetPlayLength());
            }
            HandWeight = Player->GetTraversalHandWeight();
            HandTarget = Mesh->GetComponentTransform().InverseTransformPosition(Player->GetTraversalHandTarget());
            if (Mode == ETallowPlayerTraversalMode::Mantling)
            {
                bPalmContact = true;
                FMinimalViewInfo View;
                Player->GetFirstPersonCameraComponent()->GetCameraView(DeltaSeconds, View);
                const FVector PalmTarget = Player->GetTraversalHandTarget();
                const FVector SurfaceNormal = Player->GetTraversalHandSurfaceNormal();
                const FVector SurfaceForward = FVector::VectorPlaneProject(
                    Player->GetActorForwardVector(), SurfaceNormal).GetSafeNormal();
                const FVector SurfaceLeft = -FVector::CrossProduct(SurfaceNormal, SurfaceForward);
                // First-person scale/FOV morph the mesh on the GPU. Solve the
                // inverse transform so the displayed palm, not just its raw
                // skeleton, touches the physical surface.
                const FVector Unprojected = View.TransformFirstPersonToWorld(PalmTarget, false);
                const FVector Finger = View.TransformFirstPersonToWorld(PalmTarget + SurfaceForward, false) - Unprojected;
                const FVector Across = View.TransformFirstPersonToWorld(PalmTarget + SurfaceLeft, false) - Unprojected;
                HandTarget = Mesh->GetComponentTransform().InverseTransformPosition(Unprojected);
                HandForward = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Finger).GetSafeNormal();
                HandNormal = Mesh->GetComponentTransform().InverseTransformVectorNoScale(
                    -FVector::CrossProduct(Across, Finger)).GetSafeNormal();
            }
            UpperIndex = Mesh->GetBoneIndex(TEXT("upperarm_l"));
            LowerIndex = Mesh->GetBoneIndex(TEXT("lowerarm_l"));
            HandIndex = Mesh->GetBoneIndex(TEXT("hand_l"));
            IndexIndex = Mesh->GetBoneIndex(TEXT("index_01_l"));
            MiddleIndex = Mesh->GetBoneIndex(TEXT("middle_01_l"));
            PinkyIndex = Mesh->GetBoneIndex(TEXT("pinky_01_l"));
        }
        if (Next != Clip)
        {
            SourcePose = LastPose;
            SourceCurve.CopyFrom(LastCurve);
            BlendAge = 0.f;
        }
        BlendAge += DeltaSeconds;
        Clip = Next;
        Time = NextTime;
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        if (!Clip) return true;
        FAnimationPoseData Target(Output);
        FAnimExtractContext Extract(static_cast<double>(Time), false);
        Clip->GetAnimationPose(Target, Extract);
        const float Alpha = FMath::SmoothStep(0.f, .24f, BlendAge);
        if (Alpha < 1.f && SourcePose.Num() == Output.Pose.GetNumBones())
        {
            FPoseContext Previous(this);
            Previous.ResetToRefPose();
            for (const FCompactPoseBoneIndex Index : Previous.Pose.ForEachBoneIndex())
                Previous.Pose[Index] = SourcePose[Index.GetInt()];
            Previous.Curve.CopyFrom(SourceCurve);
            FAnimationPoseData Prior(Previous);
            FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, Prior, Alpha);
        }
        LastPose.SetNum(Output.Pose.GetNumBones());
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
            LastPose[Index.GetInt()] = Output.Pose[Index];
        LastCurve.CopyFrom(Output.Curve);

        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        auto Compact = [&](int32 Index) {
            return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
                : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
        };
        const auto Upper = Compact(UpperIndex), Lower = Compact(LowerIndex), Hand = Compact(HandIndex);
        if (HandWeight > .001f && Upper != INDEX_NONE && Lower != INDEX_NONE && Hand != INDEX_NONE)
        {
            TArray<FTransform> CS;
            CS.SetNum(Output.Pose.GetNumBones());
            for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
            {
                const auto Parent = Bones.GetParentBoneIndex(Index);
                CS[Index.GetInt()] = Parent != INDEX_NONE ? Output.Pose[Index] * CS[Parent.GetInt()] : Output.Pose[Index];
            }
            FTransform A = CS[Upper.GetInt()], B = CS[Lower.GetInt()], C = CS[Hand.GetInt()];
            FVector WristTarget = HandTarget;
            FQuat ContactRotation = C.GetRotation();
            const auto Index = Compact(IndexIndex), Middle = Compact(MiddleIndex), Pinky = Compact(PinkyIndex);
            if (bPalmContact && Index != INDEX_NONE && Middle != INDEX_NONE && Pinky != INDEX_NONE)
            {
                const FVector Finger = (CS[Middle.GetInt()].GetLocation() - C.GetLocation()).GetSafeNormal();
                const FVector Across = (CS[Pinky.GetInt()].GetLocation() - CS[Index.GetInt()].GetLocation()).GetSafeNormal();
                // In Unreal coordinates, left-hand index-to-pinky and finger
                // directions give the palm side with the negative cross product.
                const FVector PalmNormal = -FVector::CrossProduct(Across, Finger).GetSafeNormal();
                const FQuat AlignNormal = FQuat::FindBetweenNormals(PalmNormal, HandNormal);
                const FVector AlignedFinger = FVector::VectorPlaneProject(
                    AlignNormal.RotateVector(Finger), HandNormal).GetSafeNormal();
                const float Twist = FMath::Atan2(FVector::DotProduct(HandNormal,
                    FVector::CrossProduct(AlignedFinger, HandForward)),
                    FVector::DotProduct(AlignedFinger, HandForward));
                ContactRotation = (FQuat(HandNormal, Twist) * AlignNormal * C.GetRotation()).GetNormalized();
                FTransform Contact = C;
                Contact.SetRotation(ContactRotation);
                const FVector PalmOffset = C.InverseTransformPosition(CS[Middle.GetInt()].GetLocation()) * .55f;
                WristTarget -= Contact.TransformVector(PalmOffset);
            }
            // No arm scaling: an unreachable contact relaxes at the chain's natural reach.
            const float Reach = FVector::Distance(A.GetLocation(), B.GetLocation())
                + FVector::Distance(B.GetLocation(), C.GetLocation());
            const FVector Clamped = A.GetLocation() + (WristTarget - A.GetLocation()).GetClampedToMaxSize(Reach * .985f);
            const FVector Effector = FMath::Lerp(C.GetLocation(), Clamped, HandWeight);
            const FVector Pole = B.GetLocation() + (B.GetLocation() - (A.GetLocation() + C.GetLocation()) * .5f) * 2.f;
            AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Effector, false, 1.f, 1.f);
            C.SetRotation(FQuat::Slerp(C.GetRotation(), ContactRotation, HandWeight).GetNormalized());
            const auto Parent = Bones.GetParentBoneIndex(Upper);
            Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
            Output.Pose[Lower] = B.GetRelativeTransform(A);
            Output.Pose[Hand] = C.GetRelativeTransform(B);
        }
        Output.Pose.NormalizeRotations();
        return true;
    }
};

FAnimInstanceProxy* UTallowStealthAnimInstance::CreateAnimInstanceProxy() { return new FTallowStealthAnimProxy(this); }
void UTallowStealthAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
