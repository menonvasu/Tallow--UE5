#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowKeeperVoiceComponent.generated.h"

class USoundBase;
class USoundAttenuation;
class UAudioComponent;

/** What the Keeper is saying, one pool of my recorded lines each (Tools/Audio/CREATURE-VOICE.md). */
UENUM(BlueprintType)
enum class ETallowKeeperLine : uint8
{
	Lure,       // from the soft heap, before you know what it is: "Hi, are you lost?"
	Murmur,     // to itself while it wanders: "I don't remember being little."
	Heard,      // it caught a sound: "Shhhh!", "I'm trying to listen."
	Search,     // looking for you: "Come out, come out wherever you are!"
	Seen,       // the call when it sees you, the one the player learns: "Hello!"
	Chase,      // "Wait for me, you're too fast!"
	Lost,       // it lost you: "Where'd you go?"
	Capture,    // "Gotcha!"
	Breath,     // wordless: gasps, hisses, a hum
	Giggle      // the Smile: it doesn't roar, it giggles (my voice-changer take)
};

/** The Keeper's voice and body sounds, always from where it really is: my recorded lines picked by what it is
 *  doing (never more than one at a time, every pool shuffled so nothing repeats until the rest have played), soft
 *  scratches and knocks while it searches, and its line on the capture. The brain can ask for a line with Say();
 *  otherwise it follows CurrentState and the player's SeenCallAt. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowKeeperVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	bool IsSpeaking() const;
	UTallowKeeperVoiceComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Distance (cm) at which its voice fades out */
	UPROPERTY(EditAnywhere, Category = "Keeper|Voice")
	float HearingDistance = 1800.f;
	/** The capture scream starts this long before the strike: its breath, then its attack on the jaw snapping open,
	 *  80 ms ahead of the sting so the sting's own attack does not bury it. */
	UPROPERTY(EditAnywhere, Category = "Keeper|Voice", meta=(ClampMin="0", ClampMax=".3"))
	float ScreamLeadSeconds = .18f;
	/** Imported low voice appears regularly during aggression, never in calm chatter. */
	UPROPERTY(EditAnywhere, Category="Keeper|Voice", meta=(ClampMin="0", ClampMax="1"))
	float DemonChance = .4f;
	UPROPERTY(EditAnywhere, Category="Keeper|Voice", meta=(ClampMin="10"))
	float DemonCooldown = 12.f;

	/** Speak now if this pool is off cooldown and nothing else of it is playing. Returns whether it spoke. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	bool Say(ETallowKeeperLine Line, bool bForce = false);

	/** One of the story's scripted lines from /Game/Audio/VO (VO_Mim_*), cutting across anything else. Until I have
	 *  recorded it, Fallback (one of his creature lines) plays instead, if given. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	bool SayLine(const FString& Name, const FString& Fallback = TEXT(""), float Volume = .6f);

	/** The brain sets this while it sits as the soft heap: it lures instead of searching aloud. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void SetDisguised(bool bOn) { bDisguised = bOn; }

	/** The brain sets this while it lies in wait (a hold, a corner, a covered ambush, a watch): it keeps still and
	 *  quiet, with no search lines or scratching to give the place away; only its breath this close. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void SetLurking(bool bOn) { bLurking = bOn; }

	/** The hunter calls this while it creeps in silence: no ordinary lines, no scratching, and the soundscape and the
	 *  footstep component keep her foley near-inaudible and the house bed up, so her arrival is not announced. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void SetCreeping(bool bOn) { bCreeping = bOn; }
	UFUNCTION(BlueprintPure, Category = "Keeper|Voice")
	bool IsCreeping() const { return bCreeping; }

	/** Nails on wood or a soft knock from where it is (the brain calls this at doors and on walls). */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void Foley(bool bKnock, float Volume = .35f);

	/** Sleeping and reset hunters leave the house audible without talking over the opening. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void SetAwake(bool bOn);
	void SetRecoveryQuiet(bool bQuiet) { bRecoveryQuiet=bQuiet; }

	/** Stop speech and cancel every delayed capture cue when a run resets. */
	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void ResetVoice();

	/** Review telemetry for delayed cues that must be cleared before a run reset. */
	UFUNCTION(BlueprintPure, Category = "Keeper|Voice|Review")
	int32 GetPendingCaptureCueCount() const;

	UFUNCTION(BlueprintCallable, Category = "Keeper|Voice")
	void PlayCapture();

private:
	bool bRecoveryQuiet=false;
	USoundBase* Sound(const FString& Name);
	USoundBase* Next(ETallowKeeperLine Line);
	void UpdateVoice(float Now);
	bool Speak(USoundBase* Cue, float Volume, float Pitch = 1.f);
	void CaptionLine(USoundBase* Cue);
	void ClearCaptureTimers();
	void PlayCaptureGiggle();
	void PlayCaptureLine();

	struct FPool { TArray<FString> Names; TArray<int32> Bag; float Gap = 10.f, Volume = .5f, Ready = 0.f; };
	TMap<ETallowKeeperLine, FPool> Pools;
	FPool DemonPool;
	TMap<FName, float> PlayedAt;
	float NextDemon = 0.f;
	USoundBase* Draw(FPool& Pool);

	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FoleyAttenuation;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Speaking;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FoleyPlaying;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Cache;
	uint8 LastState = 0;
	float QuietUntil = 0.f, NextAny = 0.f, NextOrdinary = 0.f, NextFoley = 0.f, NextFoleyAllowed = 0.f;
	float HeardSeenCall = -100.f, ChaseStarted = -100.f, CaptureEndsAt = 0.f;
	float AggressiveUntil = 0.f;
	int32 LastKnock = INDEX_NONE, LastScratch = INDEX_NONE;
	bool bDisguised = false, bLurking = false, bCreeping = false, bAwake = true, bCapturing = false;
	FTimerHandle StrikeHandle, ScreamHandle, GiggleHandle, CaptureLineHandle;
};
