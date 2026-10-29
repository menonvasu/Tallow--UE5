#include "Input/TallowInputRouterSubsystem.h"
#include "Input/TallowGamepadContext.h"
#include "Input/TallowHaptics.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "GameMode/TallowGameMode.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowPlayDirector.h"
#include "Tasks/TallowChairEscape.h"
#include "Tasks/TallowLockpickGame.h"
#include "UI/Presentation/TallowSlidesWidget.h"

UTallowInputRouterSubsystem* UTallowInputRouterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTallowInputRouterSubsystem>() : nullptr;
}
bool UTallowInputRouterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UTallowInputRouterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Refresh();
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float) { Refresh(); return true; }), .1f);
}

void UTallowInputRouterSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	if (Pad)
	{
		SetContext(Pad->Main, UTallowGamepadContext::MainPriority, false);
		SetContext(Pad->Lockpick, UTallowGamepadContext::LockpickPriority, false);
		SetContext(Pad->Skip, UTallowGamepadContext::SkipPriority, false);
	}
	Super::Deinitialize();
}

static UEnhancedInputLocalPlayerSubsystem* PadSubsystem(const UWorld* World)
{
	const ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	return LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
}
void UTallowInputRouterSubsystem::SetContext(UInputMappingContext* Context, int32 Priority, bool bWanted)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PadSubsystem(GetWorld());
	if (!Subsystem || !Context) return;
	const bool bHas = Subsystem->HasMappingContext(Context);
	if (bWanted && !bHas) Subsystem->AddMappingContext(Context, Priority);
	else if (!bWanted && bHas) Subsystem->RemoveMappingContext(Context);
}
bool UTallowInputRouterSubsystem::IsMainContextApplied() const { const auto* S = PadSubsystem(GetWorld()); return S && Pad && S->HasMappingContext(Pad->Main); }
bool UTallowInputRouterSubsystem::IsLockpickContextApplied() const { const auto* S = PadSubsystem(GetWorld()); return S && Pad && S->HasMappingContext(Pad->Lockpick); }
bool UTallowInputRouterSubsystem::IsSkipContextApplied() const { const auto* S = PadSubsystem(GetWorld()); return S && Pad && S->HasMappingContext(Pad->Skip); }

ATallowPlayDirector* UTallowInputRouterSubsystem::Director()
{
	if (!CachedDirector.IsValid()) CachedDirector = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowPlayDirector::StaticClass()));
	return CachedDirector.Get();
}

void UTallowInputRouterSubsystem::Bind(APlayerController* PC)
{
	BoundController = PC;
	Component = NewObject<UEnhancedInputComponent>(PC, TEXT("TallowPadInput"));
	auto BindStarted = [this](FName Name, void (UTallowInputRouterSubsystem::*Fn)())
	{
		if (UInputAction* Action = Pad->FindAction(Name)) Component->BindAction(Action, ETriggerEvent::Started, this, Fn);
	};
	BindStarted(TEXT("IA_TallowPause"), &UTallowInputRouterSubsystem::HandlePause);
	BindStarted(TEXT("IA_TallowInventory"), &UTallowInputRouterSubsystem::HandleInventory);
	BindStarted(TEXT("IA_TallowMap"), &UTallowInputRouterSubsystem::HandleMap);
	BindStarted(TEXT("IA_TallowSkip"), &UTallowInputRouterSubsystem::HandleSkip);
	BindStarted(TEXT("IA_TallowReadToggle"), &UTallowInputRouterSubsystem::HandleReadToggle);
	BindStarted(TEXT("IA_TallowReadSkip"), &UTallowInputRouterSubsystem::HandleReadSkip);
	BindStarted(TEXT("IA_TallowStopLockpick"), &UTallowInputRouterSubsystem::HandleStopLockpick);
	PC->PushInputComponent(Component);
}

bool UTallowInputRouterSubsystem::WantsLockpickContext() const
{
	return BoundLock.IsValid() && BoundLock->IsComponentTickEnabled();
}

bool UTallowInputRouterSubsystem::WantsSkipContext()
{
	TArray<UUserWidget*> Slides;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Slides, UTallowSlidesWidget::StaticClass(), false);
	for (UUserWidget* Widget : Slides) if (const auto* Slide = Cast<UTallowSlidesWidget>(Widget)) if (Slide->IsPlaying()) return true;
	const ATallowPlayDirector* Dir = Director();
	if (!Dir) return false;
	// On the ending map the stills and credits skip; on the menu map a closed menu means the intro cinematic is running.
	return (Dir->Scene == ETallowPlayScene::Ending || Dir->Scene == ETallowPlayScene::MainMenu) && !Dir->IsMenuOpen();
}

