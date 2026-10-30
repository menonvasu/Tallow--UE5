#include "Presentation/TallowPlayDirector.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Save/TallowSaveSubsystem.h"
#include "Presentation/TallowRunWidget.h"
#include "UI/Presentation/TallowPaper.h"
#include "UI/Presentation/TallowInventoryWidget.h"
#include "UI/Presentation/TallowJournalWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "AI/TallowCreature.h"
#include "Animation/AnimInstance.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "GameMode/TallowGameMode.h"
#include "Interactable/TallowInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Player/Presentation/TallowToolContactComponent.h"
#include "Player/Map/TallowHouseMapComponent.h"
#include "Player/TallowCharacter.h"
#include "Tasks/TallowChairEscape.h"
#include "Tasks/TallowCrowbarPickup.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowFusePickup.h"
#include "Tasks/TallowProjector.h"
#include "Tasks/TallowFilmReelPickup.h"
#include "Tasks/TallowLockpickGame.h"
#include "Tasks/TallowPlank.h"
#include "UI/Presentation/TallowLessonWidget.h"
#include "UI/Presentation/TallowMatchCountWidget.h"
#include "UI/Presentation/TallowCandleStatusWidget.h"
#include "UI/Presentation/TallowSlidesWidget.h"
#include "Audio/TallowCorruptedVoice.h"
#include "UI/Presentation/TallowSubtitles.h"
#include "Animation/AnimSequence.h"
#include "Animation/Presentation/TallowPresentationAnimInstance.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "Audio/TallowFootstepComponent.h"
#include "Audio/TallowSoundscapeComponent.h"
#include "Audio/TallowOpeningMusicSubsystem.h"
#include "Components/AudioComponent.h"
#include "Environment/TallowDoor.h"
#include "Presentation/TallowRouteGate.h"
#include "Environment/TallowCandlePickup.h"
#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Presentation/TallowNellNotes.h"
#include "Player/Presentation/TallowBreathFeedbackComponent.h"
#include "Misc/PackageName.h"
#include "Environment/TallowCollectionSlot.h"
#include "Environment/TallowMatchbox.h"
#include "Environment/TallowPickupSpot.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Engine/SkeletalMesh.h"

namespace
{
FRotator CaptureFacing(const ATallowCreature* Creature, const FVector& TowardPlayer)
{
    if (Creature->UsesCanonicalCaptureBasis()) return FRotator(0.f, TowardPlayer.Rotation().Yaw, 0.f);
    const auto* Mesh = Creature->GetMesh();
    const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset || Asset->GetFName() != TEXT("SK_Mim")) return TowardPlayer.Rotation();
    const auto& Skeleton = Asset->GetRefSkeleton();
    const auto& Poses = Skeleton.GetRefBonePose();
    auto ReferencePoint = [&](FName Name)
    {
        int32 Index = Skeleton.FindBoneIndex(Name);
        if (Index == INDEX_NONE) return FVector::ZeroVector;
        FTransform Transform = Poses[Index];
        while ((Index = Skeleton.GetParentIndex(Index)) != INDEX_NONE) Transform *= Poses[Index];
        return Transform.GetLocation();
    };
    const FVector Eyes = (ReferencePoint(TEXT("eye_l")) + ReferencePoint(TEXT("eye_r"))) * .5f;
    const FVector LocalFace = Mesh->GetRelativeTransform().TransformVectorNoScale(
        Eyes - ReferencePoint(TEXT("head"))).GetSafeNormal2D();
    return FRotator(0.f, TowardPlayer.Rotation().Yaw - LocalFace.Rotation().Yaw, 0.f);
}

FVector CaptureFaceCenter(const USkeletalMeshComponent* Mesh, float& Width)
{
    const FVector Head = Mesh->GetSocketLocation(TEXT("head"));
    Width = 65.f;
    if (!Mesh->DoesSocketExist(TEXT("eye_l")) || !Mesh->DoesSocketExist(TEXT("eye_r"))) return Head;
    const FVector Left = Mesh->GetSocketLocation(TEXT("eye_l")), Right = Mesh->GetSocketLocation(TEXT("eye_r"));
    const FVector Eyes = (Left + Right) * .5f;
    Width = FMath::Clamp(FVector::Distance(Left, Right) * 3.2f, 36.f, 120.f);
    if (!Mesh->DoesSocketExist(TEXT("jaw"))) return Eyes;
    const FVector Forward = (Eyes - Head).GetSafeNormal();
    const FVector Jaw = Mesh->GetSocketLocation(TEXT("jaw"));
    const FVector MouthPlane = Jaw + Forward * FVector::DotProduct(Eyes - Jaw, Forward);
    const float Opening = Mesh->GetAnimInstance()
        ? FMath::Clamp(Mesh->GetAnimInstance()->GetCurveValue(TEXT("JawOpen")), 0.f, 1.2f) : 0.f;
    // Low enough to take in the dropped jaw, high enough that the eyes stay whole in the strike's close frame.
    return FMath::Lerp(Eyes, MouthPlane, .45f)
        - Mesh->GetOwner()->GetActorUpVector() * Width * (.08f + .05f * Opening);
}
}

namespace
{
	// Presentation can be built against both the existing gameplay creature and the newer ceiling-capture adapter.
	template<typename T> auto IsCeilingCapture(T* Creature,int) -> decltype(Creature->IsCeilingCapturePresentation(),bool()) { return Creature->IsCeilingCapturePresentation(); }
	template<typename T> bool IsCeilingCapture(T*,long) { return false; }
}

ATallowPlayDirector::ATallowPlayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATallowPlayDirector::BeginPlay()
{
	Super::BeginPlay();
	// The game map opens on black and fades in once the player is set up in the chair
	// (InitializeGameplay), so the first frames never show an unplaced view.
	if (Scene == ETallowPlayScene::Game)
		if (APlayerController* FirstPC = UGameplayStatics::GetPlayerController(this, 0))
			if (FirstPC->PlayerCameraManager) FirstPC->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
	GetWorldTimerManager().SetTimer(InitializeHandle, this, &ATallowPlayDirector::InitializeScene, .15f, false);
}

void ATallowPlayDirector::InitializeScene()
{
	for(const FName Name:{FName(TEXT("Notebook")),FName(TEXT("Parchment")),FName(TEXT("Charcoal")),FName(TEXT("Selection")),FName(TEXT("JournalDrawing")),FName(TEXT("Eye")),FName(TEXT("Ear")),FName(TEXT("Heart")),FName(TEXT("ChapterDoor"))}) TallowPaper::Art(Name);

	PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->IsLocalController()) return;
	EnableInput(PC);
	InputComponent->Priority = 10;
	auto& PauseKey = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATallowPlayDirector::TogglePause);
	PauseKey.bExecuteWhenPaused = true;
	PauseKey.bConsumeInput = false;
	InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ATallowPlayDirector::OpenInventory);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATallowPlayDirector::OpenInventory);
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
		// Both 1974 endings are their own illustrated sequence; the retired rooftop movie belongs to the earlier story.
		OnSequenceFinished();
	}
	else InitializeGameplay();
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION ready scene=%d"), static_cast<int32>(Scene));
}

