"""Publish the archives and installers for a stable release."""

import argparse
import json
import subprocess
from pathlib import Path

from launcher.update import PLATFORMS, VERSION, asset_name, version_key


def gh(*arguments, capture=False):
    result = subprocess.run(['gh', *arguments], check=True, text=True, capture_output=capture)
    return result.stdout.strip() if capture else None


def release_list():
    return json.loads(gh('release', 'list', '--limit', '100', '--json',
                         'tagName,isDraft,isPrerelease,isLatest', capture=True))


def release_assets(tag):
    return json.loads(gh('release', 'view', tag, '--json', 'assets', '--jq', '.assets', capture=True))


def assert_assets(tag, required):
    uploaded = {asset['name']: asset['size'] for asset in release_assets(tag)}
    if any(uploaded.get(file.name) != file.stat().st_size for file in required):
        raise RuntimeError('GitHub is missing one or more uploaded assets')


def publish_stable(tag, files, existing):
    release = next((item for item in existing if item['tagName'] == tag), None)
    if release and release['isPrerelease']:
        raise ValueError(f'{tag} is a prerelease')
    already_published = release and not release['isDraft']
    previous = next((item['tagName'] for item in existing if item['isLatest']), None)
    if not already_published and previous and VERSION.fullmatch(previous):
        if version_key(tag) <= version_key(previous):
            raise ValueError(f'{tag} must be newer than the current stable release {previous}')
    if not release:
        args = ['--verify-tag', '--draft', '--generate-notes', '--title', f'Le Dernier Train {tag}']
        if previous:
            args.extend(('--notes-start-tag', previous))
        gh('release', 'create', tag, *args)
    gh('release', 'upload', tag, *(str(file) for file in files), '--clobber')
    assert_assets(tag, files)
    if release:
        notes = gh('release', 'view', tag, '--json', 'body', '--jq', '.body', capture=True)
        if not notes:
            fields = ['-f', f'tag_name={tag}']
            if previous and previous != tag:
                fields.extend(('-f', f'previous_tag_name={previous}'))
            notes = gh('api', '--method', 'POST',
                       'repos/{owner}/{repo}/releases/generate-notes',
                       *fields, '--jq', '.body', capture=True)
            gh('release', 'edit', tag, '--notes', notes)
    if not already_published:
        gh('release', 'edit', tag, '--draft=false', '--latest')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('artifacts', type=Path)
    parser.add_argument('--platform', action='append', choices=PLATFORMS, required=True)
    args = parser.parse_args()
    installers = {
        'linux': 'Le-Dernier-Train-Setup-Linux-x86_64.run',
        'windows': 'Le-Dernier-Train-Setup-Windows-x86_64.exe',
    }
    files = [args.artifacts / name for platform in args.platform
             for name in (asset_name(args.version, platform), installers[platform])]
    missing = [file.name for file in files if not file.is_file()]
    if missing:
        raise ValueError(f'Missing release artifacts: {", ".join(missing)}')
    publish_stable(args.version, files, release_list())


if __name__ == '__main__':
    main()
