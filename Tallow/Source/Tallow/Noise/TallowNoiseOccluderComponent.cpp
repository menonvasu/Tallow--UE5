#include "Noise/TallowNoiseOccluderComponent.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "GameFramework/Actor.h"

float UTallowNoiseOccluderComponent::GetTransmission() const
{
	return bIsOpen ? OpenTransmission : ClosedTransmission;
}

void UTallowNoiseOccluderComponent::SetOpen(bool bNewOpen, bool bMakeNoise)
{
	if (bIsOpen == bNewOpen)
	{
		return;
	}

	bIsOpen = bNewOpen;

	if (bMakeNoise && GetOwner())
	{
		if (UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(this))
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::DoorOpen, GetOwner()->GetActorLocation(), GetOwner());
		}
	}
}
