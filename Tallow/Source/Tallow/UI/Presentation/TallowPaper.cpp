#include "UI/Presentation/TallowPaper.h"

#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Algo/Reverse.h"

namespace TallowPaper
{
	const FScrollBoxStyle* ScrollArea()
	{
		static FScrollBoxStyle Style=[](){
			FScrollBoxStyle S=FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox");
			const FSlateBrush Blank=*FCoreStyle::Get().GetBrush("NoBrush");
			S.SetTopShadowBrush(Blank).SetBottomShadowBrush(Blank).SetLeftShadowBrush(Blank).SetRightShadowBrush(Blank);
			return S;
		}();
		return &Style;
	}
	const FScrollBarStyle* ScrollBar(bool bOnPaper)
	{
		auto Make=[](bool bPaper){
			FScrollBarStyle S=FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			const FSlateBrush Blank=*FCoreStyle::Get().GetBrush("NoBrush");
			S.SetHorizontalBackgroundImage(Blank).SetVerticalBackgroundImage(Blank)
			 .SetVerticalTopSlotImage(Blank).SetVerticalBottomSlotImage(Blank)
			 .SetHorizontalTopSlotImage(Blank).SetHorizontalBottomSlotImage(Blank);
			FSlateBrush PencilMark=*FCoreStyle::Get().GetBrush("WhiteBrush");
			PencilMark.TintColor=FSlateColor((bPaper?PaperInk:Ink).CopyWithNewOpacity(.7f));
			S.SetNormalThumbImage(PencilMark);
			PencilMark.TintColor=FSlateColor(Blood);
			S.SetHoveredThumbImage(PencilMark).SetDraggedThumbImage(PencilMark);
			return S;
		};
		static FScrollBarStyle Paper=Make(true),Card=Make(false);
		return bOnPaper?&Paper:&Card;
	}
	FSlateFontInfo Writing(int32 Size)
	{
		// Built once from the journal's font face and kept alive for the whole run.
		static TSharedPtr<FCompositeFont> Composite;
		if (!Composite)
			if (UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/Presentation/Journal/FF_Journal.FF_Journal")))
			{
				Face->AddToRoot();
				Composite = MakeShared<FCompositeFont>();
				Composite->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Face);
			}
		return Composite ? FSlateFontInfo(Composite, Size) : FCoreStyle::GetDefaultFontStyle("Italic", Size);
	}

	namespace
	{
		enum class EFace : uint8 { Fell, FellItalic, Child, Count };

