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
	InputComponent->BindKey(EKeys::V,IE_Pressed,this,&ATallowPlayDirector::ToggleBackgroundReading);
	InputComponent->BindKey(EKeys::BackSpace,IE_Pressed,this,&ATallowPlayDirector::SkipBackgroundReading);
	if (Scene != ETallowPlayScene::Game && PC->PlayerCameraManager)
	{
		// Setting the fade puts the sound back up, in case a capture's cut was still holding it down on the way here.
		PC->PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
		PC->PlayerCameraManager->StopCameraFade();
	}
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
	EndCaptureShot();
	bCaptureTracking = false;
	if (CaptureBaseFOV > 0.f && Player) Player->GetFirstPersonCameraComponent()->SetFieldOfView(CaptureBaseFOV);
	if (CaptureLight) CaptureLight->SetIntensity(0.f);
	if (auto* CandleFlame = Player ? Player->FindComponentByClass<UTallowCandleFlameComponent>() : nullptr) CandleFlame->LumensOverride = 0.f;
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
	// Everything hits on the variant's strike frame, not when the capture starts: the turn and the stare stay silent.
	CaptureStrike = 0.f;
	if (Keeper) CaptureStrike = Keeper->GetCaptureStrikeTime();
	// The procedural grab (no authored clip on V13) strikes as the mask opens.
	if (CaptureStrike <= .05f) CaptureStrike = .55f;
	CaptureCutAt = Keeper ? FMath::Max(Keeper->GetCapturePresentationDuration(), CaptureStrike + .1f) : CaptureStrike + .54f;
	// One hard sting, flat and on the frame: no pitch spread, nothing on top of it but her own scream.
	// My own big stinger (FinalPass/Capture); the old creature sting stays as the fallback.
	GetWorldTimerManager().SetTimer(StingHandle, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		static const TCHAR* Strike = TEXT("/Game/FinalPass/Capture/K_Capture_StrikeSting.K_Capture_StrikeSting");
		USoundBase* Sting = LoadObject<USoundBase>(nullptr, Strike, nullptr, LOAD_NoWarn | LOAD_Quiet);
		const float Volume = Sting ? CaptureStingVolume : .8f;
		if (!Sting) Sting = Cue(TEXT("K_Mim_Capture_Sting"));
		if (Sting) CaptureSting = UGameplayStatics::SpawnSound2D(this, Sting, Volume, 1.f, 0.f, nullptr, false, false);
		UE_LOG(LogTemp, Display, TEXT("MIM_CAPTURE sting %s volume %.2f at %.3fs"), *GetNameSafe(Sting), Volume, CaptureTime);
	}), CaptureStrike, false);
	// Stage her on the line she caught you from, at the distance her clip is authored for. She comes the rest of
	// the way while the view turns onto her, rather than appearing there.
	bCaptureGlide = false;
	bCaptureSquare = false;
	CaptureLevelDrop = CaptureOffAxisDegrees = 0.f;
	CaptureLensShift = FVector::ZeroVector; CaptureStareSide = CaptureStareRise = 0.f;
	if (Keeper && Player)
	{
		const FVector Eye = Player->GetFirstPersonCameraComponent()->GetComponentLocation();
		FVector Look = Keeper->GetActorLocation() - Eye; Look.Z = 0.f;
		if (Look.Normalize())
		{
			float Reach = 125.f; FHitResult Hit;
			if (const auto* Anim = Keeper->FindComponentByClass<UTallowCreatureAnimationComponent>()) Reach *= FMath::Clamp(Anim->KeeperMeshScale, .5f, 2.f);  // staged for a 1.0 scale Keeper
			FCollisionQueryParams Params(TEXT("CaptureStage"), false, Player); Params.AddIgnoredActor(Keeper);
			const auto* StageCapsule = Keeper->GetCapsuleComponent();
			// Short of a wall between you, she stays on your side of it.
			if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look * Reach, ECC_Visibility, Params))
				Reach = FMath::Max(StageCapsule->GetScaledCapsuleRadius() + 20.f, Hit.Distance - StageCapsule->GetScaledCapsuleRadius() - 6.f);
			const FVector Stage(Eye.X + Look.X * Reach, Eye.Y + Look.Y * Reach, Keeper->GetActorLocation().Z);
			const FQuat StageFacing = CaptureFacing(Keeper.Get(), -Look).Quaternion();
			// Wherever she ends up, the shot takes her straight on (UpdateCaptureShot); this is her front against her actor.
			CaptureFrontYaw = FRotator::NormalizeAxis((-Look).Rotation().Yaw - StageFacing.Rotator().Yaw);
			bCaptureSquare = true;
			const auto StageShape = FCollisionShape::MakeCapsule(StageCapsule->GetScaledCapsuleRadius(), StageCapsule->GetScaledCapsuleHalfHeight());
			// Never stage the grab inside furniture: an embedded mask reads as a
			// broken jumpscare (and can strand her off the navmesh afterwards).
			if (!GetWorld()->OverlapBlockingTestByProfile(Stage, StageFacing, StageCapsule->GetCollisionProfileName(), StageShape, Params))
			{
				FHitResult Path;
				const bool bPathBlocked = GetWorld()->SweepSingleByProfile(Path, Keeper->GetActorLocation(), Stage, StageFacing,
					StageCapsule->GetCollisionProfileName(), StageShape, Params);
				if (bPathBlocked)
					Keeper->SetActorLocationAndRotation(Stage, StageFacing.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
				else
				{
					CaptureGlideFrom = Keeper->GetActorLocation(); CaptureGlideTo = Stage;
					CaptureGlideFromRotation = Keeper->GetActorQuat(); CaptureGlideToRotation = StageFacing;
					bCaptureGlide = true;
				}
				UE_LOG(LogTemp, Display, TEXT("MIM_CAPTURE stage=%.1fcm from eye glide=%d scale=%.3f strike=%.3f cut=%.3f"),
					Reach, bCaptureGlide ? 1 : 0, Keeper->GetActorScale3D().GetAbsMin(), CaptureStrike, CaptureCutAt);
			}
			else
			{
				// No room to stage her: she stays where she is but still turns to face you (in place, so nothing can block it).
				CaptureGlideFrom = CaptureGlideTo = Keeper->GetActorLocation();
				CaptureGlideFromRotation = Keeper->GetActorQuat(); CaptureGlideToRotation = StageFacing;
				bCaptureGlide = true;
				UE_LOG(LogTemp, Warning, TEXT("MIM_CAPTURE floor stage blocked; preserving physical capsule, turning in place"));
			}
		}
	}
	if (Player)
	{
		auto* Camera = Player->GetFirstPersonCameraComponent();
		CaptureBaseFOV = Camera->FieldOfView;
        if (!bCaptureCameraBaseValid)
        {
            CaptureBaseCameraLocation = Camera->GetRelativeLocation();
            CaptureBaseCameraRotation = Camera->GetRelativeRotation();
            bCaptureCameraBaseValid = true;
        }
        CaptureStartRotation = CaptureAim = Camera->GetComponentQuat();
        // The shot is taken from where the eye was when she caught you, the point her stage is measured from.
        // The body turning round would otherwise carry the camera's forward offset up to twice its length toward her.
        CaptureBaseEye = Camera->GetComponentLocation();
        CaptureFloorZ = Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // Through the shot the lens is placed against the pose she is actually drawn in this frame, not the last
        // one: her head can move several centimetres a frame in the shakes. Cameras update after every tick.
        if (Keeper) AddTickPrerequisiteComponent(Keeper->GetMesh());
        // The turn is quick for a glance and never slower than a third of a second for a full about-face.
        float FaceWidth = 65.f;
        const FVector Face = Keeper && Keeper->GetMesh()->DoesSocketExist(TEXT("head"))
            ? CaptureFaceCenter(Keeper->GetMesh(), FaceWidth) : Camera->GetComponentLocation() + Camera->GetForwardVector() * 100.f;
        const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Camera->GetForwardVector(),
            (Face - Camera->GetComponentLocation()).GetSafeNormal()), -1.f, 1.f)));
        CaptureTurnSeconds = FMath::Clamp(Angle / FMath::Max(CaptureTurnDegreesPerSecond, 1.f), CaptureTurnMinSeconds, CaptureTurnMaxSeconds);
        bCaptureInDarkness = Player->GetCandleComponent()->GetCandleState() == ECandleState::Out;
        CapturePullDistance = CaptureAimErrorDegrees = 0.f;
        CaptureLensClearance = 0.f;
        bCaptureHandsBaseValid = Presentation && Presentation->IsPresentationEnabled();
        if (bCaptureHandsBaseValid) CaptureHandsBase = Presentation->HandsRelativeTransform;
		if (!CaptureLight)
		{
			// The light the shot is seen by when the candle is out. It sits low and to the candle side of the
			// lens (about 30 degrees off the axis at her face), so her face gets shape and falloff instead of flat
			// front fill, and it never ends up inside her hands or her face at the strike.
			CaptureLight = NewObject<UPointLightComponent>(Player, TEXT("IntegratedCaptureLight"));
			CaptureLight->SetupAttachment(Camera);
			CaptureLight->SetRelativeLocation(FVector(2.f, 18.f, -18.f));
			CaptureLight->SetIntensityUnits(ELightUnits::Lumens); CaptureLight->SetIntensity(0.f);
			CaptureLight->SetLightColor(FLinearColor(1.f, .62f, .32f)); CaptureLight->SetAttenuationRadius(250.f); CaptureLight->SetCastShadows(true);
			Player->AddInstanceComponent(CaptureLight); CaptureLight->RegisterComponent();
		}
		UE_LOG(LogTemp, Display, TEXT("MIM_CAPTURE turn=%.3fs dark=%d fov=%.1f"), CaptureTurnSeconds, bCaptureInDarkness ? 1 : 0, CaptureBaseFOV);
	}
	if (Player)
		if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>()) Interaction->CancelInteract();
}

