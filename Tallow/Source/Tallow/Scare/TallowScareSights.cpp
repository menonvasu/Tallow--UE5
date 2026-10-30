#include "Scare/TallowScareDirector.h"
#include "Scare/TallowScareProxy.h"
#include "AI/TallowCreature.h"
#include "Candle/TallowCandleComponent.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/TallowDoor.h"
#include "Environment/TallowHousePresence.h"
#include "GameFramework/PlayerController.h"
#include "House/TallowHouseRooms.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TallowCharacter.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Tasks/TallowLockpickGame.h"

// The sight scares (29 Oct: "I didn't experience one minor scare"; six had played, all of them sounds far off).
// Each one happens where she is looking, a few steps away, in her room or its doorway and in her own candle light,
// with nothing between her eye and it. Small sounds go with them; what she sees carries the moment.

namespace
{
    using namespace TallowScareSight;
    const TCHAR* const Detail = TEXT("/Game/FinalPass/Sound/Detail/");
    const TCHAR* const Foley = TEXT("/Game/Audio/CC0/");
    FString SoundPath(const TCHAR* Folder, const TCHAR* Name) { return FString::Printf(TEXT("%s%s.%s"), Folder, Name, Name); }
    // The room a thing belongs to, judged a step from it towards the side it faces (a door: her side of it).
    FName RoomOf(const FVector& At, const FVector& Towards, float Step = 30.f)
    {
        return TallowHouseRooms::RoomAt(At + (Towards - At).GetSafeNormal2D() * Step);
    }
    bool MeshNamed(const UStaticMeshComponent* Mesh, std::initializer_list<const TCHAR*> Words)
    {
        if (!Mesh || !Mesh->GetStaticMesh()) return false;
        const FString Name = Mesh->GetStaticMesh()->GetName();
        for (const TCHAR* Word : Words) if (Name.Contains(Word)) return true;
        return false;
    }
    FVector MimAt(const UWorld* World)
    {
        TActorIterator<ATallowCreature> It(const_cast<UWorld*>(World));
        return It ? It->GetActorLocation() : FVector(1e7f);
    }
    bool CandleLit(const APawn* Pawn)
    {
        const auto* Player = Cast<ATallowCharacter>(Pawn);
        const UTallowCandleComponent* Candle = Player ? Player->GetCandleComponent() : nullptr;
        return Candle && Candle->IsAcquired() && Candle->GetCandleState() == ECandleState::Lit && !Candle->IsRelighting();
    }
    float FeetOf(const APawn* Pawn)
    {
        const auto* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
        return Pawn->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 62.f);
    }
}

bool UTallowScareDirector::ReadView(FView& Out) const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetPawn()) return false;
    FRotator Rotation;
    PC->GetPlayerViewPoint(Out.Eye, Rotation);
    Out.Forward = Rotation.Vector();
    Out.Room = TallowHouseRooms::RoomAt(PC->GetPawn()->GetActorLocation());
    return true;
}

bool UTallowScareDirector::IsClear(const FVector& Eye, const FVector& At, const AActor* Target, float Slack) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ScareSight), false, Target);
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Query.AddIgnoredActor(Pawn);
        TArray<AActor*> Held; Pawn->GetAttachedActors(Held); Query.AddIgnoredActors(Held);
    }
    for (const AActor* Stand : Staged) if (Stand) Query.AddIgnoredActor(Stand);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, At, ECC_Visibility, Query) || Hit.Distance >= FVector::Dist(Eye, At) - Slack;
}

USoundBase* UTallowScareDirector::LoadSound(const FString& Path)
{
    TObjectPtr<USoundBase>& Sound = Loaded.FindOrAdd(Path);
    if (!Sound) Sound = LoadObject<USoundBase>(nullptr, *Path);
    if (!Sound) UE_LOG(LogTemp, Warning, TEXT("TALLOW_SCARE missing %s"), *Path);
    return Sound;
}

void UTallowScareDirector::PlayAt(const TCHAR* Path, const FVector& Where, float Volume, float Delay, bool bSilent)
{
    if (bSilent) return;
    PendingCues.Add({float(GetWorld()->GetTimeSeconds() + Delay), Path, Where, Volume});
}

