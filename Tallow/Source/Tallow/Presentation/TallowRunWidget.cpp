#include "Presentation/TallowRunWidget.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Presentation/TallowPlayDirector.h"
#include "Presentation/TallowPlaytestSubsystem.h"
#include "Presentation/TallowStorySubsystem.h"
#include "Player/TallowCharacter.h"
#include "AI/TallowCreature.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "Candle/TallowCandleComponent.h"
#include "UI/Presentation/TallowPaper.h"
#include "Input/TallowInputPrompts.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UTallowRunWidget::RebuildWidget()
{ SetVisibility(ESlateVisibility::HitTestInvisible); return SNew(SBox); }
void UTallowRunWidget::ResetChapters() { Visited=0; bFoundStart=false; ChapterTime=0; }
void UTallowRunWidget::NativeTick(const FGeometry& G, float Dt)
{
	Super::NativeTick(G,Dt);
	if (!Player.IsValid()) Player=Cast<ATallowCharacter>(GetOwningPlayerPawn());
	if (!Mim.IsValid()) Mim=Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this,ATallowCreature::StaticClass()));
	if (!Player.IsValid()) return;
	if (!bFoundStart) { StartZ=Player->GetActorLocation().Z; bFoundStart=true; }
	ChapterTime=FMath::Max(0.f,ChapterTime-Dt); MotionTime+=Dt;
	if (Director && Director->IsMenuOpen()) return;
	const float Rise=Player->GetActorLocation().Z-StartZ;
	const auto* Story=UTallowStorySubsystem::GetStory(this);
	int32 Next=Player->IsTiedToChair()?0:!Director||!Director->bBoardsCleared?1:!Director->bPowerRestored?2:!Story||!Story->HasRead(TEXT("labels"))?3:!Story->HasRead(TEXT("maisie"))?4:5;
	if (const auto* Chapters = UTallowChapterSubsystem::GetActive(this)) Next = Chapters->GetChapterCardIndex();
	if (!(Visited & (1<<Next))) { Chapter=Next; Visited|=1<<Next; ChapterTime=6.f; }
}
int32 UTallowRunWidget::NativePaint(const FPaintArgs& A, const FGeometry& G, const FSlateRect& R, FSlateWindowElementList& O, int32 L, const FWidgetStyle& W, bool B) const
{
	L=Super::NativePaint(A,G,R,O,L,W,B);
	if (!Player.IsValid() || (Director && Director->IsMenuOpen())) return L;
	using namespace TallowPaper;
	const FVector2D Screen=G.GetLocalSize(); const float S=FMath::Clamp(float(Screen.Y/1080.f),.5f,1.4f);
	if (UTallowPlaytestSubsystem::IsBabyModeEnabled())
		if (const auto* Playtest = GetWorld()->GetSubsystem<UTallowPlaytestSubsystem>())
		{
			const FVector2D At(46*S,40*S);
			const FSlateFontInfo Line=FellItalic(FMath::RoundToInt(Pt::Small*S)), Quiet=FellItalic(FMath::RoundToInt((Pt::Small-3)*S));
			const FString Activity=Playtest->GetMimActivity(), Where=Playtest->GetMimWhereabouts();
			const float Width=FMath::Max3(float(Measure(Activity,Line).X),float(Measure(Where,Quiet).X),160.f*S);
			DrawSmudge(O,++L,G,At+FVector2D(Width*.5f,44*S),FVector2D(Width,88*S),.9f);
			auto Note=[&](const FString& Text,const FSlateFontInfo& Font,float Y,FLinearColor Colour)
			{ DrawText(O,++L,G,Text,Font,At+FVector2D(0,Y*S)+Measure(Text,Font)*FVector2D(.5f,.5f),Colour); L+=2; };
			Note(TEXT("TY IS A BABY"),Caps(FMath::RoundToInt(Pt::Label*S)),0,Ochre);
			Note(Activity,Line,30,Ink);
			Note(Where,Quiet,62,Pencil);
		}
	const UTallowCandleComponent* C=Player->GetCandleComponent();
	if (C && C->IsAcquired())
	{
		// What she carries is one key away; said once, quietly, in the corner.
		const FString Hint=FString::Printf(TEXT("[ %s ]  what you carry"),*TallowInputPrompts::GetCurrent(this,ETallowPromptAction::Inventory).Label.ToString());
		const FSlateFontInfo Font=FellItalic(FMath::RoundToInt(Pt::Small*S));
		const FVector2D Size=MeasureKeys(Hint,Font);
		DrawKeys(O,++L,G,Hint,Font,FVector2D(Screen.X-56*S-Size.X,Screen.Y-50*S),Pencil.CopyWithNewOpacity(.85f),Ink.CopyWithNewOpacity(.85f));L+=3;
	}
	if (ChapterTime>0)
	{
		const float Alpha=FMath::Min(FMath::Clamp((6-ChapterTime)/1.f,0.f,1.f),FMath::Clamp(ChapterTime/1.5f,0.f,1.f));
		const TCHAR* Numbers[]={TEXT("I"),TEXT("II"),TEXT("III"),TEXT("IV"),TEXT("V"),TEXT("VI")};
		const TCHAR* Names[]={TEXT("THE CHAIR"),TEXT("THE FAMILIAR HOUSE"),TEXT("THE RECORDS"),TEXT("THE CANDLES"),TEXT("THE CHILDREN"),TEXT("THE WAY OUT")};
		const auto* Chapters = UTallowChapterSubsystem::GetActive(this);
		const FString Number=FString(TEXT("CHAPTER  "))+(Chapters ? Chapters->GetChapterCardNumber().ToString() : FString(Numbers[Chapter]));
		const FString Title=(Chapters ? Chapters->GetChapterCardTitle().ToString() : FString(Names[Chapter])).ToUpper();
		// The chapter is set like a title page: a small line, the name in open capitals, one red thread under it.
		const FSlateFontInfo Small=Caps(FMath::RoundToInt((Pt::Label+1)*S),420), Big=Caps(FMath::RoundToInt(Pt::Display*S),300);
		const FVector2D At(Screen.X*.5f,Screen.Y*.3f);
		const FVector2D TitleSize=Measure(Title,Big);
		DrawSmudge(O,++L,G,At+FVector2D(0,8*S),FVector2D(TitleSize.X,TitleSize.Y+60*S),.85f*Alpha);
		DrawText(O,++L,G,Number,Small,At+FVector2D(0,-38*S),Pencil.CopyWithNewOpacity(Alpha));L+=2;
		DrawText(O,++L,G,Title,Big,At+FVector2D(0,14*S),Ivory.CopyWithNewOpacity(Alpha));L+=2;
		DrawThread(O,++L,G,At+FVector2D(-70,58)*S,At+FVector2D(70,57)*S,23,Alpha);L+=2;
	}
	// The cue responds to movement that exists in the world; no random danger flashes.
	if (Mim.IsValid() && !Player->IsTiedToChair())
	{
		const float Distance=FVector::Dist(Mim->GetActorLocation(),Player->GetActorLocation());
		// The eye means live sight only: the hunter writes bSeenByKeeper from a fresh clear
		// trace each tick, so hunting a last-known spot never reads as being watched.
		const bool Seen=Player->bSeenByKeeper;
		const float Awareness=Player->KeeperSight;
		// Felt distance is the way round, not through a floor or a wall; and lying in wait it gives nothing away.
		const UTallowHunterDirector* Pacing=GetWorld()->GetSubsystem<UTallowHunterDirector>();
		const ATallowHunterAIController* Brain=Cast<ATallowHunterAIController>(Mim->GetController());
		const float Felt=Pacing?FMath::Max(Distance,Pacing->GetWalkingDistance()):Distance;
		const bool Lurking=Brain&&Brain->IsLurking();
		const bool Moving=!Lurking && Mim->GetVelocity().Size()>15.f && Felt<1700.f;
		const bool Near=!Lurking && Felt<650.f && Mim->CurrentState!=ETallowCreatureState::Patrol;
		const FVector2D At(Screen.X-190*S,110*S);
		if (Seen || Awareness>.1f || Moving || Near)
		{
			// Maisie's signs, in the same words as her journal's last pages, so each one says what it means.
			const bool Lost=!Seen && Awareness>.1f && GetWorld()->GetTimeSeconds()-Player->SeenCallAt<30.f;
			const FString Label=Seen?TEXT("it sees me"):Lost?TEXT("out of its sight"):Awareness>.1f?TEXT("it's looking"):Near?TEXT("it's close"):TEXT("i can hear it");
			const FSlateFontInfo Font=FellItalic(FMath::RoundToInt(Pt::Body*S));
			const FVector2D Words=Measure(Label,Font);
			DrawSmudge(O,++L,G,At+FVector2D(0,10*S),FVector2D(Words.X,Words.Y+18*S),.8f);
			// Danger is the red thread; the rest a dust line.
			if (Seen||Near) DrawThread(O,++L,G,At+FVector2D(-34,-14)*S,At+FVector2D(34,-14)*S,1974);
			else DrawLine(O,++L,G,At+FVector2D(-34,-14)*S,At+FVector2D(34,-14)*S,Pencil,1.2f,1974);
			L+=2;
			DrawText(O,++L,G,Label,Font,At+FVector2D(0,12*S),Seen||Near?Ivory:Ink); L+=2;
		}
	}
	return L+2;
}
