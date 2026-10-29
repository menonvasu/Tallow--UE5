#include "Presentation/TallowMenuScene.h"

#include "Animation/AnimSequence.h"
#include "Audio/TallowOpeningMusicSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Light.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Presentation/TallowPlayDirector.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Sound/SoundBase.h"
#include "Containers/Ticker.h"
#include "UI/Presentation/TallowMenuWidget.h"

ATallowMenuScene::ATallowMenuScene()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	auto MakePuppet = [this](const TCHAR* Name)
	{
		USkeletalMeshComponent* Puppet = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Puppet->SetupAttachment(RootComponent);
		Puppet->SetUsingAbsoluteLocation(true);
		Puppet->SetUsingAbsoluteRotation(true);
		Puppet->SetUsingAbsoluteScale(true);
		Puppet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puppet->SetGenerateOverlapEvents(false);
		Puppet->SetCanEverAffectNavigation(false);
		Puppet->SetVisibility(false);
		Puppet->bReceivesDecals = false;
		// Posed while hidden, so the first frame she is seen in is already the right one.
		Puppet->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Puppet->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		return Puppet;
	};
	Mim = MakePuppet(TEXT("Mim"));
	MimClose = MakePuppet(TEXT("MimClose"));
}

void ATallowMenuScene::BeginPlay()
{
	Super::BeginPlay();
	Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()));
	LightStates.SetNum(Lights.Num());
	for (int32 I = 0; I < Lights.Num(); ++I)
	{
		ULightComponent* Light = Lights[I].Light ? Lights[I].Light->GetLightComponent() : nullptr;
		if (!Light) continue;
		if (Light->Mobility == EComponentMobility::Static)
			UE_LOG(LogTemp, Warning, TEXT("TALLOW_MENU_SCENE %s is static and cannot be dimmed"), *Lights[I].Light->GetName());
		LightStates[I].Original = Light->Intensity;
	}
	SetLights(1.f);
	for (USkeletalMeshComponent* Puppet : {Mim.Get(), MimClose.Get()})
	{
		Puppet->SetTextureForceResidentFlag(true);
		PrepareFace(Puppet);
	}
	for (int32 I = 0; I < Apparitions.Num(); ++I) Order.Add(I);
	Schedule(FirstDelay);
}

void ATallowMenuScene::EndPlay(const EEndPlayReason::Type Reason)
{
	if (StingVoice) StingVoice->Stop();
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* ATallowMenuScene::Puppet(int32 Index) const
{
	return Index == 1 ? MimClose.Get() : Mim.Get();
}

void ATallowMenuScene::PrepareFace(USkeletalMeshComponent* P) const
{
	// Her eyes glow in the house; here only the candle may light her.
	static const FName GlowName(TEXT("EyeGlowGain"));
	for (int32 Slot = 0; P && Slot < P->GetNumMaterials(); ++Slot)
	{
		float Glow = 0.f;
		UMaterialInterface* Material = P->GetMaterial(Slot);
		if (Material && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(GlowName), Glow))
			if (UMaterialInstanceDynamic* Dynamic = P->CreateDynamicMaterialInstance(Slot, Material)) Dynamic->SetScalarParameterValue(GlowName, EyeGlow);
	}

	const USkinnedAsset* Asset = P ? P->GetSkinnedAsset() : nullptr;
	const FSkeletalMeshRenderData* Render = Asset ? Asset->GetResourceForRendering() : nullptr;
	if (!Render) return;
	const TArray<FSkeletalMaterial>& Slots = Asset->GetMaterials();
	for (int32 Lod = 0; Lod < Render->LODRenderData.Num(); ++Lod)
	{
		const TArray<FSkelMeshRenderSection>& Sections = Render->LODRenderData[Lod].RenderSections;
		for (int32 Section = 0; Section < Sections.Num(); ++Section)
		{
			const int32 Material = Sections[Section].MaterialIndex;
			const bool bFace = Slots.IsValidIndex(Material) && FaceSlots.Contains(Slots[Material].MaterialSlotName);
			P->ShowMaterialSection(Material, Section, bFace, Lod);
		}
	}
}

void ATallowMenuScene::Schedule(const FVector2D& Range)
{
	Waited = 0.f;
	NextAt = FMath::FRandRange(Range.X, FMath::Max(Range.X, Range.Y));
}

