#include "Candle/TallowCandleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Noise/TallowNoiseSubsystem.h" 
#include "Noise/TallowNoiseTypes.h"


UTallowCandleComponent::UTallowCandleComponent()
{
    // performance 
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .1f;
}

ETallowCandleFuelStage UTallowCandleComponent::GetFuelStage() const
{
    if (CurrentState==ECandleState::Out || WaxFraction<=0.f) return ETallowCandleFuelStage::Out;
    if (IsGuttering()) return ETallowCandleFuelStage::Danger;
    if (WaxFraction>.65f) return ETallowCandleFuelStage::Full;
    if (WaxFraction>.35f) return ETallowCandleFuelStage::Medium;
    if (WaxFraction>.15f) return ETallowCandleFuelStage::Low;
    return ETallowCandleFuelStage::Danger;
}

void UTallowCandleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    if (!bIsAcquired || bGivenUp || CurrentState==ECandleState::Out || !bBurnsDown) return;
    const float Rate=CurrentState==ECandleState::Cupped ? FMath::Clamp(CuppedBurnMultiplier,.1f,1.f) : 1.f;
    WaxFraction=FMath::Max(0.f,WaxFraction-FMath::Max(DeltaTime,0.f)*Rate/FMath::Max(BurnDurationSeconds,1.f));
    if (WaxFraction<=0.f)
    {
        GetWorld()->GetTimerManager().ClearTimer(RelightTimerHandle);
        SetCandleState(ECandleState::Out);
    }
    OnFuelChanged.Broadcast(WaxFraction,GetFuelStage());
}

void UTallowCandleComponent::AddWax(float Fraction)
{
    if (!bIsAcquired || bGivenUp || !FMath::IsFinite(Fraction) || Fraction<=0.f) return;
    WaxFraction=FMath::Clamp(WaxFraction+Fraction,0.f,1.f);
    OnFuelChanged.Broadcast(WaxFraction,GetFuelStage());
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
	if (bGivenUp && NewState != ECandleState::Out) return;     // left in the collection: it stays out
	if (WaxFraction<=0.f && NewState!=ECandleState::Out) return;

	const ECandleState OldState = CurrentState;
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
    OnFuelChanged.Broadcast(WaxFraction,GetFuelStage());

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
    // The order of match, flame and light for the log (30 Oct: the light used to come on before any flame).
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CANDLE state %d->%d light=%d intensity=%.2f f=%llu t=%.3f"), int32(OldState), int32(CurrentState),
        CachedLightComponent && CachedLightComponent->IsVisible() ? 1 : 0, CachedLightComponent ? CachedLightComponent->Intensity : 0.f,
        (unsigned long long)GFrameCounter, GetWorld()->GetTimeSeconds());
}

void UTallowCandleComponent::StartRelighting()
{
    // if not acquired, or left in the collection, return
    if (!bIsAcquired || bGivenUp || WaxFraction<=0.f) return;
    // if lit, return
    if (CurrentState == ECandleState::Lit) return;
    // if candle already out or no matches left or still currently relighting, end early
    if (CurrentState != ECandleState::Out) return;
    if (!bUnlimitedMatches && MatchCount <= 0) { OnMatchesChanged.Broadcast(0, true); return; }
    if (GetWorld()->GetTimerManager().IsTimerActive(RelightTimerHandle)) return;

    // Striking a match immediately breaks full sprint:
    if (CachedMovementComponent)
    {
        CachedMovementComponent->MaxWalkSpeed = DefaultWalkSpeed;
    }

    // start ligthing anim
    OnRelightStarted.Broadcast();

    // the match head flares as it catches (GetMatchFlare)
    MatchStruckAt = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Display, TEXT("TALLOW_CANDLE strike light=%d f=%llu t=%.3f"), CachedLightComponent && CachedLightComponent->IsVisible() ? 1 : 0,
        (unsigned long long)GFrameCounter, GetWorld()->GetTimeSeconds());

    // report candle lit noise
    if (UTallowNoiseSubsystem* NoiseSubsystem = GetWorld()->GetSubsystem<UTallowNoiseSubsystem>())
    {
        NoiseSubsystem->EmitNoise(ETallowNoiseType::MatchStrike, GetOwner()->GetActorLocation(), GetOwner(), RelightNoiseLoudness, true);
    }

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
    // A reset can invalidate this attempt before its timer callback runs.
    if (!bIsAcquired || bGivenUp || WaxFraction<=0.f || CurrentState!=ECandleState::Out) return;

    // remove a match and set candle to lit
	if (!bUnlimitedMatches) { MatchCount = FMath::Max(0, MatchCount - 1); OnMatchesChanged.Broadcast(MatchCount, false); }
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

