#include "Presentation/TallowJournalVoices.h"
#include "Presentation/TallowNarrative.h"
FName TallowJournalVoices::Identify(const TArray<FText>& Pages)
{
 if(!Pages.IsEmpty())for(FName Key:TallowNarrative::Keys())
 {const auto Known=TallowNarrative::Pages(Key);if(!Known.IsEmpty()&&Known[0].EqualTo(Pages[0]))return Key;}
 return NAME_None;
}
FName TallowJournalVoices::Speaker(FName Document, int32 Page)
{
 if(Document==TEXT("journal")||Document==TEXT("maisie"))return TEXT("Maisie");
 if(Document==TEXT("register"))return Page==0?TEXT("JosiahNell"):TEXT("Nell");
 if(Document==TEXT("notebook"))return TEXT("ConstableNell");
 if(Document==TEXT("letter"))return TEXT("Constable");
 if(Document==TEXT("nan"))return TEXT("Nan");
 if(Document==TEXT("ruth"))return TEXT("RuthNell");
 return TEXT("Nell");
}
FText TallowJournalVoices::Name(FName Speaker)
{
 if(Speaker==TEXT("JosiahNell"))return FText::FromString(TEXT("Josiah and Nell"));
 if(Speaker==TEXT("ConstableNell"))return FText::FromString(TEXT("the constable and Nell"));
 if(Speaker==TEXT("RuthNell"))return FText::FromString(TEXT("Ruth and Nell"));
 const TCHAR* Label=Speaker==TEXT("Maisie")?TEXT("Maisie Evans"):Speaker==TEXT("Nell")?TEXT("Nell Price"):Speaker==TEXT("Josiah")?TEXT("Josiah Pryce"):Speaker==TEXT("Ruth")?TEXT("Ruth Pryce"):Speaker==TEXT("Constable")?TEXT("the constable"):Speaker==TEXT("Nan")?TEXT("Nan"):Speaker==TEXT("Thomas")?TEXT("Thomas Pryce"):TEXT("Mim");
 return FText::FromString(Label);
}
FName TallowJournalVoices::Portrait(FName Speaker, int32 Index)
{
 if(Index<0||Index>1)return NAME_None;
 if(Speaker==TEXT("JosiahNell"))return Index==0?FName(TEXT("Josiah")):FName(TEXT("Nell"));
 if(Speaker==TEXT("ConstableNell"))return Index==0?FName(TEXT("Constable")):FName(TEXT("Nell"));
 if(Speaker==TEXT("RuthNell"))return Index==0?FName(TEXT("Ruth")):FName(TEXT("Nell"));
 return Index==0?(Speaker.IsNone()?FName(TEXT("Nell")):Speaker):NAME_None;
}
void FTallowReadingPlayback::Prepare(bool bHasAudio,float Length,bool bAutoPlay)
{*this=FTallowReadingPlayback();bAvailable=bHasAudio;Duration=FMath::Max(0.f,Length);bUserPaused=!bAutoPlay;}
void FTallowReadingPlayback::Interrupt(double Now,float Seconds)
{HoldUntil=FMath::Max(HoldUntil,Now+FMath::Clamp(Seconds,0.f,120.f));bPriorityPaused=Now<HoldUntil;}
void FTallowReadingPlayback::Advance(float Delta,double Now,bool bWorldPaused)
{
 bPriorityPaused=bWorldPaused||Now<HoldUntil;
 if(IsRunning()){Position=FMath::Min(Duration,Position+FMath::Max(0.f,Delta));if(Duration>0&&Position>=Duration)bFinished=true;}
}
void FTallowReadingPlayback::Toggle(){if(!bAvailable)return;if(bFinished){Replay();return;}bUserPaused=!bUserPaused;}
void FTallowReadingPlayback::Replay(){if(!bAvailable)return;Position=0;bFinished=false;bUserPaused=false;}
