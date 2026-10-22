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

UENUM(BlueprintType)
enum class EProjectorState : uint8
{
	Empty,
	Threaded,
	Projecting,
	Finished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectorStateChanged, EProjectorState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReelLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProjectorCodeRevealed, int32, Value, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProjectorFinished);

UCLASS()
class TALLOW_API ATallowProjector : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATallowProjector();
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	TArray<int32> GetCodes() const { return Codes; }
	
	UFUNCTION(BlueprintCallable, Category = "Projector|Reset")
	void ResetProjector();
	
	UFUNCTION(BlueprintPure, Category = "Projector")
	float GetThreadProgress() const { return ThreadProgress; }
	
	UPROPERTY(BlueprintAssignable)
	FOnProjectorStateChanged OnProjectorStateChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnProjectorCodeRevealed OnProjectorCodeRevealed;
	
	UPROPERTY(BlueprintAssignable)
	FOnProjectorFinished OnProjectorFinished;
	
	UPROPERTY(BlueprintCallable)
	FOnReelLoaded OnReelLoaded;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void ProjectorFinished();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UStaticMeshComponent> ProjectorMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UStaticMeshComponent> SpoolMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<USpotLightComponent> ProjectorLight;

	UPROPERTY(EditInstanceOnly) 
	TObjectPtr<ATallowFuseBox> LinkedFuseBox;
	
	// audio
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector|Audio")
	TObjectPtr<UAudioComponent> ClatterAudioComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector|Audio")
	TObjectPtr<UAudioComponent> ProjectionAudioComponent;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Audio")
	USoundBase* ClatterLoopSound;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Audio")
	USoundBase* ProjectorLoopSound;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Audio")
	float MinPitch = 0.5f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Audio")
	float MaxPitch = 2.0f;
	
	// mash tuning
	UPROPERTY(EditAnywhere, Category = "Projector|Threading")
	float ProgressPerTap = 0.1f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Threading")
	float DecayPerSecond = 0.15f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Threading")
	float DecayDelay = 0.4f; // grace before draining
	
	UPROPERTY(EditAnywhere, Category = "Projector|Threading")
	float InputLockoutAfterThreshold = 0.75f; // to prevent e spam into projector start
	
	UPROPERTY(EditAnywhere, Category = "Projector|Threading")
	float TapLoudness = 0.5f;
	
	// projecting tuning
	UPROPERTY(EditAnywhere, Category = "Projector|Projecting") 
	TArray<int32> Codes;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Projecting")
	float ShowDuration = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Projecting")
	float GapDuration = 0.5f;
	
	// visual
	UPROPERTY(EditAnywhere, Category = "Projector|Visual") 
	TObjectPtr<UMaterialInterface> LightFunctionBase;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Visual")
	float ProjectionIntensity = 5000.0f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Visual")
	float RotationSpeedNormal = 0.5f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Visual")
	float RotationSpeedSlow = 0.2f;
	
	UPROPERTY(EditAnywhere, Category = "Projector|Visual")
	FName DigitParam = TEXT("Digit");
	
	UPROPERTY(EditAnywhere, Category = "Projector|Visual")
	FName VisibleParam = TEXT("Visible");
	
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LightMID;
	
	FTimerHandle GapTimerHandle;
	FTimerHandle CodeTimerHandle;
	FTimerHandle EndTimerHandle;
	
	void HideCode(); // timer callback 
	
private:
	void SetState(EProjectorState NewState);
	void RevealNextCode();
	
	EProjectorState CurrentState = EProjectorState::Empty;
	float ThreadProgress = 0.0f;
	float TimeSinceLastTap = 0.0f;
	int32 CodeIndex = 0;
	bool bReelLoaded = false;
	float LockoutEndTime = 0.0f;
	bool bHasFinishedProjectingOnce = false;
};
