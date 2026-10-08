#include "TallowInteractWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void UTallowInteractWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTallowInteractWidget::SetHoldProgress(float InPercent)
{
	if (HoldProgressBar)
	{
		// push float directly to GPU
		HoldProgressBar->SetPercent(FMath::Clamp(InPercent, 0.0f, 1.0f));
	}
}

void UTallowInteractWidget::ShowPrompt(const FText& ActionText, const bool bIsHold, const bool bCanInteract)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	
	// Determine color based on interactability
	const FLinearColor TargetColor = bCanInteract ? NormalColor : WarningColor;
	
	// Set Text and Color
	if (ActionLabelText)
	{
		ActionLabelText->SetText(ActionText);
		ActionLabelText->SetColorAndOpacity(FSlateColor(TargetColor));
	}	
	
	// if not held interact, hide ring
	if (!bIsHold)
	{
		if (HoldProgressBar)
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	// if hold interact
	else
	{
		if (HoldProgressBar)
		{
			HoldProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UTallowInteractWidget::HidePrompt()
{
	// Collapse UI and reset progress
	SetVisibility(ESlateVisibility::Collapsed);
	if (HoldProgressBar)
	{
		HoldProgressBar->SetPercent(0.0f);
	}
}
