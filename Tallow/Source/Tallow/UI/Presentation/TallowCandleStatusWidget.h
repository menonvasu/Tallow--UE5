#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Candle/TallowCandleComponent.h"
#include "TallowCandleStatusWidget.generated.h"
class UTallowCandleComponent;
UCLASS()
class TALLOW_API UTallowCandleStatusWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UTallowCandleStatusWidget(const FObjectInitializer& O);
 void Setup(UTallowCandleComponent* InCandle) { Candle=InCandle; }
#if WITH_EDITOR
 /** Layout review without a player: a fixed wax stage, lit or out. */
 void ConfigureReview(ETallowCandleFuelStage Stage, bool bLit);
#endif
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 TWeakObjectPtr<UTallowCandleComponent> Candle;
 bool bReview=false, bReviewLit=true;
 ETallowCandleFuelStage ReviewStage=ETallowCandleFuelStage::Full;
 bool IsShown() const;
 bool IsLit() const;
 ETallowCandleFuelStage Stage() const;
};
