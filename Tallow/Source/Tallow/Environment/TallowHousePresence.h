#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Candle/TallowCandleComponent.h"
#include "Components/AudioComponent.h"
#include "TallowHousePresence.generated.h"

class UAudioComponent;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;

/** A shared quiet period stops rooms from firing scares over one another. */
UCLASS()
class TALLOW_API UTallowHousePresenceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    bool ClaimEvent(float QuietSeconds);
    UFUNCTION(BlueprintPure, Category="House") float GetQuietUntil() const { return QuietUntil; }
    UFUNCTION(BlueprintPure, Category="House") int32 GetEventCount() const { return EventCount; }
private:
    void ApplyHouseDarkness();
    UFUNCTION() void OnCandleLightState(ECandleState State);
    TWeakObjectPtr<UPointLightComponent> HeldLight;
    TWeakObjectPtr<UTallowCandleComponent> HeldCandle;
    FTimerHandle DarknessHandle;
    float QuietUntil = 0.f;
    int32 EventCount = 0;
};

/** Additive environmental movement. Never changes player state, objectives or hunter rules. */
UCLASS()
class TALLOW_API ATallowHousePresence : public AActor
{
    GENERATED_BODY()
public:
    ATallowHousePresence();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House") TObjectPtr<UStaticMeshComponent> MovingProp;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House") TObjectPtr<UPointLightComponent> Practical;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House") TObjectPtr<UAudioComponent> EventAudio;
    UPROPERTY(EditAnywhere, Category="House") bool bCandlePractical = false;
    UPROPERTY(EditAnywhere, Category="House") float PracticalLumens = 22.f;
    UPROPERTY(EditAnywhere, Category="House") TObjectPtr<USoundBase> EventSound;
    UPROPERTY(EditAnywhere, Category="House") FVector Movement = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="House") FRotator Swing = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, Category="House") float Duration = 2.4f;
    UPROPERTY(EditAnywhere, Category="House") float Frequency = 2.f;
    UPROPERTY(EditAnywhere, Category="House") bool bStayDisplaced = false;
    UPROPERTY(EditAnywhere, Category="House") bool bTelephone = false;
    UPROPERTY(EditAnywhere, Category="House") bool bRequirePlayerLooking = true;
    UPROPERTY(EditAnywhere, Category="House") bool bAlertsMim = false;
    UPROPERTY(EditAnywhere, Category="House") float NoiseLoudness = .35f;
    UPROPERTY(EditAnywhere, Category="House") float NoiseRange = 1200.f;
    UPROPERTY(EditAnywhere, Category="House") float MinimumDistance = 140.f;
    UPROPERTY(EditAnywhere, Category="House") float MaximumDistance = 750.f;
    UPROPERTY(EditAnywhere, Category="House") float InitialDelayMin = 24.f;
    UPROPERTY(EditAnywhere, Category="House") float InitialDelayMax = 65.f;
    UPROPERTY(EditAnywhere, Category="House") float QuietSeconds = 55.f;
    UPROPERTY(EditAnywhere, Category="House") int32 MaximumEvents = 2;

    void ResetForChapter();
    /** The chapter scares give some props a better sound than the one they were placed with. */
    void SetEventSound(USoundBase* Sound) { EventSound = Sound; if (EventAudio) EventAudio->SetSound(Sound); }
    UPROPERTY(Transient) bool bChapterSilent = false;
    int32 GetSelfScheduledEvents() const { return SelfScheduledEvents; }
    UFUNCTION(BlueprintCallable, Category="House") bool TryTriggerEvent();
    UFUNCTION(BlueprintPure, Category="House") bool IsEventActive() const { return bActive; }
    UFUNCTION(BlueprintPure, Category="House") int32 GetEventsTriggered() const { return EventsTriggered; }
    UFUNCTION(BlueprintPure, Category="House") int32 GetReportedNoiseCount() const;

private:
    bool PlayerCanExperienceEvent() const;
    FTransform Rest;
    FTransform ChapterRest;
    FRandomStream Random;
    float NextCheck = 0.f;
    float EventAge = 0.f;
    float NextRing = 0.f;
    float Phase = 0.f;
    bool bActive = false;
    int32 EventsTriggered = 0;
    int32 SelfScheduledEvents = 0;
};
