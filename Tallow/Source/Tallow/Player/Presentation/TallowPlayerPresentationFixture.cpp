// Explicit editor review harness. Never compiled into a non-editor target.
#if WITH_EDITOR
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

namespace TallowPlayerFixture
{
static TWeakObjectPtr<ACameraActor> Fixture;
static TWeakObjectPtr<APlayerController> Controller;
static TWeakObjectPtr<AActor> PreviousView;
static TWeakObjectPtr<APawn> HiddenPawn;
static bool bPawnWasHidden = false;

static bool Allowed(UWorld* World)
{
	if (!World) return false;
	const FString Package = World->GetOutermost()->GetName();
	FString Leaf = FPackageName::GetShortName(Package);
	if (Leaf.StartsWith(TEXT("UEDPIE_")))
	{
		Leaf.RightChopInline(7); // instance number followed by underscore
		int32 Separator;
		if (!Leaf.FindChar(TEXT('_'), Separator)) return false;
		Leaf.RightChopInline(Separator + 1);
	}
	return FPackageName::GetLongPackagePath(Package) == TEXT("/Game/Presentation/Maps")
		&& Leaf.StartsWith(TEXT("Lvl_PlayerContactReview_"));
}

static void Remove()
{
	if (Controller.IsValid() && Fixture.IsValid() && Controller->GetViewTarget() == Fixture.Get() && PreviousView.IsValid())
		Controller->SetViewTarget(PreviousView.Get());
	if (HiddenPawn.IsValid()) HiddenPawn->SetActorHiddenInGame(bPawnWasHidden);
	if (Fixture.IsValid()) Fixture->Destroy();
	Fixture.Reset(); Controller.Reset(); PreviousView.Reset(); HiddenPawn.Reset();
}

static bool Pose(const FString& Name)
{
	const TArray<FString> Names = {TEXT("Rest"), TEXT("Hold"), TEXT("Cup"), TEXT("Interact"), TEXT("Relight")};
	const FString* Canonical = Names.FindByPredicate([&](const FString& Value){ return Value.Equals(Name, ESearchCase::IgnoreCase); });
	if (!Canonical || !Fixture.IsValid()) return false;
	const FString Path = TEXT("/Game/Presentation/PlayerGenerated/AN_PlayerGenerated_") + *Canonical;
	UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path);
	UTallowPlayerPresentationComponent* Adapter = Fixture->FindComponentByClass<UTallowPlayerPresentationComponent>();
	const bool bSampled = Adapter && Adapter->SetPresentationPose(Clip, 17.f / 30.f);
	if (bSampled)
	{
		USkeletalMeshComponent* Hands = Adapter->GetPresentationHands();
		const UCameraComponent* Camera = Fixture->GetCameraComponent();
		const FTransform View = Camera->GetComponentTransform();
		UE_LOG(LogTemp, Display, TEXT("PlayerFixture pose=%s registered=%d visible=%d hidden=%d ownerHidden=%d ownerOnly=%d ownerNoSee=%d bones=%d transform=%s boundsOrigin=%s boundsExtent=%s handRcamera=%s handLcamera=%s material=%s viewOwner=%s"),
			**Canonical, Hands->IsRegistered(), Hands->IsVisible(), Hands->bHiddenInGame, Fixture->IsHidden(),
			Hands->bOnlyOwnerSee, Hands->bOwnerNoSee, Hands->GetNumBones(), *Hands->GetComponentTransform().ToString(),
			*Hands->Bounds.Origin.ToString(), *Hands->Bounds.BoxExtent.ToString(),
			*View.InverseTransformPosition(Hands->GetSocketLocation(TEXT("hand_r"))).ToString(),
			*View.InverseTransformPosition(Hands->GetSocketLocation(TEXT("hand_l"))).ToString(),
			*GetNameSafe(Hands->GetMaterial(0)), *GetNameSafe(Fixture->GetWorld()->GetFirstPlayerController()->GetViewTarget()));
	}
	return bSampled;
}

