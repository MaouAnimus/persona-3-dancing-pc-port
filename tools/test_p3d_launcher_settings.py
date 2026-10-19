"""Regression checks for the P3D settings, real UI and launch lifecycle.

Run: python tools/test_p3d_launcher_settings.py
All writes and fake game processes are isolated from the user's configuration.
"""
import itertools
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import Qt, QPoint
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
import p3d_launcher as launcher
import p3d_settings as settings


class TemporarySettings(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='p3d-settings-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.ini = self.root / 'p3d.ini'

class SettingsTests(TemporarySettings):
    def test_supported_combinations_round_trip(self):
        for mode, fps, rcas, vsync, overlay in itertools.product(
                ('windowed', 'borderless', 'fullscreen'), settings.FPS_LIMITS,
                ('0', '1'), ('0', '1'), ('0', '1')):
            cfg = settings.normalize_config(dict(display_mode=mode, fps_limit=fps,
                                                rcas=rcas, vsync=vsync, show_fps=overlay))
            settings.write_config(self.ini, cfg)
            self.assertEqual(settings.read_config(self.ini), cfg)

    def test_unsupported_options_and_legacy_aa_do_not_override_sharpening(self):
        cfg = settings.normalize_config(dict(render_resolution='2x', upscaler='fsr3',
                                            fsr1='1', engine_fps='240', rcas='0',
                                            anti_aliasing='rcas', fps_limit='999'))
        self.assertEqual((cfg['render_resolution'], cfg['upscaler'], cfg['fsr1']), ('native', 'off', '0'))
        self.assertEqual((cfg['rcas'], cfg['anti_aliasing'], cfg['fps_limit']), ('0', 'off', '60'))
        self.assertEqual(settings.normalize_config({'anti_aliasing': 'fsr1_rcas'})['rcas'], '1')

    def test_preserves_engine_settings_comments_and_removes_owned_duplicates(self):
        self.ini.write_text('\ufeff# keep this\noverlay_corner=3\nvsync=0\nvsync=1\ncustom=value\n', encoding='utf-8')
        cfg = settings.read_config(self.ini)
        cfg['vsync'] = '0'
        settings.write_config(self.ini, cfg)
        content = self.ini.read_text(encoding='utf-8')
        self.assertIn('# keep this\noverlay_corner=3\n', content)
        self.assertIn('custom=value', content)
        self.assertEqual(content.count('vsync='), 1)
        self.assertFalse(content.startswith('\ufeff'))

    def test_failed_write_keeps_previous_configuration(self):
        self.ini.write_text('vsync=1\n', encoding='utf-8')
        with patch.object(Path, 'replace', side_effect=PermissionError('read-only')):
            with self.assertRaises(PermissionError):
                settings.write_config(self.ini, {'vsync': '0'})
        self.assertEqual(self.ini.read_text(), 'vsync=1\n')
        self.assertFalse(self.ini.with_suffix('.ini.tmp').exists())

    def test_environment_overrides_stale_values_with_selected_settings(self):
        inherited = dict(GOW3_FULLSCREEN='1', GOW3_RENDER_RES='4K', GOW3_RCAS='1',
                         GOW3_ANISOTROPY='2', GOW3_FPS_LIMIT='30', GOW3_VBLANK_HZ='60')
        with patch.dict(os.environ, inherited):
            for fps in settings.FPS_LIMITS:
                env = settings.launch_environment(dict(fps_limit=fps, rcas='0', anisotropy='16'), self.root)
                self.assertEqual(env['GOW3_FPS_LIMIT'], fps)
                self.assertEqual(env['GOW3_VBLANK_HZ'], str(max(60, int(fps))))
                self.assertEqual(env['GOW3_RCAS'], '0')
                self.assertEqual(env['GOW3_ANISOTROPY'], '16')
                self.assertEqual(env['GOW3_CONFIG'], str(self.ini))
                self.assertNotIn('GOW3_FULLSCREEN', env)
                self.assertNotIn('GOW3_RENDER_RES', env)

    def test_high_fps_settings_and_environment_cannot_accelerate_intro(self):
        for fps in ('0', '120', '144', '240'):
            self.ini.write_text(f'fps_limit={fps}\nengine_fps=240\nvsync=0\n', encoding='utf-8')
            cfg = settings.read_config(self.ini)
            self.assertEqual(cfg['fps_limit'], '60')
            with patch.dict(os.environ, {'GOW3_VBLANK_HZ': '240', 'GOW3_FPS_LIMIT': '144'}):
                env = settings.launch_environment(cfg, self.root)
            self.assertEqual(env['GOW3_VBLANK_HZ'], '60')
            self.assertEqual(env['GOW3_FPS_LIMIT'], '60')
            self.assertEqual(env['GOW3_ENGINE_FPS'], '60')
            settings.write_config(self.ini, cfg)
            self.assertIn('fps_limit=60\n', self.ini.read_text())


class LauncherTests(TemporarySettings):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setStyle('Fusion')
        cls.app.setEffectEnabled(Qt.UI_AnimateCombo, False)
        cls.app.setQuitOnLastWindowClosed(False)

    def setUp(self):
        super().setUp()
        engine = self.root / 'bin' / 'p3d_engine.exe'
        boot = self.root / 'out' / 'p3d_test' / 'boot-linked.bin'
        game = self.root / 'CUSA12636'
        for file in (engine, boot, game / 'eboot.bin'):
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'test fixture')
        self.overrides = patch.multiple(launcher, ROOT_DIR=self.root, CONFIG_FILE=self.ini,
                                       LOG_FILE=self.root / 'game_last_run.log',
                                       ENGINE_EXE=engine, BOOT_BIN=boot, APP0_DIR=game,
                                       USER_DIR=self.root / 'user',
                                       SCREENSHOTS_DIR=self.root / 'user' / 'screenshots')
        self.overrides.start()
        self.addCleanup(self.overrides.stop)
        self.window = launcher.P3DModernLauncher()
        self.window.show()
        QTest.qWait(30)
        self.addCleanup(self.window.close)

    def test_all_enabled_combo_values_save_and_reload(self):
        self.window.cb_mode.setCurrentText('Ventana')
        pairs = [
            (self.window.cb_out_res, 'output_res', settings.OUTPUT_RESOLUTIONS),
            (self.window.cb_fps, 'fps_limit', settings.FPS_LIMITS),
            (self.window.cb_af, 'anisotropy', ('8', '16', '4', '2', '0')),
        ]
        for combo, key, values in pairs:
            for index, expected in enumerate(values):
                combo.setCurrentIndex(index)
                QTest.mouseClick(self.window.btn_save, Qt.LeftButton)
                self.assertEqual(settings.read_config(self.ini)[key], expected)
        for index, expected in enumerate(('borderless', 'fullscreen', 'windowed')):
            self.window.cb_mode.setCurrentIndex(index)
            self.window.save_manual()
            self.assertEqual(settings.read_config(self.ini)['display_mode'], expected)

    def test_unsupported_fields_and_borderless_resolution_are_disabled(self):
        self.assertFalse(self.window.cb_in_res.isEnabled())
        self.assertFalse(self.window.cb_scale.isEnabled())
        self.assertEqual([self.window.cb_fps.itemText(i) for i in range(self.window.cb_fps.count())], ['60 FPS', '30 FPS'])
        self.window.cb_mode.setCurrentText('Sin bordes')
        self.assertFalse(self.window.cb_out_res.isEnabled())
        self.window.cb_mode.setCurrentText('Ventana')
        self.window.cb_out_res.setCurrentText('2560 × 1440')
        self.assertFalse(self.window.cb_rcas.isEnabled())
        self.assertEqual(self.window.cb_rcas.currentText(), 'Desactivada')

    def test_toggles_hit_area_animation_and_real_persistence(self):
        for toggle, key in [(self.window.toggle_vsync, 'vsync'), (self.window.toggle_fps_osd, 'show_fps')]:
            for x in (2, 23, 43):
                before = toggle.isChecked()
                geometry = toggle.geometry()
                QTest.mouseClick(toggle, Qt.LeftButton, pos=QPoint(x, 11))
                for _ in range(40):
                    QTest.qWait(10)
                    if toggle.get_pos() == float(not before):
                        break
                self.assertEqual(toggle.isChecked(), not before)
                self.assertEqual(toggle.get_pos(), float(not before))
                self.assertEqual(toggle.geometry(), geometry)
                self.window.save_manual()
                self.assertEqual(settings.read_config(self.ini)[key], str(int(not before)))

    def test_missing_log_shows_no_fake_hardware_or_game_run(self):
        self.window.refresh_log()
        self.assertIn('Todavía no hay registros', self.window.txt_log.toPlainText())
        self.assertNotIn('NVIDIA', self.window.txt_log.toPlainText())
        launcher.LOG_FILE.write_text('\n'.join(str(i) for i in range(250)), encoding='utf-8')
        self.window.refresh_log()
        self.assertEqual(self.window.txt_log.toPlainText().splitlines(), [str(i) for i in range(50, 250)])

    def test_save_failure_is_reported_and_does_not_launch(self):
        with patch.object(launcher, 'save_config', side_effect=PermissionError('denied')), \
                patch.object(launcher.QMessageBox, 'critical') as message, \
                patch.object(launcher.subprocess, 'Popen') as process:
            self.window.save_manual()
            self.assertNotIn('Guardado', self.window.btn_save.text())
            self.window.launch_game()
            process.assert_not_called()
            self.assertEqual(message.call_count, 2)

    def test_launch_runs_selected_environment_and_restores_play_button(self):
        self.window.cb_fps.setCurrentText('60 FPS')
        self.window.toggle_vsync.setChecked(False)
        with patch.object(launcher.subprocess, 'Popen') as process, \
                patch.object(launcher.QMessageBox, 'critical') as message:
            process.return_value.wait.return_value = 0
            self.window.launch_game()
            for _ in range(30):
                QTest.qWait(10)
                if self.window.btn_play.isEnabled():
                    break
            self.assertTrue(self.window.btn_play.isEnabled())
            self.assertEqual(self.window.btn_play.text(), '▶  JUGAR')
            args, kwargs = process.call_args
            self.assertEqual(args[0][1:], [str(launcher.BOOT_BIN), '--app0', str(launcher.APP0_DIR),
                                         '--user', str(launcher.USER_DIR)])
            self.assertEqual(kwargs['env']['GOW3_FPS_LIMIT'], '60')
            self.assertEqual(kwargs['env']['GOW3_VBLANK_HZ'], '60')
            self.assertEqual(settings.read_config(self.ini)['vsync'], '0')
            message.assert_not_called()

    def test_process_error_is_reported_on_gui_thread(self):
        with patch.object(launcher.subprocess, 'Popen', side_effect=OSError('cannot start')), \
                patch.object(launcher.QMessageBox, 'critical') as message:
            self.window.launch_game()
            for _ in range(30):
                QTest.qWait(10)
                if self.window.btn_play.isEnabled():
                    break
            self.assertTrue(self.window.btn_play.isEnabled())
            message.assert_called_once()
            self.assertIn('cannot start', message.call_args.args[-1])

    def test_close_on_launch_and_missing_game_files(self):
        launcher.BOOT_BIN.unlink()
        with patch.object(launcher.QMessageBox, 'critical') as message, \
                patch.object(launcher.subprocess, 'Popen') as process:
            self.window.launch_game()
            message.assert_called_once()
            process.assert_not_called()
        launcher.BOOT_BIN.write_bytes(b'test fixture')
        self.window.cfg['close_on_launch'] = '1'
        with patch.object(launcher.subprocess, 'Popen') as process:
            process.return_value.wait.return_value = 0
            self.window.launch_game()
            for _ in range(100):
                QTest.qWait(20)
                if not self.window.isVisible():
                    break
            self.assertFalse(self.window.isVisible())

    def test_navigation_log_refresh_and_capture_button(self):
        self.window.btn_tab_controls.click()
        self.assertEqual(self.window.stacked.currentIndex(), 1)
        self.window.btn_tab_log.click()
        self.assertEqual(self.window.stacked.currentIndex(), 2)
        launcher.LOG_FILE.write_text('actual runtime message', encoding='utf-8')
        buttons = self.window.page_log.findChildren(launcher.QPushButton)
        next(button for button in buttons if 'Recargar' in button.text()).click()
        self.assertEqual(self.window.txt_log.toPlainText(), 'actual runtime message')
        with patch.object(launcher.os, 'startfile') as opened:
            next(button for button in buttons if 'Abrir capturas' in button.text()).click()
            self.assertTrue(launcher.SCREENSHOTS_DIR.is_dir())
            opened.assert_called_once_with(str(launcher.SCREENSHOTS_DIR))
        with patch.object(Path, 'mkdir', side_effect=PermissionError('denied')), \
                patch.object(launcher.QMessageBox, 'critical') as message:
            self.window.open_screenshots()
            message.assert_called_once()
        self.window.btn_tab_graphics.click()
        self.assertEqual(self.window.stacked.currentIndex(), 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
