#include "Chapter/Presentation/TallowBloodWriting.h"
#include "CanvasItem.h"
#include "Components/DecalComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "UI/Presentation/TallowPaper.h"
namespace
{
    constexpr int32 InkWidth = 2048, InkHeight = 1024;
    // The letters fill the top of the mask; the rest is room for the runs.
    constexpr float TextTop = .07f, TextBottom = .6f;
}
ATallowBloodWriting::ATallowBloodWriting()
{
    PrimaryActorTick.bCanEverTick = true;
    Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Blood")); SetRootComponent(Decal);
    Decal->SetFadeScreenSize(0.f); Decal->SortOrder = 5;
}
bool ATallowBloodWriting::FindWall(const UWorld* World, const FTransform& Anchor, const AActor* Subject, FVector& OutPoint, FVector& OutNormal)
{
    const FVector At = Anchor.GetLocation(); OutPoint = At; OutNormal = FVector::ZeroVector;
    if (!World) return false;
    FCollisionQueryParams Params(TEXT("TallowBloodWall"), false, Subject);
    if (const auto* Player = UGameplayStatics::GetPlayerPawn(World, 0)) Params.AddIgnoredActor(Player);
    TArray<FVector> Ways;
    // The anchor was measured on the wall behind its story object, so look from the object first.
    if (Subject) if (const FVector Way = (At - Subject->GetActorLocation()).GetSafeNormal2D(); !Way.IsNearlyZero()) Ways.Add(Way);
    for (int32 I = 0; I < 16; ++I) Ways.Add(FRotator(0.f, I * 22.5f, 0.f).Vector());
    float Best = TNumericLimits<float>::Max();
    for (const FVector& Way : Ways)
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, At - Way * 80.f, At + Way * 40.f, ECC_Visibility, Params)) continue;
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
        if (Normal.IsNearlyZero() || FVector::DotProduct(Normal, -Way) < .7f) continue;
        const float Distance = FVector::Dist(Hit.ImpactPoint, At);
        if (Distance < Best) { Best = Distance; OutPoint = Hit.ImpactPoint; OutNormal = Normal; }
        if (&Way == &Ways[0] && Subject && Distance < 30.f) break;
    }
    return !OutNormal.IsNearlyZero();
}
float ATallowBloodWriting::Write(const FText& Text, const FVector& WallPoint, const FVector& WallNormal, int32 Seed, float WidthScale)
{
    Text.ToString().ParseIntoArray(Lines, TEXT("\n")); InkSeed = Seed;
    int32 Longest = 1; for (const FString& Line : Lines) Longest = FMath::Max(Longest, Line.Len());
    Em = FMath::Min(InkWidth * .86f / (Longest * .56f), InkHeight * (TextBottom - TextTop) / (FMath::Max(1, Lines.Num()) * 1.12f));
    Ink = UKismetRenderingLibrary::CreateRenderTarget2D(this, InkWidth, InkHeight, RTF_RGBA8_SRGB, FLinearColor::Black);
    DrawInk();
    // Only the placement script's widths (.6 to 1) count; an anchor that merely inherits its machine's scale is full width.
    const float Narrow = WidthScale >= .6f && WidthScale <= 1.f ? WidthScale : 1.f;
    const float CmPerPixel = FMath::Min(LetterHeightCm / Em, MaxWidthCm * Narrow / InkWidth);
    const float Width = InkWidth * CmPerPixel, Height = InkHeight * CmPerPixel;
    // Decal UVs run along its local Z (u) and Y (v): X into the wall, Y down the wall, so Z reads to the right.
    const FVector Into = -WallNormal.GetSafeNormal2D();
    const FRotator Facing = FRotationMatrix::MakeFromXY(Into, -FVector::UpVector).Rotator();
    const float TextCentre = (TextTop + TextBottom) * .5f;
    // The words sit a little above the anchor, over a child's eye line; the runs hang below them.
    SetActorLocationAndRotation(WallPoint + FVector(0.f, 0.f, 12.f - (.5f - TextCentre) * Height), Facing);
    Decal->DecalSize = FVector(14.f, Height * .5f, Width * .5f);
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FinalPass/Beats/M_BeatBloodWriting.M_BeatBloodWriting")))
    {
        Blood = UMaterialInstanceDynamic::Create(Material, this);
        Blood->SetTextureParameterValue(TEXT("Ink"), Ink);
        Blood->SetScalarParameterValue(TEXT("Seed"), float(Seed % 997));
        Blood->SetScalarParameterValue(TEXT("Growth"), 0.f); Blood->SetScalarParameterValue(TEXT("Drip"), 0.f);
        Decal->SetDecalMaterial(Blood);
    }
    else UE_LOG(LogTemp, Warning, TEXT("TALLOW_BEATS missing M_BeatBloodWriting"));
    Decal->MarkRenderStateDirty();
    WrittenAt = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS writing \"%s\" at %s normal %s width %.0f cm"), *Text.ToString().Replace(TEXT("\n"), TEXT(" / ")), *WallPoint.ToCompactString(), *WallNormal.ToCompactString(), Width);
    return Width;
}
void ATallowBloodWriting::DrawInk()
{
    // Without a renderer (headless runs) there is no target to draw into; the beat still runs its timeline.
    if (!Ink || !FApp::CanEverRender()) return;
    UKismetRenderingLibrary::ClearRenderTarget2D(this, Ink, FLinearColor::Black);
    if (Lines.IsEmpty()) return;
    UCanvas* Canvas = nullptr; FVector2D Size; FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Ink, Canvas, Size, Context);
    if (!Canvas) { UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context); return; }
    // Journal hand, written by something that is not used to writing: every letter its own size and height,
    // each stroke laid twice like a finger dragged through it.
    // The journal hand as a runtime font: a canvas only draws text that carries a UFont, so the bare Slate face
    // drew nothing. Cached at 96 and scaled to size.
    if (!Hand)
        if (UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/Presentation/Journal/FF_Journal.FF_Journal")))
        {
            Hand = NewObject<UFont>(this, TEXT("BloodHand"));
            Hand->FontCacheType = EFontCacheType::Runtime; Hand->LegacyFontSize = 96;
            Hand->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Face);
        }
    if (!Hand) bEngineFont = true;
    const FSlateFontInfo Font = Hand ? FSlateFontInfo(Hand, 96) : TallowPaper::Writing(96);
    const float Grow = Em * .75f / 96.f;
    FRandomStream Random(InkSeed);
    const float Block = Lines.Num() * Em * 1.12f;
    float Y = InkHeight * (TextTop + TextBottom) * .5f - Block * .5f;
    for (const FString& Line : Lines)
    {
        float X = InkWidth * .5f - (bEngineFont ? Line.Len() * Em * .58f : TallowPaper::Measure(Line, Font).X * Grow) * .5f + Random.FRandRange(-.4f, .4f) * Em;
        const float Slope = Random.FRandRange(-.05f, .05f);
        for (int32 I = 0; I < Line.Len(); ++I)
        {
            const FString Letter = Line.Mid(I, 1);
            const float Advance = bEngineFont ? Em * .58f : TallowPaper::Measure(Letter, Font).X * Grow;
            const FVector2D Scale(Random.FRandRange(.9f, 1.14f), Random.FRandRange(.9f, 1.2f));
            const FVector2D At(X, Y + Random.FRandRange(-.06f, .06f) * Em + Slope * (X - InkWidth * .5f));
            // A finger, not a pen: each stroke laid in a small ring so it is as wide as a fingertip, then dragged.
            for (const FVector2D Drag : {FVector2D(0.f, 0.f), FVector2D(.03f, 0.f), FVector2D(-.03f, 0.f), FVector2D(0.f, .03f), FVector2D(0.f, -.03f), FVector2D(.05f, .035f)})
            {
                FCanvasTextItem Item(At + Drag * Em, FText::FromString(Letter), Font, FLinearColor::White);
                Item.Font = Hand;
                if (bEngineFont) Item = FCanvasTextItem(At + Drag * Em, FText::FromString(Letter), GEngine->GetLargeFont(), FLinearColor::White);
                Item.Scale = bEngineFont ? Scale * (Em / 24.f) : Scale * Grow; Item.BlendMode = SE_BLEND_Translucent;
                Canvas->DrawItem(Item);
            }
            X += Advance * Scale.X * .97f;
        }
        Y += Em * 1.12f;
    }
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}
float ATallowBloodWriting::GetAge() const { return WrittenAt < 0.0 ? 0.f : float(GetWorld()->GetTimeSeconds() - WrittenAt); }
void ATallowBloodWriting::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Blood || WrittenAt < 0.0) return;
    const float Age = GetAge();
    // Glyphs a font has not drawn before reach its atlas a frame late: lay the words again once they are there.
    if ((Redraws == 0 && Age > .05f) || (Redraws == 1 && Age > .5f)) { ++Redraws; DrawInk(); }
    else if (Redraws == 2 && Age > .7f)
    {
        // If the hand never reached the mask, the words still go up in the engine's own face.
        ++Redraws; TArray<FColor> Samples; uint8 Peak = 0;
        if (!Ink || !FApp::CanEverRender()) return;
        if (UKismetRenderingLibrary::ReadRenderTarget(this, Ink, Samples, false)) for (const FColor& Sample : Samples) Peak = FMath::Max(Peak, Sample.R);
        if (Peak < 40) { UE_LOG(LogTemp, Warning, TEXT("TALLOW_BEATS ink empty; drawing the words in the engine font")); bEngineFont = true; DrawInk(); }
        else UE_LOG(LogTemp, Display, TEXT("TALLOW_BEATS ink drawn peak=%d"), Peak);
    }
    // Fast at first, as if it pushes through, then the last strokes finish slowly. It stays wet a long time.
    const float Form = FMath::Clamp((Age - .15f) / FormSeconds, 0.f, 1.f);
    Blood->SetScalarParameterValue(TEXT("Growth"), 1.f - FMath::Pow(1.f - Form, 2.2f));
    Blood->SetScalarParameterValue(TEXT("Drip"), FMath::Pow(FMath::Clamp((Age - 1.8f) / DripSeconds, 0.f, 1.f), .6f));
    Blood->SetScalarParameterValue(TEXT("Wet"), FMath::Lerp(1.f, .55f, FMath::Clamp(Age / 90.f, 0.f, 1.f)));
    if (Age > 95.f) SetActorTickEnabled(false);
}
