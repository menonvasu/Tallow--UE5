#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowStealthFeedbackWidget.generated.h"

class ATallowStealthCharacter;

/** Brief, silent feedback. It appears only while bracing, recovering or in a tight passage. */
UCLASS()
class TALLOW_API UTallowStealthFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetCharacter(ATallowStealthCharacter* InCharacter) { Character = InCharacter; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
    TWeakObjectPtr<ATallowStealthCharacter> Character;
};
