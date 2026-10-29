// Included inside FTallowMimAnimProxy. Fits the raw current pose once, before
// cached-pose transition blending. Curves, gait time and all rigid face scales
// stay owned by the original animation.
UAnimSequence* FitNeutralClip = nullptr;
UAnimSequence* FitTallClip = nullptr;
UAnimSequence* FitUprightClip = nullptr;
UAnimSequence* FitLowClip = nullptr;
TArray<FTransform> FitNeutralLocal, FitTallLocal, FitLowLocal, FitUprightLocal;
float FitUprightAlpha = 0.f;
FVector FitClearance = FVector(800.f, 800.f, 350.f);
FVector FitLayerWeights = FVector::ZeroVector;
FVector FitBodyScale = FVector::OneVector;
bool bFitApplied = false, bFitWithinLimits = true;
bool bFitWorldContacts = false;
int32 FitReplants = 0;
float FitEvaluationMilliseconds = 0.f;
const UTallowHunterMotionComponent* FitGeometry = nullptr;
float FitSupportPlane = -2.f;

void ApplyAdaptiveFit(FPoseContext& Output)
{
    if (!bFitApplied || !FitNeutralClip || !FitTallClip || !FitLowClip) return;
    const double Began = FPlatformTime::Seconds();
    const auto& Bones = Output.Pose.GetBoneContainer();
    const int32 Count = Output.Pose.GetNumBones();
    const auto Root = MotionRootMeshIndex != INDEX_NONE
        ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MotionRootMeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
    auto Build = [&](const TArray<FTransform>& Local, TArray<FTransform>& CS)
    {
        CS.SetNum(Count);
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
        {
            const auto Parent = Bones.GetParentBoneIndex(Index);
            CS[Index.GetInt()] = Parent != INDEX_NONE
                ? Local[Index.GetInt()] * CS[Parent.GetInt()] : Local[Index.GetInt()];
        }
    };
    auto ReadReference = [&](UAnimSequence* Clip, TArray<FTransform>& Local)
    {
        FPoseContext Pose(this); Pose.ResetToRefPose();
        FAnimationPoseData Data(Pose);
        Clip->GetAnimationPose(Data, FAnimExtractContext(0.0, false));
        FTransform& Basis = Pose.Pose[FCompactPoseBoneIndex(0)];
        const FQuat Facing(FVector::UpVector, -HALF_PI);
        Basis.SetRotation((Facing * Basis.GetRotation()).GetNormalized());
        Basis.SetTranslation(Facing.RotateVector(Basis.GetTranslation()));
        if (Root != INDEX_NONE) Pose.Pose[Root] = Pose.Pose.GetRefPose(Root);
        Local.SetNum(Count);
        for (const FCompactPoseBoneIndex Index : Pose.Pose.ForEachBoneIndex()) Local[Index.GetInt()] = Pose.Pose[Index];
    };
    if (FitNeutralLocal.Num() != Count)
    {
        ReadReference(FitNeutralClip, FitNeutralLocal);
        ReadReference(FitTallClip, FitTallLocal);
        ReadReference(FitLowClip, FitLowLocal);
    }
    if (FitUprightClip && FitUprightLocal.Num() != Count)
        ReadReference(FitUprightClip, FitUprightLocal);
    TArray<FTransform> TallLocal = FitTallLocal;
    if (FitUprightLocal.Num() == Count && FitUprightAlpha > .001f)
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
        {
            const int32 I = Index.GetInt();
            const FString Name = HeadMesh->GetRefSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt()).ToString();
            if (Name == TEXT("body") || Name == TEXT("chest") || Name == TEXT("neck") || Name == TEXT("head")
                || Name.StartsWith(TEXT("abdomen_")) || Name.StartsWith(TEXT("plush_")))
                TallLocal[I].Blend(FitTallLocal[I], FitUprightLocal[I], FitUprightAlpha);
        }
    TArray<FTransform> Neutral, Tall, Low, Current;
    Build(FitNeutralLocal, Neutral); Build(FitTallLocal, Tall); Build(FitLowLocal, Low);
    TArray<FTransform> Raw; Raw.SetNum(Count);
    auto Rebuild = [&]()
    {
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex()) Raw[Index.GetInt()] = Output.Pose[Index];
        Build(Raw, Current);
    };
    auto Compact = [&](int32 Index)
    { return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)); };
    auto Named = [&](const TCHAR* Name)
    { return Compact(HeadMesh ? HeadMesh->GetRefSkeleton().FindBoneIndex(Name) : INDEX_NONE); };
    Rebuild();
    const TArray<FTransform> Unfitted = Current;
    const float Forward = FitLayerWeights.X, Side = FitLayerWeights.Y, Height = FitLayerWeights.Z;
    const float Strength = FitLayerWeights.GetMax();
    const float LowMix = FMath::Clamp((205.f - FitClearance.Z) / 95.f, 0.f, 1.f);
    const float BodyStrength = FMath::Max(FMath::SmoothStep(.28f, .95f, FMath::Max(Forward, Side)),
        FMath::SmoothStep(.0f, .70f, Height));
    for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
    {
        const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
        const FString Name = HeadMesh->GetRefSkeleton().GetBoneName(MeshIndex).ToString();
        const int32 I = Index.GetInt();
        if (Name.StartsWith(TEXT("arm")) && Name.Contains(TEXT("_d")))
        {
            // The compact reference was measured with curled fingers. Keep
            // the current gait's small curl changes, not its fully spread digits.
            const FQuat MotionDelta = FitNeutralLocal[I].GetRotation().Inverse() * Output.Pose[Index].GetRotation();
            const FQuat Curled = (TallLocal[I].GetRotation()
                * FQuat::Slerp(FQuat::Identity, MotionDelta, .10f)).GetNormalized();
            Output.Pose[Index].SetRotation(FQuat::Slerp(Output.Pose[Index].GetRotation(), Curled, Strength).GetNormalized());
        }
        if (Name.StartsWith(TEXT("plush_")))
        {
            const FVector ReferenceScale = FMath::Lerp(TallLocal[I].GetScale3D(), FitLowLocal[I].GetScale3D(), LowMix);
            const FVector Ratio = Output.Pose[Index].GetScale3D() / FitNeutralLocal[I].GetScale3D();
            Output.Pose[Index].SetScale3D(FMath::Lerp(Output.Pose[Index].GetScale3D(),
                ReferenceScale * FMath::Lerp(FVector::OneVector, Ratio, .15f), BodyStrength));
        }
    }
    for (const TCHAR* Name : {TEXT("body"), TEXT("chest"), TEXT("neck"), TEXT("head"), TEXT("abdomen_01"), TEXT("abdomen_02")})
    {
        if (bCrampedCapture && FCString::Strcmp(Name, TEXT("head")) == 0) continue;
        const auto Index = Named(Name); if (Index == INDEX_NONE) continue;
        const int32 I = Index.GetInt();
        FTransform Reference;
        Reference.Blend(TallLocal[I], FitLowLocal[I], LowMix);
        const FTransform Before = Output.Pose[Index];
        const FQuat MotionDelta = FitNeutralLocal[I].GetRotation().Inverse() * Before.GetRotation();
        const FQuat TargetRotation = (Reference.GetRotation()
            * FQuat::Slerp(FQuat::Identity, MotionDelta, .65f)).GetNormalized();
        Output.Pose[Index].SetRotation(FQuat::Slerp(Before.GetRotation(), TargetRotation, BodyStrength).GetNormalized());
        if (FCString::Strcmp(Name, TEXT("body")) == 0)
        {
            const FVector Target = Reference.GetTranslation()
                + (Before.GetTranslation() - FitNeutralLocal[I].GetTranslation()) * .35f;
            Output.Pose[Index].SetTranslation(FMath::Lerp(Before.GetTranslation(), Target, BodyStrength));
        }
    }
    Rebuild();
    const auto Head = Compact(HeadMeshIndex), Body = Named(TEXT("body"));
    if (!bCrampedCapture && Head != INDEX_NONE && Height > .5f)
    {
        FTransform Face = Current[Head.GetInt()];
        const FVector Direction = Face.GetRotation().RotateVector(HeadFaceAxis).GetSafeNormal();
        const FVector ForwardLook = FVector(1.f, Direction.Y * .2f, Direction.Z * .2f).GetSafeNormal();
        const FQuat Aim = FQuat::FindBetweenNormals(Direction, ForwardLook);
        Face.SetRotation((FQuat::Slerp(FQuat::Identity, Aim, Height) * Face.GetRotation()).GetNormalized());
        const auto Parent = Bones.GetParentBoneIndex(Head);
        Output.Pose[Head] = Parent != INDEX_NONE ? Face.GetRelativeTransform(Current[Parent.GetInt()]) : Face;
        Rebuild();
    }
    FitBodyScale = FVector(FMath::Lerp(1.f, FMath::Clamp((FitClearance.X - 4.f) / 220.f, .78f, 1.f), Forward),
        FMath::Lerp(1.f, FMath::Clamp((FitClearance.Y - 2.f) / 92.f, .78f, 1.f), Side),
        FMath::Lerp(1.f, FMath::Clamp((FitClearance.Z - 8.f) / 95.f, .78f, 1.f), Height));
    for (const TCHAR* Name : {TEXT("plush_hood"), TEXT("plush_neck"), TEXT("plush_chest"),
        TEXT("plush_body"), TEXT("plush_abd1"), TEXT("plush_abd2")})
    {
        const auto Index = Named(Name); if (Index == INDEX_NONE) continue;
        const FQuat Rotation = Current[Index.GetInt()].GetRotation();
        FVector LocalScale;
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            FVector V = FVector::ZeroVector; V[Axis] = 1.f;
            LocalScale[Axis] = (Rotation.RotateVector(V) * FitBodyScale).Size();
        }
        Output.Pose[Index].SetScale3D(Output.Pose[Index].GetScale3D() * LocalScale);
    }
    Rebuild();
    TArray<FTransform> MeshPose;
    auto RefreshMeshPose = [&]()
    {
        MeshPose.Init(FTransform::Identity, HeadMesh->GetRefSkeleton().GetNum());
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
        {
            const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
            if (MeshPose.IsValidIndex(MeshIndex)) MeshPose[MeshIndex] = Current[Index.GetInt()];
        }
    };
    if (FitGeometry && Body != INDEX_NONE)
    {
        // Fur and the soft hood need their own measured padding; the rigid
        // mask and eyes never inherit a scale from these plush controls.
        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            RefreshMeshPose();
            const FBox Mass = FitGeometry->MeasureAdaptiveFitBounds(MeshPose, INDEX_NONE, INDEX_NONE, true);
            if (!Mass.IsValid) break;
            const FVector Span = Mass.GetSize();
            const FVector Padding(1.f,
                FMath::Lerp(1.f, FMath::Clamp((FitClearance.Y - 4.f) / FMath::Max(Span.Y, 1.f), .78f, 1.f), Side),
                FMath::Lerp(1.f, FMath::Clamp((FitClearance.Z - FitSupportPlane - 4.f)
                    / FMath::Max(Span.Z, 1.f), .84f, 1.f), Height));
            if (Pass < 2 && !Padding.Equals(FVector::OneVector, .005f))
            {
                for (const TCHAR* Name : {TEXT("plush_hood"), TEXT("plush_neck"), TEXT("plush_chest"),
                    TEXT("plush_body"), TEXT("plush_abd1"), TEXT("plush_abd2")})
                {
                    const auto Index = Named(Name); if (Index == INDEX_NONE) continue;
                    const FQuat Rotation = Current[Index.GetInt()].GetRotation();
                    FVector LocalScale;
                    for (int32 Axis = 0; Axis < 3; ++Axis)
                    {
                        FVector V = FVector::ZeroVector; V[Axis] = 1.f;
                        LocalScale[Axis] = (Rotation.RotateVector(V) * Padding).Size();
                    }
                    Output.Pose[Index].SetScale3D(Output.Pose[Index].GetScale3D() * LocalScale);
                }
                FitBodyScale *= Padding;
                Rebuild();
                RefreshMeshPose();
            }
            const FBox FittedMass = FitGeometry->MeasureAdaptiveFitBounds(MeshPose, INDEX_NONE, INDEX_NONE, true);
            const float FloorShift = FitSupportPlane + 2.f - FittedMass.Min.Z;
            const float RoofShift = FitClearance.Z - 2.f - FittedMass.Max.Z;
            const float ShiftZ = FloorShift <= RoofShift ? FMath::Clamp(0.f, FloorShift, RoofShift)
                : (FloorShift + RoofShift) * .5f;
            float ShiftX = 0.f;
            if (Forward > .01f)
            {
                // The imported trunk is asymmetric around its motion root.
                // Center its measured mass in the same longitudinal interval
                // used by the following limb solve, without scaling anatomy.
                const float BackShift = -FitClearance.X * .5f + 2.f - FittedMass.Min.X;
                const float FrontShift = FitClearance.X * .5f - 2.f - FittedMass.Max.X;
                const float CenterShift = -FittedMass.GetCenter().X * Forward;
                ShiftX = BackShift <= FrontShift ? FMath::Clamp(CenterShift, BackShift, FrontShift)
                    : (BackShift + FrontShift) * .5f;
            }
            FTransform At = Current[Body.GetInt()];
            At.AddToTranslation(FVector(ShiftX, -FittedMass.GetCenter().Y * Side, ShiftZ));
            const auto Parent = Bones.GetParentBoneIndex(Body);
            Output.Pose[Body] = Parent != INDEX_NONE ? At.GetRelativeTransform(Current[Parent.GetInt()]) : At;
            Rebuild();
        }
    }
    const float HalfWidth = FMath::Max(12.f, FitClearance.Y * .5f - 25.f);
    const float ShoulderWidth = FMath::Max(14.f, FitClearance.Y * .5f - 22.f);
    const float HalfLength = FMath::Max(65.f, FitClearance.X * .5f - 14.f);
    const float Top = FMath::Max(28.f, FitClearance.Z - 26.f);
    TArray<FVector> Elbows;
    for (const FArmBinding& Arm : ArmBindings)
    {
        const auto Upper = Compact(Arm.Upper), Fore = Compact(Arm.Fore), Hand = Compact(Arm.Hand);
        auto Attachment = Compact(Arm.Shoulder != INDEX_NONE ? Arm.Shoulder : Arm.Upper);
        if (Attachment == INDEX_NONE) Attachment = Upper;
        if (Upper == INDEX_NONE || Fore == INDEX_NONE || Hand == INDEX_NONE || Arm.bHasApproach) continue;
        const int32 U = Upper.GetInt(), E = Fore.GetInt(), W = Hand.GetInt();
        FTransform A = Current[U], B = Current[E], C = Current[W];
        const FVector ShoulderShift(0.f,
            FMath::Clamp(A.GetLocation().Y, -ShoulderWidth, ShoulderWidth) - A.GetLocation().Y,
            FMath::Clamp(A.GetLocation().Z, FitSupportPlane + 26.f, Top) - A.GetLocation().Z);
        FTransform Attach = Current[Attachment.GetInt()]; Attach.AddToTranslation(ShoulderShift);
        const auto AttachParent = Bones.GetParentBoneIndex(Attachment);
        Output.Pose[Attachment] = AttachParent != INDEX_NONE
            ? Attach.GetRelativeTransform(Current[AttachParent.GetInt()]) : Attach;
        Rebuild(); A = Current[U]; B = Current[E]; C = Current[W];
        const FVector RefWrist = FMath::Lerp(Tall[W].GetLocation(), Low[W].GetLocation(), LowMix);
        const FVector Motion = Unfitted[W].GetLocation() - Neutral[W].GetLocation();
        FVector Goal = FMath::Lerp(C.GetLocation(), RefWrist + Motion * FVector(.50f, .12f, .18f), Strength);
        if (Forward > .01f) Goal.X = FMath::Clamp(Goal.X, -HalfLength, HalfLength);
        Goal.Y = FMath::Clamp(Goal.Y, -HalfWidth, HalfWidth);
        Goal.Z = FMath::Clamp(Goal.Z, FitSupportPlane + 22.f, Top);
        const float Curve = FMath::Clamp(Output.Curve.Get(Arm.Curve), 0.f, 1.f);
        const FHandContact& Existing = HandContacts[Arm.ContactIndex];
        const bool bKeepPlant = Curve >= .65f && Existing.bPinned
            && FVector::DotProduct(Existing.NormalWorld, SupportNormal) > .9f;
        const FVector OldPlant = ComponentToWorld.InverseTransformPosition(Existing.AnchorWorld);
        if (bKeepPlant && FMath::Abs(OldPlant.Y) < FitClearance.Y * .5f - 8.f)
        { Goal.X = OldPlant.X; Goal.Y = OldPlant.Y; }
        if (Curve >= .65f) Goal.Z = FitSupportPlane + 22.f;
        else if (Height > .8f) Goal.Z = FMath::Lerp(Goal.Z,
            FitSupportPlane + FitClearance.Z * .48f, Height * .8f);

        // All current digit angles are part of this hand-local skin envelope.
        // A roll then moves this intact hand; it never scales its fingers.
        FBox HandShape(ForceInit);
        if (FitGeometry)
        {
            RefreshMeshPose();
            TArray<FTransform> HandSpace = MeshPose;
            const FTransform InverseHand = C.Inverse();
            for (FTransform& Transform : HandSpace) Transform = Transform * InverseHand;
            HandShape = FitGeometry->MeasureAdaptiveFitBounds(HandSpace, Arm.Hand, INDEX_NONE, false, true, false);
        }
        const FQuat ReferenceRoll = FQuat::Slerp(Tall[W].GetRotation(), Low[W].GetRotation(), LowMix).GetNormalized();
        FQuat HandRoll = FQuat::Slerp(C.GetRotation(), ReferenceRoll, Strength).GetNormalized();
        float HandX0 = -BIG_NUMBER, HandX1 = BIG_NUMBER;
        if (HandShape.IsValid)
        {
            TArray<FQuat, TInlineAllocator<12>> Rolls;
            Rolls.Add(HandRoll); Rolls.Add(Tall[W].GetRotation()); Rolls.Add(Low[W].GetRotation());
            const FVector Size = HandShape.GetSize();
            int32 LongAxis = Size.Y > Size.X ? 1 : 0; if (Size.Z > Size[LongAxis]) LongAxis = 2;
            FVector LocalAxis = FVector::ZeroVector;
            LocalAxis[LongAxis] = HandShape.GetCenter()[LongAxis] < 0.f ? -1.f : 1.f;
            const float Sign = RefWrist.X < A.GetLocation().X ? -1.f : 1.f;
            const FQuat Aligned = (FQuat::FindBetweenNormals(HandRoll.RotateVector(LocalAxis),
                FVector(Sign, 0.f, 0.f)) * HandRoll).GetNormalized();
            for (int32 Roll = 0; Roll < 8; ++Roll)
                Rolls.Add((FQuat(FVector::ForwardVector, Roll * PI * .25f) * Aligned).GetNormalized());
            float BestHand = BIG_NUMBER; FVector BestGoal = Goal;
            for (const FQuat& Rotation : Rolls)
            {
                FTransform Candidate = C; Candidate.SetLocation(FVector::ZeroVector); Candidate.SetRotation(Rotation);
                const FBox Shape = HandShape.TransformBy(Candidate.ToMatrixWithScale());
                FVector At = Goal;
                const float X0 = -FitClearance.X * .5f + 1.f - Shape.Min.X;
                const float X1 = FitClearance.X * .5f - 1.f - Shape.Max.X;
                if (Forward > .01f && X0 <= X1) At.X = FMath::Clamp(At.X, X0, X1);
                const float Y0 = -FitClearance.Y * .5f + 1.f - Shape.Min.Y;
                const float Y1 = FitClearance.Y * .5f - 1.f - Shape.Max.Y;
                const float Z0 = FitSupportPlane + 1.f - Shape.Min.Z;
                const float Z1 = FitClearance.Z - 2.f - Shape.Max.Z;
                if (Y0 <= Y1) At.Y = FMath::Clamp(At.Y, Y0, Y1);
                if (Z0 <= Z1) At.Z = Curve >= .65f ? Z0 : FMath::Clamp(At.Z, Z0, Z1);
                const float Violation = (Forward > .01f ? FMath::Square(FMath::Max(0.f, X0 - X1)) : 0.f)
                    + FMath::Square(FMath::Max(0.f, Y0 - Y1))
                    + FMath::Square(FMath::Max(0.f, Z0 - Z1));
                const float Score = Violation * 10000.f + FVector::DistSquared(At, Goal) * .25f
                    + FMath::Square(Rotation.AngularDistance(ReferenceRoll)) * 20.f;
                if (Score < BestHand) { BestHand = Score; BestGoal = At; HandRoll = Rotation; }
            }
            Goal = BestGoal;
            if (Forward > .01f)
            {
                FTransform At = C; At.SetLocation(FVector::ZeroVector); At.SetRotation(HandRoll);
                const FBox Shape = HandShape.TransformBy(At.ToMatrixWithScale());
                HandX0 = -FitClearance.X * .5f + 1.f - Shape.Min.X;
                HandX1 = FitClearance.X * .5f - 1.f - Shape.Max.X;
            }
        }

        const float Lu = FVector::Distance(A.GetLocation(), B.GetLocation());
        const float Lf = FVector::Distance(B.GetLocation(), C.GetLocation());
        const float Minimum = FMath::Abs(Lu - Lf) + 1.5f, Maximum = Lu + Lf - 1.f;
        const float Sign = RefWrist.X < A.GetLocation().X ? -1.f : 1.f;
        FVector BestGoal = Goal, Pole = B.GetLocation(); float BestScore = BIG_NUMBER;
        // A short wrist distance creates a large elbow circle. In a narrow,
        // low opening, reach forward/back until that circle can actually fit.
        for (int32 Reach = 0; Reach < 4; ++Reach)
        {
            FVector Wanted = Goal;
            const float Fractions[4] = {0.f, .96f, .985f, .995f};
            float Required = Reach == 0 ? Minimum : FMath::Max(Minimum, (Lu + Lf) * Fractions[Reach]);
            FVector Along = Wanted - A.GetLocation();
            if (Along.Size() < Required)
                Wanted.X = A.GetLocation().X + Sign * FMath::Sqrt(FMath::Max(1.f,
                    Required * Required - Along.Y * Along.Y - Along.Z * Along.Z));
            if (HandX0 <= HandX1) Wanted.X = FMath::Clamp(Wanted.X, HandX0, HandX1);
            Along = Wanted - A.GetLocation();
            if (Along.Size() > Maximum)
                Wanted.X = A.GetLocation().X + Sign * FMath::Sqrt(FMath::Max(1.f,
                    Maximum * Maximum - Along.Y * Along.Y - Along.Z * Along.Z));
            Along = Wanted - A.GetLocation();
            const float Distance = FMath::Max(Along.Size(), .01f);
            const FVector Direction = Along / Distance;
            const float Projection = (Lu * Lu - Lf * Lf + Distance * Distance) / (2.f * Distance);
            const FVector Center = A.GetLocation() + Direction * Projection;
            const float Radius = FMath::Sqrt(FMath::Max(0.f, Lu * Lu - Projection * Projection));
            FVector Hint = FMath::Lerp(Tall[E].GetLocation(), Low[E].GetLocation(), LowMix) - Center;
            Hint -= Direction * FVector::DotProduct(Hint, Direction);
            if (!Hint.Normalize()) Hint = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
            if (Hint.IsNearlyZero()) Hint = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
            const FVector Other = FVector::CrossProduct(Direction, Hint).GetSafeNormal();
            float LeastViolation = BIG_NUMBER;
            for (int32 Sample = 0; Sample < 48; ++Sample)
            {
                const float Angle = 2.f * PI * Sample / 48.f;
                const FVector Candidate = Center + Radius * (Hint * FMath::Cos(Angle) + Other * FMath::Sin(Angle));
                const float Violation = FMath::Square(FMath::Max(0.f, FMath::Abs(Candidate.Y) - HalfWidth))
                    + FMath::Square(FMath::Max(0.f, Candidate.Z - Top))
                    + FMath::Square(FMath::Max(0.f, FitSupportPlane + 26.f - Candidate.Z));
                LeastViolation = FMath::Min(LeastViolation, Violation);
                float Score = Violation * 10000.f + FVector::DistSquared(Candidate, B.GetLocation()) * .025f
                    + FVector::DistSquared(Wanted, Goal) * .01f;
                if (Forward > .01f) Score += 10000.f * FMath::Square(FMath::Max(0.f, FMath::Abs(Wanted.X) - HalfLength));
                for (const FVector& Previous : Elbows)
                    Score += 20.f * FMath::Square(FMath::Max(0.f, 18.f - FVector::Dist(Candidate, Previous)));
                if (Score < BestScore) { BestScore = Score; Pole = Candidate; BestGoal = Wanted; }
            }
            if (LeastViolation < .15f) break;
        }
        Goal = BestGoal; Elbows.Add(Pole);
        AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Goal, false, 1.f, 1.f);
        C.SetRotation(HandRoll);
        const auto Parent = Bones.GetParentBoneIndex(Upper);
        Output.Pose[Upper] = Parent != INDEX_NONE ? A.GetRelativeTransform(Current[Parent.GetInt()]) : A;
        Output.Pose[Fore] = B.GetRelativeTransform(A);
        Output.Pose[Hand] = C.GetRelativeTransform(B);
        Rebuild();
        if (FitGeometry)
        {
            RefreshMeshPose();
            const FBox Skin = FitGeometry->MeasureAdaptiveFitBounds(MeshPose,
                Arm.Shoulder != INDEX_NONE ? Arm.Shoulder : Arm.Upper, Arm.Fore, false, true, false);
            const float Edge = FitClearance.Y * .5f - 1.f;
            const float Floor = FitSupportPlane + 1.f, Roof = FitClearance.Z - 2.f;
            auto Violation = [&](const FBox& Box)
            {
                if (!Box.IsValid) return 0.f;
                float V = FMath::Square(FMath::Max(0.f, -Edge - Box.Min.Y))
                    + FMath::Square(FMath::Max(0.f, Box.Max.Y - Edge))
                    + FMath::Square(FMath::Max(0.f, Floor - Box.Min.Z))
                    + FMath::Square(FMath::Max(0.f, Box.Max.Z - Roof));
                if (Forward > .01f) V += FMath::Square(FMath::Max(0.f, -FitClearance.X * .5f + 1.f - Box.Min.X))
                    + FMath::Square(FMath::Max(0.f, Box.Max.X - FitClearance.X * .5f + 1.f));
                return V;
            };
            if (Skin.IsValid && Violation(Skin) > .01f)
            {
                // Only failing arms pay for three direct-part envelopes. These
                // contain current weighted skin, not guessed cylinder radii.
                auto LocalPart = [&](int32 MeshBone, const FTransform& Transform)
                {
                    TArray<FTransform> Local = MeshPose;
                    const FTransform Inverse = Transform.Inverse();
                    for (FTransform& Bone : Local) Bone = Bone * Inverse;
                    return FitGeometry->MeasureAdaptiveFitBounds(Local, MeshBone, INDEX_NONE, false, false, false);
                };
                const int32 MountBone = Arm.Shoulder != INDEX_NONE ? Arm.Shoulder : Arm.Upper;
                const FBox MountLocal = LocalPart(MountBone, Current[Attachment.GetInt()]);
                const FBox UpperLocal = LocalPart(Arm.Upper, Current[U]);
                const FBox ForeLocal = LocalPart(Arm.Fore, Current[E]);
                const FBox Mount = MountLocal.TransformBy(Current[Attachment.GetInt()].ToMatrixWithScale());
                const FBox UpperSkin = UpperLocal.TransformBy(Current[U].ToMatrixWithScale());
                FVector Shift = FVector::ZeroVector;
                for (const int32 Axis : {1, 2})
                {
                    const float Lower = Axis == 1 ? -Edge : Floor, Higher = Axis == 1 ? Edge : Roof;
                    float L = Lower - Mount.Min[Axis], H = Higher - Mount.Max[Axis];
                    const float UL = FMath::Max(L, Lower - UpperSkin.Min[Axis]);
                    const float UH = FMath::Min(H, Higher - UpperSkin.Max[Axis]);
                    if (UL <= UH) { L = UL; H = UH; }
                    if (L <= H) Shift[Axis] = FMath::Clamp(0.f, L, H);
                }
                FTransform MountAt = Current[Attachment.GetInt()]; MountAt.AddToTranslation(Shift);
                Output.Pose[Attachment] = AttachParent != INDEX_NONE
                    ? MountAt.GetRelativeTransform(Current[AttachParent.GetInt()]) : MountAt;
                Rebuild(); A = Current[U]; B = Current[E]; C = Current[W];
                FTransform WinnerA = A, WinnerB = B, WinnerC = C;
                float WinnerScore = BIG_NUMBER;
                auto RollPart = [&](FTransform& Part, const FBox& Local, const FVector& Axis)
                {
                    const FTransform Initial = Part; FTransform Best = Part;
                    float Cost = BIG_NUMBER, BestViolation = BIG_NUMBER;
                    for (const float Angle : {0.f, HALF_PI, -HALF_PI, PI})
                    {
                        FTransform At = Initial;
                        At.SetRotation((FQuat(Axis, Angle) * Initial.GetRotation()).GetNormalized());
                        const float V = Violation(Local.TransformBy(At.ToMatrixWithScale()));
                        const float Score = V * 10000.f + Angle * Angle;
                        if (Score < Cost) { Cost = Score; Best = At; BestViolation = V; }
                    }
                    Part = Best; return BestViolation;
                };
                for (int32 Reach = 0; Reach < 4; ++Reach)
                {
                    FVector Wanted = Goal;
                    const float Fraction[4] = {0.f, .96f, .985f, .995f};
                    const float Required = Reach ? FMath::Max(Minimum, (Lu + Lf) * Fraction[Reach]) : Minimum;
                    FVector Along = Wanted - A.GetLocation();
                    if (Along.Size() < Required)
                        Wanted.X = A.GetLocation().X + Sign * FMath::Sqrt(FMath::Max(1.f,
                            Required * Required - Along.Y * Along.Y - Along.Z * Along.Z));
                    if (HandX0 <= HandX1) Wanted.X = FMath::Clamp(Wanted.X, HandX0, HandX1);
                    Along = Wanted - A.GetLocation();
                    if (Along.Size() > Maximum)
                        Wanted.X = A.GetLocation().X + Sign * FMath::Sqrt(FMath::Max(1.f,
                            Maximum * Maximum - Along.Y * Along.Y - Along.Z * Along.Z));
                    const FVector Direction = (Wanted - A.GetLocation()).GetSafeNormal();
                    const float Distance = FMath::Max(FVector::Dist(Wanted, A.GetLocation()), .01f);
                    const float Projection = (Lu * Lu - Lf * Lf + Distance * Distance) / (2.f * Distance);
                    const FVector Center = A.GetLocation() + Direction * Projection;
                    const float Radius = FMath::Sqrt(FMath::Max(0.f, Lu * Lu - Projection * Projection));
                    FVector Axis = FVector::VectorPlaneProject(B.GetLocation() - Center, Direction).GetSafeNormal();
                    if (Axis.IsNearlyZero()) Axis = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
                    const FVector Other = FVector::CrossProduct(Direction, Axis).GetSafeNormal();
                    float Least = BIG_NUMBER;
                    for (int32 Sample = 0; Sample < 48; ++Sample)
                    {
                        const float Angle = Sample * 2.f * PI / 48.f;
                        const FVector AtPole = Center + Radius * (Axis * FMath::Cos(Angle) + Other * FMath::Sin(Angle));
                        FTransform FitA = A, FitB = B, FitC = C;
                        AnimationCore::SolveTwoBoneIK(FitA, FitB, FitC, AtPole, Wanted, false, 1.f, 1.f);
                        FitC.SetRotation(HandRoll);
                        const float V = RollPart(FitA, UpperLocal, (AtPole - A.GetLocation()).GetSafeNormal())
                            + RollPart(FitB, ForeLocal, (Wanted - AtPole).GetSafeNormal());
                        Least = FMath::Min(Least, V);
                        const float Score = V * 10000.f + FVector::DistSquared(AtPole, B.GetLocation()) * .015f
                            + FVector::DistSquared(Wanted, Goal) * .01f;
                        if (Score < WinnerScore) { WinnerScore = Score; WinnerA = FitA; WinnerB = FitB; WinnerC = FitC; }
                    }
                    if (Least < .01f) break;
                }
                const auto RepairParent = Bones.GetParentBoneIndex(Upper);
                Output.Pose[Upper] = RepairParent != INDEX_NONE
                    ? WinnerA.GetRelativeTransform(Current[RepairParent.GetInt()]) : WinnerA;
                Output.Pose[Fore] = WinnerB.GetRelativeTransform(WinnerA);
                Output.Pose[Hand] = WinnerC.GetRelativeTransform(WinnerB);
                Rebuild();
            }
        }
        if (bKeepPlant)
        {
            const FVector Now = Current[W].GetLocation();
            AdaptiveForcedReplant[Arm.ContactIndex] = FVector2D(Now.X - OldPlant.X, Now.Y - OldPlant.Y).Size() > 1.5f;
        }
    }
    if (bCrampedCapture && Head != INDEX_NONE)
    {
        // Keep the authored face acting while the trunk and limbs stay folded.
        FTransform Face = Current[Head.GetInt()];
        Face.SetRotation(Unfitted[Head.GetInt()].GetRotation());
        const auto Parent = Bones.GetParentBoneIndex(Head);
        Output.Pose[Head] = Parent != INDEX_NONE ? Face.GetRelativeTransform(Current[Parent.GetInt()]) : Face;
        Rebuild();
    }
    FitEvaluationMilliseconds = (FPlatformTime::Seconds() - Began) * 1000.f;
}