bool UTallowScareDirector::PlaceSight(ETallowScarePlace Place, const FView& View, FSightPick& Out)
{
    const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn) return false;
    UWorld* World = GetWorld();
    const FVector Mim = MimAt(World);
    TArray<FSpot> Spots;
    TArray<AActor*> Targets;
    auto Choose = [&](const FBand& Band) -> int32
    {
        const int32 I = Pick(View, Spots, Band, Random);
        if (I == INDEX_NONE) return I;
        Out.Target = Targets[I]; Out.At = Spots[I].At;
        Out.Distance = FVector::Dist(View.Eye, Out.At); Out.Degrees = DegreesOff(View, Out.At);
        return I;
    };
    switch (Place)
    {
    case ETallowScarePlace::Door:
    {
        // A door of her room, in front of her and in her light; never one whose swing could reach her, never near Mim.
        if (!CandleLit(Pawn)) return false;
        for (TActorIterator<ATallowDoor> It(World); It; ++It)
        {
            if (!It->CanDrift() || FVector::Dist(It->GetLeafCentre(), Mim) < 900.f) continue;
            if (FVector::Dist2D(Pawn->GetActorLocation(), It->GetHingeLocation()) < It->GetLeafWidth() + 45.f) continue;
            const FVector At = It->GetLeafCentre();
            Spots.Add({At, RoomOf(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), View.Eye, 40.f), IsClear(View.Eye, At, *It)});
            Targets.Add(*It);
        }
        FBand Band; Band.MinCm = 150.f; Band.MaxCm = 520.f; Band.MaxDegrees = 40.f; Band.MaxRise = 160.f;
        if (Choose(Band) == INDEX_NONE) return false;
        Out.bShut = Cast<ATallowDoor>(Out.Target.Get())->IsOpenForChapter();
        return true;
    }
    case ETallowScarePlace::Train:
    {
        // The toy train leaves its room for hers: it runs out of the dark at the edge of her view, across the boards in
        // front of her, and stops in her light. The one in the toy room is gone from where it was, for the chapter.
        if (!CandleLit(Pawn) || UsedTargets.Contains(TEXT("Train"))) return false;
        ATallowHousePresence* Train = nullptr;
        for (TActorIterator<ATallowHousePresence> It(World); It; ++It)
            if (!It->bCandlePractical && !It->IsHidden() && MeshNamed(It->MovingProp, {TEXT("ToyTrain")})) { Train = *It; break; }
        if (!Train || !Train->MovingProp->GetStaticMesh()) return false;
        const FVector Home = Train->MovingProp->GetComponentLocation();
        if (FVector::Dist(Home, View.Eye) < 1100.f && DegreesOff(View, Home) < 70.f && IsClear(View.Eye, Home, Train, 30.f)) return false;
        const FBox Local = Train->MovingProp->GetStaticMesh()->GetBoundingBox();
        const FVector Scale = Train->MovingProp->GetComponentScale().GetAbs();
        const FVector Half = Local.GetExtent() * Scale;
        const float Feet = FeetOf(Pawn);
        const FVector Ahead = View.Forward.GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Ahead);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ScareTrain), false, Train);
        auto FloorAt = [&](const FVector& P, float& Z)
        {
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Feet + 40.f), FVector(P.X, P.Y, Feet - 40.f), ECC_Visibility, Query) || Hit.ImpactNormal.Z < .95f) return false;
            Z = Hit.ImpactPoint.Z; return true;
        };
        FCollisionObjectQueryParams Solid; Solid.AddObjectTypesToQuery(ECC_WorldStatic); Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
        Solid.AddObjectTypesToQuery(ECC_Pawn); Solid.AddObjectTypesToQuery(ECC_PhysicsBody);
        // The floor sits well below her eye: the run is set as deep as it takes to be in the lower middle of her view
        // for the way she is looking (level: about 3 m; looking down: nearer), and never lower in it than she can see.
        const float Rise = View.Eye.Z - Feet;
        const float Down = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-View.Forward.GetSafeNormal().Z, -1.f, 1.f)));
        const float Want = Rise / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Down + 6.f, 14.f, 40.f)));
        auto Below = [&](const FVector& P) { return FMath::RadiansToDegrees(FMath::Atan2(View.Eye.Z - P.Z, FVector::Dist2D(View.Eye, P))) - Down; };
        for (int32 Try = 0; Try < 10; ++Try)
        {
            const float Depth = FMath::Clamp(Want * Random.FRandRange(.9f, 1.05f), 150.f, 300.f), Sign = (Try % 2) ? -1.f : 1.f;
            const FVector Mid(Pawn->GetActorLocation().X + Ahead.X * Depth, Pawn->GetActorLocation().Y + Ahead.Y * Depth, Feet);
            // From the edge of her view (in the dark beyond the middle of the light) to just past the middle of it.
            const FVector Dir = Side * Sign;
            const FVector From = Mid - Dir * (Depth * .8f), To = Mid + Dir * Random.FRandRange(35.f, 80.f);
            float Z0 = 0.f, Z = 0.f; bool bFlat = FloorAt(Mid, Z0);
            for (int32 S = 0; S <= 6 && bFlat; ++S) bFlat = FloorAt(FMath::Lerp(From, To, S / 6.f), Z) && FMath::Abs(Z - Z0) < 3.f;
            if (!bFlat) continue;
            const FName Room = View.Room;
            if (Room.IsNone() || TallowHouseRooms::RoomAt(FVector(From.X, From.Y, Z0 + 20.f)) != Room || TallowHouseRooms::RoomAt(FVector(To.X, To.Y, Z0 + 20.f)) != Room) continue;
            // Nothing in its way from end to end (the wheels' last few centimetres left out, so a rug edge is no wall),
            // and well clear of her feet.
            const FVector Lift(0.f, 0.f, Z0 + 4.f + (Half.Z * 2.f - 4.f) * .5f);
            const FQuat Heading = FRotator(0.f, Dir.Rotation().Yaw, 0.f).Quaternion();
            const FCollisionShape Box = FCollisionShape::MakeBox(FVector(Half.X + 2.f, Half.Y + 2.f, FMath::Max(2.f, Half.Z - 2.f)));
            FHitResult Hit;
            if (World->SweepSingleByObjectType(Hit, FVector(From.X, From.Y, 0.f) + Lift, FVector(To.X, To.Y, 0.f) + Lift, Heading, Solid, Box, Query)) continue;
            if (FMath::PointDistToSegment(Pawn->GetActorLocation() * FVector(1, 1, 0), From * FVector(1, 1, 0), To * FVector(1, 1, 0)) < 90.f) continue;
            const FVector Seen(To.X, To.Y, Z0 + 10.f);
            if (Below(Seen) > 23.f || Below(FVector(Mid.X, Mid.Y, Z0)) > 23.f || !IsClear(View.Eye, Seen, Train)) continue;
            Out.Target = Train; Out.From = FVector(From.X, From.Y, Z0); Out.To = FVector(To.X, To.Y, Z0); Out.At = Seen;
            Out.Distance = FVector::Dist(View.Eye, Seen); Out.Degrees = DegreesOff(View, Seen);
            return true;
        }
        return false;
    }
    case ETallowScarePlace::Frame:
    {
        // A picture, a mirror or a child's drawing on a wall of her room, in front of her. Swung once, it stays crooked.
        if (!CandleLit(Pawn)) return false;
        TArray<FVector> Normals;
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
            if (It->IsHidden() || UsedTargets.Contains(It->GetFName()) || !MeshNamed(Mesh, {TEXT("Painting"), TEXT("Furnish_Mirror"), TEXT("ChildDrawing")}) || MeshNamed(Mesh, {TEXT("Story")})) continue;
            const FBoxSphereBounds Bounds = Mesh->Bounds;
            const FVector C = Bounds.Origin, E = Bounds.BoxExtent;
            if (E.Z > 55.f || FMath::Max(E.X, E.Y) > 60.f) continue;
            const float Rise = C.Z - TallowHouseRooms::FloorZ(TallowHouseRooms::FloorAt(C.Z));
            if (Rise < 80.f || Rise > 300.f) continue;
            const FVector Normal = E.X < E.Y ? FVector(FMath::Sign(View.Eye.X - C.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(View.Eye.Y - C.Y), 0.f);
            Spots.Add({C, TallowHouseRooms::RoomAt(C + Normal * 30.f), IsClear(View.Eye, C, *It)});
            Targets.Add(*It); Normals.Add(Normal);
        }
        FBand Band; Band.MinCm = 120.f; Band.MaxCm = 430.f; Band.MaxDegrees = 36.f; Band.MaxRise = 220.f;
        for (int32 Try = 0; Try < 4; ++Try)
        {
            const int32 I = Choose(Band);
            if (I == INDEX_NONE) return false;
            // Room to swing: nothing touching its outline at its own depth off the wall (a mantel under a mirror, a shelf).
            const UStaticMeshComponent* Mesh = Cast<AStaticMeshActor>(Targets[I])->GetStaticMeshComponent();
            const FVector C = Mesh->Bounds.Origin, E = Mesh->Bounds.BoxExtent, N = Normals[I];
            const float Thin = FMath::Min(E.X, E.Y), Wide = FMath::Max(E.X, E.Y), Reach = .17f * FMath::Max(Wide, E.Z) + 2.f;
            const FVector Extent = FMath::Abs(N.X) > .5f ? FVector(.5f, Wide + Reach, E.Z + Reach) : FVector(Wide + Reach, .5f, E.Z + Reach);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ScareFrame), false, Targets[I]);
            Query.AddIgnoredActor(Pawn);
            if (World->OverlapAnyTestByChannel(C + N * (Thin + .6f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(Extent), Query))
            {
                Spots[I].bClear = false;
                continue;
            }
            Out.Axis = N; Out.From = C + FVector(0.f, 0.f, E.Z);
            return true;
        }
        return false;
    }
    case ETallowScarePlace::Gust:
    {
        // Her own flame, with nothing near to blame: only when Mim is far off, so it can never be read as her tell.
        const auto* Player = Cast<ATallowCharacter>(Pawn);
        if (!CandleLit(Pawn) || Player->GetCandleComponent()->IsInDraft() || !Pawn->FindComponentByClass<UTallowCandleFlameComponent>()) return false;
        if (FVector::Dist(Mim, Pawn->GetActorLocation()) < 1400.f) return false;
        Out.Target = const_cast<APawn*>(Pawn); Out.At = View.Eye + View.Forward * 50.f; Out.Distance = 50.f;
        return true;
    }
    case ETallowScarePlace::Light:
    {
        // A lit candle or lamp she can see (its flame and its light), in her room or through its doorway.
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->ActorHasTag(TEXT("Tallow.AuthoredLight"))) continue;
            UPointLightComponent* Lamp = It->FindComponentByClass<UPointLightComponent>();
            if (!Lamp || !Lamp->IsVisible() || Lamp->Intensity <= 0.f || Gutters.ContainsByPredicate([&](const FGutter& G) { return G.Light.Get() == Lamp; })) continue;
            const FVector At = Lamp->GetComponentLocation();
            AActor* Flame = nullptr;
            for (TActorIterator<AStaticMeshActor> F(World); F; ++F)
                if (MeshNamed(F->GetStaticMeshComponent(), {TEXT("TallowFlame")}) && FVector::Dist(F->GetActorLocation(), At) < 30.f) { Flame = *F; break; }
            if (!Flame) continue;
            Spots.Add({At, RoomOf(At, View.Eye), IsClear(View.Eye, At, Flame, 25.f)});
            Targets.Add(*It);
        }
        FBand Band; Band.MinCm = 120.f; Band.MaxCm = 650.f; Band.MaxDegrees = 40.f; Band.MaxRise = 260.f; Band.bSameRoom = false;
        return Choose(Band) != INDEX_NONE;
    }
    case ETallowScarePlace::Look:
    {
        // A small thing on a surface behind her in her room: the music box if she is with the toys, else whatever sits there.
        const float Floor = FeetOf(Pawn);
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
            if (It->IsHidden() || !Mesh || !Mesh->GetStaticMesh() || !Mesh->IsVisible() || MeshNamed(Mesh, {TEXT("TallowFlame")})) continue;
            const FVector C = Mesh->Bounds.Origin, E = Mesh->Bounds.BoxExtent;
            if (E.GetMax() > 35.f || C.Z - Floor < 30.f || C.Z - Floor > 150.f) continue;
            const FName Room = TallowHouseRooms::RoomAt(C);
            if (Room != View.Room) continue;
            Spots.Add({C, Room, IsClear(View.Eye, C, *It, 10.f)});
            Targets.Add(*It);
        }
        FBand Band; Band.MinCm = 140.f; Band.MaxCm = 520.f; Band.MinDegrees = 115.f; Band.MaxDegrees = 180.f; Band.MaxRise = 200.f;
        for (int32 I = 0; I < Spots.Num(); ++I)
            if (MeshNamed(Cast<AStaticMeshActor>(Targets[I])->GetStaticMeshComponent(), {TEXT("MusicBox")}) && Fits(View, Spots[I], Band))
            {
                Out.Target = Targets[I]; Out.At = Spots[I].At; Out.Distance = FVector::Dist(View.Eye, Out.At); Out.Degrees = DegreesOff(View, Out.At);
                return true;
            }
        return Choose(Band) != INDEX_NONE;
    }
    default: return false;
    }
}