void UTallowInputRouterSubsystem::Refresh()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController()) return;
	if (!Pad)
	{
		Pad = NewObject<UTallowGamepadContext>(this);
		float Yaw = 1.f, Pitch = 1.f;
		if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Yaw = PC->GetDeprecatedInputYawScale(); Pitch = PC->GetDeprecatedInputPitchScale();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		Pad->Build(Yaw, Pitch);
	}
	Pad->ApplyInversion();
	if (BoundController.Get() != PC) Bind(PC);
	SetContext(Pad->Main, UTallowGamepadContext::MainPriority, true);

	// The lock: bind its events, and push the pick feel only while a session ticks.
	const ATallowCharacter* Player = Cast<ATallowCharacter>(PC->GetPawn());
	UTallowLockpickGame* Lock = Player ? Player->GetLockpickComponent() : nullptr;
	if (Lock && BoundLock.Get() != Lock)
	{
		BoundLock = Lock;
		Lock->OnPinSet.AddUniqueDynamic(this, &UTallowInputRouterSubsystem::HandlePinSet);
		Lock->OnSnap.AddUniqueDynamic(this, &UTallowInputRouterSubsystem::HandleSnap);
		Lock->OnLockpickSet.AddUniqueDynamic(this, &UTallowInputRouterSubsystem::HandleLockOpen);
	}
	SetContext(Pad->Lockpick, UTallowGamepadContext::LockpickPriority, WantsLockpickContext());
	SetContext(Pad->Skip, UTallowGamepadContext::SkipPriority, WantsSkipContext());

	if (ATallowGameMode* Mode = World->GetAuthGameMode<ATallowGameMode>(); Mode && BoundMode.Get() != Mode)
	{
		BoundMode = Mode;
		Mode->OnCaught.AddUniqueDynamic(this, &UTallowInputRouterSubsystem::HandleCaught);
	}
	if (UTallowChapterSubsystem* Chapters = UTallowChapterSubsystem::GetActive(World); Chapters && BoundChapters.Get() != Chapters)
	{
		BoundChapters = Chapters;
		Chapters->OnBeatRequested.AddUniqueDynamic(this, &UTallowInputRouterSubsystem::HandleBeat);
	}

	// Chair: a rock is a count step; getting free is the slam.
	if (!CachedChair.IsValid()) CachedChair = Cast<ATallowChairEscape>(UGameplayStatics::GetActorOfClass(World, ATallowChairEscape::StaticClass()));
	if (const ATallowChairEscape* Chair = CachedChair.Get())
	{
		const int32 Rocks = Chair->GetRockCount();
		if (Rocks > LastRocks) TallowHaptics::Play(this, ETallowHaptic::ChairRock);
		LastRocks = Rocks;
	}
	const bool bTied = Player && Player->IsTiedToChair();
	if (bWasTied && !bTied && Player) TallowHaptics::Play(this, ETallowHaptic::ChairSlam);
	bWasTied = bTied;
}

void UTallowInputRouterSubsystem::HandlePause() { if (ATallowPlayDirector* D = Director()) D->TogglePause(); }
void UTallowInputRouterSubsystem::HandleInventory() { if (ATallowPlayDirector* D = Director()) D->OpenInventory(); }
void UTallowInputRouterSubsystem::HandleMap()
{
	if (ATallowCharacter* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) Player->ToggleHouseMap();
}
void UTallowInputRouterSubsystem::HandleSkip() { if (ATallowPlayDirector* D = Director()) D->SkipIntro(); }
void UTallowInputRouterSubsystem::HandleReadToggle() { if (ATallowPlayDirector* D = Director()) D->ToggleBackgroundReading(); }
void UTallowInputRouterSubsystem::HandleReadSkip() { if (ATallowPlayDirector* D = Director()) D->SkipBackgroundReading(); }
void UTallowInputRouterSubsystem::HandleStopLockpick()
{
	if (BoundLock.IsValid() && BoundLock->IsComponentTickEnabled() && !BoundLock->GetIsCompleting()) BoundLock->EndLockpickSession(false);
}
void UTallowInputRouterSubsystem::HandlePinSet() { TallowHaptics::Play(this, ETallowHaptic::LockPin); }
void UTallowInputRouterSubsystem::HandleSnap() { TallowHaptics::Play(this, ETallowHaptic::LockSnap); }
void UTallowInputRouterSubsystem::HandleLockOpen() { TallowHaptics::Play(this, ETallowHaptic::LockOpen); }
void UTallowInputRouterSubsystem::HandleCaught() { TallowHaptics::Play(this, ETallowHaptic::Capture); }
void UTallowInputRouterSubsystem::HandleBeat(ETallowChapterBeat Beat, const FTallowBeatContext&)
{
	if (Beat == ETallowChapterBeat::Transformation) TallowHaptics::Play(this, ETallowHaptic::Transformation);
	else if (Beat == ETallowChapterBeat::FuseEvent) TallowHaptics::Play(this, ETallowHaptic::FuseSeat);
}
