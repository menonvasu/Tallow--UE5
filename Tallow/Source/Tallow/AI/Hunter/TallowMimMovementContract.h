#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TallowMimMovementContract.generated.h"

class ACharacter;
class UCapsuleComponent;

UENUM(BlueprintType)
enum class ETallowMimPosture : uint8
{
    Normal,
    Compact
};

USTRUCT(BlueprintType)
struct TALLOW_API FTallowMimConfiguration
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ETallowMimPosture Posture = ETallowMimPosture::Normal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector MeshOffset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct TALLOW_API FTallowMimTransitionResult
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bValid = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCapsuleClear = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bBodyClear = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFloorSupported = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName FailureKind = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Blocker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Reason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Radius = 34.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HalfHeight = 50.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float InitialDepth = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector TargetPosition = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FHitResult Hit;
};

/**
 * The one movement contract for Mim: a strict MEASURING instrument, one travel policy.
 *
 * validate*() measure a transition strictly and report root capsule, torso/head proxies
 * and floor support separately, never fudged. Travel then works like this (and only
 * like this -- every layer that used to veto on its own is what made her stall):
 *
 * - CharacterMovement + the navmesh are the physical authority. The capsule really
 *   collides, slides and steps; paths only exist where the nav agent fits. No
 *   contract result vetoes a move: duplicates of capsule/floor physics with
 *   different tolerances disagreed with the real thing and froze her mid-slide.
 * - Contract results choose POSTURE and feed recovery: IsCapsuleBlocked folds her
 *   to Compact and, sustained, starts the compact-capsule fit assist; IsTorsoBrush
 *   is the normal state of a ~3 m soft body indoors and only informs fit/speed.
 * - Posture changes are the one place a result can refuse: growing back to Normal
 *   requires a fully valid transition (never expand into geometry). Shrinking is
 *   always attempted -- smaller cannot fit worse -- and execution verifies it.
 *
 * Hands/arms/fur remain animation contacts; they never gate ordinary travel.
 */
UCLASS()
class TALLOW_API UTallowMimMovementContract : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static void BuildCollisionPolicy(const UCapsuleComponent* Capsule,
        FCollisionQueryParams& Query, FCollisionResponseParams& Responses);
    static FVector2D PostureSize(ETallowMimPosture Posture);

    /** Travel policy helpers, shared by planner and executor. See class comment.
     *  Neither is a movement veto: they classify a measured result for posture,
     *  status and recovery decisions. */
    UFUNCTION(BlueprintPure, Category="Mim|Movement")
    static bool IsCapsuleBlocked(const FTallowMimTransitionResult& Result)
    { return !Result.bCapsuleClear; }
    UFUNCTION(BlueprintPure, Category="Mim|Movement")
    static bool IsTorsoBrush(const FTallowMimTransitionResult& Result)
    { return !Result.bBodyClear && Result.bCapsuleClear; }

    UFUNCTION(BlueprintPure, Category="Mim|Movement")
    static FTallowMimConfiguration GetConfiguration(ACharacter* Character);

    UFUNCTION(BlueprintCallable, Category="Mim|Movement")
    static FTallowMimTransitionResult ValidateTransition(ACharacter* Character,
        const FTallowMimConfiguration& From, const FTallowMimConfiguration& To, bool bRequireFloor = true);

    UFUNCTION(BlueprintCallable, Category="Mim|Movement")
    static FTallowMimTransitionResult ValidateGroundMove(ACharacter* Character,
        const FVector& Toward, float DesiredYaw, ETallowMimPosture Posture, float MaxDistance = 12.f);

    UFUNCTION(BlueprintCallable, Category="Mim|Movement")
    static FTallowMimTransitionResult TryChangePosture(ACharacter* Character, ETallowMimPosture Posture);
};