bool UTallowScareDirector::FireSight(const FTallowScareGroupDef& G, FString& Where, AActor*& Target)
{
    // Only when she can take it in: free, out of hiding, not picking a lock.
    const auto* Player = Cast<ATallowCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player || Player->IsTiedToChair() || Player->IsHiddenInSpot() || (Player->GetLockpickComponent() && Player->GetLockpickComponent()->IsInMinigame())) return false;
    FView View;
    FSightPick Pick;
    if (!ReadView(View) || !PlaceSight(G.Place, View, Pick) || !StageSight(G, Pick, Where)) return false;
    Target = Pick.Target.Get();
    return true;
}

bool UTallowScareDirector::StageSight(const FTallowScareGroupDef& G, const FSightPick& Pick, FString& Where)
{
    UWorld* World = GetWorld();
    const float Now = World->GetTimeSeconds();
    AActor* Target = Pick.Target.Get();
    if (!Target) return false;
    const bool bQuiet = G.bSilent;
    FString What;
    switch (G.Place)
    {
    case ETallowScarePlace::Door:
    {
        auto* Door = Cast<ATallowDoor>(Target);
        if (Pick.bShut)
        {
            // Slowly, the way a door goes when no one is holding it, and the latch clicks home.
            if (!Door->DriftShut(.3f)) return false;
            PlayAt(*SoundPath(Detail, TEXT("AD_DoorDrift_01")), Pick.At, .45f, 0.f, bQuiet);
            PlayAt(*SoundPath(Foley, TEXT("P_Latch")), Pick.At, .3f, Door->GetSwingSeconds(.3f) * .97f, bQuiet);
            What = TEXT("shut");
        }
        else
        {
            // The latch lets go first, then the hinge, and it stops a hand's width open.
            if (!Door->DriftAjar(14.f / FMath::Max(10.f, FMath::Abs(Door->OpenAngle)), .2f)) return false;
            PlayAt(*SoundPath(Foley, TEXT("P_Latch")), Pick.At, .28f, 0.f, bQuiet);
            PlayAt(*SoundPath(Detail, TEXT("AD_DoorDrift_00")), Pick.At, .45f, .3f, bQuiet);
            What = TEXT("ajar");
        }
        break;
    }
    case ETallowScarePlace::Train:
    {
        auto* Train = Cast<ATallowHousePresence>(Target);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Copy = World->SpawnActor<ATallowScareProxy>(Pick.From, FRotator::ZeroRotator, Params);
        if (!Copy) return false;
        Copy->CopyLook(Train->MovingProp);
        const float Lift = -Train->MovingProp->GetStaticMesh()->GetBoundingBox().Min.Z * Train->MovingProp->GetComponentScale().Z;
        Copy->Run(Pick.From, Pick.To, 2.6f, Lift);
        Train->SetActorHiddenInGame(true);
        Staged.Add(Copy); HiddenProps.Add(Train); UsedTargets.Add(TEXT("Train"));
        if (!bQuiet)
            if (USoundBase* Sound = LoadSound(TEXT("/Game/FinalPass/Sound/Scare/SC_ToyTrain_00.SC_ToyTrain_00")))
                UGameplayStatics::SpawnSoundAttached(Sound, Copy->Mesh, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, .5f, 1.f, 0.f, PlacedAttenuation);
        What = TEXT("runs");
        break;
    }
    case ETallowScarePlace::Frame:
    {
        auto* Picture = Cast<AStaticMeshActor>(Target);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Copy = World->SpawnActor<ATallowScareProxy>(Pick.From, FRotator::ZeroRotator, Params);
        if (!Copy) return false;
        Copy->CopyLook(Picture->GetStaticMeshComponent());
        const float Sign = Random.FRand() < .5f ? -1.f : 1.f;
        Copy->Swing(Pick.From, Pick.Axis, Sign * Random.FRandRange(8.f, 11.f), -Sign * Random.FRandRange(3.f, 5.f), 5.5f);
        Picture->SetActorHiddenInGame(true);
        Staged.Add(Copy); HiddenProps.Add(Picture); UsedTargets.Add(Picture->GetFName());
        const bool bPaper = MeshNamed(Picture->GetStaticMeshComponent(), {TEXT("ChildDrawing")});
        if (bPaper) PlayAt(TEXT("/Game/FinalPass/Sound/Scare/SC_Drawing_00.SC_Drawing_00"), Pick.At, .4f, 0.f, bQuiet);
        else PlayAt(*SoundPath(Foley, *FString::Printf(TEXT("H_Settle_%02d"), Random.RandRange(0, 2))), Pick.From, .3f, 0.f, bQuiet);
        What = TEXT("swings");
        break;
    }
    case ETallowScarePlace::Gust:
    {
        auto* Flame = Target->FindComponentByClass<UTallowCandleFlameComponent>();
        if (!Flame) return false;
        Flame->LayDown(Random.FRandRange(1.7f, 2.2f));
        What = TEXT("gust");
        break;
    }
    case ETallowScarePlace::Light:
    {
        UPointLightComponent* Lamp = Target->FindComponentByClass<UPointLightComponent>();
        if (!Lamp) return false;
        AActor* Flame = nullptr;
        for (TActorIterator<AStaticMeshActor> F(World); F; ++F)
            if (MeshNamed(F->GetStaticMeshComponent(), {TEXT("TallowFlame")}) && FVector::Dist(F->GetActorLocation(), Lamp->GetComponentLocation()) < 30.f) { Flame = *F; break; }
        FGutter Gutter; Gutter.Light = Lamp; Gutter.Flame = Flame; Gutter.Base = Lamp->Intensity; Gutter.Started = Now;
        Gutter.OutAt = Now + Random.FRandRange(1.3f, 1.8f); Gutter.BackAt = Gutter.OutAt + Random.FRandRange(1.6f, 2.6f);
        Gutters.Add(Gutter);
        PlayAt(*SoundPath(Foley, TEXT("P_Candle_Puff")), Pick.At, .3f, Gutter.OutAt - Now, bQuiet);
        PlayAt(*SoundPath(Foley, TEXT("P_Candle_Catch")), Pick.At, .16f, Gutter.BackAt - Now, bQuiet);
        What = TEXT("gutters");
        break;
    }
    case ETallowScarePlace::Look:
    {
        if (bQuiet) return false;
        const FString& Path = G.Sounds.IsEmpty() ? FString(TEXT("/Game/FinalPass/Sound/Scare/SC_MusicBox_Wind_00.SC_MusicBox_Wind_00")) : G.Sounds[Random.RandRange(0, G.Sounds.Num() - 1)];
        USoundBase* Sound = LoadSound(Path);
        if (!Sound || !PlacedAttenuation) return false;
        Cue = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Pick.At, FRotator::ZeroRotator, G.Volume, 1.f, 0.f, PlacedAttenuation.Get());
        if (!Cue) return false;
        CueFrom = CueTo = Pick.At; CueStarted = Now; CueSeconds = 0.f;
        LookAt = Pick.At; bLookCue = true;
        What = TEXT("plays");
        break;
    }
    default: return false;
    }
    Where = FString::Printf(TEXT("%s %s@%s d=%.0f a=%.0f room=%s"), *Target->GetName(), *What, *Pick.At.ToCompactString(), Pick.Distance, Pick.Degrees,
        *TallowHouseRooms::RoomAt(UGameplayStatics::GetPlayerPawn(this, 0)->GetActorLocation()).ToString());
    return true;
}