float ATallowPlayDirector::MeasureCaptureLensClearance(const FVector& Eye, bool bGripping) const
{
	if (!Keeper) return TNumericLimits<float>::Max();
	const auto* Mesh = Keeper->GetMesh();
	const float Rig = Mesh->GetComponentScale().GetAbsMin();
	float Clearance = TNumericLimits<float>::Max();
	auto Segment = [&](const FName From, const FName To, float RadiusRigCm)
	{
		if (!Mesh->DoesSocketExist(From) || !Mesh->DoesSocketExist(To)) return;
		const FVector A = Mesh->GetSocketLocation(From), B = Mesh->GetSocketLocation(To);
		Clearance = FMath::Min(Clearance, float(FMath::PointDistToSegment(Eye, A, B)) - RadiusRigCm * Rig);
	};
	// Bone capsules from the export (Final01/Export/mim-capsules.json, rig cm) with 16 cm of coat on every arm
	// segment and hand; the fingers carry 5.5 cm cards. All of it scales with her.
	static const float UpperRadius[] = {22.7f, 13.2f, 10.2f}, ForeRadius[] = {9.2f, 10.7f, 13.4f}, HandRadius[] = {7.9f, 9.1f, 11.5f};
	constexpr float Coat = 16.f;
	for (int32 Pair = 1; Pair <= 3; ++Pair)
		for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
		{
			const auto Bone = [&](const TCHAR* Part) { return FName(*FString::Printf(TEXT("arm%02d_%s_%s"), Pair, Part, Side)); };
			Segment(Bone(TEXT("upper")), Bone(TEXT("fore")), UpperRadius[Pair - 1] + Coat);
			// Once the four front hands hold the frame they are placed from the lens itself and keep their own
			// distance (each lets go if it comes inside the keep-out); moving the lens away from them would only
			// drag them after it.
			if (bGripping && Pair < 3) continue;
			Segment(Bone(TEXT("fore")), Bone(TEXT("hand")), ForeRadius[Pair - 1] + Coat);
			Segment(Bone(TEXT("hand")), Bone(TEXT("hand")), HandRadius[Pair - 1] + Coat);
			for (int32 Digit = 1; Digit <= 5; ++Digit)
				Segment(Bone(TEXT("hand")), FName(*FString::Printf(TEXT("arm%02d_d%d_03_%s"), Pair, Digit, Side)), HandRadius[Pair - 1] * .6f + 5.5f);
		}
	// The face: the mask is about 20 cm proud of the eyes and the open jaw drops below it.
	// Her nose and teeth sit 22 rig cm in front of the eyes, the dropped lower teeth 24 in front of the jaw.
	Segment(TEXT("eye_l"), TEXT("eye_r"), 30.f);
	Segment(TEXT("jaw"), TEXT("eye_l"), 30.f);
	Segment(TEXT("jaw"), TEXT("eye_r"), 30.f);
	return Clearance;
}

