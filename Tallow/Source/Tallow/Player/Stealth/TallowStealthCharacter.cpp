#include "Player/Stealth/TallowStealthCharacter.h"
#include "Player/Stealth/TallowStealthAnimInstance.h"
#include "Player/Stealth/TallowStealthFeedbackWidget.h"
#include "Player/Presentation/TallowPlayerPresentationComponent.h"
#include "AI/TallowCreature.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sound/SoundBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/TallowGameMode.h"
#include "Interactable/TallowInteractionComponent.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"
#include "UI/Presentation/TallowLessonWidget.h"
#include "UObject/ConstructorHelpers.h"

ATallowStealthCharacter::ATallowStealthCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    GetCharacterMovement()->MaxWalkSpeedCrouched = 105.f;
    GetCharacterMovement()->MaxAcceleration = 1100.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1200.f;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Cup(
        TEXT("/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_CupShield_03"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Reach(
        TEXT("/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_Reach_01"));
    if (Cup.Succeeded()) BraceClip = Cup.Object;
    if (Reach.Succeeded()) ReachClip = Reach.Object;
}

void ATallowStealthCharacter::BeginPlay()
{
    Super::BeginPlay();
    DefaultRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    DefaultHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    BaseBoomLocation = CameraBoom->GetRelativeLocation();
    BaseCameraRotation = FirstPersonCameraComponent->GetRelativeRotation();
    // Replace only our inherited binding. Other candle and story listeners remain.
    if (CandleComponent)
    {
        CandleComponent->OnCandleStateChanged.RemoveDynamic(this, &ATallowCharacter::SetLightPerception);
        CandleComponent->OnCandleStateChanged.AddUniqueDynamic(this, &ATallowStealthCharacter::RefreshCandleSight);
        CandleComponent->OnCandleReset.AddUniqueDynamic(this, &ATallowStealthCharacter::HandleCandleReset);
        RefreshCandleSight(CandleComponent->GetCandleState());
    }
    if (auto* GameMode = GetWorld()->GetAuthGameMode<ATallowGameMode>())
        GameMode->OnCaught.AddUniqueDynamic(this, &ATallowStealthCharacter::ResetStealthState);
}

void ATallowStealthCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    bKeepStoryTransformationEvent = false;
    ReleaseBreath(false, false);
    if (Feedback) Feedback->RemoveFromParent();
    if (Presentation.IsValid() && Presentation->IsPresentationEnabled())
        Presentation->UpdateHandsTransform(BaseHandsTransform);
    Super::EndPlay(Reason);
}

void ATallowStealthCharacter::RefreshCandleSight(ECandleState State)
{
    bCandleSightRegistered = State != ECandleState::Out;
    if (UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(GetWorld()))
    {
        if (bCandleSightRegistered)
            Perception->RegisterSourceForSenseClass(UAISense_Sight::StaticClass(), *this);
        else
            Perception->UnregisterSource(*this, UAISense_Sight::StaticClass());
    }
}

UTallowNoiseSubsystem* ATallowStealthCharacter::GetNoiseSystem() const
{
    return UTallowNoiseSubsystem::Get(this);
}

bool ATallowStealthCharacter::BeginBreathHold()
{
    const auto* PC = Cast<APlayerController>(GetController());
    if (IsTiedToChair() || bIsHoldingStill || BreathFraction < .24f || RecoveryDelay > .7f
        || TraversalMode != ETallowPlayerTraversalMode::None
        || (PC && PC->IsMoveInputIgnored())
        || (!IsHiddenInSpot() && !GetCharacterMovement()->IsMovingOnGround())
        || (InteractionComponent && InteractionComponent->IsInteracting())) return false;
    StillnessStartTime = GetWorld()->GetTimeSeconds();
    bIsHoldingStill = true;
    HeldViewRotation = GetControlRotation().GetNormalized();
    bOriginalControllerYaw = bUseControllerRotationYaw;
    bSprintInput = false;
    // Holding your breath silences you; it doesn't freeze you or touch the candle (25 Oct). You
    // can still creep, slowly, and only the light gives you away.
    GetCharacterMovement()->MaxWalkSpeed = FMath::Min(GetCharacterMovement()->MaxWalkSpeed, 110.f);
    if (CandleComponent) CandleComponent->HandleSprintEnded();
    ShieldReleaseDelay = 0.f;
    FVector WallLeft, WallRight;
    FHitResult Wall;
    const FVector Eye = FirstPersonCameraComponent->GetComponentLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerBrace), false, this);
    bBracedAgainstSurface = GetWorld()->LineTraceSingleByChannel(Wall, Eye,
        Eye + GetActorForwardVector() * 75.f, ECC_Visibility, Query)
        && FMath::Abs(Wall.ImpactNormal.Z) < .5f;
    if (!bBracedAgainstSurface)
        bBracedAgainstSurface = FindSqueezeWalls(GetActorLocation(), WallLeft, WallRight);
    if (!bSeenBraceIntroduction)
    {
        IntroductionTime = 4.f;
        bSeenBraceIntroduction = true;
    }
    OnStill.Broadcast(true);
    return true;
}

