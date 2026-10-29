#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowChairEscapeWidget.generated.h"

class ATallowChairEscape;

/** Swing meter for the chair escape. Drawn in native code so it reads the chair directly: a pencil track on a scrap
 *  of card, an ink stroke follows the chair's tilt and the back of the track inks in when a rock will land.
 */
UCLASS()
class TALLOW_API UTallowChairEscapeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = "Chair")
	TObjectPtr<ATallowChairEscape> Chair;

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Paint is const; these only drive the "too early" flash
	mutable int32 LastRockCount = 0;
	mutable double MissFlashUntil = 0.0;
};
