#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowMimVisualScaleComponent.generated.h"

/** Her drawn skin against the world (review only): vertices past a surface from their own bone. */
USTRUCT(BlueprintType)
struct FTallowMimSkinAudit
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Vertices = 0;
    /** Past a world surface by more than the minimum depth, counted from the vertex's heaviest bone. */
    UPROPERTY(BlueprintReadOnly) int32 Clipped = 0;
    UPROPERTY(BlueprintReadOnly) int32 ClippedFur = 0;
    /** Of those, the ones the player's camera has a clear line to. */
    UPROPERTY(BlueprintReadOnly) int32 Visible = 0;
    UPROPERTY(BlueprintReadOnly) int32 VisibleFur = 0;
    UPROPERTY(BlueprintReadOnly) float WorstDepth = 0.f;
    UPROPERTY(BlueprintReadOnly) float WorstVisibleDepth = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Against;
    UPROPERTY(BlueprintReadOnly) TArray<FVector> VisibleExamples;
};

/**
 * Mim's drawn body is bigger than the body her movement plans for (29 Oct: "too small", "a little bigger").
 * Her capsule, navigation, traversal and fit checks all keep the actor scale; only the mesh is drawn larger.
 * The extra size goes on just before her animation is evaluated (so the fit layer folds the body she is drawn at)
 * and comes off again at the start of the next frame, before anything that plans her movement reads her mesh.
 * In open space she is drawn at full size. Where the free width round her closes in (a doorway, the stairs, a
 * cluttered passage), on any climb, squeeze or pounce, and wherever her drawn mass (torso, hood, coat) would go
 * further into anything than it does at her planned size, she settles back to her planned size, which the fit
 * layer and the movement core were built and checked round. She settles in fast and swells back slowly, a second
 * after the space opens.
 */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowMimVisualScaleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTallowMimVisualScaleComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    /** The drawn body over the planned one this frame (1 = the size her movement plans for). */
    UFUNCTION(BlueprintPure, Category="Hunter|Appearance") float GetVisualScale() const { return Current; }
    /** What holds her below her full size this frame, for review logs. */
    UFUNCTION(BlueprintPure, Category="Hunter|Appearance") FString GetVisualScaleReason() const { return Reason; }
    /** The narrowest free width measured round her this frame, cm (review). */
    UFUNCTION(BlueprintPure, Category="Hunter|Appearance") float GetSpaceWidth() const { return SpaceWidth; }
    /** Free width, in rig cm (times her body scale), at or under which she is drawn at her planned size... */
    UPROPERTY(EditAnywhere, Category="Hunter|Appearance") float NarrowWidth = 260.f;
    /** ...and at or over which she is drawn at her full size. */
    UPROPERTY(EditAnywhere, Category="Hunter|Appearance") float OpenWidth = 380.f;
    /** Review only (CPU skinning, one trace per sampled vertex): her drawn skin against the world now. */
    UFUNCTION(BlueprintCallable, Category="Hunter|Review") FTallowMimSkinAudit AuditDrawnSkin(int32 VertexStride = 8, float MinDepth = 2.f) const;
private:
    /** The drawn size the space round her allows: from the free width, then checked on her mass. */
    float MeasureFit(const class ATallowHunterCreature& Creature);
    void Restore(UWorld* World, ELevelTick TickType, float DeltaSeconds);
    float Current = 1.f;
    FString Reason;
    FVector PlannedScale = FVector::OneVector, DrawnScale = FVector::OneVector;
    bool bDrawn = false;
    TWeakObjectPtr<AActor> OrderedController;
    FDelegateHandle WorldTickHandle;
    float FullClearSince = -1.f;
    float SpaceWidth = 800.f;
    FBox BodyBox = FBox(ForceInit);
    float NextBodyMeasure = 0.f;
    FVector LastLocation = FVector::ZeroVector;
};
