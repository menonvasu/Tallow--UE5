#include "Presentation/TallowPlayDirector.h"
#include "Presentation/TallowRunWidget.h"
#include "AI/TallowCreature.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameMode/TallowGameMode.h"
#include "Interactable/TallowInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Player/TallowCharacter.h"
#include "Tasks/TallowChairEscape.h"
#include "Tasks/TallowCrowbarPickup.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowFusePickup.h"
#include "Tasks/TallowPlank.h"
#include "UI/Presentation/TallowLessonWidget.h"
#include "Animation/AnimSequence.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "Audio/TallowFootstepComponent.h"
#include "Components/AudioComponent.h"
#include "Environment/TallowDoor.h"
#include "Presentation/TallowRouteGate.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

ATallowPlayDirector::ATallowPlayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATallowPlayDirector::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(InitializeHandle, this, &ATallowPlayDirector::InitializeScene, .15f, false);
}

void ATallowPlayDirector::InitializeScene()
{
	PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->IsLocalController()) return;
	EnableInput(PC);
	InputComponent->Priority = 10;
	auto& PauseKey = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATallowPlayDirector::TogglePause);
	PauseKey.bExecuteWhenPaused = true;
	PauseKey.bConsumeInput = false;
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ATallowPlayDirector::SkipIntro).bConsumeInput = false;
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
	bInitialized = true;
	if (Scene == ETallowPlayScene::MainMenu)
	{
		PlaySequence(MenuSequence, true);
		SetMenu(ETallowMenuMode::Main);
	}
	else if (Scene == ETallowPlayScene::Ending)
	{
		if (!PlaySequence(OutroSequence, false)) OnSequenceFinished();
	}
	else InitializeGameplay();
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION ready scene=%d"), static_cast<int32>(Scene));
}

