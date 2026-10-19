#!/usr/bin/env python3
"""Read-only production art gate. Metrics must come from the inspected export mesh."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def check(data, root):
    errors = []
    def require(ok, message):
        if not ok:
            errors.append(message)
    def number(section, key, low, high):
        value = section.get(key)
        require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high,
                f'{key} must be finite in [{low}, {high}]')
    def artifact(section, key):
        name = section.get(key)
        path = root / name if isinstance(name, str) else None
        if path is None or not path.is_file():
            errors.append(f'{key}: missing file')
            return None
        if path.read_bytes()[:43].startswith(b'version https://git-lfs.github.com/spec/v1'):
            errors.append(f'{key}: unresolved LFS pointer')
            return None
        return path
    require(data.get('schema_version') == 1, 'schema_version must be 1')
    require(data.get('asset_id') == 'crooked_toy_cabinet', 'this pilot gate is scoped to crooked_toy_cabinet')
    raw = artifact(data, 'raw_source')
    production = artifact(data, 'production_source')
    export = artifact(data, 'export_mesh')
    if raw and production:
        require(raw.resolve() != production.resolve(), 'raw source must remain separate')
        require(hashlib.sha256(raw.read_bytes()).digest() != hashlib.sha256(production.read_bytes()).digest(),
                'production source cannot be a byte-for-byte raw copy')
    uv = data.get('uv', {})
    require(uv.get('layer') == 'ProductionUV', 'production layer must be ProductionUV')
    require(uv.get('authored_after_import') is True, 'new production unwrap required; generated UVs are not production UVs')
    number(uv, 'zero_area_faces', 0, 0)
    number(uv, 'out_of_bounds_faces', 0, 0)
    number(uv, 'overlap_fraction', 0, 0.001)
    number(uv, 'coverage_fraction', 0.45, 1)
    number(uv, 'padding_px_at_2048', 8, 2048)
    mesh = data.get('mesh', {})
    budget = 20000 if data.get('focal_asset_approved') is True else 8000
    number(mesh, 'triangles', 1, budget)
    for key in ('non_manifold_edges', 'degenerate_faces', 'loose_vertices'):
        number(mesh, key, 0, 0)
    require(mesh.get('scale_applied') is True, 'apply mesh scale before export')
    require(mesh.get('pivot') == 'base_center', 'pilot pivot must be base_center')
    dims = mesh.get('dimensions_cm')
    require(isinstance(dims, list) and len(dims) == 3 and all(type(v) in (int, float) and math.isfinite(v) and 0 < v <= cap + 0.001 for v, cap in zip(dims, [140,45,190])), 'pilot must fit within 140 x 45 x 190 cm')
    review = data.get('review', {})
    for key in ('checker_front', 'checker_back', 'checker_closeup'):
        artifact(review, key)
    require(review.get('checker_approved') is True, 'checker review approval required')
    require(bool(review.get('reviewer')), 'record reviewer identity')
    if export:
        require(data.get('export_sha256') == hashlib.sha256(export.read_bytes()).hexdigest(),
                'export_sha256 does not match inspected export')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    try:
        data = json.loads(args.manifest.read_text())
        errors = check(data, args.manifest.parent)
    except (OSError, ValueError, TypeError, AttributeError) as exc:
        errors = [f'invalid manifest: {exc}']
    print(json.dumps({'passed': not errors, 'errors': errors}, indent=2))
    return int(bool(errors))

if __name__ == '__main__':
    raise SystemExit(main())
