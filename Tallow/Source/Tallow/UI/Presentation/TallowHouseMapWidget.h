#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Input/TallowUIInput.h"
#include "TallowHouseMapWidget.generated.h"
class UTallowHouseMapComponent;
class SWidget;
UCLASS()
class TALLOW_API UTallowHouseMapWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UTallowHouseMapWidget(const FObjectInitializer& O);
 static bool ApplyPadIntent(ETallowPadIntent Intent, int32& ViewedFloor, bool& bClose);
 void Open(UTallowHouseMapComponent* InMap,APlayerController* PC);
 UFUNCTION(BlueprintCallable,Category="Map")void Close(bool bImmediate=false);
 UFUNCTION(BlueprintCallable,Category="Map")void SetViewedFloor(int32 InFloor){Floor=FMath::Clamp(InFloor,0,2);}
#if WITH_EDITOR
 void ConfigureReview(){bReview=true;Floor=1;}
#endif
protected:
 virtual TSharedRef<SWidget> RebuildWidget()override;
 virtual FReply NativeOnKeyDown(const FGeometry&,const FKeyEvent&)override;
 virtual void NativeTick(const FGeometry&,float Dt)override;
 virtual void NativeDestruct()override;
 virtual void ReleaseSlateResources(bool B)override;
private:
 TWeakObjectPtr<UTallowHouseMapComponent>Map;
 TWeakObjectPtr<APlayerController>InputOwner;
 TSharedPtr<SWidget>Paper;
 float Age=0.f,CloseAge=-1.f;
 int32 Floor=0;
 bool bOpen=false,bHadCursor=false,bReview=false;
 FTimerHandle FoldTimer;
 void ReleaseInput();
};
