#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowMimMovementContract.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterAnimInstance.h"
#include "AI/Hunter/TallowMimAnimInstance.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "Audio/TallowKeeperVoiceComponent.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "AI/Hunter/TallowHunterSearchAnchor.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/TallowHideVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Audio/TallowFootstepComponent.h"
#include "TallowAnimation/TallowSteppedAnimationComponent.h"
#include "Player/TallowCharacter.h"
#include "Candle/TallowCandleComponent.h"
#include "Misc/MemStack.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkeletalRenderPublic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
float Cross2(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * T * (T * (6.f * T - 15.f) + 10.f); }
void Spring(FVector& X, FVector& V, const FVector& Goal, float Frequency, float Delta)
{
    float Remaining = FMath::Min(Delta, .12f);
    while (Remaining > SMALL_NUMBER)
    {
        const float Step = FMath::Min(Remaining, 1.f / 120.f);
        V += ((Goal - X) * Frequency * Frequency - V * (2.f * Frequency)) * Step;
        X += V * Step; Remaining -= Step;
    }
}
void Wobble(FVector& X, FVector& V, const FVector& Goal, float Frequency, float Damping, float Delta)
{
    // Underdamped: it overshoots and settles, which is what reads as liquid weight.
    float Remaining = FMath::Min(Delta, .12f);
    while (Remaining > SMALL_NUMBER)
    {
        const float Step = FMath::Min(Remaining, 1.f / 120.f);
        V += ((Goal - X) * Frequency * Frequency - V * (2.f * Damping * Frequency)) * Step;
        X += V * Step; Remaining -= Step;
    }
}
}

static TAutoConsoleVariable<int32> CVarKeeperProxy(TEXT("tallow.KeeperProxy"), 0,
    TEXT("1: use the low box stand-in. 0: use Mim's authored body and locomotion."));

bool UTallowHunterMotionComponent::IsProxyMode() { return CVarKeeperProxy.GetValueOnGameThread() != 0; }

void UTallowHunterMotionComponent::InitializeProxy()
{
    // Mim (25 Oct) is huge: a plush body about 1.6 m across on ten long arms, its top at about 1.8 m. The box is
    // that body, squeezed each tick to the room it has (UpdateFit), with a red nose that turns with its attention.
    bProxy = true;
    auto* Mesh = Creature->GetMesh();
    Mesh->SetVisibility(false, true); Mesh->SetHiddenInGame(true, true);
    Mesh->SetComponentTickEnabled(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Capsule = Creature->GetCapsuleComponent();
    const float HH = Capsule->GetUnscaledCapsuleHalfHeight();
    auto Paint = [this](UStaticMeshComponent* Part, const FLinearColor& Colour)
    {
        if (auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            auto* Paint = UMaterialInstanceDynamic::Create(Base, this);
            Paint->SetVectorParameterValue(TEXT("Color"), Colour);
            Part->SetMaterial(0, Paint);
        }
    };
    ProxyBody = NewObject<UStaticMeshComponent>(Creature, TEXT("ProxyBody"));
    ProxyBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    ProxyBody->SetupAttachment(Capsule);
    ProxyBody->SetRelativeScale3D(FVector(.4f, .4f, .8f));
    ProxyBody->SetRelativeLocation(FVector(0.f, 0.f, -HH + 40.f));
    ProxyBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Paint(ProxyBody, FLinearColor(.06f, .05f, .05f));
    ProxyPaint = Cast<UMaterialInstanceDynamic>(ProxyBody->GetMaterial(0));
    ProxyNose = NewObject<UStaticMeshComponent>(Creature, TEXT("ProxyNose"));
    ProxyNose->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
    ProxyNose->SetupAttachment(Capsule);
    ProxyNose->SetRelativeLocation(FVector(62.f, 0.f, -HH + 62.f));
    ProxyNose->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    ProxyNose->SetRelativeScale3D(FVector(.32f));
    ProxyNose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Paint(ProxyNose, FLinearColor(.75f, .05f, .03f));
    for (auto* Part : {ProxyBody.Get(), ProxyNose.Get()}) { Creature->AddInstanceComponent(Part); Part->RegisterComponent(); }
    LookDirection = Creature->GetActorForwardVector();
    PreviousLocation = Creature->GetActorLocation(); PreviousYaw = Creature->GetActorRotation().Yaw;
    auto* Movement = Creature->GetCharacterMovement();
    Movement->bOrientRotationToMovement = true; Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0, 220, 0); Movement->MaxAcceleration = 420.f;
    Movement->BrakingDecelerationWalking = 320.f; Movement->GroundFriction = 4.f;
    Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;
    Creature->bUseControllerRotationYaw = false;
    bInitialized = true;
}

void UTallowHunterMotionComponent::UpdateFit(float DeltaSeconds)
{
    // Mim never clips. Each tick the body takes the free space round it: it narrows to a doorway, flattens under
    // a frame, pours long through a crawl and slides off the walls, keeping its volume where it can, like a stuffed toy.
    const FTransform Frame = Creature->GetActorTransform();
    const FQuat Rotation = Frame.GetRotation();
    const FVector Up = Rotation.GetUpVector(), Forward = Rotation.GetForwardVector(), Right = Rotation.GetRightVector();
    const float HH = Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = Creature->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Base = Frame.GetLocation() - Up * HH;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterFit), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (APawn* Her = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Her);
    auto Free = [&](const FVector& From, const FVector& Direction, float Max)
    {
        FHitResult Hit;
        return GetWorld()->LineTraceSingleByChannel(Hit, From, From + Direction * Max, ECC_Pawn, Query) ? Hit.Distance : Max;
    };
    // On a wall or the ceiling it hugs the surface; on the floor it stands on its arms, the body 40 cm up.
    const bool bClinging = Up.Z < .7f;
    const FVector Wanted(FMath::Lerp(160.f, 220.f, MoundAlpha), FMath::Lerp(160.f, 220.f, MoundAlpha), FMath::Lerp(140.f, 130.f, MoundAlpha));
    const float WantedLift = bClinging ? 8.f : FMath::Lerp(40.f, 0.f, MoundAlpha);
    const float Above = Free(Base + Up * 4.f, Up, 260.f) + 4.f;
    const FVector Core = Base + Up * FMath::Clamp(Above * .5f, 15.f, 110.f);
    const float Left = Free(Core, -Right, 170.f), RightRoom = Free(Core, Right, 170.f);
    const float Ahead = Free(Core, Forward, 260.f), Behind = Free(Core, -Forward, 260.f);
    FVector Size;
    const float Lift = FMath::Clamp(Above - Wanted.Z - 4.f, 0.f, WantedLift);
    Size.Z = FMath::Clamp(Above - Lift - 4.f, 30.f, Wanted.Z);
    Size.Y = FMath::Clamp(Left + RightRoom - 6.f, 40.f, Wanted.Y);
    // What it loses across and in height it gains in length, but never longer than the space.
    Size.X = FMath::Clamp(Wanted.X * Wanted.Y * Wanted.Z / (Size.Y * Size.Z), Wanted.X, Wanted.X * 2.4f);
    Size.X = FMath::Clamp(Size.X, 40.f, Ahead + Behind - 6.f);
    FVector Offset = FVector::ZeroVector;
    if (Left < Size.Y * .5f + 3.f) Offset.Y = Size.Y * .5f + 3.f - Left;
    else if (RightRoom < Size.Y * .5f + 3.f) Offset.Y = -(Size.Y * .5f + 3.f - RightRoom);
    if (Behind < Size.X * .5f + 3.f) Offset.X = Size.X * .5f + 3.f - Behind;
    else if (Ahead < Size.X * .5f + 3.f) Offset.X = -(Size.X * .5f + 3.f - Ahead);
    const float Blend = FMath::Clamp(DeltaSeconds * 10.f, 0.f, 1.f);
    FitSize = FMath::Lerp(FitSize, Size, Blend);
    FitOffset = FMath::Lerp(FitOffset, Offset, Blend);
    FitLift = FMath::Lerp(FitLift, Lift, Blend);
    // Then the hard rule: whatever the traces missed (a corner, a body turned to a wall, the lag of the blend) it is
    // pushed out of and shrinks away from, until nothing that blocks a body is inside it.
    auto Center = [&]() { return Frame.GetLocation() + Rotation.RotateVector(FVector(FitOffset.X, FitOffset.Y, -HH + FitLift + FitSize.Z * .5f)); };
    bool bClear = false;
    for (int32 Pass = 0; Pass < 5 && !bClear; ++Pass)
    {
        const FCollisionShape Shape = FCollisionShape::MakeBox(FitSize * .5f);
        const FVector At = Center();
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByChannel(Overlaps, At, Rotation, ECC_Pawn, Shape, Query);
        bClear = true;
        for (const FOverlapResult& Overlap : Overlaps)
        {
            UPrimitiveComponent* Other = Overlap.GetComponent();
            FMTDResult MTD;
            if (!Overlap.bBlockingHit || !Other || !Other->ComputePenetration(MTD, Shape, At, Rotation) || MTD.Distance < .2f) continue;
            bClear = false;
            const FVector Push = Rotation.UnrotateVector(MTD.Direction);
            const float Depth = MTD.Distance + .5f;
            int32 Axis = FMath::Abs(Push.X) > FMath::Abs(Push.Y) ? 0 : 1;
            if (FMath::Abs(Push.Z) > FMath::Abs(Push[Axis])) Axis = 2;
            if (Axis == 2)
            {
                // Pressed from above it flattens; from below (a step, a rail) it lifts its belly over it.
                FitSize.Z = FMath::Max(30.f, FitSize.Z - Depth);
                if (Push.Z > 0.f) FitLift += Depth;
            }
            else
            {
                // The side that touches moves away; the far side stays where it is.
                FitSize[Axis] = FMath::Max(40.f, FitSize[Axis] - Depth);
                FitOffset[Axis] += FMath::Sign(Push[Axis]) * Depth * .5f;
            }
            break;
        }
    }
    if (!bClear)
    {
        // Last resort, never seen in the lab: the box that fits inside its own capsule, which the movement keeps clear.
        const FCollisionShape Shape = FCollisionShape::MakeBox(FitSize * .5f);
        if (GetWorld()->OverlapBlockingTestByChannel(Center(), Rotation, ECC_Pawn, Shape, Query))
        {
            FitSize = FVector(Radius * 1.38f, Radius * 1.38f, HH * 1.8f);
            FitOffset = FVector::ZeroVector; FitLift = HH * .1f;
        }
    }
    // Squeezed, it is slower and turns wider: a corridor is where she out-turns it, an open room where it is fastest.
    const float Narrow = FMath::Clamp((FitSize.Y - 90.f) / FMath::Max(Wanted.Y - 90.f, 1.f), 0.f, 1.f);
    const float Low = FMath::Clamp((FitSize.Z - 60.f) / FMath::Max(Wanted.Z - 60.f, 1.f), 0.f, 1.f);
    FitSpeed = bClinging || MoundAlpha > .5f ? 1.f : FMath::Lerp(.72f, 1.f, FMath::Min(Narrow, FMath::Lerp(.6f, 1.f, Low)));
}

void UTallowHunterMotionComponent::UpdateProxy(float DeltaSeconds)
{
    if (!ProxyNose || !ProxyBody) return;
    const float HH = Creature->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    // A heavy landing: the box squashes as it hits the floor, with a thud.
    const bool bFalling = Creature->GetCharacterMovement()->IsFalling();
    if (bProxyWasFalling && !bFalling)
    {
        ProxySquash = 1.f;
        PlayPlant(Creature->GetActorLocation() - FVector(0, 0, HH), false, 600.f);
    }
    bProxyWasFalling = bFalling;
    // Fewer, heavier pats for a body this size (a long stride), still soft.
    const float Speed = Creature->GetVelocity().Size();
    ProxyStride += Speed * DeltaSeconds;
    if (!bFalling && !bMound && Speed > 30.f && ProxyStride > FMath::Lerp(70.f, 110.f, FMath::Clamp(Speed / 340.f, 0.f, 1.f)))
    {
        ProxyStride = 0.f;
        const FVector Up = Creature->GetActorUpVector();
        PlayPlant(Creature->GetActorLocation() - Up * HH + Creature->GetActorRightVector() * FMath::FRandRange(-180.f, 180.f), Up.Z < .5f, Speed);
    }
    // The resting form (my concepts: "soft, fluffy, harmless"): a big low heap in its own colour. It sinks slowly,
    // and springs back up in a blink when it reveals.
    const float Before = MoundAlpha;
    MoundAlpha = FMath::FInterpConstantTo(MoundAlpha, bMound ? 1.f : 0.f, DeltaSeconds, bMound ? 1.1f : bMoundFast ? 8.f : 1.4f);
    if (bMoundFast && Before > .15f && MoundAlpha <= .15f) ProxySquash = -.45f;
    ProxySquash = FMath::FInterpTo(ProxySquash, 0.f, DeltaSeconds, 6.f);
    UpdateFit(DeltaSeconds);
    // Landings, pauses and the pop on reveal only ever make the fitted box smaller, never push it into a wall.
    const float Dip = Rhythm == ERhythm::Pause && !bMound ? .06f : 0.f;
    const float Height = FitSize.Z * FMath::Clamp(1.f - .25f * FMath::Max(ProxySquash, 0.f) - Dip, .5f, 1.f);
    // The front 18 cm of the fitted space belongs to the nose.
    const float Length = FMath::Max(FitSize.X - 18.f, 20.f);
    ProxyBody->SetRelativeScale3D(FVector(Length, FitSize.Y, Height) / 100.f);
    ProxyBody->SetRelativeLocation(FVector(FitOffset.X - 9.f, FitOffset.Y, -HH + FitLift + Height * .5f));
    // The nose shows where its attention is: it swings to look before the box turns.
    const FVector Local = Creature->GetActorTransform().InverseTransformVectorNoScale(LookDirection).GetSafeNormal();
    const FRotator Aim = Local.Rotation();
    ProxyNose->SetRelativeRotation(FRotator(Aim.Pitch - 90.f, Aim.Yaw, 0.f));
    ProxyNose->SetRelativeScale3D(FVector(FMath::Lerp(.22f, .12f, MoundAlpha), FMath::Lerp(.22f, .12f, MoundAlpha), .18f));
    ProxyNose->SetRelativeLocation(FVector(FitOffset.X + FitSize.X * .5f - 18.f, FitOffset.Y, -HH + FitLift + Height * .7f) + Local * 9.f);
    if (ProxyPaint && !FMath::IsNearlyEqual(Before, MoundAlpha))
        ProxyPaint->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(.06f, .05f, .05f), FLinearColor(.34f, .30f, .40f), MoundAlpha));
}

void UTallowHunterMotionComponent::BeginSqueezeBeat(float Seconds)
{
    SqueezeBeatUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

TArray<FTallowClipHit> UTallowHunterMotionComponent::AuditClipping(float MinDepth) const
{
    TArray<FTallowClipHit> Hits;
    if (!Creature) return Hits;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterClipAudit), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (APawn* Her = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Her);
    auto Test = [&](const FString& Part, const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape)
    {
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByChannel(Overlaps, Center, Rotation, ECC_Pawn, Shape, Query);
        for (const FOverlapResult& Overlap : Overlaps)
        {
            UPrimitiveComponent* Other = Overlap.GetComponent();
            FMTDResult MTD;
            if (!Overlap.bBlockingHit || !Other || !Other->ComputePenetration(MTD, Shape, Center, Rotation) || MTD.Distance <= MinDepth) continue;
            FTallowClipHit Hit;
            Hit.Part = Part; Hit.Against = GetNameSafe(Other->GetOwner()) + TEXT(".") + Other->GetName(); Hit.Depth = MTD.Distance;
            Hits.Add(Hit);
        }
    };
    if (bProxy && ProxyBody)
    {
        const FTransform Body = ProxyBody->GetComponentTransform();
        Test(TEXT("Body"), Body.GetLocation(), Body.GetRotation(), FCollisionShape::MakeBox(Body.GetScale3D() * 50.f));
        if (ProxyNose) Test(TEXT("Nose"), ProxyNose->GetComponentLocation(), FQuat::Identity, FCollisionShape::MakeSphere(9.f));
        return Hits;
    }
    // The rigged body: each arm segment as a capsule, and the body.
    const auto* Mesh = Creature->GetMesh();
    if (!Mesh) return Hits;
    if (bUsingMim)
    {
        auto Segment = [&](const FString& Part, FName A, FName B, float Radius)
        {
            if (Mesh->GetBoneIndex(A) == INDEX_NONE || Mesh->GetBoneIndex(B) == INDEX_NONE) return;
            const FVector From = Mesh->GetBoneLocation(A), To = Mesh->GetBoneLocation(B);
            const FVector Along = To - From;
            if (Along.Size() < 1.f) return;
            Test(Part, (From + To) * .5f, FRotationMatrix::MakeFromZ(Along).ToQuat(),
                FCollisionShape::MakeCapsule(Radius, Along.Size() * .5f + Radius));
        };
        for (int32 Pair = 1; Pair <= 5; ++Pair)
            for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
            {
                const FString Prefix = FString::Printf(TEXT("arm%02d_"), Pair);
                const FName Upper(*FString::Printf(TEXT("%supper_%s"), *Prefix, Side));
                const FName Fore(*FString::Printf(TEXT("%sfore_%s"), *Prefix, Side));
                const FName Hand(*FString::Printf(TEXT("%shand_%s"), *Prefix, Side));
                Segment(FString::Printf(TEXT("Arm%d%s.Upper"), Pair, Side), Upper, Fore, 5.5f);
                Segment(FString::Printf(TEXT("Arm%d%s.Fore"), Pair, Side), Fore, Hand, 5.0f);
            }
        Segment(TEXT("Body"), TEXT("chest"), TEXT("abdomen_02"), 26.f);
        if (Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
            Test(TEXT("Head"), Mesh->GetBoneLocation(TEXT("head")), FQuat::Identity,
                FCollisionShape::MakeSphere(24.f));
        return Hits;
    }
    for (int32 I = 0; I < Hands.Num(); ++I)
    {
        const FTallowHunterHand& H = Hands[I];
        const FVector Points[] = {Mesh->GetBoneLocation(H.Upper), Mesh->GetBoneLocation(H.Lower), Mesh->GetBoneLocation(H.Hand)};
        for (int32 S = 0; S < 2; ++S)
        {
            const FVector Along = Points[S + 1] - Points[S];
            const float Half = Along.Size() * .5f;
            if (Half < 1.f) continue;
            Test(FString::Printf(TEXT("Arm%d.%s"), I, S == 0 ? TEXT("Upper") : TEXT("Fore")), (Points[S] + Points[S + 1]) * .5f,
                FRotationMatrix::MakeFromZ(Along).ToQuat(), FCollisionShape::MakeCapsule(6.f, Half + 6.f));
        }
    }
    Test(TEXT("Body"), Mesh->GetBoneLocation(TEXT("DEF_body")), FQuat::Identity, FCollisionShape::MakeSphere(40.f));
    return Hits;
}

bool UTallowHunterMotionComponent::CanCheckedRecoveryTranslate(const FVector& Destination, FString& Reason) const
{
    if (!Creature || !bUsingMim || Creature->GetActorUpVector().Z < .95f
        || !Creature->GetCharacterMovement()->IsMovingOnGround())
    { Reason = TEXT("checked escape requires an upright grounded body"); return false; }
    const FVector Here = Creature->GetActorLocation();
    FVector Delta = Destination - Here;
    if (FMath::Abs(Delta.Z) > 12.f || Delta.Size2D() > 120.1f || Delta.Size2D() < .5f)
    { Reason = TEXT("checked escape exceeds the120cm level local edge"); return false; }
    // This edge deliberately keeps the actual heading and capsule height.
    // Its executor rechecks the changing evaluated pose in <=8cm slices.
    Delta.Z = 0.f;
    const auto* Capsule = Creature->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimCheckedEscape), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    const auto CapsuleShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
        Capsule->GetScaledCapsuleHalfHeight());
    FHitResult CapsuleHit;
    if (GetWorld()->SweepSingleByProfile(CapsuleHit, Here, Here + Delta, Creature->GetActorQuat(),
        Capsule->GetCollisionProfileName(), CapsuleShape, Query))
    { Reason = TEXT("checked escape capsule blocked by ") + GetNameSafe(CapsuleHit.GetActor()); return false; }
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Delta.Size() / 2.f));
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const FVector At = Here + Delta * (float(Step) / Steps);
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor, At + FVector::UpVector * 15.f,
            At - FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 60.f), ECC_Pawn, Query)
            || Floor.ImpactNormal.Z < .7f
            || FMath::Abs(Floor.ImpactPoint.Z - (Here.Z - Capsule->GetScaledCapsuleHalfHeight())) > 12.f)
        { Reason = TEXT("checked escape lacks continuous level floor support"); return false; }
    }
    return CanCheckedBodyTranslate(Delta, Reason);
}

bool UTallowHunterMotionComponent::CanCheckedGroundAdvance(const FVector& Toward, FString& Reason) const
{
    if (!Creature || !bUsingMim || Creature->GetActorUpVector().Z < .95f || Toward.ContainsNaN())
    { Reason = TEXT("checked ground advance requires an upright evaluated body"); return false; }
    auto* Movement = Creature->GetCharacterMovement();
    if (!Movement || !Movement->IsMovingOnGround())
    { Reason = TEXT("checked ground advance requires grounded movement"); return false; }
    const FVector Here = Creature->GetActorLocation();
    FVector Advance = Toward - Here; Advance.Z = 0.f;
    if (Advance.SizeSquared() < .25f) return false;
    const float FrameTravel = FMath::Max(Movement->MaxWalkSpeed, float(Movement->Velocity.Size2D()))
        * GetWorld()->GetDeltaSeconds() + 2.f;
    if (FrameTravel > 32.f)
    { Reason = TEXT("checked ground advance requires a shorter movement frame"); return false; }
    Advance = Advance.GetClampedToMaxSize(FMath::Max(8.f, FrameTravel));
    const auto* Capsule = Creature->GetCapsuleComponent();
    const float HH = Capsule->GetScaledCapsuleHalfHeight();
    FFindFloorResult CurrentFloor;
    Movement->FindFloor(Here, CurrentFloor, false);
    if (!CurrentFloor.IsWalkableFloor())
    { Reason = TEXT("checked ground advance lacks current walkable capsule support"); return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimCheckedGroundAdvance), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    const FVector CurrentBase = Here - FVector::UpVector * HH;
    const FVector NextBase = CurrentBase + Advance;
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByChannel(Floor,
        NextBase + FVector::UpVector * (Movement->MaxStepHeight + 4.f),
        NextBase - FVector::UpVector * (Movement->MaxStepHeight + 60.f), ECC_Pawn, Query)
        || !Movement->IsWalkable(Floor))
    { Reason = TEXT("checked ground advance lacks next walkable support"); return false; }
    const float Rise = Floor.ImpactPoint.Z - CurrentFloor.HitResult.ImpactPoint.Z;
    if (FMath::Abs(Rise) > Movement->MaxStepHeight + .1f)
    { Reason = TEXT("checked ground advance exceeds actual step height"); return false; }
    const float FloorGap = FMath::Clamp(CurrentFloor.FloorDist,
        UCharacterMovementComponent::MIN_FLOOR_DIST, UCharacterMovementComponent::MAX_FLOOR_DIST);
    FVector Wanted = Here + Advance;
    Wanted.Z = Floor.ImpactPoint.Z + HH + FloorGap;
    const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HH);
    auto CapsuleClear = [&](const FVector& From, const FVector& To)
    {
        FHitResult Hit;
        return !GetWorld()->SweepSingleByProfile(Hit, From, To, Creature->GetActorQuat(),
            Capsule->GetCollisionProfileName(), Shape, Query);
    };
    if (!CapsuleClear(Here, Wanted))
    {
        if (Rise <= 0.f)
        { Reason = TEXT("checked ground advance capsule blocked on level/downward step"); return false; }
        // Validate the real walkable-riser envelope rather than moving her
        // diagonally through its face. CharacterMovement still executes it.
        const float Lift = FMath::Max(0.f, Movement->MaxStepHeight - CurrentFloor.FloorDist);
        const FVector Raised = Here + FVector::UpVector * Lift;
        const FVector Across = Raised + Advance;
        if (!CapsuleClear(Here, Raised) || !CapsuleClear(Raised, Across) || !CapsuleClear(Across, Wanted))
        { Reason = TEXT("checked ground advance capsule step-up envelope blocked"); return false; }
        if (!CanCheckedBodyTranslate(Raised - Here, Reason)
            || !CanCheckedBodyTranslate(Across - Here, Reason)) return false;
    }
    if (!CanCheckedBodyTranslate(Wanted - Here, Reason)) return false;
    Reason = TEXT("checked next-frame walkable body/capsule advance");
    return true;
}

