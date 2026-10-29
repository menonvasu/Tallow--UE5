// TALLOW post-process look. One unbound post-process component carries the fixed
// grade (manual exposure, white balance, three-way colour, bloom, grain) and the
// painterly blendables: a half-resolution structure tensor and the resolve that
// paints brush masses, world-fixed pigment, broken tonal steps, contours and the
// dark-adapted read. Candle state eases the abstraction, contours, dark read and
// exposure. Sources: Tools/FinalPass/look (painterly.hlsl, build_materials.py).
#include "Presentation/Illustrated/TallowIllustratedLook.h"
#include "Components/PostProcessComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/TallowCharacter.h"
#include "Candle/TallowCandleComponent.h"
#include "AI/TallowCreature.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GPUProfiler.h"

static TAutoConsoleVariable<int32> CVarIllustratedPreset(TEXT("Tallow.Illustrated.Preset"), -1,
    TEXT("-1 editor profile; 0 off; 1 subtle; 2 illustrated; 3 expressionist."));

namespace
{
// Measured against visual-plan/refs/hero.png: most of the frame sits in warm
// near-black, only flames reach white, and nothing falls to a dead #000.
constexpr float BloomIntensity = .6f;             // a warm halo on flames and the lit hand only
constexpr float BloomThreshold = .9f;
constexpr float GrainIntensity = .2f;             // strongest in the shadow mass
constexpr float BlackLift = -.0025f;              // FilmBlackClip below zero lifts black to a warm #0B0909
}

ATallowIllustratedLook::ATallowIllustratedLook()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .033f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Illustrated"));
    PostProcess->SetupAttachment(RootComponent); PostProcess->bUnbound = true; PostProcess->Priority = 80.f;
    Subtle.Abstraction = .5f; Subtle.Radius = 2.5f; Subtle.Anisotropy = .5f;
    Subtle.Contour = .03f; Subtle.ToneStrength = .1f; Subtle.ToneSteps = 6.f; Subtle.SurfaceVariation = .015f; Subtle.Saturation = 1.f;
    Expressionist.Abstraction = .95f; Expressionist.Radius = 6.5f; Expressionist.Anisotropy = .9f;
    Expressionist.Contour = .16f; Expressionist.ToneStrength = .6f; Expressionist.ToneSteps = 4.f; Expressionist.SurfaceVariation = .045f; Expressionist.Saturation = .95f;
    ApplyGradeSettings();
}

void ATallowIllustratedLook::ApplyGradeSettings()
{
    // The look owns the grade only while it is active; preset Off blends all of it away.
    FPostProcessSettings& S = PostProcess->Settings;
    S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    S.bOverride_AutoExposureApplyPhysicalCameraExposure = true; S.AutoExposureApplyPhysicalCameraExposure = false;
    S.bOverride_AutoExposureBias = true; S.AutoExposureBias = ExposureBias;
    S.bOverride_WhiteTemp = true; S.WhiteTemp = 6500.f;  // neutral: the candle sources carry their own amber
    S.bOverride_FilmBlackClip = true; S.FilmBlackClip = BlackLift;
    // Shadows keep a brown-black body; candlelit middle values glow amber like hero.png's
    // (mean lit colour 81/43/17); highlights stay cream so the flame core reads white-hot.
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.06f);
    S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(1.f, .96f, .92f, 1.f);
    S.bOverride_ColorOffsetShadows = true; S.ColorOffsetShadows = FVector4(.0015f, .0009f, .0006f, 0.f);
    S.bOverride_ColorSaturationShadows = true; S.ColorSaturationShadows = FVector4(1.f, 1.f, 1.f, .85f);
    S.bOverride_ColorGainMidtones = true; S.ColorGainMidtones = FVector4(1.1f, .97f, .8f, 1.f);
    S.bOverride_ColorSaturationMidtones = true; S.ColorSaturationMidtones = FVector4(1.f, 1.f, 1.f, 1.2f);
    S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.04f, .97f, .88f, 1.f);
    S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(1.f, 1.f, 1.f, .9f);
    S.bOverride_BloomIntensity = true; S.BloomIntensity = BloomIntensity;
    S.bOverride_BloomThreshold = true; S.BloomThreshold = BloomThreshold;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.f;
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = GrainIntensity;
    S.bOverride_FilmGrainIntensityShadows = true; S.FilmGrainIntensityShadows = 1.f;
    S.bOverride_FilmGrainIntensityMidtones = true; S.FilmGrainIntensityMidtones = .6f;
    S.bOverride_FilmGrainIntensityHighlights = true; S.FilmGrainIntensityHighlights = .25f;
    S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
    S.bOverride_DepthOfFieldFstop = false;
}

void ATallowIllustratedLook::BeginPlay()
{
    Super::BeginPlay();
    auto* Tensor = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FinalPass/Look/M_LookTensor.M_LookTensor"));
    auto* Resolve = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FinalPass/Look/M_LookPainterly.M_LookPainterly"));
    if (!Tensor || !Resolve)
    {
        // The earlier resolve has no dark read but keeps the house painted.
        Tensor = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Illustrated10/M_TallowTensor.M_TallowTensor"));
        Resolve = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Illustrated10/M_TallowIllustrated.M_TallowIllustrated"));
    }
    if (!Tensor || !Resolve) { PostProcess->BlendWeight = 0.f; SetActorTickEnabled(false); return; }
    Painter = UMaterialInstanceDynamic::Create(Resolve, this);
    PostProcess->AddOrUpdateBlendable(Tensor, 1.f); PostProcess->AddOrUpdateBlendable(Painter, 1.f);
    ApplyGradeSettings();
    ProtectSubjects();
}