void ATallowPlayDirector::InitializeGameplay()
{
	Player = Cast<ATallowCharacter>(PC->GetPawn());
	Keeper = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()));
	FuseBox = Cast<ATallowFuseBox>(UGameplayStatics::GetActorOfClass(this, ATallowFuseBox::StaticClass()));
	if (!Player) { UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION missing existing player")); return; }
	bWasTied = Player->IsTiedToChair();
	PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false;
	Lessons = CreateWidget<UTallowLessonWidget>(PC);
	Lessons->AddToViewport(12); Lessons->ShowLesson(ETallowLesson::Chair);
	RunUI = CreateWidget<UTallowRunWidget>(PC);
	RunUI->Director = this; RunUI->AddToViewport(8);
	RandomizePickups();
	for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It) ApplyPickupVisual(*It);
	for (TActorIterator<ATallowCrowbarPickup> It(GetWorld()); It; ++It) ApplyPickupVisual(*It);
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ATallowPlayDirector::OnActorSpawned));
	BindGameplayEvents();
	AddFootsteps();
	SetKeeperAwake(false);
	SetPowerVisuals(false);
	if (!PlayerHands || !CandleMesh || !FlameMesh || !FlameMaterial || !HoldClip)
	{
		UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION missing presentation references")); return;
	}
	// Hide the Blueprint's original visual pieces as well as the native flicker adapter's prop.
	// HiddenInGame is retained even if an existing acquisition callback sets Visibility again.
	for (auto* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Player))
	{
		if (Mesh->GetName() == TEXT("HeldItemMesh")) OriginalHeldTool = Mesh;
		Mesh->SetHiddenInGame(true);
		Mesh->SetCastShadow(false);
	}
	Player->GetMesh()->SetHiddenInGame(true);
	Player->GetMesh()->SetCastShadow(false);
	HeldCandle = NewObject<UStaticMeshComponent>(Player, TEXT("IntegratedCandle"));
	HeldCandle->SetupAttachment(Player->GetFirstPersonCameraComponent());
	HeldCandle->SetStaticMesh(CandleMesh);
	HeldCandle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldCandle->SetCastShadow(false);
	HeldCandle->SetOnlyOwnerSee(true);
	Player->AddInstanceComponent(HeldCandle); HeldCandle->RegisterComponent();
	Wick = NewObject<USceneComponent>(Player, TEXT("IntegratedWick"));
	Wick->SetupAttachment(HeldCandle); Wick->SetRelativeLocation(WickPosition);
	Player->AddInstanceComponent(Wick); Wick->RegisterComponent();
	Flame = NewObject<UTallowFlameVisualComponent>(Player, TEXT("IntegratedFlame"));
	Flame->WickAnchor = Wick; Flame->CandleSource = Player->GetCandleComponent();
	Flame->FlameMesh = FlameMesh; Flame->FlameMaterial = FlameMaterial; Flame->bFirstPersonVisual = true;
	Player->AddInstanceComponent(Flame); Flame->RegisterComponent(); Flame->InitializeVisual();
	if (Flame->GetVisualMesh()) Flame->GetVisualMesh()->SetOnlyOwnerSee(true);
	Presentation = NewObject<UTallowPlayerPresentationComponent>(Player, TEXT("IntegratedPlayer"));
	Player->AddInstanceComponent(Presentation); Presentation->RegisterComponent();
	if (!Presentation->EnablePresentation(Player->GetFirstPersonCameraComponent(), PlayerHands, HeldCandle, Flame->GetVisualMesh()))
	{
		UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION could not enable hands")); return;
	}
	Presentation->SetPresentationPose(HoldClip, 0.f);
	if (!Presentation->AttachHeldCandleToHand(CandleHandBone, CandleCameraTransform))
		UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION candle hand attachment failed"));
	if (ToolHoldClip && Presentation->SetPresentationPose(ToolHoldClip, 0.f))
	{
		auto* Hands = Presentation->GetPresentationHands();
		const FTransform Hand = Hands->GetSocketTransform(TEXT("hand_l"));
		const FTransform Camera = Player->GetFirstPersonCameraComponent()->GetComponentTransform();
		CrowbarHandTransform = (CrowbarCameraTransform * Camera).GetRelativeTransform(Hand);
		FuseHandTransform = (FuseCameraTransform * Camera).GetRelativeTransform(Hand);
		HeldTool = NewObject<UStaticMeshComponent>(Player, TEXT("IntegratedHeldTool"));
		HeldTool->SetupAttachment(Hands,TEXT("hand_l"));
		HeldTool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldTool->SetOnlyOwnerSee(true); HeldTool->SetCastShadow(false);
		HeldTool->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Player->AddInstanceComponent(HeldTool); HeldTool->RegisterComponent();
	}
	if (MatchMesh && RelightClip && Presentation->SetPresentationPose(RelightClip, MatchCalibrationTime))
	{
		HeldMatch = NewObject<UStaticMeshComponent>(Player,TEXT("IntegratedMatch"));
		HeldMatch->SetupAttachment(Player->GetFirstPersonCameraComponent());
		HeldMatch->SetStaticMesh(MatchMesh); HeldMatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldMatch->SetOnlyOwnerSee(true); HeldMatch->SetCastShadow(false);
		HeldMatch->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Player->AddInstanceComponent(HeldMatch); HeldMatch->RegisterComponent();
		HeldMatch->SetRelativeTransform(MatchCameraTransform);
		HeldMatch->AttachToComponent(Presentation->GetPresentationHands(),FAttachmentTransformRules::KeepWorldTransform,TEXT("hand_l"));
		auto* Tip = NewObject<USceneComponent>(Player,TEXT("IntegratedMatchTip"));
		Tip->SetupAttachment(HeldMatch); Tip->SetRelativeLocation(FVector(0,0,4.1f));
		Player->AddInstanceComponent(Tip); Tip->RegisterComponent();
		// The match points toward the wick; its flame rises instead of inheriting the stick's downward tilt.
		Tip->SetUsingAbsoluteRotation(true); Tip->SetWorldRotation(FRotator::ZeroRotator);
		MatchFlame = NewObject<UTallowFlameVisualComponent>(Player,TEXT("IntegratedMatchFlame"));
		MatchFlame->WickAnchor = Tip; MatchFlame->bStandalone = true; MatchFlame->bFirstPersonVisual = true;
		MatchFlame->FlameMesh = FlameMesh; MatchFlame->FlameMaterial = FlameMaterial;
		MatchFlame->HeightCm = 1.2f; MatchFlame->WidthCm = .35f; MatchFlame->StandaloneState = ECandleState::Out;
		Player->AddInstanceComponent(MatchFlame); MatchFlame->RegisterComponent(); MatchFlame->InitializeVisual();
		if (MatchFlame->GetVisualMesh()) MatchFlame->GetVisualMesh()->SetOnlyOwnerSee(true);
	}
	for (auto* Mesh : TInlineComponentArray<USkeletalMeshComponent*>(Player))
		if (Mesh != Player->GetMesh() && Mesh != Presentation->GetPresentationHands()) Mesh->SetVisibility(false);
	if (auto* Light = Player->FindComponentByClass<UTallowCandleFlameComponent>()) Light->UseExternalPresentation(Wick, CandleIntensityLumens);
	HeldCandle->SetVisibility(Player->GetCandleComponent()->IsAcquired());
	UpdatePlayerAnimation();
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FColor::Black, false, false);
}

void ATallowPlayDirector::ApplyPickupVisual(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	UStaticMesh* Mesh = Actor->IsA<ATallowFusePickup>() ? FuseMesh.Get() : Actor->IsA<ATallowCrowbarPickup>() ? CrowbarMesh.Get() : nullptr;
	if (!Mesh) return;
	auto* Visual = Actor->FindComponentByClass<UStaticMeshComponent>();
	if (!Visual || Visual->GetStaticMesh() == Mesh) return;
	const FVector Surface = Actor->GetActorLocation();
	const FRotator Rotation(90.f,Actor->GetActorRotation().Yaw,0.f);
	const FBox Bounds = Mesh->GetBoundingBox().TransformBy(FTransform(Rotation));
	Visual->SetStaticMesh(Mesh);
	Visual->EmptyOverrideMaterials();
	Actor->SetActorScale3D(FVector::OneVector);
	Actor->SetActorLocationAndRotation(Surface + FVector(0,0,-Bounds.Min.Z),Rotation);
	Visual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Visual->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Visual->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
}

