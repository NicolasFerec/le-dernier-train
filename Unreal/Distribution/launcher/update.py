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
DEV_TAG = 'unreal-dev'
CHANNELS = ('stable', 'dev')
PLATFORMS = ('linux', 'windows')
ENTRIES = {
    'linux': Path('LeDernierTrain.sh'),
    'windows': Path('LeDernierTrain/Binaries/Win64/LeDernierTrain-Win64-Shipping.exe'),
}
MAX_ASSET = 2 * 1024**3 - 1
MAX_EXTRACTED = 12 * 1024**3
REPO = re.compile(r'[A-Za-z0-9-]+/[A-Za-z0-9_.-]+\Z')
HASH = re.compile(r'[a-f0-9]{64}\Z')
VERSION = {
    'stable': re.compile(r'v(0|[1-9]\d{0,9})\.(0|[1-9]\d{0,9})\.(0|[1-9]\d{0,9})\Z'),
    'dev': re.compile(r'(0|[1-9]\d{0,14})\.([1-9]\d{0,5})\Z'),
}


@dataclass(frozen=True)
class Asset:
    channel: str
    version: str
    digest: str
    size: int
    url: str


def asset_name(channel, version, platform):
    if channel not in CHANNELS or not VERSION[channel].fullmatch(version) or platform not in PLATFORMS:
        raise ValueError('Invalid release identity')
    return f'{PRODUCT}-{channel}-{version}-{platform}-x86_64.zip'


def version_key(channel, version):
    return tuple(map(int, VERSION[channel].fullmatch(version).groups()))


def select_asset(release, repo, channel, platform):
    """Choose the newest uploaded archive for this installation’s platform."""
    if not REPO.fullmatch(repo) or channel not in CHANNELS or platform not in PLATFORMS:
        raise ValueError('Invalid update feed')
    tag = release.get('tag_name', '')
    if release.get('draft') or bool(release.get('prerelease')) != (channel == 'dev'):
        raise ValueError('Release is not published on this channel')
    if (channel == 'dev' and tag != DEV_TAG) or (channel == 'stable' and not VERSION[channel].fullmatch(tag)):
        raise ValueError('Unexpected release tag')
    builds = {}
    prefix = f'{PRODUCT}-{channel}-'
    for entry in release.get('assets', []):
        name = entry.get('name', '')
        digest = entry.get('digest') or ''
        size = entry.get('size')
        if (not name.startswith(prefix) or entry.get('state') != 'uploaded'
                or not digest.startswith('sha256:') or not HASH.fullmatch(digest[7:])
                or type(size) is not int or not 0 < size <= MAX_ASSET):
            continue
        for system in PLATFORMS:
            suffix = f'-{system}-x86_64.zip'
            if not name.endswith(suffix):
                continue
            version = name[len(prefix):-len(suffix)]
            if VERSION[channel].fullmatch(version) and (channel == 'dev' or version == tag):
                url = f'https://github.com/{repo}/releases/download/{tag}/{name}'
                builds.setdefault(version, {})[system] = Asset(channel, version, digest[7:], size, url)
    available = [version for version, assets in builds.items() if platform in assets]
    if not available:
        raise ValueError('No build for this platform on this channel')
    version = max(available, key=lambda item: version_key(channel, item))
    return builds[version][platform]


class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlsplit(newurl).scheme != 'https':
            raise ValueError('The update redirected outside HTTPS')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


HTTP = urllib.request.build_opener(HTTPSRedirect())


def fetch_asset(repo, channel, platform):
    if not REPO.fullmatch(repo):
        raise ValueError('Invalid GitHub repository')
    route = f'tags/{DEV_TAG}' if channel == 'dev' else 'latest'
    request = urllib.request.Request(
        f'https://api.github.com/repos/{repo}/releases/{route}',
        headers={'Accept': 'application/vnd.github+json', 'Cache-Control': 'no-cache', 'User-Agent': PRODUCT},
    )
    with HTTP.open(request, timeout=5) as response:
        return select_asset(json.loads(response.read(4 * 1024**2)), repo, channel, platform)


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

    def location(self, channel, version, digest):
        return self.root / 'versions' / channel / f'{version}-{digest}'

    def current(self, channel):
        try:
            data = json.loads((self.root / f'current-{channel}.json').read_text(encoding='utf-8'))
            version, digest = data['version'], data['sha256']
            if VERSION[channel].fullmatch(version) and HASH.fullmatch(digest):
                directory = self.location(channel, version, digest)
                if (directory / ENTRIES[self.platform]).is_file():
                    return version, digest, directory
        except (OSError, ValueError, KeyError, TypeError):
            pass
        return None

    def install(self, asset, progress, local_archive=None):
        previous = self.current(asset.channel)
        destination = self.location(asset.channel, asset.version, asset.digest)
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
            atomic_write(self.root / f'current-{asset.channel}.json', json.dumps({
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
            if (channel in CHANNELS and VERSION[channel].fullmatch(version) and data['platform'] == self.platform
                    and type(size) is int and 0 < size <= MAX_ASSET and HASH.fullmatch(digest)):
                return Asset(channel, version, digest, size, '')
        except (OSError, ValueError, KeyError, TypeError):
            pass
        return None

    def bootstrap(self, asset, progress):
        if not asset:
            return
        archive = self.root / 'bootstrap.zip'
        installed = self.current(asset.channel)
        if not installed or (installed[:2] != (asset.version, asset.digest)
                             and version_key(asset.channel, installed[0]) <= version_key(asset.channel, asset.version)):
            progress('install', None)
            self.install(asset, progress, local_archive=archive)
        archive.unlink(missing_ok=True)
        (self.root / 'bootstrap.json').unlink(missing_ok=True)

    def update(self, repo, channel, progress):
        local = self.current(channel)
        progress('check', None)
        asset = fetch_asset(repo, channel, self.platform)
        if local and (version_key(channel, asset.version) < version_key(channel, local[0])
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
    environment = os.environ.copy()
    environment['TRAIN_LAUNCHER_SETTINGS'] = str(settings)
    # PyInstaller prepends its own libraries. The Unreal executable must load its own.
    if getattr(sys, 'frozen', False):
        if os.name == 'nt':
            import ctypes
            ctypes.windll.kernel32.SetDllDirectoryW(None)
        elif original := environment.get('LD_LIBRARY_PATH_ORIG'):
            environment['LD_LIBRARY_PATH'] = original
        else:
            environment.pop('LD_LIBRARY_PATH', None)
    player_directory = settings / 'Player'
    if os.name == 'nt':
        # Unreal parses the raw Windows command line and expects quotes around the value.
        command = f'{subprocess.list2cmdline([str(entry)])} -UserDir="{player_directory}"'
        return subprocess.Popen(command, executable=entry, cwd=directory, env=environment)
    return subprocess.Popen([str(entry), f'-UserDir={player_directory}'], cwd=directory, env=environment)
