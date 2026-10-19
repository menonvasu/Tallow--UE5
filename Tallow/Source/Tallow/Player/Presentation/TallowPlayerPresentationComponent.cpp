#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Presentation/TallowPresentationAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

UTallowPlayerPresentationComponent::UTallowPlayerPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
}

bool UTallowPlayerPresentationComponent::EnablePresentation(UCameraComponent* Camera, USkeletalMesh* PartialMesh,
	UPrimitiveComponent* HeldCandle, UPrimitiveComponent* VisibleFlame)
{
	AActor* Owner = GetOwner();
	if (bPresentationEnabled || !IsValid(Owner) || !GetWorld() || !IsValid(Camera) || !IsValid(PartialMesh)
		|| !PartialMesh->GetSkeleton() || !IsValid(HeldCandle) || !IsValid(VisibleFlame) || HeldCandle == VisibleFlame
		|| Camera->GetOwner() != Owner || HeldCandle->GetOwner() != Owner || VisibleFlame->GetOwner() != Owner
		|| !Camera->IsRegistered() || !HeldCandle->IsRegistered() || !VisibleFlame->IsRegistered()
		|| !FMath::IsFinite(PresentationFieldOfView) || PresentationFieldOfView < 5.f || PresentationFieldOfView > 170.f
		|| !FMath::IsFinite(PresentationScale) || PresentationScale < 0.001f || PresentationScale > 1.f
		|| !FMath::IsFinite(HandsBoundsScale) || HandsBoundsScale < 1.f || HandsBoundsScale > 10.f
		|| !HandsRelativeTransform.IsValid()) return false;

	// Overlapping leases on the same owner would make restoration order ambiguous.
	TArray<UTallowPlayerPresentationComponent*> Presentations;
	Owner->GetComponents(Presentations);
	for (const UTallowPlayerPresentationComponent* Other : Presentations)
		if (Other != this && Other->IsPresentationEnabled()) return false;

	Hands = NewObject<USkeletalMeshComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(Hands);
	Hands->SetupAttachment(Camera);
	Hands->SetRelativeTransform(HandsRelativeTransform);
	Hands->SetSkeletalMesh(PartialMesh);
	Hands->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Hands->bSuppressNotifyEventDispatch = true;
	Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hands->SetGenerateOverlapEvents(false);
	Hands->SetOnlyOwnerSee(true);
	Hands->SetCastShadow(false);
	// UE import bounds are static and stop at Z=80.96cm, while Cup wrists reach Z=98.38cm.
	// The tested 3x envelope encloses animated hands/fingers and prevents premature frustum culling.
	Hands->SetBoundsScale(HandsBoundsScale);
	Hands->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	Hands->RegisterComponent();
	Hands->SetComponentTickEnabled(false); // Pose time is supplied explicitly, never inferred from gameplay.
	if (!Hands->IsRegistered()) { DisablePresentation(); return false; }

	ProjectionState.Apply(Camera, HeldCandle, VisibleFlame, PresentationFieldOfView, PresentationScale);
	PresentationCamera = Camera;
	PresentationCandle = HeldCandle;
	bPresentationEnabled = true;
	Super::Activate(false);
	return true;
}

void UTallowPlayerPresentationComponent::DisablePresentation()
{
	RestoreHeldCandleAttachment();
	PresentationCamera.Reset();
	PresentationCandle.Reset();
	bPresentationEnabled = false;
	ProjectionState.Restore();
	if (IsValid(Hands))
	{
		if (AActor* Owner = Hands->GetOwner()) Owner->RemoveInstanceComponent(Hands);
		Hands->DestroyComponent();
	}
	Hands = nullptr;
	Super::Deactivate();
}

bool UTallowPlayerPresentationComponent::SetPresentationPose(UAnimSequence* Clip, float TimeSeconds)
{
	if (!bPresentationEnabled || !IsValid(Hands) || !IsValid(Clip) || !FMath::IsFinite(TimeSeconds)
		|| !Hands->GetSkeletalMeshAsset() || Clip->GetSkeleton() != Hands->GetSkeletalMeshAsset()->GetSkeleton()) return false;
	Hands->SetComponentTickEnabled(false);
	Hands->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (!Hands->GetSingleNodeInstance()) return false;
	Hands->SetAnimation(Clip);
	Hands->SetPosition(FMath::Clamp(TimeSeconds, 0.f, Clip->GetPlayLength()), false);
	// Update the single-node proxy before evaluating the explicitly sampled pose.
	Hands->TickAnimation(0.f, false);
	Hands->RefreshBoneTransforms();
	Hands->UpdateBounds();
	Hands->MarkRenderTransformDirty();
	Hands->MarkRenderDynamicDataDirty();
	return true;
}

