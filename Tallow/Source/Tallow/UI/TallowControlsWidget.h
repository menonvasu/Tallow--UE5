#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "TallowControlsWidget.generated.h"

class ATallowCharacter;

/** Small on-screen key legend, bottom-left. Shows only what's usable right now (hidden while tied to the chair,
 *  which has its own meter). Drawn in native code, no widget blueprint. */
UCLASS()
class TALLOW_API UTallowControlsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<ATallowCharacter> Player;
protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
private:
	FSlateColorBrush Solid = FSlateColorBrush(FLinearColor::White);
};
