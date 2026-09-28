#include "Animation/TallowCreatureAnimationComponent.h"
#include "AI/TallowCreature.h"
#include "AI/Traversal/TallowSurfaceTraversalComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

UTallowCreatureAnimationComponent::UTallowCreatureAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Defaults so every Keeper gets the authored mesh and clips, not only the one placed in the M3 level
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Keeper/Animated/SK_Keeper.SK_Keeper"));
	KeeperMesh = Mesh.Object;
	auto Clip = [](const TCHAR* Name) -> UAnimSequence*
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*FString::Printf(TEXT("/Game/Characters/Keeper/Animated/AN_Keeper_%s.AN_Keeper_%s"), Name, Name));
		return Finder.Object;
	};
	IdleAnimation = Clip(TEXT("Idle"));
	WalkAnimation = Clip(TEXT("HostWalk"));
	ChaseAnimation = Clip(TEXT("RunChase"));
	InvestigateAnimation = Clip(TEXT("SearchUnder"));
	CaptureAnimation = Clip(TEXT("JumpscareDeath"));
	WallAnimation = Clip(TEXT("WallCrawl"));
	CeilingAnimation = Clip(TEXT("CeilingCrawl"));
	SqueezeAnimation = Clip(TEXT("Squeeze"));
	RevealAnimation = Clip(TEXT("Reveal"));
	SneakAnimation = Clip(TEXT("Sneak"));
	SearchUnderAnimation = Clip(TEXT("SearchUnder"));
	SearchOverAnimation = Clip(TEXT("SearchOver"));
}

void UTallowCreatureAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	Creature = Cast<ATallowCreature>(GetOwner());
	bActive = bEnableAuthoredPlayback && Creature && KeeperMesh && IdleAnimation;
	if (!bActive)
	{
		SetComponentTickEnabled(false);
		return;
	}
	USkeletalMeshComponent* Mesh = Creature->GetMesh();
	Mesh->SetSkeletalMesh(KeeperMesh);
	Mesh->SetRelativeLocationAndRotation(KeeperMeshOffset, FRotator(0.f, KeeperMeshYaw, 0.f));
	BaseMeshScale = FVector(KeeperMeshScale);
	Mesh->SetRelativeScale3D(BaseMeshScale);
	Mesh->SetVisibility(true, true);
	Mesh->SetHiddenInGame(false, true);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// Tick even without a rendered viewport so AI and automated playtests advance authored poses.
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GameMode = Cast<ATallowGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		GameMode->OnCaught.AddUniqueDynamic(this, &UTallowCreatureAnimationComponent::PlayCaptureAnimation);
	}
	PlayClip(IdleAnimation, true, 1.f);
}

void UTallowCreatureAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GameMode)
	{
		GameMode->OnCaught.RemoveDynamic(this, &UTallowCreatureAnimationComponent::PlayCaptureAnimation);
	}
	Super::EndPlay(EndPlayReason);
}

void UTallowCreatureAnimationComponent::PlayClip(UAnimSequence* Animation, bool bLoop, float PlayRate)
{
	if (!bActive || !Animation || !Creature) return;
	USkeletalMeshComponent* Mesh = Creature->GetMesh();
	if (Animation->GetSkeleton() != Mesh->GetSkeletalMeshAsset()->GetSkeleton()) return;
	if (ActiveClip != Animation)
	{
		Mesh->PlayAnimation(Animation, bLoop);
		ActiveClip = Animation;
	}
	if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
	{
		Instance->SetLooping(bLoop);
		Instance->SetPlayRate(FMath::Clamp(PlayRate, 0.1f, 3.f));
	}
}

void UTallowCreatureAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive || !Creature || bCaptured) return;
	const auto* SurfaceTraversal = Creature->FindComponentByClass<UTallowSurfaceTraversalComponent>();
	const float SqueezeAlpha = SurfaceTraversal ? FMath::Clamp(SurfaceTraversal->GetSqueezeAlpha(), 0.f, 1.f) : 0.f;
	Creature->GetMesh()->SetRelativeScale3D(BaseMeshScale * FVector(FMath::Lerp(1.f, SqueezeWidthScale, SqueezeAlpha), 1.f, FMath::Lerp(1.f, SqueezeHeightScale, SqueezeAlpha)));
	const bool bNowChasing = Creature->CurrentState == ETallowCreatureState::Chasing;
	const bool bNowTraversing = SurfaceTraversal && SurfaceTraversal->IsTraversing() && !SurfaceTraversal->IsSqueezing();
	if (UCharacterMovementComponent* Movement = Creature->GetCharacterMovement())
	{
		const bool bInvestigating = Creature->CurrentState == ETallowCreatureState::Investigating;
		Movement->MaxWalkSpeed = bNowChasing ? ChaseSpeed : (bInvestigating ? InvestigateSpeed : PatrolSpeed);
		Movement->RotationRate.Yaw = bNowChasing ? ChaseTurnRate : PatrolTurnRate;
	}
	if (SurfaceTraversal && SurfaceTraversal->IsTraversing()) RevealTimeRemaining = 0.f;
	if (RevealAnimation && bNowChasing && !bWasChasing && !(SurfaceTraversal && SurfaceTraversal->IsTraversing()))
	{
		RevealTimeRemaining = RevealAnimation->GetPlayLength();
		ActiveClip = nullptr;
	}
	bWasChasing = bNowChasing;
	bWasTraversing = bNowTraversing;
	if (TraversalClip)
	{
		PlayClip(TraversalClip, bTraversalLoop, TraversalRate);
		return;
	}
	if (RevealTimeRemaining > 0.f)
	{
		RevealTimeRemaining = FMath::Max(0.f, RevealTimeRemaining - DeltaTime);
		PlayClip(RevealAnimation, false, 1.f);
		return;
	}
	if (const auto* Traversal = Creature->FindComponentByClass<UTallowSurfaceTraversalComponent>())
	{
		if (Traversal->IsSqueezing() && SqueezeAnimation)
		{
			PlayClip(SqueezeAnimation, true, 1.f);
			return;
		}
		if (Traversal->IsTraversing())
		{
			const bool bCeiling = Traversal->GetSurfaceNormal().Z < -.5f;
			UAnimSequence* SurfaceClip = bCeiling ? CeilingAnimation.Get() : WallAnimation.Get();
			if (SurfaceClip)
			{
				PlayClip(SurfaceClip, true, FMath::Clamp(Traversal->GetTraversalSpeed() / FMath::Max(bCeiling ? CeilingReferenceSpeed : WallReferenceSpeed, 1.f), .1f, 3.f));
				return;
			}
		}
	}
	const float Speed = Creature->GetVelocity().Size();
	if (Speed > 5.f)
	{
		bWasStationarySearch = false;
		const bool bChase = Creature->CurrentState == ETallowCreatureState::Chasing;
		UAnimSequence* Clip = bChase && ChaseAnimation ? ChaseAnimation.Get() : WalkAnimation.Get();
		// The authored sneak is a very slow crawl: use it only near its supported stride speed.
		if (!bChase && Creature->CurrentState == ETallowCreatureState::Investigating && SneakAnimation && Speed <= SneakReferenceSpeed * 3.f) Clip = SneakAnimation;
		const float ReferenceSpeed = bChase && ChaseAnimation ? ChaseReferenceSpeed : WalkReferenceSpeed;
		const float ClipReferenceSpeed = Clip == SneakAnimation && SneakAnimation ? SneakReferenceSpeed : ReferenceSpeed;
		PlayClip(Clip ? Clip : IdleAnimation.Get(), true, Clip ? FMath::Clamp(Speed / FMath::Max(ClipReferenceSpeed, 1.f), .1f, 3.f) : 1.f);
	}
	else
	{
		const bool bInspect = Creature->CurrentState == ETallowCreatureState::Investigating ||
			Creature->CurrentState == ETallowCreatureState::SuspiciousIdle ||
			Creature->CurrentState == ETallowCreatureState::SearchingCorner;
		if (bInspect && !bWasStationarySearch)
		{
			StationarySearchClip = bNextSearchOver && SearchOverAnimation ? SearchOverAnimation.Get() : SearchUnderAnimation.Get();
			if (!StationarySearchClip) StationarySearchClip = InvestigateAnimation;
			bNextSearchOver = !bNextSearchOver;
		}
		bWasStationarySearch = bInspect;
		PlayClip(bInspect && StationarySearchClip ? StationarySearchClip.Get() : IdleAnimation.Get(), true, 1.f);
	}
}

void UTallowCreatureAnimationComponent::PlayTraversalAnimation(UAnimSequence* Animation, bool bLoop, float PlayRate)
{
	TraversalClip = Animation;
	bTraversalLoop = bLoop;
	TraversalRate = PlayRate;
	if (!bCaptured) PlayClip(Animation, bLoop, PlayRate);
}

void UTallowCreatureAnimationComponent::ClearTraversalAnimation()
{
	TraversalClip = nullptr;
}

void UTallowCreatureAnimationComponent::PlayCaptureAnimation()
{
	if (!bActive) return;
	bCaptured = true;
	TraversalClip = nullptr;
	ActiveClip = nullptr; // A second capture must start from the first frame.
	PlayClip(CaptureAnimation ? CaptureAnimation.Get() : IdleAnimation.Get(), false, 1.f);
}

void UTallowCreatureAnimationComponent::ResetPlayback()
{
	if (Creature && bActive) Creature->GetMesh()->SetRelativeScale3D(BaseMeshScale);
	bCaptured = false;
	bWasChasing = false;
	bWasTraversing = false;
	RevealTimeRemaining = 0.f;
	bWasStationarySearch = false;
	bNextSearchOver = false;
	StationarySearchClip = nullptr;
	TraversalClip = nullptr;
	ActiveClip = nullptr;
	PlayClip(IdleAnimation, true, 1.f);
}

float UTallowCreatureAnimationComponent::GetCaptureDuration() const
{
	return bActive && CaptureAnimation ? CaptureAnimation->GetPlayLength() : 0.f;
}
