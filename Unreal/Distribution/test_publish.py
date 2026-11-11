import unittest
from pathlib import Path
from unittest.mock import call, patch

import publish
from launcher import update


class PublishTests(unittest.TestCase):
    def test_prune_keeps_two_archives_per_platform(self):
        versions = {'linux': ['9.1', '10.1', '11.1'], 'windows': ['8.1', '9.1']}
        assets = [
            {'name': update.asset_name('dev', version, platform), 'state': 'uploaded'}
            for platform, builds in versions.items()
            for version in builds
        ]
        with patch.object(publish, 'release_assets', return_value=assets), patch.object(publish, 'gh') as gh:
            publish.prune_dev()

        gh.assert_called_once_with(
            'release', 'delete-asset', update.DEV_TAG, update.asset_name('dev', '9.1', 'linux'), '--yes'
        )

    def test_published_stable_accepts_linux_artifacts_and_keeps_notes(self):
        tag = 'v0.2.0'
        files = [Path(update.asset_name('stable', tag, 'linux'))]
        releases = [{'tagName': tag, 'isLatest': True, 'isPrerelease': False, 'isDraft': False}]
        with patch.object(publish, 'assert_assets') as assert_assets, patch.object(
            publish, 'gh', return_value='Existing changelog'
        ) as gh:
            publish.publish_stable(tag, files, releases)

        self.assertEqual(gh.call_args_list, [
            call('release', 'upload', tag, str(files[0]), '--clobber'),
            call('release', 'view', tag, '--json', 'body', '--jq', '.body', capture=True),
        ])
        assert_assets.assert_called_once_with(tag, files)
