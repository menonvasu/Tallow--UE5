#include "AI/TallowCreature.h"
#include "Animation/TallowCreatureAnimationComponent.h"
#include "AI/Traversal/TallowSurfaceTraversalComponent.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "TallowAIController.h"

void ATallowCreature::PrepareCapturePresentation() {}

float ATallowCreature::GetCapturePresentationDuration() const
{
	return KeeperAnimation ? KeeperAnimation->GetCaptureDuration() : 0.f;
}

float ATallowCreature::GetCaptureStrikeTime() const
{
	return KeeperAnimation ? KeeperAnimation->GetCaptureStrikeTime() : 0.f;
}

float ATallowCreature::GetCaptureStagingDistance() const
{
	return 125.f * (KeeperAnimation ? FMath::Clamp(KeeperAnimation->KeeperMeshScale, .5f, 2.f) : 1.f);
}

ATallowCreature::ATallowCreature(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	KeeperAnimation = CreateDefaultSubobject<UTallowCreatureAnimationComponent>(TEXT("KeeperAnimation"));
	SurfaceTraversal = CreateDefaultSubobject<UTallowSurfaceTraversalComponent>(TEXT("SurfaceTraversal"));
	Voice = CreateDefaultSubobject<UTallowKeeperVoiceComponent>(TEXT("Voice"));

	// Sets tick to false
	PrimaryActorTick.bCanEverTick = false;

	// Set size for collision capsule (Default)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Turn the body toward where the controller is looking at a limited rate, rather than snapping to it
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 140.f, 0.f);

	// Set AI controller class
	AIControllerClass = ATallowAIController::StaticClass();

	// Set possession
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// Setup grab sphere
	GrabSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Grab Sphere"));
	GrabSphere->SetupAttachment(GetCapsuleComponent());
	GrabSphere->SetSphereRadius(85.0f);
	GrabSphere->SetCollisionProfileName(TEXT("Trigger"));
}

void ATallowCreature::BeginPlay()
{
	Super::BeginPlay();

	if (AutoPossessAI != EAutoPossessAI::Disabled && Controller == nullptr)
	{
		SpawnDefaultController();
	}

	InitialSpawnTransform = GetActorTransform();
	
	if (GrabSphere)
	{
		GrabSphere->OnComponentBeginOverlap.AddDynamic(this, &ATallowCreature::OnOverlapBegin);
	}
}

void ATallowCreature::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATallowAIController* AICon = GetController<ATallowAIController>())
	{
		AICon->NotifyPlayerTouched(OtherActor);
	}
}

void ATallowCreature::ResetToSpawn()
{
	if (SurfaceTraversal) SurfaceTraversal->ResetTraversal();
	if (KeeperAnimation) KeeperAnimation->ResetPlayback();
	SetActorTransform(InitialSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if (ATallowAIController* AICon = GetController<ATallowAIController>())
	{
		AICon->ResetBrain();
	}
}

void ATallowCreature::SetPhase(ETallowCreaturePhase NewPhase)
{
	Phase = NewPhase;

	// Swap which routes the controller is allowed to path through
	if (ATallowAIController* AIController = Cast<ATallowAIController>(GetController()))
	{
		AIController->ApplyPhaseNavigationFilter(Phase);
	}
}

void ATallowCreature::SetCreatureVisualState(ETallowCreatureState NewVisualState)
{
	if (CurrentState == NewVisualState) return;
	CurrentState = NewVisualState;
	OnCreatureStateChanged.Broadcast(NewVisualState);
}

void ATallowCreature::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	/** if (GetMesh())
	{
		const FName HeadSocketName = TEXT("Head");
		if (GetMesh()->DoesSocketExist(HeadSocketName))
		{
			OutLocation = GetMesh()->GetSocketLocation(HeadSocketName);
			OutRotation = GetMesh()->GetSocketRotation(HeadSocketName);
			return;
		}
	} */

	Super::GetActorEyesViewPoint(OutLocation, OutRotation);
}
