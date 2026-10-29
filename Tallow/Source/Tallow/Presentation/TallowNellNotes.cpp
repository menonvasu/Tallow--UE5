#include "Presentation/TallowNellNotes.h"

#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/TallowCreature.h"
#include "Audio/TallowSoundscapeComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/TallowHousePresence.h"
#include "Environment/TallowJournal.h"
#include "Environment/TallowMatchbox.h"
#include "GameMode/TallowGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Map/TallowHouseMapComponent.h"
#include "Player/Presentation/TallowBreathFeedbackComponent.h"
#include "Player/TallowCharacter.h"
#include "Presentation/TallowEscapePoint.h"
#include "Presentation/TallowPlayDirector.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Tasks/TallowFuseBox.h"
#include "Tasks/TallowLockpickGame.h"
#include "Tasks/TallowPlank.h"
#include "Tasks/TallowProjector.h"
#include "UI/Presentation/TallowInventoryWidget.h"
#include "UI/Presentation/TallowJournalWidget.h"
#include "UI/Presentation/TallowNellNoteWidget.h"

UTallowNellNotesComponent::UTallowNellNotesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UTallowNellNotesComponent* UTallowNellNotesComponent::Find(const UObject* WorldContext)
{
	const AActor* Owner = UGameplayStatics::GetActorOfClass(WorldContext, ATallowPlayDirector::StaticClass());
	return Owner ? Owner->FindComponentByClass<UTallowNellNotesComponent>() : nullptr;
}

void UTallowNellNotesComponent::Setup(ATallowPlayDirector* InDirector, ATallowCharacter* InPlayer, ATallowCreature* InKeeper, UTallowSoundscapeComponent* InSoundscape)
{
	Director = InDirector; Player = InPlayer; Keeper = InKeeper; Soundscape = InSoundscape;
	if (!InPlayer) return;
	FuseBox = UGameplayStatics::GetActorOfClass(this, ATallowFuseBox::StaticClass());
	// Maisie's book and the matches on the table by the chair, where the night starts.
	const FVector Start = InPlayer->GetActorLocation();
	auto Nearest = [&Start](AActor* Actor, TWeakObjectPtr<AActor>& Best)
	{
		if (!Best.IsValid() || FVector::DistSquared(Actor->GetActorLocation(), Start) < FVector::DistSquared(Best->GetActorLocation(), Start)) Best = Actor;
	};
	for (TActorIterator<ATallowJournal> It(GetWorld()); It; ++It) if (It->Tags.Contains(FName(TEXT("Tallow.Lore.journal")))) Nearest(*It, Journal);
	for (TActorIterator<ATallowMatchbox> It(GetWorld()); It; ++It) Nearest(*It, StartMatches);
	for (TActorIterator<ATallowPlank> It(GetWorld()); It; ++It) It->OnPlankDestroyed.AddUniqueDynamic(this, &UTallowNellNotesComponent::OnPlank);
	if (auto* Candle = InPlayer->GetCandleComponent())
	{
		Candle->OnAcquireCandle.AddUniqueDynamic(this, &UTallowNellNotesComponent::OnCandleAcquired);
		Candle->OnMatchesChanged.AddUniqueDynamic(this, &UTallowNellNotesComponent::OnMatches);
	}
	if (auto* Lock = InPlayer->GetLockpickComponent()) Lock->OnSnap.AddUniqueDynamic(this, &UTallowNellNotesComponent::OnSnap);
	if (auto* Mode = GetWorld()->GetAuthGameMode<ATallowGameMode>()) Mode->OnCaught.AddUniqueDynamic(this, &UTallowNellNotesComponent::OnCaught);
	if (auto* House = GetWorld()->GetSubsystem<UTallowHousePresenceSubsystem>()) HouseEvents = House->GetEventCount();
	SeenCall = InPlayer->SeenCallAt;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		Widget = CreateWidget<UTallowNellNoteWidget>(PC);
		if (Widget) Widget->AddToViewport(11);
	}
}

void UTallowNellNotesComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Widget) Widget->RemoveFromParent();
	if (Player.IsValid()) Player->GuideEmphasis = 0.f;
	Super::EndPlay(Reason);
}

bool UTallowNellNotesComponent::IsUrgent(ETallowNote Note)
{
	return Note == ETallowNote::Seen || Note == ETallowNote::Close || Note == ETallowNote::Listen
		|| Note == ETallowNote::HideClose || Note == ETallowNote::Glimpse || Note == ETallowNote::Draught;
}

void UTallowNellNotesComponent::Notice(ETallowNote Note, float Delay)
{
	if (HasLearned(Note) || Note == Showing || !bShowNotes) return;
	const float Now = GetWorld()->GetTimeSeconds();
	// One waiting at a time. A danger note replaces a calmer one (being seen replaces anything); anything else that
	// can't wait is simply not learned yet, and comes again the next time it happens.
	if (Pending != ETallowNote::Count && Note != ETallowNote::Seen && !(IsUrgent(Note) && !IsUrgent(Pending))) return;
	Pending = Note; PendingFrom = Now + Delay; PendingUntil = PendingFrom + (IsUrgent(Note) ? 2.5f : 6.f);
}

float UTallowNellNotesComponent::KeeperDistance() const
{
	return Keeper.IsValid() && Player.IsValid() ? FVector::Dist(Keeper->GetActorLocation(), Player->GetActorLocation()) : 1e7f;
}

bool UTallowNellNotesComponent::CanSeeKeeper() const
{
	if (!Keeper.IsValid() || !Player.IsValid() || Keeper->IsHidden()) return false;
	if (const auto* Hunter = Cast<ATallowHunterAIController>(Keeper->GetController()); Hunter && Hunter->IsDisguised()) return false;
	const UCameraComponent* Camera = Player->GetFirstPersonCameraComponent();
	const FVector Eye = Camera->GetComponentLocation(), Body = Keeper->GetActorLocation() + FVector(0, 0, 60);
	const auto* Candle = Player->GetCandleComponent();
	const float Range = Candle->IsAcquired() && Candle->GetCandleState() != ECandleState::Out ? 1100.f : 450.f;
	if (FVector::Dist(Eye, Body) > Range || FVector::DotProduct((Body - Eye).GetSafeNormal(), Camera->GetForwardVector()) < .8f) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NellNotesSight), false, Player.Get());
	Query.AddIgnoredActor(Keeper.Get());
	return !GetWorld()->LineTraceTestByChannel(Eye, Body, ECC_Visibility, Query);
}

bool UTallowNellNotesComponent::IsSuppressed() const
{
	if (!Player.IsValid() || !Director.IsValid() || Director->IsMenuOpen() || UGameplayStatics::IsGamePaused(this)) return true;
	if (const auto* Mode = GetWorld()->GetAuthGameMode<ATallowGameMode>(); Mode && Mode->IsCaptureInProgress()) return true;
	if (Player->IsTiedToChair()) return true;
	if (const auto* Map = Player->FindComponentByClass<UTallowHouseMapComponent>(); Map && Map->IsMapOpen()) return true;
	if (const auto* Lock = Player->GetLockpickComponent(); Lock && Lock->IsComponentTickEnabled()) return true;
	for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) if (It->GetIsCurrentlyInputtingKeypad()) return true;
	TArray<UUserWidget*> Open;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Open, UTallowJournalWidget::StaticClass(), false);
	for (auto* Reader : Open) if (const auto* Paper = Cast<UTallowJournalWidget>(Reader); Paper && Paper->IsOpen() && !Paper->IsCompact()) return true;
	Open.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Open, UTallowInventoryWidget::StaticClass(), false);
	for (auto* Inventory : Open) if (Inventory->IsInViewport()) return true;
	return false;
}

void UTallowNellNotesComponent::OnCandleAcquired()
{
	if (!Player.IsValid()) return;
	CandleSince = GetWorld()->GetTimeSeconds();
	Notice(ETallowNote::Candle, 1.5f);   // after her "Maisie?"
}

