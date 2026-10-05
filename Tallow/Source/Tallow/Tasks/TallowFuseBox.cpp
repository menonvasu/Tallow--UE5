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

void ATallowFuseBox::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) return;
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	if (!Player) return;
	
	if (CurrentFuses >= RequiredFuses) { return; }
	
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
	else
	{
		// Play buzz when player tries to insert fuse without holding one
		OnNothingInserted.Broadcast();	
	}
}


