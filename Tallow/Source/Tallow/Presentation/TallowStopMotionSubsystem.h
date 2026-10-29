#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TallowStopMotionSubsystem.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

/**
 * Stop-motion "on twos": everything that animates updates at 12 Hz, like a puppet moved between
 * frames, while the player's camera stays smooth.
 *
 * - Skeletal meshes and the candle flame/light components tick at 1/Rate, so poses and flicker step.
 * - Meshes of characters other than the player also hold their on-screen transform between steps.
 *   Only the rendered transform is held: the game's own placement is restored at the start of
 *   every frame, so movement, AI, traces and collision never see the held value.
 * - Mim is exempt: she animates and moves smoothly every frame (off twos for Mim only).
 * tallow.OnTwos 0/1 toggles it; tallow.OnTwos.Rate sets the step rate.
 */
UCLASS()
class TALLOW_API UTallowStopMotionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FHeld
	{
		TWeakObjectPtr<USceneComponent> Component;
		FTransform Relative;   // the game's own relative transform, restored at frame start
		FTransform World;      // what is shown until the next step
		bool bOverridden = false;
	};

	void OnWorldTickStart(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	void Scan();
	void Release();
	bool ShouldHoldTransform(const USkeletalMeshComponent* Mesh) const;

	TArray<FHeld> Held;
	TSet<TWeakObjectPtr<UActorComponent>> Stepped;
	FDelegateHandle TickStartHandle;
	float NextScan = 0.f;
	float AppliedInterval = -1.f;
	bool bActive = false;
};
