#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/TallowCharacter.h"
#include "TallowInventoryWidget.generated.h"
class ATallowCharacter;
class SVerticalBox;
class SScrollBox;
UCLASS()
class TALLOW_API UTallowInventoryWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UTallowInventoryWidget(const FObjectInitializer& O);
 UFUNCTION(BlueprintCallable, Category="Inventory") void Open(APlayerController* PC);
 UFUNCTION(BlueprintCallable, Category="Inventory") void Close();
#if WITH_EDITOR
 void ConfigureReview();
#endif
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry&, const FKeyEvent&) override;
 virtual void ReleaseSlateResources(bool) override;
 virtual void NativeDestruct() override;
private:
 TWeakObjectPtr<ATallowCharacter> Player;
 struct FEntry { FText Name; FText Description; int32 Mark; FName Document; ECarriedItemType Item = ECarriedItemType::None; float Amount = 1.f; };
 TArray<FEntry> Entries;
 int32 Selected=0;
 bool bHadCursor=false, bOpen=false;
 TSharedPtr<SVerticalBox> Detail;
 TSharedPtr<SVerticalBox> ItemSlots;
 TSharedPtr<SScrollBox> ItemScroll;
 TWeakObjectPtr<APlayerController> InputOwner;
 void BuildEntries();
 void Refresh();
 void ReadSelected();
 UPROPERTY(Transient) TObjectPtr<class UTallowJournalWidget> Reader;
};
