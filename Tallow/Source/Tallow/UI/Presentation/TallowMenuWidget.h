#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "TallowMenuWidget.generated.h"

class SVerticalBox;
class SScrollBox;
class UFont;
class UTexture2D;

UENUM(BlueprintType)
enum class ETallowMenuMode : uint8 { Main, Pause, Outro };

UENUM(BlueprintType)
enum class ETallowMenuPage : uint8 { Home, Controls, Credits, Confirm };

UENUM(BlueprintType)
enum class ETallowMenuAction : uint8 { Start, Resume, Restart, MainMenu, Quit, Continue };

UENUM(BlueprintType)
enum class ETallowMenuCommand : uint8
{
	Start, Resume, Restart, MainMenu, Quit, Controls, Credits, Back, Confirm, Cancel, Settings, BabyMode, Continue, Vibration, InvertLook, InvertMove, HiddenVoice, Volume, Brightness
};

/** Small presentation state machine shared by the widget and its navigation tests. */
struct TALLOW_API FTallowMenuState
{
	ETallowMenuMode Mode = ETallowMenuMode::Main;
	ETallowMenuPage Page = ETallowMenuPage::Home;
	int32 Selection = 1;
	bool bAwaitingAction = false;
	bool bContinueAvailable = false;
	TOptional<ETallowMenuAction> PendingConfirmation;

	void Show(ETallowMenuMode InMode);
	TArray<ETallowMenuCommand> Commands() const;
	void Navigate(int32 Direction);
	bool IsEnabled(ETallowMenuCommand Command) const;
	TOptional<ETallowMenuAction> Activate(ETallowMenuCommand Command);
	TOptional<ETallowMenuAction> Back();

private:
	int32 ReturnSelection = 0;
	TOptional<ETallowMenuAction> Request(ETallowMenuAction Action);
	void ReturnHome();
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTallowMenuActionRequested, ETallowMenuAction, Action);

/** Menu presentation only. The owner handles pause, input mode, level loading and quit. */
UCLASS()
class TALLOW_API UTallowMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UTallowMenuWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void ShowMenu(ETallowMenuMode Mode);
	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void Navigate(int32 Direction);
	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void ActivateSelected();
	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void ActivateCommand(ETallowMenuCommand Command);
	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void GoBack();
	/** Call if the owner rejects/fails a request and keeps this menu open. */
	UFUNCTION(BlueprintCallable, Category="Tallow|Menu")
	void RearmActions();
	UFUNCTION(BlueprintPure, Category="Tallow|Menu")
	ETallowMenuPage GetMenuPage() const { return State.Page; }
	UFUNCTION(BlueprintPure, Category="Tallow|Menu")
	ETallowMenuMode GetMenuMode() const { return State.Mode; }
	UFUNCTION(BlueprintPure, Category="Tallow|Menu")
	bool IsAwaitingAction() const { return State.bAwaitingAction; }

	UPROPERTY(BlueprintAssignable, Category="Tallow|Menu")
	FTallowMenuActionRequested OnActionRequested;
	/** Supply remapped controls here if the input bindings change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Menu")
	FText ControlsText;
	/** Extend with the final asset/music credits before release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Menu", meta=(MultiLine=true))
	FText CreditsText;
	/** Bundled serif used for the title fallback and menu choices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Menu")
	TObjectPtr<UFont> TitleFont;
	/** Original transparent hand-cut title, referenced for cooking and GC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tallow|Menu")
	TObjectPtr<UTexture2D> TitleArtwork;

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UPROPERTY(Transient)
	FSlateBrush TitleBrush;
	FTallowMenuState State;
	TSharedPtr<SVerticalBox> Content;
	TSharedPtr<SScrollBox> ReadingScroll;
	void Refresh();
	void Dispatch(TOptional<ETallowMenuAction> Action);
	FText Heading() const;
	FText Body() const;
	FText Label(ETallowMenuCommand Command) const;
};
