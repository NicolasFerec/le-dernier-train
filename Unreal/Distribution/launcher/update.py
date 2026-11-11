"""Install GitHub release assets into versioned, per-user directories."""

import contextlib
import hashlib
import json
import ntpath
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath

PRODUCT = 'Le-Dernier-Train-Unreal'
DEFAULT_REPO = 'NicolasFerec/le-dernier-train'
PLATFORMS = ('linux', 'windows')
ENTRIES = {
    'linux': Path('LeDernierTrain.sh'),
    'windows': Path('LeDernierTrain/Binaries/Win64/LeDernierTrain-Win64-Shipping.exe'),
}
MAX_ASSET = 2 * 1024**3 - 1
MAX_EXTRACTED = 12 * 1024**3
REPO = re.compile(r'[A-Za-z0-9-]+/[A-Za-z0-9_.-]+\Z')
HASH = re.compile(r'[a-f0-9]{64}\Z')
VERSION = re.compile(r'v(0|[1-9]\d{0,9})\.(0|[1-9]\d{0,9})\.(0|[1-9]\d{0,9})\Z')
LAUNCHER_PREFIX = 'Le-Dernier-Train-Launcher-'
LAUNCHER_SUFFIX = '-linux-x86_64'


@dataclass(frozen=True)
class Asset:
    version: str
    digest: str
    size: int
    url: str


def asset_name(version, platform):
    if not VERSION.fullmatch(version) or platform not in PLATFORMS:
        raise ValueError('Invalid release identity')
    return f'{PRODUCT}-stable-{version}-{platform}-x86_64.zip'


def version_key(version):
    return tuple(map(int, VERSION.fullmatch(version).groups()))


def launcher_asset_name(version):
    if not VERSION.fullmatch(version):
        raise ValueError('Invalid launcher version')
    return f'{LAUNCHER_PREFIX}{version}{LAUNCHER_SUFFIX}'


def stable_tag(release, repo):
    if not REPO.fullmatch(repo):
        raise ValueError('Invalid update feed')
    if release.get('draft') or release.get('prerelease'):
        raise ValueError('Release is not published as stable')
    tag = release.get('tag_name', '')
    if not VERSION.fullmatch(tag):
        raise ValueError('Invalid stable release tag')
    return tag


def release_asset(entry, repo, tag, version, name):
    digest = entry.get('digest') or ''
    size = entry.get('size')
    if (entry.get('state') == 'uploaded' and digest.startswith('sha256:') and HASH.fullmatch(digest[7:])
            and type(size) is int and 0 < size <= MAX_ASSET):
        url = f'https://github.com/{repo}/releases/download/{tag}/{name}'
        return Asset(version, digest[7:], size, url)
    return None


def select_asset(release, repo, platform):
    tag = stable_tag(release, repo)
    name = asset_name(tag, platform)
    for entry in release.get('assets', []):
        if entry.get('name') == name and (asset := release_asset(entry, repo, tag, tag, name)):
            return asset
    raise ValueError('No stable build for this platform')


def select_launcher_asset(release, repo, installed_version):
    tag = stable_tag(release, repo)
    candidates = []
    for entry in release.get('assets', []):
        name = entry.get('name', '')
        if not name.startswith(LAUNCHER_PREFIX) or not name.endswith(LAUNCHER_SUFFIX):
            continue
        version = name[len(LAUNCHER_PREFIX):-len(LAUNCHER_SUFFIX)]
        if VERSION.fullmatch(version) and version_key(version) > version_key(installed_version):
            if asset := release_asset(entry, repo, tag, version, name):
                candidates.append(asset)
    return max(candidates, key=lambda asset: version_key(asset.version), default=None)


class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlsplit(newurl).scheme != 'https':
            raise ValueError('The update redirected outside HTTPS')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


HTTP = urllib.request.build_opener(HTTPSRedirect())


def fetch_release(repo):
    if not REPO.fullmatch(repo):
        raise ValueError('Invalid GitHub repository')
    request = urllib.request.Request(
        f'https://api.github.com/repos/{repo}/releases/latest',
        headers={'Accept': 'application/vnd.github+json', 'Cache-Control': 'no-cache', 'User-Agent': PRODUCT},
    )
    with HTTP.open(request, timeout=5) as response:
        return json.loads(response.read(4 * 1024**2))


