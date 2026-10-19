"""Bounded integration audit: click the launcher, then check the real engine log.

Uses isolated configs/saves, and stops only the game processes created here.
The direct .exe check reads the user's settings without changing them.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from PySide6.QtCore import Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
import p3d_launcher as launcher
from p3d_settings import DEFAULT_CONFIG, read_config


def snapshot(path):
    return {str(p.relative_to(path)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in path.rglob('*') if p.is_file()} if path.exists() else {}


def main():
    output = ROOT / 'out' / 'settings-audit' / time.strftime('runtime-%Y%m%d-%H%M%S')
    output.mkdir(parents=True)
    original_ini = launcher.CONFIG_FILE.read_bytes()
    original_saves = snapshot(ROOT / 'user' / 'savedata')
    app = QApplication.instance() or QApplication([])
    app.setStyle('Fusion')
    app.setQuitOnLastWindowClosed(False)
    real_popen = subprocess.Popen
    profiles = [
        dict(name='window-720p-30', display_mode='windowed', output_res='1280x720', fps_limit='30',
             vsync='0', rcas='0', show_fps='1', anisotropy='16', async_shaders='0', frames_queued='2'),
        dict(name='legacy-fps144-safe60', display_mode='windowed', output_res='1920x1080', fps_limit='144',
             vsync='0', rcas='1', show_fps='0', anisotropy='2'),
        dict(name='fullscreen-720p-60', display_mode='fullscreen', output_res='1280x720', fps_limit='60',
             vsync='1', rcas='0', show_fps='1', anisotropy='4'),
        dict(name='borderless-60', display_mode='borderless', output_res='1280x720', fps_limit='60',
             vsync='0', rcas='1', show_fps='0', anisotropy='8'),
        dict(name='window-1440p-60', display_mode='windowed', output_res='2560x1440', fps_limit='60', vsync='0', rcas='0'),
        dict(name='window-4k-60', display_mode='windowed', output_res='3840x2160', fps_limit='60', vsync='0', rcas='0'),
        dict(name='window-ultrawide-1080', display_mode='windowed', output_res='2560x1080', fps_limit='60', vsync='0', rcas='0'),
        dict(name='window-ultrawide-1440', display_mode='windowed', output_res='3440x1440', fps_limit='60', vsync='0', rcas='0'),
    ]
    if len(sys.argv) > 1:
        wanted = {arg for arg in sys.argv[1:] if not arg.startswith('--')}
        if wanted:
            profiles = [profile for profile in profiles if profile['name'] in wanted]
    results = []
    for profile in profiles:
        directory = output / profile['name']
        directory.mkdir()
        user = directory / 'user'
        if (ROOT / 'user' / 'savedata').exists():
            shutil.copytree(ROOT / 'user' / 'savedata', user / 'savedata')
        pad = directory / 'pad.txt'
        pad.write_text('')
        owned = []

        def launch(*args, **kwargs):
            kwargs['env'].update(GOW3_FRAME_STATS='1', GOW3_PAD_FILE=str(pad),
                                 GOW3_GPU_USER_DIR=str(output / 'cache'))
            child = real_popen(*args, **kwargs)
            owned.append(child)
            return child

        with patch.multiple(launcher, CONFIG_FILE=directory / 'p3d.ini',
                            LOG_FILE=directory / 'runtime.log', USER_DIR=user), \
                patch.object(launcher.subprocess, 'Popen', side_effect=launch), \
                patch.object(launcher.QMessageBox, 'critical'):
            window = launcher.P3DModernLauncher()
            window.cfg = {**DEFAULT_CONFIG, **{k: v for k, v in profile.items() if k != 'name'}}
            window._load_values_into_ui()
            window.show()
            QTest.qWait(100)
            QTest.mouseClick(window.btn_play, Qt.LeftButton)
            start = time.monotonic()
            last_pulse = -1
            try:
                while time.monotonic() - start < (9 if '--brief' in sys.argv else 17):
                    elapsed = time.monotonic() - start
                    if elapsed >= 6:
                        pulse = int((elapsed - 6) / 1.4)
                        if pulse != last_pulse:
                            pad.write_text('l1 r1' if pulse == 0 else 'cross')
                            last_pulse = pulse
                        elif (elapsed - 6) % 1.4 > 0.2:
                            pad.write_text('')
                    QTest.qWait(50)
                    if owned and owned[0].poll() is not None:
                        raise AssertionError(f"Game exited during {profile['name']}: {owned[0].returncode}")
            finally:
                for child in owned:
                    if child.poll() is None:
                        child.terminate()
                        child.wait(timeout=10)
                for _ in range(100):
                    QTest.qWait(20)
                    if window.btn_play.isEnabled():
                        break
            assert owned, 'Launcher did not start the engine'
            assert window.btn_play.isEnabled(), 'Play button did not reset'
            window.close()
            text = launcher.LOG_FILE.read_text(encoding='utf-8', errors='replace')
            cfg = read_config(launcher.CONFIG_FILE)
            summary = next(line for line in text.splitlines() if line.startswith('P3D settings:'))
            fields = dict(part.strip().split('=', 1) for part in summary.split(':', 1)[1].split(','))
            for key in ('vsync', 'fps_limit', 'rcas', 'anisotropy', 'show_fps', 'async_shaders', 'frames_queued'):
                assert fields[key] == cfg[key], (profile['name'], key, fields[key], cfg[key])
            assert f"VSync {'on' if cfg['vsync'] == '1' else 'off'}" in text
            assert f"frame limit {cfg['fps_limit']} FPS" in text
            assert 'P3D image: internal=1920x1080' in text
            if profile['display_mode'] == 'windowed':
                assert f"Window output: {cfg['output_res']} pixels" in text
            elif profile['display_mode'] == 'borderless':
                screen = app.primaryScreen().size()
                assert f'Window output: {screen.width()}x{screen.height()} pixels' in text
            elif 'Window output: 1280x720 pixels' not in text:
                assert 'requested fullscreen 1280x720 unavailable' in text
            facts = [line for line in text.splitlines() if line.startswith(('P3D settings:', 'P3D image:',
                     'Window output:', 'Swapchain:', 'VideoOut:', 'Frame stats:'))]
            results.append(dict(profile=profile, facts=facts, log=str(launcher.LOG_FILE)))
            print('PASS real launcher:', profile['name'], flush=True)

    if '--skip-native' in sys.argv:
        assert (ROOT / 'p3d.ini').read_bytes() == original_ini
        assert snapshot(ROOT / 'user' / 'savedata') == original_saves
        report = output / 'results.json'
        report.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
        print('PASS: original settings and saved games unchanged; report:', report, flush=True)
        return

    # Actual root executable: existing settings, but an isolated save directory.
    native = output / 'native-exe'
    native.mkdir()
    user = native / 'user'
    if (ROOT / 'user' / 'savedata').exists():
        shutil.copytree(ROOT / 'user' / 'savedata', user / 'savedata')
    root_log = ROOT / 'game_last_run.log'
    if root_log.exists():
        shutil.copy2(root_log, native / 'previous-root.log')
    env = os.environ.copy()
    env['GOW3_FRAME_STATS'] = '1'
    child = real_popen([str(ROOT / 'Jugar.exe'), '--user', str(user), '--timeout', '0'],
                       cwd=os.environ.get('TEMP', str(ROOT)), env=env,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    # The wrapper waits for its engine child. Cleanup targets this owned tree.
    try:
        QTest.qWait(16000)
        assert child.poll() is None, f'Native wrapper exited early: {child.returncode}'
        text = root_log.read_text(encoding='utf-8', errors='replace')
        assert 'P3D settings:' in text and 'P3D image:' in text
        shutil.copy2(root_log, native / 'runtime.log')
        results.append(dict(profile={'name': 'native-exe'}, facts=[line for line in text.splitlines()
                       if line.startswith(('P3D settings:', 'P3D image:', 'Window output:', 'VideoOut:'))]))
        print('PASS real Jugar.exe: settings and forwarded isolated user path', flush=True)
    finally:
        # taskkill /T acts only on the PID created above and its descendants.
        if child.poll() is None:
            subprocess.run(['taskkill', '/PID', str(child.pid), '/T', '/F'], capture_output=True)
            child.wait(timeout=10)
    assert (ROOT / 'p3d.ini').read_bytes() == original_ini, 'User settings changed'
    assert snapshot(ROOT / 'user' / 'savedata') == original_saves, 'User saves changed'
    report = output / 'results.json'
    report.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
    print('PASS: original settings and saved games unchanged', flush=True)
    print('Report:', report, flush=True)


if __name__ == '__main__':
    main()
