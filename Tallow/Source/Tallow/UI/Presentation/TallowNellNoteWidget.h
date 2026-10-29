#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowNellNoteWidget.generated.h"

/** One thing Nell has just understood, in her own hand on a torn scrap, low in the middle of the screen. It fades in,
 *  holds long enough to read twice, and goes. Never a panel, never an icon; the optional second line is the key she
 *  would press, in pencil, the way Maisie writes keys in her book. Painted only; never takes input or focus. */
UCLASS()
class TALLOW_API UTallowNellNoteWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UTallowNellNoteWidget(const FObjectInitializer& ObjectInitializer);
	void ShowNote(const FString& InText, const FString& InHint, float InSeconds);
	/** Drop the note quickly (being caught, a menu). */
	void HideNote();
	/** Hold the note's clock while the player can't look at it (reading, the inventory, the map). */
	void SetSuppressed(bool bInSuppressed) { bSuppressed = bInSuppressed; }
	bool IsShowing() const { return Age < Seconds; }
	float GetAge() const { return Age; }
	const FString& GetText() const { return Text; }
#if WITH_EDITOR
	/** Visual fixture: the note fully written in, as it holds. */
	void ConfigureReview(const FString& InText, const FString& InHint) { ShowNote(InText, InHint, 30.f); Age = 2.f; }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	FString Text, Hint;
	float Age = 1e6f, Seconds = 0.f;
	bool bSuppressed = false;
};
