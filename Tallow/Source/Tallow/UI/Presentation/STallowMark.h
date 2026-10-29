#pragma once
#include "UI/Presentation/TallowPaper.h"
#include "Widgets/SLeafWidget.h"

/** Small engraved marks, shared by menus, chapter cards and gameplay. */
class STallowMark : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STallowMark) : _Mark(TallowPaper::EMark::Eye), _Colour(TallowPaper::Ink), _Size(88.f), _Amount(1.f) {}
		SLATE_ATTRIBUTE(TallowPaper::EMark, Mark)
		SLATE_ATTRIBUTE(FLinearColor, Colour)
		SLATE_ARGUMENT(float, Size)
		SLATE_ATTRIBUTE(float, Amount)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Mark=A._Mark; Colour=A._Colour; Size=A._Size; Amount=A._Amount; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size,Size*1.2f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle&, bool) const override
	{ TallowPaper::DrawMark(O,L,G,Mark.Get(),G.GetLocalSize()*.5f,Size,Colour.Get(),Amount.Get()); return L+1; }
private:
	TAttribute<TallowPaper::EMark> Mark;
	TAttribute<FLinearColor> Colour;
	float Size=88.f;
	TAttribute<float> Amount;
};

/** The red thread drawn under whatever is selected, the width of its slot. */
class STallowThread : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STallowThread) : _Shown(true), _Seed(1) {}
		SLATE_ATTRIBUTE(bool, Shown)
		SLATE_ARGUMENT(int32, Seed)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Shown=A._Shown; Seed=A._Seed; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(24.f,8.f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle&, bool) const override
	{
		if(!Shown.Get()) return L;
		const FVector2D Size=G.GetLocalSize();
		TallowPaper::DrawThread(O,L,G,FVector2D(1.f,Size.Y*.5f),FVector2D(Size.X-1.f,Size.Y*.5f),Seed);
		return L+2;
	}
private:
	TAttribute<bool> Shown;
	int32 Seed=1;
};

/** A line of words with keys in it ("[ esc ]  back"), each key in its engraved cap. */
class STallowKeys : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STallowKeys) : _Font(TallowPaper::FellItalic(TallowPaper::Pt::Small)), _Colour(TallowPaper::Pencil), _KeyColour(TallowPaper::Ink) {}
		SLATE_ATTRIBUTE(FString, Text)
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_ATTRIBUTE(FLinearColor, Colour)
		SLATE_ATTRIBUTE(FLinearColor, KeyColour)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Text=A._Text; Font=A._Font; Colour=A._Colour; KeyColour=A._KeyColour; }
	virtual FVector2D ComputeDesiredSize(float) const override { return TallowPaper::MeasureKeys(Text.Get(), Font) + FVector2D(2.f, 4.f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle& S, bool) const override
	{
		const float Fade = S.GetColorAndOpacityTint().A;
		TallowPaper::DrawKeys(O, L, G, Text.Get(), Font, FVector2D(0.f, G.GetLocalSize().Y * .5f), Colour.Get().CopyWithNewOpacity(Colour.Get().A * Fade), KeyColour.Get().CopyWithNewOpacity(KeyColour.Get().A * Fade));
		return L + 3;
	}
private:
	TAttribute<FString> Text;
	FSlateFontInfo Font;
	TAttribute<FLinearColor> Colour, KeyColour;
};

/** Soot falling away from the left edge (or up from the bottom) across the whole slot. */
class STallowScrim : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STallowScrim) : _FromLeft(true), _Alpha(1.f) {}
		SLATE_ARGUMENT(bool, FromLeft)
		SLATE_ATTRIBUTE(float, Alpha)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { bFromLeft=A._FromLeft; Alpha=A._Alpha; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle&, bool) const override
	{
		if (Alpha.Get() > 0.f) TallowPaper::DrawScrim(O, L, G, FVector2D::ZeroVector, G.GetLocalSize(), bFromLeft, Alpha.Get());
		return L + 1;
	}
private:
	bool bFromLeft = true;
	TAttribute<float> Alpha;
};

/** The soft soot pool under words that sit over the game, spread past its slot so the words stay on the dark middle. */
class STallowSmudge : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STallowSmudge) : _Alpha(1.f) {}
		SLATE_ATTRIBUTE(float, Alpha)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Alpha=A._Alpha; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle& S, bool) const override
	{
		const FVector2D Size = G.GetLocalSize();
		TallowPaper::DrawSmudge(O, L, G, Size * .5f, Size * .62f, Alpha.Get() * S.GetColorAndOpacityTint().A);
		return L + 1;
	}
private:
	TAttribute<float> Alpha;
};
