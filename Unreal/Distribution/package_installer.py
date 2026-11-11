"""Build the standalone launcher and its per-user installer for the current OS."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

from launcher.update import DEFAULT_REPO, MAX_ASSET, PRODUCT, REPO, VERSION, launcher_asset_name

HERE = Path(__file__).resolve().parent
ART = HERE / 'launcher'
WINDOWS = sys.platform == 'win32'
LAUNCHER_VERSION = (ART / 'version.txt').read_text(encoding='utf-8').strip()


def build_launcher(scratch, repo, version=LAUNCHER_VERSION):
    repository = scratch / 'repository.txt'
    repository.write_text(repo + '\n', encoding='utf-8')
    version_file = scratch / 'version.txt'
    version_file.write_text(version + '\n', encoding='utf-8')
    options = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onefile', '--windowed',
               '--name=LeDernierTrainLauncher', f'--distpath={scratch / "dist"}',
               f'--workpath={scratch / "build"}', f'--specpath={scratch}']
    for resource in (ART / 'depot.png', ART / 'icon.png', repository, version_file):
        options.append(f'--add-data={resource}:.')
    if WINDOWS:
        options.append(f'--icon={ART / "icon.ico"}')
    subprocess.run([*options, str(ART / 'splash.py')], check=True)
    return scratch / 'dist' / ('LeDernierTrainLauncher.exe' if WINDOWS else 'LeDernierTrainLauncher')


def bootstrap(game, scratch):
    platform = 'windows' if WINDOWS else 'linux'
    match = re.fullmatch(rf'{PRODUCT}-stable-(.+)-{platform}-x86_64\.zip', game.name)
    if not match or not VERSION.fullmatch(match[1]):
        raise ValueError(f'Expected a {platform} Shipping archive from package_game.py')
    with game.open('rb') as file:
        digest = hashlib.file_digest(file, 'sha256').hexdigest()
    metadata = scratch / 'bootstrap.json'
    metadata.write_text(json.dumps({'channel': 'stable', 'version': match[1], 'platform': platform,
                                    'size': game.stat().st_size, 'sha256': digest}), encoding='utf-8')
    return metadata


def linux_setup(launcher, game, metadata, output):
    with output.open('wb') as file:
        file.write((HERE / 'installers/linux-setup.sh').read_bytes())
        with tarfile.open(fileobj=file, mode='w|') as archive:
            archive.add(launcher, 'LeDernierTrainLauncher')
            archive.add(ART / 'icon.png', 'icon.png')
            archive.add(HERE / 'installers/linux-uninstall.sh', 'uninstall.sh')
            archive.add(game, 'bootstrap.zip')
            archive.add(metadata, 'bootstrap.json')
    output.chmod(0o755)


def windows_setup(launcher, game, metadata, output):
    compiler = shutil.which('makensis') or shutil.which('makensis.exe')
    if not compiler:
        raise ValueError('Install NSIS and add makensis to PATH')
    prefix = '/D' if WINDOWS else '-D'
    subprocess.run([compiler, f'{prefix}SETUP_FILE={output}', f'{prefix}TRAIN_LAUNCHER={launcher}',
                    f'{prefix}TRAIN_ICON={ART / "icon.ico"}', f'{prefix}TRAIN_BUNDLE={game}',
                    f'{prefix}TRAIN_BUNDLE_META={metadata}', str(HERE / 'installers/windows.nsi')], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', default=DEFAULT_REPO)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=HERE.parent / 'Saved/Distribution/Packages')
    args = parser.parse_args()
    if not REPO.fullmatch(args.repository):
        parser.error('Expected a GitHub repository in owner/repository format')
    args.output.mkdir(parents=True, exist_ok=True)
    platform = 'Windows-x86_64.exe' if WINDOWS else 'Linux-x86_64.run'
    output = args.output.resolve() / f'Le-Dernier-Train-Setup-{platform}'
    with tempfile.TemporaryDirectory(prefix='train-setup-') as directory:
        scratch = Path(directory)
        game = args.game.resolve()
        metadata = bootstrap(game, scratch)
        launcher = build_launcher(scratch, args.repository)
        temporary = output.with_name('building-' + output.name)
        try:
            if WINDOWS:
                windows_setup(launcher, game, metadata, temporary)
            else:
                linux_setup(launcher, game, metadata, temporary)
            if temporary.stat().st_size > MAX_ASSET:
                raise ValueError('The installer exceeds the GitHub Releases asset limit')
            temporary.replace(output)
            if not WINDOWS:
                launcher_output = output.with_name(launcher_asset_name(LAUNCHER_VERSION))
                shutil.copy2(launcher, temporary)
                temporary.replace(launcher_output)
                print(launcher_output, flush=True)
        finally:
            temporary.unlink(missing_ok=True)
    print(output, flush=True)


if __name__ == '__main__':
    main()