void ATallowPlayDirector::InitializeGameplay()
{
	// A fresh house (a new run or a restart): Maisie's candle is in the cellar again, whatever the last run chose.
	if (auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>()) Story->ResetHouse();
	Player = Cast<ATallowCharacter>(PC->GetPawn());
	Keeper = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()));
	FuseBox = Cast<ATallowFuseBox>(UGameplayStatics::GetActorOfClass(this, ATallowFuseBox::StaticClass()));
	if (!Player) { UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION missing existing player")); return; }
	bWasTied = Player->IsTiedToChair();
	PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false;
	Lessons = CreateWidget<UTallowLessonWidget>(PC);
	Lessons->AddToViewport(12); Lessons->ShowLesson(ETallowLesson::Chair);
	if (!bScreenInstructions) Lessons->SetVisibility(ESlateVisibility::Collapsed);
	RunUI = CreateWidget<UTallowRunWidget>(PC);
	RunUI->Director = this; RunUI->AddToViewport(8);
	MatchUI = CreateWidget<UTallowMatchCountWidget>(PC);
	MatchUI->AddToViewport(9);
	CandleStatusUI=CreateWidget<UTallowCandleStatusWidget>(PC);
	CandleStatusUI->Setup(Player->GetCandleComponent());CandleStatusUI->AddToViewport(9);
	if (!FilmReelMesh) FilmReelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Presentation/ProjectorFinal/SM_FilmReel.SM_FilmReel"));
	for (TActorIterator<ATallowFusePickup> It(GetWorld()); It; ++It) ApplyPickupVisual(*It);
	for (TActorIterator<ATallowCrowbarPickup> It(GetWorld()); It; ++It) ApplyPickupVisual(*It, true);
	// After the visuals: a spot seats the finished mesh, not the Blueprint's stand-in.
	RandomizePickups();
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ATallowPlayDirector::OnActorSpawned));
	BindGameplayEvents();
	AddFootsteps();
	Soundscape = NewObject<UTallowSoundscapeComponent>(this, TEXT("Soundscape"));
	Soundscape->RegisterComponent();
	Soundscape->Setup(Player, Keeper);
	// What a first-time player has to understand, told once by Nell, and her held breath made something you feel.
	auto* NellNotes = NewObject<UTallowNellNotesComponent>(this, TEXT("NellNotes"));
	AddInstanceComponent(NellNotes); NellNotes->RegisterComponent(); NellNotes->Setup(this, Player, Keeper, Soundscape);
	auto* Breath = NewObject<UTallowBreathFeedbackComponent>(Player, TEXT("BreathFeedback"));
	Player->AddInstanceComponent(Breath); Breath->RegisterComponent(); Breath->Setup(Player);
	if (auto* Waltz = UTallowOpeningMusicSubsystem::GetOpeningMusic(this)) Waltz->NoteHouseBegun(Player->IsTiedToChair());
	SetKeeperAwake(false);
	SetPowerVisuals(false);
	if (UTallowChapterSubsystem::ShouldRunChapters())
		if (auto* Chapters = GetWorld()->GetSubsystem<UTallowChapterSubsystem>()) Chapters->Activate(this);
	bWasTied = Player->IsTiedToChair();
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
		FilmReelHandTransform = (FilmReelCameraTransform * Camera).GetRelativeTransform(Hand);
		HeldTool = NewObject<UStaticMeshComponent>(Player, TEXT("IntegratedHeldTool"));
		HeldTool->SetupAttachment(Hands,TEXT("hand_l"));
		HeldTool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldTool->SetOnlyOwnerSee(true); HeldTool->SetCastShadow(false);
		HeldTool->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		// The hands and the candle sit on lighting channel 1, lit by the presentation light. The tool was left on
		// channel 0, so the real candle light a few centimetres away blew the crowbar out to flat white.
		HeldTool->SetLightingChannels(false, true, false);
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
	HeldCandle->SetVisibility(Player->GetCandleComponent()->IsAcquired() && !Player->GetCandleComponent()->IsGivenUp());
	auto* Contact=NewObject<UTallowToolContactComponent>(Player,TEXT("ToolContact"));
	Player->AddInstanceComponent(Contact);Contact->RegisterComponent();Contact->Setup(Player,Presentation,HeldTool);
	UpdatePlayerAnimation();
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FColor::Black, false, false);
}

void ATallowPlayDirector::ApplyPickupVisual(AActor* Actor, bool bPlaceOnSurface)
{
	if (!IsValid(Actor)) return;
	UStaticMesh* Mesh = Actor->IsA<ATallowFusePickup>() ? FuseMesh.Get() : Actor->IsA<ATallowCrowbarPickup>() ? CrowbarMesh.Get() : nullptr;
	if (!Mesh) return;
	auto* Visual = Actor->FindComponentByClass<UStaticMeshComponent>();
	// A new run moves the existing pickup back to a surface marker. Its mesh
	// may already be correct; its pivot height and resting rotation still need seating.
	if (!Visual || (Visual->GetStaticMesh() == Mesh && !bPlaceOnSurface)) return;
	FVector Surface = Actor->GetActorLocation();
	// A dropped one is spawned 2 cm off the top it was dropped on: set it down on that top, not above it.
	if (!bPlaceOnSurface)
	{
		FHitResult Hit; FCollisionQueryParams Params(TEXT("PickupDrop"), true, Actor);
		Params.AddIgnoredActor(Actor->GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Hit, Surface + FVector(0,0,1), Surface - FVector(0,0,6), ECC_Visibility, Params)) Surface.Z = Hit.ImpactPoint.Z;
	}
	// Lying the way it lies on an authored spot, turned to where it was dropped.
	const FRotator Rotation = (FRotator(0.f,Actor->GetActorRotation().Yaw,0.f).Quaternion()
		* ATallowPickupSpot::RestingRotation(Actor->IsA<ATallowCrowbarPickup>() ? ETallowPickupSpotItem::Crowbar : ETallowPickupSpotItem::Fuse).Quaternion()).Rotator();
	const FBox Bounds = Mesh->GetBoundingBox().TransformBy(FTransform(Rotation));
	Visual->SetStaticMesh(Mesh);
	Visual->EmptyOverrideMaterials();
	Actor->SetActorScale3D(FVector::OneVector);
	Actor->SetActorLocationAndRotation(Surface + FVector(0,0,-Bounds.Min.Z + (bPlaceOnSurface ? .08f : 0.f)),Rotation);
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
		if(It->bRequiredForEscape) { ++TotalPlanks; It->OnPlankDestroyed.AddUniqueDynamic(this, &ATallowPlayDirector::OnPlankRemoved); }
		else It->OnPlankDestroyed.AddUniqueDynamic(this,&ATallowPlayDirector::OnOptionalPlankRemoved);
	}
	if (FuseBox) FuseBox->OnPowerRestored.AddUniqueDynamic(this, &ATallowPlayDirector::OnPower);
	if (FuseBox) FuseBox->OnFuseInserted.AddUniqueDynamic(this, &ATallowPlayDirector::OnFuseInserted);
	if (auto* Candle = Player->GetCandleComponent())
	{
		Candle->OnAcquireCandle.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleAcquired);
		Candle->OnCandleReset.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleReset);
		Candle->OnCandleStateChanged.AddUniqueDynamic(this, &ATallowPlayDirector::OnCandleStateChanged);
		Candle->OnRelightStarted.AddUniqueDynamic(this, &ATallowPlayDirector::OnRelight);
		if (MatchUI) Candle->OnMatchesChanged.AddUniqueDynamic(MatchUI.Get(), &UTallowMatchCountWidget::ShowCount);
	}
	Player->OnStill.AddUniqueDynamic(this, &ATallowPlayDirector::OnStill);
	if (UTallowLockpickGame* Lock = Player->GetLockpickComponent())
	{
		Lock->OnPinSet.AddUniqueDynamic(this, &ATallowPlayDirector::OnLockPin);
		Lock->OnFalseSet.AddUniqueDynamic(this, &ATallowPlayDirector::OnLockFalseSet);
		Lock->OnSnap.AddUniqueDynamic(this, &ATallowPlayDirector::OnLockSnap);
		Lock->OnLockpickSet.AddUniqueDynamic(this, &ATallowPlayDirector::OnLockOpen);
	}
	if (auto* GM = GetWorld()->GetAuthGameMode<ATallowGameMode>()) { GM->OnCaught.AddUniqueDynamic(this, &ATallowPlayDirector::OnCaught); GM->OnCapturePresented.AddUniqueDynamic(this, &ATallowPlayDirector::ShowCaughtMenu); }
	if (auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>())
		Story->OnLoreRead.AddWeakLambda(this, [this](FName Key) { OnLoreRead(Key); });
}

