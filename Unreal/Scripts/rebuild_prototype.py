"""Reapply presentation and rebuild maps. Character retargeting is a separate, slower import."""
from pathlib import Path
import runpy
folder=Path(__file__).resolve().parent
for name in ['finalize_surfaces.py','rebuild_world.py','import_audio.py']:
    runpy.run_path(str(folder/name),run_name='__main__')
print('TRAIN_PROTOTYPE_REBUILT')
