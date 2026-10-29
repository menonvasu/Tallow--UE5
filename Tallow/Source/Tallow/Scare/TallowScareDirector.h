#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chapter/TallowChapterTypes.h"
#include "Scare/TallowScareScheduler.h"
#include "TallowScareDirector.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class UTallowSoundscapeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTallowScareFired, FName, Group, AActor*, Emitter, bool, bAlertedMim);

UCLASS()
class TALLOW_API UTallowScareDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	static UTallowScareDirector* Get(const UObject* WorldContext);
	void BeginChapter(int32 ChapterIndex, const TArray<FTallowScareGroupDef>& Groups, const TArray<FName>& EnabledGroups, const FTallowScareRules& Rules, int32 Seed);
	void SetPhase(ETallowChapterPhase Phase);
	void Stop();
	UFUNCTION(BlueprintPure, Category="Scares") bool OwnsScheduling() const { return bOwnsScheduling; }
	UFUNCTION(BlueprintPure, Category="Scares") int32 GetChapterEventCount() const { return ChapterEvents; }
	UPROPERTY(BlueprintAssignable, Category="Scares") FTallowScareFired OnScareFired;
    bool TryFireGroup(FName Group);
    /** Reviews only (level probes): fire a group now, as placed and heard in play, past its gaps and gates. */
    UFUNCTION(BlueprintCallable, Category="Scares|Review") bool FireGroupForReview(FName Group);
    void Describe(TArray<FString>& Lines) const;
    float GetLastFireTime() const { return Scheduler.GetLastFireTime(); }
    /** A placed scare sound is still sounding (the soundscape keeps its herrings and whispers off it). */
    bool IsCuePlaying() const;
protected:
    UFUNCTION() void OnPhaseChanged(ETallowChapterPhase OldPhase, ETallowChapterPhase NewPhase);
    bool Fire(int32 Index);
    void BindChapter();
    /** Nothing fires over a chase, a reading, a held breath, inside earned relief, or with Mim close enough to be heard. */
    bool HouseAllowsScare(bool& bOutNormal, bool& bOutMajor) const;
    bool FireSound(const FTallowScareGroupDef& Group, bool bAlert, FString& OutWhere);
    bool PlaceSound(const FTallowScareGroupDef& Group, FVector& OutFrom, FVector& OutTo);
    USoundBase* PickSound(const FTallowScareGroupDef& Group);
    UTallowSoundscapeComponent* FindSoundscape() const;
    struct FEmitterSettings
    {
        TWeakObjectPtr<class ATallowHousePresence> Emitter;
        bool bAlerts = false;
        float Loudness = 0.f, Range = 0.f;
        TWeakObjectPtr<USoundBase> Sound;
    };
    TArray<FEmitterSettings> OriginalSettings;
    TMap<FName, TArray<TWeakObjectPtr<class ATallowHousePresence>>> Emitters;
    TWeakObjectPtr<class UTallowChapterSubsystem> BoundChapter;
    mutable TWeakObjectPtr<UTallowSoundscapeComponent> Soundscape;
    FTallowScareScheduler Scheduler;
    FRandomStream Random;
    ETallowChapterPhase CurrentPhase = ETallowChapterPhase::None;
    int32 Chapter = 0;
    float SampleAge = 0.f;
	bool bOwnsScheduling = false;
	int32 ChapterEvents = 0;
    /** The scare sound now playing; Above cues walk from one point to another while they sound. */
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Cue;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> PlacedAttenuation;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> ThroughAttenuation;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<USoundBase>> Loaded;
    TMap<FName, FString> LastSound;
    FVector CueFrom = FVector::ZeroVector, CueTo = FVector::ZeroVector;
    float CueStarted = 0.f, CueSeconds = 0.f, ResumeAt = 0.f, BlockedSince = -1.f;
};
