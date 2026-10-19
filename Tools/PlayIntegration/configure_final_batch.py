from pathlib import Path
import runpy
R=Path(__file__).resolve().parents[2]
for p in ['Tools/PlayIntegration/update_cinematic_props.py','Tools/PlayIntegration/configure_playable.py']:
 runpy.run_path(str(R/p),run_name='__main__')
