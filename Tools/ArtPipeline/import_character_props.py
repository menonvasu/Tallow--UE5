"""Import validated delivery packs into unused presentation namespaces only.

Run in a separate Unreal editor with PythonScriptPlugin enabled. A successful
import is a technical gate; camera, deformation and contact review are separate.
"""
import importlib.util
import json
import traceback
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Tallow/Saved/Presentation/character-prop-import.json'
OUT.parent.mkdir(parents=True, exist_ok=True)
report = {'completed': [], 'errors': []}

try:
    for name, relative in [
        ('player', 'Tallow/SourceAssets/Player/TripoPipeline/import_player_unreal.py'),
        ('props', 'Tools/ArtPipeline/import_hero_props.py'),
        ('staging', 'Tools/ArtPipeline/stage_hero_props_unreal.py'),
    ]:
        spec = importlib.util.spec_from_file_location('tallow_import_' + name, ROOT / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        report['completed'].append(name)
        OUT.write_text(json.dumps(report, indent=2) + '\n')
except Exception:
    report['errors'].append(traceback.format_exc())
    u.log_error(report['errors'][-1])
finally:
    OUT.write_text(json.dumps(report, indent=2) + '\n')
    u.SystemLibrary.quit_editor()