void ATallowPlayDirector::OnActorSpawned(AActor* Actor)
{
	if (!Actor || (!Actor->IsA<ATallowFusePickup>() && !Actor->IsA<ATallowCrowbarPickup>())) return;
	// Blueprint construction finishes after the spawn notification. Keep its drop/reset ownership intact.
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,WeakActor] { ApplyPickupVisual(WeakActor.Get()); }));
}

void ATallowPlayDirector::BindGameplayEvents()
{
	TotalPlanks = 0;
	for (TActorIterator<ATallowPlank> It(GetWorld()); It; ++It)
	{
		++TotalPlanks; It->OnPlankDestroyed.AddUniqueDynamic(this, &ATallowPlayDirector::OnPlankRemoved);
	}
	if (FuseBox) FuseBox->OnPowerRestored.AddUniqueDynamic(this, &ATallowPlayDirector::OnPower);
	if (FuseBox) FuseBox->OnFuseInserted.AddUniqueDynamic(this, &ATallowPlayDirector::OnFuseInserted);
	if (auto* Candle = Player->GetCandleComponent())
	{
		Candle->OnAcquireCandle.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleAcquired);
		Candle->OnCandleReset.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleReset);
		Candle->OnCandleStateChanged.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleStateChanged);
		Candle->OnRelightStarted.AddUniqueDynamic(this, &ATallowPlayDirector::OnRelight);
	}
	Player->OnStill.AddUniqueDynamic(this, &ATallowPlayDirector::OnStill);
	if (auto* GM = GetWorld()->GetAuthGameMode<ATallowGameMode>()) GM->OnCaught.AddUniqueDynamic(this, &ATallowPlayDirector::OnCaught);
}

void ATallowPlayDirector::RandomizePickups()
{
	ActiveRunSeed = RunSeed ? RunSeed : FMath::Rand();
	FRandomStream Random(ActiveRunSeed);
	auto Candidates = [this](FName Tag)
	{
		TArray<AActor*> Points; UGameplayStatics::GetAllActorsWithTag(this, Tag, Points);
		Points.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
		return Points;
	};
	TArray<AActor*> Bars = Candidates(TEXT("Tallow.Spawn.Crowbar"));
	if (auto* Crowbar = UGameplayStatics::GetActorOfClass(this, ATallowCrowbarPickup::StaticClass()); Crowbar && Bars.Num())
	{
		const auto* Point = Bars[Random.RandRange(0, Bars.Num()-1)];
		Crowbar->SetActorLocationAndRotation(Point->GetActorLocation(), Point->GetActorRotation());
	}
	TArray<AActor*> Fuses; UGameplayStatics::GetAllActorsOfClass(this, ATallowFusePickup::StaticClass(), Fuses);
	Fuses.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	const FName Groups[] = {TEXT("Tallow.Spawn.Fuse.Basement"), TEXT("Tallow.Spawn.Fuse.Ground"), TEXT("Tallow.Spawn.Fuse.Upper")};
	for (int32 I = 0; I < Fuses.Num() && I < 3; ++I)
	{
		const auto Points = Candidates(Groups[I]);
		if (!Points.Num()) { UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION missing fuse spawn group %d"), I); continue; }
		const auto* Point = Points[Random.RandRange(0, Points.Num()-1)];
		Fuses[I]->SetActorLocationAndRotation(Point->GetActorLocation(), Point->GetActorRotation());
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION run seed=%d, fuses=%d; placements persist through capture"), ActiveRunSeed, Fuses.Num());
}

void ATallowPlayDirector::SetKeeperAwake(bool bAwake)
{
	bKeeperAwake = bAwake;
	if (Keeper)
		if (auto* AI = Cast<AAIController>(Keeper->GetController()))
			if (auto* Brain = AI->GetBrainComponent())
			{
				if (bAwake) Brain->ResumeLogic(TEXT("Opening complete"));
				else { Brain->PauseLogic(TEXT("Safe opening")); AI->StopMovement(); }
			}
}

void ATallowPlayDirector::SetPowerVisuals(bool bEnabled)
{
	TArray<AActor*> Lights; UGameplayStatics::GetAllActorsWithTag(this, TEXT("Tallow.PowerLight"), Lights);
	for (auto* Actor : Lights)
		for (auto* Light : TInlineComponentArray<ULightComponent*>(Actor)) Light->SetVisibility(bEnabled);
	// Old wiring humming in the walls once the power is back; capture resets it with the lights.
	if (!bEnabled && PowerHum) { PowerHum->Stop(); PowerHum = nullptr; }
	if (bEnabled && !PowerHum && FuseBox && Cue(TEXT("H_Power_Hum")))
		PowerHum = UGameplayStatics::SpawnSoundAtLocation(this, Cue(TEXT("H_Power_Hum")), FuseBox->GetActorLocation(), FRotator::ZeroRotator, .35f, 1.f, 0.f, CueAttenuation);
}

void ATallowPlayDirector::SetMenu(ETallowMenuMode Mode)
{
	if (!Menu)
	{
		Menu = CreateWidget<UTallowMenuWidget>(PC);
		Menu->OnActionRequested.AddUniqueDynamic(this, &ATallowPlayDirector::OnMenuAction);
	}
	Menu->ShowMenu(Mode);
	if (!Menu->IsInViewport()) Menu->AddToViewport(50);
	FInputModeUIOnly Input; Input.SetWidgetToFocus(Menu->TakeWidget()); PC->SetInputMode(Input);
	PC->bShowMouseCursor = true; Menu->SetKeyboardFocus();
}

void ATallowPlayDirector::CloseMenu()
{
	if (Menu) Menu->RemoveFromParent();
	if (PC) { PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false; }
	if (Scene == ETallowPlayScene::Game) UGameplayStatics::SetGamePaused(this, false);
}

bool ATallowPlayDirector::PlaySequence(ULevelSequence* Sequence, bool bLoop)
{
	if (SequencePlayer) { SequencePlayer->OnFinished.RemoveAll(this); SequencePlayer->Stop(); }
	if (SequenceActor) SequenceActor->Destroy();
	if (!Sequence) return false;
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.LoopCount.Value = bLoop ? -1 : 0;
	Settings.bHideHud = false;
	Settings.bDisableMovementInput = true; Settings.bDisableLookAtInput = true;
	ALevelSequenceActor* CreatedActor = nullptr;
	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, CreatedActor);
	SequenceActor = CreatedActor;
	if (!SequencePlayer) return false;
	SequencePlayer->OnFinished.AddUniqueDynamic(this, &ATallowPlayDirector::OnSequenceFinished);
	SequencePlayer->Play(); return true;
}

void ATallowPlayDirector::OnMenuAction(ETallowMenuAction Action)
{
	switch (Action)
	{
	case ETallowMenuAction::Start:
		CloseMenu(); bIntroPlaying = true;
		if (!PlaySequence(IntroSequence, false)) OnSequenceFinished();
		break;
	case ETallowMenuAction::Resume: CloseMenu(); break;
	case ETallowMenuAction::Restart: LoadDestination(GameMap); break;
	case ETallowMenuAction::MainMenu: LoadDestination(MenuMap); break;
	case ETallowMenuAction::Quit: UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false); break;
	}
}

