#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowJournal.generated.h"

class UStaticMeshComponent;
class UTallowJournalWidget;

/**
 * Maisie's notebook in the first room, left for whoever came next. 25 Oct: the game
 * teaches itself through this rather than on-screen notes. READ opens its pages.
 */
UCLASS()
class TALLOW_API ATallowJournal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATallowJournal();

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	TArray<FText> Pages;

	/** Set in type, not a child's hand: the newspaper, the posters, the register. The same pickup reads
	 *  every lore page in the house (TALLOW-LORE.md); nothing counts them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	bool bPrinted = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Journal")
	TObjectPtr<UStaticMeshComponent> Book;

	UPROPERTY(Transient)
	TObjectPtr<UTallowJournalWidget> Widget;
};
