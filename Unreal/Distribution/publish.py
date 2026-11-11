"""Publish both platform archives and setup files after their builds complete."""

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
    names = {}
    for asset in release_assets(DEV_TAG):
        name = asset['name']
        match = expression.fullmatch(name)
        if match and VERSION['dev'].fullmatch(match[1]):
            names.setdefault(match[1], []).append(name)
    # Keep two complete builds so a launcher that already selected the previous one can finish.
    complete = [version for version, files in names.items() if len(files) == len(PLATFORMS)]
    retained = set(sorted(complete, key=lambda v: version_key('dev', v))[-2:])
    for version, files in names.items():
        if version not in retained:
            for name in files:
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
    previous = next((release['tagName'] for release in existing if release['isLatest']), None)
    if previous and VERSION['stable'].fullmatch(previous) and version_key('stable', tag) <= version_key('stable', previous):
        raise ValueError(f'{tag} must be newer than the current stable release {previous}')
    draft = next((release for release in existing if release['tagName'] == tag), None)
    if draft and (not draft['isDraft'] or draft['isPrerelease']):
        raise ValueError(f'{tag} is already published or is a prerelease')
    if not draft:
        args = ['--verify-tag', '--draft', '--generate-notes', '--title', f'Le Dernier Train {tag}']
        if previous:
            args.extend(('--notes-start-tag', previous))
        gh('release', 'create', tag, *args)
    gh('release', 'upload', tag, *(str(file) for file in files), '--clobber')
    assert_assets(tag, files)
    gh('release', 'edit', tag, '--draft=false', '--latest')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('channel', choices=('stable', 'dev'))
    parser.add_argument('version')
    parser.add_argument('artifacts', type=Path)
    args = parser.parse_args()
    names = [asset_name(args.channel, args.version, system) for system in PLATFORMS]
    names += ['Le-Dernier-Train-Setup-Linux-x86_64.run', 'Le-Dernier-Train-Setup-Windows-x86_64.exe']
    files = [args.artifacts / name for name in names]
    if any(not file.is_file() for file in files):
        raise ValueError('Both game archives and both installers must be present before publishing')
    if args.channel == 'dev':
        commit = os.environ['GITHUB_SHA']
        remote = subprocess.check_output(['git', 'ls-remote', 'origin', 'refs/heads/main'], text=True)
        publish_dev(args.version, files, release_list(), commit, remote.split()[0])
    else:
        publish_stable(args.version, files, release_list())


if __name__ == '__main__':
    main()
