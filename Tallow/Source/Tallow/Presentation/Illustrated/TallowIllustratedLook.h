#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowIllustratedLook.generated.h"

class UPostProcessComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ETallowIllustratedPreset : uint8 { Off, Subtle, Illustrated, Expressionist };

USTRUCT(BlueprintType)
struct FTallowIllustratedProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float Abstraction=.75f;
    /** Brush footprint in pixels at 1080p; it scales with the viewport. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=".5",ClampMax="12")) float Radius=3.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float Anisotropy=.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax=".4")) float Contour=.06f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float ToneStrength=.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="2",ClampMax="12")) float ToneSteps=5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax=".08")) float SurfaceVariation=.025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=".75",ClampMax="1.1")) float Saturation=1.f;
};

/** Visual-only pipeline. It never writes candle, perception, navigation or narrative state. */
UCLASS()
class TALLOW_API ATallowIllustratedLook : public AActor
{
    GENERATED_BODY()
public:
    ATallowIllustratedLook();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPostProcessComponent> PostProcess;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated") ETallowIllustratedPreset Preset=ETallowIllustratedPreset::Illustrated;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated") FTallowIllustratedProfile Subtle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated") FTallowIllustratedProfile Illustrated;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated") FTallowIllustratedProfile Expressionist;
    /** X out, Y cupped, Z exposed. Lower abstraction preserves faint shadow information. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Candle") FVector CandleAbstraction=FVector(.55,.8,1.);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Candle") FVector CandleContours=FVector(.45,.7,1.);
    /** X out, Y cupped, Z exposed. Faint impression of nearby form the dark-adapted eye keeps. Seen only, never lit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Candle") FVector CandleDarkRead=FVector(.025,.01,.003);
    /** X out, Y cupped, Z exposed. Exposure offset in EV; it eases in like an eye adjusting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Candle") FVector CandleExposure=FVector(0.,0.,0.);
    /** Distance in cm at which the dark read has halved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Candle", meta=(ClampMin="50")) float DarkReachCm=320.f;
    /** Fixed manual exposure for every gameplay camera; no adaptation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Illustrated|Grade") float ExposureBias=-1.2f;
    UFUNCTION(BlueprintCallable, Category="Illustrated") void SetPreset(ETallowIllustratedPreset Value);
    UFUNCTION(BlueprintPure, Category="Illustrated|Review") float GetGPUFrameMilliseconds() const;
private:
    void ApplyGradeSettings();
    void ProtectSubjects();
    void ReleaseSubjects();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Painter;
    struct FStencilLease { TWeakObjectPtr<UPrimitiveComponent> Mesh; bool Rendered=false; int32 Value=0; int32 Applied=0; };
    TArray<FStencilLease> Protected;
    float RefreshAt=0.f;
    float StateAbstraction=1.f, StateContour=1.f, StateDark=0.f, StateExposure=0.f;
};
