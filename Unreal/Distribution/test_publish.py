import unittest
from pathlib import Path
from unittest.mock import call, patch

import publish
from launcher import update


class PublishTests(unittest.TestCase):
    def test_published_stable_accepts_linux_artifacts_and_keeps_notes(self):
        tag = 'v0.2.0'
        files = [Path(update.asset_name(tag, 'linux'))]
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