void ATallowPlayDirector::LoadDestination(const TSoftObjectPtr<UWorld>& Destination)
{
	if (Destination.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION destination missing"));
		if (Menu) Menu->RearmActions(); return;
	}
	bLeaving = true;
	CloseMenu();
	if (SequencePlayer) { SequencePlayer->OnFinished.RemoveAll(this); SequencePlayer->Stop(); }
	if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Destination);
}

void ATallowPlayDirector::OnSequenceFinished()
{
	if (bLeaving) return;
	if (Scene == ETallowPlayScene::MainMenu && bIntroPlaying) { bIntroPlaying = false; LoadDestination(GameMap); }
	else if (Scene == ETallowPlayScene::Ending) SetMenu(ETallowMenuMode::Outro);
}

void ATallowPlayDirector::SkipIntro()
{
	if (bIntroPlaying) OnSequenceFinished();
	else if (Scene == ETallowPlayScene::Ending && !IsMenuOpen())
	{
		if (SequencePlayer) SequencePlayer->Stop();
		if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1,1,0,FColor::Black,false,true);
		OnSequenceFinished();
	}
}

void ATallowPlayDirector::TogglePause()
{
	if (bIntroPlaying || Scene == ETallowPlayScene::Ending) { SkipIntro(); return; }
	if (Scene != ETallowPlayScene::Game || bLeaving) return;
	if (IsMenuOpen()) { Menu->GoBack(); return; }
	if (Player)
		if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>()) Interaction->CancelInteract();
	UGameplayStatics::SetGamePaused(this, true);
	SetMenu(ETallowMenuMode::Pause);
}

