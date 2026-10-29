#include "House/TallowHouseGraph.h"

const FTallowHouseEdge* FTallowHouseGraph::Find(FName Id) const
{
    for(const auto& Edge:Edges) if(Edge.Id==Id) return &Edge;
    return nullptr;
}
TArray<const FTallowHouseEdge*> FTallowHouseGraph::EdgesOf(FName Room) const
{
    TArray<const FTallowHouseEdge*> Result;
    for(const auto& Edge:Edges) if(Edge.Joins(Room)) Result.Add(&Edge);
    return Result;
}
const FTallowHouseGraph& TallowHouseGraph::Get()
{
    static const FTallowHouseGraph Graph=[]
    {
        FTallowHouseGraph G;
#include "House/TallowHouseGraphData.inl"
        return G;
    }();
    return Graph;
}
