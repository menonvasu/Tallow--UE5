// Generated from Tallow.Layout.ProbeGraph. Do not edit by hand.
{
    FTallowHouseEdge E;
    E.Id = TEXT("BP_TallowDoor"); E.RoomA = TEXT("cellar_hall"); E.RoomB = TEXT("workshop");
    E.Kind = ETallowEdgeKind::Door;
    E.DoorActor = TEXT("BP_TallowDoor_C_3"); E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(584.997, -1000.000, -330.000);
    E.AllowedStates = {ETallowDoorState::Open, ETallowDoorState::Closed, ETallowDoorState::Lockpick, ETallowDoorState::Crowbar};
    E.ProbeStart = FVector(280.000, -960.000, -330.000);
    E.ProbeEnd = FVector(805.000, -660.000, -330.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("DOOR_BedroomLocked"); E.RoomA = TEXT("bedroom"); E.RoomB = TEXT("upper_hall");
    E.Kind = ETallowEdgeKind::Door;
    E.DoorActor = TEXT("BP_TallowDoor_C_14"); E.BoardsActor = TEXT("TallowOptionalBoards_6"); E.GateActor = NAME_None;
    E.Doorway = FVector(974.998, -110.000, 390.000);
    E.AllowedStates = {ETallowDoorState::Open, ETallowDoorState::Closed, ETallowDoorState::Lockpick, ETallowDoorState::Crowbar};
    E.ProbeStart = FVector(685.000, -420.000, 375.357);
    E.ProbeEnd = FVector(1255.000, -310.000, 390.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("DOOR_BedroomLocked_Nursery"); E.RoomA = TEXT("bedroom"); E.RoomB = TEXT("upper_back");
    E.Kind = ETallowEdgeKind::Door;
    E.DoorActor = TEXT("BP_TallowDoor_C_17"); E.BoardsActor = TEXT("TallowOptionalBoards_7"); E.GateActor = NAME_None;
    E.Doorway = FVector(1172.105, -650.000, 390.000);
    E.AllowedStates = {ETallowDoorState::Open, ETallowDoorState::Closed, ETallowDoorState::Lockpick, ETallowDoorState::Crowbar};
    E.ProbeStart = FVector(1255.000, -310.000, 390.000);
    E.ProbeEnd = FVector(1255.000, -960.000, 390.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("DOOR_StudyLocked"); E.RoomA = TEXT("hall"); E.RoomB = TEXT("study");
    E.Kind = ETallowEdgeKind::Door;
    E.DoorActor = TEXT("BP_TallowDoor_C_5"); E.BoardsActor = TEXT("TallowOptionalBoards_4"); E.GateActor = NAME_None;
    E.Doorway = FVector(974.998, -100.000, 0.000);
    E.AllowedStates = {ETallowDoorState::Open, ETallowDoorState::Closed, ETallowDoorState::Lockpick, ETallowDoorState::Crowbar};
    E.ProbeStart = FVector(790.000, -660.000, 0.000);
    E.ProbeEnd = FVector(1315.000, -330.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("DOOR_StudyLocked_Kitchen"); E.RoomA = TEXT("back"); E.RoomB = TEXT("study");
    E.Kind = ETallowEdgeKind::Door;
    E.DoorActor = TEXT("BP_TallowDoor_C_9"); E.BoardsActor = TEXT("TallowOptionalBoards_5"); E.GateActor = NAME_None;
    E.Doorway = FVector(1440.000, -649.998, 0.000);
    E.AllowedStates = {ETallowDoorState::Open, ETallowDoorState::Closed, ETallowDoorState::Lockpick, ETallowDoorState::Crowbar};
    E.ProbeStart = FVector(1315.000, -330.000, 0.000);
    E.ProbeEnd = FVector(1370.000, -960.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("FinalCollectionPowerGate"); E.RoomA = TEXT("collection"); E.RoomB = TEXT("upper_hall");
    E.Kind = ETallowEdgeKind::RouteGate;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = TEXT("TallowRouteGate_1");
    E.Doorway = FVector(585.001, -1122.083, 390.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(485.000, -1248.000, 390.000);
    E.ProbeEnd = FVector(685.000, -1248.000, 390.000);
    E.bRequiresPower = true; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("back__hall"); E.RoomA = TEXT("back"); E.RoomB = TEXT("hall");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(975.001, -918.000, 0.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(790.000, -660.000, 0.000);
    E.ProbeEnd = FVector(1370.000, -960.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("back__laundry"); E.RoomA = TEXT("back"); E.RoomB = TEXT("laundry");
    E.Kind = ETallowEdgeKind::Stairs;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(1274.783, -976.956, -130.001);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(1375.000, -960.000, -327.549);
    E.ProbeEnd = FVector(1370.000, -960.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("back__upper_back"); E.RoomA = TEXT("back"); E.RoomB = TEXT("upper_back");
    E.Kind = ETallowEdgeKind::Stairs;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(1447.302, -1065.872, 269.999);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(1370.000, -960.000, 0.000);
    E.ProbeEnd = FVector(1255.000, -960.000, 390.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("boiler__cellar_hall"); E.RoomA = TEXT("boiler"); E.RoomB = TEXT("cellar_hall");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(974.998, -300.000, -330.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(805.000, -660.000, -330.000);
    E.ProbeEnd = FVector(1255.000, -310.000, -330.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("boiler__laundry"); E.RoomA = TEXT("boiler"); E.RoomB = TEXT("laundry");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(1329.375, -649.999, -315.833);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(1255.000, -310.000, -330.000);
    E.ProbeEnd = FVector(1375.000, -960.000, -327.549);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("cellar_hall__laundry"); E.RoomA = TEXT("cellar_hall"); E.RoomB = TEXT("laundry");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(974.998, -1034.998, -330.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(805.000, -660.000, -330.000);
    E.ProbeEnd = FVector(1375.000, -960.000, -327.549);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("dining__hall"); E.RoomA = TEXT("dining"); E.RoomB = TEXT("hall");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(585.001, -1188.684, 0.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(512.376, -1184.862, 0.000);
    E.ProbeEnd = FVector(660.000, -1210.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("dining__parlour"); E.RoomA = TEXT("dining"); E.RoomB = TEXT("parlour");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(327.142, -650.002, 0.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(280.000, -290.000, 0.000);
    E.ProbeEnd = FVector(280.000, -960.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("hall__parlour"); E.RoomA = TEXT("hall"); E.RoomB = TEXT("parlour");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(584.998, -335.967, 0.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(280.000, -290.000, 0.000);
    E.ProbeEnd = FVector(790.000, -660.000, 0.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("hall__upper_hall"); E.RoomA = TEXT("hall"); E.RoomB = TEXT("upper_hall");
    E.Kind = ETallowEdgeKind::Stairs;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(694.855, -610.532, 270.001);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(790.000, -660.000, 0.000);
    E.ProbeEnd = FVector(685.000, -420.000, 375.357);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("toys__upper_hall"); E.RoomA = TEXT("toys"); E.RoomB = TEXT("upper_hall");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(584.999, -337.727, 390.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(280.000, -310.000, 390.000);
    E.ProbeEnd = FVector(685.000, -420.000, 375.357);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("upper_back__upper_hall"); E.RoomA = TEXT("upper_back"); E.RoomB = TEXT("upper_hall");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(974.998, -1105.001, 390.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(875.000, -1248.000, 390.000);
    E.ProbeEnd = FVector(1075.000, -1248.000, 390.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
{
    FTallowHouseEdge E;
    E.Id = TEXT("wake__workshop"); E.RoomA = TEXT("wake"); E.RoomB = TEXT("workshop");
    E.Kind = ETallowEdgeKind::Opening;
    E.DoorActor = NAME_None; E.BoardsActor = NAME_None; E.GateActor = NAME_None;
    E.Doorway = FVector(324.286, -649.999, -330.000);
    E.AllowedStates = {ETallowDoorState::Open};
    E.ProbeStart = FVector(298.000, -304.000, -330.000);
    E.ProbeEnd = FVector(280.000, -960.000, -330.000);
    E.bRequiresPower = false; E.bPlayerOnly = false;
    G.Edges.Add(E);
}