void ATallowStealthCharacter::EndBreathHold() { ReleaseBreath(false); }
void ATallowStealthCharacter::DoStillnessStart() { BeginBreathHold(); }
void ATallowStealthCharacter::DoStillnessEnd() { EndBreathHold(); }

void ATallowStealthCharacter::ReleaseBreath(bool bExhausted, bool bRestoreCandle)
{
    if (!bIsHoldingStill) return;
    const double HeldSeconds = GetWorld()->GetTimeSeconds() - StillnessStartTime;
    bIsHoldingStill = false;
    // A hiding volume owns its yaw and movement configuration.
    if (!IsHiddenInSpot() && !IsTiedToChair()) bUseControllerRotationYaw = bOriginalControllerYaw;
    OnStill.Broadcast(false);
    if (bKeepStoryTransformationEvent && HeldSeconds >= MinStillnessHoldDuration)
        OnTransformation.Broadcast(++TransformationLevel);
    bBracedAgainstSurface = false;
    RecoveryDelay = bExhausted ? 1.2f : .55f;
    NextBreathNoise = 3.6f; // No immediate noise from a short or voluntary hold.
    if (bExhausted)
    {
        ++ExhaustionReleaseCount;
        ExhaustionCueTime = 1.6f;
        // A single nearby clue after a long visible warning, never on an ordinary release.
        if (auto* Noise = UTallowNoiseSubsystem::Get(this))
            Noise->EmitCustomNoise(FirstPersonCameraComponent->GetComponentLocation(), this, .16f, 220.f);
        bOwnsCup = false; // Keep the flame shielded instead of flaring it on an involuntary breath.
    }
    else if (bRestoreCandle && bOwnsCup && !bManualCup)
        ShieldReleaseDelay = .65f;
    else if (!bRestoreCandle)
        bOwnsCup = false;
    if (CandleComponent) RefreshCandleSight(CandleComponent->GetCandleState());
    RestoreMovementSpeed();
}

void ATallowStealthCharacter::HandleCandleReset() { ResetStealthState(); }

void ATallowStealthCharacter::ResetStealthState()
{
    // A reset is not a deliberate use of the story mechanic.
    const bool bStory = bKeepStoryTransformationEvent;
    bKeepStoryTransformationEvent = false;
    ReleaseBreath(false, false);
    bKeepStoryTransformationEvent = bStory;
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling) FinishMantle(false);
    if (TraversalMode == ETallowPlayerTraversalMode::Squeezing
        && CapsuleFits(GetActorLocation(), DefaultRadius, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
        GetCapsuleComponent()->SetCapsuleRadius(DefaultRadius, true);
        TraversalMode = ETallowPlayerTraversalMode::None;
    }
    BreathFraction = 1.f; BreathStrain = 0.f; BracePoseAlpha = 0.f;
    RecoveryDelay = 0.f; ShieldReleaseDelay = 0.f; ExhaustionCueTime = 0.f;
    NextBreathNoise = 3.6f;
    bOwnsCup = false; bManualCup = false; bSprintInput = false; bStandBlocked = false;
    bCrouchInput = false;
    if (IsTiedToChair())
    {
        // The chair has already placed and locked the pawn at its reset pose.
        GetCapsuleComponent()->SetCapsuleSize(DefaultRadius, DefaultHalfHeight, true);
        SetIsCrouched(false);
        GetCharacterMovement()->bWantsToCrouch = false;
        TraversalMode = ETallowPlayerTraversalMode::None;
        CameraCrouchOffset = 0.f;
        MantleCameraOffset = 0.f;
        MantleCameraLean = 0.f;
    }
    if (CandleComponent) RefreshCandleSight(CandleComponent->GetCandleState());
    RestoreMovementSpeed();
}