void ATallowPlayDirector::RandomizePickups()
{
	ActiveRunSeed = RunSeed ? RunSeed : FMath::Rand();
	FRandomStream Random(ActiveRunSeed);
	// Every pickup starts the run on an authored spot, a real top it rests on (28 Oct). A spot is taken at
	// most once, so two matchboxes never share one.
	TArray<ATallowPickupSpot*> Taken;
	auto Place = [&](AActor* Item, ETallowPickupSpotItem Kind, FName Floor, const TCHAR* What)
	{
		ATallowPickupSpot* Spot = ATallowPickupSpot::PickSpot(this, Kind, Floor, {}, Taken, Random);
		if (!Spot) { UE_LOG(LogTemp, Error, TEXT("TALLOW_INTEGRATION no pickup spot left for the %s (%s)"), What, *Floor.ToString()); return; }
		Taken.Add(Spot);
		Spot->SeatItem(Item);
		UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION %s at %s, %s (%s) %s"), What, *Spot->Room.ToString(), *Spot->Place, *GetNameSafe(Spot), *Spot->GetActorLocation().ToCompactString());
	};
	if (auto* Crowbar = UGameplayStatics::GetActorOfClass(this, ATallowCrowbarPickup::StaticClass())) Place(Crowbar, ETallowPickupSpotItem::Crowbar, NAME_None, TEXT("crowbar"));
	const FName Floors[] = {TEXT("Basement"), TEXT("Ground"), TEXT("Upper")};
	TArray<AActor*> Fuses; UGameplayStatics::GetAllActorsOfClass(this, ATallowFusePickup::StaticClass(), Fuses);
	Fuses.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	for (int32 I = 0; I < Fuses.Num() && I < 3; ++I) Place(Fuses[I], ETallowPickupSpotItem::Fuse, Floors[I], TEXT("fuse"));
	// The start box stays by the candle. The others keep their floor (tagged Tallow.Matchbox.<Floor>).
	TArray<AActor*> Boxes; UGameplayStatics::GetAllActorsOfClass(this, ATallowMatchbox::StaticClass(), Boxes);
	Boxes.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	for (AActor* Box : Boxes)
		for (const FName Floor : Floors)
			if (Box->ActorHasTag(FName(TEXT("Tallow.Matchbox.") + Floor.ToString()))) Place(Box, ETallowPickupSpotItem::Matchbox, Floor, TEXT("matches"));
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION run seed=%d, fuses=%d; placements persist through capture"), ActiveRunSeed, Fuses.Num());
}

void ATallowPlayDirector::SetKeeperAwake(bool bAwake)
{
	bKeeperAwake = bAwake;
	if (Soundscape) Soundscape->SetKeeperAwake(bAwake);
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
		for (auto* Light : TInlineComponentArray<ULightComponent*>(Actor)) Light->SetVisibility(false); // The repaired circuit drives the release motor; the lamps remain dead.
	// The release motor clunks once in OnPower; the dead house has no running mains hum.
	if (PowerHum) { PowerHum->Stop(); PowerHum = nullptr; }
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
	if (Action == ETallowMenuAction::Restart && Menu && Menu->GetMenuMode()==ETallowMenuMode::Caught)
	{
		CloseMenu();
		if(auto* GM=GetWorld()->GetAuthGameMode<ATallowGameMode>()) GM->ResetGame();
		return;
	}

	switch (Action)
	{
	case ETallowMenuAction::Start:
		if (auto* Save = UTallowSaveSubsystem::Get(this)) Save->PrepareNewGame();
		if (auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>()) Story->BeginRun();
		CloseMenu(); bIntroPlaying = true;
		// The stills and the constable's voice first, then the camera goes down into the chair.
		PlaySlides(false, [this] { if (bIntroPlaying && !PlaySequence(IntroSequence, false)) OnSequenceFinished(); });
		break;
	case ETallowMenuAction::Resume: CloseMenu(); break;
	case ETallowMenuAction::Restart:
		if (auto* Save = UTallowSaveSubsystem::Get(this)) Save->PrepareRestart(Scene == ETallowPlayScene::Ending);
		LoadDestination(GameMap); break;
	case ETallowMenuAction::Continue:
		if (auto* Save = UTallowSaveSubsystem::Get(this); Save && Save->PrepareContinue()) LoadDestination(GameMap);
		else if (Menu) Menu->RearmActions();
		break;
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
	if (SequencePlayer) SequencePlayer->OnFinished.RemoveAll(this);
	// Travel on black. The map load is several seconds, and stopping the sequence or the fade
	// first left its last frame frozen on screen. The intro is usually already black (Tick).
	float FadeSeconds = 0.f;
	if (PC && PC->PlayerCameraManager && !bIntroFading)
	{
		FadeSeconds = .35f;
		// From the caught menu the screen is already black: going on from there, not from 0, keeps her last frame
		// from flashing back up, and the sound stays down with it (the next map brings it back).
		auto* Camera = PC->PlayerCameraManager.Get();
		const bool bFromBlack = Camera->bEnableFading && Camera->FadeAmount > 0.f;
		Camera->StartCameraFade(bFromBlack ? Camera->FadeAmount : 0.f, 1.f, FadeSeconds, FColor::Black, bFromBlack, true);
	}
	GetWorldTimerManager().SetTimer(TravelHandle, FTimerDelegate::CreateWeakLambda(this, [this, Destination]
	{
		if (SequencePlayer) SequencePlayer->Stop();
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, Destination);
	}), FMath::Max(FadeSeconds, .01f), false);
}

void ATallowPlayDirector::OnSequenceFinished()
{
	if (bLeaving) return;
	if (Scene == ETallowPlayScene::MainMenu && bIntroPlaying) { bIntroPlaying = false; LoadDestination(GameMap); }
	else if (Scene == ETallowPlayScene::Ending)
	{
		if (bEndSlidesShown) SetMenu(ETallowMenuMode::Outro);
		else { bEndSlidesShown = true; PlaySlides(true, [this] { SetMenu(ETallowMenuMode::Outro); }); }
	}
}

void ATallowPlayDirector::PlaySlides(bool bEnding, TFunction<void()> Then)
{
	// The stills are found by name (Tools/Lore/make_slides.py): Intro_01.. then Intro_Title; EndA_ or EndB_ for the
	// ending this run earned, then Confess_ if the constable's unsent letter was read. Each plays VO_<name> (my
	// booth read, Tools/Lore/booth.py) and holds as long as he spoke over it.
	const auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>();
	// Whoever reaches the ending slides has seen an ending: the second playthrough begins here.
	if (bEnding) TallowCorrupted::MarkStoryCompleted();
	TArray<FString> Names;
	auto Run = [&Names](const TCHAR* Prefix)
	{
		for (int32 I = 1; I < 20; ++I)
		{
			const FString Name = FString::Printf(TEXT("%s_%02d"), Prefix, I);
			if (!FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Presentation/Lore/Slides/T_%s"), *Name))) break;
			Names.Add(Name);
		}
	};
	if (!bEnding) { Run(TEXT("Intro")); Names.Add(TEXT("Intro_Title")); }
	else
	{
		Run(Story && Story->GetEnding() == ETallowEnding::SaidNo ? TEXT("EndB") : TEXT("EndA"));
		if (Story && Story->HasRead(TEXT("letter"))) Run(TEXT("Confess"));
	}
	TArray<FTallowSlide> Slides;
	for (const FString& Name : Names)
	{
		FTallowSlide Slide;
		// The final pass's caption-free plate when there is one; the slides widget then sets the words itself.
		Slide.Name = FName(*Name);
		Slide.Image = UTallowSlidesWidget::LoadPlate(Slide.Name);
		if (!Slide.Image) continue;
		// A separate prefix admits only the new script; the old 1894 VO remains retired.
		const FString VoiceName = TEXT("VO_1974_") + Name;
		const FString VoicePackage = TEXT("/Game/Audio/VO/") + VoiceName;
		if (FPackageName::DoesPackageExist(VoicePackage))
			Slide.Voice = LoadObject<USoundBase>(nullptr, *(VoicePackage + TEXT(".") + VoiceName));
		if (!Slide.Voice && Name==TEXT("Intro_Title")) Slide.Voice=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Creature/V_Creature_01.V_Creature_01"));
		Slide.Seconds=Name.Contains(TEXT("Title"))?4.f:8.f;
		if (Slide.Voice) Slide.Seconds = FMath::Max(Slide.Seconds, Slide.Voice->GetDuration() + .8f);
		if (Slide.Voice)
		{
			// The hidden voice has the last word on a slide: hold until its last line is done.
			Slide.CorruptKey = *(TEXT("slide:") + Name);
			Slide.Seconds = FMath::Max(Slide.Seconds, TallowCorrupted::AfterEnd(Slide.CorruptKey, TallowCorrupted::StoryCompleted()) + .8f);
		}

		Slides.Add(Slide);
	}
	UE_LOG(LogTemp, Display, TEXT("TALLOW_STORY slides %s"), *FString::Join(Names, TEXT(",")));
	if (Slides.IsEmpty() || !PC) { Then(); return; }
	if (!SlidesUI) SlidesUI = CreateWidget<UTallowSlidesWidget>(PC);
	SlidesUI->Play(Slides, MoveTemp(Then));
}

void ATallowPlayDirector::SkipIntro()
{
	// Space skips the stills first; a second press skips the cinematic as before.
	if (SlidesUI && SlidesUI->IsPlaying()) { SlidesUI->Finish(); return; }
	if (bIntroPlaying) OnSequenceFinished();
	else if (Scene == ETallowPlayScene::Ending && !IsMenuOpen())
	{
		if (SequencePlayer) SequencePlayer->Stop();
		if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1,1,0,FColor::Black,false,true);
		OnSequenceFinished();
	}
}