void UTallowScareDirector::UpdateSights(float Now)
{
    for (int32 I = PendingCues.Num() - 1; I >= 0; --I)
    {
        if (Now < PendingCues[I].At) continue;
        if (USoundBase* Sound = LoadSound(PendingCues[I].Path))
            UGameplayStatics::PlaySoundAtLocation(this, Sound, PendingCues[I].Where, PendingCues[I].Volume, Random.FRandRange(.96f, 1.03f), 0.f, PlacedAttenuation.Get());
        PendingCues.RemoveAt(I);
    }
    for (int32 I = Gutters.Num() - 1; I >= 0; --I)
    {
        FGutter& G = Gutters[I];
        ULightComponent* Lamp = G.Light.Get();
        if (!Lamp) { Gutters.RemoveAt(I); continue; }
        float Level = 1.f;
        if (Now < G.OutAt)
        {
            // It shudders harder and harder, dipping almost out between breaths, then goes.
            const float T = (Now - G.Started) / FMath::Max(.1f, G.OutAt - G.Started);
            Level = 1.f - (.35f + .6f * T) * FMath::Abs(FMath::PerlinNoise1D(Now * 11.f + I * 7.f));
        }
        else if (Now < G.BackAt) Level = 0.f;
        else Level = FMath::Min(1.f, .15f + (Now - G.BackAt) / .8f);
        const bool bOut = Now >= G.OutAt && Now < G.BackAt;
        if (bOut != G.bOut) { G.bOut = bOut; if (AActor* Flame = G.Flame.Get()) Flame->SetActorHiddenInGame(bOut); }
        Lamp->SetIntensity(G.Base * Level);
        if (Now >= G.BackAt + .8f) { Lamp->SetIntensity(G.Base); Gutters.RemoveAt(I); }
    }
    // The music box: it plays while her back is to it, and stops dead when she turns and looks.
    if (bLookCue)
    {
        FView View;
        if (!Cue || !Cue->IsPlaying()) bLookCue = false;
        else if (ReadView(View) && DegreesOff(View, LookAt) < 24.f && IsClear(View.Eye, LookAt, nullptr, 25.f))
        {
            Cue->FadeOut(.05f, 0.f); bLookCue = false;
            UE_LOG(LogTemp, Display, TEXT("TALLOW_SCARE ch=%d looked=%s after=%.1f"), Chapter, *LookAt.ToCompactString(), Now - CueStarted);
        }
    }
}

void UTallowScareDirector::ClearSights()
{
    // The house as it was: stand-ins gone, props back, lamps lit, nothing left sounding.
    for (AActor* Stand : Staged) if (IsValid(Stand)) Stand->Destroy();
    Staged.Reset();
    for (const auto& Prop : HiddenProps) if (AActor* A = Prop.Get()) A->SetActorHiddenInGame(false);
    HiddenProps.Reset();
    for (const FGutter& G : Gutters)
    {
        if (ULightComponent* Lamp = G.Light.Get()) Lamp->SetIntensity(G.Base);
        if (AActor* Flame = G.Flame.Get()) Flame->SetActorHiddenInGame(false);
    }
    Gutters.Reset(); PendingCues.Reset(); UsedTargets.Reset();
    if (bLookCue && Cue) Cue->Stop();
    bLookCue = false; SightWaitSince = -1.f;
}