void UTallowNellNotesComponent::NoticeBolted()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bShowNotes || Now - BoltedAt < 20.f || Pending != ETallowNote::Count || Showing == ETallowNote::Bolted) return;
	BoltedAt = Now;
	// Unlike the first-time notes this one may come again; it is not marked learned.
	LearnedMask &= ~(1ull << uint32(ETallowNote::Bolted));
	Notice(ETallowNote::Bolted);
}

void UTallowNellNotesComponent::OnPlank(bool bBroken) {}
void UTallowNellNotesComponent::OnSnap() {}
void UTallowNellNotesComponent::OnMatches(int32 Count, bool bTriedWithNone) { if (bTriedWithNone) Notice(ETallowNote::NoMatches); }

void UTallowNellNotesComponent::OnCaught()
{
	bCaught = true;
	Pending = ETallowNote::Count;
	if (Widget) Widget->HideNote();
}

void UTallowNellNotesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Player.IsValid() || !Director.IsValid()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextCheck) { NextCheck = Now + .1f; Watch(Now); }
	Present(Now);
	// The draught leans harder while she first notices it, then settles back.
	Player->GuideEmphasis = FMath::FInterpConstantTo(Player->GuideEmphasis, Now < GuideUntil ? 1.f : 0.f, DeltaTime, .8f);
}

