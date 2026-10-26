#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowPlayDirector.h"
#include "Player/TallowCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Tasks/TallowProjector.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TallowKeypadWidget.h"
#include "Noise/TallowNoiseSubsystem.h"

ATallowEscapePoint::ATallowEscapePoint()
{
	Latch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EscapeLatch"));
	RootComponent = Latch;
	Latch->SetCollisionProfileName(TEXT("BlockAll"));
}
void ATallowEscapePoint::BeginPlay()
{
	Super::BeginPlay();
	LinkedProjector = Cast<ATallowProjector>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowProjector::StaticClass()));
}

bool ATallowEscapePoint::CanInteract_Implementation(AActor* Interactor) const
{
	const auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()));
	return Cast<ATallowCharacter>(Interactor) && Director && Director->CanEscape(); 
}

float ATallowEscapePoint::GetInteractionDuration_Implementation() const
{
	return bCodeVerified ? 1.4f : 0.0f;
}

FText ATallowEscapePoint::GetInteractionPromptText_Implementation(AActor* Interactor) const
{
	if (!CanInteract_Implementation(Interactor))
	{
		return FText::FromString(TEXT("THE RELEASE NEEDS POWER"));
	}
	if (CanInteract_Implementation(Interactor) && !bCodeVerified)
	{
		return FText::FromString(TEXT("ENTER ACCESS CODE"));
	}
	return FText::FromString(TEXT("RELEASE THE LATCH"));
}

void ATallowEscapePoint::OnInteract_Implementation(AActor* Interactor)
{
	if (!Interactor) return;
	ATallowCharacter* Player = Cast<ATallowCharacter>(Interactor);
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	CachedPC = PC;
	
	if (!bCodeVerified && PC && KeypadWidgetClass)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		ActiveWidgetInstance = CreateWidget<UTallowKeypadWidget>(PC, KeypadWidgetClass);
		ActiveWidgetInstance->AddToViewport();
		ActiveWidgetInstance->InitializeKeypad(this);
		ActiveWidgetInstance->SetKeyboardFocus();
		bIsCurrentlyInputtingKeypad = true;
	}
}

void ATallowEscapePoint::OnInteractComplete_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor)) return;
	auto* Director = Cast<ATallowPlayDirector>(UGameplayStatics::GetActorOfClass(this, ATallowPlayDirector::StaticClass()));
	if (Director && bCodeVerified)
	{
		Director->FinishRun();	
	}
}

bool ATallowEscapePoint::CheckSubmittedCode(const TArray<int32>& EnteredCode) 
{
	ATallowCharacter* Player = Cast<ATallowCharacter>(GetOwner());
	// if code match, remove keypad and enable input
	if (LinkedProjector && EnteredCode == LinkedProjector->GetCodes())
	{
		bCodeVerified = true;
		if (CachedPC.IsValid())
		{
			CachedPC->SetIgnoreLookInput(false);
			CachedPC->SetIgnoreMoveInput(false);
		}
		if (ActiveWidgetInstance)
		{
			ActiveWidgetInstance->RemoveFromParent();
			ActiveWidgetInstance = nullptr;
			bIsCurrentlyInputtingKeypad = false;
		}
		OnCorrectAccessCode.Broadcast();
		return true;
	}
	
	// else
	UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>();
	if (NoiseSubsystem)
	{
		NoiseSubsystem->EmitCustomNoise(GetActorLocation(), CachedPC.IsValid() ? CachedPC->GetPawn() : nullptr, InputFailedLoudness, InputFailedRange, true);
	}
	OnWrongAccessCode.Broadcast();
	return false;
}
