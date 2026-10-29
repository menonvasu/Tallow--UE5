#pragma once

#include "CoreMinimal.h"
#include "Environment/TallowJournal.h"
#include "TallowSoundBite.generated.h"

/**
 * A lore reading left as a small tape: the paper pickups of TALLOW-LORE.md are recordings now, the
 * same Tallow.Lore.<key> tag and the same journal reading as the paper they replace, tucked away at once
 * so she keeps moving while it plays. Optional and uncounted; a tape on a wall hangs by its reel.
 */
UCLASS()
class TALLOW_API ATallowSoundBite : public ATallowJournal
{
	GENERATED_BODY()

public:
	ATallowSoundBite();

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Sound Bite") float ReelDegreesPerSecond = 160.f;
	UPROPERTY(VisibleAnywhere, Category = "Sound Bite") TObjectPtr<UStaticMeshComponent> ReelA;
	UPROPERTY(VisibleAnywhere, Category = "Sound Bite") TObjectPtr<UStaticMeshComponent> ReelB;
	/** The nail a tape on a wall hangs from; no mesh on a tape that sits on a surface. */
	UPROPERTY(VisibleAnywhere, Category = "Sound Bite") TObjectPtr<UStaticMeshComponent> Nail;

	UFUNCTION(BlueprintPure, Category = "Sound Bite") UTallowJournalWidget* GetReader() const { return Widget; }
	FName GetReading() const { return Reading; }
	bool IsPlaying() const;

private:
	FName Reading = TEXT("journal");
};
