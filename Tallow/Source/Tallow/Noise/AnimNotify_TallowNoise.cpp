#include "Noise/AnimNotify_TallowNoise.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_TallowNoise::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Returns nullptr in the animation editor preview, so notifies stay silent there
	UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(MeshComp);
	if (!NoiseSubsystem)
	{
		return;
	}

	const FVector Location = SocketName.IsNone() ? MeshComp->GetComponentLocation() : MeshComp->GetSocketLocation(SocketName);

	NoiseSubsystem->EmitNoise(NoiseType, Location, MeshComp->GetOwner(), LoudnessScale, bAlertsAI);
}

FString UAnimNotify_TallowNoise::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Noise: %s"), *StaticEnum<ETallowNoiseType>()->GetNameStringByValue(static_cast<int64>(NoiseType)));
}
