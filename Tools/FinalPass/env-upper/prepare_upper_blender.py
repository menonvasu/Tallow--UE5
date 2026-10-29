"""Prepare the upstairs BlenderKit models for Unreal with ../env/prepare_props_blender.py and this folder's recipes
(join, fit, pivot at the base centre, budget, and the model's own maps baked as Unreal PNGs).

blender -b --factory-startup <model.blend> --python prepare_upper_blender.py -- <name> <out_dir>
Then: python3 -I stamp_upper.py <SourceAssets/FinalPass/Env> and run.sh headless import_upper_meshes.py.
"""
from pathlib import Path

SPEC = {
    # Toy room: the plank-cut rocking horse at the generated one's length (124 cm along its rockers).
    'RockingHorse': dict(sm='SM_Env_U1_RockingHorse', drop=(), scale=1.24, budget=16000, bake=True),
}
src = Path(__file__).resolve().parent.parent / 'env' / 'prepare_props_blender.py'
code = src.read_text().replace('SPEC = {', 'SPEC = {**UPPER_SPEC, ', 1)
exec(compile(code, str(src), 'exec'), {'__name__': '__main__', '__file__': str(src), 'UPPER_SPEC': SPEC})