bool UTallowCandleComponent::GustOut()
{
    // only an open, steady flame: cupping protects it, and a match mid-strike isn't a flame yet
    if (!bIsAcquired || bGivenUp || CurrentState != ECandleState::Lit || IsRelighting()) return false;
    SetCandleState(ECandleState::Out);
    OnCandleGustedOut.Broadcast();
    return true;
}

float UTallowCandleComponent::GetMatchFlare() const
{
    if (!GetWorld()) return 0.f;
    const float Age = GetWorld()->GetTimeSeconds() - MatchStruckAt;
    if (Age < 0.f || Age >= MatchFlareSeconds) return 0.f;
    // a bright burst as the head catches, settling to the match's own burn, then gone
    return FMath::Lerp(.45f, 1.f, FMath::Exp(-Age / .25f)) * FMath::Clamp((MatchFlareSeconds - Age) / .3f, 0.f, 1.f);
}

void UTallowCandleComponent::GiveUp()
{
    GetWorld()->GetTimerManager().ClearTimer(RelightTimerHandle);
    SetCandleState(ECandleState::Out);
    bGivenUp = true;
}

void UTallowCandleComponent::ResetCandle()
{
    GetWorld()->GetTimerManager().ClearTimer(RelightTimerHandle);
    MatchStruckAt = -100.0;
    bIsAcquired = false;
    bGivenUp = false;
    WaxFraction = 1.f;
    SetCandleState(ECandleState::Out);
    ResetMatchCount();
    OnCandleReset.Broadcast();
    OnFuelChanged.Broadcast(WaxFraction,GetFuelStage());
}

void UTallowCandleComponent::RestoreForChapter(bool bAcquired, float Wax, int32 Matches, ECandleState State)
{
    if (GetWorld())
    {
        auto& Timers = GetWorld()->GetTimerManager();
        Timers.ClearTimer(RelightTimerHandle); Timers.ClearTimer(SprintExtinguishTimerHandle); Timers.ClearTimer(DraftExtinguishTimerHandle);
    }
    bIsAcquired = bAcquired; bGivenUp = false; MatchStruckAt = -100.0;
    // A save from before 31 Oct can carry a part-burned candle; with burning off it comes back whole.
    WaxFraction = bBurnsDown ? FMath::Clamp(Wax, 0.f, 1.f) : 1.f; MatchCount = FMath::Max(0, Matches);
    ActiveDraftCount = 0;
    CurrentState = bAcquired && WaxFraction > 0.f ? State : ECandleState::Out;
    // The light goes with the restored state; a light left on from before the restore would shine with no flame.
    if (CachedLightComponent) CachedLightComponent->SetVisibility(CurrentState != ECandleState::Out);
    if (CachedMovementComponent) CachedMovementComponent->MaxWalkSpeed = DefaultWalkSpeed;
    OnCandleGutteringChanged.Broadcast(false);
    OnMatchesChanged.Broadcast(MatchCount, false);
    OnFuelChanged.Broadcast(WaxFraction, GetFuelStage());
    OnCandleStateChanged.Broadcast(CurrentState);
}
