#include "Noise/TallowNoiseSubsystem.h"
#include "Perception/AISense_Hearing.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Noise/TallowNoiseOccluderComponent.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Tallow.h"

static TAutoConsoleVariable<bool> CVarNoiseDebug(
	TEXT("tallow.Noise.Debug"),
	false,
	TEXT("Draws the range of every noise event, and the occlusion traces to each creature"));

static TAutoConsoleVariable<bool> CVarNoiseOcclusion(
	TEXT("tallow.Noise.Occlusion"),
	true,
	TEXT("Whether walls and doors reduce noise"));

static TAutoConsoleVariable<float> CVarWallTransmission(
	TEXT("tallow.Noise.WallTransmission"),
	0.35f,
	TEXT("Fraction of sound that gets through a wall with no occluder component"));

/** Hits closer than this on the same component count as one wall, so both faces of a wall aren't counted twice */
static constexpr float WallMergeDistance = 60.0f;

/** Most traces a single occlusion check will make */
static constexpr int32 MaxOcclusionTraces = 12;

/** Below this almost nothing gets through, so stop tracing */
static constexpr float MinTransmission = 0.01f;

void UTallowNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Starting values, tune these once the house is blocked out
	NoiseProfiles.Add(ETallowNoiseType::Footstep, FTallowNoiseProfile(0.25f, 600.0f));
	NoiseProfiles.Add(ETallowNoiseType::CrouchStep, FTallowNoiseProfile(0.1f, 250.0f));
	NoiseProfiles.Add(ETallowNoiseType::RunStep, FTallowNoiseProfile(0.6f, 1500.0f));
	NoiseProfiles.Add(ETallowNoiseType::Land, FTallowNoiseProfile(0.5f, 1200.0f));
	NoiseProfiles.Add(ETallowNoiseType::MatchStrike, FTallowNoiseProfile(0.5f, 900.0f));
	NoiseProfiles.Add(ETallowNoiseType::ChairRock, FTallowNoiseProfile(0.7f, 1800.0f));
	NoiseProfiles.Add(ETallowNoiseType::LockSnap, FTallowNoiseProfile(0.6f, 1200.0f));
	NoiseProfiles.Add(ETallowNoiseType::FuseInsert, FTallowNoiseProfile(1.0f, 2500.0f));
	NoiseProfiles.Add(ETallowNoiseType::Pry, FTallowNoiseProfile(0.8f, 2000.0f));
	NoiseProfiles.Add(ETallowNoiseType::Projector, FTallowNoiseProfile(0.7f, 1800.0f));
	NoiseProfiles.Add(ETallowNoiseType::DoorOpen, FTallowNoiseProfile(0.3f, 800.0f));
	NoiseProfiles.Add(ETallowNoiseType::DoorSlam, FTallowNoiseProfile(0.9f, 2200.0f));
	NoiseProfiles.Add(ETallowNoiseType::PropFall, FTallowNoiseProfile(0.8f, 2000.0f));
	NoiseProfiles.Add(ETallowNoiseType::Custom, FTallowNoiseProfile(0.5f, 1000.0f));
}

UTallowNoiseSubsystem* UTallowNoiseSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UTallowNoiseSubsystem>() : nullptr;
}

void UTallowNoiseSubsystem::EmitNoise(ETallowNoiseType Type, FVector Location, AActor* Instigator, float LoudnessScale, bool bAlertsAI)
{
	const FTallowNoiseProfile Profile = GetNoiseProfile(Type);

	FTallowNoiseEvent NoiseEvent;
	NoiseEvent.Location = Location;
	NoiseEvent.Loudness = FMath::Clamp(Profile.Loudness * LoudnessScale, 0.0f, 1.0f);
	NoiseEvent.Range = Profile.Range * LoudnessScale;
	NoiseEvent.Type = Type;
	NoiseEvent.Instigator = Instigator;
	NoiseEvent.bAlertsAI = bAlertsAI;

	ProcessNoise(NoiseEvent);
}

void UTallowNoiseSubsystem::EmitCustomNoise(FVector Location, AActor* Instigator, float Loudness, float Range, bool bAlertsAI)
{
	FTallowNoiseEvent NoiseEvent;
	NoiseEvent.Location = Location;
	NoiseEvent.Loudness = FMath::Clamp(Loudness, 0.0f, 1.0f);
	NoiseEvent.Range = FMath::Max(Range, 0.0f);
	NoiseEvent.Type = ETallowNoiseType::Custom;
	NoiseEvent.Instigator = Instigator;
	NoiseEvent.bAlertsAI = bAlertsAI;

	ProcessNoise(NoiseEvent);
}

