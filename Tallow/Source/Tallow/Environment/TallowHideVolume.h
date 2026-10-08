#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowHideVolume.generated.h"

class UBoxComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHide, bool, bIsHidden);

UENUM(BlueprintType)
enum class ETallowHidingSpotType : uint8
{
	Wardrobe UMETA(DisplayName = "Wardrobe"),
	UnderBed UMETA(DisplayName = "Under Bed")
};

UCLASS()
class TALLOW_API ATallowHideVolume : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	// Constructor
	ATallowHideVolume();

	// Hide Event
	UPROPERTY(BlueprintAssignable, Category = "Hide|Events")
	FOnHide OnHide;

	UFUNCTION(BlueprintPure, Category = "Hide")
	ETallowHidingSpotType GetHidingSpotType() const { return HidingSpotType; }

	UFUNCTION(BlueprintPure, Category = "Hide")
	USceneComponent* GetExitPoint() const { return ExitPoint; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Hide|Components")
	TObjectPtr<UBoxComponent> HideBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hide|Components")
	TObjectPtr<USceneComponent> HidingViewPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hide|Components")
	TObjectPtr<USceneComponent> ExitPoint;

	/** Cached controller for endplay */
	TWeakObjectPtr<APlayerController> CachedPC;

	bool bIsHidden = false;

	/** Camera limits */
	float CachedPitchMin = 0.0f;
	float CachedPitchMax = 0.0f;
	float CachedYawMin = 0.0f;
	float CachedYawMax = 0.0f;

	/** Type of hiding spot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hide|Setup")
	ETallowHidingSpotType HidingSpotType = ETallowHidingSpotType::Wardrobe;

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	/** Runs on entering */
	void EnterHiddenSpot(APlayerController* PC);

	/** Runs on exiting */
	void ExitHiddenSpot(APlayerController* PC);

	/** Handles rare cases */
	void EndPlay(const EEndPlayReason::Type EndPlayReason);
};