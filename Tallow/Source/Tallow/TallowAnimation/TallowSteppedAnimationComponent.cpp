#include "TallowAnimation/TallowSteppedAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UTallowSteppedAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents<USkeletalMeshComponent>(SteppedMeshes);
	}

	ApplyStepping();
}

void UTallowSteppedAnimationComponent::SetStepped(bool bNewStepped)
{
	bStepped = bNewStepped;
	ApplyStepping();
}

void UTallowSteppedAnimationComponent::SetStepRate(float NewStepRate)
{
	StepRate = FMath::Clamp(NewStepRate, 1.0f, 60.0f);
	ApplyStepping();
}

void UTallowSteppedAnimationComponent::ApplyStepping()
{
	// Ticking the mesh on an interval means the animation is evaluated with the time since its last tick,
	// so the timing stays correct and only the pose updates are held
	const float TickInterval = bStepped ? 1.0f / StepRate : 0.0f;

	for (USkeletalMeshComponent* Mesh : SteppedMeshes)
	{
		if (Mesh)
		{
			Mesh->SetComponentTickInterval(TickInterval);
		}
	}
}
