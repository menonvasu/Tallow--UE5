#include "AI/Hunter/TallowMimAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Player/TallowCharacter.h"
#include "TwoBoneIK.h"

struct FTallowMimAnimProxy : FAnimInstanceProxy
{
    explicit FTallowMimAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    struct FArmClearance
    {
        int32 Upper, Fore, Hand;
        FVector Target, Pole;
        float Weight;
    };
    struct FArmBinding
    {
        int32 Upper, Fore, Hand, ContactIndex;
        FName Curve;
        int32 Shoulder = INDEX_NONE;
        bool bHasApproach = false;
        bool bCornerApproach = false;
        bool bSurfaceTransfer = false;
        FVector ApproachWorld = FVector::ZeroVector;
        FVector ApproachNormal = FVector::UpVector;
    };
    struct FHandContact
    {
        bool bPinned = false;
        bool bAwaitingLift = false;
        bool bSawLift = false;
        bool bApproachAnchor = false;
        FVector AnchorWorld = FVector::ZeroVector;
        FQuat RotationWorld = FQuat::Identity;
        FVector NormalWorld = FVector::UpVector;
    };
    struct FPlantEvent
    {
        FVector WristWorld, SurfaceNormal;
        float Speed;
    };

    UAnimSequence* ClipA = nullptr;
    UAnimSequence* ClipB = nullptr;
    UAnimSequence* Action = nullptr;
    float TimeA = 0.f;
    float TimeB = 0.f;
    float GaitBlend = 0.f;
    float ActionTime = 0.f;
    float ActionWeight = 0.f;
    float TransitionBlend = 1.f;
    uint32 SeenSerial = 0;
    TArray<FTransform> LastPose;
    TArray<FTransform> SourcePose;
    TArray<FTransform> LastRawPose, SourceRawPose;
    FBlendedHeapCurve LastRawCurve, SourceRawCurve;
    FBlendedHeapCurve LastCurve;
    FBlendedHeapCurve SourceCurve;
    TArray<FArmClearance> Clearances;
    TArray<FArmBinding> ArmBindings;
    TArray<FHandContact> HandContacts;
    FTransform ComponentToWorld = FTransform::Identity;
    FTransform PreviousComponentToWorld = FTransform::Identity;
    bool bHasComponentTransform = false;
    bool bResetContacts = true;
    bool bSuppressPlantEvents = true;
    FVector PreviousOwnerLocation = FVector::ZeroVector;
    FVector SupportNormal = FVector::UpVector;
    FVector DirectRoofSurface = FVector::ZeroVector;
    FVector DirectRoofNormal = FVector::DownVector;
    bool bHasDirectRoofPlane = false;
    FVector DirectFloorSurface = FVector::ZeroVector;
    bool bHasDirectFloorPlane = false;
    float ActualSpeed = 0.f;
    TArray<FPlantEvent> PendingPlants;
    TArray<float> ContactWeights;
    TArray<bool> ContactPinned;
    TArray<FVector> ContactAnchorsWorld;
    TArray<FVector> ContactNormalsWorld;
    TArray<float> ContactAnchorErrorCm;
    int32 ContactReleaseCount = 0;
    int32 ContactPlantEventCount = 0;
    int32 EvaluationSerial = 0;
    int32 MotionRootMeshIndex = INDEX_NONE;
    float RootTranslationRemovedCm = 0.f;
    float RootRotationRemovedDeg = 0.f;
    const USkeletalMesh* HeadMesh = nullptr;
    int32 HeadMeshIndex = INDEX_NONE;
    FVector HeadFaceAxis = FVector::ForwardVector;
    FVector LookWorld = FVector::ForwardVector;
    FVector HeadLookDirection = FVector::ForwardVector;
    FVector HeadAuthoredDirection = FVector::ForwardVector;
    float HeadLookWeight = 0.f;
    bool bKeepCaptureRoot = false;
    bool bCheckedPounce = false;
    TArray<bool> AdaptiveForcedReplant;
    bool bCanonicalCapture = false;
    bool bCrampedCapture = false;
    bool bGroundHandPadding = false;
    // The capture shot (28 Oct): once the view is turned onto her, the face looks into the
    // lens and the four front hands take the head at the frame edges, wherever the lens is.
    bool bCaptureShot = false;
    FVector CaptureGripTargets[4];
    FVector CaptureGripBends[4];
    float CaptureGripWeights[4] = {0.f, 0.f, 0.f, 0.f};
    // Per hand: extra depth (cm) its frame corner is moved out by while that hand comes near the lens.
    float CaptureGripReach[4] = {0.f, 0.f, 0.f, 0.f};

#include "TallowMimAdaptiveFit.inl"