void ATallowPlayDirector::OpenInventory()
{
	if(Scene!=ETallowPlayScene::Game || bIntroPlaying || bCaptureTracking || IsMenuOpen() || !PC || !Player || Player->IsTiedToChair()) return;
	for (TActorIterator<ATallowEscapePoint> It(GetWorld());It;++It) if (It->GetIsCurrentlyInputtingKeypad()) return;
	StopLockpicking();
	if(!InventoryUI) InventoryUI=CreateWidget<UTallowInventoryWidget>(PC);
	InventoryUI->Open(PC);
}

void ATallowPlayDirector::ToggleBackgroundReading()
{
	if(Scene!=ETallowPlayScene::Game||IsMenuOpen()||bCaptureTracking)return;
	TArray<UUserWidget*> Readers;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Readers,UTallowJournalWidget::StaticClass(),false);
	for(auto* Reader:Readers)if(auto* Journal=Cast<UTallowJournalWidget>(Reader))if(Journal->IsCompact())Journal->ToggleReading();
}

void ATallowPlayDirector::SkipBackgroundReading()
{
	TArray<UUserWidget*> Readers;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Readers,UTallowJournalWidget::StaticClass(),false);
	for(auto* Reader:Readers)if(auto* Journal=Cast<UTallowJournalWidget>(Reader))if(Journal->IsCompact())Journal->SkipReading();
}

void ATallowPlayDirector::TogglePause()
{
	if (bIntroPlaying || Scene == ETallowPlayScene::Ending) { SkipIntro(); return; }
	if (Scene != ETallowPlayScene::Game || bLeaving) return;
	if (IsMenuOpen()) { Menu->GoBack(); return; }
	for (TActorIterator<ATallowEscapePoint> It(GetWorld());It;++It) if (It->GetIsCurrentlyInputtingKeypad()) { It->CloseKeypad(); return; }
	// Like the keypad, Escape first puts the picks away.
	if (StopLockpicking()) return;
	if (Player)
		if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>()) Interaction->CancelInteract();
	UGameplayStatics::SetGamePaused(this, true);
	SetMenu(ETallowMenuMode::Pause);
}

void ATallowPlayDirector::OnChairEscaped()
{
	if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(ETallowChapterEvent::ChairEscaped);
	PlayCue(TEXT("P_Chair_Free"), .42f);
	if (Soundscape) Soundscape->PlayEffort();
	Lessons->CompleteLessonIfActive(ETallowLesson::Chair); Lessons->ShowLesson(ETallowLesson::CandlePickup);
	ForcedClip = EscapeClip; ForcedUntil = GetWorld()->GetTimeSeconds() + (EscapeClip ? EscapeClip->GetPlayLength() : 0.f);
}
void ATallowPlayDirector::OnLockPin() { PlayCue(TEXT("P_Fuse_Insert"), .3f); }
void ATallowPlayDirector::OnLockFalseSet() { PlayCue(TEXT("P_Fuse_Insert"), .25f); }
void ATallowPlayDirector::OnLockSnap() { PlayCue(TEXT("P_Latch"), .45f); }   // the Keeper hears this one (Vasu's LockSnap noise)
void ATallowPlayDirector::OnLockOpen() { PlayCue(TEXT("P_Latch"), .5f); }
void ATallowPlayDirector::OnCandleAcquired()
{
	Lessons->CompleteLessonIfActive(ETallowLesson::CandlePickup);
	Lessons->ShowLesson(ETallowLesson::Cup);
	PlayCue(TEXT("P_Cup_Rustle_01"), .3f);
	ForcedClip = PickupClip; ForcedUntil = GetWorld()->GetTimeSeconds() + (PickupClip ? PickupClip->GetPlayLength() : 0.f);
	TryNellLine(1); // A single quiet "Maisie?" as Nell first takes the light.
}
void ATallowPlayDirector::ResetPresentationForChapter()
{
	GetWorldTimerManager().ClearTimer(StingHandle);
	OnCandleReset();
	bWasTied = Player && Player->IsTiedToChair();
	if (!bWasTied && Lessons) Lessons->DismissLesson();
}

void ATallowPlayDirector::OnCandleReset()
{
	EndCaptureShot();
	bCaptureTracking = false;
	if (CaptureBaseFOV > 0.f && Player) Player->GetFirstPersonCameraComponent()->SetFieldOfView(CaptureBaseFOV);
    if (bCaptureCameraBaseValid && Player)
    {
        Player->GetFirstPersonCameraComponent()->SetRelativeLocation(CaptureBaseCameraLocation);
        Player->GetFirstPersonCameraComponent()->SetRelativeRotation(CaptureBaseCameraRotation);
    }
    bCaptureCameraBaseValid = false;
    CapturePullDistance = CaptureAimErrorDegrees = 0.f;
	if (CaptureLight) CaptureLight->SetIntensity(0.f);
	if (auto* CandleFlame = Player ? Player->FindComponentByClass<UTallowCandleFlameComponent>() : nullptr) CandleFlame->LumensOverride = 0.f;
	CaptureTime = 0.f;
	ForcedClip = nullptr; bPowerRestored = false; bBoardsCleared = false; RemovedPlanks = 0; FirstPlankAt = -1.f;
	SetPowerVisuals(false); SetKeeperAwake(false);
	if (UTallowChapterSubsystem::GetActive(this)) { bPowerRestored = FuseBox && FuseBox->HasPower(); bBoardsCleared = true; SetPowerVisuals(bPowerRestored); }
	// A capture puts Maisie's candle back in Nell's hands, so the choice at the collection is undone too.
	if (auto* Story = GetGameInstance()->GetSubsystem<UTallowStorySubsystem>()) Story->ResetHouse();
	for (TActorIterator<ATallowCollectionSlot> It(GetWorld()); It; ++It) It->ResetSlot();
	bSaidNo = false; SaidNoLines = 0;
	if (Soundscape) { Soundscape->SetPowerRestored(bPowerRestored); Soundscape->NotifyReset(); }
	if (Lessons) { Lessons->DismissLesson(); Lessons->ShowLesson(ETallowLesson::Chair); }
}
void ATallowPlayDirector::OnCandleStateChanged(ECandleState State)
{
	// A small puff and a wisp when it goes out; the sleeve when you cup it.
	if (State == ECandleState::Out && LastCandleState != ECandleState::Out && Player->GetCandleComponent()->IsAcquired())
	{
		PlayCue(TEXT("P_Candle_Puff"), .25f);
		if (Soundscape) Soundscape->NotifyCandleOut();
	}
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
	PlayCue(TEXT("P_Match_Strike"), .9f);   // recorded quiet (-33 dB RMS)
	GetWorldTimerManager().SetTimer(CatchHandle, FTimerDelegate::CreateWeakLambda(this, [this] { PlayCue(TEXT("P_Candle_Catch"), .4f); }), MatchCalibrationTime, false);
}
void ATallowPlayDirector::OnPower()
{
	bPowerRestored = true;
	if (Soundscape) Soundscape->SetPowerRestored(true);
	if (FuseBox) { const FVector Box = FuseBox->GetActorLocation(); PlayCue(TEXT("P_Breaker_Clunk"), .8f, &Box); }
	SetPowerVisuals(true);
	TryNellLine(10); // The mechanism works; the house still has only candlelight.
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INTEGRATION power restored; attic route released"));
}
void ATallowPlayDirector::OnOptionalPlankRemoved(bool bBroken)
{
	OnPlankRemoved(false); // Same audible fall; it does not count toward the main route.
}
void ATallowPlayDirector::OnPlankRemoved(bool bBroken)
{
	// Nails letting go, then the board hitting the floor.
	PlayCue(TEXT("P_Board_Pry"), .6f);
	if (Soundscape) Soundscape->PlayEffort();
	GetWorldTimerManager().SetTimer(BoardDropHandle, FTimerDelegate::CreateWeakLambda(this, [this] { PlayCue(TEXT("P_Board_Drop"), .34f); }), .55f, false);
	if (!bBroken || bBoardsCleared) return;
	if (!bKeeperAwake) FirstPlankAt = GetWorld()->GetTimeSeconds();
	SetKeeperAwake(true);
	RemovedPlanks = FMath::Min(RemovedPlanks+1, TotalPlanks);
	bBoardsCleared = TotalPlanks > 0 && RemovedPlanks >= TotalPlanks;
}
void ATallowPlayDirector::OnStill(bool bStill) { if (!bStill) StillTime = 0.f; }
void ATallowPlayDirector::ShowCaughtMenu()
{
	UGameplayStatics::SetGamePaused(this,true);
	SetMenu(ETallowMenuMode::Caught);
}

