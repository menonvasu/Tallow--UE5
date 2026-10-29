#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowHouseAtmosphereComponent.generated.h"

class ATallowCharacter;
class ATallowCreature;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class UTallowSoundscapeComponent;

/** The house as a place: room tone, the clock in the hall, the fridge and the dripping tap in the kitchen, the heater in
 *  the cellar, the humming box, rain and wind at whichever windows are nearest, the street beyond the front of the house,
 *  and one-shot details (boards, pipes, a car, a bell, a door on its hinge) set out in the rooms around Nell.
 *  Each chapter has its own colour and the house gets darker and closer as they go (the sounds: Tools/FinalPass/sound/build_sound.py).
 *  Every layer ducks with the soundscape's house gain, so Mim's honest cues keep their margin; nothing here imitates her.
 *  It also gives the chapter's current objective a voice (29 Oct): tape hiss from the waiting machine, a low hum from
 *  the battery, the humming box itself once she carries the battery, faint through the house and clearer close. */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowHouseAtmosphereComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowHouseAtmosphereComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void Setup(UTallowSoundscapeComponent* InMix, ATallowCharacter* InPlayer, ATallowCreature* InKeeper);

	/** The chapter colour in use: 0 to 3, from the chapter loop, or from the act on the old single route. */
	UFUNCTION(BlueprintPure, Category="Audio|Review") int32 GetColour() const { return Colour; }
	UFUNCTION(BlueprintPure, Category="Audio|Review") int32 GetDetailCount() const { return Details; }
	/** Reviews only: an objective voice's gain now, by ETallowGuideKind from Tape (0 tape, 1 battery, 2 fuse box). */
	UFUNCTION(BlueprintPure, Category="Audio|Review") float GetObjectiveGain(int32 Index) const { return ObjectiveGains.IsValidIndex(Index) ? ObjectiveGains[Index] : 0.f; }
	/** Reviews only (level probes): set out one detail now, as the scheduler would. */
	UFUNCTION(BlueprintCallable, Category="Audio|Review") bool PlayDetailForReview() { return Player.IsValid() && Mix.IsValid() && PlayDetail(GetWorld()->GetTimeSeconds()); }

	UPROPERTY(EditAnywhere, Category="Audio") bool bAtmosphere = true;
	/** One trim for everything here, for a quick listening pass. */
	UPROPERTY(EditAnywhere, Category="Audio", meta=(ClampMin="0", ClampMax="2")) float Level = 1.f;

private:
	struct FLayer
	{
		const TCHAR* Sound = nullptr;
		float Volume = 1.f;
		float Gains[4] = {1.f, 1.f, 1.f, 1.f};   // by chapter colour
		bool bOutside = false;                     // rain, wind and street: darker by chapter, further down under a threat
		int32 Voices = 1;                          // >1: follows the nearest windows
		float Falloff = 800.f;
		TArray<FVector> Fixed;                     // empty with Voices > 0: nearest window points; empty and Voices 0: 2D
		TArray<int32> Index;                       // into Sources
		float Gain = 0.f;
		float Duck = 1.f;                          // set by the objective voices (the humming box steps back for them)
	};
	void BuildLayers();
	void UpdateLayers(float DeltaTime);
	void UpdateDetails(float Now);
	/** The current objective's voice: only the current one sounds, and it fades out once that objective is done. */
	void UpdateObjective(float DeltaTime);
	bool PlayDetail(float Now);
	int32 ComputeColour() const;
	USoundBase* Load(const FString& Folder, const FString& Name);
	USoundAttenuation* MakeAttenuation(float Falloff, bool bOcclude);
	FVector Ears() const;

	TWeakObjectPtr<UTallowSoundscapeComponent> Mix;
	TWeakObjectPtr<ATallowCharacter> Player;
	TWeakObjectPtr<ATallowCreature> Keeper;
	TArray<FLayer> Layers;
	TArray<FVector> Windows;
	TArray<FVector> WindowAt;                  // where each window voice is now (they glide to the nearest windows)
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sources;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<USoundAttenuation>> Attenuations;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> ObjectiveVoices;   // by ETallowGuideKind, from Tape
	TArray<float> ObjectiveGains;
	int32 HummingBoxLayer = INDEX_NONE;
	TArray<FString> RecentDetails;
	FRandomStream Random;
	int32 Colour = -1, Details = 0;
	float NextDetail = 0.f;
	bool bBuilt = false;
};