void ATallowPlayDirector::OnChairEscaped()
{
	PlayCue(TEXT("P_Chair_Free"), .6f);
	Lessons->CompleteLessonIfActive(ETallowLesson::Chair); Lessons->ShowLesson(ETallowLesson::CandlePickup);
	ForcedClip = EscapeClip; ForcedUntil = GetWorld()->GetTimeSeconds() + (EscapeClip ? EscapeClip->GetPlayLength() : 0.f);
}
void ATallowPlayDirector::OnCandleAcquired()
{
	Lessons->CompleteLessonIfActive(ETallowLesson::CandlePickup);
	Lessons->ShowLesson(ETallowLesson::Cup);
	PlayCue(TEXT("P_Cup_Rustle_01"), .3f);
	ForcedClip = PickupClip; ForcedUntil = GetWorld()->GetTimeSeconds() + (PickupClip ? PickupClip->GetPlayLength() : 0.f);
}
void ATallowPlayDirector::OnCandleReset()
{
	bCaptureTracking = false;
	if (CaptureBaseFOV > 0.f && Player) Player->GetFirstPersonCameraComponent()->SetFieldOfView(CaptureBaseFOV);
	if (CaptureLight) CaptureLight->SetIntensity(0.f);
	CaptureTime = 0.f;
	ForcedClip = nullptr; bPowerRestored = false; bBoardsCleared = false; RemovedPlanks = 0; FirstPlankAt = -1.f;
	SetPowerVisuals(false); SetKeeperAwake(false);
	if (Lessons) { Lessons->DismissLesson(); Lessons->ShowLesson(ETallowLesson::Chair); }
}
void ATallowPlayDirector::OnCandleStateChanged(ECandleState State)
{
	// A small puff and a wisp when it goes out; the sleeve when you cup it.
	if (State == ECandleState::Out && LastCandleState != ECandleState::Out && Player->GetCandleComponent()->IsAcquired()) PlayCue(TEXT("P_Candle_Puff"), .5f);
	if (State == ECandleState::Cupped && LastCandleState == ECandleState::Lit) PlayCue(FString::Printf(TEXT("P_Cup_Rustle_%02d"), FMath::RandRange(0, 1)), .3f);
	LastCandleState = State;
	if (!Lessons || !Player->GetCandleComponent()->IsAcquired()) return;
	if (State == ECandleState::Out) Lessons->ShowLesson(ETallowLesson::Relight);
	if (State == ECandleState::Lit) Lessons->CompleteLessonIfActive(ETallowLesson::Relight);
}
void ATallowPlayDirector::OnRelight()
{
	ForcedClip = RelightClip; ForcedUntil = GetWorld()->GetTimeSeconds() + 1.2f;
	// Scratch and flare as the hand comes in; the wick takes when the match meets it (MatchCalibrationTime).
	PlayCue(TEXT("P_Match_Strike"), .55f);
	GetWorldTimerManager().SetTimer(CatchHandle, FTimerDelegate::CreateWeakLambda(this, [this] { PlayCue(TEXT("P_Candle_Catch"), .45f); }), MatchCalibrationTime, false);
}
void ATallowPlayDirector::OnPower()
{
	bPowerRestored = true;
	if (FuseBox) { const FVector Box = FuseBox->GetActorLocation(); PlayCue(TEXT("P_Breaker_Clunk"), .8f, &Box); }
	SetPowerVisuals(true);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION power restored; attic route released"));
}
void ATallowPlayDirector::OnPlankRemoved(bool bBroken)
{
	// Nails letting go, then the board hitting the floor.
	PlayCue(TEXT("P_Board_Pry"), .7f);
	GetWorldTimerManager().SetTimer(BoardDropHandle, FTimerDelegate::CreateWeakLambda(this, [this] { PlayCue(TEXT("P_Board_Drop"), .55f); }), .55f, false);
	if (!bBroken || bBoardsCleared) return;
	if (!bKeeperAwake) FirstPlankAt = GetWorld()->GetTimeSeconds();
	SetKeeperAwake(true);
	RemovedPlanks = FMath::Min(RemovedPlanks+1, TotalPlanks);
	bBoardsCleared = TotalPlanks > 0 && RemovedPlanks >= TotalPlanks;
}
void ATallowPlayDirector::OnStill(bool bStill) { if (!bStill) StillTime = 0.f; }
void ATallowPlayDirector::OnCaught()
{
	if (Lessons) Lessons->DismissLesson();
	ForcedClip = nullptr;
	bCaptureTracking = true;
	CaptureTime = 0.f;
	// Everything hits on the variant's strike frame, not when the capture starts: the wind-up stays quiet.
	CaptureStrike = 0.f;
	if (Keeper)
		if (const auto* Anim = Keeper->FindComponentByClass<UTallowCreatureAnimationComponent>()) CaptureStrike = Anim->GetCaptureStrikeTime();
	if (CaptureStrike > .05f)
		GetWorldTimerManager().SetTimer(StingHandle, FTimerDelegate::CreateWeakLambda(this, [this] { PlayCue(TEXT("K_Capture_Sting"), .85f); }), CaptureStrike, false);
	else PlayCue(TEXT("K_Capture_Sting"), .85f);
	// The jumpscares are staged with the Keeper's root 1.25 m along the look line: closer and its head meets the lens,
	// further and the maw shrinks. Stop short of any wall in between.
	if (Keeper && Player)
	{
		const FVector Eye = Player->GetFirstPersonCameraComponent()->GetComponentLocation();
		FVector Look = Keeper->GetActorLocation() - Eye; Look.Z = 0.f;
		if (Look.Normalize())
		{
			float Reach = 125.f; FHitResult Hit;
			if (const auto* Anim = Keeper->FindComponentByClass<UTallowCreatureAnimationComponent>()) Reach *= FMath::Clamp(Anim->KeeperMeshScale, .5f, 2.f);  // staged for a 1.0 scale Keeper
			FCollisionQueryParams Params(TEXT("CaptureStage"), false, Player); Params.AddIgnoredActor(Keeper);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look * Reach, ECC_Visibility, Params)) Reach = FMath::Max(60.f, Hit.Distance - 40.f);
			Keeper->SetActorLocationAndRotation(FVector(Eye.X + Look.X * Reach, Eye.Y + Look.Y * Reach, Keeper->GetActorLocation().Z), (-Look).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	if (Player)
	{
		CaptureBaseFOV = Player->GetFirstPersonCameraComponent()->FieldOfView;
		if (!CaptureLight)
		{
			// The candle thrust up at its face: without it the opening mask is lost in the dark.
			CaptureLight = NewObject<UPointLightComponent>(Player, TEXT("IntegratedCaptureLight"));
			CaptureLight->SetupAttachment(Player->GetFirstPersonCameraComponent());
			CaptureLight->SetRelativeLocation(FVector(20.f, 18.f, -28.f));  // beside the held candle, never inside its head at the strike
			CaptureLight->SetIntensityUnits(ELightUnits::Lumens); CaptureLight->SetIntensity(0.f);
			CaptureLight->SetLightColor(FLinearColor(1.f, .62f, .32f)); CaptureLight->SetAttenuationRadius(250.f); CaptureLight->SetCastShadows(true);
			Player->AddInstanceComponent(CaptureLight); CaptureLight->RegisterComponent();
		}
	}
	if (Player)
		if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>()) Interaction->CancelInteract();
}