bool ATallowPlayDirector::StopLockpicking()
{
	auto* Lock = Player ? Player->GetLockpickComponent() : nullptr;
	if (!Lock || !Lock->IsInMinigame()) return false;
	Lock->EndLockpickSession(false);
	return true;
}

void ATallowPlayDirector::OnCaught()
{
	// A session left open would keep the overlay up and the mouse on the pick after she wakes in the chair.
	StopLockpicking();
	if (NellVoice) NellVoice->FadeOut(.08f, 0.f);
	if (Lessons) Lessons->DismissLesson();
	if (InventoryUI) InventoryUI->Close();
	TArray<UUserWidget*> Papers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Papers,UTallowJournalWidget::StaticClass(),false);
	for(auto* Paper:Papers) if(auto* Journal=Cast<UTallowJournalWidget>(Paper)) Journal->SkipReading();
	if (Soundscape) Soundscape->NotifyCaught();
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
		const float Volume = Sting ? CaptureStingVolume : .2f;
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
		if (IsCeilingCapture(Keeper.Get(),0))
		{
			if (!Look.Normalize()) Look = Player->GetActorForwardVector().GetSafeNormal2D();
			if (Look.IsNearlyZero()) Look = FVector::ForwardVector;
			const auto* Capsule = Keeper->GetCapsuleComponent();
			const float Radius = Capsule->GetScaledCapsuleRadius(), HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			FCollisionQueryParams Params(TEXT("CeilingCaptureStage"), false, Player); Params.AddIgnoredActor(Keeper);
			float FloorZ = Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			FHitResult Floor;
			if (GetWorld()->LineTraceSingleByChannel(Floor, Player->GetActorLocation(),
				FVector(Eye.X, Eye.Y, FloorZ - 120.f), ECC_Visibility, Params) && Floor.ImpactNormal.Z > .5f)
				FloorZ = Floor.ImpactPoint.Z;
			float Reach = Keeper->GetCaptureStagingDistance(); FHitResult Wall;
			if (GetWorld()->LineTraceSingleByChannel(Wall, Eye, Eye + Look * Reach, ECC_Visibility, Params))
				Reach = FMath::Min(Reach, FMath::Max(0.f, Wall.Distance - Radius - 6.f));
			const FQuat Facing = CaptureFacing(Keeper.Get(), -Look).Quaternion();
			const FVector From(Eye.X, Eye.Y, FloorZ + HalfHeight + 2.f);
			const auto Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
			FVector Stage = From + Look * Reach; FHitResult Block;
			if (GetWorld()->SweepSingleByProfile(Block, From, Stage, Facing,
				Capsule->GetCollisionProfileName(), Shape, Params))
			{
				Reach = FMath::Max(0.f, Block.Distance - 4.f);
				Stage = From + Look * Reach;
			}
			if (!GetWorld()->OverlapBlockingTestByProfile(Stage, Facing, Capsule->GetCollisionProfileName(), Shape, Params))
			{
				// This variant retains its authored hang-to-floor root. Put the
				// actor at the floor base once; the mesh supplies the descent.
				Keeper->SetActorLocationAndRotation(Stage, Facing, false, nullptr, ETeleportType::TeleportPhysics);
			}
			else UE_LOG(LogTemp, Warning, TEXT("MIM_CAPTURE ceiling stage blocked; preserving physical capsule"));
		}
		else if (Look.Normalize())
		{
			float Reach = Keeper->GetCaptureStagingDistance(); FHitResult Hit;
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
	// Her drawn size (she is drawn larger than the body her movement plans for).
	const auto* Hunter = Cast<ATallowHunterCreature>(Keeper.Get());
	const float Scale = Hunter ? Hunter->GetVisibleBodyScale() : Keeper->GetActorScale3D().GetAbsMin();
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

void ATallowPlayDirector::PlayMapGesture(bool bPuttingAway)
{
	ForcedClip=ReachClip;ForcedUntil=GetWorld()->GetTimeSeconds()+(bPuttingAway?.3f:.6f);
}

void ATallowPlayDirector::PlayProjectorGesture(bool bInserting)
{
	if (!Presentation || bCaptureTracking || !Player) return;
	const float Now=GetWorld()->GetTimeSeconds();
	if (!bInserting && Now<ProjectorGestureUntil-.08f) return;
	ForcedClip=bInserting && FuseInsertClip?FuseInsertClip:ReachClip;
	if (!ForcedClip) ForcedClip=PickupClip;
	ProjectorGestureDuration=bInserting?.55f:.32f;
	ProjectorGestureUntil=ForcedUntil=Now+ProjectorGestureDuration;
	if (ForcedClip) Presentation->PlayPresentationClip(ForcedClip,false,ForcedClip->GetPlayLength()/ProjectorGestureDuration,.06f,true);
	if (auto* Contact=Player->FindComponentByClass<UTallowToolContactComponent>()) Contact->PlayProjectorImpulse(bInserting);
}

void ATallowPlayDirector::UpdatePlayerAnimation()
{
	if (!Presentation || !Presentation->IsPresentationEnabled()) return;
	const auto* Candle = Player->GetCandleComponent();
	HeldCandle->SetVisibility(Candle->IsAcquired() && !Candle->IsGivenUp());
	const auto Item = Player->GetCarriedItem();
	const auto* Map=Player->FindComponentByClass<UTallowHouseMapComponent>();
	const bool bReadingMap=Map&&Map->IsMapOpen();
	const bool bRelighting = ForcedClip == RelightClip && ForcedClip && GetWorld()->GetTimeSeconds() < ForcedUntil;
	if (HeldMatch) HeldMatch->SetVisibility(bRelighting);
	if (MatchFlame) MatchFlame->StandaloneState = bRelighting ? ECandleState::Lit : ECandleState::Out;
	if (OriginalHeldTool) OriginalHeldTool->SetHiddenInGame(true);
	if (HeldTool)
	{
		HeldTool->SetStaticMesh(Item == ECarriedItemType::Crowbar ? CrowbarMesh : Item == ECarriedItemType::Fuse ? FuseMesh : Item == ECarriedItemType::FilmReel ? FilmReelMesh : nullptr);
		HeldTool->SetRelativeTransform(Item == ECarriedItemType::Crowbar ? CrowbarHandTransform : Item == ECarriedItemType::FilmReel ? FilmReelHandTransform : FuseHandTransform);
		HeldTool->SetVisibility(Item != ECarriedItemType::None && !bReadingMap && !Player->IsTiedToChair() && !bRelighting && Candle->GetCandleState() != ECandleState::Cupped);
	}
	UAnimSequence* Clip = RestClip;
	bool bLoop = true;
	float Rate = 1.f;
	if (Player->IsTiedToChair() && BoundClip) Clip = BoundClip;
	else if (ForcedClip && GetWorld()->GetTimeSeconds() < ForcedUntil) { Clip = ForcedClip; bLoop = false; if (GetWorld()->GetTimeSeconds()<ProjectorGestureUntil) Rate=Clip->GetPlayLength()/ProjectorGestureDuration; }
	else if (Player->IsHiddenInSpot()) Clip = CrouchClip ? CrouchClip : HoldClip;
	else if (Candle->GetCandleState() == ECandleState::Cupped) Clip = CupClip;
	else if (auto* Interaction = Player->FindComponentByClass<UTallowInteractionComponent>(); Interaction && Interaction->IsInteracting())
	{
		if (Item == ECarriedItemType::Crowbar && Cast<ATallowPlank>(Interaction->GetInteractionTarget()) && CrowbarPryClip)
		{
			Clip = CrowbarPryClip; bLoop = false; Rate = 0.f;
		}
		else if (Item == ECarriedItemType::Crowbar) Clip = ToolHoldClip;
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
		else if (Player->GetVelocity().Size2D() > 300.f && SprintClip) { Clip = SprintClip; Rate = FMath::Clamp(Player->GetVelocity().Size2D()/450.f,.75f,1.25f); }
		else if (Player->GetVelocity().Size2D() > 25.f && WalkClip) { Clip = WalkClip; Rate = FMath::Clamp(Player->GetVelocity().Size2D()/250.f,.45f,1.15f); }
	}
	if (Clip) Presentation->PlayPresentationClip(Clip, bLoop, Rate, .18f);
	if (auto* Interaction=Player->FindComponentByClass<UTallowInteractionComponent>(); Interaction && Interaction->IsInteracting() && Clip==CrowbarPryClip)
		if (auto* Instance=Cast<UTallowPresentationAnimInstance>(Presentation->GetPresentationHands()->GetAnimInstance()))
			Instance->SetPlaybackPosition(Interaction->GetInteractionProgress()*CrowbarPryClip->GetPlayLength());
}

FText ATallowPlayDirector::GetObjective() const
{
	if (!bScreenInstructions) return FText::GetEmpty();
	if (const auto* Chapters = UTallowChapterSubsystem::GetActive(this)) return Chapters->GetObjectiveText();
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
	for (TActorIterator<ATallowProjector> It(GetWorld());It;++It)
	{
		if (!It->IsReelLoaded()) return FText::FromString(Player->GetInventoryCount(ECarriedItemType::FilmReel)>0?TEXT("Take the film reel to the projector."):TEXT("Find the film reel. The release needs its code."));
		if (It->GetProjectorState()==EProjectorState::Empty) return FText::FromString(TEXT("Tap E repeatedly to thread the film."));
		if (!It->HasCompletedProjection()) return FText::FromString(TEXT("Start the projector and watch all five digits on the wall."));
	}
	for (TActorIterator<ATallowEscapePoint> It(GetWorld());It;++It) if (!It->IsCodeVerified()) return FText::FromString(TEXT("Enter the five film digits at the attic release."));
	return FText::FromString(TEXT("Hold E at the attic latch to release it."));
}

bool ATallowPlayDirector::CanEscape() const
{
	if (!CanApproachExit()) return false;
	bool bHasProjector=false;
	for (TActorIterator<ATallowProjector> It(GetWorld());It;++It)
	{
		bHasProjector=true;
		if (!It->HasCompletedProjection()) return false;
	}
	if (!bHasProjector) return true;
	for (TActorIterator<ATallowEscapePoint> It(GetWorld());It;++It) if (It->IsCodeVerified()) return true;
	return false;
}

void ATallowPlayDirector::FinishRun()
{
	if (!CanEscape()) return;
	PlayCue(TEXT("P_Latch"), .5f);
	SetKeeperAwake(false);
	if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(ETallowChapterEvent::FinalReached);
	LoadDestination(EndingMap);
}

void ATallowPlayDirector::OnCandleGivenBack(const FVector& Where)
{
	// The existing collection interaction now follows Maisie's finished place, rather than giving up Nell's candle.
	if(!CanEscape() || bLeaving) return;
	if(auto* Story=UTallowStorySubsystem::GetStory(this)) Story->GiveCandleBack();
	SetKeeperAwake(false);
	if (auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Chapters->NotifyEvent(ETallowChapterEvent::FinalReached);
	LoadDestination(EndingMap);
}

float ATallowPlayDirector::SayAt(const FString& Name, const FString& Fallback, const FVector& Where, float Volume)
{
	auto Load = [](const FString& N, const TCHAR* Folder) { return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), Folder, *N, *N)); };
	USoundBase* Line = Load(Name, TEXT("VO"));
	if (!Line && !Fallback.IsEmpty()) Line = Load(Fallback, TEXT("Creature"));
	if (!Line) return 0.f;
	// Her scripted lines from the dark get the same lift as everything else she says (30 Oct).
	if (const auto* Voice = Keeper ? Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>() : nullptr) Volume *= Voice->GetSpokenLift();
	UE_LOG(LogTemp, Display, TEXT("TALLOW_STORY say %s vol=%.2f"), *Line->GetName(), Volume);
	if (auto* Subtitles = UTallowSubtitleSubsystem::Get(this)) Subtitles->Begin(Line->GetFName(), Line->GetDuration(), ETallowSubtitleVoice::Mim);
	UGameplayStatics::PlaySoundAtLocation(this, Line, Where, Volume, 1.f, 0.f, CueAttenuation);
	return Line->GetDuration();
}