void ValidateAdaptiveFit(const FPoseContext& Output)
{
    if (!bFitApplied || !FitGeometry || !HeadMesh) return;
    const double Began = FPlatformTime::Seconds();
    const auto& Bones = Output.Pose.GetBoneContainer();
    TArray<FTransform> CS, MeshPose;
    CS.SetNum(Output.Pose.GetNumBones());
    MeshPose.Init(FTransform::Identity, HeadMesh->GetRefSkeleton().GetNum());
    for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
    {
        const auto Parent = Bones.GetParentBoneIndex(Index);
        CS[Index.GetInt()] = Parent != INDEX_NONE ? Output.Pose[Index] * CS[Parent.GetInt()] : Output.Pose[Index];
        const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
        if (MeshPose.IsValidIndex(MeshIndex)) MeshPose[MeshIndex] = CS[Index.GetInt()];
    }
    // Loose hair may brush the opening; rigid skin, the coat shell and hands
    // still have to fit. Individual cards cannot stop a valid movement route.
    const FBox FinalSkin = FitGeometry->MeasureAdaptiveFitBounds(MeshPose, INDEX_NONE, INDEX_NONE, false, true, false);
    if (FinalSkin.IsValid)
        bFitWithinLimits &= FinalSkin.Min.Y >= -FitClearance.Y * .5f - 5.f
            && FinalSkin.Max.Y <= FitClearance.Y * .5f + 5.f
            && FinalSkin.Min.Z >= FitSupportPlane - 2.f
            && FinalSkin.Max.Z <= FitClearance.Z + 2.f;
    FitEvaluationMilliseconds += (FPlatformTime::Seconds() - Began) * 1000.f;
}
