#pragma once

#include "CoreMinimal.h"
#include "TallowNoiseTypes.generated.h"

class AActor;

/**
 *  Every kind of sound the game can make. Each type has a default profile in the noise subsystem.
 */
UENUM(BlueprintType)
enum class ETallowNoiseType : uint8
{
	Footstep,
	CrouchStep,
	RunStep,
	Land,
	MatchStrike,
	ChairRock,
	LockSnap,
	FuseInsert,
	Pry,
	Projector,
	DoorOpen,
	DoorSlam,
	DoorClose,
	PropFall,
	Custom
};

/**
 *  How loud a type of noise is and how far it can carry
 */
USTRUCT(BlueprintType)
struct FTallowNoiseProfile
{
	GENERATED_BODY()

	/** 0 to 1, how loud the noise is at its source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Loudness = 0.5f;

	/** Distance in cm at which the noise has fallen to nothing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.0", Units = "cm"))
	float Range = 1000.0f;

	FTallowNoiseProfile() {}
	FTallowNoiseProfile(float InLoudness, float InRange) : Loudness(InLoudness), Range(InRange) {}
};

/**
 *  A single noise made somewhere in the world
 */
USTRUCT(BlueprintType)
struct FTallowNoiseEvent
{
	GENERATED_BODY()

	/** Where the noise was made */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	FVector Location = FVector::ZeroVector;

	/** 0 to 1 loudness at the source */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	float Loudness = 0.0f;

	/** Distance in cm at which the noise has fallen to nothing */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	float Range = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	ETallowNoiseType Type = ETallowNoiseType::Custom;

	/** World time in seconds when the noise was made */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	double Timestamp = 0.0;

	/** Actor responsible for the noise */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	TObjectPtr<AActor> Instigator;

	/** False for noises the creature should never react to, like its own footsteps */
	UPROPERTY(BlueprintReadOnly, Category = "Noise")
	bool bAlertsAI = true;
};