void ATallowStealthCharacter::UpdateBreath(float DeltaSeconds)
{
    RecoveryDelay = FMath::Max(0.f, RecoveryDelay - DeltaSeconds);
    ExhaustionCueTime = FMath::Max(0.f, ExhaustionCueTime - DeltaSeconds);
    IntroductionTime = FMath::Max(0.f, IntroductionTime - DeltaSeconds);
    if (bIsHoldingStill)
    {
        const auto* PC = Cast<APlayerController>(Controller);
        if (IsTiedToChair() || (PC && PC->IsLookInputIgnored())
            || (!IsHiddenInSpot() && GetCharacterMovement()->MovementMode == MOVE_None))
            ReleaseBreath(false, false);
        else
        {
            BreathFraction = FMath::Max(0.f, BreathFraction - DeltaSeconds / FMath::Max(3.f, BreathCapacitySeconds));
            BreathStrain = FMath::SmoothStep(.58f, 1.f, 1.f - BreathFraction);
            if (BreathFraction <= 0.f) ReleaseBreath(true);
        }
    }
    else
    {
        const bool bRunning = GetVelocity().Size2D() > 300.f;
        if (RecoveryDelay <= 0.f)
            BreathFraction = bRunning ? FMath::Max(.38f, BreathFraction - DeltaSeconds * .04f)
                : FMath::Min(1.f, BreathFraction + DeltaSeconds * .18f);
        BreathStrain = FMath::FInterpTo(BreathStrain, 0.f, DeltaSeconds, 2.4f);
        const auto* PC = Cast<APlayerController>(Controller);
        if (!IsTiedToChair() && (GetCharacterMovement()->MovementMode != MOVE_None || IsHiddenInSpot())
            && !(PC && PC->IsLookInputIgnored()))
        {
            NextBreathNoise -= DeltaSeconds;
            if (NextBreathNoise <= 0.f)
            {
                const float Exertion = 1.f - BreathFraction;
                if (auto* Noise = UTallowNoiseSubsystem::Get(this))
                    Noise->EmitCustomNoise(FirstPersonCameraComponent->GetComponentLocation(), this,
                        .045f + .08f * Exertion, 130.f + 120.f * Exertion);
                NextBreathNoise = FMath::Lerp(3.6f, 2.f, Exertion);
            }
        }
    }
    if (ShieldReleaseDelay > 0.f)
    {
        ShieldReleaseDelay = FMath::Max(0.f, ShieldReleaseDelay - DeltaSeconds);
        if (ShieldReleaseDelay <= 0.f)
        {
            if (bOwnsCup && !bManualCup && CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped)
                CandleComponent->SetCandleState(ECandleState::Lit);
            bOwnsCup = false;
        }
    }
}

void ATallowStealthCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBreath(DeltaSeconds);
    UpdateTraversal(DeltaSeconds);
    UpdatePresentation(DeltaSeconds);
    if (IsLocallyControlled()) UpdateAwarenessFeedback(DeltaSeconds);
    if (IsLocallyControlled() && bShowStealthFeedback && !Feedback)
    {
        if (auto* PC = Cast<APlayerController>(Controller))
        {
            Feedback = CreateWidget<UTallowStealthFeedbackWidget>(PC);
            if (Feedback) { Feedback->SetCharacter(this); Feedback->AddToViewport(32); }
        }
    }
    // The inherited director's old lesson describes magical invisibility. Retire that
    // lesson only for this opt-in pawn; our contextual prompt explains the physical action.
    LessonCheckTime -= DeltaSeconds;
    if (LessonCheckTime <= 0.f && IsLocallyControlled())
    {
        LessonCheckTime = .25f;
        TArray<UUserWidget*> Lessons;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Lessons, UTallowLessonWidget::StaticClass(), false);
        for (auto* Widget : Lessons)
            if (auto* Lesson = Cast<UTallowLessonWidget>(Widget))
                Lesson->CompleteLessonIfActive(ETallowLesson::Stillness);
    }
}

void ATallowStealthCharacter::UpdateAwarenessFeedback(float DeltaSeconds)
{
    // Being seen is felt, not written on screen: as the Keeper becomes sure of you the clay world boils
    // harder, drains of colour and pulses (MPC_TallowLook, read by M_PP_ClayLook), and a heartbeat swells.
    // Holding your breath narrows the view a touch and the heartbeat rises as the air runs out.
    const float Target = FMath::Clamp(HunterAwareness, 0.f, 1.f);
    // Once its live look breaks (cover, light out) the stare lets go quickly, so breaking sight is felt to work;
    // while it still has her in view the fall stays slow.
    WatchedLook = FMath::FInterpTo(WatchedLook, Target, DeltaSeconds, Target > WatchedLook ? 6.f : bSeenByKeeper ? 1.2f : 3.5f);
    BreathLook = FMath::FInterpTo(BreathLook, bIsHoldingStill ? .4f + .6f * BreathStrain : 0.f, DeltaSeconds, 4.f);
    if (!LookParams)
        LookParams = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Presentation/Look/MPC_TallowLook.MPC_TallowLook"));
    if (LookParams)
    {
        UKismetMaterialLibrary::SetScalarParameterValue(this, LookParams, TEXT("Watched"), WatchedLook);
        UKismetMaterialLibrary::SetScalarParameterValue(this, LookParams, TEXT("Breath"), BreathLook);
    }
    if (!Heartbeat)
        if (auto* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Player/S_Player_Heartbeat.S_Player_Heartbeat")))
            Heartbeat = UGameplayStatics::SpawnSoundAttached(Sound, GetRootComponent(), NAME_None, FVector::ZeroVector,
                EAttachLocation::KeepRelativeOffset, false, .001f, 1.f, 0.f, nullptr, nullptr, false);
    if (Heartbeat)
    {
        // The heartbeat is for hiding with it close (Dead by Daylight's terror radius, under a bed) and for the air
        // running out; being seen is now the Keeper's call and the candle's reaction, so it only colours that.
        float Near = 0.f;
        if (IsHiddenInSpot())
            if (const AActor* Keeper = UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()))
                Near = FMath::Clamp(1.f - FVector::Dist(Keeper->GetActorLocation(), GetActorLocation()) / 400.f, 0.f, 1.f);
        const float Level = FMath::Max3(Near, .55f * BreathStrain, .4f * WatchedLook);
        // The synth loop is -18 dB RMS; at .2 it peaks near -32, under the Keeper's tells, never over them.
        Heartbeat->SetVolumeMultiplier(FMath::Max(Level * .2f, .001f));
        Heartbeat->SetPitchMultiplier(.9f + .35f * WatchedLook);
        if (!Heartbeat->IsPlaying() && Level > .02f) Heartbeat->Play();
    }
}

