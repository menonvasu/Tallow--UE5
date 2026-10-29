#include "Environment/TallowSoundBite.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Presentation/TallowStorySubsystem.h"
#include "UI/Presentation/TallowJournalWidget.h"

ATallowSoundBite::ATallowSoundBite()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = true;
	for (auto* Reel : {&ReelA, &ReelB})
	{
		*Reel = CreateDefaultSubobject<UStaticMeshComponent>(Reel == &ReelA ? TEXT("ReelA") : TEXT("ReelB"));
		(*Reel)->SetupAttachment(Book); (*Reel)->SetCollisionEnabled(ECollisionEnabled::NoCollision); (*Reel)->SetCanEverAffectNavigation(false);
	}
	Nail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Nail"));
	Nail->SetupAttachment(Book); Nail->SetCollisionEnabled(ECollisionEnabled::NoCollision); Nail->SetCanEverAffectNavigation(false);
}

bool ATallowSoundBite::IsPlaying() const
{
	return Widget && Widget->IsOpen() && Widget->GetDocumentKey() == Reading && Widget->IsReading();
}

bool ATallowSoundBite::CanInteract_Implementation(AActor* Interactor) const
{
	return Cast<APawn>(Interactor) != nullptr;
}

FText ATallowSoundBite::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	return FText::FromString(IsPlaying() ? TEXT("Stop the tape") : TEXT("Listen"));
}

void ATallowSoundBite::OnInteract_Implementation(AActor* Interactor)
{
	if (IsPlaying()) { Widget->SkipReading(); return; }
	// The reading itself is the paper's: same key, same pages, same NoteRead.
	for (const FName& Tag : Tags)
		if (Tag.ToString().StartsWith(TEXT("Tallow.Lore."))) Reading = FName(*Tag.ToString().RightChop(12));
	Super::OnInteract_Implementation(Interactor);
	if (!Widget || !Widget->IsOpen()) return;
	// She asked to hear the tape, even with paper voices off; that choice is not changed for the papers.
	if (Widget->IsNarrationMuted())
	{
		Widget->ReplayReading();
		if (auto* Story = UTallowStorySubsystem::GetStory(this)) Story->SetDocumentNarrationMuted(true);
	}
	Widget->MinimizeReading();
}

void ATallowSoundBite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsPlaying()) return;
	for (auto* Reel : {ReelA.Get(), ReelB.Get()}) Reel->AddLocalRotation(FRotator(0, -ReelDegreesPerSecond * DeltaSeconds, 0));
}
