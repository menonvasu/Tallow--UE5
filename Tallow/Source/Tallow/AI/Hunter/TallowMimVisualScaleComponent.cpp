#include "AI/Hunter/TallowMimVisualScaleComponent.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterCreature.h"
#include "AI/Hunter/TallowHunterMotionComponent.h"
#include "AI/Hunter/TallowHunterTraversalComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MemStack.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"

static TAutoConsoleVariable<float> CVarMimVisualScale(TEXT("tallow.Mim.VisualScale"), 0.f,
    TEXT("Mim's drawn size over the body her movement plans for. 0 uses the creature's MimVisualScale; 1 draws her at her planned size."));

UTallowMimVisualScaleComponent::UTallowMimVisualScaleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTallowMimVisualScaleComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Creature = Cast<ATallowHunterCreature>(GetOwner());
    if (!Creature || !Creature->GetMesh()) { SetComponentTickEnabled(false); return; }
    // After her traversal and motion have planned and fitted this frame, before her animation is evaluated.
    if (Creature->HunterTraversal) AddTickPrerequisiteComponent(Creature->HunterTraversal);
    if (Creature->HunterMotion) AddTickPrerequisiteComponent(Creature->HunterMotion);
    Creature->GetMesh()->AddTickPrerequisiteComponent(this);
    WorldTickHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UTallowMimVisualScaleComponent::Restore);
}

void UTallowMimVisualScaleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FWorldDelegates::OnWorldTickStart.Remove(WorldTickHandle);
    Restore(GetWorld(), LEVELTICK_All, 0.f);
    Super::EndPlay(EndPlayReason);
}

void UTallowMimVisualScaleComponent::Restore(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
    if (World != GetWorld() || !bDrawn) return;
    bDrawn = false;
    const auto* Creature = Cast<ACharacter>(GetOwner());
    // Only our own change comes off: anything that set her mesh since keeps what it set.
    if (USkeletalMeshComponent* Mesh = Creature ? Creature->GetMesh() : nullptr;
        Mesh && Mesh->GetRelativeScale3D().Equals(DrawnScale, 1.e-4f))
        Mesh->SetRelativeScale3D(PlannedScale);
}