void ATallowStealthCharacter::UpdatePresentation(float DeltaSeconds)
{
    if (!Presentation.IsValid())
    {
        auto* Found = FindComponentByClass<UTallowPlayerPresentationComponent>();
        if (Found && Found->IsPresentationEnabled())
        {
            Presentation = Found;
            BaseHandsTransform = Found->HandsRelativeTransform;
            if (auto* Hands = Found->GetPresentationHands())
                Hands->SetAnimInstanceClass(UTallowStealthAnimInstance::StaticClass());
        }
    }
    BracePoseAlpha = FMath::FInterpTo(BracePoseAlpha, bIsHoldingStill ? 1.f : 0.f, DeltaSeconds, bIsHoldingStill ? 5.5f : 3.5f);
    const float Time = GetWorld()->GetTimeSeconds();
    const float Tremor = FMath::Sin(Time * 17.f) * .35f + FMath::Sin(Time * 23.1f) * .16f;
    const float RecoveryBreath = !bIsHoldingStill && BreathFraction < .65f ? FMath::Sin(Time * 3.4f) * (1.f - BreathFraction) : 0.f;
    if (Presentation.IsValid() && Presentation->IsPresentationEnabled())
    {
        FTransform Hands = BaseHandsTransform;
        const float Squeeze = TraversalMode == ETallowPlayerTraversalMode::Squeezing ? 1.f : 0.f;
        // The candle is attached to the right hand, so this folds the whole protected
        // grip towards the chest without separating the prop from its fingers.
        Hands.AddToTranslation(FVector(-7.f * BracePoseAlpha - 4.f * Squeeze,
            -3.f * BracePoseAlpha + Tremor * BreathStrain * BracePoseAlpha,
            -4.5f * BracePoseAlpha + .4f * RecoveryBreath + Tremor * BreathStrain));
        Hands.SetRotation(FRotator(-3.f * BracePoseAlpha, 0.f, -4.f * Squeeze).Quaternion() * Hands.GetRotation());
        Presentation->UpdateHandsTransform(Hands);
    }
    // Blueprint camera/socket offsets can place the standing eye above the native
    // boom. Account for the complete eye transform instead of assuming boom Z is
    // eye height. Standing keeps its authored height; a crouched eye settles ten
    // centimetres below the crouched capsule top.
    const float EyeAboveBoom = FirstPersonCameraComponent->GetComponentLocation().Z
        - CameraBoom->GetComponentLocation().Z;
    const float BaseEyeAboveCapsule = GetActorTransform().TransformPosition(BaseBoomLocation).Z
        - GetActorLocation().Z + EyeAboveBoom;
    const float CrouchedEyeAboveCapsule = FMath::Max(8.f,
        GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 10.f);
    const float CrouchTarget = bIsCrouched
        ? FMath::Min(0.f, CrouchedEyeAboveCapsule - BaseEyeAboveCapsule) : 0.f;
    CameraCrouchOffset = FMath::FInterpTo(CameraCrouchOffset, CrouchTarget, DeltaSeconds, 9.f);
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling)
    {
        // Plant before lifting. Bending at the waist keeps the shoulder within
        // natural reach of the fixed ledge while the legs pull up beneath it.
        // Its height stays above the capsule's feet and still uses the normal
        // eye-clearance sweep; this never moves the collision body or locks look.
        const float Bend = FMath::SmoothStep(0.f, .18f, TraversalProgress)
            * (1.f - FMath::SmoothStep(.54f, .92f, TraversalProgress));
        const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float PlantEye = FMath::Max(Feet + 15.f, TraversalHandTarget.Z + 20.f);
        const float NormalEye = GetActorLocation().Z + BaseEyeAboveCapsule + CameraCrouchOffset;
        MantleCameraOffset = FMath::Min(0.f, PlantEye - NormalEye) * Bend;
        // Native camera/socket pitch changes how far the eye sits ahead of the
        // body. Reach towards the patch from that actual eye position rather
        // than only working at one scripted look angle.
        const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
        const FVector UnleanedEye = FirstPersonCameraComponent->GetComponentLocation() - Forward * MantleCameraLean;
        const float ReachLean = FMath::Clamp(FVector::DotProduct(
            TraversalHandTarget - UnleanedEye, Forward) - 10.f, 0.f, 28.f);
        MantleCameraLean = ReachLean * Bend;
    }
    else
    {
        MantleCameraOffset = FMath::FInterpTo(MantleCameraOffset, 0.f, DeltaSeconds, 8.f);
        MantleCameraLean = FMath::FInterpTo(MantleCameraLean, 0.f, DeltaSeconds, 8.f);
    }
    // Tied in the chair the neck pivot sits back against the chair's back, so looking down
    // sees the lap and the chair arms instead of putting the eye out over the knees.
    const float TiedEyeBack = IsTiedToChair() ? -26.f : 0.f;
    FVector Boom = BaseBoomLocation + FVector(MantleCameraLean + TiedEyeBack, 0.f,
        CameraCrouchOffset + MantleCameraOffset - 1.8f * BracePoseAlpha);
    CameraBoom->SetRelativeLocation(Boom);
    FirstPersonCameraComponent->SetRelativeRotation(BaseCameraRotation +
        FRotator(.11f * RecoveryBreath, 0.f, .12f * Tremor * BreathStrain * BracePoseAlpha));
    const FVector DesiredEye = FirstPersonCameraComponent->GetComponentLocation();
    CameraBoom->AddWorldOffset(ConstrainEyeToClearSpace(DesiredEye) - DesiredEye);
}

