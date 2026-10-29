#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowSoundscapeComponent.generated.h"

class ATallowCharacter;
class ATallowCreature;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundMix;

/** One level for the whole mix, from the Keeper's state and distance (Recording/MIX-PLAN.md, section 3). */
UENUM(BlueprintType)
enum class ETallowTension : uint8
{
	Calm, Uneasy, Hunted, Chase, Caught, After
};

/** The house's mix around the player: beds ducked by the tension level, the two drones, the horror bed for the
 *  last stretch, the lies (rare sounds placed where the Keeper is not), and the girl's own gasps and held breath.
 *  The Keeper's honest tells live on the Keeper (UTallowKeeperVoiceComponent). */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowSoundscapeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowSoundscapeComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Setup(ATallowCharacter* InPlayer, ATallowCreature* InKeeper);
	UFUNCTION(BlueprintCallable, Category="Audio") void SetKeeperAwake(bool bAwake);
	void SetPowerRestored(bool bRestored);
	void NotifyCaught();
	void NotifyReset();
	void NotifyCandleOut();
	void PlayEffort();
	/** Authored room moments share the random scheduler's safety and cooldowns. */
	UFUNCTION(BlueprintCallable, Category="Audio") bool RequestDistraction();

	UFUNCTION(BlueprintPure, Category = "Audio")
	ETallowTension GetTension() const { return Tension; }
	UFUNCTION(BlueprintPure, Category="Audio|Review") bool HasDistractionPlaying() const;
	UFUNCTION(BlueprintPure, Category="Audio|Review") float GetHouseGain() const { return BedGain * HushGain; }
	/** Earned recovery after a search or escape; a fresh detection still takes priority. */
	UPROPERTY(EditAnywhere, Category="Audio|Pacing", meta=(ClampMin="8", ClampMax="45"))
	float ReliefSeconds = 22.f;

	/** The house settling only plays while nothing is near. */
	bool AllowsSettles() const { return Tension == ETallowTension::Calm || Tension == ETallowTension::Uneasy; }
	/** The house's own rooms, weather and details (they replace the director's random settles). */
	bool HasHouseAtmosphere() const { return Atmosphere != nullptr; }
	/** A minor scare may play: nothing near, no chase, no relief, no reading, no held breath, nothing else sounding. */
	bool AllowsMinorScare() const;
	/** The scare director's whisper pass: the authored whisper, on the director's clock instead of its own. */
	bool PlayWhisperPass();

	/** Built on the FNAF 1 sound reference I used (Scruffy, "How Audio Enhances the Horror of FNAF"): some sound
	 *  helps you (patterns you learn), some works against you (a masking bed, red herrings), and holding still
	 *  to listen costs you something, like the cameras cost power. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	bool bDrones = true;

	UPROPERTY(EditAnywhere, Category = "Audio|Lies")
	bool bLies = true;

	/** Once the power is back, the fuse box hums within a few steps of it. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	bool bPowerHum = true;

	/** A rare recorded giggle on a room change, shared with the creature's speech scheduler. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	float RoomTellGap = 35.f;

	/** Seconds between lies, at random within this range. */
	UPROPERTY(EditAnywhere, Category = "Audio|Lies")
	FVector2D LieGap = FVector2D(75.f, 150.f);

	/** Share of lies played near the next objective, so a sharp player can learn to follow them. */
	UPROPERTY(EditAnywhere, Category = "Audio|Lies")
	float FairLieChance = .25f;

private:
	USoundBase* Load(const FString& Folder, const FString& Name);
	UAudioComponent* MakeLoop(const FString& Folder, const FString& Name);
	ETallowTension ComputeTension(float Now) const;
	void UpdateMix(float DeltaTime);
	void UpdateLies(float Now);
	void UpdateVoice(float Now);
	bool PlayLie(float Now);
	/** How sparse the red herrings are at this point of the evening's arc (1 = the authored gap). */
	float LieGapScale() const;
	FVector NextObjective() const;
	bool CanSeeKeeper() const;
	void Gasp(float Now);
	void RefreshReadingFocus(float Now);
	void InterruptDistractions();
	void PlayPlayer(const FString& Name, float Volume);
	void FollowEarsWithoutRenderer(float DeltaTime);
	UPROPERTY(Transient) TObjectPtr<USoundMix> StateMix;
	UPROPERTY(Transient) TMap<FName,TObjectPtr<USoundClass>> MixClasses;
	TMap<FName,float> LastClassGains;
	bool bNarrating=false;
	float NextReadingCheck=0.f;
	FTimerHandle GaspHandle;

	TWeakObjectPtr<ATallowCharacter> Player;
	TWeakObjectPtr<ATallowCreature> Keeper;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> DarkDrone;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> BassDrone;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> HorrorBed;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> PowerHum;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Beds;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> CurrentLie;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> LieAttenuation;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Sounds;
	TArray<float> BedBase;
	TArray<FString> RecentLies;
	ETallowTension Tension = ETallowTension::Calm;
	ETallowTension LastLevel = ETallowTension::Calm;
	/** The house goes quiet when the Keeper is about to enter Nell's room. */
	UPROPERTY(EditAnywhere, Category="Audio|Hush") float HushNearCm = 600.f;
	UPROPERTY(EditAnywhere, Category="Audio|Hush") float HushDepth = .06f;
	UPROPERTY(EditAnywhere, Category="Audio|Hush") float HushHoldSeconds = 4.f;
	void UpdateHush(float DeltaTime);
		UPROPERTY(EditAnywhere, Category="Audio|Hush", meta=(ClampMin="0", ClampMax="1")) float HushChance = .33f;
	bool bWasEntering = false, bHushThisEntry = false;
	float HushGain = 1.f, HushUntil = 0.f, LastKeeperDistance = -1.f;
	float BedGain = 1.f, DarkGain = 0.f, BassGain = 0.f, HorrorGain = 0.f;
	float NextLie = 0.f, NoLiesUntil = 0.f, LastShriek = -1000.f, AfterUntil = 0.f;
	float LastBreath = -10.f, LastGasp = -10.f, NextSightCheck = 0.f, LastRoomTell = -100.f, ListenGain = 1.f;
	FIntVector KeeperCell = FIntVector(MAX_int32);
	void UpdateRoomTell(float Now);
	void UpdateWhispers(float Now);
	bool Whisper(float Now, int32 EndSide, bool bDirected = false);
	UPROPERTY(Transient) TObjectPtr<class UTallowHouseAtmosphereComponent> Atmosphere;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> WhisperVoice;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> WhisperAttenuation;
	float HeardSeenCall = -100.f, LastWhisper = -1000.f, WhisperStarted = 0.f, LastProgressAt = 0.f, DarkSince = -1.f, KeeperWasNearHidden = -1.f;
	int32 WhispersPlayed = 0, WhisperFrom = 1, ProgressSignature = -1;
	bool bWhisperedDark = false, bWhisperedToyRoom = false;
	float SafeSince = -1.f;
	bool bPendingRelease = false;
	int32 LastGaspIndex = -1;
	bool bKeeperAwake = false, bPowerRestored = false, bCaught = false, bWasHolding = false, bSeenThisEncounter = false;
};
