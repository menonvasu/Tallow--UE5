#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowLockpickWidget.generated.h"

UCLASS()
class TALLOW_API UTallowLockpickWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** connects UI to the game simulation */
	void InitializeLockpickWidget(class UTallowLockpickGame* InGameComponent);
	
protected:
	virtual void NativeConstruct();
	
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
	/** Reference to the model */
	TObjectPtr<UTallowLockpickGame> LockpickComponent;	
	
private:
	FSlateColorBrush Solid = FSlateColorBrush(FLinearColor::White);	
};
