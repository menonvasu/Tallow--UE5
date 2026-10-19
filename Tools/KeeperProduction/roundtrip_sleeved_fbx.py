"""FBX roundtrip structure check in clean headless Blender, no Unreal project mutation."""
import bpy,json
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(OUT/'Export/SK_Keeper_Tripo_Direct.fbx'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE'];assert len(meshes)==1 and len(rigs)==1
m=meshes[0];r=rigs[0];m.data.calc_loop_triangles();triangles=len(m.data.loop_triangles);assert triangles==78554;assert len(r.data.bones)==109
hands=[b.name for b in r.data.bones if b.name.endswith('_hand')];assert len(hands)==6
keys=list(m.data.shape_keys.key_blocks.keys()) if m.data.shape_keys else [];assert 'FULL_HORROR' in keys
assert len(m.data.uv_layers)>=1
report=dict(roundtrip='Blender FBX re-import',triangles=triangles,bones=len(r.data.bones),hands=hands,morphs=keys,uv_layers=[l.name for l in m.data.uv_layers],mesh_dimensions=list(m.dimensions),engine_import_tested=False)
(OUT/'fbx-roundtrip.json').write_text(json.dumps(report,indent=2));print('FBX_ROUNDTRIP '+json.dumps(report))