bool UTallowPlayerPresentationComponent::PlayPresentationClip(UAnimSequence* Clip, bool bLoop, float PlayRate, float BlendSeconds, bool bRestart)
{
	if (!bPresentationEnabled || !IsValid(Hands) || !IsValid(Clip) || !Hands->GetSkeletalMeshAsset()
		|| Clip->GetSkeleton() != Hands->GetSkeletalMeshAsset()->GetSkeleton()
		|| !FMath::IsFinite(PlayRate) || PlayRate < 0.f || !FMath::IsFinite(BlendSeconds) || BlendSeconds < 0.f) return false;
	if (!Cast<UTallowPresentationAnimInstance>(Hands->GetAnimInstance()))
		Hands->SetAnimInstanceClass(UTallowPresentationAnimInstance::StaticClass());
	auto* Instance = Cast<UTallowPresentationAnimInstance>(Hands->GetAnimInstance());
	if (!Instance || !Instance->PlayClip(Clip, bLoop, PlayRate, BlendSeconds, bRestart)) return false;
	Hands->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Hands->SetComponentTickEnabled(true);
	return true;
}

void UTallowPlayerPresentationComponent::StopPresentationPlayback()
{
	if (!IsValid(Hands)) return;
	if (auto* Instance = Cast<UTallowPresentationAnimInstance>(Hands->GetAnimInstance())) Instance->StopPlayback();
	Hands->SetComponentTickEnabled(false);
}

bool UTallowPlayerPresentationComponent::AttachHeldCandleToHand(FName HandBone, const FTransform& CandleRelativeToCamera)
{
	if (!bPresentationEnabled || !IsValid(Hands) || !PresentationCamera.IsValid() || !PresentationCandle.IsValid()
		|| !CandleRelativeToCamera.IsValid() || Hands->GetBoneIndex(HandBone) == INDEX_NONE) return false;
	RestoreHeldCandleAttachment();
	UPrimitiveComponent* Candle = PresentationCandle.Get();
	OriginalCandleParent = Candle->GetAttachParent();
	OriginalCandleSocket = Candle->GetAttachSocketName();
	OriginalCandleRelative = Candle->GetRelativeTransform();
	Candle->SetWorldTransform(CandleRelativeToCamera * PresentationCamera->GetComponentTransform());
	if (!Candle->AttachToComponent(Hands, FAttachmentTransformRules::KeepWorldTransform, HandBone))
	{
		Candle->SetRelativeTransform(OriginalCandleRelative);
		OriginalCandleParent.Reset();
		return false;
	}
	bCandleAttachmentOwned = true;
	return true;
}

void UTallowPlayerPresentationComponent::RestoreHeldCandleAttachment()
{
	if (bCandleAttachmentOwned && PresentationCandle.IsValid() && PresentationCandle->GetAttachParent() == Hands.Get())
	{
		UPrimitiveComponent* Candle = PresentationCandle.Get();
		if (OriginalCandleParent.IsValid())
			Candle->AttachToComponent(OriginalCandleParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, OriginalCandleSocket);
		else Candle->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		Candle->SetRelativeTransform(OriginalCandleRelative);
	}
	bCandleAttachmentOwned = false;
	OriginalCandleParent.Reset();
}

bool UTallowPlayerPresentationComponent::UpdateHandsTransform(const FTransform& Transform)
{
	if (!Transform.IsValid()) return false;
	HandsRelativeTransform = Transform;
	if (IsValid(Hands)) Hands->SetRelativeTransform(Transform);
	return true;
}

void UTallowPlayerPresentationComponent::Deactivate()
{
	DisablePresentation();
}

void UTallowPlayerPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisablePresentation();
	Super::EndPlay(EndPlayReason);
}

void UTallowPlayerPresentationComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DisablePresentation();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
