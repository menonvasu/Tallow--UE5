#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowBeatPresenter.generated.h"

class ATallowBloodWriting;
class ATallowCharacter;
class ATallowDoor;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundBase;
class USoundMix;

/** Pass 2 beats (PASS2-ARCHITECTURE 4.4), staged in the house: the black blood writing, her eyes closing into the
 *  chair, Mim gone in a blink, the fuse blowing, the door that ends the chapter. Each is claimed the frame it is
 *  requested and cut only with the view covered. Transformation keeps the fade until the plush model exists. */
UCLASS()
class TALLOW_API UTallowBeatPresenter : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    ETallowChapterBeat GetStagedBeat() const { return Beat; }
    ATallowBloodWriting* GetWriting() const { return Writing; }
    /** The doors of a room: the house graph's doors on its edges, and any other door standing in its walls. */
    static void FindRoomDoors(const UWorld* World, FName Room, TArray<ATallowDoor*>& OutDoors);
    /** The writing beat, for tests: the doors it threw shut, whether it holds her view, the red lamp. */
    TArray<ATallowDoor*> GetSlammedDoors() const;
    bool IsHoldingView() const { return Beat == ETallowChapterBeat::BloodWriting && !bInputReleased && !bViewFree; }
    bool IsViewTurned() const;
    UPointLightComponent* GetStrobe() const { return Strobe; }
private:
    UFUNCTION() void OnBeatRequested(ETallowChapterBeat InBeat, const FTallowBeatContext& Context);
    UFUNCTION() void OnRoundReset(int32 ChapterIndex, ETallowRestartReason Reason);
    void BeginWriting();
    void StageWriting(float T);
    /** Every open door of her room thrown shut at once. Returns how many. */
    int32 SlamRoomDoors(const FVector& Words);
    /** Her view taken round onto the words and held there while they form. */
    void HoldView(float T);
    /** A failing red lamp in the room while the words form: irregular, never a steady beat. */
    void LightStrobe(const FVector& Words, const FVector& Normal);
    void StageStrobe(float T, float DeltaTime);
    void StageRestraint(float T);
    void StageRelease(float T);
    void StageFuse(float T);
    void StageReset(float T);
    /** True on the first tick at or past At. */
    bool Once(float T, float At);
    void Cut();
    void Finish();
    void Abort();
    void Clear();
    void SetLids(float Close, float Tremble, float LowerRate, float Dark, float Haze);
    void SetMix(USoundMix* Mix, bool bOn);
    void SurgeLights(float Level);
    void Fade(float From, float To, float Seconds);
    void Play(const TCHAR* Path, float Volume, const FVector* Where = nullptr, float Pitch = 1.f);
    UAudioComponent* Hold(const TCHAR* Path, float Volume, const FVector* Where = nullptr);
    USoundBase* Sound(const TCHAR* Path);
    ATallowCharacter* GetPlayer() const;
    AActor* GetMim() const;
#if !UE_BUILD_SHIPPING
    /** -TallowBeatsRehearse: plays chapter one's beats in order from its start (debug runs only). -TallowBeatsFilm=<dir>
     *  writes a frame every quarter second while a beat plays. */
    void Rehearse();
    void StageListening(const AActor* Story, const FVector& Anchor, bool bOpenDoors);
    void Film();
    int32 RehearseStep = -1;
    int32 WordsChapter = -1;
    double RehearseAt = 0.0, NextFrame = 0.0, FilmUntil = 0.0, FilmStep = .25;
    int32 Frames = 0;
    FString FilmDir;
#endif

    ETallowChapterBeat Beat = ETallowChapterBeat::None;
    int32 Serial = 0;
    FTallowBeatContext Context;
    double StartedAt = 0.0;
    TSet<int32> Fired;
    bool bCutDone = false;
    bool bInCut = false;
    bool bInputReleased = false;
    bool bHoldingBlack = false;
    bool bPausedMixes = false;
    float LowerLid = 0.f;
    double LidsAt = 0.0;
    FVector MimWas = FVector::ZeroVector;
    FVector FuseAt = FVector::ZeroVector;
    // The writing: when the words start forming, how long the turn onto them takes, the slam's jolt.
    float WordsDelay = 0.f, WordsWhole = 0.f, TurnSeconds = 0.f, SlamJolt = 0.f, StrobeLevel = 0.f;
    FQuat TurnFrom = FQuat::Identity;
    FVector WordsAt = FVector::ZeroVector;
    bool bViewFree = false;
    /** (seconds, level) steps of the lamp, each held until the next. */
    TArray<FVector2f> StrobeKeys;
    TArray<TWeakObjectPtr<ATallowDoor>> Slammed;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Strobe;
    /** Loaded with the world so the decal's shaders are ready long before the first writing. */
    UPROPERTY(Transient) TObjectPtr<class UMaterialInterface> BloodMaterial;
    /** A centimetre of the blood drawn in the air in front of her eyes at the start (see PrimeBlood). */
    UPROPERTY(Transient) TObjectPtr<class UDecalComponent> Primer;
    double PrimedAt = -1.0;
    void PrimeBlood();
    UPROPERTY(Transient) TObjectPtr<ATallowBloodWriting> Writing;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Lids;
    UPROPERTY(Transient) TObjectPtr<USoundMix> Muffle;
    UPROPERTY(Transient) TObjectPtr<USoundMix> Silence;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundMix>> Pushed;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Held;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Spark;
    UPROPERTY(Transient) TObjectPtr<class USoundAttenuation> Falloff;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Sounds;
    struct FSurged { TWeakObjectPtr<class UPointLightComponent> Light; float Intensity = 0.f; bool bVisible = false; };
    TArray<FSurged> Surged;
};
