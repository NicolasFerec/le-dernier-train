import hashlib
import io
import json
import stat
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from launcher import update


def bundle(contents=b'game'):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for executable in update.ENTRIES.values():
            info = zipfile.ZipInfo(executable.as_posix())
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o755) << 16
            archive.writestr(info, contents)
    return data.getvalue()


def entry(version, platform, data):
    return {'name': update.asset_name(version, platform), 'state': 'uploaded',
            'size': len(data), 'digest': 'sha256:' + hashlib.sha256(data).hexdigest()}


def release(version, data):
    return {'tag_name': version, 'prerelease': False, 'draft': False,
            'assets': [entry(version, platform, data) for platform in update.PLATFORMS]}


class UpdateTests(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.TemporaryDirectory()
        self.addCleanup(self.root.cleanup)
        self.directory = Path(self.root.name) / 'Le Dernier Train'
        self.installation = update.Installation(self.directory, 'linux')

    def serve(self, metadata, archive):
        def reply(request, **_options):
            if request.full_url.startswith('https://api.github.com/'):
                return io.BytesIO(json.dumps(metadata).encode())
            self.assertTrue(request.full_url.startswith('https://github.com/'))
            return io.BytesIO(archive)
        return patch.object(update.HTTP, 'open', side_effect=reply)

    def test_install_and_upgrade_stable(self):
        for version, code in [('v0.2.0', b'old'), ('v0.2.1', b'new')]:
            archive = bundle(code)
            progress = []
            with self.serve(release(version, archive), archive):
                directory = self.installation.update(update.DEFAULT_REPO,
                                                     lambda stage, value: progress.append(stage))
            self.assertEqual(progress, ['check', 'download', 'download', 'install'])
            self.assertEqual((directory / update.ENTRIES['linux']).read_bytes(), code)
            self.assertEqual(self.installation.current()[0], version)
        self.assertEqual(self.installation.current()[0], 'v0.2.1')
        self.assertFalse((self.directory / 'incoming').exists())

    def test_bad_update_keeps_previous_version(self):
        first = bundle(b'first')
        with self.serve(release('v0.2.0', first), first):
            previous = self.installation.update(update.DEFAULT_REPO, lambda *_: None)
        with self.serve(release('v0.2.1', bundle(b'second')), bundle(b'broken')):
            with self.assertRaises(ValueError):
                self.installation.update(update.DEFAULT_REPO, lambda *_: None)
        with patch.object(update.HTTP, 'open', side_effect=TimeoutError):
            with self.assertRaises(TimeoutError):
                self.installation.update(update.DEFAULT_REPO, lambda *_: None)
        self.assertEqual(self.installation.current()[2], previous)
        self.assertEqual((previous / update.ENTRIES['linux']).read_bytes(), b'first')
        self.assertFalse((self.directory / 'incoming').exists())

    def test_bundled_game_installs_without_network_and_does_not_downgrade(self):
        self.directory.mkdir()
        for version, code in [('v0.2.1', b'new'), ('v0.2.0', b'old')]:
            archive = bundle(code)
            (self.directory / 'bootstrap.zip').write_bytes(archive)
            (self.directory / 'bootstrap.json').write_text(json.dumps({
                'platform': 'linux', 'channel': 'stable', 'version': version,
                'size': len(archive), 'sha256': hashlib.sha256(archive).hexdigest(),
            }), encoding='utf-8')
            with patch.object(update.HTTP, 'open', side_effect=AssertionError('Network used during bootstrap')):
                self.installation.bootstrap(self.installation.bootstrap_asset(), lambda *_: None)
            current = self.installation.current()
            self.assertEqual(current[0], 'v0.2.1')
            self.assertEqual((current[2] / update.ENTRIES['linux']).read_bytes(), b'new')
            self.assertFalse((self.directory / 'bootstrap.zip').exists())

    def test_stale_stable_feed_does_not_downgrade(self):
        archive = bundle()
        with self.serve(release('v0.2.1', archive), archive):
            previous = self.installation.update(update.DEFAULT_REPO, lambda *_: None)
        with self.serve(release('v0.2.0', archive), archive) as http:
            self.assertEqual(self.installation.update(update.DEFAULT_REPO, lambda *_: None), previous)
            self.assertEqual(http.call_count, 1)

    def test_stable_release_requires_only_the_requested_platform(self):
        archive = bundle()
        metadata = release('v0.2.1', archive)
        metadata['assets'] = [entry('v0.2.1', 'linux', archive)]
        self.assertEqual(update.select_asset(metadata, update.DEFAULT_REPO, 'linux').version, 'v0.2.1')
        with self.assertRaises(ValueError):
            update.select_asset(metadata, update.DEFAULT_REPO, 'windows')

    def test_ignore_prerelease(self):
        metadata = release('v0.2.1', bundle())
        metadata['prerelease'] = True
        with self.assertRaises(ValueError):
            update.select_asset(metadata, update.DEFAULT_REPO, 'linux')

    def test_reject_other_engine_packages(self):
        metadata = release('v0.1.5', bundle())
        for item in metadata['assets']:
            item['name'] = item['name'].replace(update.PRODUCT, 'Le-Dernier-Train')
        with self.assertRaises(ValueError):
            update.select_asset(metadata, update.DEFAULT_REPO, 'windows')

    def test_archive_never_writes_through_traversal_or_symlink(self):
        for name, kind in [('../escape', stat.S_IFREG), ('escape', stat.S_IFLNK), ('C:/escape', stat.S_IFREG)]:
            with self.subTest(name=name, kind=kind):
                archive = Path(self.root.name) / 'bad.zip'
                with zipfile.ZipFile(archive, 'w') as file:
                    info = zipfile.ZipInfo(name)
                    info.create_system = 3
                    info.external_attr = (kind | 0o777) << 16
                    file.writestr(info, 'outside')
                with self.assertRaises(ValueError):
                    update.unpack(archive, self.directory)
                self.assertFalse((Path(self.root.name) / 'escape').exists())

    def test_second_launcher_cannot_update_a_running_install(self):
        with update.installation_lock(self.directory):
            with self.assertRaises(update.AlreadyRunning), update.installation_lock(self.directory):
                self.fail('Second lock granted')


if __name__ == '__main__':
    unittest.main()
