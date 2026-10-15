#include "TallowLockpickWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	void DrawBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FSlateBrush* Brush, FVector2D Pos, FVector2D Size, FLinearColor Colour)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Colour);
	}

	void DrawLabel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, int32 Size, FVector2D Centre, FLinearColor Colour)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(Centre - TextSize * .5f)), Text, Font, ESlateDrawEffect::None, Colour);
	}
}


void UTallowLockpickWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

int32 UTallowLockpickWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!LockpickComponent) return Layer;

	const FLinearColor Panel(0.f, 0.f, 0.f, .72f), ShearLine(0.3f, 0.6f, 1.0f, 1.0f);
	const FVector2D Screen = AllottedGeometry.GetLocalSize();
	const FVector2D PanelSize(600.f, 300.f);
	const FVector2D PanelPos((Screen.X - PanelSize.X) * .5f, Screen.Y - PanelSize.Y - 60.f);
	DrawBox(OutDrawElements, ++Layer, AllottedGeometry, &Solid, PanelPos, PanelSize, Panel);
	
	
}

void InitializeLockpickWidget(class UTallowLockpickGame* InGameComponent) {}