		/** One composite per face, built from the first font face found and kept for the whole run.
		 *  The FinalPass faces come first; the older Fell import stands in if they were never imported. */
		FSlateFontInfo Face(EFace Which, int32 Size)
		{
			static TSharedPtr<FCompositeFont> Faces[int32(EFace::Count)];
			static double TriedAt[int32(EFace::Count)] = {};
			static const TCHAR* Paths[int32(EFace::Count)][2] = {
				{TEXT("/Game/FinalPass/UI/Fonts/FF_Fell.FF_Fell"), TEXT("/Game/Presentation/Journal/FF_Print.FF_Print")},
				{TEXT("/Game/FinalPass/UI/Fonts/FF_FellItalic.FF_FellItalic"), nullptr},
				{TEXT("/Game/FinalPass/UI/Fonts/FF_NellHand.FF_NellHand"), nullptr}};
			const int32 I = int32(Which);
			// A missing face is looked for again at most once a second, not every frame.
			if (!Faces[I] && FPlatformTime::Seconds() - TriedAt[I] > 1.0)
			{
				TriedAt[I] = FPlatformTime::Seconds();
				for (const TCHAR* Path : Paths[I])
					if (UFontFace* Found = Path ? LoadObject<UFontFace>(nullptr, Path, nullptr, LOAD_NoWarn) : nullptr)
					{
						Found->AddToRoot();
						Faces[I] = MakeShared<FCompositeFont>();
						Faces[I]->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Found);
						break;
					}
			}
			if (Faces[I]) return FSlateFontInfo(Faces[I], Size);
			// Italic and the child's hand fall back to upright Fell rather than to the engine's face.
			return Which == EFace::Fell ? FCoreStyle::GetDefaultFontStyle("Regular", Size) : Face(EFace::Fell, Size);
		}
	}

	FSlateFontInfo Fell(int32 Size) { return Face(EFace::Fell, Size); }
	FSlateFontInfo FellItalic(int32 Size) { return Face(EFace::FellItalic, Size); }
	FSlateFontInfo Child(int32 Size) { return Face(EFace::Child, Size); }
	FSlateFontInfo Caps(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = Fell(Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	FSlateFontInfo Hand(int32 Size)
	{
		// The interface's quieter voice is the book face's italic; a child's own words use Child().
		return FellItalic(Size);
	}

	namespace
	{
		/** The painted soot card, or null until the engraved set is imported. */
		const FSlateBrush* PaintedCard()
		{
			static FSlateBrush Brush;
			static bool bReady = false;
			if (!bReady)
				if (UTexture2D* Card = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Presentation/UI/Engraved/T_Card.T_Card"), nullptr, LOAD_NoWarn))
				{
					// Its dry-brush edge lives in the outer 22 px of 512 x 256; drawn at half size
					// so a small note keeps most of its body.
					Card->AddToRoot();
					Brush.SetResourceObject(Card);
					Brush.ImageSize = FVector2D(256.f, 128.f);
					Brush.DrawAs = ESlateBrushDrawType::Box;
					Brush.Margin = FMargin(.075f, .15f);
					bReady = true;
				}
			return bReady ? &Brush : nullptr;
		}
	}

	const FSlateBrush* Scrap()
	{
		if (const FSlateBrush* Card = PaintedCard()) return Card;
		static FSlateBrush Brush;
		static bool bReady = false;
		if (!bReady)
			if (UTexture2D* Card = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Presentation/UI/Paper/T_UI_CardScrap.T_UI_CardScrap")))
			{
				Card->AddToRoot();
				Brush.SetResourceObject(Card);
				Brush.ImageSize = FVector2D(512.f, 256.f);
				Brush.DrawAs = ESlateBrushDrawType::Box;
				Brush.Margin = FMargin(.11f, .22f);   // the torn edge lives in the outer 48 px of 512 x 256
				bReady = true;
			}
		return bReady ? &Brush : FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	void DrawScrap(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Pos, FVector2D Size, float Alpha)
	{
		const FSlateBrush* Card = Art(TEXT("Charcoal"));
		// The painted card carries its own soot; the native fallback is tinted to it.
		const FLinearColor Tint = Card->GetResourceObject() ? FLinearColor(1.f, 1.f, 1.f, Alpha) : FLinearColor(.008f, .007f, .006f, Alpha);
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Card, ESlateDrawEffect::None, Tint);
	}

	const FSlateBrush* Ground(bool bSheet)
	{
		auto Make = [](const TCHAR* Path, FLinearColor Fallback)
		{
			FSlateBrush B = *FCoreStyle::Get().GetBrush("WhiteBrush");
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path, nullptr, LOAD_NoWarn))
			{
				Texture->AddToRoot();
				B.SetResourceObject(Texture);
				B.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				B.DrawAs = ESlateBrushDrawType::Image;
				B.Tiling = ESlateBrushTileType::Both;
				B.TintColor = FSlateColor(FLinearColor::White);
			}
			else B.TintColor = FSlateColor(Fallback);
			return B;
		};
		static const FSlateBrush SootGround = Make(TEXT("/Game/Presentation/UI/Engraved/T_Soot.T_Soot"), FLinearColor(.008f, .007f, .006f));
		static const FSlateBrush Sheet = Make(TEXT("/Game/Presentation/UI/Engraved/T_Sheet.T_Sheet"), FLinearColor::FromSRGBColor(FColor(35, 32, 28)));
		return bSheet ? &Sheet : &SootGround;
	}

	const FSlateBrush* KeyCap()
	{
		static const FSlateBrush Cap = []()
		{
			FSlateBrush B = *FCoreStyle::Get().GetBrush("NoBrush");
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Presentation/UI/Engraved/T_Key.T_Key"), nullptr, LOAD_NoWarn))
			{
				Texture->AddToRoot();
				B.SetResourceObject(Texture);
				B.ImageSize = FVector2D(40.f, 40.f);    // corners stay 13 units whatever the cap's width
				B.DrawAs = ESlateBrushDrawType::Box;
				B.Margin = FMargin(.33f);
				B.TintColor = FSlateColor(FLinearColor::White);
			}
			return B;
		}();
		return &Cap;
	}

	void DrawThread(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D From, FVector2D To, int32 Seed, float Alpha)
	{
		// A real 3 px line, not beads: the thread wanders a little and is caught down by a stitch now and then.
		DrawLine(Out, Layer, Geo, From, To, Thread.CopyWithNewOpacity(Alpha), 3.f, Seed);
		const FVector2D Along = To - From;
		const float Length = Along.Size();
		if (Length < 24.f) return;
		const FVector2D Dir = Along / Length, Normal(-Dir.Y, Dir.X);
		const FLinearColor Stitch = FLinearColor::LerpUsingHSV(Thread, Ink, .18f).CopyWithNewOpacity(.9f * Alpha);
		for (float D = 14.f + FMath::Abs(FMath::PerlinNoise1D(Seed * .37f)) * 10.f; D < Length - 8.f; D += 46.f + FMath::PerlinNoise1D(Seed + D * .05f) * 12.f)
		{
			const FVector2D At = From + Dir * D;
			const TArray<FVector2D> Tick{At - Normal * 3.5f - Dir * 1.5f, At + Normal * 3.5f + Dir * 1.5f};
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Tick, ESlateDrawEffect::None, Stitch, true, 1.4f);
		}
	}

	const FSlateBrush* Smudge()
	{
		static const FSlateBrush Brush = []()
		{
			FSlateBrush B = *FCoreStyle::Get().GetBrush("NoBrush");
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FinalPass/UI/T_Smudge.T_Smudge"), nullptr, LOAD_NoWarn))
			{
				Texture->AddToRoot();
				B.SetResourceObject(Texture);
				B.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				B.DrawAs = ESlateBrushDrawType::Image;
				B.TintColor = FSlateColor(FLinearColor::White);
			}
			return B;
		}();
		return &Brush;
	}

	void DrawSmudge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, FVector2D Size, float Alpha)
	{
		// The pool's solid middle is about the inner half of the texture: spread it just past the words,
		// so it holds them over a lit wall without reading as a stain round them.
		const FVector2D Spread(Size.X * 1.3f + 64.f, Size.Y * 1.8f + 36.f);
		const FSlateBrush* Brush = Smudge();
		if (Brush->GetResourceObject())
			FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Spread, FSlateLayoutTransform(Centre - Spread * .5f)), Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Alpha));
		else
			DrawScrim(Out, Layer, Geo, Centre - Size * .5f - FVector2D(24.f, 12.f), Size + FVector2D(48.f, 24.f), false, .55f * Alpha);
	}

	void DrawScrim(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Pos, FVector2D Size, bool bFromLeft, float Alpha)
	{
		// Smoothstep in seven stops: dense soot at the edge, gone before the far side, no visible band.
		TArray<FSlateGradientStop> Stops;
		const float Length = bFromLeft ? Size.X : Size.Y;
		for (int32 I = 0; I <= 6; ++I)
		{
			const float T = I / 6.f;
			const float Fall = 1.f - T * T * (3.f - 2.f * T);
			const float At = bFromLeft ? T * Length : (1.f - T) * Length;
			Stops.Add(FSlateGradientStop(bFromLeft ? FVector2D(At, 0.f) : FVector2D(0.f, At), Soot.CopyWithNewOpacity(Alpha * Fall)));
		}
		if (!bFromLeft) Algo::Reverse(Stops);
		FSlateDrawElement::MakeGradient(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Stops, bFromLeft ? Orient_Vertical : Orient_Horizontal);
	}

	namespace
	{
		/** "[ R ]  strike a match" -> ("R", true), ("strike a match", false). */
		void SplitKeys(const FString& Text, TArray<TPair<FString, bool>>& Parts)
		{
			int32 At = 0;
			while (At < Text.Len())
			{
				const int32 Open = Text.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
				const int32 Close = Open == INDEX_NONE ? INDEX_NONE : Text.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
				if (Open == INDEX_NONE || Close == INDEX_NONE) { Parts.Add({Text.Mid(At), false}); return; }
				if (Open > At) Parts.Add({Text.Mid(At, Open - At), false});
				Parts.Add({Text.Mid(Open + 1, Close - Open - 1).TrimStartAndEnd(), true});
				At = Close + 1;
			}
		}
		FSlateFontInfo KeyFont(const FSlateFontInfo& Font) { return Fell(FMath::Max(10, FMath::RoundToInt(Font.Size * .82f))); }

		/** One DualSense glyph in a key line: the engraved texture, and a native label over its cap where the cap is blank. */
		struct FPadGlyph { FName Art; FString Label; FVector2D Size; };
		bool PadGlyphFor(const FString& Word, float CapH, FPadGlyph& Out)
		{
			static const TMap<FString, TPair<FName, FString>> Table = {
				{TEXT("cross"), {TEXT("PadCross"), FString()}}, {TEXT("circle"), {TEXT("PadCircle"), FString()}},
				{TEXT("square"), {TEXT("PadSquare"), FString()}}, {TEXT("triangle"), {TEXT("PadTriangle"), FString()}},
				{TEXT("d-pad up"), {TEXT("PadUp"), FString()}}, {TEXT("d-pad down"), {TEXT("PadDown"), FString()}},
				{TEXT("d-pad left"), {TEXT("PadLeft"), FString()}}, {TEXT("d-pad right"), {TEXT("PadRight"), FString()}},
				{TEXT("d-pad"), {TEXT("PadDPad"), FString()}},
				{TEXT("options"), {TEXT("PadOptions"), FString()}}, {TEXT("create"), {TEXT("PadCreate"), FString()}},
				{TEXT("left stick"), {TEXT("PadStick"), TEXT("L")}}, {TEXT("right stick"), {TEXT("PadStick"), TEXT("R")}},
				{TEXT("l3"), {TEXT("PadStick"), TEXT("L3")}}, {TEXT("r3"), {TEXT("PadStick"), TEXT("R3")}},
				{TEXT("l1"), {TEXT("PadBumper"), TEXT("L1")}}, {TEXT("r1"), {TEXT("PadBumper"), TEXT("R1")}},
				{TEXT("l2"), {TEXT("PadTrigger"), TEXT("L2")}}, {TEXT("r2"), {TEXT("PadTrigger"), TEXT("R2")}}};
			const TPair<FName, FString>* Found = Table.Find(Word.TrimStartAndEnd().ToLower());
			if (!Found) return false;
			const FSlateBrush* Brush = Art(Found->Key);
			if (!Brush || Brush == Scrap() || !Brush->GetResourceObject()) return false;   // glyph not imported: the word cap stays
			const FVector2D Raw = Brush->GetImageSize();
			const float H = CapH * 1.1f;
			Out = {Found->Key, Found->Value, FVector2D(H * Raw.X / FMath::Max(1.f, float(Raw.Y)), H)};
			return true;
		}
		/** "cross", or "cross / create": every part a glyph, or none (then it is a word in a cap). */
		bool PadGlyphs(const FString& Key, const FSlateFontInfo& Font, TArray<FPadGlyph>& Glyphs)
		{
			TArray<FString> Words;
			Key.ParseIntoArray(Words, TEXT("/"));
			for (const FString& Word : Words)
			{
				FPadGlyph Glyph;
				if (!PadGlyphFor(Word, FMath::RoundToFloat(Font.Size * 1.45f), Glyph)) { Glyphs.Reset(); return false; }
				Glyphs.Add(Glyph);
			}
			return Glyphs.Num() > 0;
		}
		FVector2D CapSize(const FString& Key, const FSlateFontInfo& Font)
		{
			TArray<FPadGlyph> Glyphs;
			if (PadGlyphs(Key, Font, Glyphs))
			{
				FVector2D Size(0.f, 0.f);
				for (const FPadGlyph& Glyph : Glyphs) { Size.X += Glyph.Size.X + Font.Size * .2f; Size.Y = FMath::Max(Size.Y, Glyph.Size.Y); }
				return FVector2D(Size.X - Font.Size * .2f, FMath::RoundToFloat(Font.Size * 1.45f));
			}
			const FVector2D Words = Measure(Key, KeyFont(Font));
			const float H = FMath::RoundToFloat(Font.Size * 1.45f);
			return FVector2D(FMath::Max(H, Words.X + Font.Size * .9f), H);
		}
	}

	FVector2D MeasureKeys(const FString& Text, const FSlateFontInfo& Font)
	{
		TArray<TPair<FString, bool>> Parts;
		SplitKeys(Text, Parts);
		FVector2D Size(0.f, 0.f);
		for (const auto& Part : Parts)
		{
			const FVector2D Piece = Part.Value ? CapSize(Part.Key, Font) + FVector2D(Font.Size * .2f, 0.f) : Measure(Part.Key, Font);
			Size.X += Piece.X; Size.Y = FMath::Max(Size.Y, Piece.Y);
		}
		return Size;
	}

	float DrawKeys(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FSlateFontInfo& Font, FVector2D LeftMiddle, FLinearColor Colour, FLinearColor KeyColour)
	{
		TArray<TPair<FString, bool>> Parts;
		SplitKeys(Text, Parts);
		float X = LeftMiddle.X;
		for (const auto& Part : Parts)
		{
			if (Part.Value)
			{
				// The key in its engraved cap, a hair's gap either side.
				const FVector2D Cap = CapSize(Part.Key, Font);
				const FVector2D At(X + Font.Size * .1f, LeftMiddle.Y - Cap.Y * .5f);
				TArray<FPadGlyph> Glyphs;
				if (PadGlyphs(Part.Key, Font, Glyphs))
				{
					// A pad button: its engraved glyph on the key's baseline, no cap, a native label where the glyph is blank.
					float GlyphX = At.X;
					for (const FPadGlyph& Glyph : Glyphs)
					{
						const FSlateBrush* Brush = Art(Glyph.Art);
						const FVector2D Top(GlyphX, LeftMiddle.Y - Glyph.Size.Y * .5f);
						FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Glyph.Size, FSlateLayoutTransform(Top)), Brush, ESlateDrawEffect::None, KeyColour.CopyWithNewOpacity(KeyColour.A * .95f));
						if (!Glyph.Label.IsEmpty())
						{
							const float Lift = Glyph.Art == TEXT("PadBumper") ? .06f : Glyph.Art == TEXT("PadTrigger") ? .08f : 0.f;
							DrawText(Out, Layer + 1, Geo, Glyph.Label, Fell(FMath::Max(9, FMath::RoundToInt(Font.Size * (Glyph.Label.Len() > 1 ? .66f : .72f)))), Top + Glyph.Size * FVector2D(.5f, .5f + Lift), KeyColour);
						}
						GlyphX += Glyph.Size.X + Font.Size * .2f;
					}
					X += Cap.X + Font.Size * .2f;
					continue;
				}
				FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Cap, FSlateLayoutTransform(At)), KeyCap(), ESlateDrawEffect::None, KeyColour.CopyWithNewOpacity(KeyColour.A * .9f));
				DrawText(Out, Layer + 1, Geo, Part.Key, KeyFont(Font), At + Cap * .5f + FVector2D(0.f, -1.f), KeyColour);
				X += Cap.X + Font.Size * .2f;
			}
			else if (!Part.Key.IsEmpty())
			{
				const FVector2D Words = Measure(Part.Key, Font);
				DrawText(Out, Layer + 1, Geo, Part.Key, Font, FVector2D(X + Words.X * .5f, LeftMiddle.Y), Colour);
				X += Words.X;
			}
		}
		return X - LeftMiddle.X;
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font)
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FSlateFontInfo& Font, FVector2D Centre, FLinearColor Colour)
	{
		const FVector2D Size = Measure(Text, Font);
		// A soft dark under-stroke so the ink holds up over a lit wall as well as the card.
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Centre - Size * .5f + FVector2D(.65f, .8f))), Text, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, .7f * Colour.A));
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Centre - Size * .5f)), Text, Font, ESlateDrawEffect::None, Colour);
	}

	namespace
	{
		float Tremble(int32 Seed, float T) { return FMath::PerlinNoise1D(Seed * 7.31f + T * 5.f) * .6f + FMath::PerlinNoise1D(Seed * 3.1f + T * 17.f) * .25f; }
	}

	void DrawLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D From, FVector2D To, FLinearColor Colour, float Thickness, int32 Seed)
	{
		const FVector2D Along = To - From;
		const FVector2D Normal = FVector2D(-Along.Y, Along.X).GetSafeNormal();
		const int32 Steps = FMath::Clamp(int32(Along.Size() / 12.f), 4, 64);
		TArray<FVector2D> Points;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = float(I) / Steps;
			Points.Add(From + Along * T + Normal * Tremble(Seed, T) * 1.15f);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
	}

	void DrawRing(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, float Radius, float Fraction, FLinearColor Colour, float Thickness, int32 Seed)
	{
		Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		if (Fraction <= 0.f) return;
		const int32 Steps = FMath::Max(3, int32(48 * Fraction));
		TArray<FVector2D> Points;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float T = Fraction * I / Steps;
			const float Angle = -PI * .5f + T * 2.f * PI;
			const float R = Radius * (1.f + .045f * Tremble(Seed, T));
			Points.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Thickness);
	}
}

