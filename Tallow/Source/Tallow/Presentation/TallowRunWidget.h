#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowRunWidget.generated.h"
class ATallowPlayDirector;
class ATallowCharacter;
class ATallowCreature;
UCLASS()
class TALLOW_API UTallowRunWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) TObjectPtr<ATallowPlayDirector> Director;
	void ResetChapters();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	TWeakObjectPtr<ATallowCharacter> Player;
	TWeakObjectPtr<ATallowCreature> Mim;
	float StartZ=0.f, ChapterTime=0.f, MotionTime=0.f;
	int32 Chapter=0;
	uint32 Visited=0;
	bool bFoundStart=false;
};