bool UTallowHunterMotionComponent::CanCheckedBodyTranslate(const FVector& WorldDelta, FString& Reason, bool bTrunkOnly) const
{
    if (bTrunkOnly && Creature && bUsingMim)
    {
        const auto From = UTallowMimMovementContract::GetConfiguration(Creature);
        auto To = From; To.Position += WorldDelta;
        const auto Result = UTallowMimMovementContract::ValidateTransition(Creature, From, To, false);
        Reason = Result.Reason;
        return Result.bBodyClear;
    }
    if (!Creature || !bUsingMim || WorldDelta.ContainsNaN())
    { Reason = TEXT("checked body translation has no valid evaluated body"); return false; }
    const auto* Mesh = Creature->GetMesh();
    if (!Mesh) return false;
    const FVector Delta = WorldDelta;
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Delta.Size() / 2.f));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimCheckedBodyTranslate), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    struct FPart { FString Name; FVector Center; FQuat Rotation; FCollisionShape Shape; };
    TArray<FPart, TInlineAllocator<24>> Parts;
    auto Segment = [&](FName A, FName B, float Radius)
    {
        if (Mesh->GetBoneIndex(A) == INDEX_NONE || Mesh->GetBoneIndex(B) == INDEX_NONE) return;
        const FVector From = Mesh->GetBoneLocation(A), To = Mesh->GetBoneLocation(B), Along = To - From;
        if (Along.Size() > 1.f) Parts.Add({A.ToString(), (From + To) * .5f,
            FRotationMatrix::MakeFromZ(Along).ToQuat(), FCollisionShape::MakeCapsule(Radius, Along.Size() * .5f + Radius)});
    };
    // The same22 arm, torso and head shapes as AuditClipping/checked-step.
    for (int32 Pair = 1; !bTrunkOnly && Pair <= 5; ++Pair) for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
    {
        Segment(FName(*FString::Printf(TEXT("arm%02d_upper_%s"), Pair, Side)),
            FName(*FString::Printf(TEXT("arm%02d_fore_%s"), Pair, Side)), 5.5f);
        Segment(FName(*FString::Printf(TEXT("arm%02d_fore_%s"), Pair, Side)),
            FName(*FString::Printf(TEXT("arm%02d_hand_%s"), Pair, Side)), 5.f);
    }
    Segment(TEXT("chest"), TEXT("abdomen_02"), 26.f);
    Parts.Add({TEXT("head"), Mesh->GetBoneLocation(TEXT("head")), FQuat::Identity, FCollisionShape::MakeSphere(24.f)});
    float InitialDepth = 0.f, EndDepth = 0.f;
    for (const FPart& Part : Parts)
    {
        TMap<UPrimitiveComponent*, float> Previous, Initial;
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const FVector At = Part.Center + Delta * (float(Step) / Steps);
            TArray<FOverlapResult> Overlaps;
            GetWorld()->OverlapMultiByChannel(Overlaps, At, Part.Rotation, ECC_Pawn, Part.Shape, Query);
            TMap<UPrimitiveComponent*, float> Current;
            for (const auto& Overlap : Overlaps)
            {
                auto* Other = Overlap.GetComponent(); FMTDResult MTD;
                if (!Overlap.bBlockingHit || !Other || !Other->ComputePenetration(MTD, Part.Shape, At, Part.Rotation)) continue;
                Current.FindOrAdd(Other) = FMath::Max(Current.FindRef(Other), MTD.Distance);
                if (Step > 0 && MTD.Distance > FMath::Max(2.f,
                    FMath::Min(Initial.FindRef(Other), Previous.FindRef(Other)) + .1f))
                {
                    Reason = FString::Printf(TEXT("checked escape %s would deepen %s contact %.2f->%.2fcm"),
                        *Part.Name, *GetNameSafe(Other->GetOwner()), Previous.FindRef(Other), MTD.Distance);
                    return false;
                }
            }
            for (const auto& Contact : Current)
            {
                if (Step == 0) InitialDepth += Contact.Value;
                if (Step == Steps) EndDepth += Contact.Value;
            }
            if (Step == 0) Initial = Current;
            Previous = MoveTemp(Current);
        }
    }
    Reason = FString::Printf(TEXT("checked body translation; contact %.2f->%.2fcm"), InitialDepth, EndDepth);
    return true;
}

bool UTallowHunterMotionComponent::TryCheckedRecoveryStep(const FVector& Toward, FString& Reason, bool bTurnOnly)
{
    if (!Creature || !bUsingMim || Creature->GetActorUpVector().Z < .95f) return false;
    auto* Movement = Creature->GetCharacterMovement();
    if (bTurnOnly && !Movement->IsMovingOnGround()) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - MimRecoveryStepAt > 1.f) MimRecoveryStepSince = Now;
    if (Now - MimRecoveryStepSince > 8.f) { Reason = TEXT("checked recovery time exhausted"); return false; }
    const float Dt = MimRecoveryStepAt < 0.f ? .1f : FMath::Clamp(Now - MimRecoveryStepAt, .004f, .12f);
    MimRecoveryStepAt = Now;
    const FVector Here = Creature->GetActorLocation();
    FVector Delta = Toward - Here; Delta.Z = 0.f;
    const float Distance = Delta.Size();
    if (Distance < .5f) return false;
    FQuat Turn = FQuat::Identity;
    float TurnDegrees = 0.f;
    if (bTurnOnly)
    {
        const float Difference = FMath::FindDeltaAngleDegrees(Creature->GetActorRotation().Yaw, Delta.Rotation().Yaw);
        if (FMath::Abs(Difference) < .5f) return false;
        TurnDegrees = FMath::Clamp(Difference, -FMath::Min(3.f, 90.f * Dt), FMath::Min(3.f, 90.f * Dt));
        Turn = FQuat(FVector::UpVector, FMath::DegreesToRadians(TurnDegrees));
        Delta = FVector::ZeroVector;
    }
    else Delta *= FMath::Min(Distance, FMath::Min(8.f, 60.f * Dt)) / Distance;
    auto* Capsule = Creature->GetCapsuleComponent();
    const auto* Mesh = Creature->GetMesh();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimCheckedRecovery), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
    TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
    FHitResult Floor, CapsuleHit;
    const FVector Wanted = Here + Delta;
    if (bTurnOnly)
    {
        FFindFloorResult SupportedFloor;
        Movement->FindFloor(Here, SupportedFloor, false);
        if (!SupportedFloor.IsWalkableFloor())
        { Reason = TEXT("checked body turn lacks walkable capsule floor support"); return false; }
    }
    else if (!GetWorld()->LineTraceSingleByChannel(Floor, Wanted + FVector::UpVector * 15.f,
        Wanted - FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 60.f), ECC_Pawn, Query)
        || Floor.ImpactNormal.Z < .7f
        || FMath::Abs(Floor.ImpactPoint.Z - (Here.Z - Capsule->GetScaledCapsuleHalfHeight())) > 12.f)
    { Reason = TEXT("checked recovery lacks a level supporting floor"); return false; }
    if (GetWorld()->SweepSingleByProfile(CapsuleHit, Here, Wanted, Creature->GetActorQuat(),
        Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
            Capsule->GetScaledCapsuleHalfHeight()), Query))
    { Reason = TEXT("checked recovery capsule blocked by ") + GetNameSafe(CapsuleHit.GetActor()); return false; }
    if (bTurnOnly)
        for (int32 I = 1, Count = FMath::Max(1, FMath::CeilToInt(FMath::Abs(TurnDegrees))); I <= Count; ++I)
            if (GetWorld()->OverlapBlockingTestByProfile(Here,
                FQuat::Slerp(FQuat::Identity, Turn, float(I) / Count) * Creature->GetActorQuat(),
                Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
                    Capsule->GetScaledCapsuleHalfHeight()), Query))
            { Reason = TEXT("checked body turn capsule blocked"); return false; }
    struct FPart { FVector Center; FQuat Rotation; FCollisionShape Shape; };
    TArray<FPart, TInlineAllocator<24>> Parts;
    auto Segment = [&](FName A, FName B, float Radius)
    {
        if (Mesh->GetBoneIndex(A) == INDEX_NONE || Mesh->GetBoneIndex(B) == INDEX_NONE) return;
        const FVector From = Mesh->GetBoneLocation(A), To = Mesh->GetBoneLocation(B), Along = To - From;
        if (Along.Size() > 1.f) Parts.Add({(From + To) * .5f, FRotationMatrix::MakeFromZ(Along).ToQuat(),
            FCollisionShape::MakeCapsule(Radius, Along.Size() * .5f + Radius)});
    };
    // Exactly the existing AuditClipping body/arm/head checks, translated in
    // small steps. Existing overlap may not worsen; new collisions reject.
    for (int32 Pair = 1; Pair <= 5; ++Pair) for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
    {
        Segment(FName(*FString::Printf(TEXT("arm%02d_upper_%s"), Pair, Side)),
            FName(*FString::Printf(TEXT("arm%02d_fore_%s"), Pair, Side)), 5.5f);
        Segment(FName(*FString::Printf(TEXT("arm%02d_fore_%s"), Pair, Side)),
            FName(*FString::Printf(TEXT("arm%02d_hand_%s"), Pair, Side)), 5.f);
    }
    Segment(TEXT("chest"), TEXT("abdomen_02"), 26.f);
    Parts.Add({Mesh->GetBoneLocation(TEXT("head")), FQuat::Identity, FCollisionShape::MakeSphere(24.f)});
    for (const FPart& Part : Parts)
    {
        TMap<UPrimitiveComponent*, float> Baseline;
        TArray<FOverlapResult> Old;
        GetWorld()->OverlapMultiByChannel(Old, Part.Center, Part.Rotation, ECC_Pawn, Part.Shape, Query);
        for (const auto& O : Old)
        {
            FMTDResult MTD; auto* Other = O.GetComponent();
            if (O.bBlockingHit && Other && Other->ComputePenetration(MTD, Part.Shape, Part.Center, Part.Rotation))
                Baseline.Add(Other, MTD.Distance);
        }
        const float RadiusFromPivot = FVector::Dist(Part.Center, Here) + Part.Shape.GetExtent().Size();
        const float Arc = FMath::Abs(FMath::DegreesToRadians(TurnDegrees)) * RadiusFromPivot;
        const int32 Steps = FMath::Max(FMath::Max(1, FMath::CeilToInt(Delta.Size() / 2.f)),
            FMath::Max(FMath::CeilToInt(Arc / 2.f), FMath::CeilToInt(FMath::Abs(TurnDegrees))));
        for (int32 Step = 1; Step <= Steps; ++Step)
        {
            const float Alpha = float(Step) / Steps;
            const FQuat Rotation = FQuat::Slerp(FQuat::Identity, Turn, Alpha);
            const FVector At = Here + Rotation.RotateVector(Part.Center - Here) + Delta * Alpha;
            const FQuat ShapeRotation = Rotation * Part.Rotation;
            TArray<FOverlapResult> New;
            GetWorld()->OverlapMultiByChannel(New, At, ShapeRotation, ECC_Pawn, Part.Shape, Query);
            for (const auto& O : New)
            {
                FMTDResult MTD; auto* Other = O.GetComponent();
                if (!O.bBlockingHit || !Other || !Other->ComputePenetration(MTD, Part.Shape, At, ShapeRotation)) continue;
                const float* OldDepth = Baseline.Find(Other);
                if (MTD.Distance > FMath::Max(2.f, OldDepth ? *OldDepth + .1f : 0.f))
                { Reason = (bTurnOnly ? TEXT("checked body turn would deepen contact with ") : TEXT("checked recovery would deepen body contact with ")) + FString(GetNameSafe(Other->GetOwner())); return false; }
            }
        }
    }
    // Stop path steering for this step only. No persistent rotation override.
    Movement->StopMovementImmediately();
    const bool bRestoreOrient = Movement->bOrientRotationToMovement;
    Movement->bOrientRotationToMovement = false;
    const FQuat BeforeRotation = Creature->GetActorQuat();
    FHitResult Hit;
    Capsule->MoveComponent(Delta, Turn * BeforeRotation, true, &Hit);
    Movement->bOrientRotationToMovement = bRestoreOrient;
    const bool bMoved = bTurnOnly ? BeforeRotation.AngularDistance(Creature->GetActorQuat()) > FMath::DegreesToRadians(.01f)
        : FVector::DistSquared(Here, Creature->GetActorLocation()) > .01f;
    Reason = bMoved ? (bTurnOnly ? TEXT("checked body turn toward floor route") : TEXT("checked floor backout/side-step"))
        : TEXT("checked floor step did not move");
    return bMoved;
}

void UTallowHunterMotionComponent::SetMound(bool bOn, bool bFast)
{
    bMound = bOn;
    bMoundFast = !bOn && bFast;
}

UTallowHunterMotionComponent::UTallowHunterMotionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(
        TEXT("/Game/Presentation/KeeperRiggedV07/SK_KeeperRiggedV07_Mouth09.SK_KeeperRiggedV07_Mouth09"));
    KeeperMesh = Mesh.Object;
    for (const TCHAR* Name : {TEXT("Walk"), TEXT("Stalk"), TEXT("RunChase"), TEXT("Search_LookAround"),
        TEXT("Search_Peek"), TEXT("Sniff"), TEXT("WallCrawl"), TEXT("CeilingCrawl"),
        TEXT("LostYou"), TEXT("Squeeze"), TEXT("Listen"), TEXT("TurnL"), TEXT("TurnR")})
    {
        const FString Path = FString::Printf(TEXT("/Game/Presentation/KeeperV08/AN_Keeper_%s.AN_Keeper_%s"), Name, Name);
        if (auto* Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) Clips.Add(FName(Name), Clip);
    }
}

void UTallowHunterMotionComponent::InitializeMesh()
{
    Creature = Cast<ATallowHunterCreature>(GetOwner());
    if (!Creature) return;
    if (IsProxyMode()) { InitializeProxy(); return; }
    FString Set = FPlatformMisc::GetEnvironmentVariable(TEXT("TALLOW_KEEPER"));
    if (Set.IsEmpty() || Set.Equals(TEXT("MIM"), ESearchCase::IgnoreCase))
    {
        if (InitializeMimMesh()) return;
        Set = TEXT("V13");
    }
    Set = Set.Equals(TEXT("V12"), ESearchCase::IgnoreCase) ? TEXT("V12") : TEXT("V13");
    USkeletalMesh* Candidate = LoadObject<USkeletalMesh>(nullptr,
        *FString::Printf(TEXT("/Game/Preview/Keeper%s/SK_Keeper%s.SK_Keeper%s"), *Set, *Set, *Set), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Candidate && Set == TEXT("V13"))
    {
        Set = TEXT("V12");
        Candidate = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Preview/KeeperV12/SK_KeeperV12.SK_KeeperV12"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (Candidate)
    {
        // V12 and V13 are procedural. One held pose supplies the hands, fingers and hood; nothing else plays.
        KeeperMesh = Candidate; Clips.Reset();
        if (auto* Clip = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Preview/Keeper%s/AN_Keeper_CrouchIdle.AN_Keeper_CrouchIdle"), *Set),
            nullptr, LOAD_NoWarn | LOAD_Quiet)) { Clips.Add(TEXT("CrouchIdle"), Clip); Clips.Add(TEXT("Walk"), Clip); }
        bUsingClothRig = true; bProcedural = true;
    }
    else if (auto* Old = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Preview/KeeperV11/SK_KeeperV11.SK_KeeperV11"), nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        KeeperMesh = Old; Clips.Reset();
        for (const TCHAR* Name : {TEXT("Walk"), TEXT("Stalk"), TEXT("RunChase"), TEXT("Search_LookAround"),
            TEXT("Search_Peek"), TEXT("Sniff"), TEXT("WallCrawl"), TEXT("CeilingCrawl"), TEXT("LostYou"),
            TEXT("Squeeze"), TEXT("Listen"), TEXT("TurnL"), TEXT("TurnR"), TEXT("CrouchIdle")})
        {
            const FString Path = FString::Printf(TEXT("/Game/Preview/KeeperV11/AN_Keeper_%s.AN_Keeper_%s"), Name, Name);
            if (auto* Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) Clips.Add(FName(Name), Clip);
        }
        bUsingClothRig = true;
    }
    if (!KeeperMesh || !Clips.Contains(TEXT("Walk"))) return;
    auto* Mesh = Creature->GetMesh();
    Mesh->SetSkeletalMesh(KeeperMesh);
    // A freshly spawned creature first wears the original SK_Keeper with MI_Keeper_* per-slot overrides
    // (TallowCreatureAnimationComponent). Overrides survive the mesh swap and drew the old surface on
    // this mesh's UVs, so drop them and let the mesh's own materials show.
    Mesh->EmptyOverrideMaterials();
    Mesh->SetRelativeLocationAndRotation(FVector(0, 0, -Creature->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FRotator(0, -90, 0));
    Mesh->SetRelativeScale3D(FVector(CreatureScale));
    Mesh->SetVisibility(true, true); Mesh->SetHiddenInGame(false, true);
    Mesh->SetAnimInstanceClass(UTallowHunterAnimInstance::StaticClass());
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->AddTickPrerequisiteComponent(this);
    AddTickPrerequisiteComponent(Creature->GetCharacterMovement());
    auto BoneName = [&](const FString& Name) {
        FName Result(*Name);
        if (Mesh->GetBoneIndex(Result) == INDEX_NONE) Result = FName(*Name.Replace(TEXT("."), TEXT("_")));
        return Result;
    };
    Hands.Reset();
    const float Lengths[] = {.553332f, .575207f, .438748f};
    for (int32 Row = 1; Row <= 3; ++Row)
        for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
        {
            FTallowHunterHand H;
            const FString Prefix = FString::Printf(TEXT("DEF_arm_%d.%s"), Row, Side);
            H.Upper = BoneName(Prefix + TEXT("_upper")); H.Lower = BoneName(Prefix + TEXT("_lower"));
            H.Hand = BoneName(Prefix + TEXT("_hand")); H.Reach = Lengths[Row - 1] * 100.f * CreatureScale;
            Hands.Add(H);
        }
    HeadBone = BoneName(TEXT("head")); LookDirection = Creature->GetActorForwardVector();
    PreviousLocation = Creature->GetActorLocation(); PreviousYaw = Creature->GetActorRotation().Yaw;
    auto* Movement = Creature->GetCharacterMovement();
    Movement->bOrientRotationToMovement = true; Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0, 150, 0); Movement->MaxAcceleration = 280.f;
    Movement->BrakingDecelerationWalking = 320.f; Movement->GroundFriction = 4.f;
    Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;
    Creature->bUseControllerRotationYaw = false;
    bInitialized = true; UpdatePlayback(0.f, 0.f);
}

bool UTallowHunterMotionComponent::InitializeMimMesh()
{
    USkeletalMesh* Candidate = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mim/SK_Mim.SK_Mim"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Candidate) return false;

    static const TCHAR* Names[] = {TEXT("Creep"), TEXT("Stalk"), TEXT("Walk"), TEXT("Hunt"), TEXT("Chase")};
    MimGaitClips.Reset();
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/Characters/Mim/Anims/MIM_Loco_%s.MIM_Loco_%s"), Name, Name);
        UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Clip || Clip->GetSkeleton() != Candidate->GetSkeleton())
        {
            // Never fail silently into the Keeper fallback: name the clip that broke Mim.
            UE_LOG(LogTemp, Warning, TEXT("MIM gait clip %s missing or on the wrong skeleton (%s); Mim mesh not used"),
                *Path, *GetNameSafe(Candidate->GetSkeleton()));
            MimGaitClips.Reset();
            return false;
        }
        MimGaitClips.Add(Clip);
    }

    KeeperMesh = Candidate;
    USkeletalMeshComponent* Mesh = Creature->GetMesh();
    Mesh->SetSkeletalMesh(Candidate);
    Mesh->EmptyOverrideMaterials();
    const FBoxSphereBounds Bounds = Candidate->GetBounds();
    const float MeshBottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
    MimMeshBottom = MeshBottom;
    MimFitScale = FVector::OneVector;
    const float CapsuleHalfHeight = Creature->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight - MeshBottom), FRotator::ZeroRotator);
    Mesh->SetRelativeScale3D(FVector::OneVector);
    Mesh->SetVisibility(true, true);
    Mesh->SetHiddenInGame(false, true);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetAnimInstanceClass(UTallowMimAnimInstance::StaticClass());
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    // Mim animates every frame (off twos, only for Mim). The global stop-motion
    // subsystem exempts her too; this also undoes a per-actor stepped component.
    Mesh->SetComponentTickInterval(0.f);
    if (auto* Stepped = Creature->FindComponentByClass<UTallowSteppedAnimationComponent>()) Stepped->SetStepped(false);
    Mesh->AddTickPrerequisiteComponent(this);
    AddTickPrerequisiteComponent(Creature->GetCharacterMovement());

    Hands.Reset();
    HeadBone = TEXT("head");
    LookDirection = Creature->GetActorForwardVector();
    PreviousLocation = Creature->GetActorLocation();
    PreviousYaw = Creature->GetActorRotation().Yaw;
    auto* Movement = Creature->GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0.f, 150.f, 0.f);
    Movement->MaxAcceleration = 420.f;
    Movement->BrakingDecelerationWalking = 320.f;
    Movement->GroundFriction = 4.f;
    Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;
    Creature->bUseControllerRotationYaw = false;

    bUsingMim = true;
    bProcedural = false;
    bInitialized = true;
    MimActionName = NAME_None;
    MimActionAge = 0.f;
    MimLastEventCount = -1;
    bMimWasMound = bMound;
    UpdateMimLocomotion(0.f, 0.f);
    return true;
}

FBox UTallowHunterMotionComponent::MeasureMimPoseBounds()
{
    const double Started = FPlatformTime::Seconds();
    const auto* Mesh = Creature ? Creature->GetMesh() : nullptr;
    const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset) return FBox(ForceInit);
    const auto& Poses = Mesh->GetComponentSpaceTransforms();
    const auto& Inverse = Asset->GetRefBasesInvMatrix();
    if (MimFitSkinGroups.IsEmpty())
    {
        const auto* Render = Asset->GetResourceForRendering();
        const auto* Weights = Mesh->GetSkinWeightBuffer(0);
        if (!Render || Render->LODRenderData.IsEmpty() || !Weights
            || !Weights->GetNeedsCPUAccess() || Inverse.IsEmpty()) return FBox(ForceInit);
        const auto& LOD = Render->LODRenderData[0];
        TMap<FIntVector4, int32> Groups;
        for (const auto& Section : LOD.RenderSections)
        {
            const bool bLooseFur = GetNameSafe(Mesh->GetMaterial(Section.MaterialIndex)).Contains(TEXT("FurCard"));
            for (uint32 V = Section.BaseVertexIndex; V < Section.BaseVertexIndex + Section.NumVertices; ++V)
            {
                FIntVector4 Key(0, 0, 0, 0);
                int32 Bones[4] = {0, 0, 0, 0};
                uint32 Values[4] = {0, 0, 0, 0}, Total = 0, Used = 0;
                uint32 Offset = 0, Count = 0;
                Weights->GetVertexInfluenceOffsetCount(V, Offset, Count);
                for (uint32 I = 0; I < Count; ++I)
                {
                    const uint32 Value = Weights->GetBoneWeight(V, I);
                    const int32 SectionBone = Weights->GetBoneIndex(V, I);
                    if (!Value || !Section.BoneMap.IsValidIndex(SectionBone)) continue;
                    if (Used >= 4) { MimFitSkinGroups.Reset(); return FBox(ForceInit); }
                    const int32 Bone = Section.BoneMap[SectionBone];
                    Bones[Used] = Bone; Values[Used] = Value; Total += Value;
                    Key[Used] = int32((uint32(Bone) << 16) | Value);
                    ++Used;
                }
                if (!Total) continue;
                int32* Found = Groups.Find(Key);
                int32 GroupIndex;
                if (Found) GroupIndex = *Found;
                else
                {
                    GroupIndex = MimFitSkinGroups.AddDefaulted();
                    auto& Group = MimFitSkinGroups[GroupIndex];
                    for (int32 I = 0; I < 4; ++I)
                    {
                        Group.Bones[I] = Bones[I]; Group.Weights[I] = float(Values[I]) / Total;
                        if (Values[I])
                        {
                            const FString Name = Asset->GetRefSkeleton().GetBoneName(Bones[I]).ToString();
                            Group.bMass |= Name.StartsWith(TEXT("plush_")) || Name == TEXT("head")
                                || Name == TEXT("neck") || Name == TEXT("jaw") || Name.StartsWith(TEXT("eye"))
                                || Name == TEXT("body") || Name == TEXT("chest") || Name.StartsWith(TEXT("abdomen_"));
                        }
                    }
                    Groups.Add(Key, GroupIndex);
                }
                MimFitSkinGroups[GroupIndex].Positions.Add(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V));
                if (!bLooseFur) MimFitSkinGroups[GroupIndex].BodyPositions.Add(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V));
            }
        }
    }
    const FBox Bounds = MeasureAdaptiveFitBounds(Poses, INDEX_NONE, INDEX_NONE, false, true, false);
    MimFitMeasureMilliseconds = float((FPlatformTime::Seconds() - Started) * 1000.0);
    return Bounds;
}

