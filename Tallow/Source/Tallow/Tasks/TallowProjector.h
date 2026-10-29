#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowProjector.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ATallowFuseBox;
class UDecalComponent;
class UTextRenderComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EProjectorState : uint8 { Empty, Threaded, Projecting, Finished };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectorStateChanged, EProjectorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReelLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProjectorCodeRevealed, int32, Value, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProjectorFinished);

/** A physical reel, manual threading, then a repeatable five-digit projection. */
UCLASS()
class TALLOW_API ATallowProjector : public AActor, public IInteractable
{
 GENERATED_BODY()
public:
 ATallowProjector();
 virtual void OnInteract_Implementation(AActor* Interactor) override;
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
 virtual float GetInteractionProgress_Implementation(AActor* Interactor) const override;
 UFUNCTION(BlueprintPure, Category="Projector") bool IsReelLoaded() const { return bReelLoaded; }
 UFUNCTION(BlueprintPure, Category="Projector") TArray<int32> GetCodes() const { return Codes; }
 UFUNCTION(BlueprintPure, Category="Projector") EProjectorState GetProjectorState() const { return CurrentState; }
 UFUNCTION(BlueprintPure, Category="Projector") bool HasCompletedProjection() const { return bHasFinishedProjectingOnce; }
 UFUNCTION(BlueprintPure, Category="Projector") float GetThreadProgress() const { return ThreadProgress; }
 /** Full retry: consumed reel returns to its placed pickup through GameMode's reset. */
 UFUNCTION(BlueprintCallable, Category="Projector|Reset") void ResetProjector();
 /** Stops timers, motor, light and noise; a threaded reel can be projected again. */
 UFUNCTION(BlueprintCallable, Category="Projector") void StopProjection();
 UPROPERTY(BlueprintAssignable) FOnProjectorStateChanged OnProjectorStateChanged;
 UPROPERTY(BlueprintAssignable) FOnProjectorCodeRevealed OnProjectorCodeRevealed;
 UPROPERTY(BlueprintAssignable) FOnProjectorFinished OnProjectorFinished;
 UPROPERTY(BlueprintAssignable) FOnReelLoaded OnReelLoaded;
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaTime) override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 UFUNCTION() void ProjectorFinished();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UStaticMeshComponent> ProjectorMesh;
 /** Supply reel centre; local Y is its axle. Hidden until a reel is loaded. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UStaticMeshComponent> SpoolMesh;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UStaticMeshComponent> TakeupSpoolMesh;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UStaticMeshComponent> InsertionReelMesh;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<USpotLightComponent> ProjectorLight;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UDecalComponent> ProjectionDecal;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector") TObjectPtr<UTextRenderComponent> ProjectedDigit;
 UPROPERTY(EditInstanceOnly, Category="Projector") TObjectPtr<ATallowFuseBox> LinkedFuseBox;
 UPROPERTY(EditAnywhere, Category="Projector") bool bRequiresPower = true;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector|Audio") TObjectPtr<UAudioComponent> ClatterAudioComponent;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector|Audio") TObjectPtr<UAudioComponent> ProjectionAudioComponent;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projector|Audio") TObjectPtr<UAudioComponent> ReelInsertAudioComponent;
 UPROPERTY(EditAnywhere, Category="Projector|Audio") TObjectPtr<USoundBase> ClatterLoopSound;
 UPROPERTY(EditAnywhere, Category="Projector|Audio") TObjectPtr<USoundBase> ProjectorLoopSound;
 UPROPERTY(EditAnywhere, Category="Projector|Audio") TObjectPtr<USoundBase> ReelInsertSound;
 UPROPERTY(EditAnywhere, Category="Projector|Audio") float MinPitch = .7f;
 UPROPERTY(EditAnywhere, Category="Projector|Audio") float MaxPitch = 1.25f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading", meta=(ClampMin=".01",ClampMax="1")) float ProgressPerTap = .1f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading", meta=(ClampMin="0")) float DecayPerSecond = .15f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading", meta=(ClampMin="0")) float DecayDelay = .4f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading", meta=(ClampMin="0")) float InputLockoutAfterThreshold = .75f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading", meta=(ClampMin=".1")) float ReelInsertDuration = .55f;
 UPROPERTY(EditAnywhere, Category="Projector|Threading") float TapLoudness = .5f;
 UPROPERTY(EditAnywhere, Category="Projector|Projecting") TArray<int32> Codes;
 UPROPERTY(EditAnywhere, Category="Projector|Projecting", meta=(ClampMin=".1")) float ShowDuration = 1.5f;
 UPROPERTY(EditAnywhere, Category="Projector|Projecting", meta=(ClampMin=".05")) float GapDuration = .5f;
 UPROPERTY(EditAnywhere, Category="Projector|Projecting", meta=(ClampMin=".1")) float OperationNoiseInterval = .6f;
 UPROPERTY(EditAnywhere, Category="Projector|Projecting", meta=(ClampMin="0",ClampMax="1")) float OperationLoudness = .65f;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") TObjectPtr<UMaterialInterface> DecalMaterialBase;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float ProjectionIntensity = 5000.f;
 /** Degrees per second around local Y; takeup turns in the same film direction. */
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float RotationSpeedNormal = 150.f;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float RotationSpeedSlow = 65.f;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") FName DigitParam = TEXT("Digit");
 UPROPERTY(EditAnywhere, Category="Projector|Visual") FName VisibleParam = TEXT("Visible");
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float MaxProjectionDistance = 2000.f;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float DecalDepth = 60.f;
 UPROPERTY(EditAnywhere, Category="Projector|Visual") float DecalRollOffset = 90.f;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DecalMID;
 FTimerHandle GapTimerHandle, CodeTimerHandle, EndTimerHandle;
 void HideCode();
private:
 void SetState(EProjectorState NewState);
 void RevealNextCode();
 void PlaceDecal();
 void SetDigitVisible(bool bVisible);
 void EmitOperationNoise(AActor* Instigator, float Loudness);
 bool HasPower() const;
 EProjectorState CurrentState = EProjectorState::Empty;
 float ThreadProgress = 0.f, TimeSinceLastTap = 0.f, LockoutEndTime = 0.f;
 float NoiseCountdown = 0.f, InsertAge = -1.f;
 FTransform InsertStart;
 FRotator SupplyRestRotation, TakeupRestRotation;
 int32 CodeIndex = 0;
 bool bReelLoaded = false, bHasFinishedProjectingOnce = false;
 TWeakObjectPtr<AActor> Operator;
};