void UTallowNellNotesComponent::Watch(float Now)
{
	ATallowCharacter* Nell = Player.Get();
	const auto* Candle = Nell->GetCandleComponent();
	const bool bTied = Nell->IsTiedToChair();
	if (bWasTied && !bTied && bCaught) { bCaught = false; Notice(ETallowNote::Retry, .8f); }
	bWasTied = bTied;
	if (bTied) return;
	const bool bHave = Candle->IsAcquired() && !Candle->IsGivenUp();
	const ECandleState State = Candle->GetCandleState();
	const bool bLit = bHave && State != ECandleState::Out;
	const bool bAwake = Director->IsKeeperAwake();
	const float Distance = KeeperDistance();
	const ETallowTension Tension = Soundscape.IsValid() ? Soundscape->GetTension() : ETallowTension::Calm;
	OutSince = bHave && State == ECandleState::Out ? (OutSince < 0.f ? Now : OutSince) : -1.f;
	const bool bFreshCall = Nell->SeenCallAt > SeenCall + .01f;
	if (bFreshCall) SeenCall = Nell->SeenCallAt;
	if (Tension == ETallowTension::Chase || Nell->bSeenByKeeper) ThreatAt = Now;

	// Danger first: these are the moments the rules are learned or a life is lost.
	// 29 Oct: no notes about what the Keeper saw, heard or is doing. The flame and her breath say it.

	// The candle.
	if (bHave && State == ECandleState::Lit && Candle->IsInDraft()) Notice(ETallowNote::Draught);
	if (bHave && State == ECandleState::Lit && Candle->IsGuttering() && !Candle->IsInDraft() && Nell->GetVelocity().Size2D() > 300.f) Notice(ETallowNote::Sprint);
	if (OutSince >= 0.f && Now - OutSince > 1.2f && !(bAwake && Distance < 1500.f))
		Notice(Candle->GetMatchCount() > 0 ? ETallowNote::Dark : ETallowNote::NoMatches);
	if (bLit && Candle->GetFuelStage() == ETallowCandleFuelStage::Low) Notice(ETallowNote::Wax);
	if (bLit && CandleSince >= 0.f && Now - CandleSince > 6.f && Now - SeenCall > 15.f && !(bAwake && Distance < 900.f)
		&& !Nell->GuideTarget.IsZero() && Nell->GuideWalk > 400.f && Nell->GetVelocity().Size2D() > 40.f)
		Notice(ETallowNote::Guide);
	// The objective's hiss carries a little way: heard once she is within earshot of it, shortly after the flame's lesson.
	if (HasLearned(ETallowNote::Guide) && Now - ShownUntil > 8.f && !Nell->GuideTarget.IsZero() && Nell->GuideWalk > 0.f && Nell->GuideWalk < 1800.f)
		Notice(ETallowNote::Hiss, 1.f);
	// Wandering a long while without getting nearer: say it again, not often.
	if (bLit && !Nell->GuideTarget.IsZero() && Nell->GuideWalk > 0.f)
	{
		if (Nell->GuideWalk < BestWalk - 250.f || Nell->GuideWalk > BestWalk + 800.f || BestWalkAt <= 0.f) { BestWalk = Nell->GuideWalk; BestWalkAt = Now; }
		if (HasLearned(ETallowNote::Hiss) && Now - BestWalkAt > 180.f && Now - FollowAt > 240.f && Now - SeenCall > 15.f && !(bAwake && Distance < 900.f) && Pending == ETallowNote::Count)
		{
			FollowAt = Now; BestWalkAt = Now;
			LearnedMask &= ~(1ull << uint32(ETallowNote::Follow));
			Notice(ETallowNote::Follow);
		}
	}

	// Maisie's book, before it has been read: Nell knows the hand.
	if (Journal.IsValid() && Now - SeenCall > 10.f)
		if (const auto* Story = UTallowStorySubsystem::GetStory(this); Story && !Story->HasRead(TEXT("journal")))
		{
			const FVector Eye = Nell->GetFirstPersonCameraComponent()->GetComponentLocation();
			const FVector To = Journal->GetActorLocation() - Eye;
			if (To.Size() < 260.f && FVector::DotProduct(To.GetSafeNormal(), Nell->GetFirstPersonCameraComponent()->GetForwardVector()) > .8f)
				Notice(ETallowNote::Journal);
		}

	// The house.
	if (auto* House = GetWorld()->GetSubsystem<UTallowHousePresenceSubsystem>())
	{
		const int32 Events = House->GetEventCount();
		HouseEvents = Events;
	}
	const bool bDistraction = Soundscape.IsValid() && Soundscape->HasDistractionPlaying();
	bWasDistraction = bDistraction;
	if (const auto* Map = Nell->FindComponentByClass<UTallowHouseMapComponent>(); Map && Map->HasMap()) Notice(ETallowNote::Map);

	// The way out.
	if (const auto* Box = Cast<ATallowFuseBox>(FuseBox.Get()))
	{
		if (Box->CurrentFuses == 1) Notice(ETallowNote::Fuse, .8f);
		if (Box->CurrentFuses == 2) Notice(ETallowNote::LastFuse, .8f);
	}
	if (Director->bPowerRestored) Notice(ETallowNote::Power);
	if (Nell->GetInventoryCount(ECarriedItemType::FilmReel) > 0) Notice(ETallowNote::Reel, .6f);
	for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It)
	{
		if (It->GetProjectorState() == EProjectorState::Projecting) Notice(ETallowNote::Projector);
		if (It->HasCompletedProjection()) Notice(ETallowNote::Code, 1.f);
	}
	for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) if (It->IsCodeVerified()) Notice(ETallowNote::Open, 1.f);
}