FBox UTallowHunterMotionComponent::GetPosedBodyWorldBounds(FName BoneName)
{
    auto* Mesh = Creature ? Creature->GetMesh() : nullptr;
    if (!Mesh) return FBox(ForceInit);
    if (MimFitSkinGroups.IsEmpty()) MeasureMimPoseBounds();
    TArray<FTransform> WorldPose = Mesh->GetComponentSpaceTransforms();
    for (FTransform& Bone : WorldPose) Bone = Bone * Mesh->GetComponentTransform();
    const int32 Bone = BoneName.IsNone() ? INDEX_NONE : Mesh->GetBoneIndex(BoneName);
    if (!BoneName.IsNone() && Bone == INDEX_NONE) return FBox(ForceInit);
    return MeasureAdaptiveFitBounds(WorldPose, Bone, INDEX_NONE, false, true, false);
}

FBox UTallowHunterMotionComponent::MeasureAdaptiveFitBounds(const TArray<FTransform>& MeshBoneCS,
    int32 BoneA, int32 BoneB, bool bMassOnly, bool bIncludeDescendants, bool bIncludeLooseFur) const
{
    const auto* Mesh = Creature ? Creature->GetMesh() : nullptr;
    const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset || MimFitSkinGroups.IsEmpty()) return FBox(ForceInit);
    const auto& Inverse = Asset->GetRefBasesInvMatrix();
    // Exact linear skin positions without a render object/readback, including
    // fur, fingers and every small-weight influence. Vertices sharing weights
    // share one affine skin matrix. Separate-bone box unions overestimated a
    // folded head/body by tens of centimetres and falsely blocked good routes.
    TArray<FMatrix44f, TInlineAllocator<160>> Skin;
    Skin.SetNum(FMath::Min(MeshBoneCS.Num(), Inverse.Num()));
    for (int32 Bone = 0; Bone < Skin.Num(); ++Bone)
        Skin[Bone] = Inverse[Bone] * FMatrix44f(MeshBoneCS[Bone].ToMatrixWithScale());
    TArray<bool, TInlineAllocator<160>> Selected;
    if (BoneA != INDEX_NONE || BoneB != INDEX_NONE)
    {
        Selected.Init(false, Skin.Num());
        const auto& Skeleton = Asset->GetRefSkeleton();
        for (int32 Bone = 0; Bone < Selected.Num(); ++Bone)
        {
            if (!bIncludeDescendants) { Selected[Bone] = Bone == BoneA || Bone == BoneB; continue; }
            for (int32 Parent = Bone; Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
                if (Parent == BoneA || Parent == BoneB) { Selected[Bone] = true; break; }
        }
    }
    FBox3f Bounds(ForceInit);
    for (const auto& Group : MimFitSkinGroups)
    {
        if (bMassOnly && !Group.bMass) continue;
        if (BoneA != INDEX_NONE || BoneB != INDEX_NONE)
        {
            bool bArm = false;
            for (int32 I = 0; I < 4; ++I)
                bArm |= Group.Weights[I] > 0.f && Selected.IsValidIndex(Group.Bones[I]) && Selected[Group.Bones[I]];
            if (!bArm) continue;
        }
        FMatrix44f Weighted;
        FMemory::Memzero(Weighted.M, sizeof(Weighted.M));
        for (int32 I = 0; I < 4; ++I)
            if (Group.Weights[I] > 0.f && Skin.IsValidIndex(Group.Bones[I]))
                for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = 0; Col < 4; ++Col)
                    Weighted.M[Row][Col] += Skin[Group.Bones[I]].M[Row][Col] * Group.Weights[I];
        for (const FVector3f& Position : bIncludeLooseFur ? Group.Positions : Group.BodyPositions)
        {
            const FVector4f Point = Weighted.TransformPosition(Position);
            Bounds += FVector3f(Point.X, Point.Y, Point.Z);
        }
    }
    return FBox(Bounds);
}

void UTallowHunterMotionComponent::UpdateMimFit(float DeltaSeconds)
{
    // Probe below the measured ceiling. A fixed standing-height origin sits
    // inside the roof of a crawl and reports an open room on its far side.
    if (!bUsingMim || !Creature) return;
    USkeletalMeshComponent* Mesh = Creature->GetMesh();
    if (!Mesh || !KeeperMesh) return;
    const FVector Forward = Creature->GetActorForwardVector();
    const FVector Right = Creature->GetActorRightVector();
    const FVector Up = Creature->GetActorUpVector();
    const float HalfHeight = Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Base = Creature->GetActorLocation() - Up * HalfHeight;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimFit), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (APawn* Her = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Her);
    auto Free = [&](const FVector& Origin, const FVector& Direction, float Maximum, FName* Blocker = nullptr)
    {
        FHitResult Hit;
        const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * Maximum, ECC_Pawn, Query);
        if (Blocker) *Blocker = bHit && Hit.GetActor() ? Hit.GetActor()->GetFName() : NAME_None;
        return bHit ? Hit.Distance : Maximum;
    };
    float Above = Free(Base + Up * 4.f, Up, 346.f) + 4.f;
    const float LookAhead = FMath::Clamp(SmoothedSpeed * .25f, 35.f, 85.f);
    const FVector AheadBase = Base + Forward * LookAhead;
    // Keep the future sample outside solid geometry. A one-centimetre
    // allowance admitted points just inside a crate and latched zero width
    // and height, preventing even a clear sideways recovery branch.
    const bool bCanLookAhead = Free(Base + Up * 8.f, Forward, LookAhead + 2.f) > LookAhead + 1.f;
    if (bCanLookAhead)
        Above = FMath::Min(Above, Free(AheadBase + Up * 4.f, Up, 346.f) + 4.f);
    const float ProbeTop = FMath::Max(12.f, FMath::Min(Above - 8.f, 105.f));
    float Left = 400.f, RightRoom = 400.f, Ahead = 600.f, Behind = 600.f;
    for (const float Fraction : {.25f, .55f, .85f})
    {
        const FVector Core = Base + Up * FMath::Max(8.f, ProbeTop * Fraction);
        Left = FMath::Min(Left, Free(Core, -Right, 400.f));
        RightRoom = FMath::Min(RightRoom, Free(Core, Right, 400.f));
        Ahead = FMath::Min(Ahead, Free(Core, Forward, 600.f));
        Behind = FMath::Min(Behind, Free(Core, -Forward, 600.f));
        if (bCanLookAhead)
        {
            const FVector FutureCore = AheadBase + Up * FMath::Max(8.f, ProbeTop * Fraction);
            Left = FMath::Min(Left, Free(FutureCore, -Right, 400.f));
            RightRoom = FMath::Min(RightRoom, Free(FutureCore, Right, 400.f));
        }
    }
    const FVector Extent = KeeperMesh->GetBounds().BoxExtent;
    const FVector PreviousOffset = Mesh->GetRelativeLocation();
    const auto* FitTraversal = Creature->HunterTraversal.Get();
    const bool bCheckCentering = Up.Z > .95f && Creature->GetCharacterMovement()->IsMovingOnGround()
        && (!FitTraversal || !FitTraversal->IsTraversing());
    // Centering the visible body is physical movement too. Sparse aperture
    // rays cannot authorize moving her arms or torso into an internal prop.
    auto CheckedCenter = [&](const FVector2D& Target, const FVector2D& SafeFallback)
    {
        if (!bCheckCentering) return Target;
        const FVector2D Previous(PreviousOffset.X, PreviousOffset.Y);
        FVector2D Delta = Target - Previous;
        if (Delta.IsNearlyZero(.01f)) return Previous;
        Delta = Delta.GetClampedToMaxSize(FMath::Clamp(120.f * DeltaSeconds, .25f, 8.f));
        for (float Fraction : {1.f, .5f, .25f})
        {
            const FVector2D Candidate = Previous + Delta * Fraction;
            const FVector WorldDelta = Forward * (Candidate.X - Previous.X)
                + Right * (Candidate.Y - Previous.Y);
            FString Reason;
            // The fitted body shifts relative to world-pinned hands; the
            // following IK pass articulates the arms, rather than rigidly
            // translating their planted segments through the surroundings.
            if (CanCheckedBodyTranslate(WorldDelta, Reason, true)) return Candidate;
        }
        return SafeFallback;
    };
    const FVector2D SideCenter = CheckedCenter(
        FVector2D(PreviousOffset.X, FMath::Clamp((RightRoom - Left) * .5f, -90.f, 90.f)),
        FVector2D(PreviousOffset.X, PreviousOffset.Y));
    float SideShift = SideCenter.Y;
    // Longitudinal room belongs to the body's shifted lane. A trace through
    // the capsule centre can still hit the crate she has moved beside and
    // permanently reject an otherwise clear return from a floor detour.
    if (!FMath::IsNearlyZero(SideShift, 1.f))
    {
        Ahead = Behind = 600.f;
        for (const float Fraction : {.25f, .55f, .85f})
        {
            const FVector Core = Base + Right * SideShift + Up * FMath::Max(8.f, ProbeTop * Fraction);
            Ahead = FMath::Min(Ahead, Free(Core, Forward, 600.f));
            Behind = FMath::Min(Behind, Free(Core, -Forward, 600.f));
        }
    }
    const FVector2D Center = CheckedCenter(
        FVector2D(FMath::Clamp((Ahead - Behind) * .5f, -75.f, 75.f), SideShift), SideCenter);
    const float ForwardShift = Center.X;
    if (!FMath::IsNearlyEqual(SideShift, Center.Y, .01f))
    {
        SideShift = Center.Y;
        Ahead = Behind = 600.f;
        for (const float Fraction : {.25f, .55f, .85f})
        {
            const FVector Core = Base + Right * SideShift + Up * FMath::Max(8.f, ProbeTop * Fraction);
            Ahead = FMath::Min(Ahead, Free(Core, Forward, 600.f));
            Behind = FMath::Min(Behind, Free(Core, -Forward, 600.f));
        }
    }
    MimFitClearance = FVector(
        FMath::Max(0.f, 2.f * (FMath::Min(Ahead - ForwardShift, Behind + ForwardShift) - 3.f)),
        FMath::Max(0.f, 2.f * (FMath::Min(Left + SideShift, RightRoom - SideShift) - 3.f)),
        FMath::Max(0.f, Above - 2.f));
    if (const auto* Traversal = Creature->HunterTraversal.Get(); Traversal && Traversal->IsSqueezing())
    {
        const FVector2D Passage = Traversal->GetRouteClearance();
        if (Passage.X > 0.f) MimFitClearance.Y = FMath::Min(MimFitClearance.Y, Passage.X - 6.f);
        if (Passage.Y > 0.f) MimFitClearance.Z = FMath::Min(MimFitClearance.Z, Passage.Y - 6.f);
    }
    const FVector LocalClearance = MimFitClearance;
    // Envelopes are authored at rig scale 1.0; scale them with the actor so a
    // smaller Mim is not permanently folded in rooms that are big *for her*.
    const float BodyScale = FMath::Max(Creature->GetActorScale3D().X, .1f);
    const FVector BodyEnvelope = FVector(160.f, 160.f, 180.f) * BodyScale;
    const FVector NaturalPoseEnvelope = FVector(650.f, 550.f, 180.f) * BodyScale;
    const FVector FoldedPoseEnvelope = FVector(160.f, 145.f, 52.f) * BodyScale;
    const float FoldedMinimum = 52.f * BodyScale;
    const float Now = GetWorld()->GetTimeSeconds();
    const bool bUpright = Up.Z > .85f;
    FBox PoseBounds = bUpright ? MeasureMimPoseBounds() : FBox(ForceInit);
    FVector ReleaseSpace = MimFitClearance;
    if (bUpright && PoseBounds.IsValid)
    {
        // Anticipate the leading limbs and retain the fold for the rear ones.
        // Geometry is sampled at 10Hz over the full current skin footprint,
        // not merely at the capsule center. No animation phase is replaced.
        if (Now >= MimNextFootprintSample
            || FVector::DistSquared(Creature->GetActorLocation(), MimFitFootprintAt) > FMath::Square(90.f))
        {
            MimNextFootprintSample = Now + .1f;
            MimFitFootprintAt = Creature->GetActorLocation();
            MimFitFootprintClearance = FVector(800.f, 800.f, 350.f);
            MimFitMassClearance = FVector(800.f, 800.f, 350.f);
            MimMassWidthProbe = MimMassHeightProbe = FVector::ZeroVector;
            MimMassWidthLeft = MimMassWidthRight = MimMassRoof = NAME_None;
            MimFitMassBounds = MeasureAdaptiveFitBounds(Mesh->GetComponentSpaceTransforms(),
                INDEX_NONE, INDEX_NONE, true, true, false);
            if (MimFitMassBounds.IsValid)
            {
                MimFitMassBounds = MimFitMassBounds.TransformBy(Mesh->GetRelativeTransform().ToMatrixWithScale());
                // Mass bounds are rig units; probe bases and heights are world.
                const float MassBack = MimFitMassBounds.Min.X * BodyScale, MassFront = MimFitMassBounds.Max.X * BodyScale;
                const float MassCenterY = MimFitMassBounds.GetCenter().Y * BodyScale;
                const float UnscaledHalfHeight = Creature->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
                const float MassLow = (MimFitMassBounds.Min.Z + UnscaledHalfHeight) * BodyScale;
                const float MassHigh = (MimFitMassBounds.Max.Z + UnscaledHalfHeight) * BodyScale;
                const int32 MassSamples = FMath::Max(2, FMath::CeilToInt((MassFront - MassBack) / 45.f));
                for (int32 I = 0; I <= MassSamples; ++I)
                {
                    const FVector MassBase = Base + Forward * FMath::Lerp(MassBack, MassFront, float(I) / MassSamples)
                        + Right * MassCenterY;
                    // Hard body width belongs to its occupied height band,
                    // not to treads underneath an elevated torso or head.
                    FVector HeightBase = MassBase;
                    FHitResult Floor;
                    if (GetWorld()->LineTraceSingleByChannel(Floor, MassBase + Up * 80.f,
                        MassBase - Up * 120.f, ECC_Pawn, Query)
                        && FVector::DotProduct(Floor.ImpactNormal, Up) > .7f
                        && FVector::DotProduct(Floor.ImpactPoint - MassBase, Up) > 0.f)
                        HeightBase = Floor.ImpactPoint;
                    FName Roof;
                    const float Height = Free(HeightBase + Up * 4.f, Up, 346.f, &Roof) + 4.f
                        + FVector::DotProduct(HeightBase - MassBase, Up);
                    if (Height > 8.f && Height - 2.f < MimFitMassClearance.Z)
                    {
                        MimFitMassClearance.Z = FMath::Max(0.f, Height - 2.f);
                        MimMassHeightProbe = HeightBase + Up * 4.f; MimMassRoof = Roof;
                    }
                    for (const float Fraction : {.25f, .55f, .85f})
                    {
                        const FVector Core = MassBase + Up * FMath::Max(8.f, FMath::Lerp(MassLow, MassHigh, Fraction));
                        FName LeftHit, RightHit;
                        const float Width = Free(Core, -Right, 400.f, &LeftHit) + Free(Core, Right, 400.f, &RightHit) - 6.f;
                        if (Width < MimFitMassClearance.Y)
                        {
                            MimFitMassClearance.Y = FMath::Max(0.f, Width);
                            MimMassWidthProbe = Core; MimMassWidthLeft = LeftHit; MimMassWidthRight = RightHit;
                        }
                    }
                }
            }
            else MimFitMassClearance = LocalClearance;
            const FVector MeshOffset = Mesh->GetRelativeLocation();
            const float Back = FMath::Clamp((PoseBounds.Min.X * MimFitScale.X + MeshOffset.X) * BodyScale - 35.f, -600.f, 0.f);
            const float Front = FMath::Clamp((PoseBounds.Max.X * MimFitScale.X + MeshOffset.X) * BodyScale + 35.f, 0.f, 600.f);
            const int32 Samples = FMath::Max(2, FMath::CeilToInt((Front - Back) / 55.f));
            for (int32 I = 0; I <= Samples; ++I)
            {
                FVector TailBase = Base + Right * SideShift + Forward * FMath::Lerp(Back, Front, float(I) / Samples);
                FHitResult Floor;
                if (GetWorld()->LineTraceSingleByChannel(Floor, TailBase + Up * 80.f,
                    TailBase - Up * 120.f, ECC_Pawn, Query) && FVector::DotProduct(Floor.ImpactNormal, Up) > .7f)
                {
                    // A remote hand may reach over descending stairs while
                    // the mass is still on this floor. Probing upward from a
                    // lower tread measures the slab underside as a tiny crawl.
                    // The actual body floor band changes when the body descends.
                    if (FVector::DotProduct(Floor.ImpactPoint - TailBase, Up) > 0.f)
                        TailBase = Floor.ImpactPoint;
                }
                const float Height = Free(TailBase + Up * 4.f, Up, 346.f) + 4.f;
                // A footprint-box slice may start inside a solid prop or wall.
                // That is an obstacle for the actual skin/planner, not a zero-
                // height aperture that should flatten the whole body forever.
                if (Height <= 8.f) continue;
                MimFitFootprintClearance.Z = FMath::Min(MimFitFootprintClearance.Z,
                    Height + FVector::DotProduct(TailBase - Base, Up) - 2.f);
                for (const float Fraction : {.25f, .55f, .85f})
                {
                    const FVector Core = TailBase + Up * FMath::Max(8.f, FMath::Min(Height - 8.f, 105.f) * Fraction);
                    const float Width = Free(Core, -Right, 400.f) + Free(Core, Right, 400.f) - 6.f;
                    if (Width > 8.f) MimFitFootprintClearance.Y = FMath::Min(MimFitFootprintClearance.Y, Width);
                }
            }
        }
        ReleaseSpace.Y = FMath::Min(ReleaseSpace.Y, MimFitMassClearance.Y);
        ReleaseSpace.Z = FMath::Min(ReleaseSpace.Z, MimFitMassClearance.Z);
    }
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float FitMeasure = FMath::Max(0.f, FMath::Min(MimFitClearance[Axis], ReleaseSpace[Axis]));
        // Smooth both ways (was: shrink instantly, release as a step). Snapped
        // clearances snapped the fold weights, the speed cap and the mesh lane,
        // which is the visible popping/dipping. Fast in, slower out.
        MimFitLatchedClearance[Axis] = FMath::FInterpTo(MimFitLatchedClearance[Axis], FitMeasure,
            DeltaSeconds, FitMeasure < MimFitLatchedClearance[Axis] ? 12.f : 3.f);
        MimFitReleaseAt[Axis] = Now + .25f;
        MimFitClearance[Axis] = MimFitLatchedClearance[Axis];
        float PoseRoom = MimFitClearance[Axis];
        // Keep an achievable fold until the trailing skin clears its opening.
        // An empty/hand-only slice cannot demand a sub-minimum rigid torso.
        if (Axis == 1 && MimFitFootprintClearance.Y >= 76.f)
            PoseRoom = FMath::Min(PoseRoom, MimFitFootprintClearance.Y);
        if (Axis == 2 && MimFitFootprintClearance.Z >= 83.f)
            PoseRoom = FMath::Min(PoseRoom, MimFitFootprintClearance.Z);
        const float Weight = FMath::Clamp((NaturalPoseEnvelope[Axis] - PoseRoom)
            / (NaturalPoseEnvelope[Axis] - FoldedPoseEnvelope[Axis]), 0.f, 1.f);
        // Symmetric smoothing (was: rise instantly, fall at 5/s) -- the pose fold
        // must not snap when a probe crosses a threshold for one frame.
        MimFitAxisWeights[Axis] = FMath::FInterpTo(MimFitAxisWeights[Axis], Weight, DeltaSeconds,
            Weight > MimFitAxisWeights[Axis] ? 8.f : 4.f);
        MimFitAxisTargetScale[Axis] = FMath::Clamp(PoseRoom / NaturalPoseEnvelope[Axis],
            FoldedMinimum / NaturalPoseEnvelope[Axis], 1.f);
    }
    // A limb/empty footprint slice below the rigid body's minimum is a route
    // blocker, not a request to flatten the torso into that remote slice.
    // Keep the hard route gate intact; the existing swept body checks alone
    // may authorize a bounded local escape while the local pose folds safely.
    const bool bLocalPoseRecovery = bUpright && PoseBounds.IsValid
        && LocalClearance.Y >= 76.f && LocalClearance.Z >= 83.f
        && (MimFitClearance.Y < 76.f || MimFitClearance.Z < 83.f);
    MimFitPoseClearance = bLocalPoseRecovery ? LocalClearance : MimFitClearance;
    if (!bLocalPoseRecovery)
    {
        if (MimFitFootprintClearance.Y >= 76.f)
            MimFitPoseClearance.Y = FMath::Min(MimFitPoseClearance.Y, MimFitFootprintClearance.Y);
        if (MimFitFootprintClearance.Z >= 83.f)
            MimFitPoseClearance.Z = FMath::Min(MimFitPoseClearance.Z, MimFitFootprintClearance.Z);
    }
    // A narrower opening is an attempted reach, not permission to collapse
    // the body past its real minimum and pass a solid obstacle.
    bMimFitPassable = MimFitClearance.GetMin() >= FoldedMinimum;
    FVector Goal;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Size = FMath::Max(Extent[Axis] * 2.f * BodyScale, 1.f);
        Goal[Axis] = FMath::Clamp(MimFitClearance[Axis] / Size,
            FMath::Min(1.f, FoldedMinimum / Size), 1.f);
    }
    // A tall narrow frame calls for an inward fold, while an actual low
    // ceiling still needs the separate crawl fit. The authored door pose is
    // normalized locally; the global imported mesh rotation stays untouched.
    const auto* Traversal = Creature->HunterTraversal.Get();
    const bool bCanPoseFit = bUpright && (!Traversal || !Traversal->IsTransferring());
    bMimTallDoorFit = bCanPoseFit && MimFitClearance.Y <= 155.f
        && MimFitClearance.Z >= 150.f && MimFitClearance.Z > MimFitClearance.Y * 1.25f;
    const bool bIndependentFit = bCanPoseFit && PoseBounds.IsValid;
    bMimTallDoorFit &= bIndependentFit;
    if (bIndependentFit)
    {
        {
            MimPosedFitSize = PoseBounds.GetSize() * BodyScale;
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                const float Size = FMath::Max(MimPosedFitSize[Axis] + 4.f, 1.f);
                Goal[Axis] = FMath::Clamp(MimFitClearance[Axis] / Size,
                    FMath::Min(1.f, FoldedMinimum / Size), 1.f);
            }
            // Height is kept whenever the current folded skin fits the real
            // lintel. Rest-pose arms must not flatten an otherwise tall pose.
            if (MimPosedFitSize.Z + 4.f <= MimFitClearance.Z) Goal.Z = 1.f;
        }
    }
    else MimPosedFitSize = FVector::ZeroVector;
    const auto* MimAnimation = Cast<UTallowMimAnimInstance>(Mesh->GetAnimInstance());
    const bool bAdaptiveLayer = MimAnimation && MimAnimation->HasAdaptiveFitLayer();
    for (int32 Axis = 0; Axis < 3; ++Axis)
        MimFitScale[Axis] = bAdaptiveLayer ? 1.f
            : FMath::FInterpTo(MimFitScale[Axis], Goal[Axis], DeltaSeconds,
                Goal[Axis] < MimFitScale[Axis] ? 10.f : 4.f);
    Mesh->SetRelativeScale3D(MimFitScale);
    FVector Offset(ForwardShift, SideShift,
        -Creature->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - MimMeshBottom * MimFitScale.Z);
    if (bIndependentFit)
    {
        // Center actual skin in the opening, including its asymmetric imported
        // origin. Source anatomy axes are not assumed to equal actor axes.
        if (!bAdaptiveLayer) Offset.Y -= PoseBounds.GetCenter().Y * MimFitScale.Y;
        // Authored floor contacts are in a zero-height motion-root plane.
        // A fixed pad avoids iteratively moving against pinned hand bounds.
        const float OwnerScale = FMath::Max(Creature->GetActorScale3D().Z, .01f);
        Offset.Z = bAdaptiveLayer ? (-HalfHeight + 2.f) / OwnerScale
            : (-HalfHeight + 2.f) / OwnerScale - PoseBounds.Min.Z * MimFitScale.Z;
        FitSize = PoseBounds.GetSize() * BodyScale * MimFitScale;
    }
    else FitSize = Extent * BodyScale * MimFitScale * 2.f;
    Mesh->SetRelativeLocation(Offset);
    FitOffset = FVector(Offset.X, Offset.Y, 0.f);
    if (bAdaptiveLayer && bCanPoseFit)
    {
        // Refuse an impossible rigid mask/body instead of shrinking anatomy.
        // The shape layer may first need a short stationary folding beat.
        // Public clearances already include six centimetres of lateral and
        // two centimetres of top padding: raw body limits are 82 by 85 cm.
        bMimFitPassable &= MimFitClearance.Y >= 76.f && MimFitClearance.Z >= 83.f
            && MimAnimation->AdaptiveFitWithinLimits;
        if (PoseBounds.IsValid && !MimFitAxisWeights.IsNearlyZero(.01f))
            // The getter reserves6cm across the opening. Permit the agreed
            //2cm visual brush per side against the actual, unpadded jambs.
            bMimFitPassable &= FitSize.Y <= MimFitClearance.Y + 10.f
                && (PoseBounds.Max.Y * MimFitScale.Y + Offset.Y) * BodyScale <= RightRoom + 2.f
                && (PoseBounds.Min.Y * MimFitScale.Y + Offset.Y) * BodyScale >= -Left - 2.f
                // A trailing hand below a stair tread does not consume room
                // above this floor. Compare the actual top with the same
                // capsule-floor plane used by the ceiling probes.
                && (PoseBounds.Max.Z * MimFitScale.Z + Offset.Z) * BodyScale + HalfHeight <= MimFitClearance.Z + 2.f
                && (PoseBounds.Max.X * MimFitScale.X + Offset.X) * BodyScale <= Ahead - 2.f
                && (PoseBounds.Min.X * MimFitScale.X + Offset.X) * BodyScale >= -Behind + 2.f;
    }
    MimFitBlockReason.Reset();
    if (!bMimFitPassable)
        MimFitBlockReason = FString::Printf(TEXT("pose %s space %s local %s poseSpace %s footprint %s massSpace %s massBounds %s massWidthAt %s hits %s/%s massRoofAt %s hit %s bounds %s offset %s front %.1f rear %.1f layer %d"),
            *FitSize.ToString(), *MimFitClearance.ToString(), *LocalClearance.ToString(), *MimFitPoseClearance.ToString(),
            *MimFitFootprintClearance.ToString(), *MimFitMassClearance.ToString(), *MimFitMassBounds.ToString(),
            *MimMassWidthProbe.ToString(), *MimMassWidthLeft.ToString(), *MimMassWidthRight.ToString(),
            *MimMassHeightProbe.ToString(), *MimMassRoof.ToString(),
            *PoseBounds.ToString(), *Offset.ToString(), Ahead, Behind,
            MimAnimation ? int32(MimAnimation->AdaptiveFitWithinLimits) : -1);
    if (bMimFitPassable) MimFitBlockedSince = -1.f;
    else if (MimFitBlockedSince < 0.f) MimFitBlockedSince = Now;
    MimFitCompression = 0.f;
    for (int32 Axis = 0; Axis < 3; ++Axis)
        MimFitCompression = FMath::Max(MimFitCompression,
            FMath::Clamp((BodyEnvelope[Axis] - MimFitClearance[Axis])
                / (BodyEnvelope[Axis] - FoldedMinimum), 0.f, 1.f));
    const float SideHaul = FMath::SmoothStep(.4f, .9f, float(MimFitAxisWeights.Y));
    const float LowHaul = FMath::SmoothStep(.25f, .6f, float(MimFitAxisWeights.Z));
    const float CombinedHaul = FMath::Lerp(1.f, .2f, SideHaul * LowHaul);
    FitSpeed = FMath::Lerp(1.f, .16f, FMath::Pow(MimFitCompression, 1.5f)) * CombinedHaul;
    if (const auto* Brain = Cast<ATallowHunterAIController>(Creature->GetController()); Brain && Brain->bRecoveryFitAssist)
    {
        bMimFitPassable = true;
        FitSpeed = FMath::Max(FitSpeed, .55f);
    }
    if (const auto* Brain = Cast<ATallowHunterAIController>(Creature->GetController());
        Brain && Brain->bUseMovementContract && (!Traversal || !Traversal->IsTraversing()))
    {
        // Fit bounds drive the appearance. Validated physical posture owns
        // ordinary travel; a distant hand cannot reduce it to zero speed.
        const bool Compact = UTallowMimMovementContract::GetConfiguration(Creature).Posture == ETallowMimPosture::Compact;
        FitSpeed = Compact ? FMath::Clamp(FitSpeed, .3f, .55f) : FMath::Max(FitSpeed, .8f);
    }
}

