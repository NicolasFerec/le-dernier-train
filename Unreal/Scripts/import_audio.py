"""Import the original Godot soundscape and create real volume buses for Unreal."""
import unreal as ue
from pathlib import Path
root = Path(ue.Paths.project_dir())
tools = ue.AssetToolsHelpers.get_asset_tools()
ue.EditorAssetLibrary.make_directory('/Game/Audio')
def asset(name, cls, factory):
    path = '/Game/Audio/' + name
    return ue.load_asset(path) or tools.create_asset(name, '/Game/Audio', cls, factory())
master = asset('Master', ue.SoundClass, ue.SoundClassFactory)
music = asset('Music', ue.SoundClass, ue.SoundClassFactory)
effects = asset('Effects', ue.SoundClass, ue.SoundClassFactory)
master.set_editor_property('child_classes', [music, effects])
mix = asset('TrainMix', ue.SoundMix, ue.SoundMixFactory)
for source in sorted((root/'SourceArt/Audio').glob('*.wav')):
    task = ue.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/Game/Audio'
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    sound = ue.load_asset('/Game/Audio/' + source.stem)
    is_loop=source.stem in {'depot_music','depot_air','wagon_music','wagon_air'}
    sound.set_editor_property('sound_class_object', music if is_loop else effects)
    sound.set_editor_property('looping',is_loop)
    ue.EditorAssetLibrary.save_loaded_asset(sound)
for obj in [master, music, effects, mix]:
    ue.EditorAssetLibrary.save_loaded_asset(obj)
ue.log('TRAIN_AUDIO_IMPORTED')