FVector ATallowStealthCharacter::ConstrainEyeToClearSpace(const FVector& DesiredEye) const
{
    // Existing chair/hide cameras retain their own scripted ownership.
    if (!GetWorld() || IsTiedToChair() || IsHiddenInSpot()) return DesiredEye;
    const FVector Start = GetActorLocation();
    FHitResult Surface;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerEyeClearance), false, this);
    if (GetWorld()->SweepSingleByChannel(Surface, Start, DesiredEye, FQuat::Identity,
        ECC_Visibility, FCollisionShape::MakeSphere(4.f), Query))
    {
        if (Surface.bStartPenetrating) return Start;
        return Surface.Location - (DesiredEye - Start).GetSafeNormal() * .5f;
    }
    return DesiredEye;
}

void ATallowStealthCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    // Camera-manager evaluation can precede our presentation tick. Apply the
    // same physical clearance to the rendered eye so a fast crouch/look cannot
    // show through a lintel for that intermediate frame.
    OutResult.Location = ConstrainEyeToClearSpace(OutResult.Location);
}

void ATallowStealthCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    CameraCrouchOffset += HalfHeightAdjust;
    CameraBoom->AddRelativeLocation(FVector(0.f, 0.f, HalfHeightAdjust));
}

void ATallowStealthCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    CameraCrouchOffset -= HalfHeightAdjust;
    CameraBoom->AddRelativeLocation(FVector(0.f, 0.f, -HalfHeightAdjust));
}

void ATallowStealthCharacter::DoAim(float Yaw, float Pitch)
{
    Super::DoAim(Yaw, Pitch);
}

void ATallowStealthCharacter::DoMove(float Right, float Forward)
{
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling) return;
    if (bIsCrouched && Forward > .25f && TraversalMode == ETallowPlayerTraversalMode::None)
        TryStartSqueeze();
    Super::DoMove(Right, Forward);
}

void ATallowStealthCharacter::RestoreMovementSpeed()
{
    auto* Movement = GetCharacterMovement();
    if (!Movement) return;
    const bool bCupped = CandleComponent && CandleComponent->GetCandleState() == ECandleState::Cupped;
    Movement->MaxWalkSpeed = bSprintInput && !bCupped && !bIsCrouched ? RunSpeed : Walkspeed * (bCupped ? .65f : 1.f);
    Movement->MaxWalkSpeedCrouched = TraversalMode == ETallowPlayerTraversalMode::Squeezing ? SqueezeSpeed : 105.f;
}

