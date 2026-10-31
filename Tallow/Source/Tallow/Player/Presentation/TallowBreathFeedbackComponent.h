#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowBreathFeedbackComponent.generated.h"

class ATallowCharacter;
class USoundBase;
class USoundMix;
class USceneComponent;

/** 28 Oct: "you just press F and freeze and have no idea." Holding her breath is felt, never written: a sharp
 *  intake, the house going dull and far away (Mim, her own breath and heartbeat stay clear), a heartbeat that
 *  speeds up, the candle steadying in a still hand, then the strain: a trembling flame, a sound she can't keep in,
 *  and at the limit a gasp (ATallowCharacter owns the rules; this only answers them). No vignette. */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowBreathFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTallowBreathFeedbackComponent();
	void Setup(ATallowCharacter* InPlayer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** How shut-in the world is right now (0 breathing, 1 fully held), for tests and the capture tools. */
	UFUNCTION(BlueprintPure, Category="Breath") float GetHoldAmount() const { return Hold; }
	UFUNCTION(BlueprintPure, Category="Breath") int32 GetHeartbeats() const { return Beats; }
	/** True once a hold has seen Mim pass close by and leave without finding her (Nell's notes read this). */
	UFUNCTION(BlueprintPure, Category="Breath") bool HasHeldPast() const { return bHeldPast; }
	float GetLastPastAt() const { return LastPastAt; }

	/** Levels from measured loudness: the efforts are mastered to -26 LUFS and Nell's lines to -24 LUFS played at .38
	 *  (about -32 LUFS heard), so .42 puts the intake just under her voice, where I can hear it. */
	UPROPERTY(EditAnywhere, Category="Breath") float IntakeVolume = .42f;
	UPROPERTY(EditAnywhere, Category="Breath") float ExhaleVolume = .34f;
	UPROPERTY(EditAnywhere, Category="Breath") float StrainVolume = .3f;
	UPROPERTY(EditAnywhere, Category="Breath") float GaspVolume = .55f;
	UPROPERTY(EditAnywhere, Category="Breath") float HeartbeatVolume = .7f;
	/** How dull the house gets: the low-pass on everything but Mim, Nell's voice and the readings. */
	UPROPERTY(EditAnywhere, Category="Breath") float MuffleHz = 650.f;
	UPROPERTY(EditAnywhere, Category="Breath") float MuffleVolume = .62f;

private:
	UFUNCTION() void OnStill(bool bStill);
	UFUNCTION() void OnReleased(bool bForced);
	USoundBase* Sound(const TCHAR* Path);
	void Play(USoundBase* Sound, float Volume);
	void SetMuffled(bool bOn);
	float MimDistance() const;

	TWeakObjectPtr<ATallowCharacter> Player;
	TWeakObjectPtr<AActor> Mim;
	TWeakObjectPtr<USceneComponent> Candle;
	FVector CandleBase = FVector::ZeroVector;
	UPROPERTY(Transient) TObjectPtr<USoundMix> Muffle;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Sounds;
	float Hold = 0.f, NextBeat = 0.f, NextPant = 0.f, Time = 0.f, NearestDuringHold = 1e7f, LastPastAt = -100.f;
	int32 Beats = 0;
	bool bMuffled = false, bStrainSounded = false, bHeldPast = false, bPantLogged = false;
};