void ATallowPlayDirector::UpdateLessons(float DeltaSeconds)
{
	if (!Lessons || !Player) return;
	const auto* Candle = Player->GetCandleComponent();
	if (Candle->IsAcquired() && Candle->GetCandleState() == ECandleState::Cupped)
	{
		CupTime += DeltaSeconds;
		if (CupTime > .6f) Lessons->CompleteLessonIfActive(ETallowLesson::Cup);
	}
	else CupTime = 0.f;
	if (Candle->IsAcquired() && Candle->IsInDraft() && !Lessons->HasCompletedLesson(ETallowLesson::Cup))
	{
		DraftTime += DeltaSeconds;
		if (DraftTime < .35f) Lessons->ShowLesson(ETallowLesson::Draft);
		else if (Candle->GetCandleState() != ECandleState::Out)
		{
			Lessons->CompleteLessonIfActive(ETallowLesson::Draft); Lessons->ShowLesson(ETallowLesson::Cup);
		}
	}
	if (bKeeperAwake && Lessons->GetActiveLesson() == ETallowLesson::None && !Lessons->HasCompletedLesson(ETallowLesson::Stillness))
		Lessons->ShowLesson(ETallowLesson::Stillness);
	if (Player->IsHoldingStill()) StillTime += DeltaSeconds;
	if (StillTime >= .6f) Lessons->CompleteLessonIfActive(ETallowLesson::Stillness);
}

void ATallowPlayDirector::UpdatePlayerAnimation()
{
	if (!Presentation || !Presentation->IsPresentationEnabled()) return;
	const auto* Candle = Player->GetCandleComponent();
	HeldCandle->SetVisibility(Candle->IsAcquired());
	const auto Item = Player->GetCarriedItem();
	const bool bRelighting = ForcedClip == RelightClip && ForcedClip && GetWorld()->GetTimeSeconds() < ForcedUntil;
	if (HeldMatch) HeldMatch->SetVisibility(bRelighting);
	if (MatchFlame) MatchFlame->StandaloneState = bRelighting ? ECandleState::Lit : ECandleState::Out;
	if (OriginalHeldTool) OriginalHeldTool->SetHiddenInGame(true);
	if (HeldTool)
	{
		HeldTool->SetStaticMesh(Item == ECarriedItemType::Crowbar ? CrowbarMesh : Item == ECarriedItemType::Fuse ? FuseMesh : nullptr);
		HeldTool->SetRelativeTransform(Item == ECarriedItemType::Crowbar ? CrowbarHandTransform : FuseHandTransform);
		HeldTool->SetVisibility(Item != ECarriedItemType::None && !Player->IsTiedToChair() && !bRelighting && Candle->GetCandleState() != ECandleState::Cupped);
	}
	UAnimSequence* Clip = RestClip;
	bool bLoop = true;
	float Rate = 1.f;
	if (Player->IsTiedToChair() && BoundClip) Clip = BoundClip;
	else if (ForcedClip && GetWorld()->GetTimeSeconds() < ForcedUntil) { Clip = ForcedClip; bLoop = false; }
	else if (Candle->GetCandleState() == ECandleState::Cupped) Clip = CupClip;
	else if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>(); Interaction && Interaction->IsInteracting())
	{
		if (Item == ECarriedItemType::Crowbar) Clip = CrowbarPryClip ? CrowbarPryClip : ToolHoldClip;
		// One push-twist-click, stretched across the fuse box's hold instead of looping.
		else if (Item == ECarriedItemType::Fuse && FuseInsertClip)
		{
			Clip = FuseInsertClip; bLoop = false;
			Rate = FuseInsertClip->GetPlayLength() / FMath::Max(.1f, FuseBox ? FuseBox->InsertionHoldDuration : 1.5f);
		}
		else if (Item == ECarriedItemType::Fuse) Clip = ToolHoldClip;
		// One reach, grab, tug and release, stretched across the 1.4-2 s holds instead of restarting mid-grab.
		else { Clip = ReachClip; bLoop = false; Rate = .6f; }
	}
	else if (Candle->IsAcquired())
	{
		Clip = HoldClip;
		if (Player->GetCarriedItem() != ECarriedItemType::None && ToolHoldClip) Clip = ToolHoldClip;
		else if (Player->bIsCrouched && CrouchClip) Clip = CrouchClip;
		else if (Player->GetVelocity().Size2D() > 300.f && SprintClip) Clip = SprintClip;
		else if (Player->GetVelocity().Size2D() > 25.f && WalkClip) Clip = WalkClip;
	}
	if (Clip) Presentation->PlayPresentationClip(Clip, bLoop, Rate, .18f);
}

