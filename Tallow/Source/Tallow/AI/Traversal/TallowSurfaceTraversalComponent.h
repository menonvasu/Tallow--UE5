#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowSurfaceTraversalComponent.generated.h"

class ACharacter;
/** Swept, explicitly authored traversal. Anchors are contact positions, never pawn centres. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowSurfaceTraversalComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTallowSurfaceTraversalComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 UFUNCTION(BlueprintCallable, Category="Traversal") bool StartSurfaceRoute();
 UFUNCTION(BlueprintCallable, Category="Traversal") bool StartSqueeze(AActor* Entry, AActor* Exit);
 /** Reset/capture must call this before resetting the actor transform. */
 UFUNCTION(BlueprintCallable, Category="Traversal") void ResetTraversal();
 UFUNCTION(BlueprintCallable, Category="Traversal") void FreezeForCapture();
 UFUNCTION(BlueprintPure, Category="Traversal") bool IsCurrentPoseClear() const;
 /** Retrace checked segments to the last grounded entry before resuming pursuit. */
 UFUNCTION(BlueprintCallable, Category="Traversal") void RequestGroundExit();
 UFUNCTION(BlueprintPure, Category="Traversal") bool IsTraversing() const { return bActive; }
 UFUNCTION(BlueprintPure, Category="Traversal") bool IsSqueezing() const { return bActive && bSqueeze; }
 UFUNCTION(BlueprintPure, Category="Traversal") FVector GetSurfaceNormal() const { return SurfaceNormal; }
 UFUNCTION(BlueprintPure, Category="Traversal") float GetTraversalSpeed() const { return ActualSpeed; }
 UFUNCTION(BlueprintPure, Category="Traversal") float GetSqueezeAlpha() const { return SqueezeAlpha; }
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal") bool bAutomaticTraversal = true;
 /** Wander the wall/ceiling route on its own while patrolling. Off: the climb pauses the AI brain, so it went deaf mid-hunt. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal") bool bAutomaticSurfaceRoute = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal", meta=(ClampMin="1")) float CrawlSpeed = 60.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal", meta=(ClampMin="1")) float CeilingSpeed = 22.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal", meta=(ClampMin="1")) float SqueezeSpeed = 65.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal") float RouteCooldown = 25.f;
 /** In a chase, go up the nearest wall, across the ceiling toward the player and drop near them. The AI brain keeps running. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase") bool bChaseSurfaceRoutes = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="0")) float ChaseRouteDelay = 3.2f;          // let the reveal finish first
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="0")) float ChaseRouteIntervalMin = 2.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="0")) float ChaseRouteIntervalMax = 5.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="1")) float ChaseCrawlSpeed = 360.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="1")) float ChaseCeilingSpeed = 300.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traversal|Chase", meta=(ClampMin="0.1")) float ChaseTurnRate = 5.f;  // rad/s while re-orienting between surfaces
 UFUNCTION(BlueprintCallable, Category="Traversal") bool StartChaseSurfaceRoute(const FVector& TargetLocation);
 UFUNCTION(BlueprintPure, Category="Traversal") bool IsChaseRoute() const { return bActive && bChaseRoute; }
private:
 struct FWaypoint { FVector Position; FQuat Rotation; FVector Normal; };
 TArray<FWaypoint> Waypoints;
 TArray<FWaypoint> SafeHistory;
 TWeakObjectPtr<ACharacter> Character;
 int32 WaypointIndex = 0;
 bool bCaptureFrozen = false;
 bool bActive = false, bSqueeze = false, bReturning = false, bPausedBrain = false, bChaseRoute = false, bWasChasing = false;
 float ChaseStartedAt = 0.f, NextChaseRouteTime = 0.f;
 bool bSavedOrient = false, bSavedControllerYaw = false, bSavedControllerDesired = false;
 float SavedGravity = 1.f, CooldownUntil = 5.f, NextProbeTime = 5.f, ActualSpeed = 0.f, SqueezeAlpha = 0.f;
 FVector SurfaceNormal = FVector::UpVector;
 bool BeginTraversal(TArray<FWaypoint>&& Points, bool bIsSqueeze, bool bKeepBrain = false);
 bool Trace(const FVector& From, const FVector& To, FHitResult& Hit) const;
 void TryChaseSurfaceRoute();
 bool PoseClear(const FVector& Position, const FQuat& Rotation) const;
 bool SweepStep(const FWaypoint& Target, float DeltaTime);
 void FinishTraversal();
 void TryAutomaticTraversal();
 static FString TagValue(const AActor* Actor, const FString& Prefix);
};
