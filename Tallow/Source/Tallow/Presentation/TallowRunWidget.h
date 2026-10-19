#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowRunWidget.generated.h"
class ATallowPlayDirector;
UCLASS()
class TALLOW_API UTallowRunWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) TObjectPtr<ATallowPlayDirector> Director;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
