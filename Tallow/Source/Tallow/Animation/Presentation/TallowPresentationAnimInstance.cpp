#include "Animation/Presentation/TallowPresentationAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

struct FTallowPresentationAnimProxy : FAnimInstanceProxy
{
	explicit FTallowPresentationAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
	UAnimSequence* Clip = nullptr;
	float Time = 0.f;
	float Alpha = 1.f;
	uint32 SeenGeneration = 0;
	TArray<FTransform> LastPose, SourcePose;
	FBlendedHeapCurve LastCurve, SourceCurve;

	virtual void Initialize(UAnimInstance* Instance) override
	{
		FAnimInstanceProxy::Initialize(Instance);
		LastPose.Reset(); SourcePose.Reset(); SeenGeneration = 0;
	}
	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto* State = CastChecked<UTallowPresentationAnimInstance>(Instance);
		if (SeenGeneration != State->Generation)
		{
			SourcePose = LastPose;
			SourceCurve.CopyFrom(LastCurve);
			SeenGeneration = State->Generation;
		}
		Clip = State->ActiveClip;
		Time = State->PlaybackPosition;
		Alpha = State->BlendDuration > 0.f ? FMath::Clamp(State->BlendElapsed / State->BlendDuration, 0.f, 1.f) : 1.f;
		Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	}
	virtual bool Evaluate(FPoseContext& Output) override
	{
		Output.ResetToRefPose();
		if (Clip)
		{
			FAnimationPoseData Target(Output);
			FAnimExtractContext Context(static_cast<double>(Time), false);
			Clip->GetAnimationPose(Target, Context);
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
		}
		Output.Pose.NormalizeRotations();
		LastPose.SetNum(Output.Pose.GetNumBones());
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex()) LastPose[Index.GetInt()] = Output.Pose[Index];
		LastCurve.CopyFrom(Output.Curve);
		return true;
	}
};

UTallowPresentationAnimInstance::UTallowPresentationAnimInstance()
{
	RootMotionMode = ERootMotionMode::NoRootMotionExtraction;
}

bool UTallowPresentationAnimInstance::PlayClip(UAnimSequence* Clip, bool bLoop, float PlayRate, float BlendSeconds, bool bRestart)
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Clip || !Mesh || !Mesh->GetSkeletalMeshAsset()
		|| Clip->GetSkeleton() != Mesh->GetSkeletalMeshAsset()->GetSkeleton()
		|| !FMath::IsFinite(PlayRate) || !FMath::IsFinite(BlendSeconds)) return false;
	if (ActiveClip != Clip || bRestart)
	{
		BlendDuration = ActiveClip ? FMath::Clamp(BlendSeconds, 0.f, 2.f) : 0.f;
		BlendElapsed = 0.f;
		PlaybackPosition = 0.f;
		ActiveClip = Clip;
		++Generation;
	}
	Rate = FMath::Clamp(PlayRate, 0.f, 4.f);
	bLooping = bLoop;
	bPlaying = true;
	return true;
}

void UTallowPresentationAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!ActiveClip || !bPlaying) return;
	const float Delta = FMath::Max(0.f, DeltaSeconds);
	BlendElapsed += Delta;
	const float Length = ActiveClip->GetPlayLength();
	PlaybackPosition += Delta * Rate;
	PlaybackPosition = bLooping && Length > UE_SMALL_NUMBER
		? FMath::Fmod(PlaybackPosition, Length) : FMath::Clamp(PlaybackPosition, 0.f, Length);
}

void UTallowPresentationAnimInstance::StopPlayback() { bPlaying = false; }
void UTallowPresentationAnimInstance::SetPlaybackPosition(float Seconds)
{
	if (ActiveClip && FMath::IsFinite(Seconds)) PlaybackPosition = FMath::Clamp(Seconds, 0.f, ActiveClip->GetPlayLength());
}
FAnimInstanceProxy* UTallowPresentationAnimInstance::CreateAnimInstanceProxy() { return new FTallowPresentationAnimProxy(this); }
void UTallowPresentationAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete InProxy; }
