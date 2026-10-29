#include "Environment/TallowCollectionSlot.h"

#include "Candle/TallowCandleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Environment/TallowCandlePickup.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowPlayDirector.h"
#include "Presentation/TallowStorySubsystem.h"

#define LOCTEXT_NAMESPACE "TallowCollectionSlot"

ATallowCollectionSlot::ATallowCollectionSlot()
{
	PrimaryActorTick.bCanEverTick = false;
	Grab = CreateDefaultSubobject<UBoxComponent>(TEXT("Grab"));
	RootComponent = Grab;
	Grab->SetBoxExtent(FVector(10.f, 10.f, 14.f));
	Grab->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Grab->SetCollisionResponseToAllChannels(ECR_Ignore);
	Grab->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	LeftCandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftCandle"));
	LeftCandle->SetupAttachment(Grab);
	LeftCandle->SetRelativeLocation(FVector(0.f, 0.f, -14.f));    // stands on the shelf the slot sits on
	LeftCandle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftCandle->SetHiddenInGame(true);
}

void ATallowCollectionSlot::BeginPlay()
{
	Super::BeginPlay();
	// The same candle Nell picked up in the cellar.
	for (TActorIterator<ATallowCandlePickup> It(GetWorld()); It; ++It)
		if (const auto* Mesh = It->FindComponentByClass<UStaticMeshComponent>())
		{
			LeftCandle->SetStaticMesh(Mesh->GetStaticMesh());
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I) LeftCandle->SetMaterial(I, Mesh->GetMaterial(I));
			LeftCandle->SetRelativeScale3D(Mesh->GetComponentScale());
			break;
		}
}

bool ATallowCollectionSlot::CanInteract_Implementation(AActor* Interactor) const
{
	const auto* Player = Cast<ATallowCharacter>(Interactor);
	const auto* Candle = Player ? Player->GetCandleComponent() : nullptr;
	const auto* Director=Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this,ATallowPlayDirector::StaticClass()));
	return !bFilled && Player && Director && Director->CanEscape();
}

FText ATallowCollectionSlot::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	return LOCTEXT("FindMaisie", "LOOK FOR MAISIE");
}

void ATallowCollectionSlot::OnInteractComplete_Implementation(AActor* Interactor)
{
	auto* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player || !CanInteract_Implementation(Interactor)) return;
	bFilled = true;

	if (auto* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTallowStorySubsystem>() : nullptr) Story->GiveCandleBack();
	if (auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass())))
		Director->OnCandleGivenBack(GetActorLocation());
}

void ATallowCollectionSlot::ResetSlot()
{
	bFilled = false;
	LeftCandle->SetHiddenInGame(true);
}

#undef LOCTEXT_NAMESPACE
