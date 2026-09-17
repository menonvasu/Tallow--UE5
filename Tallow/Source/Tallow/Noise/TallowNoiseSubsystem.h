#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Noise/TallowNoiseTypes.h"
#include "TallowNoiseSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTallowNoiseEmitted, const FTallowNoiseEvent&, NoiseEvent);

/**
 *  The noise event system. Anything that makes a sound calls EmitNoise, and the noise is
 *  passed to AI hearing and broadcast to anything else that is listening.
 */
UCLASS()
class TALLOW_API UTallowNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Subsystem initialization */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Returns the noise subsystem for the world the object is in, or nullptr */
	static UTallowNoiseSubsystem* Get(const UObject* WorldContextObject);

	/** Emits a noise using the default profile for its type */
	UFUNCTION(BlueprintCallable, Category = "Noise", meta = (AdvancedDisplay = "LoudnessScale,bAlertsAI"))
	void EmitNoise(ETallowNoiseType Type, FVector Location, AActor* Instigator, float LoudnessScale = 1.0f, bool bAlertsAI = true);

	/** Emits a noise with an explicit loudness and range */
	UFUNCTION(BlueprintCallable, Category = "Noise", meta = (AdvancedDisplay = "bAlertsAI"))
	void EmitCustomNoise(FVector Location, AActor* Instigator, float Loudness, float Range, bool bAlertsAI = true);

	/** Returns how loud a noise is at a location, from 0 to 1 */
	UFUNCTION(BlueprintPure, Category = "Noise")
	float GetLoudnessAt(const FTallowNoiseEvent& NoiseEvent, FVector ListenerLocation) const;

	/** Returns noises made within the last few seconds, oldest first */
	UFUNCTION(BlueprintPure, Category = "Noise")
	TArray<FTallowNoiseEvent> GetRecentNoises(float MaxAgeSeconds = 5.0f) const;

	/** Returns the profile for a noise type */
	UFUNCTION(BlueprintPure, Category = "Noise")
	FTallowNoiseProfile GetNoiseProfile(ETallowNoiseType Type) const;

	/** Overrides the profile for a noise type, for tuning at runtime */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void SetNoiseProfile(ETallowNoiseType Type, FTallowNoiseProfile Profile);

	/** Called every time a noise is made */
	UPROPERTY(BlueprintAssignable, Category = "Noise")
	FOnTallowNoiseEmitted OnNoiseEmitted;

protected:

	/** Logs, reports and broadcasts a finished noise event */
	void ProcessNoise(FTallowNoiseEvent& NoiseEvent);

	/** Draws the noise range when tallow.Noise.Debug is on */
	void DrawNoiseDebug(const FTallowNoiseEvent& NoiseEvent) const;

	/** Default loudness and range for every noise type */
	UPROPERTY()
	TMap<ETallowNoiseType, FTallowNoiseProfile> NoiseProfiles;

	/** Recent noises, oldest first */
	UPROPERTY()
	TArray<FTallowNoiseEvent> NoiseHistory;

	/** How long noises are kept in the history */
	static constexpr double HistoryLifetime = 10.0;
};
