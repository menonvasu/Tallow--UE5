#include "Chapter/TallowChapterDefinition.h"
FTallowChapterRules UTallowChapterDefinition::MakeRules(int32 ChapterIndex) const
{
    FTallowChapterRules Rules;
    Rules.ChapterIndex = ChapterIndex; Rules.Kind = Kind; Rules.Doors = Doors; Rules.Resources = Resources;
    Rules.StoryCandidates = StoryObjectCandidates; Rules.RequiredRooms = RequiredRooms;
    Rules.bBattery = Kind == ETallowChapterKind::Hunt;
    return Rules;
}
int32 UTallowCampaignDefinition::GetBatteryChapterCount() const
{
    int32 Count = 0;
    for (const auto& Chapter : Chapters) if (Chapter && Chapter->Kind == ETallowChapterKind::Hunt) ++Count;
    return Count;
}
UTallowCampaignDefinition* UTallowCampaignDefinition::LoadOrBuildDefault(UObject* Outer)
{
    if (auto* Asset = LoadObject<UTallowCampaignDefinition>(nullptr, TEXT("/Game/FinalPass/Pass2/DA_TallowCampaign.DA_TallowCampaign")))
        if (!Asset->Chapters.IsEmpty() && !Asset->Chapters.Contains(nullptr)) return Asset;
    return BuildDefault(Outer);
}
UTallowCampaignDefinition* UTallowCampaignDefinition::BuildDefault(UObject* Outer)
{
    auto* Campaign = NewObject<UTallowCampaignDefinition>(Outer);
    const ETallowStoryAct Acts[] = {ETallowStoryAct::FirstContact, ETallowStoryAct::FuseHunt, ETallowStoryAct::HouseWakes};
    const TCHAR* Numbers[] = { TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV") };
    const TCHAR* Titles[] = { TEXT("THE CHAIR"), TEXT("THE RECORDS"), TEXT("THE CANDLES"), TEXT("THE WAY OUT") };
    // The black blood lines written on the wall after each recording.
    const TCHAR* Writings[] = { TEXT("find the battery\ntake it to the humming box\ni'll be close"), TEXT("another battery\nsomewhere in the house\ni'm counting"), TEXT("the last battery\nthen the lights\nthen you stay") };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Chapter = NewObject<UTallowChapterDefinition>(Campaign);
        Chapter->CardNumber = FText::FromString(Numbers[Index]);
        Chapter->CardTitle = FText::FromString(Titles[Index]);
        Chapter->BatteryNoun = FText::FromString(TEXT("battery"));
        Chapter->ChapterId = FName(*FString::Printf(TEXT("Chapter_%d"), Index + 1));
        Chapter->Kind = Index == 3 ? ETallowChapterKind::Final : ETallowChapterKind::Hunt;
        Chapter->bStartsInChair = Index == 0; Chapter->bPlushPhase = Index != 3;
        if (Index < 3)
        {
            Chapter->HuntAct = Acts[Index];
            Chapter->BloodWriting = FText::FromString(Writings[Index]);
            Chapter->StoryObjectCandidates = {FName(*FString::Printf(TEXT("Pass2_Story_Ch%d_A"), Index + 1)), FName(*FString::Printf(TEXT("Pass2_Story_Ch%d_B"), Index + 1))};
            if (Index >= 1) Chapter->StoryObjectCandidates.SetNum(1);
            Chapter->Doors.MinCrowbar = 1;
            // "we need more lockpicking". Two or three picks a round, one of them on the way to the chair or the battery.
            // Chapter three caps at two because the final chapter inherits its doors.
            Chapter->Doors.MinLockpick = 2; Chapter->Doors.MaxLockpick = Index == 2 ? 2 : 3; Chapter->Doors.bLockpickOnRoute = true;
        }
        else Chapter->RequiredRooms = {TEXT("study"), TEXT("collection")};
        if (Index == 0)
        {
            Chapter->Doors.MaxCrowbar = 1;
            Chapter->Doors.ForcedStates.Add(TEXT("BP_TallowDoor"), ETallowDoorState::Crowbar);
        }
        Campaign->Chapters.Add(Chapter);
    }
    auto AddGroup = [Campaign](FName Name, TArray<FString> Patterns) -> FTallowScareGroupDef&
    {
        FTallowScareGroupDef Group; Group.Group = Name; Group.ActorNamePatterns = MoveTemp(Patterns);
        return Campaign->ScareGroups.Add_GetRef(Group);
    };
    auto Sounds = [](std::initializer_list<const TCHAR*> Names)
    {
        TArray<FString> Paths;
        for (const TCHAR* Name : Names) Paths.Add(FString::Printf(TEXT("/Game/FinalPass/Sound/Scare/%s.%s"), Name, Name));
        return Paths;
    };
    using P = ETallowChapterPhase;
    const TArray<P> Roaming = {P::Exploration, P::ActiveHunt, P::BatteryAcquired};
    auto Tune = [&Roaming](FTallowScareGroupDef& G, float Weight, float CooldownMin, float CooldownMax, int32 First, int32 Last)
    {
        G.Weight = Weight; G.CooldownMin = CooldownMin; G.CooldownMax = CooldownMax; G.FirstChapter = First; G.LastChapter = Last; G.Phases = Roaming;
    };
    // The house-presence props, moving where Nell can see them, each with a sound of its own now.
    { auto& G = AddGroup(TEXT("Props.Drawings"), {TEXT("A6_04*_HangingDrawing"), TEXT("A6_05*_HangingDrawing")}); Tune(G, .9f, 100.f, 180.f, 0, 3); G.bSilent = true; }
    { auto& G = AddGroup(TEXT("Props.Train"), {TEXT("A6_022_ToyTrain")}); Tune(G, 1.f, 200.f, 320.f, 0, 2); G.Sounds = Sounds({TEXT("SC_ToyTrain_00")}); G.bAlertsMimInHunt = true; G.NoiseLoudness = .45f; G.NoiseRange = 1300.f; }
    { auto& G = AddGroup(TEXT("Props.Mobile"), {TEXT("A6_023_NurseryMobile")}); Tune(G, .8f, 150.f, 260.f, 0, 2); G.Sounds = Sounds({TEXT("SC_Mobile_00")}); }
    { auto& G = AddGroup(TEXT("Props.Rocker"), {TEXT("A6_NurseryRockingChair")}); Tune(G, 1.f, 180.f, 300.f, 0, 2); G.Sounds = Sounds({TEXT("SC_Rocker_00")}); G.bAlertsMimInHunt = true; }
    { auto& G = AddGroup(TEXT("Props.Cup"), {TEXT("A6_024_TeaCup")}); Tune(G, .8f, 200.f, 320.f, 0, 2); G.Sounds = Sounds({TEXT("SC_Cup_Rattle_00")}); G.bAlertsMimInHunt = true; }
    { auto& G = AddGroup(TEXT("Props.Phone"), {TEXT("A6_020_PhoneReceiver")}); Tune(G, .7f, 400.f, 600.f, 2, 2); G.bMajor = true; G.bAlertsMimInHunt = true; G.NoiseLoudness = .65f; G.NoiseRange = 1500.f; G.Phases = {P::Exploration, P::ActiveHunt}; }
    // The last chapter's house only moves: every sound in it is Mim's.
    { auto& G = AddGroup(TEXT("Props.Still"), {TEXT("A6_04*_HangingDrawing"), TEXT("A6_05*_HangingDrawing"), TEXT("A6_NurseryRockingChair"), TEXT("A6_023_NurseryMobile")}); Tune(G, 1.f, 60.f, 120.f, 3, 3); G.bSilent = true; }
    { auto& G = AddGroup(TEXT("Plush.Small"), {}); Tune(G, 1.f, 90.f, 160.f, 0, 2); G.bPlush = true; G.Phases = {P::Exploration}; }
    // Sounds set out around her. None of them is Mim's: shoes on boards, doors, a toy, the walls, the old whispers.
    { auto& G = AddGroup(TEXT("Sound.StepsAbove"), {}); Tune(G, 1.4f, 150.f, 240.f, 0, 2); G.Place = ETallowScarePlace::Above; G.Volume = .5f;
      G.Sounds = Sounds({TEXT("SC_StepsAbove_00"), TEXT("SC_StepsAbove_01"), TEXT("SC_StepsAbove_02")}); }
    { auto& G = AddGroup(TEXT("Sound.DoorSomewhere"), {}); Tune(G, 1.3f, 140.f, 220.f, 0, 2); G.Place = ETallowScarePlace::Elsewhere; G.Volume = 1.f; G.bAlertsMimInHunt = true;
      G.Sounds = Sounds({TEXT("SC_DoorClose_00"), TEXT("SC_DoorClose_01"), TEXT("SC_DoorClose_02")}); }
    { auto& G = AddGroup(TEXT("Sound.MusicBox"), {}); Tune(G, 1.f, 240.f, 400.f, 0, 2); G.Place = ETallowScarePlace::Near; G.Volume = .45f;
      G.Sounds = Sounds({TEXT("SC_MusicBox_RunDown_00"), TEXT("SC_MusicBox_Wind_00")}); G.PreferredRooms = {TEXT("toys"), TEXT("upper_back"), TEXT("collection")}; }
    { auto& G = AddGroup(TEXT("Sound.WallKnock"), {}); Tune(G, 1.2f, 180.f, 280.f, 1, 2); G.Place = ETallowScarePlace::InWall; G.Volume = 1.f;
      G.Sounds = Sounds({TEXT("SC_WallKnock_00"), TEXT("SC_WallKnock_01"), TEXT("SC_WallKnock_02")}); }
    { auto& G = AddGroup(TEXT("Sound.Drag"), {}); Tune(G, 1.f, 200.f, 300.f, 1, 2); G.Place = ETallowScarePlace::Above; G.Volume = .5f; G.bAlertsMimInHunt = true;
      G.Sounds = Sounds({TEXT("SC_Drag_00"), TEXT("SC_Drag_01")}); }
    { auto& G = AddGroup(TEXT("Sound.Whisper"), {}); Tune(G, .9f, 240.f, 360.f, 1, 2); G.Place = ETallowScarePlace::Head; G.Phases = {P::Exploration, P::ActiveHunt}; }
    { auto& G = AddGroup(TEXT("Sound.Herring"), {}); Tune(G, 1.f, 120.f, 200.f, 1, 2); G.Place = ETallowScarePlace::Herring; G.Phases = {P::ActiveHunt, P::BatteryAcquired}; }
    { auto& G = AddGroup(TEXT("Sound.HeavyAbove"), {}); Tune(G, 1.f, 240.f, 360.f, 2, 2); G.Place = ETallowScarePlace::Above; G.Volume = .6f; G.bAlertsMimInHunt = true;
      G.Sounds = Sounds({TEXT("SC_StepsHeavyAbove_00")}); }
    { auto& G = AddGroup(TEXT("Sound.DoorSlam"), {}); Tune(G, .9f, 400.f, 600.f, 2, 2); G.Place = ETallowScarePlace::Elsewhere; G.bMajor = true; G.bAlertsMimInHunt = true;
      G.NoiseLoudness = .7f; G.NoiseRange = 1800.f; G.Sounds = Sounds({TEXT("SC_DoorSlam_00")}); G.Phases = {P::Exploration, P::ActiveHunt}; }
    // A steady trickle, about one every two to three minutes outside a chase, closer together as the chapters go.
    Campaign->Scares.MinGapSeconds = 70.f; Campaign->Scares.MaxGapSeconds = 160.f; Campaign->Scares.FirstGapSeconds = FVector2D(30.f, 60.f);
    Campaign->Scares.ChapterGapScale = {1.15f, 1.f, .85f, 1.2f}; Campaign->Scares.HuntGapScale = 1.f; Campaign->Scares.MaxPerChapter = 12;
    for (auto& Chapter : Campaign->Chapters)
        for (const auto& Group : Campaign->ScareGroups) Chapter->ScareGroups.Add(Group.Group);
    return Campaign;
}