void ATallowStealthCharacter::DoRunStart()
{
    if (TraversalMode != ETallowPlayerTraversalMode::None) return;
    EndBreathHold();
    if (bOwnsCup && !bManualCup && CandleComponent)
    {
        bOwnsCup = false; ShieldReleaseDelay = 0.f;
        if (CandleComponent->GetCandleState() == ECandleState::Cupped)
            CandleComponent->SetCandleState(ECandleState::Lit);
    }
    bSprintInput = true;
    Super::DoRunStart();
    RestoreMovementSpeed();
}
void ATallowStealthCharacter::DoRunEnd()
{
    bSprintInput = false;
    Super::DoRunEnd();
    RestoreMovementSpeed();
}
void ATallowStealthCharacter::DoJumpStart()
{
    if (IsTiedToChair()) { Super::DoJumpStart(); return; }
    EndBreathHold();
    if (TraversalMode != ETallowPlayerTraversalMode::None || TryStartMantle()) return;
    Super::DoJumpStart();
}
void ATallowStealthCharacter::DoCrouch()
{
    bCrouchInput = true;
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling) return;
    EndBreathHold();
    Super::DoCrouch();
}
void ATallowStealthCharacter::DoCrouchEnd()
{
    bCrouchInput = false;
    if (TraversalMode != ETallowPlayerTraversalMode::None) return;
    bStandBlocked = !CanStandHere();
    if (!bStandBlocked) Super::DoCrouchEnd();
}
void ATallowStealthCharacter::DoCup()
{
    bManualCup = true;
    Super::DoCup();
}
void ATallowStealthCharacter::DoCupEnd()
{
    bManualCup = false;
    if (bIsHoldingStill || TraversalMode != ETallowPlayerTraversalMode::None) return;
    bOwnsCup = false; ShieldReleaseDelay = 0.f;
    Super::DoCupEnd();
    RestoreMovementSpeed();
}
void ATallowStealthCharacter::DoRelight()
{
    if (TraversalMode != ETallowPlayerTraversalMode::None) return;
    ReleaseBreath(false, false);
    bOwnsCup = false; ShieldReleaseDelay = 0.f;
    Super::DoRelight();
}
void ATallowStealthCharacter::DoInteractStart()
{
    if (TraversalMode != ETallowPlayerTraversalMode::None) return;
    EndBreathHold();
    // Explicitly reaching gives up the automatic shield. A manually held cup
    // still reserves the hand, following the original interaction contract.
    if (bOwnsCup && !bManualCup && CandleComponent)
    {
        bOwnsCup = false; ShieldReleaseDelay = 0.f;
        if (CandleComponent->GetCandleState() == ECandleState::Cupped)
            CandleComponent->SetCandleState(ECandleState::Lit);
    }
    Super::DoInteractStart();
}
void ATallowStealthCharacter::DoDropItem()
{
    if (TraversalMode != ETallowPlayerTraversalMode::None) return;
    EndBreathHold();
    Super::DoDropItem();
}

bool ATallowStealthCharacter::CapsuleFits(FVector Centre, float Radius, float HalfHeight) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerTraversalFit), false, this);
    return !GetWorld()->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - .2f), FMath::Max(Radius, HalfHeight - .2f)), Query);
}
bool ATallowStealthCharacter::CapsulePathClear(FVector Start, FVector End, float Radius, float HalfHeight) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerTraversalSweep), false, this);
    return !GetWorld()->SweepTestByChannel(Start, End, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - .2f), FMath::Max(Radius, HalfHeight - .2f)), Query);
}
bool ATallowStealthCharacter::CanStandHere() const
{
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    return CapsuleFits(GetActorLocation() + FVector(0, 0, DefaultHalfHeight - HalfHeight),
        DefaultRadius, DefaultHalfHeight);
}

