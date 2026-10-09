# SPDX-License-Identifier: Apache-2.0
"""Check installer staging and credential preservation without a board."""
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.fixture
def installer(tmp_path, monkeypatch):
    path = Path(__file__).with_name('deploy_board_native.py')
    spec = importlib.util.spec_from_file_location('native_installer', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    root = tmp_path / 'native-media'
    (root / 'build').mkdir(parents=True)
    for name in ('muse-gateway', 'muse-media', 'quick-present', 'youtube-resolve'):
        (root / 'build' / name).write_bytes(b'test binary')
    for name in ('speak', 'muse-media.service', 'muse-gateway.service'):
        (root / name).write_bytes(b'test input')
    monkeypatch.setattr(module, 'ROOT', root)
    monkeypatch.setattr(sys, 'argv', [str(path), '--ssh-target', 'root@board'])
    return module


def test_installer_stages_discovery_and_preserves_pairing(installer, monkeypatch):
    calls = []

    def remote(argv, **kwargs):
        calls.append((argv[-1], kwargs.get('input')))
        return subprocess.CompletedProcess(argv, 0, b'', b'')

    monkeypatch.setattr(installer.subprocess, 'run', remote)
    installer.main()
    commands = [command for command, _ in calls]
    stop = commands.index('systemctl stop muse-gateway.service muse-media.service')
    stages = [i for i, command in enumerate(commands) if command.startswith('cat > ')]
    assert stages and max(stages) < stop
    assert any('quick-present.new' in command for command in commands)
    assert not any(command.startswith('cat > ') and 'pairing.json' in command for command in commands)
    assert not any('mkfs' in command or 'mkswap' in command for command in commands)
    schema = next(data for command, data in calls if command == 'cat > /mnt/sd/muse/state/commands.json.new')
    assert {'genui.fetch', 'genui.present', 'genui.status', 'genui.capabilities',
            'media.switch', 'audio.radio', 'speech.read_text'} <= json.loads(schema).keys()


def test_stage_failure_leaves_running_services_alone(installer, monkeypatch):
    commands = []

    def remote(argv, **kwargs):
        command = argv[-1]
        commands.append(command)
        failure = command == 'cat > /mnt/sd/muse/media/quick-present.new'
        return subprocess.CompletedProcess(argv, int(failure), b'', b'upload failed' if failure else b'')

    monkeypatch.setattr(installer.subprocess, 'run', remote)
    with pytest.raises(RuntimeError, match='upload failed'):
        installer.main()
    assert not any('systemctl stop' in command for command in commands)


def test_dry_run_does_not_connect(installer, monkeypatch, capsys):
    monkeypatch.setattr(sys, 'argv', sys.argv + ['--dry-run'])

    def forbidden(*args, **kwargs):
        raise AssertionError('dry-run connected to a board')

    monkeypatch.setattr(installer.subprocess, 'run', forbidden)
    installer.main()
    assert 'quick-present' in capsys.readouterr().out
