#include "Chapter/TallowStoryObject.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "Presentation/TallowNarrative.h"
#include "Presentation/TallowStorySubsystem.h"
#include "UI/Presentation/TallowJournalWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
ATallowStoryObject::ATallowStoryObject()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = false;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    for (auto* Reel : {&ReelA, &ReelB})
    {
        *Reel = CreateDefaultSubobject<UStaticMeshComponent>(Reel == &ReelA ? TEXT("ReelA") : TEXT("ReelB"));
        (*Reel)->SetupAttachment(Body); (*Reel)->SetCollisionEnabled(ECollisionEnabled::NoCollision); (*Reel)->SetCanEverAffectNavigation(false);
    }
    Speaker = CreateDefaultSubobject<UAudioComponent>(TEXT("Speaker")); Speaker->SetupAttachment(Body); Speaker->bAutoActivate = false;
    Speaker->bOverrideAttenuation = true;
    Speaker->AttenuationOverrides.bAttenuate = true;
    Speaker->AttenuationOverrides.bSpatialize = true;
    Speaker->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
    Speaker->AttenuationOverrides.AttenuationShapeExtents = FVector(100.f);
    Speaker->AttenuationOverrides.FalloffDistance = 1500.f;
    WritingAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("WritingAnchor")); WritingAnchor->SetupAttachment(Body);
    // On the flat of the deck at its front corner, the side away from her candle hand so the flame never hides it
    // (SM_StoryRecorder: deck top 9.39 cm).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Bead(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Indicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Indicator")); Indicator->SetupAttachment(Body);
    Indicator->SetStaticMesh(Bead.Object); Indicator->SetRelativeLocation(FVector(13.6f, 18.4f, 9.65f)); Indicator->SetRelativeScale3D(FVector(.014f, .014f, .009f));
    Indicator->SetCollisionEnabled(ECollisionEnabled::NoCollision); Indicator->SetCanEverAffectNavigation(false); Indicator->SetCastShadow(false);
    IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight")); IndicatorLight->SetupAttachment(Body);
    IndicatorLight->SetRelativeLocation(FVector(14.5f, 18.4f, 11.f)); IndicatorLight->SetMobility(EComponentMobility::Movable);
    IndicatorLight->SetIntensityUnits(ELightUnits::Lumens); IndicatorLight->SetIntensity(0.f); IndicatorLight->SetVisibility(false);
    IndicatorLight->SetLightColor(FLinearColor(1.f, .22f, .07f)); IndicatorLight->SetAttenuationRadius(60.f); IndicatorLight->SetSourceRadius(.6f);
    IndicatorLight->SetCastShadows(false); IndicatorLight->SetSpecularScale(.6f); IndicatorLight->SetIndirectLightingIntensity(0.f); IndicatorLight->SetVolumetricScatteringIntensity(0.f);
}
void ATallowStoryObject::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // The house darkens every untagged light at begin play; the lamp is the machine's own and keeps its glow.
    Tags.AddUnique(TEXT("Tallow.AuthoredLight"));
}
void ATallowStoryObject::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Lamp = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FinalPass/Tapes/M_TapeIndicator.M_TapeIndicator")))
    { IndicatorGlow = UMaterialInstanceDynamic::Create(Lamp, this); Indicator->SetMaterial(0, IndicatorGlow); }
    UpdateIndicator(0.f);
}
void ATallowStoryObject::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetForRound(); Super::EndPlay(Reason);
}
FName ATallowStoryObject::EssentialReading(int32 Chapter)
{
    const TCHAR* Readings[] = {TEXT("register"), TEXT("portrait"), TEXT("drawings"), TEXT("labels")};
    return Readings[FMath::Clamp(Chapter, 0, 3)];
}
FName ATallowStoryObject::GetReading() const { return Reading.IsNone() ? EssentialReading(ChapterIndex) : Reading; }
FName ATallowStoryObject::GetRoom() const { return TallowHouseRooms::RoomAt(GetActorLocation()); }
void ATallowStoryObject::SetAvailable(bool bInAvailable)
{
    bAvailable = bInAvailable; SetActorHiddenInGame(!bAvailable); SetActorEnableCollision(bAvailable);
    if (!bAvailable) { ResetForRound(); UpdateIndicator(0.f); }
    SetActorTickEnabled(bAvailable);
}
void ATallowStoryObject::ResetForRound()
{
    if (bPlaying && Reader && Reader->IsOpen() && Reader->GetDocumentKey() == GetReading()) Reader->SkipReading();
    bPlaying = false; bFinished = false;
    // Out, it ticks to keep its lamp and reels alive until it has been heard.
    SetActorTickEnabled(bAvailable);
    Speaker->Stop();
}
bool ATallowStoryObject::IsWaiting() const
{
    const auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    return bAvailable && !bFinished && Chapters && Chapters->GetChapterIndex() == ChapterIndex
        && (bPlaying || Chapters->GetPhase() == ETallowChapterPhase::Exploration);
}
void ATallowStoryObject::UpdateIndicator(float DeltaSeconds)
{
    // A slow warm breath while it waits, steady while it plays, out once heard.
    const bool bLit = IsWaiting();
    Glow = DeltaSeconds > 0.f ? FMath::FInterpTo(Glow, bLit ? 1.f : 0.f, DeltaSeconds, bLit ? 1.5f : 4.f) : (bLit ? 1.f : 0.f);
    const float Breath = bPlaying ? 1.f : .86f + .14f * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.6f);
    const float On = Glow * Breath;
    IndicatorLight->SetVisibility(On > .01f); IndicatorLight->SetIntensity(IndicatorLumens * On);
    // Kept low and saturated: brighter, the tonemapper bleaches it to a pale peach next to the candle.
    if (IndicatorGlow) IndicatorGlow->SetVectorParameterValue(TEXT("Glow"), FLinearColor(FMath::Lerp(.09f, 1.f, On), FMath::Lerp(.012f, .1f, On), FMath::Lerp(.008f, .025f, On), FMath::Lerp(1.f, 3.5f, On)));
}
bool ATallowStoryObject::IsPlaying() const { return bPlaying; }
void ATallowStoryObject::OnInteract_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor)) return;
    if (bPlaying)
    {
        if (Reader && Reader->IsOpen()) Reader->SkipReading();
        FinishListening(); return;
    }
    bPlaying = true; StartedAt = MovedAt = GetWorld()->GetTimeSeconds(); LastPosition = -1.f;
    if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(ETallowChapterEvent::StoryStarted, GetFName());
    const APawn* Listener = Cast<APawn>(Interactor);
    APlayerController* PC = Listener ? Cast<APlayerController>(Listener->GetController()) : nullptr;
    const TArray<FText> Pages = TallowNarrative::Pages(GetReading());
    if (PC && !Pages.IsEmpty())
    {
        // The journal's own reading, tucked away at once: she keeps moving while the tape plays.
        if (!Reader) Reader = CreateWidget<UTallowJournalWidget>(PC);
        Reader->Open(PC, Pages, TallowNarrative::Printed(GetReading()), GetReading());
        // She asked to hear the tape, even with paper voices off; that choice is not changed for the papers.
        if (Reader->IsNarrationMuted())
        {
            Reader->ReplayReading();
            if (auto* Story = UTallowStorySubsystem::GetStory(this)) Story->SetDocumentNarrationMuted(true);
        }
        Reader->MinimizeReading();
    }
    else if (Recording) { Speaker->SetSound(Recording); Speaker->Play(); }
    SetActorTickEnabled(true);
}
void ATallowStoryObject::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateIndicator(DeltaSeconds);
    if (!bPlaying)
    {
        // Waiting, the reels creep round as if the tape were threaded and only just running.
        if (IsWaiting()) for (auto* Reel : {ReelA.Get(), ReelB.Get()}) Reel->AddLocalRotation(FRotator(0, -IdleReelDegreesPerSecond * DeltaSeconds, 0));
        else if (bFinished && Glow < .01f) SetActorTickEnabled(false);
        return;
    }
    const bool bOnTape = Reader && Reader->IsOpen() && Reader->GetDocumentKey() == GetReading();
    if (bOnTape)
    {
        const double Now = GetWorld()->GetTimeSeconds();
        const float Position = Reader->GetPageIndex() * 1000.f + Reader->GetReadingPosition();
        if (!Reader->IsReading() || Position != LastPosition) { LastPosition = Position; MovedAt = Now; }
        // Without an audio device the reading's clock never moves; the tape still runs out.
        else if (Now - MovedAt > 3.0) { Reader->SkipReading(); return; }
    }
    if (bOnTape ? Reader->IsReading() : Speaker->IsPlaying())
        for (auto* Reel : {ReelA.Get(), ReelB.Get()}) Reel->AddLocalRotation(FRotator(0, -ReelDegreesPerSecond * DeltaSeconds, 0));
    if (bOnTape || Speaker->IsPlaying() || GetWorld()->GetTimeSeconds() - StartedAt < MinListenSeconds) return;
    FinishListening();
}
void ATallowStoryObject::FinishListening()
{
    if (!bPlaying || bFinished) return;
    bPlaying = false; bFinished = true;
    // It keeps ticking only to let the lamp fade out.
    Speaker->Stop();
    const FName Key = ClueKey.IsNone() ? GetReading() : ClueKey;
    if (auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>(); Story && !Key.IsNone()) Story->NoteRead(Key);
    if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(ETallowChapterEvent::StoryClueDone, GetFName());
}
bool ATallowStoryObject::CanInteract_Implementation(AActor* Interactor) const
{
    const auto* Chapters = UTallowChapterSubsystem::GetActive(this);
    return Interactor && bAvailable && !bFinished && Chapters && Chapters->GetChapterIndex() == ChapterIndex
        && (Chapters->GetPhase() == ETallowChapterPhase::Exploration || Chapters->GetPhase() == ETallowChapterPhase::StoryInteraction)
        && (!bPlaying || GetWorld()->GetTimeSeconds() - StartedAt >= MinListenSeconds);
}
FText ATallowStoryObject::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
    // The chapter's tape reads as the one that matters (29 Oct); the lore tapes only say "Listen".
    return FText::FromString(bPlaying ? TEXT("Stop the tape") : TEXT("Play the recording"));
}