    virtual void Initialize(UAnimInstance* Instance) override
    {
        FAnimInstanceProxy::Initialize(Instance);
        LastPose.Reset();
        SourcePose.Reset();
        LastRawPose.Reset(); SourceRawPose.Reset();
        SeenSerial = 0;
        HandContacts.Init(FHandContact(), 10);
        bHasComponentTransform = false;
        bResetContacts = true;
        ContactReleaseCount = 0;
        ContactPlantEventCount = 0;
        EvaluationSerial = 0;
        PendingPlants.Reset();
    }

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const auto* State = CastChecked<UTallowMimAnimInstance>(Instance);
        if (SeenSerial != State->BaseSerial)
        {
            SourcePose = LastPose;
            SourceCurve.CopyFrom(LastCurve);
            SourceRawPose = LastRawPose;
            SourceRawCurve.CopyFrom(LastRawCurve);
            SeenSerial = State->BaseSerial;
        }
        ClipA = State->GaitA;
        ClipB = State->GaitB;
        Action = State->ActionClip;
        TimeA = State->GaitTimeA;
        TimeB = State->GaitTimeB;
        GaitBlend = FMath::Clamp(State->GaitAlpha, 0.f, 1.f);
        ActionTime = State->ActionTime;
        ActionWeight = FMath::Clamp(State->ActionAlpha, 0.f, 1.f);
        TransitionBlend = State->BlendSeconds > UE_SMALL_NUMBER
            ? FMath::SmoothStep(0.f, State->BlendSeconds, State->BlendAge) : 1.f;
        Clearances.Reset();
        ArmBindings.Reset();
        bHasDirectRoofPlane = false;
        bHasDirectFloorPlane = false;
        USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
        if (!Mesh || !Mesh->GetWorld() || !Mesh->GetOwner()) return;
        MotionRootMeshIndex = Mesh->GetBoneIndex(TEXT("root"));
        AActor* Owner = Mesh->GetOwner();
        if (HeadMesh != Mesh->GetSkeletalMeshAsset())
        {
            HeadMesh = Mesh->GetSkeletalMeshAsset();
            HeadMeshIndex = Mesh->GetBoneIndex(TEXT("head"));
            const int32 EyeLeft = Mesh->GetBoneIndex(TEXT("eye_l"));
            const int32 EyeRight = Mesh->GetBoneIndex(TEXT("eye_r"));
            if (HeadMesh && HeadMeshIndex != INDEX_NONE && EyeLeft != INDEX_NONE && EyeRight != INDEX_NONE)
            {
                const FReferenceSkeleton& RefSkeleton = HeadMesh->GetRefSkeleton();
                const auto& Reference = RefSkeleton.GetRefBonePose();
                auto ReferenceCS = [&](int32 Index)
                {
                    FTransform Transform = Reference[Index];
                    for (int32 Parent = RefSkeleton.GetParentIndex(Index); Parent != INDEX_NONE;
                        Parent = RefSkeleton.GetParentIndex(Parent)) Transform = Transform * Reference[Parent];
                    return Transform;
                };
                const FVector EyeCenter = (ReferenceCS(EyeLeft).GetLocation()
                    + ReferenceCS(EyeRight).GetLocation()) * .5f;
                HeadFaceAxis = ReferenceCS(HeadMeshIndex).InverseTransformVectorNoScale(
                    EyeCenter - ReferenceCS(HeadMeshIndex).GetLocation()).GetSafeNormal();
            }
        }
        const auto* Motion = Owner->FindComponentByClass<UTallowHunterMotionComponent>();
        bKeepCaptureRoot = Motion && Motion->ShouldKeepMimCaptureRoot();
        const FString ActingName = Action && ActionWeight > .15f ? Action->GetName() : FString();
        const bool bCornerLook = ActingName == TEXT("MIM_Surf_Corner_Ambush");
        const auto* Pawn = Cast<APawn>(Owner);
        const auto* Brain = Pawn ? Cast<ATallowHunterAIController>(Pawn->GetController()) : nullptr;
        const FVector CornerFocus = bCornerLook && Brain ? Brain->GetIntentFocus() : FVector::ZeroVector;
        const bool bCornerHasFocus = bCornerLook && Brain && Brain->GetEvidenceStrength() > .05f
            && !CornerFocus.IsNearlyZero();
        const bool bPlayerGaze = Brain && Brain->HasDirectPlayerSight()
            && Brain->HuntMode != ETallowHuntMode::Captured
            && !ActingName.StartsWith(TEXT("MIM_Cap_")) && !ActingName.Contains(TEXT("Drop_Grab"));
        const bool bLookAllowed = bPlayerGaze || ActingName.IsEmpty() || ActingName.StartsWith(TEXT("MIM_Loco_"))
            || ActingName.StartsWith(TEXT("MIM_Hunt_Listen")) || ActingName == TEXT("MIM_Hunt_Withdraw") || bCornerHasFocus;
        const float LookWeight = Motion && bLookAllowed
            ? bPlayerGaze || Motion->IsGazeLocked() || (Brain && Brain->TacticalPhase == TEXT("WatchingEscape")) ? 1.f : .90f : 0.f;
        HeadLookWeight = FMath::FInterpTo(HeadLookWeight, LookWeight, DeltaSeconds, 12.f);
        if (Motion) LookWorld = Motion->GetLookDirection().GetSafeNormal();
        if (bCornerHasFocus && !bPlayerGaze)
            LookWorld = (CornerFocus - Mesh->GetBoneLocation(TEXT("head"))).GetSafeNormal();
        const FVector OwnerLocation = Owner->GetActorLocation();
        ActualSpeed = bHasComponentTransform && DeltaSeconds > UE_SMALL_NUMBER
            ? FVector::Distance(OwnerLocation, PreviousOwnerLocation) / DeltaSeconds : 0.f;
        PreviousOwnerLocation = OwnerLocation;
        SupportNormal = Owner->GetActorUpVector();
        bCheckedPounce = false;
        if (const auto* Traversal = Owner->FindComponentByClass<UTallowHunterTraversalComponent>();
            Traversal && Traversal->IsTraversing())
        {
            SupportNormal = Traversal->GetSurfaceNormal();
            bCheckedPounce = Traversal->IsPouncing();
            ActualSpeed = Traversal->GetTraversalSpeed();
        }
        FitGeometry = Motion;
        FitEvaluationMilliseconds = 0.f;
        FitNeutralClip = State->FitNeutralClip;
        FitTallClip = State->FitTallClip;
        FitUprightClip = State->FitUprightClip;
        FitLowClip = State->FitLowClip;
        const auto* FitTraversal = Owner->FindComponentByClass<UTallowHunterTraversalComponent>();
        const FVector WorldFitClearance = Motion ? Motion->GetFitPoseClearance() : FVector(800.f, 800.f, 350.f);
        // The fit solver works in body (rig) units against component-space pose
        // bounds; feeding it world centimetres made it read every room as
        // 1/scale too small and shove the body down (worse the smaller she is).
        const float FitBodyUnits = FMath::Max(Mesh->GetComponentScale().GetAbsMin(), .1f);
        FitClearance = WorldFitClearance / FitBodyUnits;
        // A short, high turning space needs the existing reared torso. The
        // doorway posture is narrow but still too long for a stair corner.
        // Arms keep their gait and use the same finite-length fit solve.
        const auto* GroundMovement = Owner->FindComponentByClass<UCharacterMovementComponent>();
        const bool bGroundUprightFit = FitUprightClip && GroundMovement && GroundMovement->IsMovingOnGround()
            && SupportNormal.Z > .95f
            && (!FitTraversal || !FitTraversal->IsTraversing()) && !ActingName.StartsWith(TEXT("MIM_Cap_"));
        const float UprightFit = bGroundUprightFit
            ? (1.f - FMath::SmoothStep(190.f, 235.f, float(WorldFitClearance.X)))
                * FMath::SmoothStep(270.f, 310.f, float(WorldFitClearance.Z)) : 0.f;
        FitUprightAlpha = !bGroundUprightFit ? 0.f
            : FMath::FInterpTo(FitUprightAlpha, UprightFit, DeltaSeconds, 7.f);
        bCanonicalCapture = State->HasAdaptiveFitLayer() && ActingName.StartsWith(TEXT("MIM_Cap_"));
        bCrampedCapture = bCanonicalCapture && !bKeepCaptureRoot
            && (WorldFitClearance.Y < 180.f || WorldFitClearance.Z < 160.f);
        const bool bFitExcluded = bCheckedPounce || bKeepCaptureRoot
            || ActingName == TEXT("MIM_Surf_FloorToCeiling")
            || (FitTraversal && FitTraversal->IsCornerBracing()) || (bCanonicalCapture && !bCrampedCapture);
        FVector FitGoal = FVector::ZeroVector;
        if (Motion && State->HasAdaptiveFitLayer() && !bFitExcluded)
        {
            // FitClearance is in body units here (see above), matching these
            // rig-authored fold targets at any actor scale.
            FitGoal.X = Motion->GetFitAxisWeights().X;
            FitGoal.Y = FMath::Clamp((550.f - FitClearance.Y) / (550.f - 144.f), 0.f, 1.f);
            FitGoal.Z = FMath::Clamp((205.f - FitClearance.Z) / 95.f, 0.f, 1.f);
        }
        FitLayerWeights = bFitExcluded ? FVector::ZeroVector
            : FMath::VInterpTo(FitLayerWeights, FitGoal, DeltaSeconds, 10.f);
        bFitApplied = State->HasAdaptiveFitLayer() && FitLayerWeights.GetMax() > .005f && !bFitExcluded;
        // An aperture envelope does not describe furniture inside that space.
        // Ground fitting must retain the actual arm/world contact correction.
        bFitWorldContacts = bFitApplied && GroundMovement && GroundMovement->IsMovingOnGround()
            && SupportNormal.Z > .95f && !bCanonicalCapture
            && (!FitTraversal || !FitTraversal->IsTraversing());
        bGroundHandPadding = !bFitApplied && Owner->GetActorUpVector().Z > .85f
            && !bCheckedPounce && !bKeepCaptureRoot && !bCanonicalCapture
            && !ActingName.StartsWith(TEXT("MIM_Cap_")) && !ActingName.Contains(TEXT("Drop_Grab"))
            && (!Brain || Brain->HuntMode != ETallowHuntMode::Captured)
            && (!FitTraversal || !FitTraversal->IsTraversing());
        // Motion getters already reserve6cm total side/2cm top clearance.
        bFitWithinLimits = WorldFitClearance.Y >= 76.f && WorldFitClearance.Z >= 83.f;
        FitBodyScale = FVector::OneVector;
        if (bFitApplied && !bPlayerGaze) HeadLookWeight *= 1.f - FitLayerWeights.GetMax();
        bCaptureShot = false;
        const bool bShotClip = ActingName == TEXT("MIM_Cap_Grab_Floor") || ActingName == TEXT("MIM_Cap_Grab_Dark");
        if (bShotClip && !bKeepCaptureRoot && Brain && Brain->HuntMode == ETallowHuntMode::Captured)
            if (const auto* Her = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(Owner, 0)))
                if (const UCameraComponent* Lens = Her->GetFirstPersonCameraComponent())
                {
                    // The clip is authored against a nominal lens; these keep its framing at any
                    // size, eye height or staging error. Offsets are rig metres, scaled to the body.
                    bCaptureShot = true;
                    const FVector Eye = Lens->GetComponentLocation();
                    const FVector Forward = Lens->GetForwardVector(), Right = Lens->GetRightVector(), Up = Lens->GetUpVector();
                    const float Scale = Mesh->GetComponentScale().GetAbsMin(), RigMetre = Scale * 100.f;
                    LookWorld = (Eye - Mesh->GetBoneLocation(TEXT("head"))).GetSafeNormal();
                    HeadLookWeight = FMath::Clamp(State->GetCurveValue(TEXT("CapLook")), 0.f, 1.f);
                    const float Grip = FMath::Clamp(State->GetCurveValue(TEXT("CapGrip")), 0.f, 1.f);
                    // The four hands land on the corners of the frame, .30 rig metres in front of the lens: the
                    // upper pair high, the raised pair low, so the grab reads round the edges of the picture.
                    const float Depth = .30f * RigMetre;
                    const float Half = Depth * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Lens->FieldOfView, 30.f, 120.f)) * .5f);
                    const float Aspect = FMath::Max(Lens->AspectRatio, 1.f);
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        const bool bUpperPair = Index < 2;
                        const TCHAR* SideName = Index % 2 == 0 ? TEXT("l") : TEXT("r");
                        const int32 Pair = bUpperPair ? 1 : 2;
                        const FName Shoulder(*FString::Printf(TEXT("arm%02d_upper_%s"), Pair, SideName));
                        // No clipping: the hand and its fingers to their tips, fur included (mim-capsules.json
                        // hand radius plus 16 cm of coat at the wrist, 5.5 cm on the fingers), as they stood last
                        // frame. A hand that came inside 10 cm takes its corner further out along the same line,
                        // so it stays on the edge of the frame and clears the lens.
                        static const float HandRadius[2] = {7.9f, 9.1f};
                        const FVector Wrist = Mesh->GetBoneLocation(FName(*FString::Printf(TEXT("arm%02d_hand_%s"), Pair, SideName)));
                        float Clearance = FVector::Distance(Wrist, Eye) - (HandRadius[Pair - 1] + 16.f) * Scale;
                        for (int32 Digit = 1; Digit <= 5; ++Digit)
                        {
                            const FName Last(*FString::Printf(TEXT("arm%02d_d%d_03_%s"), Pair, Digit, SideName));
                            const FName Middle(*FString::Printf(TEXT("arm%02d_d%d_02_%s"), Pair, Digit, SideName));
                            if (Mesh->GetBoneIndex(Last) == INDEX_NONE || Mesh->GetBoneIndex(Middle) == INDEX_NONE) continue;
                            const FVector Knuckle = Mesh->GetBoneLocation(Last);
                            const FVector Tip = Knuckle + (Knuckle - Mesh->GetBoneLocation(Middle));
                            Clearance = FMath::Min(Clearance, float(FMath::Min(FMath::PointDistToSegment(Eye, Wrist, Knuckle),
                                FMath::PointDistToSegment(Eye, Knuckle, Tip))) - (HandRadius[Pair - 1] * .6f + 5.5f) * Scale);
                        }
                        float& Reach = CaptureGripReach[Index];
                        Reach = Clearance < 10.f ? Reach + (10.f - Clearance) * 1.2f : FMath::Max(0.f, Reach - .5f);
                        const float Out = (Depth + Reach) / Depth;
                        // Each hand takes the side of the view its own shoulder is on.
                        const float Side = FVector::DotProduct(Mesh->GetBoneLocation(Shoulder) - Eye, Right) >= 0.f ? 1.f : -1.f;
                        CaptureGripTargets[Index] = Eye + (Forward * Depth + Right * Side * Half + Up * (bUpperPair ? 1.f : -1.f) * Half / Aspect) * Out;
                        // Elbows out wide and back toward her body, never toward the lens.
                        CaptureGripBends[Index] = (Right * Side * 1.30f + Forward * .55f + Up * (bUpperPair ? .45f : -.35f));
                        CaptureGripWeights[Index] = Grip;
                    }
                }
        if (!bCaptureShot) for (float& Reach : CaptureGripReach) Reach = 0.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MimArmClearance), false, Mesh->GetOwner());
        if (const auto* Capsule = Owner->FindComponentByClass<UCapsuleComponent>())
            Query.AddIgnoredActors(Capsule->GetMoveIgnoreActors());
        if (APawn* Player = UGameplayStatics::GetPlayerPawn(Mesh, 0)) Query.AddIgnoredActor(Player);
        ComponentToWorld = Mesh->GetComponentTransform();
        FitSupportPlane = -2.f;
        if (bFitApplied || bGroundHandPadding)
        {
            FHitResult Support;
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Support,
                OwnerLocation + SupportNormal * 10.f, OwnerLocation - SupportNormal * 350.f, ECC_Pawn, Query)
                && FVector::DotProduct(Support.ImpactNormal, SupportNormal) > .7f)
                FitSupportPlane = ComponentToWorld.InverseTransformPosition(Support.ImpactPoint).Z;
        }
        if (bHasComponentTransform)
        {
            const bool bTeleported = FVector::DistSquared(ComponentToWorld.GetLocation(),
                PreviousComponentToWorld.GetLocation()) > FMath::Square(200.f);
            const bool bChangedSurface = ComponentToWorld.GetRotation().AngularDistance(
                PreviousComponentToWorld.GetRotation()) > FMath::DegreesToRadians(45.f);
            const bool bChangedSize = !ComponentToWorld.GetScale3D().Equals(
                PreviousComponentToWorld.GetScale3D(), .15f);
            bResetContacts |= bTeleported || bChangedSurface || bChangedSize;
        }
        PreviousComponentToWorld = ComponentToWorld;
        bHasComponentTransform = true;
        const bool bCapture = Action && ActionWeight > .15f
            && (Action->GetName().StartsWith(TEXT("MIM_Cap_")) || Action->GetName().Contains(TEXT("Drop_Grab")));
        bSuppressPlantEvents = bResetContacts || TransitionBlend < .999f || bCapture;
        for (int32 Pair = 1; Pair <= 5; ++Pair)
            for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
            {
                const FString Prefix = FString::Printf(TEXT("arm%02d_"), Pair);
                const FName UpperName(*FString::Printf(TEXT("%supper_%s"), *Prefix, Side));
                const FName ForeName(*FString::Printf(TEXT("%sfore_%s"), *Prefix, Side));
                const FName HandName(*FString::Printf(TEXT("%shand_%s"), *Prefix, Side));
                const int32 Upper = Mesh->GetBoneIndex(UpperName);
                const int32 Fore = Mesh->GetBoneIndex(ForeName);
                const int32 Hand = Mesh->GetBoneIndex(HandName);
                if (Upper == INDEX_NONE || Fore == INDEX_NONE || Hand == INDEX_NONE) continue;
                const int32 ContactIndex = (Pair - 1) * 2 + (Side[0] == 'l' ? 0 : 1);
                const FVector A = Mesh->GetBoneLocation(UpperName), B = Mesh->GetBoneLocation(ForeName);
                const FVector C = Mesh->GetBoneLocation(HandName);
                FArmBinding Binding{Upper, Fore, Hand, ContactIndex,
                    FName(*FString::Printf(TEXT("Contact_%02d_%s"), Pair, Side))};
                Binding.Shoulder = Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("%sshoulder_%s"), *Prefix, Side)));
                if (const auto* Traversal = Owner->FindComponentByClass<UTallowHunterTraversalComponent>())
                {
                    Binding.bHasApproach = Traversal->GetApproachContact(ContactIndex, A,
                        Binding.ApproachWorld, Binding.ApproachNormal);
                    Binding.bCornerApproach = Binding.bHasApproach && Traversal->IsCornerBracing();
                    Binding.bSurfaceTransfer = Binding.bHasApproach && Traversal->IsTransferring();
                    if (Binding.bHasApproach && !Binding.bCornerApproach)
                        Binding.ApproachWorld += Binding.ApproachNormal * 6.5f;
                    if (Traversal->IsDirectCeilingTransfer() && ActingName == TEXT("MIM_Surf_FloorToCeiling")
                        && Binding.bHasApproach && Binding.ApproachNormal.Z < -.5f)
                    {
                        DirectRoofNormal = Binding.ApproachNormal;
                        DirectRoofSurface = Binding.ApproachWorld - DirectRoofNormal
                            * UTallowHunterTraversalComponent::GetDirectCeilingWristStandOff(ContactIndex);
                        bHasDirectRoofPlane = true;
                    }
                }
                ArmBindings.Add(Binding);
                FVector WristPush = FVector::ZeroVector, ElbowPush = FVector::ZeroVector;
                float Greatest = 0.f;
                auto Separate = [&](const FVector& From, const FVector& To, bool bUpper)
                {
                    const FVector Along = To - From;
                    if (Along.Size() < 1.f) return;
                    const FVector Mid = (From + To) * .5f;
                    const FQuat Rotation = FRotationMatrix::MakeFromZ(Along).ToQuat();
                    const FCollisionShape Shape = FCollisionShape::MakeCapsule(6.f, Along.Size() * .5f + 6.f);
                    TArray<FOverlapResult> Overlaps;
                    Mesh->GetWorld()->OverlapMultiByChannel(Overlaps, Mid, Rotation, ECC_Pawn, Shape, Query);
                    for (const FOverlapResult& Overlap : Overlaps)
                    {
                        UPrimitiveComponent* Other = Overlap.GetComponent();
                        FMTDResult MTD;
                        if (!Overlap.bBlockingHit || !Other || !Other->ComputePenetration(MTD, Shape, Mid, Rotation)
                            || MTD.Distance < 2.f) continue;
                        const float Depth = FMath::Min(MTD.Distance + 2.f, 90.f);
                        Greatest = FMath::Max(Greatest, Depth);
                        const FVector Push = MTD.Direction * Depth;
                        ElbowPush += Push * (bUpper ? 1.f : .65f);
                        WristPush += Push * (bUpper ? .55f : 1.f);
                    }
                };
                Separate(A, B, true);
                Separate(B, C, false);
                if (Greatest < 2.f) continue;
                WristPush = WristPush.GetClampedToMaxSize(95.f);
                ElbowPush = ElbowPush.GetClampedToMaxSize(95.f);
                Clearances.Add({Upper, Fore, Hand,
                    ComponentToWorld.InverseTransformPosition(C + WristPush),
                    ComponentToWorld.InverseTransformPosition(B + ElbowPush),
                    FMath::Clamp(Greatest / 14.f, 0.f, 1.f)});
            }
        if (bHasDirectRoofPlane)
        {
            // The floor stays world-up while the actor rolls onto the roof.
            FHitResult Floor;
            if (Mesh->GetWorld()->LineTraceSingleByChannel(Floor, OwnerLocation + FVector::UpVector * 12.f,
                OwnerLocation - FVector::UpVector * 500.f, ECC_Pawn, Query) && Floor.ImpactNormal.Z > .85f)
            {
                DirectFloorSurface = Floor.ImpactPoint;
                bHasDirectFloorPlane = true;
            }
        }
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        ++EvaluationSerial;
        ContactWeights.Init(0.f, 10);
        ContactPinned.Init(false, 10);
        ContactAnchorsWorld.Init(FVector::ZeroVector, 10);
        ContactNormalsWorld.Init(FVector::UpVector, 10);
        ContactAnchorErrorCm.Init(0.f, 10);
        RootTranslationRemovedCm = 0.f;
        RootRotationRemovedDeg = 0.f;
        PendingPlants.Reset();
        AdaptiveForcedReplant.Init(false, 10);
        if (ClipA)
        {
            FAnimationPoseData Target(Output);
            const FAnimExtractContext ContextA(static_cast<double>(TimeA), false);
            ClipA->GetAnimationPose(Target, ContextA);

            if (ClipB && ClipB != ClipA && GaitBlend > 0.f)
            {
                FPoseContext Other(this);
                Other.ResetToRefPose();
                FAnimationPoseData OtherData(Other);
                const FAnimExtractContext ContextB(static_cast<double>(TimeB), false);
                ClipB->GetAnimationPose(OtherData, ContextB);
                FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, OtherData, 1.f - GaitBlend);
            }

            if (bFitApplied || bCanonicalCapture)
            {
                FTransform& Basis = Output.Pose[FCompactPoseBoneIndex(0)];
                const FQuat Facing(FVector::UpVector, -HALF_PI * (bCanonicalCapture ? 1.f : FitLayerWeights.GetMax()));
                Basis.SetRotation((Facing * Basis.GetRotation()).GetNormalized());
                Basis.SetTranslation(Facing.RotateVector(Basis.GetTranslation()));
            }

            if (Action && ActionWeight > 0.f)
            {
                FPoseContext Acting(this);
                Acting.ResetToRefPose();
                FAnimationPoseData ActingData(Acting);
                const FAnimExtractContext ActionContext(static_cast<double>(ActionTime), false);
                Action->GetAnimationPose(ActingData, ActionContext);
                if (Action->GetFName() == TEXT("MIM_Surf_Pounce_FromTop")
                    || Action->GetFName() == TEXT("MIM_Surf_FloorToCeiling")
                    || Action->GetFName() == TEXT("MIM_Surf_Corner_Ambush")
                    || Action->GetFName() == TEXT("MIM_Act_Door_Squeeze")
                    || Action->GetFName() == TEXT("MIM_Act_Door_PullThrough") || bFitApplied || bCanonicalCapture)
                {
                    // These new actions use actor-forward coordinates. The
                    // imported skeleton faces +Y; correct their raw pose once,
                    // before blending with an already evaluated previous pose.
                    FTransform& Basis = Acting.Pose[FCompactPoseBoneIndex(0)];
                    const bool bFullBasis = Action->GetFName() == TEXT("MIM_Surf_Pounce_FromTop")
                        || Action->GetFName() == TEXT("MIM_Surf_FloorToCeiling")
                        || Action->GetFName() == TEXT("MIM_Surf_Corner_Ambush")
                        || Action->GetFName() == TEXT("MIM_Act_Door_Squeeze")
                        || Action->GetFName() == TEXT("MIM_Act_Door_PullThrough") || bCanonicalCapture;
                    const FQuat Facing(FVector::UpVector, -HALF_PI * (bFullBasis ? 1.f : FitLayerWeights.GetMax()));
                    Basis.SetRotation((Facing * Basis.GetRotation()).GetNormalized());
                    Basis.SetTranslation(Facing.RotateVector(Basis.GetTranslation()));
                }
                FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, ActingData, 1.f - ActionWeight);
            }

            // Navigation and traversal move and rotate the actor. Travelling
            // clips must not also move the visible skeleton away from its
            // capsule, or a climb visually teleports while path following
            // continues from the old position.
            // FBX may add an Armature bone above the authored motion root.
            const FCompactPoseBoneIndex Root = MotionRootMeshIndex != INDEX_NONE
                ? Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MotionRootMeshIndex))
                : FCompactPoseBoneIndex(INDEX_NONE);
            if (Root != INDEX_NONE && !bKeepCaptureRoot)
            {
                const FTransform& Reference = Output.Pose.GetRefPose(Root);
                FTransform RootParentToComponent = FTransform::Identity;
                const auto& RootBones = Output.Pose.GetBoneContainer();
                auto Ancestor = RootBones.GetParentBoneIndex(Root);
                while (Ancestor != INDEX_NONE)
                {
                    RootParentToComponent = RootParentToComponent * Output.Pose[Ancestor];
                    Ancestor = RootBones.GetParentBoneIndex(Ancestor);
                }
                const FVector RootShift = Output.Pose[Root].GetLocation() - Reference.GetLocation();
                RootTranslationRemovedCm = ComponentToWorld.TransformVector(
                    RootParentToComponent.TransformVector(RootShift)).Size();
                RootRotationRemovedDeg = FMath::RadiansToDegrees(
                    Output.Pose[Root].GetRotation().AngularDistance(Reference.GetRotation()));
                Output.Pose[Root] = Reference;
            }

            // Fitted poses cannot be used as an unfitted transition source:
            // blending their rotations after fitting can leave the aperture.
            // Keep an independent raw history, blend that first, and fit once.
            const bool bRawTransition = TransitionBlend < 1.f
                && SourceRawPose.Num() == Output.Pose.GetNumBones();
            TArray<FTransform> RawTarget;
            FBlendedHeapCurve RawTargetCurve;
            const bool bRestoreRawTarget = bRawTransition && !bFitApplied;
            if (bRestoreRawTarget)
            {
                RawTarget.SetNum(Output.Pose.GetNumBones());
                for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                    RawTarget[Index.GetInt()] = Output.Pose[Index];
                RawTargetCurve.CopyFrom(Output.Curve);
            }
            const FTransform CurrentFitBasis = Output.Pose[FCompactPoseBoneIndex(0)];
            if (bRawTransition)
            {
                FPoseContext RawPrevious(this); RawPrevious.ResetToRefPose();
                for (const FCompactPoseBoneIndex Index : RawPrevious.Pose.ForEachBoneIndex())
                    RawPrevious.Pose[Index] = SourceRawPose[Index.GetInt()];
                RawPrevious.Curve.CopyFrom(SourceRawCurve);
                FAnimationPoseData RawPreviousData(RawPrevious);
                FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, RawPreviousData, TransitionBlend);
                // Geometry owns the current fit basis; a previous open pose
                // must not rotate the whole raw transition away from it.
                if (bFitApplied) Output.Pose[FCompactPoseBoneIndex(0)] = CurrentFitBasis;
            }
            LastRawPose.SetNum(Output.Pose.GetNumBones());
            for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                LastRawPose[Index.GetInt()] = Output.Pose[Index];
            LastRawCurve.CopyFrom(Output.Curve);
            if (bRestoreRawTarget)
            {
                for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                    Output.Pose[Index] = RawTarget[Index.GetInt()];
                Output.Curve.CopyFrom(RawTargetCurve);
            }
            ApplyAdaptiveFit(Output);

            if (!bFitApplied && TransitionBlend < 1.f && SourcePose.Num() == Output.Pose.GetNumBones())
            {
                FPoseContext Previous(this);
                Previous.ResetToRefPose();
                for (const FCompactPoseBoneIndex Index : Previous.Pose.ForEachBoneIndex())
                    Previous.Pose[Index] = SourcePose[Index.GetInt()];
                Previous.Curve.CopyFrom(SourceCurve);
                FAnimationPoseData PreviousData(Previous);
                FAnimationRuntime::BlendTwoPosesTogetherInPlace(Target, PreviousData, TransitionBlend);
            }

            if (!ArmBindings.IsEmpty())
            {
                const auto& Bones = Output.Pose.GetBoneContainer();
                TArray<FTransform> CS;
                CS.SetNum(Output.Pose.GetNumBones());
                auto Rebuild = [&]()
                {
                    for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                    {
                        const auto Parent = Bones.GetParentBoneIndex(Index);
                        CS[Index.GetInt()] = Parent != INDEX_NONE
                            ? Output.Pose[Index] * CS[Parent.GetInt()] : Output.Pose[Index];
                    }
                };
                auto Compact = [&](int32 Index)
                {
                    return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
                        : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
                };
                Rebuild();
                const auto Head = Compact(HeadMeshIndex);
                if (Head != INDEX_NONE)
                {
                    FTransform HeadCS = CS[Head.GetInt()];
                    HeadAuthoredDirection = ComponentToWorld.TransformVectorNoScale(
                        HeadCS.GetRotation().RotateVector(HeadFaceAxis)).GetSafeNormal();
                    if (HeadLookWeight > .001f && !LookWorld.IsNearlyZero() && !HeadFaceAxis.IsNearlyZero())
                    {
                        const FVector Current = HeadCS.GetRotation().RotateVector(HeadFaceAxis).GetSafeNormal();
                        const FVector Desired = ComponentToWorld.InverseTransformVectorNoScale(LookWorld).GetSafeNormal();
                        FQuat Delta = FQuat::FindBetweenNormals(Current, Desired);
                        HeadCS.SetRotation((FQuat::Slerp(FQuat::Identity, Delta, HeadLookWeight)
                            * HeadCS.GetRotation()).GetNormalized());
                        const auto Parent = Bones.GetParentBoneIndex(Head);
                        Output.Pose[Head] = Parent != INDEX_NONE
                            ? HeadCS.GetRelativeTransform(CS[Parent.GetInt()]) : HeadCS;
                        Rebuild();
                    }
                    HeadLookDirection = ComponentToWorld.TransformVectorNoScale(
                        CS[Head.GetInt()].GetRotation().RotateVector(HeadFaceAxis)).GetSafeNormal();
                }
                if (bResetContacts)
                {
                    for (FHandContact& Contact : HandContacts)
                    {
                        if (Contact.bPinned) ++ContactReleaseCount;
                        Contact.bPinned = false;
                        Contact.bAwaitingLift = false;
                        Contact.bSawLift = false;
                        Contact.bApproachAnchor = false;
                    }
                }
                const FTransform DirectRoofFrame(FRotationMatrix::MakeFromZ(DirectRoofNormal).ToQuat(), DirectRoofSurface);
                const FTransform InverseDirectRoof = DirectRoofFrame.Inverse();
                auto MeasureDirectPart = [&](int32 Bone, bool bMass, const FTransform& FrameInverse,
                    bool bIncludeDescendants = false)
                {
                    TArray<FTransform> MeshPose;
                    MeshPose.Init(FTransform::Identity, HeadMesh->GetRefSkeleton().GetNum());
                    for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                    {
                        const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
                        if (MeshPose.IsValidIndex(MeshIndex)) MeshPose[MeshIndex] = CS[Index.GetInt()] * FrameInverse;
                    }
                    return FitGeometry->MeasureAdaptiveFitBounds(MeshPose, Bone, INDEX_NONE,
                        bMass, bIncludeDescendants, false);
                };
                if (bHasDirectRoofPlane && FitGeometry && HeadMesh)
                {
                    const FBox Mass = MeasureDirectPart(INDEX_NONE, true, ComponentToWorld * InverseDirectRoof);
                    const auto Body = Compact(HeadMesh->GetRefSkeleton().FindBoneIndex(TEXT("body")));
                    if (Mass.IsValid && Mass.Min.Z < 2.f && Body != INDEX_NONE)
                    {
                        // A lower real ceiling requires a tuck, not smaller
                        // anatomy. The following hands solve back to their grips.
                        FTransform At = CS[Body.GetInt()];
                        At.AddToTranslation(ComponentToWorld.InverseTransformVector(
                            DirectRoofNormal * (2.f - Mass.Min.Z)));
                        const auto BodyParent = Bones.GetParentBoneIndex(Body);
                        Output.Pose[Body] = BodyParent != INDEX_NONE
                            ? At.GetRelativeTransform(CS[BodyParent.GetInt()]) : At;
                        Rebuild();
                    }
                }
                auto SolveRoofArm = [&](const FArmBinding& Arm, FTransform& A, FTransform& B, FTransform& C,
                    const FVector& PreferredPole, const FVector& Goal)
                {
                    if (!bHasDirectRoofPlane || !FitGeometry || !HeadMesh)
                    {
                        AnimationCore::SolveTwoBoneIK(A, B, C, PreferredPole, Goal, false, 1.f, 1.f);
                        return;
                    }
                    const FBox UpperLocal = MeasureDirectPart(Arm.Upper, false, A.Inverse());
                    const FBox ForeLocal = MeasureDirectPart(Arm.Fore, false, B.Inverse());
                    const FTransform InitialA = A, InitialB = B, InitialC = C;
                    float BestViolation = BIG_NUMBER, BestDistance = BIG_NUMBER;
                    auto TryPole = [&](const FVector& AtPole)
                    {
                        FTransform FitA = InitialA, FitB = InitialB, FitC = InitialC;
                        AnimationCore::SolveTwoBoneIK(FitA, FitB, FitC, AtPole, Goal, false, 1.f, 1.f);
                        FitC.SetRotation(InitialC.GetRotation());
                        const FBox UpperSkin = UpperLocal.TransformBy((FitA * ComponentToWorld * InverseDirectRoof).ToMatrixWithScale());
                        const FBox ForeSkin = ForeLocal.TransformBy((FitB * ComponentToWorld * InverseDirectRoof).ToMatrixWithScale());
                        const float V = (UpperSkin.IsValid ? FMath::Square(FMath::Max(0.f, 2.f - UpperSkin.Min.Z)) : 0.f)
                            + (ForeSkin.IsValid ? FMath::Square(FMath::Max(0.f, 2.f - ForeSkin.Min.Z)) : 0.f);
                        const float Move = FVector::DistSquared(FitB.GetLocation(), InitialB.GetLocation());
                        if (V < BestViolation - .001f || (FMath::Abs(V - BestViolation) <= .001f && Move < BestDistance))
                        { BestViolation = V; BestDistance = Move; A = FitA; B = FitB; C = FitC; }
                    };
                    TryPole(PreferredPole);
                    if (BestViolation < .001f) return;
                    const float Lu = FVector::Dist(InitialA.GetLocation(), InitialB.GetLocation());
                    const float Lf = FVector::Dist(InitialB.GetLocation(), InitialC.GetLocation());
                    const FVector Along = Goal - InitialA.GetLocation();
                    const float Distance = Along.Size();
                    if (Distance <= FMath::Abs(Lu - Lf) + .01f || Distance >= Lu + Lf - .01f) return;
                    const FVector Direction = Along / Distance;
                    const float Projection = (Lu * Lu - Lf * Lf + Distance * Distance) / (2.f * Distance);
                    const FVector Center = InitialA.GetLocation() + Direction * Projection;
                    const float Radius = FMath::Sqrt(FMath::Max(0.f, Lu * Lu - Projection * Projection));
                    FVector Axis = FVector::VectorPlaneProject(InitialB.GetLocation() - Center, Direction).GetSafeNormal();
                    if (Axis.IsNearlyZero()) Axis = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
                    if (Axis.IsNearlyZero()) Axis = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
                    const FVector Other = FVector::CrossProduct(Direction, Axis).GetSafeNormal();
                    for (int32 Sample = 0; Sample < 48; ++Sample)
                    {
                        const float Angle = 2.f * PI * Sample / 48.f;
                        TryPole(Center + Radius * (Axis * FMath::Cos(Angle) + Other * FMath::Sin(Angle)));
                    }
                };
                TArray<int32> GroundPaddedHands;
                if (bGroundHandPadding && FitGeometry && HeadMesh)
                {
                    TArray<FTransform> GroundMeshPose;
                    GroundMeshPose.Init(FTransform::Identity, HeadMesh->GetRefSkeleton().GetNum());
                    for (const FArmBinding& GroundArm : ArmBindings)
                    {
                        const auto PadUpper = Compact(GroundArm.Upper), PadFore = Compact(GroundArm.Fore),
                            PadHand = Compact(GroundArm.Hand);
                        if (PadUpper == INDEX_NONE || PadFore == INDEX_NONE || PadHand == INDEX_NONE) continue;
                        for (const FCompactPoseBoneIndex PadIndex : Output.Pose.ForEachBoneIndex())
                        {
                            const int32 MeshIndex = Bones.MakeMeshPoseIndex(PadIndex).GetInt();
                            if (GroundMeshPose.IsValidIndex(MeshIndex))
                                GroundMeshPose[MeshIndex] = CS[PadIndex.GetInt()];
                        }
                        const FBox HandSkin = FitGeometry->MeasureAdaptiveFitBounds(GroundMeshPose,
                            GroundArm.Hand, INDEX_NONE, false, true, false);
                        if (!HandSkin.IsValid || HandSkin.Min.Z >= FitSupportPlane - 2.f) continue;
                        FTransform PadA = CS[PadUpper.GetInt()], PadB = CS[PadFore.GetInt()], PadC = CS[PadHand.GetInt()];
                        const FQuat WristRotation = PadC.GetRotation();
                        const FVector PadGoal = PadC.GetLocation() + FVector(0.f, 0.f,
                            FitSupportPlane + 1.f - HandSkin.Min.Z);
                        const FVector PadPole = PadB.GetLocation();
                        AnimationCore::SolveTwoBoneIK(PadA, PadB, PadC, PadPole, PadGoal, false, 1.f, 1.f);
                        PadC.SetRotation(WristRotation);
                        const auto PadParent = Bones.GetParentBoneIndex(PadUpper);
                        Output.Pose[PadUpper] = PadParent != INDEX_NONE
                            ? PadA.GetRelativeTransform(CS[PadParent.GetInt()]) : PadA;
                        Output.Pose[PadFore] = PadB.GetRelativeTransform(PadA);
                        Output.Pose[PadHand] = PadC.GetRelativeTransform(PadB);
                        Rebuild();
                        GroundPaddedHands.Add(GroundArm.Hand);
                    }
                }
                TArray<int32> ApproachHands;
                for (const FArmBinding& Arm : ArmBindings)
                {
                    const auto Upper = Compact(Arm.Upper), Fore = Compact(Arm.Fore), Hand = Compact(Arm.Hand);
                    if (Upper == INDEX_NONE || Fore == INDEX_NONE || Hand == INDEX_NONE) continue;
                    FHandContact& Contact = HandContacts[Arm.ContactIndex];
                    bool bNewAudiblePlant = false;
                    const float Curve = FMath::Clamp(Output.Curve.Get(Arm.Curve), 0.f, 1.f);
                    ContactWeights[Arm.ContactIndex] = Curve;
                    if ((bFitApplied || GroundPaddedHands.Contains(Arm.Hand)) && Contact.bPinned && !Arm.bHasApproach)
                    {
                        // Keep the tangential world plant, but allow the soft
                        // hand to roll and lift to its corrected skin padding.
                        const FVector Fitted = ComponentToWorld.TransformPosition(CS[Hand.GetInt()].GetLocation());
                        Contact.AnchorWorld += SupportNormal * FVector::DotProduct(Fitted - Contact.AnchorWorld, SupportNormal);
                        Contact.RotationWorld = ComponentToWorld.GetRotation() * CS[Hand.GetInt()].GetRotation();
                        const FVector Anchor = ComponentToWorld.InverseTransformPosition(Contact.AnchorWorld);
                        if (bFitApplied && ((AdaptiveForcedReplant.IsValidIndex(Arm.ContactIndex)
                                && AdaptiveForcedReplant[Arm.ContactIndex])
                            || FMath::Abs(Anchor.Y) > FitClearance.Y * .5f - 10.f
                            || Anchor.Z > FitClearance.Z - 10.f
                            || FVector::Dist(Anchor, CS[Hand.GetInt()].GetLocation()) > 20.f))
                        {
                            Contact.bPinned = Contact.bAwaitingLift = Contact.bApproachAnchor = false;
                            Contact.bSawLift = true;
                            ++ContactReleaseCount; ++FitReplants;
                        }
                    }
                    if (bCheckedPounce)
                    {
                        // The complete authored flight was checked against the
                        // world before launch. Old gait pins or clearance from
                        // the previous pose must not deform that trajectory.
                        if (Contact.bPinned) ++ContactReleaseCount;
                        Contact.bPinned = Contact.bAwaitingLift = Contact.bApproachAnchor = false;
                        Contact.bSawLift = Curve <= .35f;
                        continue;
                    }
                    const float ClipPhase = Action ? ActionTime / FMath::Max(Action->GetPlayLength(), .01f) : 0.f;
                    const float ApproachWeight = FMath::SmoothStep(.03f, .30f, ClipPhase);
                    bool bApproach = Arm.bHasApproach && ApproachWeight > .001f
                        && (Curve < .65f || Contact.bSawLift || Contact.bApproachAnchor || Arm.bCornerApproach
                            || (Arm.bSurfaceTransfer && ApproachWeight >= .95f
                                && FVector::DotProduct(Contact.NormalWorld, Arm.ApproachNormal) < .9f));
                    FVector PhysicalApproachWorld = Arm.ApproachWorld;
                    FVector DirectRoofPoint = FVector::ZeroVector;
                    float DirectHandStandOff = 0.f;
                    bool bMeasuredDirectHand = false;
                    if (bApproach && Action && Action->GetFName() == TEXT("MIM_Surf_FloorToCeiling")
                        && Arm.ApproachNormal.Z < -.5f && FitGeometry && HeadMesh)
                    {
                        // Early reaching fingers are deeper than the final flat
                        // grip. Measure their current intact skin along the real
                        // roof normal instead of using the final wrist pad alone.
                        const float FlatStandOff = UTallowHunterTraversalComponent::GetDirectCeilingWristStandOff(Arm.ContactIndex);
                        DirectRoofPoint = Arm.ApproachWorld - Arm.ApproachNormal * FlatStandOff;
                        const FTransform RoofFrame(FRotationMatrix::MakeFromZ(Arm.ApproachNormal).ToQuat(), DirectRoofPoint);
                        const FTransform InverseRoof = RoofFrame.Inverse();
                        TArray<FTransform> RoofMeshPose;
                        RoofMeshPose.Init(FTransform::Identity, HeadMesh->GetRefSkeleton().GetNum());
                        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
                        {
                            const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
                            if (RoofMeshPose.IsValidIndex(MeshIndex))
                                RoofMeshPose[MeshIndex] = CS[Index.GetInt()] * ComponentToWorld * InverseRoof;
                        }
                        const FBox HandSkin = FitGeometry->MeasureAdaptiveFitBounds(RoofMeshPose,
                            Arm.Hand, INDEX_NONE, false, true, false);
                        if (HandSkin.IsValid)
                        {
                            const FVector WristWorld = ComponentToWorld.TransformPosition(CS[Hand.GetInt()].GetLocation());
                            const float HandDepth = FVector::DotProduct(WristWorld - DirectRoofPoint, Arm.ApproachNormal)
                                - HandSkin.Min.Z;
                            DirectHandStandOff = FMath::Max(FlatStandOff, HandDepth + 2.f);
                            PhysicalApproachWorld = DirectRoofPoint + Arm.ApproachNormal * DirectHandStandOff;
                            bMeasuredDirectHand = true;
                        }
                    }
                    if (bApproach)
                    {
                        FTransform A = CS[Upper.GetInt()], B = CS[Fore.GetInt()], C = CS[Hand.GetInt()];
                        const FVector ApproachTarget = ComponentToWorld.InverseTransformPosition(PhysicalApproachWorld);
                        const float UpperLength = FVector::Distance(A.GetLocation(), B.GetLocation());
                        const float LowerLength = FVector::Distance(B.GetLocation(), C.GetLocation());
                        const float Distance = FVector::Distance(A.GetLocation(), ApproachTarget);
                        bApproach = Distance < (UpperLength + LowerLength) * .985f
                            && Distance > FMath::Abs(UpperLength - LowerLength) + .5f;
                        if (bApproach)
                        {
                            FVector Goal = ApproachTarget;
                            if (Curve <= .35f && ClipPhase > .55f)
                                Goal += ComponentToWorld.InverseTransformVectorNoScale(Arm.ApproachNormal)
                                    * (FMath::Sin(FMath::Clamp((ClipPhase - .55f) / .40f, 0.f, 1.f) * PI) * 12.f);
                            FVector PoleWorld = ComponentToWorld.TransformPosition(B.GetLocation());
                            const float ElbowGap = FVector::DotProduct(PoleWorld - PhysicalApproachWorld, Arm.ApproachNormal);
                            if (ElbowGap < 14.f) PoleWorld += Arm.ApproachNormal * (14.f - ElbowGap);
                            const FVector Pole = ComponentToWorld.InverseTransformPosition(PoleWorld);
                            FVector BlendedGoal = FMath::Lerp(C.GetLocation(), Goal, ApproachWeight);
                            const FQuat AuthoredWristRotation = C.GetRotation();
                            if (bMeasuredDirectHand)
                            {
                                FVector GoalWorld = ComponentToWorld.TransformPosition(BlendedGoal);
                                const float Gap = FVector::DotProduct(GoalWorld - DirectRoofPoint, Arm.ApproachNormal);
                                if (Gap < DirectHandStandOff)
                                    GoalWorld += Arm.ApproachNormal * (DirectHandStandOff - Gap);
                                BlendedGoal = ComponentToWorld.InverseTransformPosition(GoalWorld);
                            }
                            SolveRoofArm(Arm, A, B, C, bHasDirectRoofPlane ? B.GetLocation() : Pole, BlendedGoal);
                            if (bMeasuredDirectHand) C.SetRotation(AuthoredWristRotation);
                            const auto Parent = Bones.GetParentBoneIndex(Upper);
                            Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
                            Output.Pose[Fore] = B.GetRelativeTransform(A);
                            Output.Pose[Hand] = C.GetRelativeTransform(B);
                            Rebuild();
                            ApproachHands.Add(Arm.Hand);
                        }
                    }
                    if (bApproach && bMeasuredDirectHand && Contact.bPinned && Contact.bApproachAnchor)
                    {
                        // A rolling hand needs a changing normal pad, while its
                        // tangential planted position remains fixed on the roof.
                        Contact.AnchorWorld += Arm.ApproachNormal
                            * FVector::DotProduct(PhysicalApproachWorld - Contact.AnchorWorld, Arm.ApproachNormal);
                        Contact.RotationWorld = ComponentToWorld.GetRotation() * CS[Hand.GetInt()].GetRotation();
                    }
                    const bool bBlocked = (!bFitApplied || !bFitWithinLimits || bFitWorldContacts)
                        && !GroundPaddedHands.Contains(Arm.Hand)
                        && !bApproach && Clearances.ContainsByPredicate([&](const FArmClearance& Item)
                    {
                        return Item.Hand == Arm.Hand;
                    });
                    if (Curve <= .35f)
                    {
                        if (Contact.bPinned) ++ContactReleaseCount;
                        Contact.bPinned = false;
                        Contact.bAwaitingLift = false;
                        Contact.bSawLift = true;
                        Contact.bApproachAnchor = false;
                    }
                    else if (bBlocked)
                    {
                        if (Contact.bPinned) ++ContactReleaseCount;
                        Contact.bPinned = false;
                        Contact.bAwaitingLift = true;
                    }
                    else if ((!Contact.bPinned || (bApproach && (!Contact.bApproachAnchor
                        || ((Arm.bCornerApproach || Arm.bSurfaceTransfer) && FVector::DotProduct(Contact.NormalWorld, Arm.ApproachNormal) < .9f))))
                        && (!Contact.bAwaitingLift || bApproach) && Curve >= .65f)
                    {
                        // Capture the newly evaluated plant, not last frame's
                        // mesh position. This keeps a blend's plant continuous.
                        Contact.bPinned = true;
                        Contact.AnchorWorld = bApproach ? PhysicalApproachWorld
                            : ComponentToWorld.TransformPosition(CS[Hand.GetInt()].GetLocation());
                        Contact.RotationWorld = ComponentToWorld.GetRotation() * CS[Hand.GetInt()].GetRotation();
                        Contact.NormalWorld = bApproach ? Arm.ApproachNormal : SupportNormal;
                        Contact.bApproachAnchor = bApproach;
                        Contact.bAwaitingLift = false;
                        bNewAudiblePlant = Contact.bSawLift && !bSuppressPlantEvents;
                        Contact.bSawLift = false;
                    }
                    if (!Contact.bPinned) continue;
                    FTransform A = CS[Upper.GetInt()], B = CS[Fore.GetInt()], C = CS[Hand.GetInt()];
                    const FVector ContactTarget = ComponentToWorld.InverseTransformPosition(Contact.AnchorWorld);
                    const float Reach = FVector::Distance(A.GetLocation(), B.GetLocation())
                        + FVector::Distance(B.GetLocation(), C.GetLocation());
                    if (FVector::Distance(A.GetLocation(), ContactTarget) > Reach * 1.015f)
                    {
                        // A teleport or a malformed plant cannot drag an arm
                        // past its length. Let the next valid plant re-anchor.
                        Contact.bPinned = false;
                        Contact.bAwaitingLift = true;
                        ++ContactReleaseCount;
                        continue;
                    }
                    const FVector Pole = B.GetLocation();
                    const FQuat PlantedWristRotation = ComponentToWorld.GetRotation().Inverse() * Contact.RotationWorld;
                    if (bHasDirectRoofPlane) C.SetRotation(PlantedWristRotation);
                    SolveRoofArm(Arm, A, B, C, Pole, ContactTarget);
                    C.SetRotation(PlantedWristRotation);
                    const auto Parent = Bones.GetParentBoneIndex(Upper);
                    Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
                    Output.Pose[Fore] = B.GetRelativeTransform(A);
                    Output.Pose[Hand] = C.GetRelativeTransform(B);
                    Rebuild();
                    if (bNewAudiblePlant)
                    {
                        PendingPlants.Add({Contact.AnchorWorld, Contact.NormalWorld, ActualSpeed});
                        ++ContactPlantEventCount;
                    }
                }
                for (const FArmClearance& Arm : Clearances)
                {
                    if (bCheckedPounce || (bFitApplied && bFitWithinLimits && !bFitWorldContacts)) break;
                    if (ApproachHands.Contains(Arm.Hand) || GroundPaddedHands.Contains(Arm.Hand)) continue;
                    const auto Upper = Compact(Arm.Upper), Fore = Compact(Arm.Fore), Hand = Compact(Arm.Hand);
                    if (Upper == INDEX_NONE || Fore == INDEX_NONE || Hand == INDEX_NONE) continue;
                    FTransform A = CS[Upper.GetInt()], B = CS[Fore.GetInt()], C = CS[Hand.GetInt()];
                    const FVector Effector = FMath::Lerp(C.GetLocation(), Arm.Target, Arm.Weight);
                    const FVector Pole = FMath::Lerp(B.GetLocation(), Arm.Pole, Arm.Weight);
                    AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Effector, false, 1.f, 1.f);
                    const auto Parent = Bones.GetParentBoneIndex(Upper);
                    Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
                    Output.Pose[Fore] = B.GetRelativeTransform(A);
                    Output.Pose[Hand] = C.GetRelativeTransform(B);
                    Rebuild();
                }
                if (bHasDirectFloorPlane && bHasDirectRoofPlane && FitGeometry && HeadMesh)
                {
                    const FTransform InverseFloor = FTransform(FQuat::Identity, DirectFloorSurface).Inverse();
                    for (const FArmBinding& FloorArm : ArmBindings)
                    {
                        const auto FloorUpper = Compact(FloorArm.Upper), FloorFore = Compact(FloorArm.Fore),
                            FloorHand = Compact(FloorArm.Hand);
                        if (FloorUpper == INDEX_NONE || FloorFore == INDEX_NONE || FloorHand == INDEX_NONE) continue;
                        const FBox HandSkin = MeasureDirectPart(FloorArm.Hand, false,
                            ComponentToWorld * InverseFloor, true);
                        if (!HandSkin.IsValid || HandSkin.Min.Z >= -2.f) continue;
                        FTransform A = CS[FloorUpper.GetInt()], B = CS[FloorFore.GetInt()], C = CS[FloorHand.GetInt()];
                        const FQuat WristRotation = C.GetRotation();
                        const FVector Goal = C.GetLocation() + ComponentToWorld.InverseTransformVector(
                            FVector::UpVector * (1.f - HandSkin.Min.Z));
                        SolveRoofArm(FloorArm, A, B, C, B.GetLocation(), Goal);
                        C.SetRotation(WristRotation);
                        const auto FloorParent = Bones.GetParentBoneIndex(FloorUpper);
                        Output.Pose[FloorUpper] = FloorParent != INDEX_NONE
                            ? A.GetRelativeTransform(CS[FloorParent.GetInt()]) : A;
                        Output.Pose[FloorFore] = B.GetRelativeTransform(A);
                        Output.Pose[FloorHand] = C.GetRelativeTransform(B);
                        Rebuild();
                        FHandContact& Contact = HandContacts[FloorArm.ContactIndex];
                        if (Contact.bPinned && Contact.NormalWorld.Z > .85f)
                        {
                            const FVector WristWorld = ComponentToWorld.TransformPosition(C.GetLocation());
                            Contact.AnchorWorld.Z = WristWorld.Z;
                        }
                    }
                }
                if (bCaptureShot)
                    for (const FArmBinding& Arm : ArmBindings)
                    {
                        if (Arm.ContactIndex > 3) continue;
                        const float Weight = CaptureGripWeights[Arm.ContactIndex];
                        if (Weight <= .001f) continue;
                        const auto Upper = Compact(Arm.Upper), Fore = Compact(Arm.Fore), Hand = Compact(Arm.Hand);
                        if (Upper == INDEX_NONE || Fore == INDEX_NONE || Hand == INDEX_NONE) continue;
                        FHandContact& Contact = HandContacts[Arm.ContactIndex];
                        Contact.bPinned = Contact.bAwaitingLift = false;
                        FTransform A = CS[Upper.GetInt()], B = CS[Fore.GetInt()], C = CS[Hand.GetInt()];
                        const FVector Goal = ComponentToWorld.InverseTransformPosition(CaptureGripTargets[Arm.ContactIndex]);
                        const float Reach = FVector::Distance(A.GetLocation(), B.GetLocation())
                            + FVector::Distance(B.GetLocation(), C.GetLocation());
                        const FVector Finite = A.GetLocation() + (Goal - A.GetLocation()).GetClampedToMaxSize(Reach * .985f);
                        const FVector Effector = FMath::Lerp(C.GetLocation(), Finite, Weight);
                        const FVector Pole = (A.GetLocation() + Effector) * .5f
                            + ComponentToWorld.InverseTransformVectorNoScale(CaptureGripBends[Arm.ContactIndex]) * 100.f;
                        const FQuat Wrist = C.GetRotation();
                        AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Effector, false, 1.f, 1.f);
                        C.SetRotation(Wrist);
                        const auto Parent = Bones.GetParentBoneIndex(Upper);
                        Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(CS[Parent.GetInt()]) : A;
                        Output.Pose[Fore] = B.GetRelativeTransform(A);
                        Output.Pose[Hand] = C.GetRelativeTransform(B);
                        Rebuild();
                    }
                for (const FArmBinding& Arm : ArmBindings)
                {
                    const auto Hand = Compact(Arm.Hand);
                    const FHandContact& Contact = HandContacts[Arm.ContactIndex];
                    ContactPinned[Arm.ContactIndex] = Contact.bPinned;
                    ContactAnchorsWorld[Arm.ContactIndex] = Contact.AnchorWorld;
                    ContactNormalsWorld[Arm.ContactIndex] = Contact.NormalWorld;
                    if (Contact.bPinned && Hand != INDEX_NONE)
                        ContactAnchorErrorCm[Arm.ContactIndex] = FVector::Distance(Contact.AnchorWorld,
                            ComponentToWorld.TransformPosition(CS[Hand.GetInt()].GetLocation()));
                }
            }
        }
        else
        {
            for (FHandContact& Contact : HandContacts)
            {
                if (Contact.bPinned) ++ContactReleaseCount;
                Contact.bPinned = false;
                Contact.bAwaitingLift = false;
                Contact.bSawLift = false;
            }
        }
        bResetContacts = false;

        Output.Pose.NormalizeRotations();
        ValidateAdaptiveFit(Output);
        LastPose.SetNum(Output.Pose.GetNumBones());
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
            LastPose[Index.GetInt()] = Output.Pose[Index];
        LastCurve.CopyFrom(Output.Curve);
        return true;
    }
};

