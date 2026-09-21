#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowNoiseOccluderComponent.generated.h"

/**
 *  Sets how much sound gets through an actor. Add to doors, thin walls, curtains and vents.
 *  Anything without this component uses the default wall transmission from the noise subsystem.
 */
UCLASS(ClassGroup = (Tallow), meta = (BlueprintSpawnableComponent))
class TALLOW_API UTallowNoiseOccluderComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Returns the fraction of sound that gets through, 0 blocks everything and 1 lets everything through */
	UFUNCTION(BlueprintPure, Category = "Noise")
	float GetTransmission() const;

	/** Opens or closes the occluder, optionally making a door noise */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void SetOpen(bool bNewOpen, bool bMakeNoise = false);

	UFUNCTION(BlueprintPure, Category = "Noise")
	bool IsOpen() const { return bIsOpen; }

protected:

	/** Whether the occluder starts open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	bool bIsOpen = false;

	/** Sound let through when closed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClosedTransmission = 0.25f;

	/** Sound let through when open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpenTransmission = 1.0f;
};