void UTallowNellNotesComponent::Present(float Now)
{
	if (!Widget) return;
	const bool bSuppressed = IsSuppressed();
	Widget->SetSuppressed(bSuppressed);
	if (Showing != ETallowNote::Count && !Widget->IsShowing()) { Showing = ETallowNote::Count; ShownUntil = Now; }
	if (Pending == ETallowNote::Count) return;
	if (bSuppressed) { PendingUntil += GetWorld()->GetDeltaSeconds(); return; }
	if (Now < PendingFrom) return;
	if (Now > PendingUntil || HasLearned(Pending)) { Pending = ETallowNote::Count; return; }
	// A danger note may cut a calm one short, and being seen cuts any other; otherwise each gets its turn, with a
	// breath between.
	const bool bCut = Showing != ETallowNote::Count && Widget->GetAge() > 1.f
		&& ((IsUrgent(Pending) && !IsUrgent(Showing)) || (Pending == ETallowNote::Seen && Showing != ETallowNote::Seen));
	if (Showing != ETallowNote::Count && !bCut) return;
	if (Showing == ETallowNote::Count && !IsUrgent(Pending) && Now - ShownUntil < 1.5f) return;
	FString Text, Hint; int32 Line = 0;
	Compose(Pending, Text, Hint, Line);
	const float Seconds = FMath::Clamp(3.2f + .055f * float(Text.Len() + Hint.Len() * .5f), 4.5f, 8.f);
	Widget->ShowNote(Text, Hint, Seconds);
	Showing = Pending; Pending = ETallowNote::Count;
	if (Showing != ETallowNote::Bolted && Showing != ETallowNote::Follow) LearnedMask |= 1ull << uint32(Showing);
	if (Showing != ETallowNote::Bolted && Showing != ETallowNote::Follow) Written.Add(Showing);
	else ++BoltedVariant;
	if (Showing == ETallowNote::Guide) GuideUntil = Now + Seconds;
	if (Line > 0) Director->SayNellLine(Line);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_NOTES %s | %s"), *Text.Replace(TEXT("\n"), TEXT(" / ")), *Hint);
}

void UTallowNellNotesComponent::Compose(ETallowNote Note, FString& Text, FString& Hint, int32& Line) const
{
	// Nell's own words: a child, plain and frightened, no contractions (as she speaks in my recordings).
	const auto* Nell = Player.Get();
	const auto* Candle = Nell ? Nell->GetCandleComponent() : nullptr;
	const bool bLit = Candle && Candle->IsAcquired() && Candle->GetCandleState() != ECandleState::Out;
	switch (Note)
	{
	case ETallowNote::Journal: Text = TEXT("that is her writing."); Line = 13; break;
	case ETallowNote::Candle:
		Text = TEXT("maisie?\nher candle. it lets me see.");
		Hint = TEXT("[ right mouse ]  shield it          [ R ]  blow it out"); break;
	case ETallowNote::Guide: Text = TEXT("the flame leans.\nsomething is pulling it that way."); break;
	case ETallowNote::Hiss: Text = TEXT("listen. it hisses when it wants me."); break;
	case ETallowNote::Follow: Text = TEXT("follow the flame."); break;
	case ETallowNote::Draught:
		Text = TEXT("not now. please.\na draught. it will blow the flame out.");
		Hint = TEXT("[ hold right mouse ]  shield it"); Line = 8; break;
	case ETallowNote::Sprint: Text = TEXT("running makes the flame gutter."); break;
	case ETallowNote::Dark:
		Text = TEXT("dark. a match will light it."); Hint = TEXT("[ R ]  strike a match"); break;
	case ETallowNote::NoMatches:
		Text = StartMatches.IsValid() && !StartMatches->IsHidden() ? TEXT("no matches.\ni left them by the candle.")
			: TEXT("no matches left.\nthere are boxes about the house.");
		break;
	case ETallowNote::Wax: Text = TEXT("it is burning down.\ni need another candle."); break;
	case ETallowNote::Fuse: Text = TEXT("one fuse in.\ntwo more."); break;
	case ETallowNote::LastFuse: Text = TEXT("i need one more."); Line = 11; break;
	case ETallowNote::Power: Text = TEXT("it is still dark.\nthe motor works. the lamps do not."); break;
	case ETallowNote::Reel: Text = TEXT("a reel of film.\nit needs a projector."); break;
	case ETallowNote::Projector: Text = TEXT("watch the wall.\nremember every number."); break;
	case ETallowNote::Code: Text = TEXT("five numbers.\nthe latch by the window wants them."); break;
	case ETallowNote::Open: Text = TEXT("it is open. i could go now.\nor look for maisie first.\nher place is on the shelves."); break;
	case ETallowNote::Retry: Text = TEXT("i am still here.\nit put everything back where it was."); Line = 20; break;
	case ETallowNote::Bolted:
		{
			static const TCHAR* Variants[] = {
				TEXT("bolted from the other side.\ni need another way round."),
				TEXT("it will not give.\nthere has to be another way."),
				TEXT("bolted. not this way.\nnot yet.")};
			Text = Variants[BoltedVariant % UE_ARRAY_COUNT(Variants)];
		}
		break;
	case ETallowNote::Map: Text = TEXT("someone drew the house."); Hint = TEXT("[ M ]  look at it"); break;
	default: break;
	}
}