float UTallowHunterMotionComponent::GetFitBlockedSeconds() const
{
    return MimFitBlockedSince >= 0.f && GetWorld() ? GetWorld()->GetTimeSeconds() - MimFitBlockedSince : 0.f;
}

bool UTallowHunterMotionComponent::PlayMimAction(FName Name, int32 Priority, bool bLoop)
{
    if (!bUsingMim || Name.IsNone() || !Name.ToString().StartsWith(TEXT("MIM_"))) return false;
    UAnimSequence* Clip = MimActionClips.FindRef(Name);
    if (!Clip)
    {
        const FString Label = Name.ToString();
        const FString Path = FString::Printf(TEXT("/Game/Characters/Mim/Anims/%s.%s"), *Label, *Label);
        Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Clip || Clip->GetSkeleton() != Creature->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
        {
            // A renamed/missing clip used to vanish silently and the gait just
            // continued, which reads as "she isn't using my animation". Say so once.
            static TSet<FName> WarnedMissingClips;
            if (!WarnedMissingClips.Contains(Name))
            {
                WarnedMissingClips.Add(Name);
                UE_LOG(LogTemp, Warning, TEXT("MIM action clip %s missing or on the wrong skeleton; action skipped"), *Path);
            }
            return false;
        }
        MimActionClips.Add(Name, Clip);
    }
    if (MimActionName == Name)
    {
        // Polling an active action must not rewind it every tick.
        return true;
    }
    if (const UAnimSequence* Current = MimActionClips.FindRef(MimActionName))
        if (!bMimActionLoop && (MimActionAge < Current->GetPlayLength() || bMimActionTracksTransfer)
            && Priority <= MimActionPriority)
            return false;
    MimActionName = Name;
    MimActionAge = 0.f;
    MimActionPriority = Priority;
    bMimActionLoop = bLoop;
    bMimActionTracksTransfer = false;
    MimActionTimeOverride = -1.f;
    MimActionReason = TEXT("Explicit action");
    ++MimActionStarts;
    return true;
}

bool UTallowHunterMotionComponent::RequestMimAction(FName Name, int32 Priority, float Cooldown,
    const FString& Reason, bool bLoop)
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (const float* Until = MimActionCooldowns.Find(Name); Until && Now < *Until) return false;
    const bool bChanged = MimActionName != Name;
    if (!PlayMimAction(Name, Priority, bLoop)) return false;
    MimActionReason = Reason;
    if (bChanged)
    {
        MimActionCooldowns.Add(Name, Now + Cooldown);
        UE_LOG(LogTemp, Display, TEXT("MIM_ACTION clip=%s reason=%s priority=%d"),
            *Name.ToString(), *Reason, Priority);
    }
    return true;
}

FName UTallowHunterMotionComponent::GetMimSearchActionForReview(const FVector& Focus) const
{
    return SelectMimSearchAction(Focus);
}

FName UTallowHunterMotionComponent::SelectMimSearchAction(const FVector& Focus) const
{
    if (!Creature || Focus.IsNearlyZero()) return TEXT("MIM_Hunt_Search_LookAround");
    // Authored hide volumes and search anchors describe a real object, without
    // exposing whether the player is occupying it.
    for (TActorIterator<ATallowHideVolume> It(GetWorld()); It; ++It)
        if (FVector::DistSquared(It->GetActorLocation(), Focus) < FMath::Square(170.f))
            return It->GetHidingSpotType() == ETallowHidingSpotType::UnderBed
                ? TEXT("MIM_Hunt_Search_Under") : TEXT("MIM_Hunt_Search_Cupboard");
    for (TActorIterator<ATallowHunterSearchAnchor> It(GetWorld()); It; ++It)
        if (It->bEnabled && FVector::DistSquared(It->GetFocusLocation(), Focus) < FMath::Square(110.f)
            && It->AnchorType == ETallowHunterAnchorType::High)
            return TEXT("MIM_Hunt_Search_High");

    const FVector Feet = Creature->GetActorLocation() - FVector::UpVector
        * Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimSearchContext), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Player);
    auto Trace = [&](const FVector& A, const FVector& B, FHitResult& Hit)
    { return GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Pawn, Query); };
    const float Height = Focus.Z - Feet.Z;
    FHitResult Above, Front;
    if (Height < 65.f && Trace(Focus + FVector(0, 0, 12), Focus + FVector(0, 0, 150), Above)
        && Above.ImpactNormal.Z < -.35f)
        return TEXT("MIM_Hunt_Search_Under");
    if (Height > 145.f) return TEXT("MIM_Hunt_Search_High");
    if (Trace(Feet + FVector(0, 0, 95), Focus, Front))
    {
        FString Label = GetNameSafe(Front.GetActor()) + TEXT(" ") + GetNameSafe(Front.GetComponent());
        if (const auto* Static = Cast<UStaticMeshComponent>(Front.GetComponent()))
            Label += TEXT(" ") + GetNameSafe(Static->GetStaticMesh());
        if (Label.Contains(TEXT("Cupboard"), ESearchCase::IgnoreCase)
            || Label.Contains(TEXT("Wardrobe"), ESearchCase::IgnoreCase)
            || Label.Contains(TEXT("Cabinet"), ESearchCase::IgnoreCase))
            return TEXT("MIM_Hunt_Search_Cupboard");
        FHitResult Top;
        if (Trace(Front.ImpactPoint + FVector(0, 0, 150), Front.ImpactPoint - FVector(0, 0, 20), Top)
            && Top.ImpactNormal.Z > .6f && Top.ImpactPoint.Z - Feet.Z < 160.f)
            return TEXT("MIM_Hunt_Search_Over");
        const FVector Side = Creature->GetActorRightVector() * 85.f;
        FHitResult Left, Right;
        const bool bLeft = Trace(Feet + FVector(0, 0, 110) - Side, Focus - Side, Left);
        const bool bRight = Trace(Feet + FVector(0, 0, 110) + Side, Focus + Side, Right);
        if (bLeft != bRight) return TEXT("MIM_Hunt_Search_Corner");
    }
    return Height < 50.f ? TEXT("MIM_Hunt_Feel_Floor") : TEXT("MIM_Hunt_Inspect");
}

void UTallowHunterMotionComponent::BeginMimCapture()
{
    if (!bUsingMim || !Creature || !MimCaptureName.IsNone()) return;
    const auto* Traversal = Creature->HunterTraversal.Get();
    const auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const TCHAR* Name = TEXT("MIM_Cap_Grab_Floor");
    if (bMound) Name = TEXT("MIM_Cap_Grab_FromMound");
    else if (Traversal && Traversal->IsTraversing() && Traversal->GetSurfaceNormal().Z < -.5f)
        Name = TEXT("MIM_Cap_Grab_FromCeiling");
    else if (Player && Player->GetCandleComponent() && Player->GetCandleComponent()->GetCandleState() == ECandleState::Out)
        Name = TEXT("MIM_Cap_Grab_Dark");
    if (PlayMimAction(Name, 5, false))
    {
        MimCaptureName = FName(Name);
        const UAnimSequence* Clip = MimActionClips.FindRef(MimCaptureName);
        MimCaptureStrikeTime = Clip ? Clip->GetPlayLength() * .80f : .75f;
        if (Clip)
            if (const FAnimSyncMarker* StrikeMarker = Clip->AuthoredSyncMarkers.FindByPredicate(
                [](const FAnimSyncMarker& Marker) { return Marker.MarkerName == TEXT("Strike"); }))
                MimCaptureStrikeTime = StrikeMarker->Time;
        MimCaptureDuration = Clip ? FMath::Max(Clip->GetPlayLength() + 1.f, MimCaptureStrikeTime + 2.9f) : 2.75f;
        UE_LOG(LogTemp, Display, TEXT("MIM_CAPTURE clip=%s strike=%.3f duration=%.3f"),
            Name, MimCaptureStrikeTime, MimCaptureDuration);
    }
}

void UTallowHunterMotionComponent::UpdateMimActions(float DeltaSeconds, float Speed)
{
    const float Now = GetWorld()->GetTimeSeconds();
    const auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
    const auto Mode = Hunter ? Hunter->HuntMode : ETallowHuntMode::Roam;
    const auto Intent = Hunter ? Hunter->GetIntent() : ETallowKeeperIntent::Idle;
    const auto* Traversal = Creature->HunterTraversal.Get();
    const bool bTraversal = Traversal && Traversal->IsTraversing();
    const bool bSqueezing = bTraversal && Traversal->IsSqueezing();
    const bool bTransfer = bTraversal && Traversal->IsTransferring();
    const bool bWaiting = Hunter && Hunter->IsWaitingForDoor();
    const bool bFalling = Creature->GetCharacterMovement()->IsFalling();
    const FName Route = Traversal ? Traversal->GetRouteKind() : NAME_None;
    const FString Phase = bTraversal ? Traversal->Phase : FString();
    const FVector Normal = bTraversal ? Traversal->GetSurfaceNormal() : FVector::UpVector;
    const FVector PendingNormal = bTransfer ? Traversal->GetPendingSurfaceNormal() : Normal;
    const FVector Here = Creature->GetActorLocation();
    const FVector TravelVelocity = bMimRoutingStarted && DeltaSeconds > SMALL_NUMBER
        ? (Here - MimPreviousRoutingLocation) / DeltaSeconds : FVector::ZeroVector;
    if (bTraversal && !bMimWasTraversing) MimSurfacePhase = 0.f;
    else if (bTraversal && !bTransfer && bMimRoutingStarted)
    {
        const float Distance = FVector::Distance(Here, MimPreviousRoutingLocation);
        const float Stride = bSqueezing ? 72.f : Normal.Z < -.5f ? 96.f : 105.f;
        if (Distance < 220.f) MimSurfacePhase = FMath::Fmod(MimSurfacePhase + Distance / Stride, 1.f);
    }
    const float Yaw = Creature->GetActorRotation().Yaw;
    const float YawDelta = bMimRoutingStarted ? FRotator::NormalizeAxis(Yaw - MimPreviousRoutingYaw) : 0.f;
    const bool bModeChanged = MimPreviousHuntMode != static_cast<int32>(Mode);
    const bool bIntentChanged = MimPreviousIntent != static_cast<int32>(Intent);
    const FName Decision = Hunter ? Hunter->LastDecision : NAME_None;
    const FString DecisionText = Decision.ToString();
    const bool bDecisionChanged = Decision != MimPreviousDecision;
    const FVector Focus = ReachUntil > Now ? ReachFocus : LookUntil > Now ? LookTarget
        : Hunter ? Hunter->GetIntentFocus() : Here + Creature->GetActorForwardVector() * 200.f;
    auto ClearAction = [&]()
    {
        MimActionName = NAME_None;
        MimActionPriority = 0;
        bMimActionLoop = bMimActionTracksTransfer = false;
        MimActionTimeOverride = -1.f;
    };
    if (const UAnimSequence* Current = MimActionClips.FindRef(MimActionName))
    {
        MimActionAge += FMath::Max(DeltaSeconds, 0.f);
        if (!bMimActionLoop && !(bMimActionTracksTransfer && bTransfer)
            && !(Traversal && Traversal->IsPouncing()
                && MimActionName == TEXT("MIM_Surf_Pounce_FromTop"))
            && MimActionAge >= Current->GetPlayLength()) ClearAction();
    }
    MimActionTimeOverride = -1.f;
    if (Mode != ETallowHuntMode::Captured && !MimCaptureName.IsNone())
    {
        MimCaptureName = NAME_None;
        MimCaptureDuration = MimCaptureStrikeTime = 0.f;
        if (MimActionName.ToString().StartsWith(TEXT("MIM_Cap_"))) ClearAction();
    }
    if (Hunter)
    {
        const FTallowKeeperEvent Event = Hunter->GetIntentEvent();
        if (Event.Count != MimLastEventCount)
        {
            MimLastEventCount = Event.Count;
            switch (Event.Type)
            {
            case ETallowKeeperEvent::Capture: BeginMimCapture(); break;
            case ETallowKeeperEvent::Reveal:
                if (bTraversal && !Traversal->IsPouncing()
                    && Traversal->LastRouteFailure.Contains(TEXT("checked climb-down route"))) break;
                RequestMimAction(bMound || bMimWasMound ? TEXT("MIM_Mound_Unfurl_Fast")
                    : bSqueezing || bMimWasSqueezing ? TEXT("MIM_Surf_Crawl_BurstOut")
                    : Normal.Z < -.5f || Route == TEXT("Drop") ? TEXT("MIM_Surf_Ceiling_Drop")
                    : Route == TEXT("Pounce") ? TEXT("MIM_Surf_Pounce_FromTop") : TEXT("MIM_Cap_Lunge"),
                    4, 2.f, TEXT("Reveal event")); break;
            case ETallowKeeperEvent::Alert: RequestMimAction(TEXT("MIM_Hunt_Alert"), 3, 2.f, TEXT("Alert event")); break;
            case ETallowKeeperEvent::Seen: RequestMimAction(TEXT("MIM_Hunt_Seen"), 3, 3.f, TEXT("Seen event")); break;
            case ETallowKeeperEvent::Heard: RequestMimAction(TEXT("MIM_Hunt_Heard_Turn"), 3, 2.f, TEXT("Heard event")); break;
            case ETallowKeeperEvent::Lost: RequestMimAction(TEXT("MIM_Hunt_Lost"), 3, 4.f, TEXT("Lost event")); break;
            default: break;
            }
        }
    }
    if (Mode == ETallowHuntMode::Captured)
    {
        BeginMimCapture();
        if (!MimActionClips.FindRef(MimActionName))
            RequestMimAction(TEXT("MIM_Cap_Hold"), 1, 0.f, TEXT("Capture hold"), true);
        bMimRoutingStarted = false;
        return;
    }

    if (bMound != bMimWasMound)
        RequestMimAction(bMound ? TEXT("MIM_Mound_Settle")
            : bMoundFast ? TEXT("MIM_Mound_Unfurl_Fast") : TEXT("MIM_Mound_Unfurl_Slow"),
            3, 1.f, TEXT("Disguise state edge"));

    // Traverse grips are sampled against the actual transfer phase. Actor
    // traversal owns translation and rotation; the animation supplies the pose.
    const bool bTransferEdge = bTransfer && (!bMimWasTransferring
        || (Phase == TEXT("Reach") && MimPreviousTraversalPhase != Phase));
    if (bTransferEdge)
    {
        MimTransferAge = 0.f;
        if (bMimActionTracksTransfer)
        {
            if (const UAnimSequence* Current = MimActionClips.FindRef(MimActionName)) MimActionAge = Current->GetPlayLength();
            bMimActionTracksTransfer = false;
        }
        FName Name;
        if (Route == TEXT("Furniture") || Route == TEXT("Climb"))
            Name = Traversal->GetTravelDirection().Z < -.1f
                ? TEXT("MIM_Surf_ClimbDown_Furniture") : TEXT("MIM_Surf_ClimbUp_Furniture");
        else if (Normal.Z > .5f && PendingNormal.Z < -.5f) Name = TEXT("MIM_Surf_FloorToCeiling");
        else if (Normal.Z > .5f && FMath::Abs(PendingNormal.Z) < .5f) Name = TEXT("MIM_Surf_FloorToWall");
        else if (Normal.Z < -.5f && FMath::Abs(PendingNormal.Z) < .5f) Name = TEXT("MIM_Surf_CeilingToWall");
        else if (FMath::Abs(Normal.Z) < .5f && PendingNormal.Z < -.5f) Name = TEXT("MIM_Surf_WallToCeiling");
        else if (FMath::Abs(Normal.Z) < .5f && PendingNormal.Z > .5f) Name = TEXT("MIM_Surf_WallToFloor");
        if (!Name.IsNone() && RequestMimAction(Name, 4, .15f, TEXT("Surface grip transfer")))
            bMimActionTracksTransfer = true;
    }
    if (bTransfer && bMimActionTracksTransfer)
    {
        MimTransferAge += DeltaSeconds;
        if (const UAnimSequence* Current = MimActionClips.FindRef(MimActionName))
        {
            const bool bDirectRise = MimActionName == TEXT("MIM_Surf_FloorToCeiling");
        const bool bDirectFloorExit = (Route == TEXT("DirectCeiling") || Route == TEXT("WallWalk"))
                && MimActionName == TEXT("MIM_Surf_WallToFloor");
            // At this low wall exit05 is above its finite floor reach. Let
            // the authored04 plant arrive before requiring a real new grip.
            // The final polished04 curves plant at frame17 of31, after the
            // earlier blocking pass. Hold beyond that real authored plant.
            const float ReachCap = bDirectFloorExit ? .58f : .34f;
            const float ReachSeconds = bDirectRise ? .60f : .30f;
            const float U = Phase == TEXT("Reach") ? FMath::Min(ReachCap, MimTransferAge / ReachSeconds * ReachCap)
                : Phase == TEXT("Settle") ? (bDirectRise ? 1.f : .98f)
                : ReachCap + (.95f - ReachCap) * Traversal->GetTransferAlpha();
            MimActionTimeOverride = Current->GetPlayLength() * U;
        }
    }
    if (bSqueezing && !bMimWasSqueezing)
        RequestMimAction(TEXT("MIM_Surf_Crawl_Enter"), 4, 1.f, TEXT("Crawl route entered"));
    else if (!bSqueezing && bMimWasSqueezing)
        RequestMimAction(Intent == ETallowKeeperIntent::Chase || Intent == ETallowKeeperIntent::Reveal
            ? TEXT("MIM_Surf_Crawl_BurstOut") : TEXT("MIM_Surf_Crawl_Exit"),
            4, 1.f, TEXT("Crawl route exited"));
    if (Route != MimPreviousRouteKind)
    {
        if (Route == TEXT("Drop")) RequestMimAction(TEXT("MIM_Surf_Ceiling_Drop"), 4, .5f, TEXT("Ceiling released"));
        else if (Route == TEXT("Pounce")) RequestMimAction(TEXT("MIM_Surf_Pounce_FromTop"), 4, .5f, TEXT("Perch pounce"));
        else if ((Route == TEXT("Climb") || Route == TEXT("Furniture")) && !bTransfer)
            RequestMimAction(Traversal->GetTravelDirection().Z < -.1f
                ? TEXT("MIM_Surf_ClimbDown_Furniture") : TEXT("MIM_Surf_ClimbUp_Furniture"),
                4, 1.f, TEXT("Furniture route entered"));
    }
    if (Traversal && Traversal->IsPouncing() && MimActionPriority < 5)
    {
        if (MimActionName != TEXT("MIM_Surf_Pounce_FromTop")) ClearAction();
        RequestMimAction(TEXT("MIM_Surf_Pounce_FromTop"), 4, 0.f, TEXT("Physical pounce trajectory"));
        if (const UAnimSequence* Pounce = MimActionClips.FindRef(MimActionName))
            MimActionTimeOverride = Traversal->GetPounceClipAlpha() * Pounce->GetPlayLength();
    }
    if (bMimWasFalling && !bFalling && MimActionPriority < 5)
    {
        if (MimActionName == TEXT("MIM_Surf_Ceiling_Drop") || MimActionName == TEXT("MIM_Surf_Pounce_FromTop")) ClearAction();
        RequestMimAction(TEXT("MIM_Surf_Land_Heavy"), 4, 1.f, TEXT("Actual landing"));
    }

    if (bDecisionChanged && DecisionText.Contains(TEXT("BlockedByBoards")))
        RequestMimAction(TEXT("MIM_Act_Door_Boarded_Feel"), 3, 12.f, TEXT("Boarded door contact"));
    else if (bDecisionChanged && DecisionText.Contains(TEXT("BlockedByLock")))
        RequestMimAction(TEXT("MIM_Act_Door_Test"), 2, 8.f, TEXT("Locked door test"));
    else if (bDecisionChanged && DecisionText.Contains(TEXT("hauling itself through")))
        RequestMimAction(MimFitScale.Y < .45f ? TEXT("MIM_Act_Door_Squeeze") : TEXT("MIM_Act_Door_PullThrough"),
            3, 4.f, TEXT("Door frame pull"));
    else if (bDecisionChanged && DecisionText.Contains(TEXT("PauseAtDoor")))
        RequestMimAction(TEXT("MIM_Act_Door_Peek"), 2, 8.f, TEXT("Open doorway peek"));
    if (bWaiting && !bMimWasWaitingForDoor) MimDoorWaitStarted = Now;
    if (bWaiting && Now - MimDoorWaitStarted > 1.2f)
        RequestMimAction(TEXT("MIM_Act_Door_Test"), 2, 7.f, TEXT("Waiting door test"));

    const bool bGround = !bTraversal && !bFalling && !bMound && !bWaiting;
    MimStationaryAge = Speed < 12.f ? MimStationaryAge + DeltaSeconds : 0.f;
    if (Speed < 12.f) MimTurnAccum = 0.f;
    if (bGround && bModeChanged && Mode == ETallowHuntMode::Listen)
        RequestMimAction(TEXT("MIM_Hunt_Listen"), 3, 3.f, TEXT("Listening begins"));
    if (bGround && bModeChanged && Mode == ETallowHuntMode::Withdraw)
        RequestMimAction(TEXT("MIM_Hunt_Withdraw"), 3, 5.f, TEXT("Withdraw mode entered"));
    if (bGround && bIntentChanged && Intent == ETallowKeeperIntent::Lunge)
        RequestMimAction(TEXT("MIM_Cap_Lunge"), 4, 2.f, TEXT("Attack windup"));
    if (bGround && Speed < 25.f && Now >= MimNextContextAt
        && (Mode == ETallowHuntMode::Inspect || Mode == ETallowHuntMode::Search)
        && (bModeChanged || FVector::DistSquared(Focus, MimPreviousSearchFocus) > FMath::Square(85.f)
            || MimStationaryAge > 3.f))
    {
        const FName Search = SelectMimSearchAction(Focus);
        if (Search != TEXT("MIM_Hunt_Search_LookAround")
            && RequestMimAction(Search, 2, 6.f, TEXT("Search target geometry")))
        {
            MimNextContextAt = Now + 6.f;
            MimPreviousSearchFocus = Focus;
        }
    }
    if (bMound && MimStationaryAge > 4.f && Now >= MimNextContextAt && Hunter
        && Hunter->GetEvidenceStrength() > .2f && FVector::DistSquared(Focus, Here) < FMath::Square(500.f))
        if (RequestMimAction(TEXT("MIM_Mound_Lure_Hand"), 2, 14.f, TEXT("Mound lure toward known evidence")))
            MimNextContextAt = Now + 14.f;

    const auto GaitName = [&]() -> FName
    {
        return Mode == ETallowHuntMode::Pursue ? TEXT("Chase")
            : Intent == ETallowKeeperIntent::Hunt ? TEXT("Hunt")
            : Intent == ETallowKeeperIntent::Stalk ? TEXT("Stalk")
            : Speed < 100.f ? TEXT("Creep") : TEXT("Walk");
    };
    if (bGround && bMimRoutingStarted)
    {
        if (!bMimRoutingMoving && Speed >= 18.f)
        {
            RequestMimAction(FName(*(TEXT("MIM_Loco_") + GaitName().ToString() + TEXT("_Start"))),
                2, 1.f, TEXT("Movement starts"));
            bMimRoutingMoving = true;
        }
        else if (bMimRoutingMoving && Speed < 12.f)
        {
            RequestMimAction(FName(*(TEXT("MIM_Loco_") + MimLastMovingGait.ToString() + TEXT("_Stop"))),
                2, 1.f, TEXT("Movement stops"));
            bMimRoutingMoving = false;
        }
        if (Speed > 25.f) MimLastMovingGait = GaitName();
        MimTurnAccum += YawDelta;
        if (Speed > 30.f && Now >= MimNextTurnAt && FMath::Abs(MimTurnAccum) > 65.f)
        {
            const bool bLeft = MimTurnAccum > 0.f;
            FName Turn;
            if (Speed > 260.f) Turn = bLeft ? TEXT("MIM_Loco_Chase_PivotL") : TEXT("MIM_Loco_Chase_PivotR");
            else
            {
                const FString Prefix = Intent == ETallowKeeperIntent::Stalk ? TEXT("MIM_Loco_Stalk_Turn") : TEXT("MIM_Loco_Turn");
                FVector Ahead;
                const bool bAround = Hunter && Hunter->GetRouteAhead(Ahead)
                    && FMath::Abs(FRotator::NormalizeAxis((Ahead - Here).Rotation().Yaw - Yaw)) > 135.f;
                Turn = FName(*(Prefix + (bLeft ? TEXT("L") : TEXT("R")) + (bAround ? TEXT("180") : TEXT("90"))));
            }
            if (RequestMimAction(Turn, 2, 3.f, TEXT("Actor turns along its path")))
            { MimNextTurnAt = Now + 3.f; MimTurnAccum = 0.f; }
        }
    }

    // Two floor traces distinguish real stairs from a body bob or a turn.
    if (bGround && Speed > 25.f)
    {
        const FVector Direction = TravelVelocity.GetSafeNormal2D();
        const FVector Feet = Here - FVector(0, 0, Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        FHitResult Ahead, Behind;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MimStairPose), false, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
        auto FloorAt = [&](const FVector& P, FHitResult& Hit)
        { return GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 140), P - FVector(0, 0, 180), ECC_Pawn, Query); };
        if (!Direction.IsNearlyZero() && FloorAt(Feet + Direction * 70.f, Ahead) && FloorAt(Feet - Direction * 70.f, Behind))
        {
            const float Rise = Ahead.ImpactPoint.Z - Behind.ImpactPoint.Z;
            if (FMath::Abs(Rise) > 10.f && FMath::Abs(Rise) < 155.f
                && Ahead.ImpactNormal.Z > .55f && Behind.ImpactNormal.Z > .55f)
            { bMimStairsUp = Rise > 0.f; MimStairUntil = Now + .35f; }
        }
    }
    FName Loop;
    if (bMound) Loop = MimStationaryAge > 10.f ? TEXT("MIM_Mound_Idle_Hum") : TEXT("MIM_Mound_Idle");
    else if (bTraversal && Route == TEXT("Corner") && Traversal->IsHolding()) Loop = TEXT("MIM_Surf_Corner_Ambush");
    else if (bSqueezing) Loop = Speed > 8.f ? TEXT("MIM_Surf_Crawl_Loop") : TEXT("MIM_Surf_Crawl_Lurk_Idle");
    else if (bTraversal && Normal.Z < -.5f) Loop = Speed > 8.f ? TEXT("MIM_Surf_Ceiling_Creep") : TEXT("MIM_Surf_Ceiling_Hang_Idle");
    else if (bTraversal && FMath::Abs(Normal.Z) < .5f) Loop = TEXT("MIM_Surf_Wall_Creep");
    else if (bTraversal && Traversal->IsHolding()) Loop = TEXT("MIM_Surf_Top_Hold_Idle");
    else if (bWaiting) Loop = TEXT("MIM_Act_Door_Wait");
    else if (bGround && Speed > 25.f && Now < MimStairUntil)
        Loop = FName(*(FString(bMimStairsUp ? TEXT("MIM_Loco_StairsUp") : TEXT("MIM_Loco_StairsDown"))
            + (Mode == ETallowHuntMode::Pursue ? TEXT("_Chase") : TEXT(""))));
    else if (bGround && Speed > 380.f) Loop = TEXT("MIM_Loco_Sprint");
    else if (Speed < 12.f && Mode == ETallowHuntMode::Listen) Loop = TEXT("MIM_Hunt_Listen_Loop");
    else if (Speed < 12.f && Mode == ETallowHuntMode::Search) Loop = TEXT("MIM_Hunt_Search_LookAround");
    else if (Speed < 12.f && Mode == ETallowHuntMode::Withdraw) Loop = TEXT("MIM_Hunt_Lost_Look");
    else if (Speed < 12.f && Mode == ETallowHuntMode::Ambush
        && (DecisionText.Contains(TEXT("door"), ESearchCase::IgnoreCase)
            || DecisionText.Contains(TEXT("WaitAtRememberedExit")))) Loop = TEXT("MIM_Hunt_Wait_Outside");
    else if (Speed < 12.f) Loop = TEXT("MIM_Loco_Idle");
    if (bGround && Speed > 20.f && Speed < 220.f && TravelVelocity.Size2D() > 10.f)
    {
        const FVector Local = Creature->GetActorTransform().InverseTransformVectorNoScale(TravelVelocity.GetSafeNormal2D());
        const FString Gait = Intent == ETallowKeeperIntent::Stalk ? TEXT("Stalk") : TEXT("Walk");
        if (Local.X < -.55f) Loop = FName(*(TEXT("MIM_Loco_") + Gait + TEXT("_Back")));
        else if (FMath::Abs(Local.Y) > FMath::Abs(Local.X) * 1.4f)
            Loop = FName(*(TEXT("MIM_Loco_") + Gait + (Local.Y > 0.f ? TEXT("_StrafeR") : TEXT("_StrafeL"))));
    }
    if (!MimActionClips.FindRef(MimActionName) || (bMimActionLoop && MimActionPriority == 1))
    {
        if (Loop.IsNone()) ClearAction();
        else RequestMimAction(Loop, 1, 0.f, TEXT("Current movement or hold"), true);
    }
    if (bGround && Speed < 12.f && Mode == ETallowHuntMode::Roam && MimStationaryAge > 5.f && Now >= MimNextIdleAt)
    {
        static const TCHAR* Idles[] = {TEXT("MIM_Idle_HeadTilt"), TEXT("MIM_Idle_PatSelf"), TEXT("MIM_Idle_Fidget"),
            TEXT("MIM_Idle_Twitch"), TEXT("MIM_Idle_Murmur"), TEXT("MIM_Idle_SelfHug")};
        if (RequestMimAction(Idles[MimIdleChoice % UE_ARRAY_COUNT(Idles)], 1, 20.f, TEXT("Quiet idle performance")))
        { ++MimIdleChoice; MimNextIdleAt = Now + 9.f; }
    }
    bMimRoutingStarted = true;
    MimPreviousRoutingLocation = Here; MimPreviousRoutingYaw = Yaw; MimPreviousSpeed = Speed;
    MimPreviousHuntMode = static_cast<int32>(Mode); MimPreviousIntent = static_cast<int32>(Intent);
    MimPreviousDecision = Decision; MimPreviousRouteKind = Route; MimPreviousTraversalPhase = Phase;
    MimPreviousRouteNormal = Normal; bMimWasTraversing = bTraversal; bMimWasSqueezing = bSqueezing;
    bMimWasTransferring = bTransfer; bMimWasWaitingForDoor = bWaiting; bMimWasFalling = bFalling;
    bMimWasMound = bMound;
}