void UTallowMimVisualScaleComponent::TickComponent(float DeltaSeconds, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
    auto* Creature = Cast<ATallowHunterCreature>(GetOwner());
    USkeletalMeshComponent* Mesh = Creature ? Creature->GetMesh() : nullptr;
    if (!Mesh) return;
    Restore(GetWorld(), TickType, DeltaSeconds);
    // The controller chooses routes and climbs from her mesh, so it also goes before the larger body.
    AActor* Controller = Creature->GetController();
    if (Controller != OrderedController.Get())
    {
        if (AActor* Previous = OrderedController.Get()) RemoveTickPrerequisiteActor(Previous);
        if (Controller) AddTickPrerequisiteActor(Controller);
        OrderedController = Controller;
    }
    const auto* Motion = Creature->HunterMotion.Get();
    const auto* Traversal = Creature->HunterTraversal.Get();
    const auto* Brain = Cast<ATallowHunterAIController>(Controller);
    const auto* Movement = Creature->GetCharacterMovement();
    const float Override = CVarMimVisualScale.GetValueOnGameThread();
    float Target = FMath::Clamp(Override > 0.f ? Override : Creature->MimVisualScale, 1.f, 1.5f);
    bool bHold = false;
    Reason.Reset();
    // Scaling about the mesh origin keeps her on the floor only where the fit layer sets that origin on it.
    if (!Motion || !Motion->HasInitialized() || !Motion->UsesMim() || !Creature->UsesCanonicalCaptureBasis())
    { Target = 1.f; Reason = TEXT("no fit layer"); }
    // The grab is staged and framed for the size she had when it began (GetCaptureStagingDistance).
    else if ((Brain && Brain->HuntMode == ETallowHuntMode::Captured)
        || Motion->GetMimActionName().ToString().StartsWith(TEXT("MIM_Cap_")))
    { bHold = true; Reason = TEXT("capture"); }
    // Climbs, pounces, squeezes and corner braces check the world for her planned body.
    else if (Traversal && Traversal->IsTraversing()) { Target = 1.f; Reason = TEXT("traversal"); }
    else if (Creature->GetActorUpVector().Z < .85f) { Target = 1.f; Reason = TEXT("off the floor"); }
    else if (Movement && Movement->IsFalling()) { bHold = true; Reason = TEXT("falling"); }
    else
    {
        // The fit layer folds the drawn body into the space it measured. What it cannot fold away, she gives up in
        // size: the largest size at which no part of her (torso, head, arm segments) comes into the world where it
        // would not at her planned size, now and a moment ahead along her heading.
        const float Fit = MeasureFit(*Creature);
        if (Fit < Target) { Target = FMath::Max(1.f, Fit); if (Reason.IsEmpty()) Reason = TEXT("space"); }
    }
    // In fast, out slowly, and only once the room has stayed open for a moment: she settles into a tight place
    // and swells back after it, without breathing in and out as her arms pass furniture. Hidden, or put down
    // somewhere new (a reset, a chapter beat, a staged grab), she is already the size that place allows.
    const float Now = GetWorld()->GetTimeSeconds();
    const FVector Here = Creature->GetActorLocation();
    if (!bHold && (Creature->IsHidden() || FVector::DistSquared(Here, LastLocation) > FMath::Square(150.f)))
    { Current = Target; FullClearSince = -1.f; }
    LastLocation = Here;
    if (Target <= Current + .01f) FullClearSince = -1.f;
    else if (FullClearSince < 0.f) FullClearSince = Now;
    if (!bHold && (Target < Current || (FullClearSince >= 0.f && Now - FullClearSince >= 1.f)))
        Current = FMath::FInterpTo(Current, Target, DeltaSeconds, Target < Current ? 14.f : 1.2f);
    if (Current < 1.0005f) { Current = 1.f; return; }
    PlannedScale = Mesh->GetRelativeScale3D();
    DrawnScale = PlannedScale * Current;
    Mesh->SetRelativeScale3D(DrawnScale);
    bDrawn = true;
}

