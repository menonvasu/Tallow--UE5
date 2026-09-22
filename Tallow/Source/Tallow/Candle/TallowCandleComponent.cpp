#include "Candle/TallowCandleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"

UTallowCandleComponent::UTallowCandleComponent()
{
    // performance 
    PrimaryComponentTick.bCanEverTick = false;
}

void UTallowCandleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Retrieve character components, walk speed and light component
	CachedMovementComponent = GetOwner()->FindComponentByClass<UCharacterMovementComponent>();
	CachedLightComponent = GetOwner()->FindComponentByClass<UPointLightComponent>();

	if (CachedMovementComponent)
	{
		DefaultWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
	}
	if (CachedLightComponent && !bIsAcquired)
	{
        CachedLightComponent->SetVisibility(false);
	}
}

void UTallowCandleComponent::AcquireCandle(bool bStartLit)
{
    bIsAcquired = true;
    OnAcquireCandle.Broadcast();

    if (bStartLit)
    {
        SetCandleState(ECandleState::Lit);   
    }
}

void UTallowCandleComponent::SetCandleState(ECandleState NewState)
{
    // exit if state already newstate, otherwise set state and broadcast anim change
	if (CurrentState == NewState) return;

	CurrentState = NewState;

    // If the flame goes out or is cupped, cancel any active struggle:
    if (CurrentState == ECandleState::Out || CurrentState == ECandleState::Cupped)
    {
        OnCandleGutteringChanged.Broadcast(false);

        if (GetWorld()->GetTimerManager().IsTimerActive(SprintExtinguishTimerHandle))
        {
            GetWorld()->GetTimerManager().ClearTimer(SprintExtinguishTimerHandle);
        }

        if (GetWorld()->GetTimerManager().IsTimerActive(DraftExtinguishTimerHandle))
        {
            GetWorld()->GetTimerManager().ClearTimer(DraftExtinguishTimerHandle);
        }
    }

	OnCandleStateChanged.Broadcast(CurrentState);

    switch (CurrentState)
    {
    case ECandleState::Out:
        // not visible if out
        if (CachedLightComponent)
        {
            CachedLightComponent->SetVisibility(false);
        }
        break;

    case ECandleState::Lit:
        if (IsInDraft())
        {
            // start extinguish timer
            if (!GetWorld()->GetTimerManager().IsTimerActive(DraftExtinguishTimerHandle))
            {
                OnCandleGutteringChanged.Broadcast(true);
                GetWorld()->GetTimerManager().SetTimer(
                    DraftExtinguishTimerHandle,
                    this,
                    &UTallowCandleComponent::HandleDraftExtinguishTimerExpired,
                    DraftExtinguishGracePeriod,
                    false
                );
            }
        }
        // normal lit radius and visible with normal walk speed
        if (CachedLightComponent)
        {
            CachedLightComponent->SetAttenuationRadius(LitLightRadius);
            CachedLightComponent->SetVisibility(true);
        }
        if (CachedMovementComponent)
        {
            CachedMovementComponent->MaxWalkSpeed = DefaultWalkSpeed;
        }
        break;

    case ECandleState::Cupped:
        // smaller light radius with slower walk speed
        if (CachedLightComponent)
        {
            CachedLightComponent->SetAttenuationRadius(CuppedLightRadius);
            CachedLightComponent->SetVisibility(true);
        }
        if (CachedMovementComponent)
        {
            CachedMovementComponent->MaxWalkSpeed = DefaultWalkSpeed * CuppedSpeedMultiplier;
        }
        break;
    }
}

void UTallowCandleComponent::StartRelighting()
{
    // if not acquired, return
    if (!bIsAcquired) return;
    // if lit, return
    if (CurrentState == ECandleState::Lit) return;
    // if candle already out or no matches left or still currently relighting, end early
	if (CurrentState != ECandleState::Out  || MatchCount <= 0) return;
	if (GetWorld()->GetTimerManager().IsTimerActive(RelightTimerHandle)) return;

    // Striking a match immediately breaks full sprint:
    if (CachedMovementComponent)
    {
        CachedMovementComponent->MaxWalkSpeed = DefaultWalkSpeed;
    }

    // start ligthing anim
	OnRelightStarted.Broadcast();

    // report candle lit noise
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetOwner()->GetActorLocation(), RelightNoiseLoudness, GetOwner(), 0.0f, FName("Relight"));

    // start timer to light up the candle
	GetWorld()->GetTimerManager().SetTimer(
		RelightTimerHandle, 
		this,
		&UTallowCandleComponent::FinishRelighting,
		RelightDuration,
		false
	);
}

void UTallowCandleComponent::FinishRelighting()
{
    // remove a match and set candle to lit
	MatchCount -= 1;
	SetCandleState(ECandleState::Lit);
}

void UTallowCandleComponent::HandleSprintStarted()
{
    // dont run function if candle not lit
    if (CurrentState != ECandleState::Lit) return;
    
    // start guttering 
    OnCandleGutteringChanged.Broadcast(true);

    // set extinguish timer 
    GetWorld()->GetTimerManager().SetTimer(
        SprintExtinguishTimerHandle,
        this,
        &UTallowCandleComponent::HandleSprintExtinguishTimerExpired,
        SprintExtinguishGracePeriod,
        false
    );
}

void UTallowCandleComponent::HandleSprintEnded()
{
    // flame stabilized
    OnCandleGutteringChanged.Broadcast(false);

    // reset timer on stopped sprint
    GetWorld()->GetTimerManager().ClearTimer(SprintExtinguishTimerHandle);

	// Restore walk speed for the current candle state (cupped stays slow)
	if (CachedMovementComponent)
	{
		CachedMovementComponent->MaxWalkSpeed =
			(CurrentState == ECandleState::Cupped)
				? DefaultWalkSpeed * CuppedSpeedMultiplier
				: DefaultWalkSpeed;
	}
}

void UTallowCandleComponent::HandleSprintExtinguishTimerExpired()
{
    // set candle state to out if sprinting
    if (CachedMovementComponent && CachedMovementComponent->Velocity.Size2D() > (DefaultWalkSpeed * 1.15f))
    {
        SetCandleState(ECandleState::Out);
    }
}

void UTallowCandleComponent::EnterDraft()
{
    // add to active drafts
    ActiveDraftCount++;
    if (CurrentState == ECandleState::Lit) 
    {
        if (GetWorld()->GetTimerManager().IsTimerActive(DraftExtinguishTimerHandle)) return;
     
        // start guttering 
        OnCandleGutteringChanged.Broadcast(true);

        GetWorld()->GetTimerManager().SetTimer(
            DraftExtinguishTimerHandle,
            this,
            &UTallowCandleComponent::HandleDraftExtinguishTimerExpired,
            DraftExtinguishGracePeriod,
            false
        );
    }
}

void UTallowCandleComponent::ExitDraft()
{
    // drop from active drafts
    ActiveDraftCount = FMath::Max(0, ActiveDraftCount - 1);

    // flame stabilized
    if (ActiveDraftCount == 0)
    {
        GetWorld()->GetTimerManager().ClearTimer(DraftExtinguishTimerHandle);
        OnCandleGutteringChanged.Broadcast(false);
    }
}

void UTallowCandleComponent::HandleDraftExtinguishTimerExpired()
{   
    // set out on expired
    SetCandleState(ECandleState::Out);
}