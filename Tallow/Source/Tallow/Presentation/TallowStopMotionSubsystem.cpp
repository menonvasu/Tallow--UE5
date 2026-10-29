#include "Presentation/TallowStopMotionSubsystem.h"

#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

namespace
{
/** Mim animates smoothly (off twos for Mim specifically); everything else stays
 *  stepped. A hunter creature whose rig has not chosen yet is also left alone for
 *  now, so her first frames are never stepped before InitializeMesh picks Mim. */
bool IsSmoothMimMesh(const USkeletalMeshComponent* Mesh)
{
	const auto* Hunter = Mesh ? Cast<ATallowHunterCreature>(Mesh->GetOwner()) : nullptr;
	if (!Hunter || !Hunter->HunterMotion) return false;
	return Hunter->HunterMotion->UsesMim() || !Hunter->HunterMotion->HasInitialized();
}
}

static TAutoConsoleVariable<int32> CVarTallowOnTwos(
	TEXT("tallow.OnTwos"), 1, TEXT("Stop-motion stepping of everything that animates (1 on, 0 off). The camera stays smooth."));
static TAutoConsoleVariable<float> CVarTallowOnTwosRate(
	TEXT("tallow.OnTwos.Rate"), 12.f, TEXT("Stop-motion step rate in frames per second (12 = on twos at 24 fps)."));

void UTallowStopMotionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickStartHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UTallowStopMotionSubsystem::OnWorldTickStart);
}

void UTallowStopMotionSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
	Release();
	Super::Deinitialize();
}

bool UTallowStopMotionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTallowStopMotionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowStopMotionSubsystem, STATGROUP_Tickables);
}

void UTallowStopMotionSubsystem::OnWorldTickStart(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (InWorld != GetWorld()) return;
	// Put back the game's own placement before anything this frame reads or writes it.
	for (FHeld& H : Held)
	{
		if (!H.bOverridden) continue;
		if (USceneComponent* Component = H.Component.Get()) Component->SetRelativeTransform(H.Relative);
		H.bOverridden = false;
	}
}

bool UTallowStopMotionSubsystem::ShouldHoldTransform(const USkeletalMeshComponent* Mesh) const
{
	// Characters that travel (the Keeper) step their position too. The player's own meshes hang off
	// the camera, which stays smooth, and set dressing and cinematic puppets don't travel.
	// Mim is exempt from stepping entirely (smooth pose and position).
	if (IsSmoothMimMesh(Mesh)) return false;
	const APawn* Pawn = Cast<APawn>(Mesh->GetOwner());
	return Pawn && !Pawn->IsPlayerControlled();
}

void UTallowStopMotionSubsystem::Scan()
{
	const float Interval = 1.f / FMath::Max(CVarTallowOnTwosRate.GetValueOnGameThread(), 1.f);
	const bool bRateChanged = !FMath::IsNearlyEqual(Interval, AppliedInterval);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UActorComponent*> Components(*It);
		for (UActorComponent* Component : Components)
		{
			const bool bAnimated = Component->IsA<USkeletalMeshComponent>()
				|| Component->IsA<UTallowCandleFlameComponent>() || Component->IsA<UTallowFlameVisualComponent>();
			if (!bAnimated) continue;
			if (const auto* MimMesh = Cast<USkeletalMeshComponent>(Component); MimMesh && IsSmoothMimMesh(MimMesh))
			{
				// Off twos: give back every-frame ticking if an earlier scan stepped her.
				if (Stepped.Contains(Component))
				{
					Component->SetComponentTickInterval(0.f);
					Stepped.Remove(Component);
				}
				Held.RemoveAll([MimMesh](const FHeld& H) { return H.Component.Get() == MimMesh; });
				continue;
			}
			if (bRateChanged || !Stepped.Contains(Component))
			{
				Component->SetComponentTickInterval(Interval);
				Stepped.Add(Component);
			}
			auto* Mesh = Cast<USkeletalMeshComponent>(Component);
			if (Mesh && ShouldHoldTransform(Mesh)
				&& !Held.ContainsByPredicate([Mesh](const FHeld& H) { return H.Component.Get() == Mesh; }))
			{
				FHeld H; H.Component = Mesh; H.World = Mesh->GetComponentTransform();
				Held.Add(H);
			}
		}
	}
	AppliedInterval = Interval;
	Held.RemoveAll([](const FHeld& H) { return !H.Component.IsValid(); });
	for (auto It = Stepped.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
}

void UTallowStopMotionSubsystem::Release()
{
	for (FHeld& H : Held)
		if (H.bOverridden)
			if (USceneComponent* Component = H.Component.Get()) Component->SetRelativeTransform(H.Relative);
	for (const TWeakObjectPtr<UActorComponent>& Component : Stepped)
		if (Component.IsValid()) Component->SetComponentTickInterval(0.f);
	Held.Reset(); Stepped.Reset(); AppliedInterval = -1.f; NextScan = 0.f;
}

void UTallowStopMotionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	if (CVarTallowOnTwos.GetValueOnGameThread() == 0)
	{
		if (bActive) Release();
		bActive = false;
		return;
	}
	bActive = true;
	const float Now = World->GetTimeSeconds();
	if (Now >= NextScan) { Scan(); NextScan = Now + .25f; }
	// Runs after every tick group: the frame's placement is final. A held mesh takes its new
	// on-screen transform only on the frame its pose stepped, so pose and position step together.
	for (FHeld& H : Held)
	{
		auto* Mesh = Cast<USkeletalMeshComponent>(H.Component.Get());
		if (!Mesh) continue;
		if (Mesh->PoseTickedThisFrame()) { H.World = Mesh->GetComponentTransform(); continue; }
		H.Relative = Mesh->GetRelativeTransform();
		Mesh->SetWorldTransform(H.World, false, nullptr, ETeleportType::TeleportPhysics);
		H.bOverridden = true;
	}
}
