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

UENUM(BlueprintType)
enum class EProjectorState : uint8
{
	Empty,
	Threaded,
	Projecting,
	Finished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectorStateChanged, EProjectorState, NewState);
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
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UStaticMeshComponent> ProjectorMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UStaticMeshComponent> SpoolMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<USpotLightComponent> ProjectorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projector")
	TObjectPtr<UAudioComponent> ClatterAudioComponent;
	
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
	float TapLoudness = 0.3f;
	
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
	
	void HideCode(); // timer callback 
	
private:
	void SetState(EProjectorState NewState);
	void RevealNextCode();
	
	EProjectorState CurrentState = EProjectorState::Empty;
	FTimerHandle CodeTimerHandle;
	float ThreadProgress = 0.0f;
	float TimeSinceLastTap = 0.0f;
	float LockoutRemaining = 0.0f;
	int32 CodeIndex = 0;
	bool bReelLoaded = false;
	float LockoutEndTime = 0.0f;
};
