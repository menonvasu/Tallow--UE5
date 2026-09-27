#include "TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Tasks/TallowChairEscape.h"
#include "Player/TallowCharacter.h"
#include "AI/TallowCreature.h"
#include "AI/TallowAIController.h"
#include "Environment/TallowCandlePickup.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"

ATallowGameMode::ATallowGameMode()
{
}

void ATallowGameMode::BeginPlay()
{
	Super::BeginPlay();

	Player = Cast<ATallowCharacter>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCharacter::StaticClass()));
	Chair = Cast<ATallowChairEscape>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowChairEscape::StaticClass()));
	Creature = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCreature::StaticClass()));
	CandlePickup = Cast<ATallowCandlePickup>(UGameplayStatics::GetActorOfClass(GetWorld(), ATallowCandlePickup::StaticClass()));
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		CameraManager = PC->PlayerCameraManager;
	}
}

void ATallowGameMode::HandlePlayerCaptured()
{
	if (CameraManager)
	{
		CameraManager->StartCameraFade(0.0f, 1.0f, FadeOutTimer, FColor::Black, true, true);
	}

	if (Player && Player->GetCharacterMovement())
	{
		Player->GetCharacterMovement()->DisableMovement();
	}

	OnCaught.Broadcast();
	
	if (Creature && Creature->GetCharacterMovement())
	{
		Creature->GetCharacterMovement()->DisableMovement();
	}

	GetWorld()->GetTimerManager().SetTimer(
		ResetTimerHandle,
		this,
		&ATallowGameMode::ResetGame,
		FadeOutTimer + BlackoutTimer,
		false
	);
}

void ATallowGameMode::ResetGame()
{
	if (Creature)
	{
		Creature->ResetToSpawn();
	}	
	if (Chair)
	{
		Chair->TiePlayerToChair(Player);
	}
	if (CandlePickup)
	{
		CandlePickup->ResetPickup(Player);
	}
	
	if (CameraManager)
	{
		CameraManager->StartCameraFade(1.0f, 0.0f, FadeInTimer, FColor::Black, false, false);
	}
}