void UTallowNoiseSubsystem::ProcessNoise(FTallowNoiseEvent& NoiseEvent)
{
	UWorld* World = GetWorld();
	if (!World || NoiseEvent.Loudness <= 0.0f || NoiseEvent.Range <= 0.0f)
	{
		return;
	}

	NoiseEvent.Timestamp = World->GetTimeSeconds();

	// Drop old noises, then store this one
	NoiseHistory.RemoveAll([Now = NoiseEvent.Timestamp](const FTallowNoiseEvent& Old)
	{
		return Now - Old.Timestamp > HistoryLifetime;
	});
	NoiseHistory.Add(NoiseEvent);

	if (NoiseEvent.bAlertsAI)
	{
		ReportNoiseToAI(NoiseEvent);
	}

	DrawNoiseDebug(NoiseEvent);

	OnNoiseEmitted.Broadcast(NoiseEvent);
}

void UTallowNoiseSubsystem::ReportNoiseToAI(const FTallowNoiseEvent& NoiseEvent) const
{
	UWorld* World = GetWorld();

	// The hearing sense needs an instigator to register the stimulus
	AActor* Instigator = NoiseEvent.Instigator.Get();
	if (!Instigator)
	{
		Instigator = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
	}

	// Hearing reports go to every listener at once, so use the creature the noise reaches best.
	// With one creature in the house this is exact.
	float BestTransmission = 0.0f;
	bool bFoundListener = false;

	for (TActorIterator<AAIController> It(World); It; ++It)
	{
		const APawn* ListenerPawn = It->GetPawn();
		if (!ListenerPawn || !It->GetPerceptionComponent() || ListenerPawn == Instigator)
		{
			continue;
		}

		bFoundListener = true;

		const float Transmission = CVarNoiseOcclusion.GetValueOnGameThread()
			? TraceOcclusion(NoiseEvent.Location, ListenerPawn->GetPawnViewLocation(), Instigator, CVarNoiseDebug.GetValueOnGameThread())
			: 1.0f;

		BestTransmission = FMath::Max(BestTransmission, Transmission);
	}

	if (!bFoundListener || BestTransmission <= MinTransmission)
	{
		return;
	}

	// The hearing sense hears a noise within HearingRange * Loudness, capped at MaxRange, so scale both
	const FName Tag = FName(StaticEnum<ETallowNoiseType>()->GetNameStringByValue(static_cast<int64>(NoiseEvent.Type)));
	UAISense_Hearing::ReportNoiseEvent(World, NoiseEvent.Location, NoiseEvent.Loudness * BestTransmission, Instigator, NoiseEvent.Range * BestTransmission, Tag);
}

float UTallowNoiseSubsystem::GetOcclusion(FVector From, FVector To, AActor* IgnoredActor) const
{
	return TraceOcclusion(From, To, IgnoredActor, false);
}

