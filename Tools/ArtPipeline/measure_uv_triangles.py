"""Exact planar UV triangle union metrics using Shapely; run on Blender-exported UV triangles."""
import argparse,json
from pathlib import Path
from shapely.geometry import Polygon,box
from shapely.ops import unary_union
from shapely import set_precision
p=argparse.ArgumentParser();p.add_argument('triangles',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
triangles=json.loads(a.triangles.read_text());polys=[Polygon(t) for t in triangles];zero=sum(p.area<=1e-12 for p in polys);outside=sum(not box(-1e-6,-1e-6,1+1e-6,1+1e-6).covers(p) for p in polys)
merged=unary_union(polys);area=merged.area;overlap=max(0,sum(p.area for p in polys)-area)
# Snap 1e-7 UV roundoff only for contiguous-island separation measurement.
snapped=unary_union([set_precision(p,1e-7) for p in polys]);islands=list(snapped.geoms) if hasattr(snapped,'geoms') else [snapped]
minimum=min((islands[i].distance(islands[j]) for i in range(len(islands)) for j in range(i+1,len(islands))),default=None)
report={'method':'Shapely triangle union; overlap is multiplicity excess / union area; island distance at 1e-7 precision', 'zero_area_faces':zero,'out_of_bounds_faces':outside,'overlap_fraction':overlap/area if area else None,'coverage_fraction':merged.intersection(box(0,0,1,1)).area,'padding_px_at_2048':minimum*2048 if minimum is not None else None,'islands':len(islands)}
a.output.write_text(json.dumps(report,indent=2));print(json.dumps(report))
