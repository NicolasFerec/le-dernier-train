"""Produce one Unreal Shipping archive on a native Linux or Windows build host."""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

from launcher.update import ENTRIES, MAX_ASSET, asset_name

PROJECT = Path(__file__).resolve().parents[1]
PROJECT_FILE = 'LeDernierTrain.uproject'
PLATFORM = 'windows' if sys.platform == 'win32' else 'linux'
NATIVE_ENTRY = (Path('LeDernierTrain/Binaries/Linux/LeDernierTrain-Linux-Shipping')
                if PLATFORM == 'linux' else ENTRIES[PLATFORM])


def stage_project(destination, project_version):
    for name in (PROJECT_FILE, 'steam_appid.txt'):
        shutil.copy2(PROJECT / name, destination)
    for name in ('Config', 'Content', 'Source'):
        shutil.copytree(PROJECT / name, destination / name)
    config = destination / 'Config/DefaultGame.ini'
    text, matches = re.subn(r'(?m)^ProjectVersion=[^\r\n]+', f'ProjectVersion={project_version}',
                            config.read_text(encoding='utf-8'))
    if matches != 1:
        raise ValueError('Expected one ProjectVersion in Config/DefaultGame.ini')
    config.write_text(text, encoding='utf-8')


def windows_runtime(engine, work):
    configured = os.environ.get('TRAIN_WIN_APPLOCAL_DIR')
    dependencies = Path(configured or engine / 'Engine/Binaries/ThirdParty/AppLocalDependencies').resolve()
    if list((dependencies / 'Win64').rglob('vcruntime140.dll')):
        return dependencies
    if configured:
        raise ValueError('TRAIN_WIN_APPLOCAL_DIR must contain Win64/x64 and the VC++ runtime DLLs')
    program_files = Path(os.environ['ProgramFiles(x86)'])
    vswhere = program_files / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if vswhere.is_file():
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                           'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                           '-property', 'installationPath'], text=True).strip()
        if install:
            redist = Path(install) / 'VC/Redist/MSVC'
            candidates = [path for path in redist.glob('*/x64/Microsoft.VC*.CRT')
                          if (path / 'vcruntime140.dll').is_file()]
            if candidates:
                runtime = max(candidates, key=lambda path: tuple(map(int, path.parents[1].name.split('.'))))
                dependencies = work / 'AppLocalDependencies'
                shutil.copytree(runtime, dependencies / 'Win64/x64' / runtime.name)
                return dependencies
    raise ValueError('Install the x64 Visual C++ redistributable files in Visual Studio, or set TRAIN_WIN_APPLOCAL_DIR')


def build(engine, project, archive):
    metadata = json.loads((engine / 'Engine/Build/Build.version').read_text(encoding='utf-8-sig'))
    if tuple(metadata[field] for field in ('MajorVersion', 'MinorVersion', 'PatchVersion')) != (5, 8, 4):
        raise ValueError('This project expects Unreal Engine 5.8.4')
    engine_platform = 'Win64' if PLATFORM == 'windows' else 'Linux'
    tools = engine / 'Engine/Build/BatchFiles'
    runner = tools / ('RunUAT.bat' if PLATFORM == 'windows' else 'RunUAT.sh')
    args = [str(runner), 'BuildCookRun', f'-project={project / PROJECT_FILE}',
            f'-targetplatform={engine_platform}', '-clientconfig=Shipping',
            '-build', '-cook', '-stage', '-pak', '-iostore', '-compressed', '-package',
            '-archive', f'-archivedirectory={archive}', '-nodebuginfo', '-unattended',
            '-nop4', '-utf8output', '-nullrhi',
            '-AdditionalCookerOptions=-skipzenstore -ModelContextProtocolPort=8015']
    if PLATFORM == 'windows':
        dependencies = windows_runtime(engine, project.parent)
        args.append(f'-applocaldirectory={dependencies}')
    subprocess.run(args, check=True, cwd=project)


