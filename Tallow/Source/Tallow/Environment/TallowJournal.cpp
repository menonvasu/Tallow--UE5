#include "Environment/TallowJournal.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Presentation/TallowNarrative.h"
#include "UI/Presentation/TallowJournalWidget.h"

#define LOCTEXT_NAMESPACE "TallowJournal"

ATallowJournal::ATallowJournal()
{
	PrimaryActorTick.bCanEverTick = false;
	Book = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Book"));
	RootComponent = Book;
	Book->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Book->SetCollisionResponseToAllChannels(ECR_Ignore);
	Book->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Maisie's book (TALLOW-LORE.md, doc 1): ordinary pages first, then what she left for whoever came next. The game
	// teaches itself through these rather than on-screen notes. Keys match IMC_Default.
	Pages = TallowNarrative::Pages(TEXT("journal"));
}

bool ATallowJournal::CanInteract_Implementation(AActor* Interactor) const
{
	return Cast<APawn>(Interactor) != nullptr && (!Widget || !Widget->IsOpen() || Widget->IsCompact());
}

FText ATallowJournal::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	return LOCTEXT("Read", "READ");
}

void ATallowJournal::OnInteract_Implementation(AActor* Interactor)
{
	const APawn* Reader = Cast<APawn>(Interactor);
	APlayerController* PC = Reader ? Cast<APlayerController>(Reader->GetController()) : nullptr;
	if (!PC) return;
	FName DocumentKey = TEXT("journal");
	// Resolve the current script by stable lore key, including older saved map instances.
	for (const FName& Tag : Tags)
	{
		const FString Name=Tag.ToString();
		if(Name.StartsWith(TEXT("Tallow.Lore.")))
		{
			const FName Key(*Name.RightChop(12)); const auto Current=TallowNarrative::Pages(Key);
			DocumentKey = Key;
			if(!Current.IsEmpty()){ Pages=Current; bPrinted=TallowNarrative::Printed(Key); }
		}
	}
	if (!Widget) Widget = CreateWidget<UTallowJournalWidget>(PC);
	if (Widget) Widget->Open(PC, Pages, bPrinted, DocumentKey);
	// The story remembers what was read (the constable's letter, the portrait): tag Tallow.Lore.<key>.
	if (auto* Story = UGameplayStatics::GetGameInstance(this) ? UGameplayStatics::GetGameInstance(this)->GetSubsystem<UTallowStorySubsystem>() : nullptr)
		for (const FName& Tag : Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(TEXT("Tallow.Lore."))) Story->NoteRead(FName(*Name.RightChop(12)));
		}
}

#undef LOCTEXT_NAMESPACE