float UTallowNoiseSubsystem::TraceOcclusion(const FVector& From, const FVector& To, AActor* IgnoredActor, bool bDrawDebug) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	const FVector Direction = (To - From).GetSafeNormal();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TallowNoiseOcclusion), false);
	QueryParams.AddIgnoredActor(IgnoredActor);

	float Transmission = 1.0f;
	FVector Start = From;
	const UPrimitiveComponent* LastComponent = nullptr;
	FVector LastHitLocation = FVector::ZeroVector;

	for (int32 TraceIndex = 0; TraceIndex < MaxOcclusionTraces && Transmission > MinTransmission; ++TraceIndex)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, To, ECC_Visibility, QueryParams))
		{
			if (bDrawDebug)
			{
				DrawDebugLine(World, Start, To, FColor::Green, false, 2.0f);
			}
			break;
		}

		AActor* HitActor = Hit.GetActor();

		// Characters don't block sound
		if (Cast<APawn>(HitActor))
		{
			QueryParams.AddIgnoredActor(HitActor);
			continue;
		}

		// Noises made on a floor or against a wall start touching it, so ignore hits right at the start of a trace
		if (Hit.bStartPenetrating || Hit.Distance < 5.0f)
		{
			Start += Direction * 5.0f;
			if (FVector::DotProduct(To - Start, Direction) <= 0.0f)
			{
				break;
			}
			continue;
		}

		const UTallowNoiseOccluderComponent* Occluder = HitActor ? HitActor->FindComponentByClass<UTallowNoiseOccluderComponent>() : nullptr;
		const float HitTransmission = Occluder ? Occluder->GetTransmission() : CVarWallTransmission.GetValueOnGameThread();

		// Open doors and anything else that lets all sound through are ignored from here on
		if (HitTransmission >= 1.0f && HitActor)
		{
			QueryParams.AddIgnoredActor(HitActor);
			continue;
		}

		// The back face of a wall already counted
		const bool bSameWall = Hit.GetComponent() == LastComponent && FVector::Dist(Hit.ImpactPoint, LastHitLocation) < WallMergeDistance;
		if (!bSameWall)
		{
			Transmission *= HitTransmission;
			LastComponent = Hit.GetComponent();
			LastHitLocation = Hit.ImpactPoint;
		}

		if (bDrawDebug)
		{
			DrawDebugLine(World, Start, Hit.ImpactPoint, FColor::Green, false, 2.0f);
			if (!bSameWall)
			{
				DrawDebugPoint(World, Hit.ImpactPoint, 10.0f, FColor::Red, false, 2.0f);
				DrawDebugString(World, Hit.ImpactPoint, FString::Printf(TEXT("%.2f"), Transmission), nullptr, FColor::Red, 2.0f);
			}
		}

		Start = Hit.ImpactPoint + Direction * 1.0f;
	}

	return FMath::Clamp(Transmission, 0.0f, 1.0f);
}

float UTallowNoiseSubsystem::GetLoudnessAt(const FTallowNoiseEvent& NoiseEvent, FVector ListenerLocation, bool bIncludeOcclusion) const
{
	if (NoiseEvent.Range <= 0.0f)
	{
		return 0.0f;
	}

	// Quadratic falloff, loud up close and dropping away quickly towards the edge of the range
	const float Distance = FVector::Dist(NoiseEvent.Location, ListenerLocation);
	const float Remaining = FMath::Clamp(1.0f - Distance / NoiseEvent.Range, 0.0f, 1.0f);

	float Loudness = NoiseEvent.Loudness * Remaining * Remaining;

	if (bIncludeOcclusion && Loudness > 0.0f && CVarNoiseOcclusion.GetValueOnGameThread())
	{
		Loudness *= TraceOcclusion(NoiseEvent.Location, ListenerLocation, NoiseEvent.Instigator.Get(), false);
	}

	return Loudness;
}

TArray<FTallowNoiseEvent> UTallowNoiseSubsystem::GetRecentNoises(float MaxAgeSeconds) const
{
	TArray<FTallowNoiseEvent> Result;

	if (const UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		for (const FTallowNoiseEvent& NoiseEvent : NoiseHistory)
		{
			if (Now - NoiseEvent.Timestamp <= MaxAgeSeconds)
			{
				Result.Add(NoiseEvent);
			}
		}
	}

	return Result;
}

FTallowNoiseProfile UTallowNoiseSubsystem::GetNoiseProfile(ETallowNoiseType Type) const
{
	if (const FTallowNoiseProfile* Profile = NoiseProfiles.Find(Type))
	{
		return *Profile;
	}

	return FTallowNoiseProfile();
}

void UTallowNoiseSubsystem::SetNoiseProfile(ETallowNoiseType Type, FTallowNoiseProfile Profile)
{
	NoiseProfiles.Add(Type, Profile);
}

void UTallowNoiseSubsystem::DrawNoiseDebug(const FTallowNoiseEvent& NoiseEvent) const
{
	if (!CVarNoiseDebug.GetValueOnGameThread())
	{
		return;
	}

	// Red for loud, yellow for quiet
	const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Red, NoiseEvent.Loudness).ToFColor(true);

	DrawDebugSphere(GetWorld(), NoiseEvent.Location, NoiseEvent.Range, 16, Color, false, 2.0f);
	DrawDebugPoint(GetWorld(), NoiseEvent.Location, 12.0f, Color, false, 2.0f);

	UE_LOG(LogTallow, Log, TEXT("Noise %s loudness %.2f range %.0f"),
		*StaticEnum<ETallowNoiseType>()->GetNameStringByValue(static_cast<int64>(NoiseEvent.Type)),
		NoiseEvent.Loudness, NoiseEvent.Range);
}