void UTallowHunterMotionComponent::UpdateMimLocomotion(float DeltaSeconds, float Speed)
{
    if (!bUsingMim || MimGaitClips.Num() != 5) return;
    UpdateMimActions(DeltaSeconds, Speed);
    UAnimSequence* Action = MimActionClips.FindRef(MimActionName);

    // Authored strides from the five shared-phase gait clips.
    static constexpr float Speeds[] = {70.f, 150.f, 190.f, 230.f, 335.f};
    static constexpr float StridesCm[] = {105.f, 137.5f, 142.5f, 153.33f, 167.5f};
    const float BodyScale = FMath::Max(Creature->GetActorScale3D().GetAbsMin(), .01f);
    const float ModelSpeed = Speed / BodyScale;
    int32 Lower = 0;
    float Blend = 0.f;
    if (ModelSpeed >= Speeds[UE_ARRAY_COUNT(Speeds) - 1])
    {
        Lower = UE_ARRAY_COUNT(Speeds) - 1;
    }
    else if (ModelSpeed > Speeds[0])
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Speeds) - 1; ++Index)
        {
            if (ModelSpeed <= Speeds[Index + 1])
            {
                Lower = Index;
                Blend = FMath::GetRangePct(Speeds[Index], Speeds[Index + 1], ModelSpeed);
                break;
            }
        }
    }
    const int32 Upper = FMath::Min(Lower + (Blend > 0.f ? 1 : 0), MimGaitClips.Num() - 1);
    const float Stride = FMath::Lerp(StridesCm[Lower], StridesCm[Upper], Blend) * BodyScale;
    if (Speed > 1.f && DeltaSeconds > 0.f)
    {
        MimGaitPhase = FMath::Fmod(MimGaitPhase + Speed * DeltaSeconds / FMath::Max(Stride, 1.f), 1.f);
        bMimWasMoving = true;
    }
    else if (bMimWasMoving)
    {
        // The only authored support phases are Plant and PlantR. Settle on the nearer one rather than freezing a hand mid-swing.
        MimGaitPhase = FMath::Fmod(static_cast<float>(FMath::RoundToInt(MimGaitPhase * 2.f)) * .5f, 1.f);
        bMimWasMoving = false;
        if (auto* Playback = Cast<UTallowMimAnimInstance>(Creature->GetMesh()->GetAnimInstance()))
            Playback->BlendFromCurrentPose();
    }

    UAnimSequence* A = MimGaitClips[Lower];
    UAnimSequence* B = MimGaitClips[Upper];
    const float TimeA = MimGaitPhase * A->GetPlayLength();
    const float TimeB = MimGaitPhase * B->GetPlayLength();
    ActiveMotion = Lower == Upper ? A->GetName() : FString::Printf(TEXT("%s+%s"), *A->GetName(), *B->GetName());
    if (auto* Playback = Cast<UTallowMimAnimInstance>(Creature->GetMesh()->GetAnimInstance()))
    {
        Playback->SetLocomotion(A, B, Blend, TimeA, TimeB);
        const float Length = Action ? Action->GetPlayLength() : 0.f;
        float ActionTime = Action ? (bMimActionLoop ? FMath::Fmod(MimActionAge, FMath::Max(Length, .01f))
            : FMath::Min(MimActionAge, Length)) : 0.f;
        if (Action && MimActionTimeOverride >= 0.f) ActionTime = MimActionTimeOverride;
        else if (Action && bMimActionLoop && (MimActionName == TEXT("MIM_Surf_Wall_Creep")
            || MimActionName == TEXT("MIM_Surf_Ceiling_Creep") || MimActionName == TEXT("MIM_Surf_Crawl_Loop")))
            ActionTime = MimSurfacePhase * Length;
        else if (Action && bMimActionLoop && MimActionName.ToString().StartsWith(TEXT("MIM_Loco_"))
            && MimActionName != TEXT("MIM_Loco_Idle")) ActionTime = MimGaitPhase * Length;
        MimActionPlaybackTime = ActionTime;
        // Only surface, capture, grip-transfer and attack (priority 4+) actions may
        // fully replace the gait pose. Alert/door/turn one-shots blend over it while
        // she is actually moving, so authored locomotion never vanishes mid-stride.
        const FString ActionLabel = MimActionName.ToString();
        const bool bFullBody = MimActionPriority >= 4 || bMimActionTracksTransfer
            || ActionLabel.StartsWith(TEXT("MIM_Surf_")) || ActionLabel.StartsWith(TEXT("MIM_Cap_"));
        const float Weight = Action ? (!bFullBody && Speed > 45.f ? .55f : 1.f) : 0.f;
        Playback->SetAction(Action, ActionTime, Weight);
        if (Action) ActiveMotion = Action->GetName();
    }
}

void UTallowHunterMotionComponent::ResetContacts()
{
    if (bUsingMim)
    {
        MimActionName = MimCaptureName = NAME_None;
        MimActionAge = MimActionPlaybackTime = MimCaptureDuration = MimCaptureStrikeTime = 0.f;
        MimActionPriority = 0;
        bMimActionLoop = bMimActionTracksTransfer = bMimRoutingStarted = bMimRoutingMoving = false;
        bMimWasTraversing = bMimWasSqueezing = bMimWasTransferring = bMimWasWaitingForDoor = bMimWasFalling = false;
        MimActionTimeOverride = -1.f;
        MimStationaryAge = MimTurnAccum = MimSurfacePhase = 0.f;
        MimPreviousHuntMode = MimPreviousIntent = INDEX_NONE;
        MimPreviousRouteKind = MimPreviousDecision = NAME_None;
        MimNextContextAt = GetWorld()->GetTimeSeconds() + 1.f;
        MimNextIdleAt = GetWorld()->GetTimeSeconds() + 7.f;
        MimActionCooldowns.Reset();
        MimFitLatchedClearance = FVector(800.f, 800.f, 350.f);
        MimFitPoseClearance = FVector(800.f, 800.f, 350.f);
        MimFitMassClearance = FVector(800.f, 800.f, 350.f);
        MimFitMassBounds = FBox(ForceInit);
        MimFitReleaseAt = MimFitAxisWeights = FVector::ZeroVector;
        MimFitAxisTargetScale = FVector::OneVector;
        MimNextFootprintSample = 0.f;
        MimFitBlockedSince = -1.f;
        bMimWasMound = bMound;
    }
    for (auto& H : Hands)
    {
        H.bPlanted = H.bReaching = H.bAttackReach = H.bFolding = false; H.Weight = H.Grip = 0.f;
        H.SwingStarted = -1.f; H.SurfaceComponent.Reset(); H.LastReleaseReason = NAME_None; H.ElbowPole = FVector::ZeroVector;
    }
    PlantedHands = WallBraces = SupportingHands = ReachingHands = 0; SupportStrength = 0.f; NextPlant = 0.f;
    BodyTranslation = BodyVelocity = BodyAngles = BodyAngularVelocity = FVector::ZeroVector;
    SmoothedVelocity = PreviousVelocity = SmoothedAcceleration = PendingSupportShift = FVector::ZeroVector;
    bNarrowPortal = false; SqueezeAmount = ReachHeightCorrection = 0.f;
    LookUntil = ReachUntil = TurnVelocity = 0.f;
    SmoothedMaxSpeed = 0.f;
    if (Creature)
    {
        PreviousLocation = Creature->GetActorLocation();
        PreviousYaw = Creature->GetActorRotation().Yaw; PreviousUp = Creature->GetActorUpVector();
        LookDirection = Creature->GetActorForwardVector();
        LookTarget = PreviousLocation + LookDirection * 200.f; ReachFocus = LookTarget;
    }
}
TArray<float> UTallowHunterMotionComponent::GetContactErrors() const
{
    TArray<float> Result;
    if (Creature) for (const auto& H : Hands)
        if (H.bPlanted && H.Weight > .95f) Result.Add(FVector::Dist(H.Target, Creature->GetMesh()->GetBoneLocation(H.Hand)));
    return Result;
}
TArray<float> UTallowHunterMotionComponent::GetContactTipErrors() const
{
    TArray<float> Result;
    if (!Creature) return Result;
    const auto* Mesh = Creature->GetMesh();
    for (const auto& H : Hands) if (H.bPlanted && H.Weight > .95f)
    {
        const FString Prefix = H.Hand.ToString().LeftChop(4);
        FVector Tip = FVector::ZeroVector;
        for (int32 Digit = 2; Digit <= 4; ++Digit)
        {
            const FVector B = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("%sdigit%d_b"), *Prefix, Digit)));
            const FVector C = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("%sdigit%d_c"), *Prefix, Digit)));
            Tip += C + (C - B) * .8f;
        }
        Result.Add(FVector::Dist(Tip / 3.f, H.Anchor));
    }
    return Result;
}
TArray<FVector> UTallowHunterMotionComponent::GetPlantedContactPoints() const
{
    TArray<FVector> Result;
    for (const auto& H : Hands) if (H.bPlanted && H.Grip > .35f) Result.Add(H.Anchor);
    return Result;
}
TArray<FString> UTallowHunterMotionComponent::GetContactStates() const
{
    TArray<FString> Result;
    if (!Creature) return Result;
    for (const auto& H : Hands)
        Result.Add(FString::Printf(TEXT("%s plant=%d reach=%d grip=%.3f span=%.2f/%.2f surface=%s loss=%s anchor=%s"),
            *H.Hand.ToString(), H.bPlanted, H.bReaching, H.Grip,
            FVector::Dist(Creature->GetMesh()->GetBoneLocation(H.Upper), H.Target), H.Reach,
            *GetNameSafe(H.SurfaceComponent.IsValid() ? H.SurfaceComponent->GetOwner() : nullptr),
            *H.LastReleaseReason.ToString(), *H.Anchor.ToCompactString()));
    return Result;
}
FString UTallowHunterMotionComponent::GetSkinVertexMaterial(int32 VertexIndex) const
{
    if (!Creature || VertexIndex < 0) return FString();
    const auto* Mesh = Creature->GetMesh();
    const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    const auto* Render = Asset ? Asset->GetResourceForRendering() : nullptr;
    if (!Render || Render->LODRenderData.IsEmpty()) return FString();
    for (const auto& Section : Render->LODRenderData[0].RenderSections)
        if (uint32(VertexIndex) >= Section.BaseVertexIndex
            && uint32(VertexIndex) < Section.BaseVertexIndex + Section.NumVertices)
            return GetNameSafe(Mesh->GetMaterial(Section.MaterialIndex));
    return FString();
}

TArray<FString> UTallowHunterMotionComponent::GetSkinVertexInfluences(int32 VertexIndex) const
{
    TArray<FString> Result;
    if (!Creature || VertexIndex < 0) return Result;
    const auto* Mesh = Creature->GetMesh();
    const auto* Asset = Mesh->GetSkeletalMeshAsset();
    const auto* Render = Asset ? Asset->GetResourceForRendering() : nullptr;
    const auto* Weights = Mesh->GetSkinWeightBuffer(0);
    if (!Render || Render->LODRenderData.IsEmpty() || !Weights || !Weights->GetNeedsCPUAccess()
        || uint32(VertexIndex) >= Weights->GetNumVertices()) return Result;
    const auto& LOD = Render->LODRenderData[0];
    for (const auto& Section : LOD.RenderSections)
    {
        if (uint32(VertexIndex) < Section.BaseVertexIndex
            || uint32(VertexIndex) >= Section.BaseVertexIndex + Section.NumVertices) continue;
        uint32 Offset = 0, Count = 0;
        Weights->GetVertexInfluenceOffsetCount(VertexIndex, Offset, Count);
        for (uint32 I = 0; I < Count; ++I)
        {
            const uint16 Weight = Weights->GetBoneWeight(VertexIndex, I);
            const uint32 Bone = Weights->GetBoneIndex(VertexIndex, I);
            if (!Weight || !Section.BoneMap.IsValidIndex(Bone)) continue;
            Result.Add(FString::Printf(TEXT("%s:%.6f"),
                *Asset->GetRefSkeleton().GetBoneName(Section.BoneMap[Bone]).ToString(), float(Weight) / 65535.f));
        }
        break;
    }
    return Result;
}
TArray<FVector> UTallowHunterMotionComponent::GetSkinnedSurfaceWorld(int32 VertexStride) const
{
    TArray<FVector> Result;
    if (!Creature) return Result;
    const auto* Mesh = Creature->GetMesh();
    // GetCPUSkinnedVertices asserts when no render mesh exists, e.g. a NullRHI test.
    if (!Mesh || !Mesh->GetMeshObject() || !Mesh->GetSkeletalMeshAsset()) return Result;
    // Slate/Python review calls do not inherit the world's animation mem-stack
    // mark. CPU skinning can synchronously refresh the anim instance and pose.
    FMemMark AnimationMemory(FMemStack::Get());
    TArray<FFinalSkinVertex> Vertices;
    Mesh->GetCPUSkinnedVertices(Vertices, 0);
    const int32 Step = FMath::Max(1, VertexStride);
    Result.Reserve((Vertices.Num() + Step - 1) / Step);
    const FTransform World = Mesh->GetComponentTransform();
    for (int32 I = 0; I < Vertices.Num(); I += Step)
        Result.Add(World.TransformPosition(FVector(Vertices[I].Position)));
    return Result;
}
void UTallowHunterMotionComponent::BeginPeek(const FVector& Focus, float Seconds)
{
    PeekFocus = Focus; PeekUntil = GetWorld()->GetTimeSeconds() + Seconds;
    const FVector To = (Focus - Creature->GetActorLocation()).GetSafeNormal2D();
    PeekSide = FVector::DotProduct(To, Creature->GetActorRightVector()) >= 0.f ? 1.f : -1.f;
}