FText ATallowPlayDirector::GetObjective() const
{
	if (!Player || Scene != ETallowPlayScene::Game || IsMenuOpen()) return FText::GetEmpty();
	if (Player->IsTiedToChair()) return FText::FromString(TEXT("Find a way out of the chair."));
	if (!Player->GetCandleComponent()->IsAcquired()) return FText::FromString(TEXT("Take the candle."));
	if (!bBoardsCleared)
		return FText::FromString(Player->GetCarriedItem() == ECarriedItemType::Crowbar
			? TEXT("Pry the boards from the workshop door. Hold E.") : TEXT("Find something to pry the workshop boards."));
	if (!bPowerRestored)
	{
		if (Player->GetCarriedItem() == ECarriedItemType::Crowbar) return FText::FromString(TEXT("Put down the crowbar with Q. Search for the three fuses."));
		const FString Lead = Player->GetCarriedItem() == ECarriedItemType::Fuse
			? TEXT("Bring this fuse to the switchboard.  ") : TEXT("Search the house for fuses.  ");
		return FText::FromString(Lead + FString::Printf(TEXT("%d / 3 restored"), FuseBox ? FuseBox->CurrentFuses : 0));
	}
	return FText::FromString(TEXT("The attic release has power. Reach the collection upstairs."));
}

void ATallowPlayDirector::FinishRun()
{
	if (!CanEscape()) return;
	PlayCue(TEXT("P_Latch"), .7f);
	SetKeeperAwake(false);
	LoadDestination(EndingMap);
}

void ATallowPlayDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bLeaving || Scene != ETallowPlayScene::Game || !Player || UGameplayStatics::IsGamePaused(this)) return;
	const bool bTied = Player->IsTiedToChair();
	if (bWasTied && !bTied) OnChairEscaped();
	bWasTied = bTied;
	// The new reveal lowers and opens the head. Follow the evaluated head pose while
	// the existing game mode owns the capture input lock, fade and reset timing.
	if (bCaptureTracking && Keeper && PC && Keeper->GetMesh()->DoesSocketExist(TEXT("head")))
	{
		const FVector Face = Keeper->GetMesh()->GetSocketLocation(TEXT("head"));
		const FRotator Aim = (Face - Player->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation();
		// The view tightens as it lunges, a tremor builds as it closes, and the candle is thrust up into its face.
		CaptureTime += DeltaSeconds;
		// Creeping tension up to the strike, then a snap on it: tighter view, a jolt, the candle thrust at its face.
		const float Approach = FMath::Clamp(CaptureTime / FMath::Max(CaptureStrike, .01f), 0.f, 1.f);
		const float Hit = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp((CaptureTime - CaptureStrike) / .08f, 0.f, 1.f), 3.f);
		const float Jolt = Hit * FMath::Exp(-FMath::Max(0.f, CaptureTime - CaptureStrike) * 6.f);
		const float Tremor = .5f * Approach + .6f * Hit + 2.2f * Jolt;
		const FRotator Jitter(FMath::PerlinNoise1D(CaptureTime * 21.f) * Tremor, FMath::PerlinNoise1D(CaptureTime * 17.f + 5.f) * Tremor, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Aim, DeltaSeconds, 16.f) + Jitter);
		if (CaptureBaseFOV > 0.f) Player->GetFirstPersonCameraComponent()->SetFieldOfView(CaptureBaseFOV - 6.f * Approach - 16.f * Hit);
		if (CaptureLight) CaptureLight->SetIntensity((12.f * Approach + 23.f * Hit) * (.85f + .15f * FMath::PerlinNoise1D(CaptureTime * 30.f)));
		UpdatePlayerAnimation();
		return;
	}
	UpdateSound(); UpdateLessons(DeltaSeconds); UpdatePlayerAnimation();
}

void ATallowPlayDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(InitializeHandle);
	GetWorldTimerManager().ClearTimer(CatchHandle);
	GetWorldTimerManager().ClearTimer(BoardDropHandle);
	GetWorldTimerManager().ClearTimer(StingHandle);
	if (PowerHum) PowerHum->Stop();
	if (ActorSpawnedHandle.IsValid()) GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	if (SequencePlayer) { SequencePlayer->OnFinished.RemoveAll(this); SequencePlayer->Stop(); }
	if (Presentation) Presentation->DisablePresentation();
	if (Menu) Menu->RemoveFromParent();
	if (Lessons) Lessons->RemoveFromParent();
	if (RunUI) RunUI->RemoveFromParent();
	Super::EndPlay(Reason);
}

void ATallowPlayDirector::OnFuseInserted()
{
	if (FuseBox) { const FVector Box = FuseBox->GetActorLocation(); PlayCue(TEXT("P_Fuse_Insert"), .7f, &Box); }
}

USoundBase* ATallowPlayDirector::Cue(const FString& Name)
{
	if (const TObjectPtr<USoundBase>* Found = Cues.Find(Name)) return *Found;
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/CC0/%s.%s"), *Name, *Name));
	if (!Sound) UE_LOG(LogTemp, Warning, TEXT("TALLOW_INTEGRATION missing sound cue %s"), *Name);
	Cues.Add(Name, Sound);
	return Sound;
}