bool ATallowStealthCharacter::TryStartMantle()
{
    auto* Movement = GetCharacterMovement();
    if (IsTiedToChair() || IsHiddenInSpot() || TraversalMode != ETallowPlayerTraversalMode::None
        || !Movement->IsMovingOnGround() || (InteractionComponent && InteractionComponent->IsInteracting())) return false;
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    const FVector Feet = GetActorLocation() - FVector(0, 0, HalfHeight);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerMantleLedge), false, this);
    FHitResult Face, Top, HandSurface;
    const FVector FaceStart = Feet + FVector(0, 0, 32.f);
    if (!GetWorld()->LineTraceSingleByChannel(Face, FaceStart, FaceStart + Forward * 82.f, ECC_Pawn, Query)
        || FMath::Abs(Face.ImpactNormal.Z) > .35f || FVector::DotProduct(Forward, -Face.ImpactNormal) < .8f
        || !Face.GetComponent() || Face.GetComponent()->Mobility != EComponentMobility::Static) return false;
    const FVector TopProbe = Face.ImpactPoint + Forward * (Radius + 14.f);
    if (!GetWorld()->LineTraceSingleByChannel(Top, FVector(TopProbe.X, TopProbe.Y, Feet.Z + MaxMantleHeight + 6.f),
        FVector(TopProbe.X, TopProbe.Y, Feet.Z + 34.f), ECC_Pawn, Query)
        || Top.ImpactNormal.Z < .72f) return false;
    const float Height = Top.ImpactPoint.Z - Feet.Z;
    if (Height < FMath::Max(38.f, Movement->MaxStepHeight + 3.f) || Height > MaxMantleHeight) return false;
    const FVector End = Top.ImpactPoint + FVector(0, 0, HalfHeight + 3.f);
    const FVector Lift(GetActorLocation().X, GetActorLocation().Y, End.Z + 5.f);
    const FVector Across(End.X, End.Y, End.Z + 5.f);
    // Check the actual swept route, not just a landing point behind the obstacle.
    if (!CapsuleFits(End, Radius, HalfHeight) || !CapsuleFits(Lift, Radius, HalfHeight)
        || !CapsulePathClear(GetActorLocation(), Lift, Radius, HalfHeight)
        || !CapsulePathClear(Lift, Across, Radius, HalfHeight)
        || !CapsulePathClear(Across, End, Radius, HalfHeight)) return false;
    // The landing and the palm can sit on different parts of a curved lid.
    // Trace the actual hand patch instead of borrowing the landing height.
    const FVector PalmProbe = Face.ImpactPoint + Forward * 10.f - GetActorRightVector() * 8.f;
    if (!GetWorld()->LineTraceSingleByChannel(HandSurface,
        FVector(PalmProbe.X, PalmProbe.Y, Feet.Z + MaxMantleHeight + 6.f),
        FVector(PalmProbe.X, PalmProbe.Y, Feet.Z + 28.f), ECC_Pawn, Query)
        || HandSurface.ImpactNormal.Z < .65f || !HandSurface.GetComponent()
        || HandSurface.GetComponent()->Mobility != EComponentMobility::Static) return false;
    ReleaseBreath(false);
    ShieldReleaseDelay = 0.f;
    if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Lit)
    {
        bOwnsCup = true;
        CandleComponent->SetCandleState(ECandleState::Cupped);
    }
    bSprintInput = false;
    MantleStart = GetActorLocation(); MantleLift = Lift; MantleAcross = Across; MantleEnd = End;
    TraversalHandSurfaceNormal = HandSurface.ImpactNormal;
    // This is the rendered palm centre, with a small allowance for flesh above
    // the skeleton. The animation proxy inverts first-person projection before IK.
    TraversalHandTarget = HandSurface.ImpactPoint + TraversalHandSurfaceNormal * .8f;
    MantleElapsed = 0.f; TraversalProgress = 0.f;
    MantleDuration = FMath::GetMappedRangeValueClamped(FVector2D(45.f, 100.f), FVector2D(1.15f, 1.45f), Height);
    TraversalMode = ETallowPlayerTraversalMode::Mantling;
    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_Flying);
    return true;
}

bool ATallowStealthCharacter::FindSqueezeWalls(FVector Probe, FVector& Left, FVector& Right) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerSqueezeWidth), false, this);
    FHitResult L, R;
    const FVector Side = GetActorRightVector();
    if (!GetWorld()->LineTraceSingleByChannel(L, Probe, Probe - Side * 48.f, ECC_Pawn, Query)
        || !GetWorld()->LineTraceSingleByChannel(R, Probe, Probe + Side * 48.f, ECC_Pawn, Query)
        || FMath::Abs(L.ImpactNormal.Z) > .35f || FMath::Abs(R.ImpactNormal.Z) > .35f) return false;
    Left = L.ImpactPoint; Right = R.ImpactPoint;
    const float Width = FVector::Distance(Left, Right);
    return Width >= SqueezeRadius * 2.f + 4.f && Width < DefaultRadius * 2.f + 6.f;
}

bool ATallowStealthCharacter::TryStartSqueeze()
{
    if (IsTiedToChair() || IsHiddenInSpot() || !bIsCrouched || TraversalMode != ETallowPlayerTraversalMode::None
        || !GetCharacterMovement()->IsMovingOnGround()) return false;
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const FVector Probe = GetActorLocation() + Forward * 48.f;
    FVector Left, Right;
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    if (!FindSqueezeWalls(Probe, Left, Right) || !CapsuleFits(Probe, SqueezeRadius, HalfHeight)
        || !CapsulePathClear(GetActorLocation(), Probe, SqueezeRadius, HalfHeight)) return false;
    ReleaseBreath(false);
    ShieldReleaseDelay = 0.f;
    if (CandleComponent && CandleComponent->GetCandleState() == ECandleState::Lit)
    {
        bOwnsCup = true;
        CandleComponent->SetCandleState(ECandleState::Cupped);
    }
    bSprintInput = false;
    SqueezeStart = PreviousSqueezePosition = GetActorLocation();
    SqueezeTravel = 0.f; TraversalProgress = 0.f;
    TraversalHandTarget = Left + FVector(0, 0, 10.f);
    GetCapsuleComponent()->SetCapsuleRadius(SqueezeRadius, true);
    TraversalMode = ETallowPlayerTraversalMode::Squeezing;
    GetCharacterMovement()->bWantsToCrouch = true;
    RestoreMovementSpeed();
    return true;
}