void UTallowHunterMotionComponent::LookAt(const FVector& Point, float Duration)
{
    if (GetWorld()->GetTimeSeconds() < GazeLockUntil) return;
    LookTarget = Point; LookUntil = GetWorld()->GetTimeSeconds() + Duration;
}
void UTallowHunterMotionComponent::UpdateCapture(float DeltaSeconds)
{
    // 25 Oct: the jumpscare stopped firing with the new model. The old capture clips belong to the
    // V11 skeleton and V13 refuses them, so the grab is procedural: every hand closes in a ring round
    // the player's face, the gaze locks on, and on the strike the mask opens and the maw works. The
    // open-head model replaces the HOOD_OPEN morph at the same moment once it exists.
    const float Now = GetWorld()->GetTimeSeconds();
    if (CaptureStartedAt < 0.f) CaptureStartedAt = Now;
    const float T = Now - CaptureStartedAt;
    auto* Mesh = Creature->GetMesh();
    const APawn* Victim = UGameplayStatics::GetPlayerPawn(this, 0);
    const FVector Head = Mesh->GetBoneLocation(TEXT("head"));
    const FVector Face = Victim ? Victim->GetPawnViewLocation() : Head + Creature->GetActorForwardVector() * 100.f;
    LockGaze(Face, .5f);
    const FVector ToFace = (Face - Head).GetSafeNormal();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, ToFace).GetSafeNormal();
    const float Close = FMath::SmoothStep(0.f, .6f, T);
    for (int32 I = 0; I < Hands.Num(); ++I)
    {
        auto& H = Hands[I];
        H.bPlanted = H.bReaching = H.bFolding = false; H.bAttackReach = true; H.Grip = 0.f;
        // Top pair beside the head, the others framing the face from below and the sides.
        const int32 Row = I / 2; const float Side = I % 2 == 0 ? -1.f : 1.f;
        const FVector Ring = Face - ToFace * FMath::Lerp(75.f, 26.f, Close)
            + Right * Side * FMath::Lerp(60.f, 28.f + 8.f * Row, Close) + FVector::UpVector * (24.f - 22.f * Row);
        const FVector Shoulder = Mesh->GetBoneLocation(H.Upper);
        const FVector Goal = Shoulder + (Ring - Shoulder).GetClampedToMaxSize(H.Reach * .95f);
        if (H.Target.IsNearlyZero()) H.Target = Mesh->GetBoneLocation(H.Hand);
        H.Target = FMath::VInterpTo(H.Target, Goal, DeltaSeconds, 9.f);
        H.Pole = Shoulder + Right * Side * 60.f - FVector::UpVector * 20.f;
        H.Weight = FMath::FInterpTo(H.Weight, 1.f, DeltaSeconds, 10.f);
    }
    const float Open = FMath::SmoothStep(.45f, .75f, T);
    Mesh->SetMorphTarget(TEXT("HOOD_OPEN"), Open);
    Mesh->SetMorphTarget(TEXT("CHOMP"), Open * (.5f + .5f * FMath::Sin(T * 22.f)));
    if (Open > .05f && !bCaptureCrackPlayed) bCaptureCrackPlayed = true;
}
void UTallowHunterMotionComponent::LockGaze(const FVector& Point, float Duration)
{
    const float Now = GetWorld()->GetTimeSeconds();
    LookTarget = Point; LookUntil = GazeLockUntil = Now + Duration;
}
bool UTallowHunterMotionComponent::IsGazeLocked() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < GazeLockUntil;
}
void UTallowHunterMotionComponent::SetReachFocus(const FVector& Point, float Duration)
{ ReachFocus = Point; ReachUntil = GetWorld()->GetTimeSeconds() + FMath::Max(Duration, .05f); LookAt(Point, Duration); }
bool UTallowHunterMotionComponent::HasBraceAgainstActor(AActor* Actor) const
{
    if (!Actor) return false;
    for (const auto& H : Hands) if (H.bPlanted && H.Grip > .65f && H.SurfaceComponent.IsValid()
        && H.SurfaceComponent->GetOwner() == Actor
        && H.SurfaceComponent->IsCollisionEnabled()
        && H.SurfaceComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block) return true;
    return false;
}
int32 UTallowHunterMotionComponent::CountContactsOnSurface(const FVector& Normal) const
{
    if (bUsingMim && Creature && Creature->GetMesh())
    {
        const auto* Mesh = Creature->GetMesh();
        const auto* Mim = Cast<UTallowMimAnimInstance>(Mesh->GetAnimInstance());
        if (!Mim) return 0;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MimPhysicalGrip), true, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
        Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
        Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
        TArray<AActor*> Attached; Creature->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
        const FVector N = Normal.GetSafeNormal();
        const auto* Traversal = Creature->HunterTraversal.Get();
        const bool bCorner = Traversal && Traversal->IsCornerBracing();
        const bool bDirectCeiling = Traversal && Traversal->GetRouteKind() == TEXT("DirectCeiling");
        int32 Count = 0;
        for (int32 I = 0; I < 10; ++I)
        {
            if (!Mim->ContactWeights.IsValidIndex(I) || Mim->ContactWeights[I] < .65f
                || !Mim->ContactPinned.IsValidIndex(I) || !Mim->ContactPinned[I]
                || !Mim->ContactAnchorsWorld.IsValidIndex(I)) continue;
            const FName Hand(*FString::Printf(TEXT("arm%02d_hand_%s"), I / 2 + 1, I % 2 == 0 ? TEXT("l") : TEXT("r")));
            if (Mesh->GetBoneIndex(Hand) == INDEX_NONE) continue;
            const FVector Wrist = Mesh->GetBoneLocation(Hand);
            if (FVector::Dist(Wrist, Mim->ContactAnchorsWorld[I]) > 14.f) continue;
            const FVector GripNormal = bCorner && Mim->ContactNormalsWorld.IsValidIndex(I)
                ? Mim->ContactNormalsWorld[I].GetSafeNormal() : N;
            if (FVector::DotProduct(GripNormal, N) < .65f) continue;
            const float StandOff = bCorner ? Traversal->GetCornerWristStandOff(I)
                : bDirectCeiling ? Traversal->GetDirectCeilingWristStandOff(I) : 20.f;
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit, Wrist + GripNormal * 12.f,
                Wrist - GripNormal * (bCorner || bDirectCeiling ? StandOff + 12.f : 32.f),
                ECC_Visibility, Query) && FVector::DotProduct(Hit.ImpactNormal, GripNormal) > .7f
                && FVector::Dist(Wrist, Hit.ImpactPoint) < (bCorner || bDirectCeiling ? StandOff + 4.f : 24.f)) ++Count;
        }
        return Count;
    }
    int32 Count = 0;
    for (const auto& H : Hands) if (H.bPlanted && H.Grip > .45f && FVector::DotProduct(H.Normal, Normal) > .7f) ++Count;
    return Count;
}

float UTallowHunterMotionComponent::GetSupportStrength() const
{
    if (bUsingMim && Creature)
        return FMath::Clamp(CountContactsOnSurface(Creature->GetActorUpVector()) / 4.f, 0.f, 1.f);
    return SupportStrength;
}
FVector UTallowHunterMotionComponent::GetEyeLocation() const
{
    if (!Creature) return FVector::ZeroVector;
    if (bProxy)
        return Creature->GetActorTransform().TransformPositionNoScale(FVector(FitOffset.X + FitSize.X * .5f - 12.f, FitOffset.Y,
            -Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + FitLift + FitSize.Z * .7f));
    const auto* Mesh = Creature->GetMesh();
    return Mesh && Mesh->GetBoneIndex(HeadBone) != INDEX_NONE ? Mesh->GetBoneLocation(HeadBone) + LookDirection * 16.f
        : Creature->GetActorLocation() + Creature->GetActorUpVector() * 65.f;
}

FQuat UTallowHunterMotionComponent::ContactRotation(const FTallowHunterHand& H, const FVector& Normal) const
{
    // The sculpt has long, curled fingers. Flattening the palm onto the floor
    // placed the wrist almost at floor height and made the front arms unreachable.
    // Point the measured claw tips into the surface with a slight forward rake.
    const FQuat Current = H.Weight > .5f ? H.Rotation : Creature->GetMesh()->GetBoneQuaternion(H.Hand);
    FVector Tangent = FVector::VectorPlaneProject(Creature->GetActorForwardVector(), Normal).GetSafeNormal();
    if (Tangent.IsNearlyZero())
        Tangent = FVector::VectorPlaneProject(Creature->GetActorUpVector(), Normal).GetSafeNormal();
    // Fingers lie along the surface, pads down, instead of the hand balancing on its claw tips.
    const FVector TipDirection = (-Normal * .30f + Tangent * .95f).GetSafeNormal();
    const FVector CurrentTip = Current.RotateVector(H.TipLocal).GetSafeNormal();
    return CurrentTip.IsNearlyZero() ? Current
        : (FQuat::FindBetweenNormals(CurrentTip, TipDirection) * Current).GetNormalized();
}

FVector UTallowHunterMotionComponent::FloorContactGoal(int32 Index, float Spread) const
{
    const int32 Row = Index / 2;
    const float Sign = Index % 2 == 0 ? -1.f : 1.f;
    const float Ahead = Row == 1 ? 64.f : Row == 0 && LowAlpha > .6f ? 82.f : -16.f;
    const float Width = (Row == 1 ? 54.f : Row == 0 && LowAlpha > .6f ? 66.f : 64.f) * FMath::Lerp(1.f, .37f, SqueezeAmount);
    const FVector Up = Creature->GetActorUpVector();
    const FVector Prediction = FVector::VectorPlaneProject(
        SmoothedVelocity * .40f + SmoothedAcceleration * .045f, Up).GetClampedToMaxSize(96.f);
    FVector Goal = Creature->GetMesh()->GetComponentLocation()
        + Creature->GetActorForwardVector() * Ahead
        + Creature->GetActorRightVector() * Sign * Width * Spread + Prediction;
    const auto& H = Hands[Index];
    if (!H.TipLocal.IsNearlyZero())
    {
        // Predict the body at landing, then keep that candidate inside the
        // current finite reach. A frozen target behind an accelerating body was
        // already exhausted by the time the rear hand touched down.
        const FVector Shoulder = Creature->GetMesh()->GetBoneLocation(H.Upper);
        const FVector Wrist = Goal - ContactRotation(H, Up).RotateVector(H.TipLocal);
        const FVector Delta = Wrist - Shoulder;
        const float Height = FVector::DotProduct(Delta, Up);
        const float Radius = FMath::Sqrt(FMath::Max(0.f, FMath::Square(H.Reach * .92f) - Height * Height));
        const FVector Planar = FVector::VectorPlaneProject(Delta, Up);
        Goal += Planar.GetClampedToMaxSize(Radius) - Planar;
    }
    if (bNarrowPortal)
    {
        const float Side = FVector::DotProduct(Goal - PortalCenter, PortalRight);
        const float SafeHalf = FMath::Max(18.f, PortalHalfWidth - 21.f);
        Goal += PortalRight * (FMath::Clamp(Side, -SafeHalf, SafeHalf) - Side);
    }
    return Goal;
}

bool UTallowHunterMotionComponent::ProbeHand(int32 Index, FVector& Point, FVector& Normal,
    bool& bWall, UPrimitiveComponent*& Surface) const
{
    const auto& H = Hands[Index];
    const FVector Up = Creature->GetActorUpVector(), Forward = Creature->GetActorForwardVector();
    const FVector Right = Creature->GetActorRightVector();
    const float Sign = Index % 2 == 0 ? -1.f : 1.f;
    const int32 Row = Index / 2;
    const FVector Shoulder = Creature->GetMesh()->GetBoneLocation(H.Upper);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterHands), true, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    FHitResult Hit;
    auto Accept = [&](const FHitResult& Candidate) {
        const auto* ContactSurface = Candidate.GetComponent();
        if (!Candidate.bBlockingHit || !ContactSurface || !ContactSurface->IsCollisionEnabled()
            || ContactSurface->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block) return false;
        const FVector Contact = Candidate.ImpactPoint + Candidate.ImpactNormal * 1.4f;
        const FVector Wrist = Contact - ContactRotation(H, Candidate.ImpactNormal).RotateVector(H.TipLocal);
        if (FVector::Dist(Shoulder, Wrist) > H.Reach * .95f) return false;
        Point = Contact; Normal = Candidate.ImpactNormal; bWall = FVector::DotProduct(Normal, Up) < .5f;
        Surface = Candidate.GetComponent(); return Surface != nullptr;
    };
    const auto* Traversal = Creature->HunterTraversal.Get();
    FVector Upcoming, UpcomingNormal;
    if (Traversal && Traversal->GetApproachContact(Index, Shoulder, Upcoming, UpcomingNormal))
        if (GetWorld()->LineTraceSingleByChannel(Hit, Upcoming + UpcomingNormal * 8.f,
            Upcoming - UpcomingNormal * 8.f, ECC_Visibility, Query) && Accept(Hit)) return true;
    if (Row == 0 && !(LowAlpha > .6f && SqueezeAmount < .2f && AttackAlpha < .02f))
    {
        if (GetWorld()->GetTimeSeconds() < ReachUntil)
        {
            const FVector Towards = (ReachFocus - Shoulder).GetSafeNormal();
            if (GetWorld()->LineTraceSingleByChannel(Hit, Shoulder, Shoulder + Towards * (H.Reach + 24.f),
                ECC_Visibility, Query) && Accept(Hit)) return true;
        }
        // Catch the near jamb before the shoulders enter it. Once the shoulder
        // crosses that plane, the free hand folds rather than reaching through it.
        if (bNarrowPortal && FVector::DotProduct(Shoulder - PortalCenter, PortalForward) < -14.f)
        {
            const float Height = FVector::DotProduct(Shoulder - PortalCenter, Up) - 12.f;
            const FVector Jamb = PortalCenter + Right * Sign * (PortalHalfWidth + 7.f)
                + Up * FMath::Clamp(Height, 105.f, 165.f);
            if (GetWorld()->LineTraceSingleByChannel(Hit, Jamb - PortalForward * 85.f,
                Jamb + PortalForward * 40.f, ECC_Visibility, Query) && Accept(Hit)) return true;
        }
        if (SqueezeAmount > .2f) return false;
        const FVector End = Shoulder + Right * Sign * (H.Reach * .95f) + Forward * 30.f;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Shoulder, End, ECC_Visibility, Query)
            && FVector::DotProduct(Hit.ImpactNormal, Up) < .5f && Accept(Hit)) return true;
        return false;
    }
    for (const float Spread : {1.f, .82f, .64f})
    {
        const FVector Desired = FloorContactGoal(Index, Spread);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Desired + Up * 110.f, Desired - Up * 90.f,
            ECC_Visibility, Query) && FVector::DotProduct(Hit.ImpactNormal, Up) > .55f && Accept(Hit)) return true;
        // A jamb becomes an intentional brace, never a target behind the near wall.
        if (GetWorld()->LineTraceSingleByChannel(Hit, Shoulder, Desired, ECC_Visibility, Query) && Accept(Hit)) return true;
    }
    return false;
}

void UTallowHunterMotionComponent::BeginReach(int32 Index, const FVector& Point,
    const FVector& Normal, bool bWall, UPrimitiveComponent* Surface, float Speed)
{
    auto& H = Hands[Index]; const auto* Mesh = Creature->GetMesh();
    H.SwingFrom = H.Weight > .5f ? H.Target : Mesh->GetBoneLocation(H.Hand);
    H.SwingRotation = H.Weight > .5f ? H.Rotation : Mesh->GetBoneQuaternion(H.Hand);
    H.LandRotation = ContactRotation(H, Normal);
    H.SwingTo = Point - H.LandRotation.RotateVector(H.TipLocal);
    H.SwingNormal = Normal; H.Anchor = Point; H.SurfaceComponent = Surface;
    H.ContactLocal = Surface ? Surface->GetComponentTransform().InverseTransformPosition(Point) : Point;
    H.NormalLocal = Surface ? Surface->GetComponentTransform().InverseTransformVectorNoScale(Normal) : Normal;
    H.ContactRotationLocal = Surface ? (Surface->GetComponentQuat().Inverse() * H.LandRotation).GetNormalized()
        : H.LandRotation;
    H.SwingStarted = GetWorld()->GetTimeSeconds();
    const float Distance = FVector::Dist(H.SwingFrom, H.SwingTo);
    H.SwingDuration = FMath::Clamp(.18f + Distance / 800.f - Speed / 2800.f, .2f, .34f);
    H.bReaching = true; H.bAttackReach = H.bPlanted = H.bFolding = false; H.bWall = bWall; H.Grip = 0.f;
    NextPlant = H.SwingStarted + FMath::Clamp(.24f - Speed / 1600.f, .1f, .24f); DistanceSincePlant = 0.f;
}

float UTallowHunterMotionComponent::EvaluateSupport(int32 ExcludedHand, FVector& Correction, int32& Count) const
{
    Correction = FVector::ZeroVector; Count = 0;
    const FVector Forward = Creature->GetActorForwardVector(), Right = Creature->GetActorRightVector();
    const FVector Up = Creature->GetActorUpVector(), Origin = Creature->GetActorLocation();
    TArray<FVector2D> Points;
    for (int32 I = 0; I < Hands.Num(); ++I)
    {
        const auto& H = Hands[I];
        if (I == ExcludedHand || !H.bPlanted || H.Grip < .35f || FVector::DotProduct(H.Normal, Up) < .45f) continue;
        const FVector P = H.Anchor - Origin;
        Points.Add(FVector2D(FVector::DotProduct(P, Forward), FVector::DotProduct(P, Right)));
    }
    Count = Points.Num();
    // Kinematic mass estimate and support projection, not a physical force/torque solve.
    const FVector Predicted = EstimatedMassCenter + SmoothedVelocity * .10f
        + SmoothedAcceleration.GetClampedToMaxSize(400.f) * .012f - Origin;
    const FVector2D Mass(FVector::DotProduct(Predicted, Forward), FVector::DotProduct(Predicted, Right));
    if (Count == 0) return -100.f;
    Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
    TArray<FVector2D> Hull;
    for (const auto& P : Points)
    {
        while (Hull.Num() >= 2 && Cross2(Hull.Last() - Hull[Hull.Num() - 2], P - Hull.Last()) <= 0.f) Hull.Pop();
        Hull.Add(P);
    }
    const int32 LowerCount = Hull.Num();
    for (int32 I = Points.Num() - 2; I >= 0; --I)
    {
        const auto& P = Points[I];
        while (Hull.Num() > LowerCount && Cross2(Hull.Last() - Hull[Hull.Num() - 2], P - Hull.Last()) <= 0.f) Hull.Pop();
        Hull.Add(P);
    }
    if (Hull.Num() > 1) Hull.Pop();
    FVector2D Nearest = Hull[0], Center = FVector2D::ZeroVector;
    float Distance = TNumericLimits<float>::Max(); bool Inside = Hull.Num() >= 3;
    for (int32 I = 0; I < Hull.Num(); ++I)
    {
        const FVector2D A = Hull[I], B = Hull[(I + 1) % Hull.Num()], Edge = B - A;
        Center += A;
        const float T = FMath::Clamp(FVector2D::DotProduct(Mass - A, Edge) / FMath::Max(Edge.SizeSquared(), UE_SMALL_NUMBER), 0., 1.);
        const FVector2D Closest = A + Edge * T; const float D = FVector2D::Distance(Mass, Closest);
        if (D < Distance) { Distance = D; Nearest = Closest; }
        if (Cross2(Edge, Mass - A) < 0.f) Inside = false;
    }
    Center /= Hull.Num();
    if (!Inside || Distance < 7.f)
    {
        const FVector2D Goal = !Inside ? Nearest + (Center - Nearest).GetSafeNormal() * 7.f : Center;
        const FVector2D Shift = (Goal - Mass).GetClampedToMaxSize(18.f);
        Correction = Forward * Shift.X + Right * Shift.Y;
    }
    return Inside ? Distance : -Distance;
}