float UTallowMimVisualScaleComponent::MeasureFit(const ATallowHunterCreature& Creature)
{
    const USkeletalMeshComponent* Mesh = Creature.GetMesh();
    const float Override = CVarMimVisualScale.GetValueOnGameThread();
    const float Largest = FMath::Clamp(Override > 0.f ? Override : Creature.MimVisualScale, 1.f, 1.5f);
    const float Body = Creature.GetActorScale3D().GetAbsMin();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimVisualScale), true, &Creature);
    Query.AddIgnoredActors(Creature.GetCapsuleComponent()->GetMoveIgnoreActors());
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Player);
    // The narrowest free width across her length and a moment ahead, at her flank and her back. Her coat and the
    // plush outgrow anything a few bones can stand for, so a doorway, a stair or a cluttered passage takes her
    // back to her planned size (which the fit layer and the movement core were built round); only open space
    // has her at her full drawn size.
    const FVector Up = Creature.GetActorUpVector();
    FVector Forward = Creature.GetVelocity().GetSafeNormal2D();
    if (Forward.IsNearlyZero()) Forward = Creature.GetActorForwardVector();
    const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
    const FVector Foot = Creature.GetActorLocation() - Up * Creature.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Reach = 200.f * Body * Largest, Lookahead = Reach + float(Creature.GetVelocity().Size2D()) * .35f;
    SpaceWidth = 800.f;
    for (const float Height : {45.f * Body, 120.f * Body})
        for (float Along = -Reach; Along <= Lookahead; Along += 30.f)
        {
            const FVector Centre = Foot + Up * Height, Station = Centre + Forward * Along;
            FHitResult Hit;
            // A station behind a wall belongs to another room.
            if (FMath::Abs(Along) > 1.f && GetWorld()->LineTraceSingleByChannel(Hit, Centre, Station, ECC_Visibility, Query)) continue;
            float Width = 0.f;
            for (const float Side : {-1.f, 1.f})
                Width += GetWorld()->LineTraceSingleByChannel(Hit, Station, Station + Right * Side * 400.f, ECC_Visibility, Query)
                    ? Hit.Distance : 400.f;
            SpaceWidth = FMath::Min(SpaceWidth, Width);
        }
    const float Narrow = NarrowWidth * Body, Open = OpenWidth * Body;
    float Fit = FMath::Lerp(1.f, Largest, FMath::SmoothStep(Narrow, Open, SpaceWidth));
    if (Fit < Largest) Reason = TEXT("narrow");
    if (Fit <= 1.001f) return 1.f;
    // Then her drawn mass (the skin-exact bounds of her torso, hood, coat and plush in the evaluated pose, as a box
    // in her own axes; the fit layer's mass) must not go deeper into anything than it does at her planned size,
    // now or a moment ahead. Her arms are solved onto their grips at the size she is drawn. Measured at ten a
    // second like the fit layer's footprint; the box follows her between measurements.
    const auto* Motion = Creature.HunterMotion.Get();
    const float Now = GetWorld()->GetTimeSeconds();
    if (Motion && (Now >= NextBodyMeasure || !BodyBox.IsValid))
    {
        NextBodyMeasure = Now + .1f;
        BodyBox = Motion->MeasureAdaptiveFitBounds(Mesh->GetComponentSpaceTransforms(), INDEX_NONE, INDEX_NONE,
            true, true, false);
    }
    if (!BodyBox.IsValid) return Fit;
    const FTransform Frame = Mesh->GetComponentTransform();
    FBox Box = BodyBox;
    // Off the floor she already stands on: scaling about her floor point never takes her into it.
    Box.Min.Z = FMath::Min(Box.Max.Z, Box.Min.Z + 3.f / FMath::Max(float(Frame.GetScale3D().Z), .01f));
    const FVector Origin = Frame.GetLocation(), Middle = Frame.TransformPosition(Box.GetCenter());
    const FVector Extent = Box.GetExtent() * Frame.GetScale3D().GetAbs();
    const FVector Shifts[2] = {FVector::ZeroVector, Creature.GetVelocity() * .15f};
    FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllStaticObjects);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    auto Depths = [&](float Scale, const FVector& Shift, TMap<UPrimitiveComponent*, float>& Out)
    {
        const FVector At = Origin + (Middle - Origin) * Scale + Shift;
        const FCollisionShape Shape = FCollisionShape::MakeBox(Extent * Scale);
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByObjectType(Overlaps, At, Frame.GetRotation(), Objects, Shape, Query);
        for (const FOverlapResult& Overlap : Overlaps)
        {
            UPrimitiveComponent* Other = Overlap.GetComponent();
            if (!Other || Other->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
            FMTDResult Push;
            // A shape it cannot measure counts as deep as the box itself.
            const float Depth = Other->ComputePenetration(Push, Shape, At, Frame.GetRotation()) ? Push.Distance : Extent.GetMax() * Scale;
            Out.FindOrAdd(Other) = Depth;
        }
    };
    TMap<UPrimitiveComponent*, float> Planned[2];
    for (int32 Ahead = 0; Ahead < 2; ++Ahead) Depths(1.f, Shifts[Ahead], Planned[Ahead]);
    for (const float Scale : {Fit, FMath::Lerp(1.f, Fit, .5f)})
    {
        bool bClear = true;
        for (int32 Ahead = 0; Ahead < 2 && bClear; ++Ahead)
        {
            TMap<UPrimitiveComponent*, float> Drawn;
            Depths(Scale, Shifts[Ahead], Drawn);
            for (const auto& Hit : Drawn)
            {
                const float* Before = Planned[Ahead].Find(Hit.Key);
                if (Hit.Value > (Before ? *Before : 0.f) + 2.f) { bClear = false; Reason = TEXT("body"); break; }
            }
        }
        if (bClear) return Scale;
    }
    return 1.f;
}

