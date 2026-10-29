#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class FSlateWindowElementList;
struct FGeometry;
struct FScrollBarStyle;
struct FScrollBoxStyle;

/** Shared book typography, warm ink and native surfaces for the game interface (DESIGN.md). */
namespace TallowPaper
{
	/** The soot every page is laid on. */
	inline const FLinearColor Soot = FLinearColor::FromSRGBColor(FColor(13, 10, 8));
	/** Tallow ivory: headings, keys, the chosen item. */
	inline const FLinearColor Ivory = FLinearColor::FromSRGBColor(FColor(233, 223, 199));
	/** Body ivory: the writing itself. */
	inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(205, 195, 171));
	/** Dust: what waits unchosen, footers, the quieter line. */
	inline const FLinearColor Pencil = FLinearColor::FromSRGBColor(FColor(148, 139, 122));
	/** Red words ("no", danger): the thread's red, lifted just enough to read as type. */
	inline const FLinearColor Blood = FLinearColor::FromSRGBColor(FColor(190, 72, 56));
	inline const FLinearColor PaperInk = FLinearColor::FromSRGBColor(FColor(36, 31, 33));
	/** Ochre: group heads only. */
	inline const FLinearColor Ochre = FLinearColor::FromSRGBColor(FColor(168, 135, 79));
	/** The one red thread. It only ever means "this one". */
	inline const FLinearColor Thread = FLinearColor::FromSRGBColor(FColor(168, 32, 23));

	/** The type scale, in 1080p layout units (720p draws the same layout at two thirds). */
	namespace Pt
	{
		constexpr int32 Display = 58;   // chapter titles, slide headlines
		constexpr int32 Heading = 46;   // a page's heading
		constexpr int32 Item = 33;      // menu choices
		constexpr int32 Lead = 28;      // reading text, Nell's notes, prompts
		constexpr int32 Body = 24;      // descriptions, sub-lines
		constexpr int32 Small = 20;     // footers, key hints, status
		constexpr int32 Label = 16;     // tracked capitals
	}

	/** IM Fell English: everything the game itself says. */
	TALLOW_API FSlateFontInfo Fell(int32 Size);
	/** Its italic: sub-lines, prompts, the quieter voice. */
	TALLOW_API FSlateFontInfo FellItalic(int32 Size);
	/** Fell capitals, opened up: small labels. Pass upper-case text. */
	TALLOW_API FSlateFontInfo Caps(int32 Size, int32 LetterSpacing = 240);
	/** Nell's pencil. Only for words a child actually wrote. */
	TALLOW_API FSlateFontInfo Child(int32 Size);
	/** Older names, kept for callers: Print is Fell, Hand is Fell italic, Tracked is spaced Fell. */
	TALLOW_API FSlateFontInfo Hand(int32 Size);
	TALLOW_API FSlateFontInfo Print(int32 Size);
	TALLOW_API FSlateFontInfo Tracked(int32 Size, int32 LetterSpacing);
	/** The journal's own hand, for words written into the world (the blood writing). */
	TALLOW_API FSlateFontInfo Writing(int32 Size);
	/** Authored textures from the UI material kit, retained for Slate and cooking. */
	TALLOW_API const FSlateBrush* Art(const FName Name);
	TALLOW_API const FScrollBoxStyle* ScrollArea();
	TALLOW_API const FScrollBarStyle* ScrollBar(bool bOnPaper=false);
	enum class EMark : uint8 { Eye, Ear, Heart, Candle, Door, House, Match, Key, Breath, Crowbar, Fuse, Book, Map, Reel, Letter, Cutting, Photo, Sewing, Drawing };
	/** An engraved mark, tinted by Colour. Under 64 units the plate cut for small sizes is used. */
	TALLOW_API void DrawMark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, EMark Mark, FVector2D Centre, float Size, FLinearColor Colour, float Amount = 1.f);
	/** The mark a found paper is filed under in the pocket. */
	TALLOW_API EMark PaperMark(FName Document);
	/** The soot ground every full-screen page is laid on, tiled; bSheet gives the lighter reading sheet. */
	TALLOW_API const FSlateBrush* Ground(bool bSheet = false);
	/** The drawn key cap, nine-sliced round a native key name. */
	TALLOW_API const FSlateBrush* KeyCap();
	/** The red thread under a selection: a real line with sparse stitch ticks, steady for a given Seed. */
	TALLOW_API void DrawThread(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D From, FVector2D To, int32 Seed, float Alpha = 1.f);

	/** The torn card scrap, nine-sliced so it stretches without smearing its torn edge. */
	TALLOW_API const FSlateBrush* Scrap();

	TALLOW_API void DrawScrap(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Pos, FVector2D Size, float Alpha = .92f);
	/** The soft soot laid under words that sit over the game: no edge, no card. Size is the words' box; the soot spreads past it. */
	TALLOW_API void DrawSmudge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, FVector2D Size, float Alpha = 1.f);
	TALLOW_API const FSlateBrush* Smudge();
	/** Soot falling away from one side of a box (bFromLeft: left to right, else bottom to top), eased so no band shows. */
	TALLOW_API void DrawScrim(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Pos, FVector2D Size, bool bFromLeft, float Alpha);
	TALLOW_API void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FSlateFontInfo& Font, FVector2D Centre, FLinearColor Colour);
	TALLOW_API FVector2D Measure(const FString& Text, const FSlateFontInfo& Font);

	/** Words with keys in them: every "[ key ]" is set in an engraved key cap, the rest in Font. */
	TALLOW_API FVector2D MeasureKeys(const FString& Text, const FSlateFontInfo& Font);
	/** Draws from the left edge of the line, centred on Middle's height; returns the width used. */
	TALLOW_API float DrawKeys(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FSlateFontInfo& Font, FVector2D LeftMiddle, FLinearColor Colour, FLinearColor KeyColour);

	/** A line drawn by hand: a slight tremble along its length, steady for a given Seed so it doesn't crawl. */
	TALLOW_API void DrawLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D From, FVector2D To, FLinearColor Colour, float Thickness, int32 Seed);

	/** A ring drawn by hand, from the top clockwise to Fraction of the way round. */
	TALLOW_API void DrawRing(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, FVector2D Centre, float Radius, float Fraction, FLinearColor Colour, float Thickness, int32 Seed);
}