void ATallowPlayDirector::OnLoreRead(FName Key)
{
	// Nell has no family connection to Thomas. The records reveal the house's history, not her ancestry.
	// A finished reading earns a short protected window to put the paper down and think.
	if (auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Pacing->BeginProtectedWindow(18.f);
}

void ATallowPlayDirector::UpdatePacing()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextPacing) return;
	NextPacing = Now + .5f;
	auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>();
	if (!Pacing) return;
	bool bReelLoaded = false, bProjecting = false, bProjectionDone = false, bCodeVerified = false;
	for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It)
	{
		bReelLoaded |= It->IsReelLoaded();
		bProjecting |= It->GetProjectorState() == EProjectorState::Projecting;
		bProjectionDone |= It->HasCompletedProjection();
	}
	for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) bCodeVerified |= It->IsCodeVerified();
	// The act is read off the route she has actually walked, so a capture reset lowers it again.
	const UTallowChapterSubsystem* Chapters = UTallowChapterSubsystem::GetActive(this);
	const ETallowStoryAct Act = Chapters ? Chapters->GetPacingAct() : bCodeVerified ? ETallowStoryAct::EndRun
		: bReelLoaded ? ETallowStoryAct::Reel
		: bPowerRestored ? ETallowStoryAct::HouseWakes
		: bBoardsCleared && (FirstPlankAt < 0.f || Now - FirstPlankAt >= FirstContactSeconds) ? ETallowStoryAct::FuseHunt
		: bKeeperAwake ? ETallowStoryAct::FirstContact
		: ETallowStoryAct::Dormant;
	Pacing->SetStoryAct(Act);
	// Watching the film is a held breath: no peak, no house tricks, until the last digit fades.
	if (bProjecting) Pacing->BeginProtectedWindow(1.f);
	if (bProjectionDone && !bPacingProjectionDone) Pacing->NoteProjectionComplete();
	bPacingProjectionDone = bProjectionDone;
}

