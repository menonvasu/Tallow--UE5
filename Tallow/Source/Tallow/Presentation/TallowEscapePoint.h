#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowEscapePoint.generated.h"
class UStaticMeshComponent;
class UAudioComponent;
class ATallowProjector;
class UTallowKeypadWidget;
class USoundBase;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWrongAccessCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCorrectAccessCode);

/** The powered release: watch the film, enter its digits, then pull the lever. */
UCLASS()
class TALLOW_API ATallowEscapePoint : public AActor, public IInteractable
{
 GENERATED_BODY()
public:
 ATallowEscapePoint();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escape") TObjectPtr<UStaticMeshComponent> Latch;
 /** Pivot (6.5,+7.5,8), imported local -Y handle; local Roll rises to +25 degrees on a complete pull. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escape") TObjectPtr<UStaticMeshComponent> ReleaseLeverMesh;
 virtual float GetInteractionDuration_Implementation() const override;
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
 virtual void OnInteract_Implementation(AActor* Interactor) override;
 virtual void OnInteractStart_Implementation(AActor* Interactor) override;
 virtual void OnInteractTick_Implementation(AActor* Interactor,float DeltaTime,float Progress) override;
 virtual void OnInteractCancel_Implementation(AActor* Interactor) override;
 virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
 UFUNCTION(BlueprintCallable, Category="Escape") bool CheckSubmittedCode(const TArray<int32>& EnteredCode);
 UFUNCTION(BlueprintPure, Category="Escape") bool GetIsCurrentlyInputtingKeypad() const { return bIsCurrentlyInputtingKeypad; }
 UFUNCTION(BlueprintPure, Category="Escape") bool IsCodeVerified() const { return bCodeVerified; }
 UFUNCTION(BlueprintCallable, Category="Escape") void SetCodeVerified(bool bNewState);
 UFUNCTION(BlueprintCallable, Category="Escape") void CloseKeypad();
 UPROPERTY(BlueprintAssignable, BlueprintCallable, Category="Escape|Events") FOnWrongAccessCode OnWrongAccessCode;
 UPROPERTY(BlueprintAssignable, BlueprintCallable, Category="Escape|Events") FOnCorrectAccessCode OnCorrectAccessCode;
protected:
 UFUNCTION() void HandlePlayerCaptured();
 UPROPERTY(EditInstanceOnly, Category="Escape") TObjectPtr<ATallowProjector> LinkedProjector;
 UPROPERTY(EditDefaultsOnly, Category="Escape|UI") TSubclassOf<UTallowKeypadWidget> KeypadWidgetClass;
 UPROPERTY(Transient) TObjectPtr<UTallowKeypadWidget> ActiveWidgetInstance;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escape|Sound") TObjectPtr<UAudioComponent> KeypadAudio;
 UPROPERTY(EditAnywhere, Category="Escape|Sound") TObjectPtr<USoundBase> ErrorSound;
 UPROPERTY(EditAnywhere, Category="Escape|Sound") TObjectPtr<USoundBase> AcceptSound;
 UPROPERTY(EditDefaultsOnly, Category="Escape|Sound") float InputFailedLoudness=.8f;
 UPROPERTY(EditDefaultsOnly, Category="Escape|Sound", meta=(ClampMin="0",Units="cm")) float InputFailedRange=1200.f;
 TWeakObjectPtr<APlayerController> CachedPC;
 bool bCodeVerified=false, bIsCurrentlyInputtingKeypad=false, bHadCursor=false;
 FRotator LeverRestRotation;
private:
 bool IsReleasePowered() const;
};
