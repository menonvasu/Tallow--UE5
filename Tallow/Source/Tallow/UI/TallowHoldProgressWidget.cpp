#include "TallowHoldProgressWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UTallowHoldProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Create dynamic material on construction
	if (ProgressRingImage && !DynamicRingMaterial)
	{
		DynamicRingMaterial = ProgressRingImage->GetDynamicMaterial();
	}
	
	SetPromptVisibility(false);
}

void UTallowHoldProgressWidget::SetHoldProgress(float InPercent)
{
	if (DynamicRingMaterial)
	{
		// push float directly to GPU
		DynamicRingMaterial->SetScalarParameterValue(TEXT("Percent"), FMath::Clamp(InPercent, 0.0f, 1.0f));
	}
}

void UTallowHoldProgressWidget::SetPromptVisibility(bool bShow)
{
	SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShow)
	{
		SetHoldProgress(0.0f);
	}
}