TArray<FString> UTallowNellNotesComponent::GetWrittenNotes() const
{
	TArray<FString> Out;
	for (const ETallowNote Note : Written)
	{
		FString Text, Hint; int32 Line = 0;
		Compose(Note, Text, Hint, Line);
		Out.Add(Text.Replace(TEXT("\n"), TEXT(" ")));
	}
	return Out;
}

void UTallowNellNotesComponent::Steps(TArray<FString>& Done, FString& Next) const
{
	const auto* Nell = Player.Get();
	const auto* Way = Director.Get();
	if (!Nell || !Way) return;
	bool bReel = Nell->GetInventoryCount(ECarriedItemType::FilmReel) > 0, bFilm = false, bCode = false;
	for (TActorIterator<ATallowProjector> It(GetWorld()); It; ++It) { bReel |= It->IsReelLoaded(); bFilm |= It->HasCompletedProjection(); }
	for (TActorIterator<ATallowEscapePoint> It(GetWorld()); It; ++It) bCode |= It->IsCodeVerified();
	const int32 Fuses = FuseBox.IsValid() ? Cast<ATallowFuseBox>(FuseBox.Get())->CurrentFuses : 0;
	const TPair<FString, bool> List[] = {
		{TEXT("get out of the chair"), !Nell->IsTiedToChair()},
		{TEXT("take maisie's candle"), Nell->GetCandleComponent()->IsAcquired()},
		{TEXT("find something to pry the boards with"), Nell->GetInventoryCount(ECarriedItemType::Crowbar) > 0 || Way->bBoardsCleared},
		{TEXT("pry the boards off the workshop door"), Way->bBoardsCleared},
		{FString::Printf(TEXT("three fuses for the switchboard (%d in)"), Fuses), Way->bPowerRestored},
		{TEXT("find the film reel"), bReel},
		{TEXT("the projector. watch the wall for the numbers"), bFilm},
		{TEXT("the five numbers, at the latch by the window"), bCode},
		{TEXT("pull the latch and get out. or look for maisie first"), false}};
	for (const auto& Step : List)
	{
		if (!Step.Value) { Next = Step.Key; return; }
		Done.Add(Step.Key);
	}
}

FText UTallowNellNotesComponent::GetSummary() const
{
	TArray<FString> Done; FString Next;
	Steps(Done, Next);
	return FText::FromString(FString::Printf(TEXT("In my own hand.\n\nnext: %s\n\n[ Enter ]  read it"), *Next));
}

TArray<FText> UTallowNellNotesComponent::GetNotebookPages() const
{
	TArray<FText> Pages;
	TArray<FString> Done; FString Next;
	Steps(Done, Next);
	FString Todo = TEXT("what i have to do\n\n");
	for (const FString& Step : Done) Todo += TEXT("x   ") + Step + TEXT("\n");
	if (!Next.IsEmpty()) Todo += TEXT("-   ") + Next + TEXT("\n");
	Pages.Add(FText::FromString(Todo));
	FString Page = TEXT("what i know\n\n");
	int32 OnPage = 0;
	for (const ETallowNote Note : Written)
	{
		FString Text, Hint; int32 Line = 0;
		Compose(Note, Text, Hint, Line);
		Page += Text.Replace(TEXT("\n"), TEXT(" ")) + (Hint.IsEmpty() ? TEXT("") : TEXT("\n") + Hint) + TEXT("\n\n");
		if (++OnPage == 4) { Pages.Add(FText::FromString(Page)); Page = TEXT("what i know\n\n"); OnPage = 0; }
	}
	if (OnPage > 0) Pages.Add(FText::FromString(Page));
	return Pages;
}
