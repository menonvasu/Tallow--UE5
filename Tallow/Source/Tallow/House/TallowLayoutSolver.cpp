#include "House/TallowLayoutSolver.h"
#include "House/TallowHouseRooms.h"

namespace
{
using DS = ETallowDoorState;
using SK = ETallowSpotKind;
const FTallowSpotCandidate* Spot(const FTallowSolverInput& I,FName Name,SK Kind)
{
    for(const auto& S:I.Spots) if(S.Spot==Name && S.Kind==Kind) return &S;
    return nullptr;
}
bool BatteryAllowed(FName Room) { const auto* R=TallowHouseRooms::Find(Room); return !R || (R->bBatteryAllowed && !R->bPowerGated); }
bool ChairAllowed(FName Room) { const auto* R=TallowHouseRooms::Find(Room); return !R || (R->bChairAllowed && !R->bPowerGated); }
bool HasChair(const FTallowSolverInput& I,FName Room) { for(const auto& S:I.Spots) if(S.Kind==SK::Chair && S.Room==Room && ChairAllowed(Room)) return true; return false; }
TArray<const FTallowSpotCandidate*> Candidates(const FTallowSolverInput& I,SK Kind)
{
    TArray<const FTallowSpotCandidate*> Out;
    for(const auto& S:I.Spots) if(S.Kind==Kind && !S.Room.IsNone()) Out.Add(&S);
    Out.Sort([](const auto& A,const auto& B){ return A.Spot.LexicalLess(B.Spot); });
    return Out;
}
TArray<const FTallowHouseEdge*> Edges(const FTallowSolverInput& I)
{
    TArray<const FTallowHouseEdge*> Out; for(const auto& E:I.Graph.Edges) Out.Add(&E);
    Out.Sort([](const auto& A,const auto& B){return A.Id.LexicalLess(B.Id);}); return Out;
}
TArray<const FTallowSpotCandidate*> Stories(const FTallowSolverInput& I,const FTallowChapterRules& R)
{
    auto Out=Candidates(I,SK::Story);
    Out.RemoveAll([&](const auto* S){return S->ChapterIndex!=R.ChapterIndex || (!R.StoryCandidates.IsEmpty()&&!R.StoryCandidates.Contains(S->Spot)) || !HasChair(I,S->Room);});
    return Out;
}
TSet<FName> WithTool(const FTallowSolverInput& I,const FTallowChapterLayout& L,FName From,bool Power)
{
    auto Reach=FTallowLayoutSolver::Reachable(I.Graph,L,From,false,Power);
    const auto* Tool=Spot(I,L.CrowbarSpot,SK::Crowbar);
    if(Tool && Reach.Contains(Tool->Room)) Reach=FTallowLayoutSolver::Reachable(I.Graph,L,From,true,Power);
    return Reach;
}
int32 Count(const FTallowChapterLayout& L,DS State) { int32 N=0; for(const auto& D:L.Doors) N+=D.State==State; return N; }
bool Mutable(const FTallowChapterRules& R,const FTallowHouseEdge& E) { return FTallowHouseGraph::IsRollable(E)&&!R.Doors.FixedEdges.Contains(E.Id)&&!R.Doors.ForcedStates.Contains(E.Id); }
void PreferUnused(TArray<const FTallowSpotCandidate*>& List,const TArray<FName>& Used)
{
    const bool Fresh=List.ContainsByPredicate([&](const auto* S){return !Used.Contains(S->Spot);});
    if(Fresh) List.RemoveAll([&](const auto* S){return Used.Contains(S->Spot);});
}
void RepairCounts(const FTallowSolverInput& I,const FTallowChapterRules& R,FTallowChapterLayout& L,FRandomStream& Random)
{
    for(DS State:{DS::Crowbar,DS::Lockpick})
    {
        const int32 Min=State==DS::Crowbar?R.Doors.MinCrowbar:R.Doors.MinLockpick;
        const int32 Max=State==DS::Crowbar?R.Doors.MaxCrowbar:R.Doors.MaxLockpick;
        TArray<int32> Options;
        for(int32 N=0;N<L.Doors.Num();++N) { const auto* E=I.Graph.Find(L.Doors[N].Edge); if(E&&Mutable(R,*E)&&L.Doors[N].State==State) Options.Add(N); }
        while(Count(L,State)>Max && !Options.IsEmpty()) { const int32 N=Random.RandRange(0,Options.Num()-1); L.Doors[Options[N]].State=DS::Closed; Options.RemoveAt(N); }
        Options.Reset();
        for(int32 N=0;N<L.Doors.Num();++N)
        {
            const auto* E=I.Graph.Find(L.Doors[N].Edge);
            const DS Other=State==DS::Crowbar?DS::Lockpick:DS::Crowbar;
            const int32 OtherMin=Other==DS::Crowbar?R.Doors.MinCrowbar:R.Doors.MinLockpick;
            if(E&&Mutable(R,*E)&&E->AllowedStates.Contains(State)&&L.Doors[N].State!=State && (L.Doors[N].State!=Other || Count(L,Other)>OtherMin)) Options.Add(N);
        }
        while(Count(L,State)<Min && !Options.IsEmpty()) { const int32 N=Random.RandRange(0,Options.Num()-1); L.Doors[Options[N]].State=State; Options.RemoveAt(N); }
    }
}
void PickSpots(const FTallowSolverInput& I,const FTallowChapterRules& R,const FTallowLayoutHistory& H,FTallowChapterLayout& L,FRandomStream& Random,FName Retarget=NAME_None)
{
    const bool Power=I.bPowerRestored||R.Kind==ETallowChapterKind::Final;
    auto Pick=[&](const TArray<const FTallowSpotCandidate*>& C)->const FTallowSpotCandidate* { return C.IsEmpty()?nullptr:C[Random.RandRange(0,C.Num()-1)]; };
    if(R.Kind==ETallowChapterKind::Hunt && Retarget.IsNone())
    {
        auto C=Stories(I,R);
        if(!H.UsedChairRooms.IsEmpty())
        {
            const FName Last=H.UsedChairRooms.Last();
            if(C.ContainsByPredicate([&](const auto* S){return S->Room!=Last;})) C.RemoveAll([&](const auto* S){return S->Room==Last;});
        }
        PreferUnused(C,H.UsedStoryObjects);
        if(const auto* S=Pick(C)) { L.StoryObject=S->Spot; L.StoryRoom=L.ChairRoom=S->Room; }
    }
    if(!Retarget.IsNone()) L.ChairRoom=L.StoryRoom=Retarget;
    if(R.Kind==ETallowChapterKind::Hunt)
    {
        auto C=Candidates(I,SK::Chair); C.RemoveAll([&](const auto* S){return S->Room!=L.ChairRoom;});
        L.ChairSpot=NAME_None; if(const auto* S=Pick(C)) L.ChairSpot=S->Spot;
    }
    auto Tool=Candidates(I,SK::Crowbar);
    const auto Start=FTallowLayoutSolver::Reachable(I.Graph,L,L.StartRoom,false,Power);
    const auto Chair=FTallowLayoutSolver::Reachable(I.Graph,L,L.ChairRoom.IsNone()?L.StartRoom:L.ChairRoom,false,Power);
    Tool.RemoveAll([&](const auto* S){return !Start.Contains(S->Room)||!Chair.Contains(S->Room);});
    L.CrowbarSpot=NAME_None; if(const auto* S=Pick(Tool)) L.CrowbarSpot=S->Spot;
    if(R.bBattery)
    {
        auto C=Candidates(I,SK::Battery);
        const auto Accessible=WithTool(I,L,L.ChairRoom,Power);
        C.RemoveAll([&](const auto* S){return S->Room==L.ChairRoom||!BatteryAllowed(S->Room)||!Accessible.Contains(S->Room)||!WithTool(I,L,S->Room,Power).Contains(I.FuseBoxRoom);});
        PreferUnused(C,H.UsedBatterySpots);
        L.BatterySpot=L.BatteryRoom=NAME_None; if(const auto* S=Pick(C)) { L.BatterySpot=S->Spot; L.BatteryRoom=S->Room; }
    }
    auto Resources=[&](SK Kind,int32 Number,TArray<FName>& Out)
    {
        Out.Reset(); auto C=Candidates(I,Kind); TSet<int32> Floors;
        const auto Accessible=WithTool(I,L,L.StartRoom,Power); C.RemoveAll([&](const auto* S){return !Accessible.Contains(S->Room);});
        while(Out.Num()<Number&&!C.IsEmpty())
        {
            auto Fresh=C; Fresh.RemoveAll([&](const auto* S){const auto* Room=TallowHouseRooms::Find(S->Room);return Floors.Contains(Room?Room->Floor:0);});
            const auto* S=Pick(Fresh.IsEmpty()?C:Fresh); Out.Add(S->Spot); const auto* Room=TallowHouseRooms::Find(S->Room); Floors.Add(Room?Room->Floor:0); C.Remove(S);
        }
    };
    Resources(SK::Matchbox,R.Resources.Matchboxes,L.MatchboxSpots); Resources(SK::Wax,R.Resources.WaxPickups,L.WaxSpots);
    TMap<FName,int32> Distance; TArray<FName> Queue{L.ChairRoom.IsNone()?L.StartRoom:L.ChairRoom}; Distance.Add(Queue[0],0);
    for(int32 N=0;N<Queue.Num();++N) for(const auto* E:I.Graph.EdgesOf(Queue[N]))
        if(!Distance.Contains(E->Other(Queue[N]))) { Distance.Add(E->Other(Queue[N]),Distance[Queue[N]]+1); Queue.Add(E->Other(Queue[N])); }
    int32 Best=-1; L.MimHuntStart=NAME_None;
    for(const auto* S:Candidates(I,SK::MimHuntStart)) if(const auto* D=Distance.Find(S->Room);D&&*D>Best) { Best=*D; L.MimHuntStart=S->Spot; }
}
int32 Changed(const FTallowChapterLayout& L,const FTallowLayoutHistory& H)
{
    if(H.PreviousDoors.IsEmpty()) return L.DoorHash()!=H.PreviousDoorHash?1:0;
    int32 N=0;
    for(const auto& D:L.Doors) if(const auto* Old=H.PreviousDoors.FindByPredicate([&](const auto& O){return O.Edge==D.Edge;});Old&&Old->State!=D.State) ++N;
    return N;
}
}
TSet<FName> FTallowLayoutSolver::Reachable(const FTallowHouseGraph& G,const FTallowChapterLayout& L,FName From,bool HasTool,bool Power)
{
    TSet<FName> Seen; if(From.IsNone()) return Seen;
    TArray<FName> Queue{From}; Seen.Add(From);
    for(int32 N=0;N<Queue.Num();++N) for(const auto* E:G.EdgesOf(Queue[N]))
    {
        const DS State=L.GetDoorState(E->Id,E->DefaultState);
        if((E->bRequiresPower&&!Power)||State==DS::StoryGated||(State==DS::Crowbar&&!HasTool)) continue;
        FName Next=E->Other(Queue[N]); if(!Seen.Contains(Next)) {Seen.Add(Next);Queue.Add(Next);}
    }
    return Seen;
}
bool FTallowLayoutSolver::Validate(const FTallowSolverInput& I,const FTallowChapterRules& R,const FTallowChapterLayout& L,const FTallowLayoutHistory& H,TArray<FString>& Out)
{
    Out.Reset(); const bool Final=R.Kind==ETallowChapterKind::Final, Power=I.bPowerRestored||Final;
    auto Problem=[&](int32 Rule,const TCHAR* Why){Out.Add(FString::Printf(TEXT("L%d %s"),Rule,Why));};
    const auto FromStart=WithTool(I,L,I.StartRoom,Power);
    if(L.StartRoom!=I.StartRoom||I.StartRoom.IsNone()) Problem(1,TEXT("invalid start room"));
    if(!Final)
    {
        const auto* Story=Spot(I,L.StoryObject,SK::Story);
        if(!Story||Story->ChapterIndex!=R.ChapterIndex||(!R.StoryCandidates.IsEmpty()&&!R.StoryCandidates.Contains(L.StoryObject))||!FromStart.Contains(L.StoryRoom)) Problem(1,TEXT("story unavailable or unreachable"));
        const auto* Chair=Spot(I,L.ChairSpot,SK::Chair);
        if(L.ChairRoom!=L.StoryRoom||!Chair||Chair->Room!=L.ChairRoom||!ChairAllowed(L.ChairRoom)) Problem(2,TEXT("chair must be usable in story room"));
        if(!H.UsedChairRooms.IsEmpty()&&L.ChairRoom==H.UsedChairRooms.Last())
            for(const auto* S:Stories(I,R)) if(S->Room!=L.ChairRoom&&FromStart.Contains(S->Room)) {Problem(2,TEXT("chair room repeats despite alternative"));break;}
    }
    if(R.bBattery)
    {
        const auto* Battery=Spot(I,L.BatterySpot,SK::Battery);
        if(!Battery||Battery->Room!=L.BatteryRoom||!WithTool(I,L,L.ChairRoom,Power).Contains(L.BatteryRoom)||!WithTool(I,L,L.BatteryRoom,Power).Contains(I.FuseBoxRoom)) Problem(3,TEXT("battery or fuse box unreachable"));
        if(L.BatteryRoom==L.ChairRoom||!BatteryAllowed(L.BatteryRoom)) Problem(4,TEXT("battery room not allowed"));
        if(H.UsedBatterySpots.Contains(L.BatterySpot)) for(const auto* S:Candidates(I,SK::Battery))
            if(S->Room!=L.ChairRoom&&BatteryAllowed(S->Room)&&!H.UsedBatterySpots.Contains(S->Spot)&&WithTool(I,L,L.ChairRoom,Power).Contains(S->Room)) {Problem(4,TEXT("battery spot repeats despite unused alternative"));break;}
    }
    if(R.Resources.bCrowbarInHouse||Count(L,DS::Crowbar)>0)
    {
        const auto* Tool=Spot(I,L.CrowbarSpot,SK::Crowbar);
        if(!Tool||!Reachable(I.Graph,L,I.StartRoom,false,Power).Contains(Tool->Room)||!Reachable(I.Graph,L,L.ChairRoom.IsNone()?I.StartRoom:L.ChairRoom,false,Power).Contains(Tool->Room)) Problem(5,TEXT("crowbar behind a crowbar edge or missing"));
    }
    if(!Final || H.PreviousDoors.IsEmpty())
        if(Count(L,DS::Lockpick)<R.Doors.MinLockpick||Count(L,DS::Lockpick)>R.Doors.MaxLockpick||Count(L,DS::Crowbar)<R.Doors.MinCrowbar||Count(L,DS::Crowbar)>R.Doors.MaxCrowbar) Problem(6,TEXT("door count outside chapter bounds"));
    for(const auto& Forced:R.Doors.ForcedStates) if(!I.Graph.Find(Forced.Key)) Problem(6,TEXT("forced edge missing from graph"));
    for(FName Fixed:R.Doors.FixedEdges) if(!I.Graph.Find(Fixed)) Problem(6,TEXT("fixed edge missing from graph"));
    TSet<FName> Assigned;
    for(const auto& D:L.Doors)
    {
        const auto* E=I.Graph.Find(D.Edge);
        if(!E||Assigned.Contains(D.Edge)||!E->AllowedStates.Contains(D.State)) Problem(6,TEXT("unknown, duplicate or unsupported assignment"));
        Assigned.Add(D.Edge);
    }
    for(const auto& E:I.Graph.Edges)
    {
        if(!Assigned.Contains(E.Id)) Problem(6,TEXT("missing edge assignment"));
        const DS State=L.GetDoorState(E.Id,E.DefaultState);
        if(const DS* Forced=R.Doors.ForcedStates.Find(E.Id);Forced&&*Forced!=State) Problem(6,TEXT("forced state ignored"));
        if(R.Doors.FixedEdges.Contains(E.Id)&&State!=E.DefaultState) Problem(6,TEXT("fixed edge changed"));
    }
    TSet<FName> Rooms; for(const auto& E:I.Graph.Edges) {Rooms.Add(E.RoomA);Rooms.Add(E.RoomB);}
    for(FName Room:Rooms)
    {
        const auto* Def=TallowHouseRooms::Find(Room);
        if(Def&&Def->bPowerGated&&!Power) continue;
        if(!FromStart.Contains(Room)) Problem(7,*FString::Printf(TEXT("orphan room %s"),*Room.ToString()));
        bool AllBlocked=true;
        for(const auto* E:I.Graph.EdgesOf(Room)) {const DS S=L.GetDoorState(E->Id,E->DefaultState);if(S!=DS::Crowbar&&S!=DS::StoryGated) AllBlocked=false;}
        if(AllBlocked) Problem(8,*FString::Printf(TEXT("every entrance blocked in %s"),*Room.ToString()));
    }
    if(Final) for(FName Room:R.RequiredRooms) if(!FromStart.Contains(Room)) Problem(10,TEXT("required final room unreachable with power"));
    if(!Final&&!L.bFallback&&(H.PreviousDoorHash||!H.PreviousDoors.IsEmpty())&&Changed(L,H)<R.Doors.MinChangedFromPrevious)
    {
        // Only demand variation when another legal assignment exists. Exhaust the tiny door domain,
        // keeping the selected objects, and validate without history variation recursively.
        bool Possible=false; FTallowChapterLayout Other=L; FTallowLayoutHistory NoPrevious=H; NoPrevious.PreviousDoorHash=0;NoPrevious.PreviousDoors.Reset();
        const auto Sorted=Edges(I);
        TFunction<void(int32)> Search=[&](int32 N)
        {
            if(Possible) return;
            if(N==Sorted.Num()) {TArray<FString> Problems;if(Changed(Other,H)>=R.Doors.MinChangedFromPrevious&&Validate(I,R,Other,NoPrevious,Problems)) Possible=true;return;}
            const auto* E=Sorted[N];
            auto* D=Other.Doors.FindByPredicate([&](const auto& A){return A.Edge==E->Id;}); if(!D) return;
            const DS Old=D->State;
            if(Mutable(R,*E)) for(DS S:E->AllowedStates) {D->State=S;Search(N+1);if(Possible) break;}
            else Search(N+1);
            D->State=Old;
        };
        Search(0); if(Possible) Problem(9,TEXT("insufficient changed door edges"));
    }
    if(Final&&!H.PreviousDoors.IsEmpty()) for(const auto& Old:H.PreviousDoors) if(L.GetDoorState(Old.Edge,Old.State)!=Old.State) Problem(9,TEXT("final chapter changed previous doors"));
    Out.Sort();
    return Out.IsEmpty();
}
FTallowChapterLayout FTallowLayoutSolver::Solve(const FTallowSolverInput& I,const FTallowChapterRules& R,int32 Seed,const FTallowLayoutHistory& H)
{
    TArray<FString> Notes;
    for(int32 Attempt=0;Attempt<MaxAttempts;++Attempt)
    {
        FTallowChapterLayout L; L.Seed=Seed; L.Attempt=Attempt; L.StartRoom=I.StartRoom;
        FRandomStream Random(int32(uint32(Seed)^((uint32(Attempt)+1u)*0x9e3779b9u)));
        for(const auto* E:Edges(I))
        {
            DS State=E->DefaultState;
            if(Mutable(R,*E))
            {
                float Total=0; TArray<float> Weights;
                for(DS S:E->AllowedStates) {float Weight=R.Doors.StateWeights.IsEmpty()?(S==DS::Open?.25f:S==DS::Closed?.35f:S==DS::Lockpick?.25f:S==DS::Crowbar?.15f:0.f):R.Doors.StateWeights.FindRef(S); Weights.Add(FMath::Max(0.f,Weight));Total+=Weights.Last();}
                float Roll=Random.FRand()*Total;
                for(int32 N=0;N<E->AllowedStates.Num();++N) if((Roll-=Weights[N])<0) {State=E->AllowedStates[N];break;}
            }
            if(const DS* Forced=R.Doors.ForcedStates.Find(E->Id)) State=*Forced;
            if(R.Kind==ETallowChapterKind::Final) if(const auto* Old=H.PreviousDoors.FindByPredicate([&](const auto& D){return D.Edge==E->Id;})) State=Old->State;
            L.Doors.Add({E->Id,State});
        }
        if(R.Kind!=ETallowChapterKind::Final||H.PreviousDoors.IsEmpty()) RepairCounts(I,R,L,Random);
        PickSpots(I,R,H,L,Random);
        TArray<FString> Problems;
        if(Validate(I,R,L,H,Problems)) {L.Notes=MoveTemp(Notes);return L;}
        Notes.Add(FString::Printf(TEXT("attempt %d: %s"),Attempt,*FString::Join(Problems,TEXT("; "))));
    }
    auto L=SafeFallback(I,R); L.Seed=Seed; L.Attempt=MaxAttempts;
    FRandomStream Random(Seed); PickSpots(I,R,H,L,Random); L.Notes=MoveTemp(Notes);
    TArray<FString> Problems; if(!Validate(I,R,L,H,Problems)) L.Notes.Add(TEXT("INVALID FALLBACK: ")+FString::Join(Problems,TEXT("; ")));
    return L;
}
FTallowChapterLayout FTallowLayoutSolver::SafeFallback(const FTallowSolverInput& I,const FTallowChapterRules& R)
{
    FTallowChapterLayout L; L.bFallback=true;L.StartRoom=I.StartRoom;
    const auto Sorted=Edges(I);
    for(const auto* E:Sorted) {DS State=FTallowHouseGraph::IsRollable(*E)?DS::Closed:E->DefaultState;if(const DS* Forced=R.Doors.ForcedStates.Find(E->Id)) State=*Forced;if(R.Doors.FixedEdges.Contains(E->Id)) State=E->DefaultState;L.Doors.Add({E->Id,State});}
    if((R.Kind==ETallowChapterKind::Hunt&&Stories(I,R).IsEmpty())||(R.bBattery&&Candidates(I,SK::Battery).IsEmpty())) {L.Notes.Add(TEXT("INVALID FALLBACK: required real spots missing"));return L;}
    bool Found=false;
    TFunction<void(int32)> Search=[&](int32 N)
    {
        if(Found) return;
        if(N==Sorted.Num())
        {
            for(int32 Pick=0;Pick<MaxAttempts;++Pick) {FRandomStream Random(Pick);PickSpots(I,R,{},L,Random);TArray<FString> Problems;if(Validate(I,R,L,{},Problems)) {Found=true;return;}}
            return;
        }
        if(!Mutable(R,*Sorted[N])) {Search(N+1);return;}
        TArray<DS> States=Sorted[N]->AllowedStates; States.Sort([](DS A,DS B){return (A==DS::Closed?-1:int32(A))<(B==DS::Closed?-1:int32(B));});
        for(DS S:States) {L.Doors[N].State=S;Search(N+1);if(Found) return;}
    };
    Search(0); if(!Found) L.Notes.Add(TEXT("INVALID FALLBACK: input has no valid layout"));
    return L;
}
bool FTallowLayoutSolver::Retarget(const FTallowSolverInput& I,const FTallowChapterRules& R,FName Room,int32 Seed,const FTallowLayoutHistory& H,FTallowChapterLayout& InOut)
{
    if(!HasChair(I,Room)) return false;
    for(int32 N=0;N<MaxAttempts;++N)
    {
        auto L=InOut;FRandomStream Random(int32(uint32(Seed)+uint32(N)));PickSpots(I,R,H,L,Random,Room);
        L.CrowbarSpot=InOut.CrowbarSpot;L.MatchboxSpots=InOut.MatchboxSpots;L.WaxSpots=InOut.WaxSpots;L.MimHuntStart=InOut.MimHuntStart;
        TArray<FString> Problems;if(Validate(I,R,L,H,Problems)) {InOut=MoveTemp(L);return true;}
    }
    return false;
}
