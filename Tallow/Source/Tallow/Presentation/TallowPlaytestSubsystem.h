#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TallowPlaytestSubsystem.generated.h"

class APointLight;
class ULocalLightComponent;

/** Optional lights and a read-only Mim note. Does not change movement or capture rules. */
UCLASS()
class TALLOW_API UTallowPlaytestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") static bool IsBabyModeEnabled();
	UFUNCTION(BlueprintCallable, Category="Tallow|Playtest") static void SetBabyModeEnabled(bool bEnabled);
	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") static FString DescribeLocation(FVector Position);
	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") FString GetMimActivity() const { return Activity; }
	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") FString GetMimWhereabouts() const { return Whereabouts; }
	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") int32 GetPlaytestLightCount() const { return FillLights.Num(); }
	UFUNCTION(BlueprintPure, Category="Tallow|Playtest") bool IsLightingApplied() const { return bApplied; }
private:
	struct FLightBefore
	{
		TWeakObjectPtr<ULocalLightComponent> Light;
		float Intensity = 0.f, Radius = 0.f, Indirect = 0.f;
		uint8 Units = 0, Mobility = 0;
		bool bVisible = false, bHidden = false, bCastShadows = false;
	};
	TArray<FLightBefore> Before;
	TArray<TWeakObjectPtr<APointLight>> FillLights;
	FString Activity = TEXT("Mim: not present"), Whereabouts;
	float RefreshAge = 1.f;
	bool bApplied = false;
	void Apply();
	void Restore();
	void UpdateNote();
};
