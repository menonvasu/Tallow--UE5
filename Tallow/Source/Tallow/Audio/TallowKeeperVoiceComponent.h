#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowKeeperVoiceComponent.generated.h"

class USoundBase;
class USoundAttenuation;

/** The Keeper's voice: rare, quiet spatial breaths (closer together while hunting, ragged in a chase) and a roar/scream on capture.
 *  Placeholder sounds: Little Robot Sound Factory horror library (CC-BY 3.0), see SourceAssets/Audio/CREDITS.md. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowKeeperVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowKeeperVoiceComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	TArray<FSoftObjectPath> Vocalisations;

	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	FSoftObjectPath CaptureRoar;

	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	FSoftObjectPath CaptureScream;

	/** Sometimes played instead of a breath while it hunts for you (sniff, porcelain tick) */
	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	TArray<FSoftObjectPath> HuntingCues;

	/** The mask splitting open, under the capture roar */
	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	FSoftObjectPath MaskCrack;

	/** Distance (cm) at which the Keeper's voice fades to silence */
	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	float HearingDistance = 900.f;

	UFUNCTION()
	void PlayCapture();

private:
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Loaded;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> LoadedHunting;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	float NextVoice = 0.f;
	FTimerHandle StrikeHandle;
	FTimerHandle ScreamHandle;
	int32 LastIndex = -1;
};