void ATallowMenuScene::SetLights(float RoomScale)
{
	for (int32 I = 0; I < Lights.Num(); ++I)
	{
		ULightComponent* Light = Lights[I].Light ? Lights[I].Light->GetLightComponent() : nullptr;
		if (!Light) continue;
		const float Living = Lights[I].bCandle ? Flicker * Gutter : 1.f;
		Light->SetIntensity(LightStates[I].Original * Lights[I].MenuScale * Living * RoomScale);
	}
}

void ATallowMenuScene::UpdateMenuState(float DeltaSeconds)
{
	UTallowMenuWidget* Menu = Director ? Director->GetMenuWidget() : nullptr;
	const bool bOpen = Director && Director->IsMenuOpen() && Menu;
	if (bOpen && !Menu->IsAwaitingAction() && !bMenuSeen)
	{
		bMenuSeen = true;
		if (auto* Waltz = UTallowOpeningMusicSubsystem::GetOpeningMusic(this)) Waltz->PlayFromMenu(Music, MusicFadeIn);
	}
	// A run has started (or the game is quitting): the waltz carries on, everything else goes back as it was.
	if (bMenuSeen && (!bOpen || Menu->IsAwaitingAction()))
	{
		PutBack();
		return;
	}
	const bool bIdle = bOpen && Menu->GetMenuMode() == ETallowMenuMode::Main && Menu->GetMenuPage() == ETallowMenuPage::Home;
	if (Step == EStep::Calm && !bHolding && bIdle)
	{
		Waited += DeltaSeconds;
		if (Waited >= NextAt && Menu->GetSecondsSinceInput() >= QuietSeconds) Appear(-1);
	}
}

void ATallowMenuScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone) return;
	UpdateMenuState(DeltaSeconds);
	if (bDone) return;
	// The candle breathes on twos, like the flame itself and everything else on the menu.
	FlickerClock += DeltaSeconds;
	if (FlickerClock >= 1.f / FMath::Max(FlickerRate, 1.f))
	{
		FlickerClock = 0.f;
		const float T = GetWorld()->GetTimeSeconds();
		const float Draught = .55f * FMath::Sin(T * 2.3f) + .3f * FMath::Sin(T * 5.9f + 1.3f) + .35f * (FMath::FRand() - .5f);
		Flicker = 1.f + FlickerAmount * Draught;
	}
	SetLights(Step == EStep::Dip ? DipLevel : 1.f);
	if (bHolding) return;

	StepLeft -= DeltaSeconds;
	switch (Step)
	{
	case EStep::Staged:
		if (StepLeft <= 0.f) { BeginFlicker(); EnterDip(0); }
		break;
	case EStep::Dip:
		if (StepLeft <= 0.f)
		{
			if (Apparitions[Current].Frames.IsValidIndex(DipNext)) EnterFrame(DipNext);
			else { Step = EStep::Settle; StepLeft = SettleSeconds; Gutter = FlareLevel; CutThisFrame(); }
		}
		break;
	case EStep::Frame:
		++FramesShown;
		if (StepLeft <= 0.f && FramesShown >= MinFrames) EnterDip(FrameIndex + 1);
		break;
	case EStep::Settle:
		// The last flare, on nothing, sinks back to the candle's own level.
		Gutter = FMath::Lerp(1.f, FlareLevel, FMath::Clamp(StepLeft / FMath::Max(SettleSeconds, .01f), 0.f, 1.f));
		if (StepLeft <= 0.f) EndFlicker();
		break;
	case EStep::Empty:
		if (StepLeft <= 0.f) { Step = EStep::Gutter; StepLeft = .55f; }
		break;
	case EStep::Gutter:
	{
		// The flame dips nearly out and comes back; she is in the chair again at the bottom of the dip.
		const float Into = 1.f - FMath::Clamp(StepLeft / .55f, 0.f, 1.f);
		Gutter = 1.f - .94f * FMath::Sin(Into * PI) * (Into < .3f ? 1.f : FMath::Exp(-(Into - .3f) * 3.f));
		if (Into > .3f && Nell && Nell->IsHidden()) Nell->SetActorHiddenInGame(false);
		if (StepLeft <= 0.f) { Gutter = 1.f; Step = EStep::Calm; Schedule(LaterDelay); }
		break;
	}
	default:
		break;
	}
	if (StingVoice && StingVoice->IsPlaying() && (StingAge += DeltaSeconds) > StingSeconds && !bStingFading)
	{
		bStingFading = true;
		StingVoice->FadeOut(2.f, 0.f);
	}
}

