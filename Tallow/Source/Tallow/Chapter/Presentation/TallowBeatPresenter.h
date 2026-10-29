#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowBeatPresenter.generated.h"

class ATallowBloodWriting;
class ATallowCharacter;
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
private:
    UFUNCTION() void OnBeatRequested(ETallowChapterBeat InBeat, const FTallowBeatContext& Context);
    UFUNCTION() void OnRoundReset(int32 ChapterIndex, ETallowRestartReason Reason);
    void BeginWriting();
    void StageWriting(float T);
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
    void Film();
    int32 RehearseStep = -1;
    int32 WordsChapter = -1;
    double RehearseAt = 0.0, NextFrame = 0.0, FilmUntil = 0.0;
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
