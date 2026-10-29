#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowBloodWriting.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

/** The BloodWriting beat's words: black blood that soaks through the wall, letters forming left to right out of their
 *  own stain, then running. The words are drawn once into a mask; the decal material grows, bleeds and drips from it. */
UCLASS(NotPlaceable)
class TALLOW_API ATallowBloodWriting : public AActor
{
    GENERATED_BODY()
public:
    ATallowBloodWriting();
    virtual void Tick(float DeltaSeconds) override;
    /** Wall point and its outward normal for a story object's WritingAnchor (the anchor's own rotation is not authored). */
    static bool FindWall(const UWorld* World, const FTransform& Anchor, const AActor* Subject, FVector& OutPoint, FVector& OutNormal);
    /** Draws Text (lines split on '\n') and starts it forming after Delay seconds. WidthScale (the anchor's X scale, set
     *  by the placement script) narrows the words for a short clear wall. Returns the decal's world width. */
    float Write(const FText& Text, const FVector& WallPoint, const FVector& WallNormal, int32 Seed, float WidthScale = 1.f, float Delay = 0.f);
    float GetAge() const;
    /** True once the words have begun to form (the decal is on the wall). */
    bool HasStarted() const { return WrittenAt >= 0.0; }
    /** Seconds from the first stroke until the last letter is whole. */
    float GetFormEnd() const { return .15f + FormSeconds; }
    /** The middle of the words on the wall (the runs hang below it). */
    FVector GetWordsCentre() const { return WordsCentre; }
    /** The black blood is drawn by a shader the renderer may still be preparing the first time; until then the
     *  decal would show the engine's stand-in material as a black box on the wall. */
    bool IsBloodReady() const;
    UPROPERTY(VisibleAnywhere, Category="Beats") TObjectPtr<UDecalComponent> Decal;
    UPROPERTY(EditAnywhere, Category="Beats") float FormSeconds = 4.6f;
    UPROPERTY(EditAnywhere, Category="Beats") float DripSeconds = 16.f;
    UPROPERTY(EditAnywhere, Category="Beats") float LetterHeightCm = 24.f;
    UPROPERTY(EditAnywhere, Category="Beats") float MaxWidthCm = 240.f;
private:
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Ink;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Blood;
    UPROPERTY(Transient) TObjectPtr<class UFont> Hand;
    void DrawInk();
    TArray<FString> Lines;
    float Em = 100.f;
    int32 InkSeed = 0;
    int32 Redraws = 0;
    bool bEngineFont = false;
    double WrittenAt = -1.0;
    double StartAt = 0.0;
    FVector WordsCentre = FVector::ZeroVector;
};