void ATallowMenuScene::Appear(int32 Index)
{
	if (bDone || Step != EStep::Calm || Apparitions.IsEmpty()) return;
	if (Index < 0)
	{
		// A shuffled bag: every appearance is seen before any repeats, and never the same one twice in a row.
		if (Order.IsEmpty()) for (int32 I = 0; I < Apparitions.Num(); ++I) if (I != Current || Apparitions.Num() == 1) Order.Add(I);
		const int32 Pick = FMath::RandHelper(Order.Num());
		Index = Order[Pick];
		Order.RemoveAtSwap(Pick);
	}
	Current = FMath::Clamp(Index, 0, Apparitions.Num() - 1);
	for (const FTallowMenuFlashFrame& Frame : Apparitions[Current].Frames) Pose(Frame);
	Step = EStep::Staged;
	StepLeft = StageLead;
}

void ATallowMenuScene::Pose(const FTallowMenuFlashFrame& Frame)
{
	USkeletalMeshComponent* P = Puppet(Frame.Puppet);
	if (!P || !Frame.Pose) return;
	P->SetWorldTransform(Frame.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	P->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	P->SetAnimation(Frame.Pose);
	P->SetPlayRate(0.f);
	P->SetPosition(Frame.PoseTime, false);
	P->TickAnimation(0.f, false);
	P->RefreshBoneTransforms();
}

void ATallowMenuScene::ShowPuppet(int32 Index)
{
	Mim->SetVisibility(Index == 0);
	MimClose->SetVisibility(Index == 1);
}

void ATallowMenuScene::BeginFlicker()
{
	bFlickering = true;
	if (!bHolding) ++Shown;
	const FTallowMenuApparition& Shot = Apparitions[Current];
	if (Shot.Sting && !bHolding)
	{
		if (StingVoice) StingVoice->Stop();
		StingVoice = UGameplayStatics::SpawnSound2D(this, Shot.Sting, StingVolume, 1.f, 0.f, nullptr, false, true);
		StingAge = 0.f; bStingFading = false;
	}
	if (!bHolding)
	{
		float Seconds = DipSeconds + SettleSeconds;
		for (const FTallowMenuFlashFrame& Frame : Shot.Frames) Seconds += DipSeconds + Frame.Seconds;
		if (auto* Waltz = UTallowOpeningMusicSubsystem::GetOpeningMusic(this)) Waltz->Duck(Seconds + MusicReturnDelay, 2.5f);
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MENU_SCENE flicker %d (appearance %d) frame=%llu"), Current, Shown, GFrameCounter);
}

void ATallowMenuScene::EnterDip(int32 Next)
{
	ShowPuppet(-1);
	if (Step == EStep::Frame) CutThisFrame();
	Step = EStep::Dip;
	StepLeft = DipSeconds;
	DipNext = Next;
	Gutter = DipLevel;
}

void ATallowMenuScene::EnterFrame(int32 Index)
{
	const TArray<FTallowMenuFlashFrame>& Frames = Apparitions[Current].Frames;
	if (!Frames.IsValidIndex(Index)) { EndFlicker(); return; }
	if (!bFlickering) BeginFlicker();
	const FTallowMenuFlashFrame& Frame = Frames[Index];
	FrameIndex = Index;
	// Only the candle's own flare lights her: no light of her own.
	ShowPuppet(Frame.Puppet);
	Gutter = FlareLevel;
	CutThisFrame();
	Step = EStep::Frame;
	StepLeft = Frame.Seconds;
	FramesShown = 0;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MENU_SCENE face %d of %d frame=%llu"), Index, Frames.Num(), GFrameCounter);
}

void ATallowMenuScene::EndFlicker(bool bQuiet)
{
	ShowPuppet(-1);
	bFlickering = false;
	Gutter = 1.f;
	SetLights(1.f);
	const float Empty = Apparitions.IsValidIndex(Current) && !bQuiet ? Apparitions[Current].EmptyChairSeconds : 0.f;
	if (Empty > 0.f && Nell)
	{
		// As if nothing happened, except that she is not there.
		Nell->SetActorHiddenInGame(true);
		Step = EStep::Empty;
		StepLeft = Empty;
	}
	else
	{
		Step = EStep::Calm;
		Schedule(LaterDelay);
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MENU_SCENE calm frame=%llu"), GFrameCounter);
}

void ATallowMenuScene::CutThisFrame() const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
}

void ATallowMenuScene::PutBack()
{
	if (bDone) return;
	bDone = true;
	bHolding = false;
	if (bFlickering) EndFlicker(true);
	Step = EStep::Calm;
	Gutter = 1.f;
	if (Nell) Nell->SetActorHiddenInGame(false);
	if (StingVoice && StingVoice->IsPlaying()) StingVoice->FadeOut(.3f, 0.f);
	for (int32 I = 0; I < Lights.Num(); ++I)
		if (ULightComponent* Light = Lights[I].Light ? Lights[I].Light->GetLightComponent() : nullptr)
			Light->SetIntensity(LightStates[I].Original);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MENU_SCENE put back after %d appearance(s)"), Shown);
}

void ATallowMenuScene::Hold(int32 Index, int32 HoldStep)
{
	if (bDone || !Apparitions.IsValidIndex(Index)) return;
	Release();
	bHolding = true;
	Current = Index;
	for (const FTallowMenuFlashFrame& Frame : Apparitions[Current].Frames) Pose(Frame);
	const int32 Count = Apparitions[Current].Frames.Num();
	if (HoldStep >= 0 && HoldStep < Count) EnterFrame(HoldStep);
	else if (HoldStep == Count) { bFlickering = true; EnterDip(Count); }
	else if (HoldStep == Count + 1) { bFlickering = true; ShowPuppet(-1); Step = EStep::Settle; Gutter = FlareLevel; }
	else if (HoldStep > Count + 1 && Nell) { Nell->SetActorHiddenInGame(true); Step = EStep::Empty; }
}

void ATallowMenuScene::Release()
{
	if (bFlickering) EndFlicker(true);
	if (Nell) Nell->SetActorHiddenInGame(false);
	bHolding = false;
	Step = EStep::Calm;
	Gutter = 1.f;
}

FString ATallowMenuScene::GetStepName() const
{
	switch (Step)
	{
	case EStep::Staged: return TEXT("Staged");
	case EStep::Dip: return TEXT("Dip");
	case EStep::Frame: return FString::Printf(TEXT("Frame%d"), FrameIndex);
	case EStep::Settle: return TEXT("Settle");
	case EStep::Empty: return TEXT("Empty");
	case EStep::Gutter: return TEXT("Gutter");
	default: return bDone ? TEXT("Done") : TEXT("Calm");
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	ATallowMenuScene* FindScene(UWorld* World)
	{
		if (!World) return nullptr;
		TActorIterator<ATallowMenuScene> It(World);
		return It ? *It : nullptr;
	}
	FAutoConsoleCommandWithWorldAndArgs AppearCommand(TEXT("Tallow.Menu.Appear"),
		TEXT("Show the menu's flash now: [index]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ATallowMenuScene* Scene = FindScene(World)) Scene->Appear(Args.Num() ? FCString::Atoi(*Args[0]) : -1);
		}));
	FAutoConsoleCommandWithWorldAndArgs HoldCommand(TEXT("Tallow.Menu.Hold"),
		TEXT("Review: hold <index> <step> (-1 calm, 0.. a flash frame, the frame count the empty chair), or 'off'."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATallowMenuScene* Scene = FindScene(World);
			if (!Scene) return;
			if (Args.Num() < 2) Scene->Release();
			else Scene->Hold(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		}));
	FAutoConsoleCommandWithWorldAndArgs KeyCommand(TEXT("Tallow.Menu.Key"),
		TEXT("Review: press and release a key through Slate, as the keyboard would (e.g. Down, Enter, Escape)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (!Args.Num() || !FSlateApplication::IsInitialized()) return;
			const FKey Key(*Args[0]);
			if (!Key.IsValid()) return;
			const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
			FSlateApplication::Get().ProcessKeyDownEvent(Event);
			FSlateApplication::Get().ProcessKeyUpEvent(Event);
		}));
	// Review runs in -game have no script: -ExecCmds="Tallow.Review.After 6 shot showui, Tallow.Review.After 20 quit".
	FAutoConsoleCommandWithWorldAndArgs AfterCommand(TEXT("Tallow.Review.After"),
		TEXT("Review: run a console command after <seconds> of real time."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (Args.Num() < 2) return;
			const float Seconds = FCString::Atof(*Args[0]);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
			{
				UWorld* Game = nullptr;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
					if (Context.World() && Context.World()->IsGameWorld()) { Game = Context.World(); break; }
				UE_LOG(LogTemp, Display, TEXT("TALLOW_REVIEW %s"), *Command);
				// As if typed in the console, so viewport commands (shot) are reached too.
				if (APlayerController* PC = Game ? Game->GetFirstPlayerController() : nullptr) PC->ConsoleCommand(Command);
				else GEngine->Exec(Game, *Command);
				return false;
			}), Seconds);
		}));
}
#endif