UTallowMimAnimInstance::UTallowMimAnimInstance()
{
    RootMotionMode = ERootMotionMode::NoRootMotionExtraction;
}

void UTallowMimAnimInstance::SetLocomotion(UAnimSequence* A, UAnimSequence* B, float Blend, float TimeA, float TimeB)
{
    if (GaitA != A || GaitB != B)
    {
        GaitA = A;
        GaitB = B;
        const bool bFullPounce = ActionClip && ActionAlpha > .99f
            && ActionClip->GetFName() == TEXT("MIM_Surf_Pounce_FromTop");
        if (!bFullPounce)
        {
            BlendAge = 0.f;
            ++BaseSerial;
        }
    }
    GaitAlpha = FMath::Clamp(Blend, 0.f, 1.f);
    GaitTimeA = FMath::Max(0.f, TimeA);
    GaitTimeB = FMath::Max(0.f, TimeB);
}

void UTallowMimAnimInstance::BlendFromCurrentPose()
{
    BlendAge = 0.f;
    ++BaseSerial;
}

void UTallowMimAnimInstance::SetAction(UAnimSequence* Clip, float Time, float Weight)
{
    if (ActionClip != Clip)
    {
        ActionClip = Clip;
        BlendAge = 0.f;
        ++BaseSerial;
    }
    ActionTime = FMath::Max(0.f, Time);
    ActionAlpha = FMath::Clamp(Weight, 0.f, 1.f);
}

void UTallowMimAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    BlendAge += FMath::Max(0.f, DeltaSeconds);
    if ((!HasAdaptiveFitLayer() || !FitUprightClip) && GetWorld() && GetWorld()->GetTimeSeconds() >= NextFitReferenceLoad)
    {
        NextFitReferenceLoad = GetWorld()->GetTimeSeconds() + 1.f;
        if (!FitNeutralClip) FitNeutralClip = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Characters/Mim/Anims/MIM_Loco_Idle.MIM_Loco_Idle"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!FitTallClip) FitTallClip = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Characters/Mim/Anims/MIM_Pose_DoorTallFold.MIM_Pose_DoorTallFold"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!FitUprightClip) FitUprightClip = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Characters/Mim/Anims/MIM_Act_Upright_Idle.MIM_Act_Upright_Idle"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!FitLowClip) FitLowClip = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Characters/Mim/Anims/MIM_Surf_Crawl_Loop.MIM_Surf_Crawl_Loop"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
}

void UTallowMimAnimInstance::NativePostEvaluateAnimation()
{
    Super::NativePostEvaluateAnimation();
    FTallowMimAnimProxy& Proxy = GetProxyOnGameThread<FTallowMimAnimProxy>();
    AdaptiveFitApplied = Proxy.bFitApplied;
    AdaptiveFitWithinLimits = Proxy.bFitWithinLimits;
    AdaptiveFitUprightWeight = Proxy.FitUprightAlpha;
    AdaptiveFitWeights = Proxy.FitLayerWeights;
    AdaptiveFitBodyScale = Proxy.FitBodyScale;
    AdaptiveFitReplants = Proxy.FitReplants;
    AdaptiveFitMilliseconds = Proxy.FitEvaluationMilliseconds;
    ContactWeights = Proxy.ContactWeights;
    ContactPinned = Proxy.ContactPinned;
    ContactAnchorsWorld = Proxy.ContactAnchorsWorld;
    ContactNormalsWorld = Proxy.ContactNormalsWorld;
    ContactAnchorErrorCm = Proxy.ContactAnchorErrorCm;
    ContactReleaseCount = Proxy.ContactReleaseCount;
    ContactPlantEventCount = Proxy.ContactPlantEventCount;
    ContactEvaluationSerial = Proxy.EvaluationSerial;
    ContactEvaluationComponentLocation = Proxy.ComponentToWorld.GetLocation();
    RootTranslationRemovedCm = Proxy.RootTranslationRemovedCm;
    RootRotationRemovedDeg = Proxy.RootRotationRemovedDeg;
    HeadLookDirection = Proxy.HeadLookDirection;
    HeadDesiredDirection = Proxy.LookWorld;
    HeadAuthoredDirection = Proxy.HeadAuthoredDirection;
    HeadLookWeight = Proxy.HeadLookWeight;
    if (USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
    {
        Mesh->SetScalarParameterValueOnMaterials(TEXT("Irises"),
            FMath::Clamp(GetCurveValue(TEXT("Irises")), 0.f, 1.f));
        if (const auto* Pawn = Cast<APawn>(Mesh->GetOwner()))
            if (auto* Brain = Cast<ATallowHunterAIController>(Pawn->GetController()))
            {
                const float Now = Mesh->GetWorld()->GetTimeSeconds();
                if (Now >= NextEyeshineSample)
                {
                    const float Shadow = Brain->GetShadowLevelAt(Mesh->GetSocketLocation(TEXT("head")));
                    EyeshineTarget = .0245f * Shadow * Shadow * Shadow;
                    NextEyeshineSample = Now + .25f;
                }
                // Standing off in the dark watching her, the two glints catch whatever light there is: the eyes in the
                // doorway are what she sees first (the stalk, 28 Oct).
                const float Target = FMath::Max(EyeshineTarget, Brain->GetWatchGlint() * WatchEyeshine);
                Eyeshine = FMath::FInterpTo(Eyeshine, Target, Mesh->GetWorld()->GetDeltaSeconds(), 8.f);
                Mesh->SetScalarParameterValueOnMaterials(TEXT("Eyeshine"), Eyeshine);
            }
        if (AActor* Owner = Mesh->GetOwner())
            if (auto* Motion = Owner->FindComponentByClass<UTallowHunterMotionComponent>())
                for (const FTallowMimAnimProxy::FPlantEvent& Plant : Proxy.PendingPlants)
                {
                    FVector Point = Plant.WristWorld;
                    FVector Normal = Plant.SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
                    FHitResult Hit;
                    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimPlantFoley), false, Owner);
                    if (APawn* Player = UGameplayStatics::GetPlayerPawn(Mesh, 0)) Query.AddIgnoredActor(Player);
                    if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, Point + Normal * 30.f,
                        Point - Normal * 45.f, ECC_Pawn, Query))
                    {
                        Point = Hit.ImpactPoint;
                        Normal = Hit.ImpactNormal;
                    }
                    ++Motion->TotalPlants;
                    Motion->PlayPlant(Point, Normal.Z < .6f, Plant.Speed);
                }
    }
    Proxy.PendingPlants.Reset();
}

FAnimInstanceProxy* UTallowMimAnimInstance::CreateAnimInstanceProxy() { return new FTallowMimAnimProxy(this); }
void UTallowMimAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