void ATallowPlayDirector::UpdateStory()
{
	const float Now = GetWorld()->GetTimeSeconds();
	// Coming round in the chair: somewhere in the dark, "Hiiii, Nell."
	if (!bHiNellPlayed && Player->IsTiedToChair())
	{
		if (TiedSince < 0.f) TiedSince = Now;
		if (Now - TiedSince > 2.5f)
		{
			bHiNellPlayed = true;
			const FVector Behind = Player->GetActorLocation() - Player->GetActorForwardVector() * 420.f + FVector(0, 0, 60);
			SayAt(TEXT("VO_Mim_HiNell"), TEXT("V_Creature_00"), Behind, .45f);
			// My waltz has come with her from the menu; it lets go as Mim speaks.
			if (auto* Waltz = UTallowOpeningMusicSubsystem::GetOpeningMusic(this)) Waltz->Finish(5.f);
		}
	}
	// Ending B's dark run: Mim talks to her out of the dark, a beat after the candle goes, and again after.
	if (bSaidNo && SaidNoLines < 2 && Now >= NextSaidNoLine) NextSaidNoLine = Now + SpeakSaidNo() + 2.f;
}

void ATallowPlayDirector::LeaveAfterSaidNo()
{
	if (SaidNoLines >= 2) { bLeaving = false; LoadDestination(EndingMap); return; }
	const float Seconds = SpeakSaidNo();
	GetWorldTimerManager().SetTimer(StoryHandle, this, &ATallowPlayDirector::LeaveAfterSaidNo, Seconds + .9f, false);
}

float ATallowPlayDirector::SpeakSaidNo()
{
	// "Nell. You're frightened." then "I can make it stop." From its body when it is near; otherwise from the dark
	// a few metres behind her, on the side it is coming from.
	const bool bFirst = SaidNoLines++ == 0;
	const FString Name = bFirst ? TEXT("VO_Mim_Frightened") : TEXT("VO_Mim_MakeItStop");
	const FString Fallback = bFirst ? TEXT("V_Creature_06") : TEXT("V_Creature_02");
	if (Keeper && FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) < 900.f)
		if (auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>())
			if (Voice->SayLine(Name, Fallback, .65f))
			{
				const FString Path = FString::Printf(TEXT("/Game/Audio/VO/%s.%s"), *Name, *Name);
				const USoundBase* Line = LoadObject<USoundBase>(nullptr, *Path);
				return Line ? Line->GetDuration() : 3.f;
			}
	const FVector Here = Player->GetActorLocation();
	const FVector From = Keeper ? (Keeper->GetActorLocation() - Here).GetSafeNormal2D() : -Player->GetActorForwardVector();
	return SayAt(Name, Fallback, Here + From * 300.f + FVector(0, 0, 40), .6f);
}

void ATallowPlayDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The menu and ending maps only need a camera. Their default pawn is the engine's grid sphere,
	// possessed a moment after start, and it sat at the origin in view of the intro's first shot.
	if (Scene != ETallowPlayScene::Game && PC && PC->GetPawn() && !PC->GetPawn()->IsHidden()) PC->GetPawn()->SetActorHiddenInGame(true);
	// Fade the intro out over its last half second so the map travel starts on black.
	if (bIntroPlaying && !bIntroFading && SequencePlayer && PC && PC->PlayerCameraManager)
	{
		const float Left = SequencePlayer->GetEndTime().AsSeconds() - SequencePlayer->GetCurrentTime().AsSeconds();
		if (Left <= .5f)
		{
			bIntroFading = true;
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FMath::Max(Left, .05f), FColor::Black, false, true);
		}
	}
	// The opening waltz sits under the stills' narration, and lower under Nell's own lines.
	if (auto* Waltz = UTallowOpeningMusicSubsystem::GetOpeningMusic(this))
		Waltz->SetUnderVoice(SlidesUI && SlidesUI->IsPlaying(), NellVoice && NellVoice->IsPlaying());
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
	UpdateSound(); UpdateNightEyes(DeltaSeconds); UpdateGuide(); UpdateLessons(DeltaSeconds); UpdatePlayerAnimation(); UpdateStory(); UpdatePacing();
}

void ATallowPlayDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(InitializeHandle);
	GetWorldTimerManager().ClearTimer(TravelHandle);
	GetWorldTimerManager().ClearTimer(CatchHandle);
	GetWorldTimerManager().ClearTimer(BoardDropHandle);
	GetWorldTimerManager().ClearTimer(StingHandle);
	if (PowerHum) PowerHum->Stop();
	if (NellVoice) NellVoice->Stop();
	if (ActorSpawnedHandle.IsValid()) GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	if (SequencePlayer) { SequencePlayer->OnFinished.RemoveAll(this); SequencePlayer->Stop(); }
	if (Presentation) Presentation->DisablePresentation();
	if (Menu) Menu->RemoveFromParent();
	if (Lessons) Lessons->RemoveFromParent();
	if (RunUI) RunUI->RemoveFromParent();
	if (InventoryUI) InventoryUI->Close();
	if (MatchUI) MatchUI->RemoveFromParent();
	if (CandleStatusUI) CandleStatusUI->RemoveFromParent();
	Super::EndPlay(Reason);
}

bool ATallowPlayDirector::SayNellLine(int32 Number)
{
	const uint32 Before = NellSaidLines;
	TryNellLine(Number);
	return NellSaidLines != Before;
}

void ATallowPlayDirector::TryNellLine(int32 Number)
{
	if (Number < 1 || Number > 20 || !Player || !PC || Scene != ETallowPlayScene::Game
		|| bIntroPlaying || bCaptureTracking || bLeaving || IsMenuOpen() || Player->IsTiedToChair()
		|| (InventoryUI && InventoryUI->IsInViewport()) || (NellVoice && NellVoice->IsPlaying())) return;
	const uint32 Bit = 1u << Number;
	if (NellSaidLines & Bit) return;
	if (Keeper)
		if (auto* Voice = Keeper->FindComponentByClass<UTallowKeeperVoiceComponent>())
			if (Voice->IsSpeaking()) return;
	TArray<UUserWidget*> Papers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Papers, UTallowJournalWidget::StaticClass(), false);
	for (auto* Paper : Papers)
		if (auto* Journal = Cast<UTallowJournalWidget>(Paper)) if (Journal->IsOpen()) return;
	const FString Name = FString::Printf(TEXT("VO_Nell_Line_%02d"), Number);
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/VO/%s.%s"), *Name, *Name));
	if (!Sound) return;
	NellVoice = UGameplayStatics::CreateSound2D(this, Sound, .38f,1.f,0.f,nullptr,false,true);
	if (NellVoice)
	{
		NellVoice->bIsUISound = false;
		NellVoice->Play();
		NellSaidLines |= Bit;
		if (auto* Subtitles = UTallowSubtitleSubsystem::Get(this)) Subtitles->Begin(*Name, Sound->GetDuration(), ETallowSubtitleVoice::Nell);
		UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE Nell %s"), *Name);
	}
}

void ATallowPlayDirector::OnFuseInserted()
{
	if (FuseBox) { const FVector Box = FuseBox->GetActorLocation(); PlayCue(TEXT("P_Fuse_Insert"), .6f, &Box); }
	// The first fuse seated is the hunt's first real win: let her feel the house let go for a breath.
	if (FuseBox && FuseBox->CurrentFuses == 1)
		if (auto* Pacing = GetWorld()->GetSubsystem<UTallowHunterDirector>()) Pacing->NotePuzzlePayoff();
}

