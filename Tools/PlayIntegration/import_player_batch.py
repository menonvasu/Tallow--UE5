import runpy
from pathlib import Path
R=Path(__file__).resolve().parents[2]
for p in ['Tools/PlayerIntegration/import_interaction_unreal.py','Tools/PlayerIntegration/import_tools_unreal.py','Tools/PlayerIntegration/import_supplement_unreal.py']:
 runpy.run_path(str(R/p),run_name='__main__')
