"""Import Meshy rigs and transfer the existing in-game automaton animations."""
import sys
from pathlib import Path

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_characters import AS, AT, ROOT, bake_clip, retarget

BASE = "/Game/Characters/Automaton"
CLIPS = ("Idle", "Walk", "Attack", "Hit", "Death")


def import_enemy(name):
    path = f"/Game/Characters/Faubourgs/{name}"
    mesh_path = f"{path}/SK_{name}"
    mesh = ue.load_asset(mesh_path)
    if not mesh:
        task = ue.AssetImportTask()
        task.filename = str(ROOT / "SourceArt/Characters/Faubourgs" / f"{name}.glb")
        task.destination_path = path
        task.automated = True
        task.save = True
        AT.import_asset_tasks([task])
        mesh = next((asset for asset in task.get_objects() if isinstance(asset, ue.SkeletalMesh)), None)
        if not mesh:
            raise RuntimeError(f"No skeletal mesh imported for {name}")
        if mesh.get_path_name().split(".")[0] != mesh_path and not AS.rename_loaded_asset(mesh, mesh_path):
            raise RuntimeError(f"Could not rename mesh for {name}")

    target_clips = [f"{path}/Animations/{clip}" for clip in CLIPS]
    if all(AS.does_asset_exist(asset) for asset in target_clips):
        return
    source = ue.load_asset(f"{BASE}/SK_Automaton")
    retargeter = retarget(source, mesh, f"Automaton_{name}")
    for clip in CLIPS:
        source_path = f"{BASE}/Animations/{clip}.{clip}"
        asset = bake_clip(source, mesh, retargeter, source_path, f"{path}/Animations", clip)
        AS.save_loaded_asset(asset)
    print("FAUBOURGS_ENEMY", name, len(CLIPS))


if __name__ == "__main__":
    for name in ("Sentry", "Guardian"):
        import_enemy(name)