USoundBase* ATallowPlayDirector::Cue(const FString& Name)
{
	if (const TObjectPtr<USoundBase>* Found = Cues.Find(Name)) return *Found;
	const TCHAR* Folder = Name == TEXT("K_Mim_Capture_Sting") ? TEXT("Creature") : TEXT("CC0");
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), Folder, *Name, *Name));
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
	else UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch,0.f,nullptr,this,false);
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
	// 50 cm strides read as hurried. Steps up 2.7 dB with the louder calm house (28 Oct) so they keep their margin.
	Steps->StrideCm = 75.f; Steps->Volume = .34f; Steps->ExtraChance = .12f; Steps->ExtraVolume = .2f;
	Player->AddInstanceComponent(Steps); Steps->RegisterComponent();
	// The Keeper: soft palms patting the boards and its fur dragging behind ("quite soft"), heard from the next room.
	if (Keeper)
	{
		auto Creature = [](const FString& Name) { return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Creature/%s.%s"), *Name, *Name)); };
		auto* Heavy = NewObject<UTallowFootstepComponent>(Keeper, TEXT("IntegratedKeeperFootsteps"));
		for (int32 I = 0; I < 6; ++I) Heavy->Steps.Add(Creature(FString::Printf(TEXT("C_Pat_%02d"), I)));
		for (int32 I = 0; I < 3; ++I) Heavy->Extras.Add(Creature(FString::Printf(TEXT("C_Drag_%02d"), I)));
		Heavy->StrideCm = 60.f; Heavy->Volume = .7f; Heavy->ExtraChance = .3f; Heavy->ExtraVolume = .45f; Heavy->FalloffCm = 3000.f; Heavy->bNaturalFalloff = true;   // track it by ear through the house
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

void ATallowPlayDirector::UpdateGuide()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Player || Now < NextGuide) return;
	// Four times a second: the next corner of her walk changes as she rounds it.
	NextGuide = Now + .25f;
	if (const auto* Chapters = UTallowChapterSubsystem::GetActive(this)) { Player->GuideTarget = Chapters->GetGuideTarget(&Player->GuideWalk); return; }
	const FVector Here = Player->GetActorLocation();
	FVector Best = FVector::ZeroVector; float BestDistance = TNumericLimits<float>::Max();
	auto Consider = [&](const AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->IsHidden()) return;
		const float Distance = FVector::Dist(Actor->GetActorLocation(), Here);
		if (Distance < BestDistance) { BestDistance = Distance; Best = Actor->GetActorLocation(); }
	};
	const ECarriedItemType Item = Player->GetCarriedItem();
	// A plank still boarding a door: its mesh has not been pried loose into physics.
	auto Boarding = [](const ATallowPlank* Plank)
	{
		for (const UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Plank)) if (Mesh->IsSimulatingPhysics()) return false;
		return true;
	};
	if (!Player->GetCandleComponent()->IsAcquired())
	{
		for (TActorIterator<ATallowCandlePickup> It(GetWorld()); It; ++It) Consider(*It);
	}
	else if (!bBoardsCleared)
	{
		if (Player->GetInventoryCount(ECarriedItemType::Crowbar)>0)
		{
			for (TActorIterator<ATallowPlank> It(GetWorld());It;++It) if (It->bRequiredForEscape && Boarding(*It)) Consider(*It);
		}
		else for (TActorIterator<ATallowCrowbarPickup> It(GetWorld());It;++It) Consider(*It);
	}
	else if (!bPowerRestored)
	{
		if (Player->GetInventoryCount(ECarriedItemType::Fuse)>0) { if (FuseBox) Consider(FuseBox); }
		else for (TActorIterator<ATallowFusePickup> It(GetWorld());It;++It) Consider(*It);
	}
	else
	{
		bool bNeedsReel=false,bNeedsProjection=false;
		for (TActorIterator<ATallowProjector> It(GetWorld());It;++It)
		{
			if (!It->IsReelLoaded())
			{
				bNeedsReel=true;
				if (Player->GetInventoryCount(ECarriedItemType::FilmReel)>0) Consider(*It);
			}
			else if (!It->HasCompletedProjection()) { bNeedsProjection=true; Consider(*It); }
		}
		if (bNeedsReel && Best.IsZero()) for (TActorIterator<ATallowFilmReelPickup> It(GetWorld());It;++It) Consider(*It);
		if (!bNeedsReel && !bNeedsProjection) for (TActorIterator<ATallowEscapePoint> It(GetWorld());It;++It) Consider(*It);
	}
	Player->GuideTarget = Best;
	Player->GuideWalk = Best.IsZero() ? 0.f : BestDistance;
}

void ATallowPlayDirector::UpdateNightEyes(float DeltaSeconds)
{
	if (!Player || !Player->GetCandleComponent()) return;
	if (!NightEyes)
	{
		NightEyes = NewObject<UPointLightComponent>(Player, TEXT("IntegratedNightEyes"));
		NightEyes->SetupAttachment(Player->GetFirstPersonCameraComponent());
		NightEyes->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
		NightEyes->SetIntensityUnits(ELightUnits::Lumens); NightEyes->SetIntensity(0.f);
		NightEyes->SetLightColor(FLinearColor(.55f, .63f, .82f)); NightEyes->SetAttenuationRadius(1100.f);
		NightEyes->SetCastShadows(false); NightEyes->SetSpecularScale(.2f);
		Player->AddInstanceComponent(NightEyes); NightEyes->RegisterComponent();
	}
	// Eyes adjust slowly to the dark and snap back the moment the flame catches.
	const bool bDark = Player->GetCandleComponent()->GetCandleState() == ECandleState::Out && !bCaptureTracking;
	NightEyesLevel = bDark ? FMath::Min(1.f, NightEyesLevel + DeltaSeconds / FMath::Max(NightEyesAdjustSeconds, .1f))
		: FMath::Max(0.f, NightEyesLevel - DeltaSeconds * 4.f);
	NightEyes->SetIntensity(NightEyesLumens * FMath::SmoothStep(0.f, 1.f, NightEyesLevel));
}

void ATallowPlayDirector::UpdateSound()
{
	for (int32 I = 0; I < Hinges.Num(); ++I)
	{
		if (!Hinges[I]) continue;
		const float Yaw = FMath::Abs(Hinges[I]->GetRelativeRotation().Yaw);
		const float Delta = Yaw - HingeYaw[I];
		// A door the house moves itself (a minor scare) brings its own creak and latch, not the hand-opened sound.
		if (const auto* Door = Cast<ATallowDoor>(Hinges[I]->GetOwner()); Door && Door->IsDrifting()) { HingeYaw[I] = Yaw; continue; }
		if (!HingeMoving[I] && FMath::Abs(Delta) > .15f)
		{
			HingeMoving[I] = true;
			const FVector Where = Hinges[I]->GetComponentLocation();
			const int32 Variant = FMath::RandRange(0, 1);
			PlayCue(Delta > 0.f ? FString::Printf(TEXT("D_Open_%02d"), Variant) : FString::Printf(TEXT("D_Close_%02d"), Variant), .45f, &Where);
		}
		else if (HingeMoving[I] && FMath::Abs(Delta) < .01f) HingeMoving[I] = false;
		HingeYaw[I] = Yaw;
	}
	const uint8 Item = static_cast<uint8>(Player->GetCarriedItem());
	if (Item != LastItem && Player->GetCarriedItem() != ECarriedItemType::None && Player->GetCarriedItem() != ECarriedItemType::FilmReel)
		PlayCue(Player->GetCarriedItem() == ECarriedItemType::Crowbar ? TEXT("P_Pickup_Crowbar") : TEXT("P_Pickup_Fuse"), .45f);
	LastItem = Item;
	// The house settling somewhere nearby, rarely and quietly, so silence never feels safe.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextSettle && (!Soundscape || (Soundscape->AllowsSettles() && !Soundscape->HasHouseAtmosphere())))
	{
		const FVector Where = Player->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(500.f, 1100.f);
		PlayCue(FString::Printf(TEXT("H_Settle_%02d"), FMath::RandRange(0, 2)), .5f, &Where);   // clears the calm bed (28 Oct)
		UE_LOG(LogTemp, Display, TEXT("TALLOW_AUDIO settle %s"), *Where.ToCompactString());
		NextSettle = Now + FMath::FRandRange(40.f, 80.f);   // rare, so a creak still means something
	}
}
