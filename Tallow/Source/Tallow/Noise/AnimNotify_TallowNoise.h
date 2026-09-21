#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Noise/TallowNoiseTypes.h"
#include "AnimNotify_TallowNoise.generated.h"

/**
 *  Animation notify that makes a noise event, for foot plants, the match strike, prying and so on.
 *  Place it on the frame the sound happens.
 */
UCLASS(meta = (DisplayName = "Tallow Noise"))
class TALLOW_API UAnimNotify_TallowNoise : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:

	/** Type of noise to make */
	UPROPERTY(EditAnywhere, Category = "Noise")
	ETallowNoiseType NoiseType = ETallowNoiseType::Footstep;

	/** Multiplies the loudness and range of the noise type */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0"))
	float LoudnessScale = 1.0f;

	/** Socket or bone the noise comes from, leave empty to use the mesh origin */
	UPROPERTY(EditAnywhere, Category = "Noise")
	FName SocketName;

	/** Turn off on creature animations so it doesn't react to its own noises */
	UPROPERTY(EditAnywhere, Category = "Noise")
	bool bAlertsAI = true;
};