def download(asset, output, progress):
    digest = hashlib.sha256()
    downloaded = 0
    deadline = time.monotonic() + 30 * 60
    request = urllib.request.Request(asset.url, headers={'User-Agent': PRODUCT})
    with HTTP.open(request, timeout=12) as response, output.open('wb') as file:
        while chunk := response.read(256 * 1024):
            downloaded += len(chunk)
            if downloaded > asset.size or time.monotonic() > deadline:
                raise ValueError('Unexpected update download size or duration')
            digest.update(chunk)
            file.write(chunk)
            progress('download', downloaded / asset.size)
    if downloaded != asset.size or digest.hexdigest() != asset.digest:
        raise ValueError('Update download failed its SHA-256 check')


def external_environment():
    environment = os.environ.copy()
    # PyInstaller prepends its own libraries. Child executables must load their own.
    if getattr(sys, 'frozen', False) and sys.platform != 'win32':
        if original := environment.get('LD_LIBRARY_PATH_ORIG'):
            environment['LD_LIBRARY_PATH'] = original
        else:
            environment.pop('LD_LIBRARY_PATH', None)
    return environment


def launcher_environment():
    environment = external_environment()
    environment['PYINSTALLER_RESET_ENVIRONMENT'] = '1'
    return environment


def stage_launcher(asset, executable, repo, progress):
    candidate = executable.with_name('.launcher-next')
    candidate.unlink(missing_ok=True)
    try:
        progress('download_launcher', 0)
        download(asset, candidate, lambda _, value: progress('download_launcher', value))
        candidate.chmod(0o755)
        progress('install_launcher', None)
        subprocess.run([candidate, '--check-launcher', asset.version, repo],
                       env=launcher_environment(), check=True, timeout=20)
    except Exception:
        candidate.unlink(missing_ok=True)
        raise
    return candidate


def restart_launcher(executable, candidate):
    backup = executable.with_name('.launcher-previous')
    backup.unlink(missing_ok=True)
    os.link(executable, backup)
    try:
        candidate.replace(executable)
    except OSError:
        backup.unlink(missing_ok=True)
        raise
    try:
        # Linux keeps the running inode valid. The lock closes on exec.
        os.execve(executable, [str(executable)], launcher_environment())
    except OSError:
        backup.replace(executable)
        raise


