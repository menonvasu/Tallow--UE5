"""Keep the final candle/player prop consistent in the existing authored cinematics."""
import unreal as u,runpy
from pathlib import Path
R=Path(__file__).resolve().parents[2];H=runpy.run_path(str(R/'Tools/Presentation/refine_cinematic_staging.py'),run_name='helpers')
ED=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
for mapname in ['Lvl_MenuPresentation','Lvl_OutroPresentation']:
 assert ED.load_level('/Game/Presentation/Cinematics/Maps/'+mapname)
 actors={a.get_actor_label():a for a in A.get_all_level_actors()}
 for a in actors.values():
  if isinstance(a,u.TallowCandlePresentationActor):
   a.candle.set_static_mesh(u.load_asset('/Game/Presentation/CandleV02/SM_CandleV02_Crooked'))
   a.wick.set_relative_location(u.Vector(.4,0,25.5),False,False)
 if mapname=='Lvl_OutroPresentation':
  hands=actors['LastFlameHands'];sc=hands.skeletal_mesh_component
  sc.set_skeletal_mesh_asset(u.load_asset('/Game/Presentation/PlayerInteractionV05/SK_PlayerV05_Partial'))
  sc.override_animation_data(u.load_asset('/Game/Presentation/PlayerInteractionV05/AN_PlayerV05_Hold'),False,False,0,0)
  camera=actors['LastFlameCamera'];rotation=camera.get_actor_rotation();transform=camera.get_actor_transform()
  candle=actors['LastFlame'];position=u.MathLibrary.transform_location(transform,u.Vector(44.5,22.5,-27.5))
  candle_rot=u.MathLibrary.compose_rotators(u.Rotator(pitch=0,yaw=90,roll=0),rotation)
  candle.set_actor_location_and_rotation(position,candle_rot,False,True)
  sequence=u.load_asset('/Game/Presentation/Cinematics/LS_Outro_LastFlame')
  tracks=H['transform_tracks'](sequence);assert len(tracks)==3
  delta=u.MathLibrary.get_forward_vector(rotation)*50
  H['rewrite_transform'](tracks[1],[(0,position,candle_rot),(210,position+delta,candle_rot),(300,position+delta,candle_rot)])
  u.EditorAssetLibrary.save_loaded_asset(sequence,False)
 assert ED.save_current_level()
u.log('TALLOW_CINEMATIC_PROPS_UPDATED')