def create_archive(staged, output):
    entry = staged / ENTRIES[PLATFORM]
    native_bin = staged / NATIVE_ENTRY
    paks = staged / 'LeDernierTrain/Content/Paks'
    if not entry.is_file() or not native_bin.is_file() or not list(paks.glob('*.utoc')):
        raise ValueError(f'Incomplete Unreal Shipping package: {staged}')
    if PLATFORM == 'windows' and not (native_bin.parent / 'vcruntime140.dll').is_file():
        raise ValueError('The Windows package is missing the app-local VC++ runtime')
    for directory in (staged, native_bin.parent):
        shutil.copy2(PROJECT / 'steam_appid.txt', directory)
    if PLATFORM == 'linux':
        for path in (entry, native_bin):
            path.chmod(path.stat().st_mode | 0o755)
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_suffix('.part')
    try:
        with zipfile.ZipFile(partial, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=3) as archive:
            for path in sorted(staged.rglob('*')):
                if path.is_file() and not path.is_symlink():
                    archive.write(path, path.relative_to(staged).as_posix())
        if partial.stat().st_size > MAX_ASSET:
            raise ValueError('The game archive exceeds the GitHub Releases asset limit')
        partial.replace(output)
    finally:
        partial.unlink(missing_ok=True)


def verify_startup(staged, settings):
    """Start the native Shipping executable and wait for its menu settings to load."""
    settings.mkdir()
    environment = dict(os.environ, TRAIN_LAUNCHER_SETTINGS=str(settings))
    native = staged / NATIVE_ENTRY
    flags = ['-nullrhi', '-nosteam', '-nosound', '-unattended', '-NoSplash']
    user_directory = settings / 'Player'
    if PLATFORM == 'windows':
        command = f'{subprocess.list2cmdline([str(native), *flags])} -UserDir="{user_directory}"'
    else:
        command = [str(native), 'LeDernierTrain', *flags, f'-UserDir={user_directory}']
    process = subprocess.Popen(command, executable=native, cwd=staged, env=environment)
    try:
        deadline = time.monotonic() + 35
        while time.monotonic() < deadline and process.poll() is None:
            if (settings / 'language.txt').is_file():
                print(f'{PLATFORM} Shipping menu loaded', flush=True)
                return
            time.sleep(.25)
        raise RuntimeError(f'Shipping menu failed to start (process status {process.poll()})')
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=os.environ.get('UNREAL_ROOT'))
    parser.add_argument('--channel', choices=('stable', 'dev'), required=True)
    parser.add_argument('--version', required=True, help='Stable: vX.Y.Z. Dev: CI run ID and attempt, e.g. 12345.1.')
    parser.add_argument('--output', type=Path, default=PROJECT / 'Saved/Distribution/Packages')
    args = parser.parse_args()
    if not args.engine:
        parser.error('Set UNREAL_ROOT or pass --engine')
    filename = asset_name(args.channel, args.version, PLATFORM)
    base = re.search(r'(?m)^ProjectVersion=([\d.]+)', (PROJECT / 'Config/DefaultGame.ini').read_text(encoding='utf-8'))
    if not base:
        parser.error('ProjectVersion must start with a numeric version')
    project_version = args.version[1:] if args.channel == 'stable' else f'{base[1]}-dev.{args.version}'
    work = PROJECT / 'Saved/Distribution'
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work, prefix='cook-') as folder:
        scratch = Path(folder)
        project = scratch / 'SourceProject'
        project.mkdir()
        stage_project(project, project_version)
        archive = scratch / 'Archive'
        build(args.engine.resolve(), project, archive)
        staged = next((path for path in archive.iterdir() if (path / ENTRIES[PLATFORM]).is_file()), None)
        if staged is None:
            raise ValueError('Unreal did not produce a staged game directory')
        verify_startup(staged, scratch / 'SmokeSettings')
        output = args.output.resolve() / filename
        create_archive(staged, output)
        print(output, flush=True)


if __name__ == '__main__':
    main()
