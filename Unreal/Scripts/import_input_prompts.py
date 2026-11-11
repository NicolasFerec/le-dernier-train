"""Import the selected CC0 Kenney input icons as UI textures."""
from pathlib import Path
import unreal as ue
root=Path(ue.Paths.project_dir())
ue.EditorAssetLibrary.make_directory('/Game/UI/InputPrompts')
for source in sorted((root/'SourceArt/InputPrompts').glob('*.png')):
    task=ue.AssetImportTask()
    task.filename=str(source)
    task.destination_path='/Game/UI/InputPrompts'
    task.automated=True
    task.replace_existing=True
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=ue.load_asset('/Game/UI/InputPrompts/'+source.stem)
    texture.set_editor_property('lod_group',ue.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('compression_settings',ue.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('never_stream',True)
    ue.EditorAssetLibrary.save_loaded_asset(texture)
ue.log('TRAIN_INPUT_GLYPHS_IMPORTED')