void UTallowHunterMotionComponent::UpdateHands(float DeltaSeconds, float Speed)
{
    const float Now = GetWorld()->GetTimeSeconds(); const FVector Up = Creature->GetActorUpVector();
    auto* Mesh = Creature->GetMesh(); auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
    AttackAlpha = Hunter ? Hunter->GetAttackWindupAlpha() : 0.f; ReachingHands = 0;
    for (int32 I = 0; I < Hands.Num(); ++I)
    {
        auto& H = Hands[I]; const FVector Wrist = Mesh->GetBoneLocation(H.Hand);
        const FQuat WristRotation = Mesh->GetBoneQuaternion(H.Hand);
        if (H.TipLocal.IsNearlyZero())
        {
            const FString Prefix = H.Hand.ToString().LeftChop(4); FVector Tips = FVector::ZeroVector;
            for (int32 Digit = 2; Digit <= 4; ++Digit)
            {
                const FVector B = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("%sdigit%d_b"), *Prefix, Digit)));
                const FVector C = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("%sdigit%d_c"), *Prefix, Digit)));
                Tips += B + (C - B) * .35f;
            }
            H.TipLocal = WristRotation.UnrotateVector(Tips / 3.f - Wrist);
            const FVector D2 = Mesh->GetSocketLocation(FName(*(Prefix + TEXT("digit2_a"))));
            const FVector D4 = Mesh->GetSocketLocation(FName(*(Prefix + TEXT("digit4_a"))));
            FVector Palm = FVector::CrossProduct(D4 - D2, Tips / 3.f - Wrist).GetSafeNormal();
            if (Palm.IsNearlyZero()) Palm = Up;
            H.PalmLocal = WristRotation.UnrotateVector(Palm); H.Rotation = WristRotation; H.Target = Wrist;
        }
        if (H.ElbowPole.IsNearlyZero()) H.ElbowPole = Mesh->GetBoneLocation(H.Lower);
        H.Pole = H.ElbowPole;
        if (H.bPlanted)
        {
            if (H.SurfaceComponent.IsValid())
            {
                H.Anchor = H.SurfaceComponent->GetComponentTransform().TransformPosition(H.ContactLocal);
                H.Normal = H.SurfaceComponent->GetComponentTransform().TransformVectorNoScale(H.NormalLocal).GetSafeNormal();
                H.Rotation = (H.SurfaceComponent->GetComponentQuat() * H.ContactRotationLocal).GetNormalized();
            }
            H.Target = H.Anchor - H.Rotation.RotateVector(H.TipLocal);
            const bool PastJamb = I < 2 && bNarrowPortal
                && FVector::DotProduct(Mesh->GetBoneLocation(H.Upper) - PortalCenter, PortalForward) > -12.f
                && FVector::DotProduct(H.Normal, PortalForward) < -.6f;
            const bool bSatUp = I < 2 && !H.bWall && LowAlpha < .35f;
            const bool bClimbingNow = Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing();
            // The hem and the rear pair trail; they never drive it forward ("pulling itself by the bottom of its
            // legs doesn't make sense for its gravity").
            const bool bTrail = I >= 4 && !H.bWall && !bClimbingNow && Speed > 60.f;
            if (bSatUp) { H.LastReleaseReason = TEXT("SatUp"); H.bPlanted = false; H.Grip = 0.f; }
            else if (bTrail) { H.LastReleaseReason = TEXT("Trail"); H.bPlanted = false; H.Grip = 0.f; }
            else if (PastJamb || !H.SurfaceComponent.IsValid()
                // Hysteresis: probes accept 95% of reach, so releasing just past 101% made a still
                // creature land and drop the same hand four times a second.
                || FVector::Dist(Mesh->GetBoneLocation(H.Upper), H.Target) > H.Reach * (Now - H.LandedAt < .4f ? 1.15f : 1.08f))
            {
                H.LastReleaseReason = PastJamb ? TEXT("PastJamb")
                    : !H.SurfaceComponent.IsValid() ? TEXT("SurfaceLost") : TEXT("ReachExhausted");
                H.bPlanted = false; H.Grip = 0.f;
            }
            else H.Grip = FMath::Min(1.f, (Now - H.LandedAt) / .14f);
        }
        if (I < 2 && AttackAlpha > .02f && !(Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing()))
        {
            H.bPlanted = H.bReaching = false; H.bAttackReach = true; H.Grip = 0.f;
            const FVector Shoulder = Mesh->GetBoneLocation(H.Upper), Direction = (LookTarget - Shoulder).GetSafeNormal();
            const FVector Goal = Shoulder + Direction * (H.Reach * FMath::Lerp(.56f, .93f, Ease(AttackAlpha)))
                + Creature->GetActorRightVector() * (I == 0 ? -12.f : 12.f);
            H.Target = FMath::VInterpTo(H.Target, Goal, DeltaSeconds, 7.f);
            H.Weight = FMath::FInterpTo(H.Weight, 1.f, DeltaSeconds, 8.f); continue;
        }
        H.bAttackReach = H.bFolding = false;
        if (H.bReaching)
        {
            ++ReachingHands;
            const float T = FMath::Clamp((Now - H.SwingStarted) / H.SwingDuration, 0.f, 1.f);
            // The selected landing point stays fixed throughout the reach.
            H.Rotation = FQuat::Slerp(H.SwingRotation, H.LandRotation, Ease(T)).GetNormalized();
            const float Lift = I < 2 ? 12.f : FMath::Lerp(17.f, 27.f, FMath::Clamp(Speed / 220.f, 0.f, 1.f));
            H.Target = FMath::Lerp(H.SwingFrom, H.SwingTo, Ease(T))
                + (Up + H.SwingNormal).GetSafeNormal() * (FMath::Sin(T * PI) * Lift);
            if (T >= 1.f)
            {
                H.bReaching = false; H.bPlanted = H.SurfaceComponent.IsValid();
                H.Target = H.SwingTo; H.Normal = H.SwingNormal; H.Rotation = H.LandRotation;
                H.LandedAt = Now; ++TotalPlants;
                if (H.bPlanted) PlayPlant(H.Target, H.bWall, Speed);
            }
        }
        if (I < 2 && !H.bPlanted && !H.bReaching && SqueezeAmount <= .12f && LowAlpha < .5f)
        {
            // Tall: the front arms hang down at its sides, long and heavy, and swing a little against the stride.
            const float Side = I % 2 == 0 ? -1.f : 1.f;
            const FVector Shoulder = Mesh->GetBoneLocation(H.Upper);
            const float Swing = FMath::Sin(GetWorld()->GetTimeSeconds() * FMath::Lerp(1.1f, 3.2f, FMath::Clamp(Speed / 400.f, 0.f, 1.f)) + Side * 1.6f)
                * FMath::Lerp(4.f, 22.f, FMath::Clamp(Speed / 400.f, 0.f, 1.f));
            const FVector Goal = Shoulder - Up * H.Reach * .86f + Creature->GetActorForwardVector() * (8.f + Swing)
                + Creature->GetActorRightVector() * Side * 14.f;
            H.Target = FMath::VInterpTo(H.Target, Goal, DeltaSeconds, 8.f);
            H.Rotation = FQuat::Slerp(H.Rotation, ContactRotation(H, -Creature->GetActorRightVector() * Side),
                FMath::Clamp(DeltaSeconds * 6.f, 0.f, 1.f)).GetNormalized();
            H.bFolding = true;
        }
        else if (I < 2 && !H.bPlanted && !H.bReaching && SqueezeAmount > .12f)
        {
            // An unplanted arm still has to pass through the doorway. This is a
            // free folded pose and never contributes a fabricated support point.
            const float Side = I % 2 == 0 ? -1.f : 1.f;
            const FVector Shoulder = Mesh->GetBoneLocation(H.Upper);
            FVector Goal = Shoulder + Creature->GetActorForwardVector() * 24.f - Up * 38.f
                - Creature->GetActorRightVector() * Side * 22.f;
            if (bNarrowPortal)
            {
                const float Lateral = FVector::DotProduct(Goal - PortalCenter, PortalRight);
                const float SafeHalf = FMath::Max(12.f, PortalHalfWidth - 27.f);
                Goal += PortalRight * (FMath::Clamp(Lateral, -SafeHalf, SafeHalf) - Lateral);
            }
            H.Target = FMath::VInterpTo(H.Target, Goal, DeltaSeconds, 10.f);
            H.Rotation = FQuat::Slerp(H.Rotation, ContactRotation(H, Up),
                FMath::Clamp(DeltaSeconds * 9.f, 0.f, 1.f)).GetNormalized();
            H.bFolding = true;
        }
        else if (!H.bPlanted && !H.bReaching && !H.bAttackReach)
        {
            // Never left hanging in the clip's pose ("its arms are always glitching halfway up in the air"). A hand
            // is planted, swinging to a plant, or tucked in close: front hands ready under the chest, the middle pair
            // under the body, the rear pair trailing behind with the hem.
            const float Side = I % 2 == 0 ? -1.f : 1.f;
            const FVector Shoulder = Mesh->GetBoneLocation(H.Upper);
            const FVector F = Creature->GetActorForwardVector(), R = Creature->GetActorRightVector();
            const FVector Goal = I < 2 ? Shoulder + F * 30.f - Up * H.Reach * .62f + R * Side * 16.f
                : I < 4 ? Shoulder + F * 6.f - Up * H.Reach * .70f + R * Side * 20.f
                : Shoulder - F * 28.f - Up * H.Reach * .68f + R * Side * 22.f;
            H.Target = FMath::VInterpTo(H.Target, Goal, DeltaSeconds, 9.f);
            H.Rotation = FQuat::Slerp(H.Rotation, ContactRotation(H, Up), FMath::Clamp(DeltaSeconds * 7.f, 0.f, 1.f)).GetNormalized();
            H.bFolding = true;
        }
        const float WantWeight = H.bPlanted || H.bReaching || H.bFolding ? 1.f : 0.f;
        H.Weight = FMath::FInterpTo(H.Weight, WantWeight, DeltaSeconds, WantWeight > H.Weight ? 9.f : 4.f);
    }
    for (int32 I = 0; I < Hands.Num(); ++I)
    {
        auto& H = Hands[I];
        const FVector Shoulder = Mesh->GetBoneLocation(H.Upper);
        const FVector To = H.Target - Shoulder;
        const FVector Direction = To.GetSafeNormal();
        if (Direction.IsNearlyZero()) continue;
        const float Side = I % 2 == 0 ? -1.f : 1.f;
        FVector Bend = H.bWall && H.bPlanted ? H.Normal * .9f + Creature->GetActorRightVector() * Side * .45f
            : Up * .75f + Creature->GetActorRightVector() * Side * .75f + Creature->GetActorForwardVector() * (I < 2 ? .25f : -.15f);
        Bend = FVector::VectorPlaneProject(Bend, Direction).GetSafeNormal();
        if (Bend.IsNearlyZero()) continue;
        const FVector Stable = (Shoulder + H.Target) * .5f + Bend * FMath::Min(70.f, To.Size() * .55f + 20.f);
        // A short blend stops a sudden swing of the elbow when the target moves quickly.
        FVector Wanted = Stable;
        if (SqueezeAmount > .01f)
        {
            const FVector Folded = Shoulder + Up * (I < 2 ? 12.f : -12.f)
                - Creature->GetActorRightVector() * Side * 56.f
                + Creature->GetActorForwardVector() * (I < 2 ? 65.f : 35.f);
            Wanted = FMath::Lerp(Stable, Folded, FMath::Clamp(SqueezeAmount * 1.4f, 0.f, 1.f));
        }
        H.ElbowPole = FMath::VInterpTo(H.ElbowPole, Wanted, DeltaSeconds, 14.f);
        H.Pole = H.ElbowPole;
    }
    PendingSupportShift = FVector::ZeroVector;
    int32 Best = INDEX_NONE; float BestScore = 0.f; FVector BestPoint, BestNormal;
    bool BestWall = false; UPrimitiveComponent* BestSurface = nullptr;
    const int32 MaxReaches = SupportingHands < 2 || (Speed > 105.f && SupportingHands >= 3) ? 2 : 1;
    if (Now >= NextPlant && ReachingHands < MaxReaches)
    {
        // Stagger contact changes by extension and remaining support, not a timer-only gait.
        const int32 Order[] = {2, 5, 3, 4, 0, 1};
        for (int32 J = 0; J < 6; ++J)
        {
            const int32 I = Order[(J + GaitIndex) % 6]; auto& H = Hands[I];
            const bool bClimbing = Creature->HunterTraversal && Creature->HunterTraversal->IsTraversing();
            if (H.bReaching || H.bAttackReach || (H.bPlanted && Now - H.LandedAt < (bClimbing ? .18f : FMath::Lerp(.45f, .22f, FMath::Clamp(Speed / 300.f, 0.f, 1.f))))) continue;
            // Sat up, the front arms are free: planting them only to release them next frame was the main twitch.
            if (I < 2 && LowAlpha < .5f && !bClimbing) continue;
            if (I >= 4 && !bClimbing && Speed > 40.f) continue;      // the rear pair trails while it moves
            // Standing still, an arm that is already down stays down unless it is overstretched.
            // (Not while climbing: a wall transfer starts standing still and needs its hands to leave the floor.)
            if (!bClimbing && H.bPlanted && Speed < 25.f && FVector::Dist(Mesh->GetBoneLocation(H.Upper), H.Target) < H.Reach * 1.02f) continue;
            FVector Point, Normal; bool Wall = false; UPrimitiveComponent* Surface = nullptr;
            if (!ProbeHand(I, Point, Normal, Wall, Surface)) continue;
            const float Stretch = FVector::Dist(Mesh->GetBoneLocation(H.Upper), H.Target) / H.Reach;
            const float PredictedStretch = FVector::Dist(Mesh->GetBoneLocation(H.Upper)
                + SmoothedVelocity * .20f, H.Target) / H.Reach;
            float Score = H.bPlanted ? FVector::Dist(Point, H.Anchor) - (I < 2 ? 35.f : 26.f) : 70.f;
            if (H.bPlanted && Stretch > .86f) Score += (Stretch - .86f) * 500.f;
            if (H.bPlanted && PredictedStretch > .88f) Score += (PredictedStretch - .88f) * 450.f;
            if (H.bPlanted && FVector::DotProduct(H.Normal, Normal) < .7f) Score += 80.f;
            if (Score <= BestScore) continue;
            FVector Shift; int32 Remaining; const float Margin = EvaluateSupport(I, Shift, Remaining);
            const bool NewSurface = FVector::DotProduct(Normal, Up) < .45f;
            if (H.bPlanted && !NewSurface && Remaining >= 2 && Margin < -10.f)
            { PendingSupportShift = Shift; ++DeferredLifts; continue; }
            if (H.bPlanted && !NewSurface && Remaining < 2 && SupportingHands >= 2) continue;
            Best = I; BestScore = Score; BestPoint = Point; BestNormal = Normal; BestWall = Wall; BestSurface = Surface;
        }
    }
    if (Best != INDEX_NONE)
    { BeginReach(Best, BestPoint, BestNormal, BestWall, BestSurface, Speed); GaitIndex = (GaitIndex + 1) % 6; }
    PlantedHands = WallBraces = 0; float Grip = 0.f;
    for (const auto& H : Hands) if (H.bPlanted) { ++PlantedHands; if (H.bWall) ++WallBraces; Grip += H.Grip; }
    FVector Correction;
    SupportMargin = EvaluateSupport(INDEX_NONE, Correction, SupportingHands);
    SupportStrength = FMath::Clamp(Grip / 3.f, 0.f, 1.f);
    if (PendingSupportShift.IsNearlyZero()) PendingSupportShift = Correction;
}

void UTallowHunterMotionComponent::UpdatePosture(float DeltaSeconds, float Speed)
{
    const FVector Up = Creature->GetActorUpVector(), Forward = Creature->GetActorForwardVector();
    const FVector Right = Creature->GetActorRightVector(), Location = Creature->GetActorLocation();
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterClearance), true, Creature);
    Query.AddIgnoredActors(Creature->GetCapsuleComponent()->GetMoveIgnoreActors());
    float SurfaceDistance = Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - Up * 200.f, ECC_Visibility, Query)
        && FVector::DotProduct(Hit.ImpactNormal, Up) > .5f) SurfaceDistance = FMath::Clamp(Hit.Distance, 28.f, 150.f);
    Creature->GetMesh()->SetRelativeLocation(FVector(0, 0, -SurfaceDistance));
    const FVector Base = Location - Up * SurfaceDistance, Probe = Base + Forward * 40.f;
    HeadClearance = 1000.f;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Probe + Up * 35.f, Probe + Up * 370.f, ECC_Visibility, Query))
        HeadClearance = FVector::DotProduct(Hit.ImpactPoint - Base, Up);
    DoorwayWidth = 360.f;
    float NarrowDistance = TNumericLimits<float>::Max();
    bNarrowPortal = false;
    // Include the trailing material length. Releasing the fold when the capsule
    // clears the jamb lets a four-metre robe expand while it is still in the wall.
    for (int32 Step = -18; Step <= 10; ++Step)
    {
        const float Ahead = Step * 20.f;
        float Left = 180.f, RightGap = 180.f;
        const FVector Torso = Base + Up * 110.f + Forward * Ahead;
        if (Step != 0 && GetWorld()->LineTraceSingleByChannel(Hit, Base + Up * 110.f,
            Torso, ECC_Visibility, Query)) continue;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Torso, Torso - Right * 180.f, ECC_Visibility, Query)) Left = Hit.Distance;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Torso, Torso + Right * 180.f, ECC_Visibility, Query)) RightGap = Hit.Distance;
        const float Width = Left + RightGap;
        if (Width < DoorwayWidth - .5f || (FMath::Abs(Width - DoorwayWidth) <= .5f && FMath::Abs(Ahead) < NarrowDistance))
        {
            DoorwayWidth = Width; NarrowDistance = FMath::Abs(Ahead);
            PortalCenter = Base + Forward * Ahead + Right * ((RightGap - Left) * .5f);
            PortalRight = Right; PortalForward = Forward; PortalHalfWidth = Width * .5f;
        }
    }
    bNarrowPortal = DoorwayWidth < 185.f;
    const auto* Traversal = Creature->HunterTraversal.Get();
    const float DesiredSqueeze = FMath::Max(Traversal ? Traversal->GetSqueezeAlpha() : 0.f,
        FMath::Max(FMath::Clamp((210.f - HeadClearance) / 100.f, 0.f, 1.f),
            FMath::Clamp((205.f - DoorwayWidth) / 105.f, 0.f, 1.f)));
    SqueezeAmount = FMath::FInterpTo(SqueezeAmount, DesiredSqueeze, DeltaSeconds, 3.8f);
    auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
    const bool Ambush = Hunter && Hunter->HuntMode == ETallowHuntMode::Ambush && Speed < 15.f;
    const float ForwardAcceleration = FVector::DotProduct(SmoothedAcceleration, Forward);
    const float SideAcceleration = FVector::DotProduct(SmoothedAcceleration, Right);
    float FeasibleHeight = 0.f;
    const auto* Mesh = Creature->GetMesh();
    for (int32 I = 2; I < Hands.Num(); ++I)
    {
        const auto& H = Hands[I];
        if (H.TipLocal.IsNearlyZero()) continue;
        const FVector Goal = FloorContactGoal(I, .82f);
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Goal + Up * 110.f, Goal - Up * 90.f,
            ECC_Visibility, Query) || FVector::DotProduct(Hit.ImpactNormal, Up) < .55f) continue;
        const FVector Wrist = Hit.ImpactPoint + Hit.ImpactNormal * 1.4f
            - ContactRotation(H, Hit.ImpactNormal).RotateVector(H.TipLocal);
        const FVector BaseShoulder = Mesh->GetBoneLocation(H.Upper) - BodyTranslation;
        const FVector Difference = BaseShoulder - Wrist;
        const float PlanarSquared = FVector::VectorPlaneProject(Difference, Up).SizeSquared();
        const float ComfortableReach = H.Reach * .90f;
        if (PlanarSquared >= FMath::Square(ComfortableReach)) continue;
        const float AllowedHeight = FMath::Sqrt(FMath::Square(ComfortableReach) - PlanarSquared);
        FeasibleHeight = FMath::Min(FeasibleHeight, AllowedHeight - FVector::DotProduct(Difference, Up));
    }
    ReachHeightCorrection = FMath::Clamp(FeasibleHeight, -42.f, 0.f);
    BodyOffset = FMath::Min(FMath::Clamp(HeadClearance - 215.f, -42.f, 0.f), ReachHeightCorrection)
        - (Ambush ? 7.f : 0.f);
    const float SpeedFrac = FMath::Clamp(Speed / 400.f, 0.f, 1.f);
    const auto HuntNow = Hunter ? Hunter->HuntMode : ETallowHuntMode::Roam;
    // Low and coiled when it stalks, stretched out and lower still when it runs.
    // Two stances. Low and predatory: the front arms become legs, the head lifts to watch.
    // Sat up: front arms free and spread, the tall listening pose. It also rears up now and then.
    const float NowPosture = GetWorld()->GetTimeSeconds();
    // 25 Oct: two modes only, tall walking or low with fully procedural movement, and no arms-up pose (the
    // random rear-up is gone). Tall to wander, listen, look and run; low to search, investigate and lie in wait.
    float LowTarget = (HuntNow == ETallowHuntMode::Investigate || HuntNow == ETallowHuntMode::Search
        || HuntNow == ETallowHuntMode::Ambush) ? 1.f : 0.f;
    if (PostureOverride >= 0.f) LowTarget = PostureOverride;
    // Hold a stance at least 1.4 s: brief Listen/Pursue flips were popping it between sat-up and low.
    else if (LowTarget != HeldLowTarget && NowPosture < PostureHoldUntil) LowTarget = HeldLowTarget;
    if (LowTarget != HeldLowTarget) { HeldLowTarget = LowTarget; PostureHoldUntil = NowPosture + 2.5f; }
    LowAlpha = FMath::FInterpTo(LowAlpha, LowTarget, DeltaSeconds, 1.5f);
    const float WantedDrop = LowAlpha * 12.f + (Rhythm == ERhythm::Pause ? 10.f : 0.f) + (HuntNow == ETallowHuntMode::Ambush ? 16.f
        : HuntNow == ETallowHuntMode::Pursue ? 6.f + SpeedFrac * 8.f
        : HuntNow == ETallowHuntMode::Roam || HuntNow == ETallowHuntMode::Withdraw ? 9.f : 5.f);
    StanceDrop = FMath::FInterpTo(StanceDrop, WantedDrop, DeltaSeconds, 1.6f);
    const float NowTime = GetWorld()->GetTimeSeconds();
    const bool bWantsPeek = NowTime < PeekUntil && Speed < 25.f && !bNarrowPortal;
    PeekAlpha = FMath::FInterpTo(PeekAlpha, bWantsPeek ? 1.f : 0.f, DeltaSeconds, bWantsPeek ? 5.5f : 8.f);
    if (PeekAlpha > .3f) LockGaze(PeekFocus, .3f);
    FVector Target = PendingSupportShift + Up * (BodyOffset - 3.f * (1.f - SupportStrength) - StanceDrop - PeekAlpha * 12.f)
        + Right * PeekSide * PeekAlpha * 46.f + Forward * (PeekAlpha * 16.f + SpeedFrac * 9.f + LowAlpha * 28.f)
        - Forward * FMath::Clamp(ForwardAcceleration * .023f, -8.f, 8.f)
        - Right * FMath::Clamp(SideAcceleration * .018f, -6.f, 6.f);
    if (bNarrowPortal && PeekAlpha < .1f)
    {
        const float Lateral = FVector::DotProduct(Target, PortalRight);
        Target += PortalRight * (FMath::Clamp(Lateral, -3.f, 3.f) - Lateral);
    }
    if (AttackAlpha > 0.f) Target += Forward * (Ease(AttackAlpha) * 13.f - FMath::Sin(AttackAlpha * PI) * 5.f);
    if (Traversal && Traversal->IsTransferring())
    {
        FVector NewSupports = FVector::ZeroVector; int32 Count = 0;
        for (const auto& H : Hands) if (H.bPlanted && H.Grip > .5f
            && FVector::DotProduct(H.Normal, Traversal->GetPendingSurfaceNormal()) > .7f) { NewSupports += H.Anchor; ++Count; }
        if (Count > 0) Target += (NewSupports / Count - EstimatedMassCenter).GetClampedToMaxSize(14.f)
            * FMath::Sin(Traversal->GetTransferAlpha() * PI);
    }
    {
        const float Phase = NowTime * (PI * 2.f / 8.f);
        Creature->GetMesh()->SetMorphTarget(TEXT("RIB_L"), .10f + .38f * (1.f - FMath::Cos(2.f * Phase)) + .07f * FMath::Square(FMath::Sin(Phase)));
        Creature->GetMesh()->SetMorphTarget(TEXT("RIB_R"), .10f + .31f * (1.f - FMath::Cos(2.f * Phase - .22f)) + .05f * FMath::Square(FMath::Sin(Phase - .11f)));
    }
    BalanceCorrection = PendingSupportShift.Size(); Spring(BodyTranslation, BodyVelocity, Target, 4.5f, DeltaSeconds);
    IdleTime += DeltaSeconds;
    SurgePhase += DeltaSeconds * (1.4f + SpeedFrac * 2.2f);
    // Never perfectly still: breath, a slow weight shift, a small searching sway.
    const float Idle = 1.f - FMath::Clamp(SpeedFrac * 2.f, 0.f, 1.f);
    const FVector IdleAngles(FMath::Sin(IdleTime * .55f + 1.f) * 1.4f * Idle,
        FMath::Sin(IdleTime * .8f) * 1.6f * Idle + FMath::Sin(IdleTime * 2.7f) * .5f * Idle,
        FMath::Sin(IdleTime * .37f) * 2.4f * Idle);
    const float Ripple = FMath::Sin(SurgePhase * 2.f) * SpeedFrac;
    const FVector Angles(FMath::Clamp(-TurnVelocity * .11f - SideAcceleration * .02f + Ripple * 3.f
            + PeekSide * PeekAlpha * 15.f, -22.f, 22.f),
        FMath::Clamp(ForwardAcceleration * .03f + SqueezeAmount * 7.f + SpeedFrac * 11.f + PeekAlpha * 6.f + LowAlpha * 32.f, -10.f, 46.f),
        FMath::Clamp(-TurnVelocity * .14f + Ripple * 2.5f, -18.f, 18.f));
    Wobble(BodyAngles, BodyAngularVelocity, Angles + IdleAngles, 4.f, 1.f, DeltaSeconds);
    // The neck trails the body and swings past it, so the whole creature reads as one flowing line.
    const FVector NeckGoal(FMath::Clamp(TurnVelocity * .08f + PeekSide * PeekAlpha * 12.f, -26.f, 26.f),
        FMath::Clamp(-ForwardAcceleration * .035f - SpeedFrac * 6.f + Ripple * 4.f - LowAlpha * 44.f, -54.f, 16.f),
        FMath::Clamp(TurnVelocity * .22f - Ripple * 5.f, -34.f, 34.f));
    Wobble(FlowAngles, FlowVelocity, NeckGoal, 3.5f, .95f, DeltaSeconds);
    NeckWorldRotation = (FQuat(Up, FMath::DegreesToRadians(FlowAngles.Z))
        * FQuat(Right, FMath::DegreesToRadians(FlowAngles.Y))
        * FQuat(Forward, FMath::DegreesToRadians(FlowAngles.X))).GetNormalized();
    BodyWorldRotation = (FQuat(Up, FMath::DegreesToRadians(BodyAngles.Z))
        * FQuat(Right, FMath::DegreesToRadians(BodyAngles.Y))
        * FQuat(Forward, FMath::DegreesToRadians(BodyAngles.X))).GetNormalized();
    // The torso is a chain, not a block. The lean is shared out along it, and on top of that it
    // hunches under low ceilings and in the low stance, arching the back so the head can lift.
    const float Clearance = FMath::Clamp((300.f - HeadClearance) / 110.f, 0.f, 1.f);
    const float HunchTarget = LowAlpha * 14.f + SqueezeAmount * 20.f + Clearance * 18.f + PeekAlpha * 8.f;
    Hunch = FMath::FInterpTo(Hunch, HunchTarget, DeltaSeconds, 2.5f);
    static const float Share[5] = {.12f, .20f, .26f, .24f, .18f};
    static const float Arch[5] = {.15f, .75f, .75f, .30f, -.35f};
    const float LookYaw = FMath::Clamp(FRotator::NormalizeAxis(Creature->GetActorTransform()
        .InverseTransformVectorNoScale(LookDirection).Rotation().Yaw), -105.f, 105.f);
    SpineYaw = FMath::FInterpTo(SpineYaw, LookYaw * .45f, DeltaSeconds, 3.f);
    static const float YawShare[5] = {0.f, 0.f, .12f, .30f, .58f};
    // Coming at a doorway the body curves into it: the head leads round the frame and the
    // torso follows it through, instead of marching at it square-on.
    float DoorTarget = 0.f;
    if (DoorwayWidth < 300.f)
    {
        const FVector ToPortal = PortalCenter - Creature->GetActorLocation();
        const float Ahead = FVector::DotProduct(ToPortal, Forward), Lateral = FVector::DotProduct(ToPortal, Right);
        if (Ahead > -80.f && Ahead < 260.f)
            DoorTarget = FMath::Clamp(Lateral * .3f, -28.f, 28.f) * (1.f - FMath::SmoothStep(120.f, 260.f, Ahead));
    }
    DoorLean = FMath::FInterpTo(DoorLean, DoorTarget, DeltaSeconds, 3.f);
    static const float DoorShare[5] = {0.f, .05f, .2f, .35f, .4f};
    for (int32 I = 0; I < 5; ++I)
    {
        // Lean and twist ripple: each vertebra follows the one below a moment later.
        const float Ripple2 = FMath::Sin(SurgePhase * 2.f - I * .7f) * SpeedFrac * 2.2f;
        SpineWorld[I] = (FQuat(Up, FMath::DegreesToRadians(BodyAngles.Z * Share[I] + SpineYaw * YawShare[I] + Ripple2 * .4f + DoorLean * DoorShare[I]))
            * FQuat(Right, FMath::DegreesToRadians(BodyAngles.Y * Share[I] + Hunch * Arch[I]))
            * FQuat(Forward, FMath::DegreesToRadians(BodyAngles.X * Share[I] + Ripple2 - DoorLean * DoorShare[I] * .5f))).GetNormalized();
    }
}

