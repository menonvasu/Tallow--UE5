#pragma once
#include "CoreMinimal.h"
#include "AI/TallowCreature.h"
#include "TallowHunterCreature.generated.h"

class UTallowHunterMotionComponent;
class UTallowHunterTraversalComponent;
class UTallowMimVisualScaleComponent;

/** Optional preview creature. The existing creature and StateTree stay intact. */
UCLASS()
class TALLOW_API ATallowHunterCreature : public ATallowCreature
{
    GENERATED_BODY()
public:
    void SetResetLocation(const FVector& L) { InitialSpawnTransform.SetLocation(L); }
    ATallowHunterCreature(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
    virtual void PrepareCapturePresentation() override;
    virtual float GetCapturePresentationDuration() const override;
    virtual float GetCaptureStrikeTime() const override;
    virtual float GetCaptureStagingDistance() const override;
    virtual bool IsCeilingCapturePresentation() const override;
    virtual bool UsesCanonicalCaptureBasis() const override;
    /** Uniform physical size; includes mesh, collision and the imported rig. */
    UPROPERTY(EditDefaultsOnly, Category="Hunter|Appearance", meta=(ClampMin=".8",ClampMax="1"))
    float MimBodyScale = .85f;
    /** How much larger her body is drawn than the body her movement plans for (29 Oct: "a little bigger").
     *  Collision, navigation and traversal keep MimBodyScale; she settles back to it where space is tight. */
    UPROPERTY(EditDefaultsOnly, Category="Hunter|Appearance", meta=(ClampMin="1",ClampMax="1.5"))
    float MimVisualScale = 1.25f;
    /** Her drawn size this frame: the actor scale times the visual scale. Capture framing is measured in this. */
    float GetVisibleBodyScale() const;
    /** Seconds the capture shot holds on the lens after the strike before the hard cut to black. */
    UPROPERTY(EditDefaultsOnly, Category="Hunter|Capture")
    float CaptureCutAfterStrike = .55f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter")
    TObjectPtr<UTallowHunterMotionComponent> HunterMotion;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter")
    TObjectPtr<UTallowHunterTraversalComponent> HunterTraversal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hunter")
    TObjectPtr<UTallowMimVisualScaleComponent> MimVisual;
};
