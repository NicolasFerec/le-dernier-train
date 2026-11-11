"""Publish the archives and installers for the platforms built by the workflow."""

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

from launcher.update import DEV_TAG, PLATFORMS, PRODUCT, VERSION, asset_name, version_key


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


def prune_dev():
    expression = re.compile(rf'{PRODUCT}-dev-([0-9.]+)-(linux|windows)-x86_64\.zip\Z')
    builds = {platform: [] for platform in PLATFORMS}
    for asset in release_assets(DEV_TAG):
        match = expression.fullmatch(asset['name'])
        if match and VERSION['dev'].fullmatch(match[1]) and asset['state'] == 'uploaded':
            builds[match[2]].append((match[1], asset['name']))
    # Keep the current and previous archive independently for each platform.
    for files in builds.values():
        for _, name in sorted(files, key=lambda item: version_key('dev', item[0]))[:-2]:
            gh('release', 'delete-asset', DEV_TAG, name, '--yes')


def publish_dev(version, files, existing, commit, current_main):
    if current_main != commit:
        print('Main advanced while this build ran. Keeping the current Dev release.')
        return
    dev = next((release for release in existing if release['tagName'] == DEV_TAG), None)
    notes = f'Build {version} · {commit}'
    if not dev:
        gh('release', 'create', DEV_TAG, '--target', commit, '--title', 'Le Dernier Train · Dev',
           '--notes', notes, '--draft', '--prerelease', '--latest=false')
    elif not dev['isPrerelease']:
        raise ValueError('The dev tag belongs to a stable release')
    gh('release', 'upload', DEV_TAG, *(str(file) for file in files), '--clobber')
    assert_assets(DEV_TAG, files)
    gh('release', 'edit', DEV_TAG, '--draft=false', '--prerelease', '--latest=false', '--notes', notes)
    gh('api', '--method', 'PATCH', f'repos/{os.environ["GITHUB_REPOSITORY"]}/git/refs/tags/{DEV_TAG}',
       '-f', f'sha={commit}', '-F', 'force=true', capture=True)
    prune_dev()


def publish_stable(tag, files, existing):
    release = next((item for item in existing if item['tagName'] == tag), None)
    if release and release['isPrerelease']:
        raise ValueError(f'{tag} is a prerelease')
    already_published = release and not release['isDraft']
    previous = next((item['tagName'] for item in existing if item['isLatest']), None)
    if not already_published and previous and VERSION['stable'].fullmatch(previous):
        if version_key('stable', tag) <= version_key('stable', previous):
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
            notes = gh('api', '--method', 'POST',
                       f'repos/{os.environ["GITHUB_REPOSITORY"]}/releases/generate-notes',
                       '-f', f'tag_name={tag}', '--jq', '.body', capture=True)
            gh('release', 'edit', tag, '--notes', notes)
    if not already_published:
        gh('release', 'edit', tag, '--draft=false', '--latest')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('channel', choices=('stable', 'dev'))
    parser.add_argument('version')
    parser.add_argument('artifacts', type=Path)
    parser.add_argument('--platform', action='append', choices=PLATFORMS, required=True)
    args = parser.parse_args()
    installers = {
        'linux': 'Le-Dernier-Train-Setup-Linux-x86_64.run',
        'windows': 'Le-Dernier-Train-Setup-Windows-x86_64.exe',
    }
    files = [args.artifacts / name for platform in args.platform
             for name in (asset_name(args.channel, args.version, platform), installers[platform])]
    missing = [file.name for file in files if not file.is_file()]
    if missing:
        raise ValueError(f'Missing release artifacts: {", ".join(missing)}')
    if args.channel == 'dev':
        commit = os.environ['GITHUB_SHA']
        remote = subprocess.check_output(['git', 'ls-remote', 'origin', 'refs/heads/main'], text=True)
        publish_dev(args.version, files, release_list(), commit, remote.split()[0])
    else:
        publish_stable(args.version, files, release_list())


if __name__ == '__main__':
    main()
