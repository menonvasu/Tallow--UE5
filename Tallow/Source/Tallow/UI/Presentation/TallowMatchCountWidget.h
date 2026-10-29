#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowMatchCountWidget.generated.h"

/** "matches 7", scrawled in the corner for a moment whenever the count changes or a strike fails. */
UCLASS()
class TALLOW_API UTallowMatchCountWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTallowMatchCountWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION()
	void ShowCount(int32 Count, bool bTriedWithNone);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText Text;
	double ShownAt = -100.0;
};
