#include "TallowFuseBox.h"
#include "Components/StaticMeshComponent.h"
#include "Player/TallowCharacter.h"
#include "Noise/TallowNoiseSubsystem.h"

// Sets default values
ATallowFuseBox::ATallowFuseBox()
{
	PrimaryActorTick.bCanEverTick = false;
	
	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;
	BoxMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	// Create fuse meshes for box insertion
	FuseMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh1"));
	FuseMesh1->SetupAttachment(BoxMesh);
	FuseMesh1->SetVisibility(false);
	FuseMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	FuseMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh2"));
	FuseMesh2->SetupAttachment(BoxMesh);
	FuseMesh2->SetVisibility(false);
	FuseMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	FuseMesh3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh3"));
	FuseMesh3->SetupAttachment(BoxMesh);
	FuseMesh3->SetVisibility(false);
	FuseMesh3->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void ATallowFuseBox::BeginPlay()
{
	Super::BeginPlay();
	
}

float ATallowFuseBox::GetInteractionDuration_Implementation() const
{
	// if already fully powered, ignore
	if (CurrentFuses >= RequiredFuses) { return 0.0f; }
	return InsertionHoldDuration;
}

void ATallowFuseBox::OnInteractStart_Implementation(AActor* Actor)
{
	OnFuseCurrentlyBeingInserted.Broadcast();
}

void ATallowFuseBox::OnInteractComplete_Implementation(AActor* Interactor)
{
	if (!Interactor) return;
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player || CurrentFuses >= RequiredFuses) return;
	
	// if holding fuse - consume
	if (Player->ConsumeFuse())
	{
		CurrentFuses++;
		
		// broadcast visual & audio
		OnFuseInserted.Broadcast();
		if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
		{
			NoiseSubsystem->EmitNoise(ETallowNoiseType::FuseInsert, GetActorLocation(), Player, FuseInsertLoudness, true);
		}
		
		switch (CurrentFuses)
		{
		case 1:
			FuseMesh1->SetVisibility(true);
			break;
		case 2:
			FuseMesh2->SetVisibility(true);
			break;
		case 3:
			FuseMesh3->SetVisibility(true);
			break;
		default:
			break;
		}
		
		// Broadcast final event 
		if (CurrentFuses >= RequiredFuses)
		{
			OnPowerRestored.Broadcast();
		}
	}
}

void ATallowFuseBox::OnInteractCancel_Implementation(AActor* Interactor)
{
	// Play buzz when player tries to insert fuse without holding one
	OnNothingInserted.Broadcast();	
}

bool ATallowFuseBox::CanInteract_Implementation(AActor* Interactor) const
{
	if (!Interactor) return true;
	const ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player || CurrentFuses >= RequiredFuses) return true;
	
	// if already powered, cannot interact
	if (CurrentFuses >= RequiredFuses)
	{
		return false;
	}
	// if missing fuse
	if (!Player->HasCarriedItem(ECarriedItemType::Fuse))
	{
		return false;
	}
	// else, can interact
	return true;
}

FText ATallowFuseBox::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!Interactor) return FText::GetEmpty();
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return FText::GetEmpty();
	
	// if already powered
	if (CurrentFuses >= RequiredFuses)
	{
		return FText::FromString(TEXT("POWER RESTORED"));
	}
	// if missing fuse
	if (!Player->HasCarriedItem(ECarriedItemType::Fuse))
	{
		return FText::FromString(TEXT("REQUIRES FUSE"));
	}
	// else
	return FText::FromString(TEXT("INSERT FUSE"));
}

void ATallowFuseBox::ResetVisual()
{
	CurrentFuses = 0;
	FuseMesh1->SetVisibility(false);
	FuseMesh2->SetVisibility(false);
	FuseMesh3->SetVisibility(false);
}
