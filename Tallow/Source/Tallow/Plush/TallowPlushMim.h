#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chapter/TallowChapterTypes.h"
#include "Plush/TallowPlushRules.h"
#include "TallowPlushMim.generated.h"

/** The small toy Mim of the exploration: never lethal, never in the way. Presentation hooks are for the art side. */
UCLASS(Blueprintable)
class TALLOW_API ATallowPlushMim : public AActor
{
	GENERATED_BODY()
public:
	ATallowPlushMim();
    virtual void Tick(float DeltaSeconds) override;
	static ATallowPlushMim* FindOrSpawn(UWorld* World, TSubclassOf<ATallowPlushMim> Class);
	void BeginChapter(int32 ChapterIndex, const FTallowPlushRules& Rules, AActor* StoryObject, int32 Seed);
	void PrepareForTransformation(FName ChairRoom);
	FTransform BeginTransformation();
    bool StageForTransformation(const FTransform& Where, bool bViewCovered);
	void Retire();
	UFUNCTION(BlueprintPure, Category="Plush") ETallowPlushState GetPlushState() const { return PlushState; }
	UFUNCTION(BlueprintImplementableEvent, Category="Plush") void OnPlushStateChanged(ETallowPlushState OldState, ETallowPlushState NewState);
	UFUNCTION(BlueprintImplementableEvent, Category="Plush") void OnPerched(AActor* Perch);
	UFUNCTION(BlueprintImplementableEvent, Category="Plush") void OnMinorScare(FName ScareId);
	UFUNCTION(BlueprintImplementableEvent, Category="Plush") void OnSeenByPlayer(float SecondsSeen);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plush") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Plush") TObjectPtr<USkeletalMeshComponent> Body;
    bool TryMinorScare(FName ScareId);
    bool TryRelocate();
    bool IsSeenAt(const FVector& Position) const;
    bool IsPreparedForTransformation(FName Room) const;
    AActor* GetPerch() const { return CurrentPerch.Get(); }
    int32 GetRelocationCount() const { return Relocations; }
    void Describe(TArray<FString>& Lines) const;
protected:
    void SetState(ETallowPlushState State);
    void GatherPerches();
    void BuildContext(FTallowPerchContext& Out) const;
    bool Place(FName Room, bool bStart);
    bool PerchUsable(AActor* Perch) const;
    bool CanMove() const;
    TArray<TWeakObjectPtr<AActor>> Perches;
    TWeakObjectPtr<AActor> CurrentPerch;
    TWeakObjectPtr<AActor> Story;
    FTallowPlushRules Rules;
    FRandomStream Random;
    FName PendingRoom;
    float UnseenAge = 0.f, SeenAge = 0.f, RelocateAge = 0.f, RelocateAfter = 0.f, ScareAge = 0.f;
    int32 Relocations = 0;
    bool bChapterEnabled = false;
	ETallowPlushState PlushState = ETallowPlushState::Hidden;
};
