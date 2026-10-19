#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowLessonWidget.generated.h"

UENUM(BlueprintType)
enum class ETallowLesson : uint8
{
	None,
	Chair,
	CandlePickup,
	Draft,
	Cup,
	Relight,
	Stillness
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTallowLessonCompleted, ETallowLesson, Lesson);

/** Presentation only. Retain this widget across capture; gameplay reports lesson success. */
UCLASS()
class TALLOW_API UTallowLessonWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UTallowLessonWidget(const FObjectInitializer& ObjectInitializer);

	/** Replaces an incomplete prompt. Completed lessons and None are ignored. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Lessons")
	bool ShowLesson(ETallowLesson Lesson);
	UFUNCTION(BlueprintCallable, Category="Tallow|Lessons")
	void CompleteLesson();
	/** Use from gameplay callbacks: rejects a result from an earlier lesson. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Lessons")
	bool CompleteLessonIfActive(ETallowLesson ExpectedLesson);
	/** Hides without marking complete, so the lesson can be offered again. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Lessons")
	void DismissLesson();
	/** Explicit new-run reset; never called automatically on capture or reconstruction. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Lessons")
	void ResetLessons();
	UFUNCTION(BlueprintPure, Category="Tallow|Lessons")
	bool HasSeenLesson(ETallowLesson Lesson) const { return SeenLessons.Contains(Lesson); }
	UFUNCTION(BlueprintPure, Category="Tallow|Lessons")
	bool HasCompletedLesson(ETallowLesson Lesson) const { return CompletedLessons.Contains(Lesson); }
	UFUNCTION(BlueprintPure, Category="Tallow|Lessons")
	ETallowLesson GetActiveLesson() const { return ActiveLesson; }
	UFUNCTION(BlueprintPure, Category="Tallow|Lessons")
	FText GetLessonPrompt() const;

	UPROPERTY(BlueprintAssignable, Category="Tallow|Lessons")
	FTallowLessonCompleted OnLessonCompleted;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Lessons|Controls")
	FText RockKey = FText::FromString(TEXT("Space"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Lessons|Controls")
	FText InteractKey = FText::FromString(TEXT("E"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Lessons|Controls")
	FText CupKey = FText::FromString(TEXT("RMB"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Lessons|Controls")
	FText RelightKey = FText::FromString(TEXT("R"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Lessons|Controls")
	FText StillnessKey = FText::FromString(TEXT("F"));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	ETallowLesson ActiveLesson = ETallowLesson::None;
	UPROPERTY(Transient)
	TSet<ETallowLesson> SeenLessons;
	UPROPERTY(Transient)
	TSet<ETallowLesson> CompletedLessons;
};
