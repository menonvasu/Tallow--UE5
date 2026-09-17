#include "Noise/TallowNoiseSubsystem.h"
#include "Perception/AISense_Hearing.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Tallow.h"

static TAutoConsoleVariable<bool> CVarNoiseDebug(
	TEXT("tallow.Noise.Debug"),
	false,
	TEXT("Draws the range of every noise event"));

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

	// Pass to AI Perception hearing. The hearing sense needs an instigator to register the stimulus
	if (NoiseEvent.bAlertsAI)
	{
		AActor* Instigator = NoiseEvent.Instigator.Get();
		if (!Instigator)
		{
			Instigator = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
		}

		const FName Tag = FName(StaticEnum<ETallowNoiseType>()->GetNameStringByValue(static_cast<int64>(NoiseEvent.Type)));
		UAISense_Hearing::ReportNoiseEvent(World, NoiseEvent.Location, NoiseEvent.Loudness, Instigator, NoiseEvent.Range, Tag);
	}

	DrawNoiseDebug(NoiseEvent);

	OnNoiseEmitted.Broadcast(NoiseEvent);
}

float UTallowNoiseSubsystem::GetLoudnessAt(const FTallowNoiseEvent& NoiseEvent, FVector ListenerLocation) const
{
	if (NoiseEvent.Range <= 0.0f)
	{
		return 0.0f;
	}

	// Quadratic falloff, loud up close and dropping away quickly towards the edge of the range
	const float Distance = FVector::Dist(NoiseEvent.Location, ListenerLocation);
	const float Remaining = FMath::Clamp(1.0f - Distance / NoiseEvent.Range, 0.0f, 1.0f);

	return NoiseEvent.Loudness * Remaining * Remaining;
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