void ATallowStealthCharacter::FinishMantle(bool bCompleted)
{
    TraversalMode = ETallowPlayerTraversalMode::None;
    TraversalProgress = bCompleted ? 1.f : 0.f;
    // Capture and a hide volume can take movement ownership while an action is interrupted.
    if (!IsHiddenInSpot() && GetCharacterMovement()->MovementMode == MOVE_Flying)
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->StopMovementImmediately();
    if (bOwnsCup && !bManualCup) ShieldReleaseDelay = .65f;
    if (bIsCrouched && !bCrouchInput && !IsHiddenInSpot() && !IsTiedToChair())
    {
        bStandBlocked = !CanStandHere();
        if (!bStandBlocked) UnCrouch();
    }
    RestoreMovementSpeed();
}

void ATallowStealthCharacter::UpdateTraversal(float DeltaSeconds)
{
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling)
    {
        if (IsTiedToChair() || IsHiddenInSpot() || GetCharacterMovement()->MovementMode != MOVE_Flying)
        {
            FinishMantle(false);
            return;
        }
        MantleElapsed += DeltaSeconds;
        TraversalProgress = FMath::Clamp(MantleElapsed / MantleDuration, 0.f, 1.f);
        FVector Goal;
        if (TraversalProgress < .18f)
            Goal = MantleStart;
        else if (TraversalProgress < .62f)
            Goal = FMath::Lerp(MantleStart, MantleLift, FMath::SmoothStep(.18f, .62f, TraversalProgress));
        else if (TraversalProgress < .9f)
            Goal = FMath::Lerp(MantleLift, MantleAcross, FMath::SmoothStep(.62f, .9f, TraversalProgress));
        else
            Goal = FMath::Lerp(MantleAcross, MantleEnd, FMath::SmoothStep(.9f, 1.f, TraversalProgress));
        FHitResult Block;
        SetActorLocation(Goal, true, &Block, ETeleportType::None);
        if (Block.bBlockingHit) FinishMantle(false);
        else if (TraversalProgress >= 1.f) FinishMantle(true);
    }
    else if (TraversalMode == ETallowPlayerTraversalMode::Squeezing)
    {
        if (IsHiddenInSpot()) return;
        GetCharacterMovement()->bWantsToCrouch = true;
        SqueezeTravel += FVector::Dist2D(GetActorLocation(), PreviousSqueezePosition);
        PreviousSqueezePosition = GetActorLocation();
        TraversalProgress = FMath::Clamp(SqueezeTravel / 120.f, 0.f, 1.f);
        FVector Left, Right;
        if (FindSqueezeWalls(GetActorLocation() + GetActorForwardVector() * 14.f, Left, Right))
            TraversalHandTarget = Left + FVector(0, 0, 10.f);
        const bool bCanExpand = CapsuleFits(GetActorLocation(), DefaultRadius,
            GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
        // A player can retreat freely. Releasing crouch requests an exit too, but
        // the capsule only grows once the entire larger shape actually fits.
        if (bCanExpand && (SqueezeTravel > 55.f || !bCrouchInput))
        {
            GetCapsuleComponent()->SetCapsuleRadius(DefaultRadius, true);
            TraversalMode = ETallowPlayerTraversalMode::None;
            if (bOwnsCup && !bManualCup) ShieldReleaseDelay = .65f;
            RestoreMovementSpeed();
            if (!bCrouchInput)
            {
                bStandBlocked = !CanStandHere();
                if (!bStandBlocked) UnCrouch();
            }
        }
    }
    else if (bIsCrouched && !bCrouchInput && bStandBlocked && CanStandHere())
    {
        bStandBlocked = false;
        UnCrouch();
    }
}

float ATallowStealthCharacter::GetTraversalHandWeight() const
{
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling)
        return FMath::SmoothStep(0.f, .16f, TraversalProgress)
            * (1.f - FMath::SmoothStep(.50f, .70f, TraversalProgress));
    if (TraversalMode == ETallowPlayerTraversalMode::Squeezing) return .8f;
    return 0.f;
}

FText ATallowStealthCharacter::GetStealthPrompt() const
{
    if (IsTiedToChair()) return FText::GetEmpty();
    if (TraversalMode == ETallowPlayerTraversalMode::Mantling) return FText::FromString(TEXT("Pulling up"));
    if (TraversalMode == ETallowPlayerTraversalMode::Squeezing) return FText::FromString(TEXT("Ease through. You can back out."));
    if (bStandBlocked) return FText::FromString(TEXT("Keep low. There is no room to stand."));
    if (ExhaustionCueTime > 0.f) return FText::FromString(TEXT("Catch your breath. Keep the flame covered."));
    if (bIsHoldingStill)
    {
        if (BreathStrain > .65f) return FText::FromString(TEXT("Chest tightening. Release to breathe."));
        if (IntroductionTime > 0.f) return FText::FromString(TEXT("Shield the flame. Stay behind cover. Release to breathe."));
        return FText::FromString(bBracedAgainstSurface ? TEXT("Braced. Release to breathe.") : TEXT("Hold steady. Release to breathe."));
    }
    if (BreathFraction < .24f) return FText::FromString(TEXT("Recovering breath"));
    return FText::GetEmpty();
}