def unpack(archive, destination):
    with zipfile.ZipFile(archive) as zipped:
        members = zipped.infolist()
        if len(members) > 30000 or sum(entry.file_size for entry in members) > MAX_EXTRACTED:
            raise ValueError('Unexpected update archive size')
        for entry in members:
            name = entry.filename.rstrip('/')
            parts = name.split('/')
            mode = entry.external_attr >> 16
            if (any(part in ('', '.', '..') or part.endswith((' ', '.')) or reserved_path(part)
                    for part in parts) or re.search(r'[\\:*?"<>|\x00-\x1f]', name)
                    or stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise ValueError(f'Unsafe path in update archive: {name!r}')
            target = destination.joinpath(*parts)
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with zipped.open(entry) as source, target.open('xb') as output:
                    shutil.copyfileobj(source, output)
                if mode & 0o111 and os.name != 'nt':
                    target.chmod(0o755)


def reserved_path(component):
    if hasattr(ntpath, 'isreserved'):
        return ntpath.isreserved(component)
    return PureWindowsPath(component).is_reserved()


def atomic_write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(content.encode('utf-8'))
            output.flush()
            os.fsync(output.fileno())
        temporary.replace(path)
    finally:
        if temporary:
            temporary.unlink(missing_ok=True)


class Installation:
    def __init__(self, root, platform):
        self.root = root
        self.platform = platform

    def location(self, version, digest):
        return self.root / 'versions' / 'stable' / f'{version}-{digest}'

    def current(self):
        try:
            data = json.loads((self.root / 'current-stable.json').read_text(encoding='utf-8'))
            version, digest = data['version'], data['sha256']
            if VERSION.fullmatch(version) and HASH.fullmatch(digest):
                directory = self.location(version, digest)
                if (directory / ENTRIES[self.platform]).is_file():
                    return version, digest, directory
        except (OSError, ValueError, KeyError, TypeError):
            pass
        return None

    def install(self, asset, progress, local_archive=None):
        previous = self.current()
        destination = self.location(asset.version, asset.digest)
        # Only the process holding the installation lock can stage or activate a build.
        staging = self.root / 'incoming'
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        try:
            if local_archive:
                archive = local_archive
                with archive.open('rb') as file:
                    if archive.stat().st_size != asset.size or hashlib.file_digest(file, 'sha256').hexdigest() != asset.digest:
                        raise ValueError('The bundled game failed its SHA-256 check')
            else:
                archive = staging / 'update.zip'
                progress('download', 0)
                download(asset, archive, progress)
            progress('install', None)
            extracted = staging / 'game'
            unpack(archive, extracted)
            entry = extracted / ENTRIES[self.platform]
            if not entry.is_file() or (self.platform == 'linux' and not os.access(entry, os.X_OK)):
                raise ValueError('The package is missing its game executable')
            destination.parent.mkdir(parents=True, exist_ok=True)
            if not destination.exists():
                extracted.replace(destination)
            else:
                # A crash may have completed the move before publishing the pointer.
                if not (destination / ENTRIES[self.platform]).is_file():
                    shutil.rmtree(destination)
                    extracted.replace(destination)
            atomic_write(self.root / 'current-stable.json', json.dumps({
                'version': asset.version, 'sha256': asset.digest,
            }))
        finally:
            shutil.rmtree(staging, ignore_errors=True)
        retained = {destination, previous[2] if previous else None}
        for other in destination.parent.iterdir():
            if other.is_dir() and other not in retained:
                shutil.rmtree(other, ignore_errors=True)
        return destination

    def bootstrap_asset(self):
        try:
            data = json.loads((self.root / 'bootstrap.json').read_text(encoding='utf-8'))
            channel, version = data['channel'], data['version']
            size, digest = data['size'], data['sha256']
            if (channel == 'stable' and VERSION.fullmatch(version) and data['platform'] == self.platform
                    and type(size) is int and 0 < size <= MAX_ASSET and HASH.fullmatch(digest)):
                return Asset(version, digest, size, '')
        except (OSError, ValueError, KeyError, TypeError):
            pass
        return None

    def bootstrap(self, asset, progress):
        if not asset:
            return
        archive = self.root / 'bootstrap.zip'
        installed = self.current()
        if not installed or (installed[:2] != (asset.version, asset.digest)
                             and version_key(installed[0]) <= version_key(asset.version)):
            progress('install', None)
            self.install(asset, progress, local_archive=archive)
        archive.unlink(missing_ok=True)
        (self.root / 'bootstrap.json').unlink(missing_ok=True)

    def update(self, repo, progress, *, release=None):
        local = self.current()
        if release is None:
            progress('check', None)
            release = fetch_release(repo)
        asset = select_asset(release, repo, self.platform)
        if local and (version_key(asset.version) < version_key(local[0])
                      or (asset.version, asset.digest) == local[:2]):
            return local[2]
        return self.install(asset, progress)


class AlreadyRunning(OSError):
    pass


@contextlib.contextmanager
def installation_lock(root):
    root.mkdir(parents=True, exist_ok=True)
    with (root / '.lock').open('a+b') as file:
        try:
            if os.name == 'nt':
                import msvcrt
                file.seek(0)
                msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise AlreadyRunning('The game or launcher is already running') from error
        yield


def state_directory():
    if os.name == 'nt':
        return Path(os.environ['LOCALAPPDATA']) / 'LeDernierTrainUnreal'
    state = Path(os.environ.get('XDG_STATE_HOME') or '.').expanduser()
    if not state.is_absolute():
        state = Path.home() / '.local/state'
    return state / 'le-dernier-train-unreal'


def launch_game(installation, directory, settings):
    settings.mkdir(parents=True, exist_ok=True)
    entry = directory / ENTRIES[installation.platform]
    environment = external_environment()
    environment['TRAIN_LAUNCHER_SETTINGS'] = str(settings)
    if getattr(sys, 'frozen', False) and os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetDllDirectoryW(None)
    player_directory = settings / 'Player'
    if os.name == 'nt':
        # Unreal parses the raw Windows command line and expects quotes around the value.
        command = f'{subprocess.list2cmdline([str(entry)])} -UserDir="{player_directory}"'
        return subprocess.Popen(command, executable=entry, cwd=directory, env=environment)
    return subprocess.Popen([str(entry), f'-UserDir={player_directory}'], cwd=directory, env=environment)