namespace TallowPaper
{
	FSlateFontInfo Print(int32 Size) { return Fell(Size); }
	FSlateFontInfo Tracked(int32 Size, int32 LetterSpacing) { return Caps(Size, LetterSpacing); }
	const FSlateBrush* Art(const FName Name)
	{
		// Shared functional surfaces are the painted soot card, or native Slate
		// brushes until it is imported. Marks come from the engraved set.
		if (Name == TEXT("Charcoal"))
			if (const FSlateBrush* Card = PaintedCard()) return Card;
		if (Name == TEXT("Charcoal") || Name == TEXT("Parchment"))
		{
			static const FSlateBrush Dark=[](){FSlateBrush B=*FCoreStyle::Get().GetBrush("WhiteBrush");B.TintColor=FSlateColor(FLinearColor(.008f,.007f,.006f,.96f));return B;}();
			static const FSlateBrush Paper=[](){FSlateBrush B=*FCoreStyle::Get().GetBrush("WhiteBrush");B.TintColor=FSlateColor(FLinearColor::FromSRGBColor(FColor(211,203,180)));return B;}();
			return Name == TEXT("Charcoal") ? &Dark : &Paper;
		}
		static TMap<FName, TSharedPtr<FSlateBrush>> Brushes;
		static TSet<FName> Missing;
		if (const auto* Found = Brushes.Find(Name)) return Found->Get();
		if (Missing.Contains(Name)) return Scrap();
		const FString NewPath = FString::Printf(TEXT("/Game/Presentation/ArtDirection09/UI/T_%s.T_%s"), *Name.ToString(), *Name.ToString());
		const FString LegacyPath = FString::Printf(TEXT("/Game/Presentation/UI/Ink/T_%s.T_%s"), *Name.ToString(), *Name.ToString());
		const FString EngravedPath = FString::Printf(TEXT("/Game/Presentation/UI/Engraved/T_%s.T_%s"), *Name.ToString(), *Name.ToString());
		// The engraved set is the interface's one hand; older sets only fill what it never drew.
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *EngravedPath, nullptr, LOAD_NoWarn);
		if (!Texture && (Name.ToString().StartsWith(TEXT("Plan")) || Name == TEXT("Map") || Name == TEXT("Crowbar")))
		{
			const FString MapPath=FString::Printf(TEXT("/Game/Presentation/UIRebuild10/T_%s.T_%s"),*Name.ToString(),*Name.ToString());
			Texture=LoadObject<UTexture2D>(nullptr,*MapPath,nullptr,LOAD_NoWarn);
		}
		if (!Texture) Texture = LoadObject<UTexture2D>(nullptr, *LegacyPath, nullptr, LOAD_NoWarn);
		if (!Texture) Texture = LoadObject<UTexture2D>(nullptr, *NewPath, nullptr, LOAD_NoWarn);
		if (Texture)
		{
			Texture->AddToRoot(); auto Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Texture); Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Brush->DrawAs = ESlateBrushDrawType::Image; Brush->ImageType=ESlateBrushImageType::FullColor;
			if (Name == TEXT("Parchment") || Name == TEXT("Charcoal") || Name == TEXT("Selection"))
			{ Brush->DrawAs = ESlateBrushDrawType::Box; Brush->Margin = Name==TEXT("Charcoal")?FMargin(.018f,.036f,.018f,.036f):FMargin(.035f); }
			Brushes.Add(Name, Brush); return &Brush.Get();
		}
		Missing.Add(Name);
		return Scrap();
	}
	EMark PaperMark(FName Document)
	{
		if (Document == TEXT("nan") || Document == TEXT("letter")) return EMark::Letter;
		if (Document == TEXT("bill") || Document == TEXT("newspaper")) return EMark::Cutting;
		if (Document == TEXT("portrait")) return EMark::Photo;
		if (Document == TEXT("ruth")) return EMark::Sewing;
		if (Document == TEXT("drawings") || Document == TEXT("chart")) return EMark::Drawing;
		if (Document == TEXT("labels")) return EMark::Candle;
		return EMark::Book;
	}

	void DrawMark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, EMark Mark, FVector2D Centre, float Size, FLinearColor Colour, float Amount)
	{
		const TCHAR* Sprite=nullptr;
		switch (Mark)
		{
		case EMark::Eye: Sprite=TEXT("Eye"); break;
		case EMark::Ear: Sprite=TEXT("Ear"); break;
		case EMark::Heart: Sprite=TEXT("Heart"); break;
		case EMark::Candle: Sprite=Amount>0?TEXT("Candle"):TEXT("CandleOut"); break;
		case EMark::Door: Sprite=TEXT("ChapterDoor"); break;
		case EMark::House: Sprite=TEXT("House"); break;
		case EMark::Match: Sprite=TEXT("Matches"); break;
		case EMark::Key: Sprite=TEXT("FrontKey"); break;
		case EMark::Breath: Sprite=TEXT("Breath"); break;
		case EMark::Crowbar: Sprite=TEXT("Crowbar"); break;
		case EMark::Fuse: Sprite=TEXT("Fuse"); break;
		case EMark::Book: Sprite=TEXT("Book"); break;
		case EMark::Map: Sprite=TEXT("Map"); break;
		case EMark::Reel: Sprite=TEXT("Reel"); break;
		case EMark::Letter: Sprite=TEXT("Letter"); break;
		case EMark::Cutting: Sprite=TEXT("Cutting"); break;
		case EMark::Photo: Sprite=TEXT("Photo"); break;
		case EMark::Sewing: Sprite=TEXT("Sewing"); break;
		case EMark::Drawing: Sprite=TEXT("Drawing"); break;
		}
		if(Sprite)
		{
			// Small marks use the plate cut for small sizes: fewer, bolder lines that hold at 720p.
			const FSlateBrush* Brush=nullptr;
			if(Size<64.f)
			{
				const FName Small(*(FString(Sprite)+TEXT("Small")));
				const FSlateBrush* Found=Art(Small);
				if(Found && Found!=Scrap() && Found->GetResourceObject()) Brush=Found;
			}
			if(!Brush) Brush=Art(Sprite);
			if(Brush && Brush!=Scrap() && Brush->GetResourceObject())
			{
				const FVector2D Raw=Brush->GetImageSize(); const float Scale=Size/FMath::Max(float(Raw.X),float(Raw.Y)); const FVector2D Bounds=Raw*Scale;
				FSlateDrawElement::MakeBox(Out,Layer,Geo.ToPaintGeometry(Bounds,FSlateLayoutTransform(Centre-Bounds*.5)),Brush,ESlateDrawEffect::None,Colour);
				return;
			}
		}

		const float S = Size / 100.f;
		auto P = [&](float X, float Y) { return Centre + FVector2D(X, Y) * S; };
		auto Line = [&](float X, float Y, float A, float B, int32 Seed = 1) { DrawLine(Out, Layer, Geo, P(X,Y), P(A,B), Colour, FMath::Max(.8f, S * 1.7f), Seed); };
		auto Ring = [&](float X, float Y, float R) { DrawRing(Out, Layer, Geo, P(X,Y), R*S, 1.f, Colour, FMath::Max(.8f, S*1.5f), 3); };
		auto Path = [&](std::initializer_list<FVector2D> Points)
		{
			FVector2D Last; bool First = true; int32 Seed = 30;
			for (const FVector2D V : Points) { if (!First) DrawLine(Out, Layer, Geo, Centre+Last*S, Centre+V*S, Colour, FMath::Max(.8f,S*1.7f), Seed++); Last=V; First=false; }
		};
		switch (Mark)
		{
		case EMark::Eye:
			Path({{-46,0},{-26,-18},{0,-24},{26,-16},{46,0},{24,18},{0,23},{-24,17},{-46,0}});
			Ring(0,0,17); Ring(0,0,7); Line(-7,-10,5,11); Line(7,-9,-4,12);
			for(int32 I=0;I<7;++I) { const float X=-36+I*12; Line(X,-16-FMath::Sin(I*.5f)*6,X-9,-34-FMath::Sin(float(I))*7,I); }
			Line(-20,18,-17,37); Line(2,24,5,44); Line(28,15,31,32); break;
		case EMark::Ear:
			Path({{-20,32},{-5,43},{9,34},{12,18},{31,-5},{31,-24},{19,-39},{-3,-42},{-22,-29},{-25,-11}});
			Path({{-12,-12},{-9,-26},{5,-30},{18,-21},{17,-9},{2,3},{5,13},{-6,22}});
			Line(41,-17,47,-29); Line(44,0,57,-5); Line(41,17,53,23); break;
		case EMark::Heart:
			Path({{0,38},{-34,7},{-39,-12},{-28,-29},{-11,-30},{0,-16},{14,-32},{32,-28},{40,-12},{33,10},{0,38}});
			for(int32 I=0;I<15;++I) { float X=-30+I*4; Line(X,10-FMath::Abs(X)*.6f,X+18,-18-FMath::Abs(X)*.22f,60+I); } break;
		case EMark::Candle:
			Path({{-14,-6},{12,-9},{14,38},{-14,38},{-14,-6}}); Line(-9,1,-8,21); Line(7,0,9,12); Line(0,-8,0,-19);
			if(Amount>0.f) Path({{0,-18},{-9,-28},{-6,-41},{1,-57},{7,-40},{10,-28},{0,-18}});
			Path({{-24,35},{-32,42},{-20,48},{22,48},{33,41},{21,35}}); break;
		case EMark::Door:
			Path({{-23,36},{-23,-36},{14,-48},{14,29},{-23,36},{28,40},{28,-39},{14,-48}}); Ring(5,0,2);
			Path({{-23,36},{-44,43},{-44,53},{-61,57}}); break;
		case EMark::House:
			Path({{-33,40},{-33,-21},{-44,-17},{1,-62},{44,-15},{31,-20},{31,40},{-33,40}});
			Path({{-8,39},{-8,11},{9,11},{9,39}}); Path({{-23,-11},{-11,-14},{-11,0},{-23,1},{-23,-11}});
			Path({{10,-15},{23,-12},{23,1},{10,-1},{10,-15}}); Line(17,-44,17,-66);Line(24,-38,24,-66); break;
		case EMark::Match: Line(-20,36,19,-31); Ring(22,-36,5); Line(-16,38,23,-28); break;
		case EMark::Key: Ring(-20,20,16);Ring(-20,20,9);Line(-9,8,31,-37);Line(-3,12,36,-32);Line(21,-23,31,-15);Line(30,-33,41,-24);break;
		case EMark::Crowbar:
			Path({{-25,42},{20,-30},{27,-36},{36,-32},{39,-23},{33,-17},{24,-18},{-17,47}}); Line(-23,41,-16,44); break;
		case EMark::Reel:
			Ring(0,0,38); Ring(0,0,10);
			for(int32 I=0;I<5;++I){const float A=I*2.f*PI/5.f;Line(FMath::Cos(A)*15.f,FMath::Sin(A)*15.f,FMath::Cos(A)*32.f,FMath::Sin(A)*32.f,I);} break;
		case EMark::Fuse:
			Path({{-16,-34},{15,-34},{15,35},{-16,35},{-16,-34}}); Line(-19,-34,18,-34); Line(-19,-23,18,-23); Line(-19,24,18,24); Line(-19,35,18,35);
			Path({{0,-23},{-6,-12},{5,-1},{-5,10},{0,24}}); break;
		case EMark::Book:
			Path({{0,-30},{-36,-38},{-38,30},{0,38},{36,29},{37,-39},{0,-30},{0,38}});
			for(int32 I=0;I<4;++I){Line(-29,-20+I*12,-8,-17+I*12,I);Line(8,-17+I*12,28,-21+I*12,I+10);} break;
		case EMark::Breath:
			for(int32 I=0;I<3;++I) Path({{-35.f,-20.f+I*20},{-18.f,-25.f+I*20},{5.f,-16.f+I*20},{27.f,-23.f+I*20}}); break;
		default: break;
		}
	}
}
