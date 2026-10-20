#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowFilmReelPickup.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFilmReelPickup);

UCLASS()
class TALLOW_API ATallowFilmReelPickup : public AActor, public IInteractable
{
	GENERATED_BODY()
	
public:
	ATallowFilmReelPickup();
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "FilmReel|Events" )
	FOnFilmReelPickup OnFilmReelPickup;
	
	UFUNCTION(BlueprintCallable, Category = "FilmReel|Reset")
	void ResetReelPickup();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FilmReel")
	TObjectPtr<UStaticMeshComponent> FilmReelMesh;
};
