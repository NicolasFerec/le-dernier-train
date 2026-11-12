"""Update lighting in existing maps without regenerating geometry/navigation.
UnrealEditor-Cmd project -run=pythonscript -script=Scripts/rebuild_lighting.py -NullRHI
Optional -TrainLightingWagonOnly for a quick visual iteration.
"""
from pathlib import Path
import shutil
import sys
import unreal as ue
ROOT = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Scripts'))
from lighting import wagon, exterior
levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
names = ['Wagon']
if 'TrainLightingWagonOnly' not in ue.SystemLibrary.get_command_line():
    names += ['Pompage'] + [f'Faubourgs{i:02d}' for i in range(2, 11)] + ['Fonderie']
for name in names:
    backup = ROOT / 'Saved/Checkpoints/Lighting' / (name + '.umap')
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(ROOT / 'Content/Maps' / backup.name, backup)
    assert levels.load_level('/Game/Maps/' + name)
    wagon() if name == 'Wagon' else exterior()
    lamps = [a.point_light_component for a in actors.get_all_level_actors() if isinstance(a, ue.PointLight)]
    assert lamps and all(c.cast_shadows and c.use_inverse_squared_falloff for c in lamps)
    assert levels.save_current_level()
    print('TRAIN_LIGHTING_SAVED', name, 'shadow-casting lamps', len(lamps))
print('TRAIN_LIGHTING_COMPLETE')