void UTallowHunterMotionComponent::UpdatePlayback(float DeltaSeconds, float Speed)
{
    auto* Playback = Cast<UTallowHunterAnimInstance>(Creature->GetMesh()->GetAnimInstance()); if (!Playback) return;
    auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
    const auto Mode = Hunter ? Hunter->HuntMode : ETallowHuntMode::Roam;
    if (Mode == ETallowHuntMode::Captured) return;
    const auto* Surface = Creature->HunterTraversal.Get();
    if (bProcedural)
    {
        if (SelectedClip != TEXT("CrouchIdle") && Clips.Contains(TEXT("CrouchIdle")))
        {
            Playback->PlayMotionClip(Clips[TEXT("CrouchIdle")], true, 1.f, .1f, false);
            SelectedClip = TEXT("CrouchIdle");
        }
        const auto* Trav = Creature->HunterTraversal.Get();
        ActiveMotion = Trav && Trav->IsSqueezing() ? TEXT("Squeeze") : Trav && Trav->IsTraversing()
            ? (Trav->GetSurfaceNormal().Z < -.45f ? TEXT("CeilingCrawl") : TEXT("WallCrawl"))
            : PeekAlpha > .25f ? TEXT("Peek") : Speed > 260.f ? TEXT("Run") : Speed > 30.f
            ? (LowAlpha > .5f ? TEXT("LowCreep") : TEXT("Walk")) : LowAlpha > .5f ? TEXT("LowHold") : TEXT("SatUp");
        return;
    }
    FName Name(TEXT("Walk")); float Rate = FMath::Clamp(Speed / (36.6667f * CreatureScale), .18f, 2.5f);
    if (Surface && Surface->IsTraversing() && !Surface->IsSqueezing())
    { Name = Surface->GetSurfaceNormal().Z < -.45f ? TEXT("CeilingCrawl") : TEXT("WallCrawl"); Rate = FMath::Clamp(Speed / 75.f, .25f, 1.45f); }
    else if (SqueezeAmount > (SelectedClip == TEXT("Squeeze") ? .22f : .38f) && Speed > 4.f)
    { Name = TEXT("Squeeze"); Rate = FMath::Clamp(Speed / 50.f, .35f, 1.1f); }
    else if (PeekAlpha > .25f && Clips.Contains(TEXT("Search_Peek")))
    { Name = TEXT("Search_Peek"); Rate = .8f; }
    else if ((Mode == ETallowHuntMode::Pursue || Speed > 200.f) && Speed > (SelectedClip == TEXT("RunChase") ? 112.f : 154.f))
    { Name = TEXT("RunChase"); Rate = FMath::Clamp(Speed / (224.f * CreatureScale), .5f, 1.1f); }
    else if (Speed < (SelectedClip == TEXT("CrouchIdle") ? 16.f : 7.f))
    {
        Name = Mode == ETallowHuntMode::Inspect ? (Hunter->InspectedSpots % 2 ? TEXT("Search_Peek") : TEXT("Sniff"))
            : Mode == ETallowHuntMode::Listen ? TEXT("Listen") : TEXT("CrouchIdle");
        Rate = .7f; if (!Clips.Contains(Name)) Name = TEXT("Search_LookAround");
    }
    else if (Mode == ETallowHuntMode::Roam || Mode == ETallowHuntMode::Withdraw || Mode == ETallowHuntMode::Ambush)
    { Name = TEXT("Stalk"); Rate = FMath::Clamp(Speed / (24.f * CreatureScale), .25f, 1.6f); }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Name != SelectedClip && Now - LastClipChange < .35f && Clips.Contains(SelectedClip)) Name = SelectedClip;
    if (auto* Found = Clips.Find(Name))
    {
        const bool Locomotion = Name == TEXT("Walk") || Name == TEXT("Stalk") || Name == TEXT("RunChase");
        const bool WasLocomotion = SelectedClip == TEXT("Walk") || SelectedClip == TEXT("Stalk") || SelectedClip == TEXT("RunChase");
        Playback->PlayMotionClip(*Found, true, Rate, Locomotion && WasLocomotion ? .38f : .52f, Locomotion && WasLocomotion);
        if (SelectedClip != Name) LastClipChange = Now;
        SelectedClip = Name; ActiveMotion = Name.ToString();
    }
}

void UTallowHunterMotionComponent::PlayPlant(const FVector& Where, bool bWall, float Speed)
{
    // A silent creep: her pats and drags stay quiet with the rest of her (the voice holds the creeping flag).
    if (Creature && Creature->Voice && Creature->Voice->IsCreeping()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (PlantSounds.IsEmpty())
    {
        // Soft hands patting the boards and fabric dragging (my foley, 25 Oct): the creature is soft.
        for (int32 I = 0; I < 6; ++I)
            if (auto* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Creature/C_Pat_%02d.C_Pat_%02d"), I, I))) PlantSounds.Add(S);
        for (int32 I = 0; I < 3; ++I)
            if (auto* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Creature/C_Drag_%02d.C_Drag_%02d"), I, I))) DragSounds.Add(S);
        PlantAttenuation = NewObject<USoundAttenuation>(this);
        PlantAttenuation->Attenuation.bAttenuate = true; PlantAttenuation->Attenuation.bSpatialize = true;
        PlantAttenuation->Attenuation.AttenuationShapeExtents = FVector(120.f); PlantAttenuation->Attenuation.FalloffDistance = 2200.f;
        PlantAttenuation->Attenuation.bAttenuateWithLPF = true; PlantAttenuation->Attenuation.LPFRadiusMin = 500.f;
        PlantAttenuation->Attenuation.LPFRadiusMax = 2200.f; PlantAttenuation->Attenuation.LPFFrequencyAtMax = 2500.f;
        // Natural falloff (linear in dB): about -14 dB at 10 m, so its steps really fade as it leaves. Walls and floors
        // dull them (occlusion), so a step on another storey stays thin.
        PlantAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        PlantAttenuation->Attenuation.dBAttenuationAtMax = -40.f;
        PlantAttenuation->Attenuation.bEnableOcclusion = true;
        PlantAttenuation->Attenuation.OcclusionLowPassFilterFrequency = 800.f;
        PlantAttenuation->Attenuation.OcclusionVolumeAttenuation = .4f;
        PlantAttenuation->Attenuation.OcclusionInterpolationTime = .2f;
    }
    // Two hands landing within a beat read as one heavy step, not a rattle.
    if (PlantSounds.IsEmpty() || Now - LastPlantSound < .12f) return;
    LastPlantSound = Now;
    const float Weight = FMath::GetMappedRangeValueClamped(FVector2D(40.f, 380.f), FVector2D(.35f, .5f), Speed);
    UGameplayStatics::PlaySoundAtLocation(this, PlantSounds[FMath::RandRange(0, PlantSounds.Num() - 1)], Where,
        bWall ? Weight * .6f : Weight, FMath::FRandRange(.86f, 1.f), 0.f, PlantAttenuation);
    // The damp hem drags behind now and then while it moves along the floor.
    if (!bWall && !DragSounds.IsEmpty() && Speed > 60.f && Now - LastDragSound > 1.4f && FMath::FRand() < .3f)
    {
        LastDragSound = Now;
        UGameplayStatics::PlaySoundAtLocation(this, DragSounds[FMath::RandRange(0, DragSounds.Num() - 1)],
            Creature->GetActorLocation(), .35f, FMath::FRandRange(.9f, 1.f), 0.f, PlantAttenuation);
    }
}

void UTallowHunterMotionComponent::PauseAndLook(const FVector& Where, float Seconds)
{
    Rhythm = ERhythm::Pause;
    RhythmUntil = GetWorld()->GetTimeSeconds() + Seconds;
    LookAt(Where, Seconds);
}

void UTallowHunterMotionComponent::UpdateRhythm(float Now, uint8 ModeByte)
{
    const auto Mode = static_cast<ETallowHuntMode>(ModeByte);
    if (Mode == ETallowHuntMode::Pursue || Mode == ETallowHuntMode::Captured) { Rhythm = ERhythm::Move; return; }
    if (Now < RhythmUntil) return;
    const bool bHunting = Mode == ETallowHuntMode::Investigate || Mode == ETallowHuntMode::Search;
    if (Rhythm != ERhythm::Move)
    {
        Rhythm = ERhythm::Move;
        RhythmUntil = Now + (bHunting ? FMath::FRandRange(1.2f, 3.f) : FMath::FRandRange(1.8f, 4.5f));
        return;
    }
    const float Roll = FMath::FRand();
    // Hunting a sound it stops to look less often and bursts more (it used to stop dead on most beats of a hunt).
    if (Roll < (bHunting ? .3f : .45f))
    {
        // Stop dead, dip, and look somewhere off to the side, the way it would when it heard something.
        Rhythm = ERhythm::Pause;
        RhythmUntil = Now + FMath::FRandRange(.7f, 1.6f);
        const FVector Side = Creature->GetActorRightVector() * (FMath::RandBool() ? 1.f : -1.f);
        PauseLook = GetEyeLocation() + (Creature->GetActorForwardVector() * FMath::FRandRange(.3f, 1.f) + Side).GetSafeNormal() * 400.f
            + Creature->GetActorUpVector() * FMath::FRandRange(-60.f, 40.f);
        LookAt(PauseLook, RhythmUntil - Now);
    }
    else if (Roll < (bHunting ? .7f : .65f))
    {
        Rhythm = ERhythm::Burst;
        RhythmUntil = Now + FMath::FRandRange(.5f, 1.f);
    }
    else RhythmUntil = Now + (bHunting ? FMath::FRandRange(1.2f, 3.f) : FMath::FRandRange(1.8f, 4.5f));
}

void UTallowHunterMotionComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaSeconds, TickType, Tick);
    if (!bInitialized || !Creature || DeltaSeconds <= SMALL_NUMBER) return;
    if (!bSilencedDistanceSteps)
        // The director's distance-timed steps (a stand-in for creatures without real plants) stay quiet on this one.
        for (UTallowFootstepComponent* Steps : TInlineComponentArray<UTallowFootstepComponent*>(Creature))
        {
            Steps->SetComponentTickEnabled(false);
            bSilencedDistanceSteps = true;
        }
    if (PoseWarmupFrames < 2 && !bProxy && !bUsingMim)
    {
        // Measure curled finger geometry after the first actual rest clip pose,
        // not the import reference pose left immediately after SetSkeletalMesh.
        ++PoseWarmupFrames; UpdatePlayback(DeltaSeconds, 0.f); return;
    }
    if (const auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
        Hunter && Hunter->HuntMode == ETallowHuntMode::Captured && bUsingMim)
    {
        UpdateMimFit(DeltaSeconds);
        UpdateMimLocomotion(DeltaSeconds, 0.f);
        return;
    }
    if (const auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
        Hunter && Hunter->HuntMode == ETallowHuntMode::Captured && !bProxy)
    {
        if (CaptureStartedAt < 0.f) { ResetContacts(); BodyWorldRotation = FQuat::Identity; }
        UpdateCapture(DeltaSeconds);
        return;
    }
    if (CaptureStartedAt >= 0.f)
    {
        // Back from a capture (the game reset): mask shut, hands free.
        CaptureStartedAt = -1.f; bCaptureCrackPlayed = false;
        Creature->GetMesh()->SetMorphTarget(TEXT("HOOD_OPEN"), 0.f);
        Creature->GetMesh()->SetMorphTarget(TEXT("CHOMP"), 0.f);
        ResetContacts();
    }
    const FVector Location = Creature->GetActorLocation(), Up = Creature->GetActorUpVector();
    const float Moved = FVector::Dist(Location, PreviousLocation); if (Moved > 220.f) ResetContacts();
    const FVector Velocity = Moved < 220.f ? (Location - PreviousLocation) / DeltaSeconds : FVector::ZeroVector;
    SmoothedVelocity = FMath::VInterpTo(SmoothedVelocity, Velocity, DeltaSeconds, 6.f);
    SmoothedAcceleration = FMath::VInterpTo(SmoothedAcceleration,
        ((SmoothedVelocity - PreviousVelocity) / DeltaSeconds).GetClampedToMaxSize(600.f), DeltaSeconds, 2.5f);
    PreviousVelocity = SmoothedVelocity; SmoothedSpeed = SmoothedVelocity.Size(); DistanceSincePlant += Moved;
    TurnVelocity = FMath::FInterpTo(TurnVelocity,
        FMath::FindDeltaAngleDegrees(PreviousYaw, Creature->GetActorRotation().Yaw) / DeltaSeconds, DeltaSeconds, 3.f);
    PreviousLocation = Location; PreviousUp = Up; PreviousYaw = Creature->GetActorRotation().Yaw;
    const float Now = GetWorld()->GetTimeSeconds(); const FVector Forward = Creature->GetActorForwardVector();
    const bool bLocked = Now < GazeLockUntil;
    // With nothing to look at the head leads the body into the turn instead of staring ahead.
    FVector RouteAhead;
    const auto* Router = Cast<ATallowHunterAIController>(Creature->GetController());
    const bool bPlayerGaze = Router && Router->HasDirectPlayerSight()
        && Router->HuntMode != ETallowHuntMode::Captured;
    const bool bRoute = Router && SmoothedSpeed > 20.f && Router->GetRouteAhead(RouteAhead)
        && FVector::Dist2D(RouteAhead, Creature->GetActorLocation()) > 80.f;
    // The head drags it forward: it looks round the next corner of its route before the body turns.
    FVector Aim = bPlayerGaze ? Router->GetVisiblePlayerGazeTarget() - GetEyeLocation()
        : Now < LookUntil ? LookTarget - GetEyeLocation()
        : bRoute ? (RouteAhead + FVector(0, 0, 60.f) - GetEyeLocation())
        : SmoothedSpeed > 40.f ? (Forward + SmoothedVelocity.GetSafeNormal() * 1.4f)
        : bMound ? Forward
        : Forward + Creature->GetActorRightVector() * (FMath::Sin(Now * .9f) * .32f + FMath::Sin(Now * 2.3f) * .06f)
            + Creature->GetActorUpVector() * FMath::Sin(Now * .63f + 1.f) * .12f;
    if (!bPlayerGaze)
    {
        FRotator Angles = Creature->GetActorTransform().InverseTransformVectorNoScale(Aim.GetSafeNormal()).Rotation();
        Angles.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Angles.Yaw), bLocked ? -105.f : -62.f, bLocked ? 105.f : 62.f);
        Angles.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Angles.Pitch), bLocked ? -42.f : -32.f, bLocked ? 42.f : 32.f);
        Aim = Creature->GetActorTransform().TransformVectorNoScale(Angles.Vector());
    }
    else Aim = Aim.GetSafeNormal(); // Visible-player gaze has no human neck cone, on any surface.
    LookDirection = FMath::VInterpNormalRotationTo(LookDirection, Aim, DeltaSeconds,
        GetWorld()->GetTimeSeconds() < GazeLockUntil ? 190.f : 160.f);
    const auto* Mesh = Creature->GetMesh();
    // Authored mass fractions for kinematic balancing, not a rigid-body mass solve.
    // The limb share follows actual segment midpoints, so reaching moves the estimate.
    EstimatedMassCenter = bProxy ? Location : bUsingMim
        ? Mesh->GetBoneLocation(TEXT("abdomen_01")) * .60f + Mesh->GetBoneLocation(HeadBone) * .20f
        : Mesh->GetBoneLocation(TEXT("DEF_body")) * .60f + Mesh->GetBoneLocation(HeadBone) * .20f;
    FVector LimbCenters = FVector::ZeroVector;
    for (const auto& H : Hands)
    {
        const FVector Shoulder = Mesh->GetBoneLocation(H.Upper), Elbow = Mesh->GetBoneLocation(H.Lower);
        const FVector Wrist = Mesh->GetBoneLocation(H.Hand);
        LimbCenters += (Shoulder + Elbow) * .5f * .45f
            + (Elbow + Wrist) * .5f * .40f + Wrist * .15f;
    }
    EstimatedMassCenter += LimbCenters / FMath::Max(1, Hands.Num()) * .20f;
    if (bProxy) UpdateProxy(DeltaSeconds);
    else if (bUsingMim)
    {
        if (Creature->HunterTraversal && Creature->HunterTraversal->IsDirectCeilingTransfer())
        {
            // This authored roll uses the actor's floor-to-roof trajectory.
            // A room-fitting sideways offset would move its already planted
            // wrists away from the ceiling and invalidate their finite reach.
            MimFitScale = FVector::OneVector;
            Creature->GetMesh()->SetRelativeScale3D(MimFitScale);
            Creature->GetMesh()->SetRelativeLocation(FVector(0.f, 0.f,
                (2.f - Creature->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
                / FMath::Max(Creature->GetActorScale3D().Z, .01f)));
        }
        else if (!Creature->HunterTraversal || (!Creature->HunterTraversal->IsPouncing()
            && !Creature->HunterTraversal->IsCornerBracing())) UpdateMimFit(DeltaSeconds);
        else MimFitScale = Creature->GetMesh()->GetRelativeScale3D();
        UpdateMimLocomotion(DeltaSeconds, SmoothedSpeed);
    }
    else { UpdatePosture(DeltaSeconds, SmoothedSpeed); UpdateHands(DeltaSeconds, SmoothedSpeed); UpdatePlayback(DeltaSeconds, SmoothedSpeed); }
    if (!Creature->HunterTraversal || !Creature->HunterTraversal->IsTraversing())
    {
        auto* Hunter = Cast<ATallowHunterAIController>(Creature->GetController());
        const auto Mode = Hunter ? Hunter->HuntMode : ETallowHuntMode::Roam;
        const float Fatigue = Hunter ? Hunter->GetPursuitFatigue() : 0.f;
        const float NowSeconds = GetWorld()->GetTimeSeconds();
        const auto* Overseer = GetWorld()->GetSubsystem<UTallowHunterDirector>();
        const float Escalation = Overseer ? Overseer->GetSpeedMultiplier() : 1.f;
        // The player sprints at 450 and walks at 250.
        const float Aggro = Overseer ? Overseer->GetAggression() : 0.f;
        // 29 Oct: "Mim is really slow... I was running a lot and it was barely able to find me". The chase is now
        // just over her sprint where the body fits the room (Normal posture, fit 1), under it in a tight one (fit .8)
        // and well under it folded (Compact, fit .3 to .55): in a straight run it closes, and doors, crawls and tight
        // turns are where she gets away. A hunt for a sound goes faster than her walk. Off the chase a predator's
        // rhythm still sets the pace: creep, stop and look, burst.
        (void)Escalation;
        UpdateRhythm(NowSeconds, static_cast<uint8>(Mode));
        const float BaseSpeed = Mode == ETallowHuntMode::Pursue
            ? FMath::Lerp(470.f, 520.f, Aggro) * FMath::Lerp(1.f, .93f, Fatigue) * (Hunter && Hunter->bCatchUp ? 1.15f : 1.f)
            : (Mode == ETallowHuntMode::Investigate ? 330.f : Mode == ETallowHuntMode::Search ? 260.f
                : Mode == ETallowHuntMode::Ambush ? 200.f : 190.f)
                * (Hunter ? Hunter->GetPaceScale() : 1.f) * RhythmScale();
        const float Pull = FMath::Lerp(.63f, 1.f, SupportStrength);
        const float Balance = SupportingHands >= 3 ? FMath::Lerp(.72f, 1.f,
            FMath::Clamp((SupportMargin + 12.f) / 24.f, 0.f, 1.f)) : .78f;
        float DesiredSpeed = (bProxy || bUsingMim) ? BaseSpeed : BaseSpeed * Pull * Balance * FMath::Lerp(1.f, .55f, SqueezeAmount);
        // Establish actual supports before hauling the mass forward. A low
        // support count slows the existing acceleration-driven path following.
        if (bProxy || bUsingMim) {}
        else if (SupportingHands == 0) DesiredSpeed = BaseSpeed * .35f;
        else if (SupportingHands == 1) DesiredSpeed = FMath::Max(DesiredSpeed, BaseSpeed * .5f);
        else if (SupportingHands == 2) DesiredSpeed = FMath::Max(DesiredSpeed, BaseSpeed * .8f);
        // The chase surges a little; off the chase the rhythm above does the work.
        if (Mode == ETallowHuntMode::Pursue) DesiredSpeed *= .97f + .04f * FMath::Sin(NowSeconds * 2.3f);
        if (Mode == ETallowHuntMode::Pursue) DesiredSpeed = FMath::Max(DesiredSpeed, BaseSpeed * .94f);
        if (SmoothedSpeed > 5.f && !bProxy && !bUsingMim)
        {
            const FVector Travel = SmoothedVelocity.GetSafeNormal();
            TArray<float> SupportTravel;
            for (const auto& H : Hands) if (H.bPlanted && H.Grip > .35f && FVector::DotProduct(H.Normal, Up) > .45f)
            {
                const FVector Difference = Mesh->GetBoneLocation(H.Upper) - H.Target;
                const float Along = FVector::DotProduct(Difference, Travel);
                const float Radicand = Along * Along + FMath::Square(H.Reach * .98f) - Difference.SizeSquared();
                SupportTravel.Add(FMath::Max(0.f, -Along + FMath::Sqrt(FMath::Max(0.f, Radicand))));
            }
            SupportTravel.Sort([](float A, float B) { return A > B; });
            if (SupportTravel.Num() >= 2)
            {
                // Brake while two existing supports still have reach in reserve,
                // rather than waiting for both limbs to become overextended.
                const float Reserve = FMath::Max(0.f, SupportTravel[1] - 12.f);
                DesiredSpeed = FMath::Min(DesiredSpeed, FMath::Max(BaseSpeed * .5f, FMath::Min(Reserve / .24f,
                    FMath::Sqrt(2.f * 320.f * Reserve))));
            }
            const float Facing = FVector::DotProduct(Travel, Forward);
            DesiredSpeed *= FMath::Lerp(.4f, 1.f, FMath::Clamp((Facing + .3f) / 1.3f, 0.f, 1.f));
        }
        // The support bookkeeping may slow a step, never the whole creature: it used to stack six brakes and crawl.
        DesiredSpeed = FMath::Max(DesiredSpeed, BaseSpeed * .97f);
        if (bProxy || bUsingMim) DesiredSpeed *= FitSpeed * (NowSeconds < SqueezeBeatUntil ? .3f : 1.f);
        SmoothedMaxSpeed = FMath::FInterpTo(SmoothedMaxSpeed, DesiredSpeed, DeltaSeconds,
            DesiredSpeed < SmoothedMaxSpeed ? 12.f : (Rhythm == ERhythm::Burst ? 9.f : 4.f));
        auto* Movement = Creature->GetCharacterMovement(); Movement->MaxWalkSpeed = SmoothedMaxSpeed;
        // Off the mark fast once it has her: the old 650 (x.35 folded) took most of a second to reach a walk.
        Movement->MaxAcceleration = (Mode == ETallowHuntMode::Pursue || Rhythm == ERhythm::Burst ? 1000.f
            : Mode == ETallowHuntMode::Investigate ? 750.f : 420.f)
            * (bUsingMim ? FMath::Lerp(.35f, 1.f, FitSpeed) : 1.f);
        // It used to turn at 72 degrees a second, like a barge. It should whip round corners.
        Movement->RotationRate = FRotator(0, (Mode == ETallowHuntMode::Pursue ? 320.f
            : Mode == ETallowHuntMode::Investigate ? 230.f : 150.f)
            * (bProxy ? FMath::Lerp(.5f, 1.f, (FitSpeed - .72f) / .28f)
                : bUsingMim ? FMath::Lerp(.4f, 1.f, FitSpeed) : 1.f), 0);
        Movement->BrakingDecelerationWalking = Rhythm == ERhythm::Pause ? 700.f : 320.f;
    }
}