static void Spawn(const TArray<FString>& Args, UWorld* World)
{
	if (!Allowed(World)) { UE_LOG(LogTemp, Warning, TEXT("PlayerFixture refused: requires /Game/Presentation/Maps/Lvl_PlayerContactReview_*")); return; }
	if (Args.Num() && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase)) { Remove(); return; }
	APlayerController* PC = World->GetFirstPlayerController();
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Presentation/PlayerGenerated/SK_PlayerGenerated_Partial"));
	UStaticMesh* Stick = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Props/Candle/wooden_candlestick_1k"));
	if (!PC || !Mesh || !Stick) { UE_LOG(LogTemp, Error, TEXT("PlayerFixture requires a local player controller (PIE/Standalone) and imported player/candle assets.")); return; }
	Remove();
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.Owner = PC;
	ACameraActor* Actor = World->SpawnActor<ACameraActor>(FVector(0, 0, 110), FRotator::ZeroRotator, Params);
	if (!Actor) return;
	Fixture = Actor;
	Actor->SetActorLabel(TEXT("PlayerProductionAdapterFixture"));
	Actor->SetActorEnableCollision(false);
	UCameraComponent* Camera = Actor->GetCameraComponent();
	Camera->SetFieldOfView(70.f);
	auto* Candle = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
	Actor->AddInstanceComponent(Candle);
	Candle->SetupAttachment(Camera);
	Candle->SetRelativeLocation(FVector(45, 26, -30));
	Candle->SetStaticMesh(Stick);
	Candle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Candle->RegisterComponent();
	auto* Wick = NewObject<USceneComponent>(Actor, NAME_None, RF_Transient);
	Actor->AddInstanceComponent(Wick);
	Wick->SetupAttachment(Candle);
	Wick->SetRelativeLocation(FVector(0, 0, Stick->GetBoundingBox().Max.Z));
	Wick->RegisterComponent();
	auto* Flame = NewObject<UTallowFlameVisualComponent>(Actor, NAME_None, RF_Transient);
	Actor->AddInstanceComponent(Flame);
	Flame->bStandalone = true;
	Flame->WickAnchor = Wick;
	Flame->RegisterComponent();
	if (!Flame->GetVisualMesh() && !Flame->InitializeVisual()) { Remove(); return; }
	auto* Adapter = NewObject<UTallowPlayerPresentationComponent>(Actor, NAME_None, RF_Transient);
	Actor->AddInstanceComponent(Adapter);
	Adapter->RegisterComponent();
	if (!Adapter->EnablePresentation(Camera, Mesh, Candle, Flame->GetVisualMesh()) || !Pose(TEXT("Hold")))
	{ UE_LOG(LogTemp, Error, TEXT("PlayerFixture adapter/pose initialization failed.")); Remove(); return; }
	Controller = PC;
	PreviousView = PC->GetViewTarget();
	HiddenPawn = PC->GetPawn();
	if (HiddenPawn.IsValid()) { bPawnWasHidden = HiddenPawn->IsHidden(); HiddenPawn->SetActorHiddenInGame(true); }
	PC->SetViewTarget(Actor); // ViewActor is the primitive owner: OnlyOwnerSee remains enabled.
	UE_LOG(LogTemp, Display, TEXT("PlayerFixture ready. PlayerPose Hold|Cup|Interact|Relight|Rest; PlayerFixture off restores the previous view/pawn."));
}

static FAutoConsoleCommandWithWorldAndArgs SpawnCommand(
	TEXT("Tallow.Presentation.PlayerFixture"), TEXT("Editor review map only: create production adapter fixture; 'off' restores view."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Spawn));
static FAutoConsoleCommandWithWorldAndArgs PoseCommand(
	TEXT("Tallow.Presentation.PlayerPose"), TEXT("Sample fixture Rest|Hold|Cup|Interact|Relight at 17/30 seconds."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!Allowed(World) || !Fixture.IsValid() || Fixture->GetWorld() != World || Args.Num() != 1 || !Pose(Args[0]))
			UE_LOG(LogTemp, Warning, TEXT("PlayerPose refused or failed: spawn fixture in its allowed review world, then supply one supported pose."));
	}));
}
#endif