void ATallowPlayDirector::PlayCue(const FString& Name, float Volume, const FVector* Where)
{
	USoundBase* Sound = Cue(Name);
	if (!Sound) return;
	const float Pitch = FMath::FRandRange(.96f, 1.04f);
	if (Where) UGameplayStatics::PlaySoundAtLocation(this, Sound, *Where, Volume, Pitch, 0.f, CueAttenuation);
	else UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
}

void ATallowPlayDirector::AddFootsteps()
{
	CueAttenuation = NewObject<USoundAttenuation>(this);
	CueAttenuation->Attenuation.bAttenuate = true; CueAttenuation->Attenuation.bSpatialize = true;
	CueAttenuation->Attenuation.AttenuationShapeExtents = FVector(150.f); CueAttenuation->Attenuation.FalloffDistance = 1500.f;
	// The child: light socked steps, now and then a board giving under them.
	auto* Steps = NewObject<UTallowFootstepComponent>(Player, TEXT("IntegratedFootsteps"));
	for (int32 I = 0; I < 5; ++I) Steps->Steps.Add(Cue(FString::Printf(TEXT("P_Step_%02d"), I)));
	for (int32 I = 0; I < 3; ++I) Steps->Extras.Add(Cue(FString::Printf(TEXT("P_Creak_%02d"), I)));
	Steps->StrideCm = 50.f; Steps->Volume = .28f; Steps->ExtraChance = .12f; Steps->ExtraVolume = .2f;
	Player->AddInstanceComponent(Steps); Steps->RegisterComponent();
	// The Keeper: weight on old timber and its damp hem dragging behind, heard from the next room.
	if (Keeper)
	{
		auto* Heavy = NewObject<UTallowFootstepComponent>(Keeper, TEXT("IntegratedKeeperFootsteps"));
		for (int32 I = 0; I < 4; ++I) Heavy->Steps.Add(Cue(FString::Printf(TEXT("K_Step_%02d"), I)));
		for (int32 I = 0; I < 3; ++I) Heavy->Extras.Add(Cue(FString::Printf(TEXT("K_Drag_%02d"), I)));
		Heavy->StrideCm = 75.f; Heavy->Volume = .7f; Heavy->ExtraChance = .45f; Heavy->ExtraVolume = .5f; Heavy->FalloffCm = 1600.f;
		Keeper->AddInstanceComponent(Heavy); Heavy->RegisterComponent();
	}
	// Vasu's doors own their state; the director only listens to their hinges (and the route gates') swinging.
	for (TActorIterator<ATallowDoor> It(GetWorld()); It; ++It)
		if (!It->IsHidden())
			for (USceneComponent* Component : TInlineComponentArray<USceneComponent*>(*It))
				if (Component->GetName() == TEXT("HingePivot")) Hinges.Add(Component);
	for (TActorIterator<ATallowRouteGate> It(GetWorld()); It; ++It) if (It->Hinge) Hinges.Add(It->Hinge);
	for (USceneComponent* Hinge : Hinges) { HingeYaw.Add(FMath::Abs(Hinge->GetRelativeRotation().Yaw)); HingeMoving.Add(false); }
	LastCandleState = Player->GetCandleComponent()->GetCandleState();
	NextSettle = GetWorld()->GetTimeSeconds() + FMath::FRandRange(12.f, 25.f);
}

void ATallowPlayDirector::UpdateSound()
{
	for (int32 I = 0; I < Hinges.Num(); ++I)
	{
		if (!Hinges[I]) continue;
		const float Yaw = FMath::Abs(Hinges[I]->GetRelativeRotation().Yaw);
		const float Delta = Yaw - HingeYaw[I];
		if (!HingeMoving[I] && FMath::Abs(Delta) > .15f)
		{
			HingeMoving[I] = true;
			const FVector Where = Hinges[I]->GetComponentLocation();
			const int32 Variant = FMath::RandRange(0, 1);
			PlayCue(Delta > 0.f ? FString::Printf(TEXT("D_Open_%02d"), Variant) : FString::Printf(TEXT("D_Close_%02d"), Variant), .55f, &Where);
		}
		else if (HingeMoving[I] && FMath::Abs(Delta) < .01f) HingeMoving[I] = false;
		HingeYaw[I] = Yaw;
	}
	const uint8 Item = static_cast<uint8>(Player->GetCarriedItem());
	if (Item != LastItem && Player->GetCarriedItem() != ECarriedItemType::None)
		PlayCue(Player->GetCarriedItem() == ECarriedItemType::Crowbar ? TEXT("P_Pickup_Crowbar") : TEXT("P_Pickup_Fuse"), .5f);
	LastItem = Item;
	// The house settling somewhere nearby, rarely and quietly, so silence never feels safe.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextSettle)
	{
		const FVector Where = Player->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(500.f, 1100.f);
		PlayCue(FString::Printf(TEXT("H_Settle_%02d"), FMath::RandRange(0, 2)), .35f, &Where);
		NextSettle = Now + FMath::FRandRange(18.f, 40.f);
	}
}
