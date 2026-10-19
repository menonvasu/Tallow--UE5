"""Exact 2D polygon overlap test; run using isolated /tmp/keeper-uv-qc Python (numpy, shapely)."""
import json,sys,numpy as np,shapely
from pathlib import Path
out=Path(sys.argv[1]);tri=np.load(out/'uv-triangles.npy');polys=shapely.polygons(tri);areas=shapely.area(polys);tree=shapely.STRtree(polys);pairs=tree.query(polys,predicate='intersects');a,b=pairs[:,pairs[0]<pairs[1]];inter=shapely.area(shapely.intersection(polys[a],polys[b]));valid=inter>1e-13;bad=sorted(set(a[valid].tolist()+b[valid].tolist()))
report=dict(triangles=len(tri),collapsed_triangles=int((areas<1e-14).sum()),minimum_area=float(areas.min()),overlap_pairs=int(valid.sum()),overlap_area=float(inter[valid].sum()),overlap_faces=len(bad),method='exact Shapely triangle intersections; area tolerance 1e-13 UV units')
(out/'uv-overlap-faces.json').write_text(json.dumps(bad));(out/'uv-exact-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report));sys.exit(1 if bad or report['collapsed_triangles'] else 0)