void ATallowPlayDirector::UpdateCaptureShot(float DeltaSeconds)
{
	auto* Camera = Player->GetFirstPersonCameraComponent();
	CaptureTime += DeltaSeconds;
	const float T = CaptureTime;
	const float Scale = Keeper->GetActorScale3D().GetAbsMin();
	const FVector BaseEye = CaptureBaseEye;
	// Beats, in seconds from the capture (the clip's frames at 24 fps): the turn [0, Turn]; the dead-still stare
	// from frame 8; the drive from frame 13 to the strike on frame 17, which takes the lens 15 S cm toward her and
	// 10 S cm down; the hold, her hands drawing it in another 8 S cm until the cut.
	const float StareAt = 8.f / 24.f, StrikeLead = 4.f / 24.f;
	float FaceWidth = 65.f;
	if (bCaptureGlide)
	{
		const float Glide = FMath::SmoothStep(0.f, FMath::Max(CaptureGlideSeconds, .01f), T);
		Keeper->SetActorLocationAndRotation(FMath::Lerp(CaptureGlideFrom, CaptureGlideTo, Glide),
			FQuat::Slerp(CaptureGlideFromRotation, CaptureGlideToRotation, Glide), false, nullptr, ETeleportType::TeleportPhysics);
		bCaptureGlide = Glide < 1.f;
	}
	CaptureFaceFocus = CaptureFaceCenter(Keeper->GetMesh(), FaceWidth);
	const float Taken = FMath::Pow(FMath::Clamp((T - (CaptureStrike - StrikeLead)) / StrikeLead, 0.f, 1.f), 2.f);
	const float Hold = 1.f - FMath::Pow(1.f - FMath::Clamp((T - CaptureStrike) / FMath::Max(CaptureCutAt - CaptureStrike, .05f), 0.f, 1.f), 1.6f);
	FVector Axis = (CaptureFaceFocus - BaseEye).GetSafeNormal2D();
	if (Axis.IsNearlyZero()) Axis = Camera->GetForwardVector().GetSafeNormal2D();
	// Rig metres to world cm: the shot keeps its proportions at any size.
	const float RigMetre = 100.f * Scale;
	// Straight on and level (29 Oct: "it was slightly on the side"). Her face is not on her own centre line
	// (the clip carries the head a little to one side and the stare tilts it), and Nell's eye is 139 cm off the
	// floor where the clips hold the face well under that at the game's size, so the view met her about 9 degrees
	// off her front and tipped down onto her. The lens comes onto the line straight out of her front through her
	// face, at her face's height: a few centimetres across and a sink, as if the child's knees go. Her body is not
	// turned for it (a turn feeds her movement's lean and rocks the head). Eased in over the rise and held from the
	// drive on: the strike is drawn round the clip's own lens, on her centre line (her four hands take the frame
	// corners round it), so the lens goes back onto that line through the drive and keeps the stare's height.
	// Following the face any further, this close in, only chases a head that is aiming at the lens, and took the
	// lens down into her hood's shadow. Never lower than a child sat on the floor, never higher than onto her toes.
	if (bCaptureSquare)
	{
		const FVector Front = FRotator(0.f, Keeper->GetActorRotation().Yaw + CaptureFrontYaw, 0.f).Vector();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Front);
		if (T < CaptureStrike - StrikeLead)
		{
			const float Lowest = FMath::Min(0.f, CaptureFloorZ + CaptureLevelMinEyeHeight - float(BaseEye.Z));
			CaptureStareSide = FMath::Clamp(float(FVector::DotProduct(CaptureFaceFocus - BaseEye, Across)), -40.f * Scale, 40.f * Scale);
			CaptureStareRise = FMath::Clamp(float(CaptureFaceFocus.Z - BaseEye.Z), Lowest, CaptureLevelMaxRise);
		}
		const float Side = CaptureStareSide * (1.f - Taken), Rise = CaptureStareRise;
		const float Settle = FMath::SmoothStep(0.f, FMath::Max(StareAt, CaptureTurnSeconds), T);
		CaptureLensShift = FMath::VInterpTo(CaptureLensShift, (Across * Side + FVector::UpVector * Rise) * Settle, DeltaSeconds, 18.f);
		CaptureLevelDrop = CaptureLensShift.Z;
	}
	FVector Eye = BaseEye + CaptureLensShift
		+ Axis * RigMetre * (.15f * Taken + .08f * Hold) - FVector::UpVector * RigMetre * (.10f * Taken + .02f * Hold);
	FCollisionQueryParams CameraQuery(TEXT("CaptureCameraPull"), false, Player);
	CameraQuery.AddIgnoredActor(Keeper);
	FHitResult CameraBlock;
	if (GetWorld()->SweepSingleByChannel(CameraBlock, BaseEye, Eye, FQuat::Identity,
		ECC_Camera, FCollisionShape::MakeSphere(4.f), CameraQuery)) Eye = CameraBlock.Location;
	// Her face never comes nearer the lens than 60 S cm (the teeth then stay about 40 S off): whatever her body
	// does, the shot keeps its framing and the lens gives way smoothly rather than in jumps.
	const float FaceFloor = 60.f * Scale;
	if (FVector::Distance(Eye, CaptureFaceFocus) < FaceFloor)
		Eye = CaptureFaceFocus + (Eye - CaptureFaceFocus).GetSafeNormal() * FaceFloor;
	// Nothing of her inside the keep-out: give the lens back along its path until she clears it.
	const bool bGripping = T >= CaptureStrike - StrikeLead;
	CaptureLensClearance = MeasureCaptureLensClearance(Eye, bGripping);
	for (int32 Step = 0; Step < 8 && CaptureLensClearance < CaptureLensKeepOut; ++Step)
	{
		const FVector Back = (Eye - CaptureFaceFocus).GetSafeNormal();
		Eye += Back * (CaptureLensKeepOut - CaptureLensClearance + 1.f);
		CaptureLensClearance = MeasureCaptureLensClearance(Eye, bGripping);
	}
	CapturePullDistance = FVector::Distance(BaseEye, Eye);
	// The turn onto her face, then the aim stays on it with a frame of weight behind it.
	const FQuat Target = (CaptureFaceFocus - Eye).Rotation().Quaternion();
	if (T < CaptureTurnSeconds)
	{
		const float X = FMath::Clamp(T / CaptureTurnSeconds, 0.f, 1.f);
		CaptureAim = FQuat::Slerp(CaptureStartRotation, Target, X * X * X * (X * (X * 6.f - 15.f) + 10.f));
	}
	else CaptureAim = FMath::QInterpTo(CaptureAim, Target, DeltaSeconds, 24.f);
	// The horizon stays level: blending between two aims picks up roll when both pitch and yaw change.
	FRotator Level = CaptureAim.Rotator(); Level.Roll = 0.f; CaptureAim = Level.Quaternion();
	// The stare is dead still. The strike lands as one 3 degree jolt; the hold shakes with her head.
	const float SinceHit = T - CaptureStrike;
	const float Jolt = SinceHit >= 0.f ? FMath::Exp(-SinceHit * 12.f) : 0.f;
	const float Shake = SinceHit >= 0.f ? FMath::Exp(-SinceHit * 2.2f) : 0.f;
	const FRotator Kick(3.f * Jolt * FMath::Cos(SinceHit * 40.f),
		.8f * Jolt * FMath::Sin(SinceHit * 71.f + 1.8f),
		2.f * Jolt * FMath::Sin(SinceHit * 23.f + 1.f) + .35f * Shake * FMath::Sin(UE_TWO_PI * 2.6f * FMath::Max(0.f, SinceHit)));
	const FRotator Final = (CaptureAim * Kick.Quaternion()).Rotator();
	PC->SetControlRotation(Final);
	Camera->SetWorldRotation(Final);
	Camera->SetWorldLocation(Eye);
	const FVector ActualAim = (CaptureFaceFocus - Eye).GetSafeNormal();
	CaptureAimErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Camera->GetForwardVector(), ActualAim), -1.f, 1.f)));
	if (bCaptureSquare)
		CaptureOffAxisDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Camera->GetForwardVector().GetSafeNormal2D(),
			-FRotator(0.f, Keeper->GetActorRotation().Yaw + CaptureFrontYaw, 0.f).Vector()), -1.f, 1.f)));
	if (CaptureBaseFOV > 0.f)
	{
		// 90 -> 86 creeping in through the stare, the punch to 74 on the strike, 76 at the cut (from a base of 90).
		const float Creep = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((T - CaptureTurnSeconds) / FMath::Max(CaptureStrike - StrikeLead - CaptureTurnSeconds, .05f), 0.f, 1.f));
		const float Fov = CaptureBaseFOV - 4.f * Creep - 12.f * Taken + 2.f * Hold;
		Camera->SetFieldOfView(FMath::Clamp(Fov, 60.f, 100.f));
	}
	// Candle out: no light at all until the strike, then a 6 lm flash that settles to 2.
	if (CaptureLight) CaptureLight->SetIntensity(bCaptureInDarkness && SinceHit >= 0.f
		? 2.f + 4.f * FMath::Exp(-SinceHit / .15f) : 0.f);
	// Her face comes closer than the held candle: keep the flame's light in front of the lens, and with the candle
	// lit give it the same few lumens (6 into the strike, settling to 2), so her face is candle-lit, not blown out.
	if (auto* CandleFlame = Player->FindComponentByClass<UTallowCandleFlameComponent>())
	{
		const bool bAtLens = T >= CaptureStrike - StrikeLead;
		CandleFlame->ViewHoldDistance = bAtLens ? 16.f : 0.f;
		CandleFlame->LumensOverride = bCaptureInDarkness || !bAtLens ? 0.f : SinceHit >= 0.f ? 2.f + 4.f * FMath::Exp(-SinceHit / .15f) : 6.f;
	}
	// Nell's hands drop out of the shot as she is turned round.
	if (bCaptureHandsBaseValid && Presentation)
	{
		const float Drop = FMath::SmoothStep(0.f, CaptureTurnSeconds + .05f, T);
		FTransform Hands = CaptureHandsBase;
		Hands.AddToTranslation(FVector(-12.f, 0.f, -38.f) * Drop);
		Presentation->UpdateHandsTransform(Hands);
	}
	UpdatePlayerAnimation();
}

float ATallowPlayDirector::GetScreenFade() const
{
	const auto* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	return Camera && Camera->bEnableFading ? Camera->FadeAmount : 0.f;
}

void ATallowPlayDirector::EndCaptureShot()
{
	if (Keeper) RemoveTickPrerequisiteComponent(Keeper->GetMesh());
	GetWorldTimerManager().ClearTimer(StingHandle);
	if (CaptureSting) { CaptureSting->Stop(); CaptureSting = nullptr; }
	bCaptureGlide = false;
	if (bCaptureHandsBaseValid && Presentation) Presentation->UpdateHandsTransform(CaptureHandsBase);
	bCaptureHandsBaseValid = false;
	if (Player)
		if (auto* CandleFlame = Player->FindComponentByClass<UTallowCandleFlameComponent>()) CandleFlame->ViewHoldDistance = 0.f;
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
		// The idle hand sway ran too fast for me: half speed.
		Rate = .5f;
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
	// The capture shot: the game mode owns the input lock, the cut and the reset timing.
	if (bCaptureTracking && Keeper && PC && Keeper->GetMesh()->DoesSocketExist(TEXT("head")))
	{
		UpdateCaptureShot(DeltaSeconds);
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