FTallowMimSkinAudit UTallowMimVisualScaleComponent::AuditDrawnSkin(int32 VertexStride, float MinDepth) const
{
    FTallowMimSkinAudit Audit;
    const auto* Creature = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent* Mesh = Creature ? Creature->GetMesh() : nullptr;
    const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    // GetCPUSkinnedVertices needs a render mesh (none in a NullRHI run).
    if (!Asset || !Mesh->GetMeshObject()) return Audit;
    const FSkeletalMeshRenderData* Render = Asset->GetResourceForRendering();
    const FSkinWeightVertexBuffer* Weights = Mesh->GetSkinWeightBuffer(0);
    if (!Render || Render->LODRenderData.IsEmpty() || !Weights || !Weights->GetNeedsCPUAccess()) return Audit;
    FMemMark AnimationMemory(FMemStack::Get());
    TArray<FFinalSkinVertex> Skinned;
    Mesh->GetCPUSkinnedVertices(Skinned, 0);
    const FTransform World = Mesh->GetComponentTransform();
    const auto& Bones = Mesh->GetComponentSpaceTransforms();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MimSkinAudit), true, Creature);
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Player);
    const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    const FVector Eye = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
    TMap<FString, int32> Blockers;
    const int32 Step = FMath::Max(1, VertexStride);
    for (const auto& Section : Render->LODRenderData[0].RenderSections)
    {
        const bool bFur = GetNameSafe(Mesh->GetMaterial(Section.MaterialIndex)).Contains(TEXT("FurCard"));
        for (uint32 V = Section.BaseVertexIndex; V < Section.BaseVertexIndex + Section.NumVertices; V += Step)
        {
            if (!Skinned.IsValidIndex(int32(V))) break;
            // The vertex's heaviest bone is inside her body by construction.
            uint32 Offset = 0, Count = 0, Heaviest = 0;
            int32 Bone = INDEX_NONE;
            Weights->GetVertexInfluenceOffsetCount(V, Offset, Count);
            for (uint32 I = 0; I < Count; ++I)
            {
                const uint32 Weight = Weights->GetBoneWeight(V, I);
                const int32 SectionBone = Weights->GetBoneIndex(V, I);
                if (Weight > Heaviest && Section.BoneMap.IsValidIndex(SectionBone)) { Heaviest = Weight; Bone = Section.BoneMap[SectionBone]; }
            }
            if (!Bones.IsValidIndex(Bone)) continue;
            ++Audit.Vertices;
            const FVector Point = World.TransformPosition(FVector(Skinned[V].Position));
            const FVector From = World.TransformPosition(Bones[Bone].GetLocation());
            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(Hit, From, Point, ECC_Visibility, Query)) continue;
            const float Depth = float(FVector::Dist(From, Point)) - Hit.Distance;
            if (Depth <= MinDepth) continue;
            ++Audit.Clipped; Audit.ClippedFur += bFur;
            Audit.WorstDepth = FMath::Max(Audit.WorstDepth, Depth);
            ++Blockers.FindOrAdd(GetNameSafe(Hit.GetActor()) + TEXT(".") + GetNameSafe(Hit.GetComponent()));
            FHitResult Sight;
            if (!Camera || GetWorld()->LineTraceSingleByChannel(Sight, Eye, Point, ECC_Visibility, Query)) continue;
            ++Audit.Visible; Audit.VisibleFur += bFur;
            Audit.WorstVisibleDepth = FMath::Max(Audit.WorstVisibleDepth, Depth);
            if (Audit.VisibleExamples.Num() < 6) Audit.VisibleExamples.Add(Point);
        }
    }
    Blockers.ValueSort([](int32 A, int32 B) { return A > B; });
    for (const auto& Blocker : Blockers)
    {
        Audit.Against.Add(FString::Printf(TEXT("%s x%d"), *Blocker.Key, Blocker.Value));
        if (Audit.Against.Num() >= 4) break;
    }
    return Audit;
}
