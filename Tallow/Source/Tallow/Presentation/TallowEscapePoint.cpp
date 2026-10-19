#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowPlayDirector.h"
#include "Player/TallowCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
ATallowEscapePoint::ATallowEscapePoint()
{
	Latch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EscapeLatch"));
	RootComponent = Latch;
	Latch->SetCollisionProfileName(TEXT("BlockAll"));
}
bool ATallowEscapePoint::CanInteract_Implementation(AActor* Interactor) const
{
	const auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()));
	return Cast<ATallowCharacter>(Interactor) && Director && Director->CanEscape();
}
FText ATallowEscapePoint::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	return FText::FromString(CanInteract_Implementation(Interactor) ? TEXT("HOLD E — RELEASE THE LATCH") : TEXT("THE RELEASE NEEDS POWER"));
}
void ATallowEscapePoint::OnInteractComplete_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor)) return;
	if (auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()))) Director->FinishRun();
}