void ATallowIllustratedLook::SetPreset(ETallowIllustratedPreset Value) { Preset = Value; }

void ATallowIllustratedLook::ProtectSubjects()
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        const bool bPlayer = It->IsA<ATallowCharacter>();
        const bool bCreature = It->IsA<ATallowCreature>();
        const bool bAuthoredDetail = It->ActorHasTag(TEXT("Tallow.Illustrated.Detail"));
        if (!bPlayer && !bCreature && !bAuthoredDetail) continue;
        for (UPrimitiveComponent* Mesh : TInlineComponentArray<UPrimitiveComponent*>(*It))
        {
            if (!Mesh->IsA<UMeshComponent>()) continue;
            if (Protected.ContainsByPredicate([Mesh](const FStencilLease& L) { return L.Mesh == Mesh; })) continue;
            if (Mesh->bRenderCustomDepth && Mesh->CustomDepthStencilValue != 0) continue;
            FStencilLease L; L.Mesh = Mesh; L.Rendered = Mesh->bRenderCustomDepth; L.Value = Mesh->CustomDepthStencilValue; L.Applied = bPlayer ? 202 : bCreature ? 201 : 203;
            Protected.Add(L); Mesh->SetCustomDepthStencilValue(L.Applied); Mesh->SetRenderCustomDepth(true);
        }
    }
}

void ATallowIllustratedLook::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (!Painter) return;
    const int32 Override = CVarIllustratedPreset.GetValueOnGameThread();
    const int32 Index = Override >= 0 ? FMath::Clamp(Override, 0, 3) : static_cast<int32>(Preset);
    PostProcess->BlendWeight = Index == 0 ? 0.f : 1.f;
    if (Index == 0 && Protected.Num()) ReleaseSubjects();
    const FTallowIllustratedProfile& P = Index == 1 ? Subtle : Index == 3 ? Expressionist : Illustrated;
    // Before the candle is found the player stands in the dark: read it as out.
    int32 Slot = 0;
    if (const auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        if (const auto* Candle = Player->GetCandleComponent(); Candle && Candle->IsAcquired())
        {
            const ECandleState State = Candle->GetCandleState();
            Slot = State == ECandleState::Lit ? 2 : State == ECandleState::Cupped ? 1 : 0;
        }
    StateAbstraction = FMath::FInterpTo(StateAbstraction, CandleAbstraction[Slot], DeltaSeconds, 5.f);
    StateContour = FMath::FInterpTo(StateContour, CandleContours[Slot], DeltaSeconds, 5.f);
    // Eyes take seconds to open up to the dark and a moment to close down to a flame.
    const float Dark = CandleDarkRead[Slot], Exposure = CandleExposure[Slot];
    StateDark = FMath::FInterpTo(StateDark, Dark, DeltaSeconds, Dark > StateDark ? .9f : 4.f);
    StateExposure = FMath::FInterpTo(StateExposure, Exposure, DeltaSeconds, Exposure > StateExposure ? .9f : 4.f);
    Painter->SetScalarParameterValue(TEXT("Abstraction"), P.Abstraction * StateAbstraction);
    Painter->SetScalarParameterValue(TEXT("Radius"), P.Radius);
    Painter->SetScalarParameterValue(TEXT("Anisotropy"), P.Anisotropy);
    Painter->SetScalarParameterValue(TEXT("Contour"), P.Contour * StateContour);
    Painter->SetScalarParameterValue(TEXT("ToneStrength"), P.ToneStrength * StateAbstraction);
    Painter->SetScalarParameterValue(TEXT("ToneSteps"), P.ToneSteps);
    Painter->SetScalarParameterValue(TEXT("SurfaceVariation"), P.SurfaceVariation);
    Painter->SetScalarParameterValue(TEXT("DarkRead"), StateDark);
    Painter->SetScalarParameterValue(TEXT("DarkReach"), DarkReachCm);
    auto& S = PostProcess->Settings;
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1., 1., 1., P.Saturation);
    S.AutoExposureBias = ExposureBias + StateExposure;
    if (Index > 0 && GetWorld()->GetTimeSeconds() > RefreshAt) { ProtectSubjects(); RefreshAt = GetWorld()->GetTimeSeconds() + .5f; }
}

float ATallowIllustratedLook::GetGPUFrameMilliseconds() const
{
    static FRHIGPUFrameTimeHistory::FState History;
    static double Last = 0.; uint64 Cycles = 0;
    while (History.PopFrameCycles(Cycles) != FRHIGPUFrameTimeHistory::EResult::Empty) Last = FPlatformTime::ToMilliseconds64(Cycles);
    return static_cast<float>(Last);
}

void ATallowIllustratedLook::ReleaseSubjects()
{
    for (const auto& L : Protected) if (auto* Mesh = L.Mesh.Get(); Mesh && Mesh->CustomDepthStencilValue == L.Applied)
    { Mesh->SetCustomDepthStencilValue(L.Value); Mesh->SetRenderCustomDepth(L.Rendered); }
    Protected.Reset();
}

void ATallowIllustratedLook::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseSubjects(); Super::EndPlay(Reason);
}
