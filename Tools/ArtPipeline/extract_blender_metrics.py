"""Read evaluated meshes from an explicit collection; never save or modify the blend."""
import argparse
import json
import sys
from pathlib import Path
import bpy
import bmesh

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--collection', required=True)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
collection = bpy.data.collections.get(args.collection)
if collection is None:
    raise ValueError(f'Collection not found: {args.collection}')
objects = sorted((o for o in collection.all_objects if o.type == 'MESH'), key=lambda o: o.name)
if not objects:
    raise ValueError('Collection contains no meshes')
depsgraph = bpy.context.evaluated_depsgraph_get()
records = []
world_points = []
for obj in objects:
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        mesh.calc_loop_triangles()
        bm = bmesh.new()
        try:
            bm.from_mesh(mesh)
            topology = {
                'non_manifold_edges': sum(not e.is_manifold for e in bm.edges),
                'degenerate_faces': sum(f.calc_area() <= 1e-12 for f in bm.faces),
                'loose_vertices': sum(not v.link_edges for v in bm.verts),
            }
        finally:
            bm.free()
        uv = mesh.uv_layers.get('ProductionUV')
        zero_area = 0
        out_of_bounds = 0
        if uv:
            for poly in mesh.polygons:
                coords = [uv.data[i].uv for i in poly.loop_indices]
                area = abs(sum(a.x * b.y - b.x * a.y for a, b in zip(coords, coords[1:] + coords[:1]))) / 2
                zero_area += area <= 1e-12
                out_of_bounds += any(v.x < -1e-6 or v.x > 1 + 1e-6 or v.y < -1e-6 or v.y > 1 + 1e-6 for v in coords)
        points = [evaluated.matrix_world @ v.co for v in mesh.vertices]
        world_points.extend(tuple(p) for p in points)
        records.append({'object': obj.name, 'triangles': len(mesh.loop_triangles), **topology,
                        'scale_applied': all(abs(v - 1) <= 1e-6 for v in obj.scale),
                        'production_uv_present': uv is not None,
                        'zero_area_faces': zero_area if uv else None,
                        'out_of_bounds_faces': out_of_bounds if uv else None})
    finally:
        evaluated.to_mesh_clear()
scale_cm = bpy.context.scene.unit_settings.scale_length * 100
bounds = [round((max(p[i] for p in world_points) - min(p[i] for p in world_points)) * scale_cm, 4) for i in range(3)] if world_points else None
report = {'source_blend': bpy.data.filepath, 'collection': args.collection,
          'evidence_scope': 'evaluated blend meshes only, not final export; UV provenance unverified',
          'objects': records,
          'mesh': {**{key: sum(r[key] for r in records) for key in ('triangles', 'non_manifold_edges', 'degenerate_faces', 'loose_vertices')},
                   'scale_applied': all(r['scale_applied'] for r in records), 'dimensions_cm': bounds, 'pivot': None},
          'uv': {'layer': 'ProductionUV', 'authored_after_import': None,
                 'zero_area_faces': sum(r['zero_area_faces'] for r in records) if all(r['production_uv_present'] for r in records) else None,
                 'out_of_bounds_faces': sum(r['out_of_bounds_faces'] for r in records) if all(r['production_uv_present'] for r in records) else None,
                 'overlap_fraction': None, 'coverage_fraction': None, 'padding_px_at_2048': None}}
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(f'Wrote measurements only: {args.output}')
