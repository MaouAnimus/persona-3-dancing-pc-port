#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Persona 3: Dancing in Moonlight — Modern PC Launcher (PySide6 Edition)
Faithful to the visual design mockup: Glassmorphism, P3D Full Moon Blue Aesthetic,
Full Graphic & Control configuration, Log reader, and Native Game Launching.
Includes smooth micro-animations for tab transitions, switches, and audio waves.
"""

import sys
import os
import subprocess
import threading
from pathlib import Path

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QPushButton,
    QComboBox, QCheckBox, QHBoxLayout, QVBoxLayout, QStackedWidget,
    QGridLayout, QTextEdit, QMessageBox, QFrame, QSizePolicy,
    QGraphicsOpacityEffect
)
from PySide6.QtCore import (
    Qt, QSize, QPoint, QEvent, QTimer, QPropertyAnimation, QParallelAnimationGroup,
    QEasingCurve, Property, Signal, Slot
)
from PySide6.QtGui import (
    QPainter, QPixmap, QColor, QFont, QPen, QBrush, QIcon
)

ROOT_DIR = Path(__file__).resolve().parent
BIN_DIR = ROOT_DIR / "bin"
ENGINE_EXE = BIN_DIR / "p3d_engine.exe"
ROOT_P3D_EXE = ROOT_DIR / "p3d.exe"
BOOT_BIN = ROOT_DIR / "out" / "p3d_test" / "boot-linked.bin"
APP0_DIR = ROOT_DIR / "CUSA12636"
CONFIG_FILE = ROOT_DIR / "p3d.ini"
LOG_FILE = ROOT_DIR / "game_last_run.log"
ICON_ICO = ROOT_DIR / "engine" / "launcher" / "p3d.ico"
BG_CLEAN = ROOT_DIR / "design" / "launcher" / "bg_clean.png"
BG_FALLBACK = ROOT_DIR / "design" / "launcher" / "persona3-launcher-concept-v1.png"
SCREENSHOTS_DIR = ROOT_DIR / "user" / "screenshots"
USER_DIR = ROOT_DIR / "user"

from p3d_settings import DEFAULT_CONFIG, read_config, write_config, launch_environment


def load_config() -> dict:
    return read_config(CONFIG_FILE)


def save_config(cfg: dict) -> None:
    write_config(CONFIG_FILE, cfg)


class AnimatedComboBox(QComboBox):
    """Native combo selection with a short slide and fade on opening."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._popup = None
        self._popup_target_pos = None
        self._popup_animation = QParallelAnimationGroup(self)
        self._popup_animation.finished.connect(self._restore_popup)

    def _restore_popup(self):
        if self._popup is not None:
            self._popup.setWindowOpacity(1.0)
            if self._popup_target_pos is not None:
                self._popup.move(self._popup_target_pos)

    def showPopup(self):
        if self.count() == 0 or self.view().isVisible():
            return

        self._popup_animation.stop()
        self._restore_popup()
        self._popup_animation.clear()
        super().showPopup()

        self._popup = self.view().window()
        self._popup.installEventFilter(self)
        if not self._popup.isVisible():
            return

        # Let Qt choose the final position, including popups above the field
        # near a screen edge; only animate the last few pixels of travel.
        self._popup_target_pos = self._popup.pos()
        field_center = self.mapToGlobal(self.rect().center())
        opens_below = self._popup.geometry().center().y() >= field_center.y()
        start_pos = self._popup_target_pos + QPoint(0, -8 if opens_below else 8)

        slide = QPropertyAnimation(self._popup, b"pos")
        slide.setDuration(170)
        slide.setStartValue(start_pos)
        slide.setEndValue(self._popup_target_pos)
        slide.setEasingCurve(QEasingCurve.OutCubic)

        fade = QPropertyAnimation(self._popup, b"windowOpacity")
        fade.setDuration(140)
        fade.setStartValue(0.0)
        fade.setEndValue(1.0)
        fade.setEasingCurve(QEasingCurve.OutCubic)

        self._popup_animation.addAnimation(slide)
        self._popup_animation.addAnimation(fade)
        self._popup.move(start_pos)
        self._popup.setWindowOpacity(0.0)
        self._popup_animation.start()

    def eventFilter(self, watched, event):
        # Qt can dismiss its popup directly, bypassing hidePopup (e.g. Escape).
        if watched is self._popup and event.type() == QEvent.Hide:
            self._popup_animation.stop()
            self._restore_popup()
        return super().eventFilter(watched, event)

    def hidePopup(self):
        # Close immediately so selection, Escape and outside clicks stay crisp.
        self._popup_animation.stop()
        super().hidePopup()
        self._restore_popup()

    def paintEvent(self, event):
        super().paintEvent(event)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setPen(QPen(QColor("#38bdf8" if self.isEnabled() else "#64748b"), 1.5))
        x, y = self.width() - 12, self.height() // 2
        direction = -1 if self.view().isVisible() else 1
        painter.drawLine(x - 4, y - direction * 2, x, y + direction * 2)
        painter.drawLine(x, y + direction * 2, x + 4, y - direction * 2)


class AnimatedToggle(QCheckBox):
    """Pill toggle switch with smooth animated sliding and color interpolation"""
    def __init__(self, parent=None, checked=False):
        super().__init__(parent)
        self.setChecked(checked)
        self.setCursor(Qt.PointingHandCursor)
        self.setFixedSize(46, 22)
        self._pos = 1.0 if checked else 0.0

        self.anim = QPropertyAnimation(self, b"toggleProgress", self)
        self.anim.setDuration(160)
        self.anim.setEasingCurve(QEasingCurve.InOutQuad)

        self.stateChanged.connect(self._on_state_changed)

    def _on_state_changed(self, state):
        self.anim.stop()
        self.anim.setStartValue(self._pos)
        self.anim.setEndValue(1.0 if state else 0.0)
        self.anim.start()

    def get_pos(self):
        return self._pos

    def set_pos(self, val):
        self._pos = val
        self.update()

    # QWidget already owns a QPoint property named "pos". Use a distinct
    # property so the animation moves the thumb instead of the entire widget.
    toggleProgress = Property(float, get_pos, set_pos)

    def hitButton(self, pos):
        # QCheckBox's default hit area only covers its native indicator when
        # there is no text; this custom switch paints across the full widget.
        return self.rect().contains(pos)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        # Interpolación suave de color del riel (track)
        r = int(30 + (0 - 30) * self._pos)
        g = int(41 + (210 - 41) * self._pos)
        b = int(59 + (255 - 59) * self._pos)
        track_color = QColor(r, g, b)

        br_r = int(51 + (56 - 51) * self._pos)
        br_g = int(65 + (189 - 65) * self._pos)
        br_b = int(85 + (248 - 85) * self._pos)
        border_color = QColor(br_r, br_g, br_b)

        painter.setPen(QPen(border_color, 1))
        painter.setBrush(QBrush(track_color))
        painter.drawRoundedRect(0, 0, 46, 22, 11, 11)

        # Posición flotante suave del deslizador
        thumb_x = 3.0 + 23.0 * self._pos
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor("#ffffff")))
        painter.drawEllipse(int(thumb_x), 2, 17, 17)
        painter.end()


class TabNavButton(QPushButton):
    """Stylish active/inactive navigation tab with hover transition"""
    def __init__(self, text, index, parent=None):
        super().__init__(text, parent)
        self.index = index
        self.setCursor(Qt.PointingHandCursor)
        self.setFont(QFont("Segoe UI", 10, QFont.Bold))
        self.setFixedHeight(30)
        self.update_style(False)

    def update_style(self, active: bool):
        if active:
            self.setStyleSheet("""
                QPushButton {
                    background: transparent;
                    color: #00f0ff;
                    border: none;
                    border-bottom: 2px solid #00f0ff;
                    padding-bottom: 4px;
                    letter-spacing: 2px;
                    font-weight: 800;
                }
            """)
        else:
            self.setStyleSheet("""
                QPushButton {
                    background: transparent;
                    color: #94a3b8;
                    border: none;
                    border-bottom: 2px solid transparent;
                    padding-bottom: 4px;
                    letter-spacing: 2px;
                    font-weight: 700;
                }
                QPushButton:hover {
                    color: #38bdf8;
                }
            """)


class P3DModernLauncher(QMainWindow):
    game_started = Signal()
    game_finished = Signal(int, str)

    def __init__(self):
        super().__init__()

        self.setWindowTitle("Persona 3: Dancing in Moonlight — PC Launcher")
        self.setFixedSize(1024, 576)

        if ICON_ICO.exists():
            self.setWindowIcon(QIcon(str(ICON_ICO)))

        self.cfg = load_config()
        self.game_started.connect(self._on_game_started, Qt.QueuedConnection)
        self.game_finished.connect(self._on_game_finished, Qt.QueuedConnection)

        # Cargar fondo de diseño
        self.bg_pixmap = None
        target_bg = BG_CLEAN if BG_CLEAN.exists() else BG_FALLBACK
        if target_bg.exists():
            self.bg_pixmap = QPixmap(str(target_bg))

        self._build_ui()
        self._load_values_into_ui()
        self._start_ambient_effects()

    def paintEvent(self, event):
        painter = QPainter(self)
        if self.bg_pixmap and not self.bg_pixmap.isNull():
            scaled = self.bg_pixmap.scaled(self.size(), Qt.KeepAspectRatioByExpanding, Qt.SmoothTransformation)
            painter.drawPixmap(0, 0, scaled)
        else:
            painter.fillRect(self.rect(), QColor("#020b18"))
        painter.end()

    def _build_ui(self):
        central = QWidget(self)
        self.setCentralWidget(central)

        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(48, 28, 48, 28)
        main_layout.setSpacing(0)

        # Panel izquierdo con ancho controlado para no tapar a Makoto y Aigis
        left_panel = QWidget(central)
        left_panel.setFixedWidth(440)
        panel_layout = QVBoxLayout(left_panel)
        panel_layout.setContentsMargins(0, 0, 0, 0)
        panel_layout.setSpacing(10)

        # ── Cabecera ───────────────────────────────────────────────
        header_widget = QWidget(left_panel)
        header_layout = QVBoxLayout(header_widget)
        header_layout.setContentsMargins(0, 0, 0, 0)
        header_layout.setSpacing(2)

        lbl_title = QLabel("PERSONA 3 🌙", header_widget)
        lbl_title.setStyleSheet("font-size: 26px; font-weight: 800; color: #ffffff; letter-spacing: 2px;")
        header_layout.addWidget(lbl_title)

        lbl_sub = QLabel("DANCING IN MOONLIGHT", header_widget)
        lbl_sub.setStyleSheet("font-size: 11px; font-weight: 700; color: #93c5fd; letter-spacing: 3px;")
        header_layout.addWidget(lbl_sub)

        self.lbl_tag = QLabel("PC LAUNCHER    ııllıl.ıllı", header_widget)
        self.lbl_tag.setStyleSheet("font-size: 10px; font-weight: 700; color: #38bdf8; letter-spacing: 2px;")
        header_layout.addWidget(self.lbl_tag)

        panel_layout.addWidget(header_widget)

        # ── Barra de Navegación de Pestañas ────────────────────────
        tabs_widget = QWidget(left_panel)
        tabs_layout = QHBoxLayout(tabs_widget)
        tabs_layout.setContentsMargins(0, 10, 0, 4)
        tabs_layout.setSpacing(24)

        self.btn_tab_graphics = TabNavButton("GRÁFICOS", 0, tabs_widget)
        self.btn_tab_controls = TabNavButton("CONTROLES", 1, tabs_widget)
        self.btn_tab_log = TabNavButton("REGISTRO", 2, tabs_widget)

        self.btn_tab_graphics.clicked.connect(lambda: self.switch_tab(0))
        self.btn_tab_controls.clicked.connect(lambda: self.switch_tab(1))
        self.btn_tab_log.clicked.connect(lambda: self.switch_tab(2))

        tabs_layout.addWidget(self.btn_tab_graphics)
        tabs_layout.addWidget(self.btn_tab_controls)
        tabs_layout.addWidget(self.btn_tab_log)
        tabs_layout.addStretch()

        panel_layout.addWidget(tabs_widget)

        # ── Stacked Widget (Contenido de cada pestaña con animación) ──
        self.stacked = QStackedWidget(left_panel)
        self.page_graphics = self._create_page_graphics()
        self.page_controls = self._create_page_controls()
        self.page_log = self._create_page_log()

        self.stacked.addWidget(self.page_graphics)
        self.stacked.addWidget(self.page_controls)
        self.stacked.addWidget(self.page_log)

        # Efecto de opacidad para animación de cambio de pestaña
        self.page_opacity_effect = QGraphicsOpacityEffect(self.stacked)
        self.stacked.setGraphicsEffect(self.page_opacity_effect)
        self.page_fade_anim = QPropertyAnimation(self.page_opacity_effect, b"opacity")
        self.page_fade_anim.setDuration(190)
        self.page_fade_anim.setEasingCurve(QEasingCurve.OutCubic)

        panel_layout.addWidget(self.stacked, 1)

        # ── Barra de Acciones Inferior ─────────────────────────────
        actions_widget = QWidget(left_panel)
        actions_layout = QHBoxLayout(actions_widget)
        actions_layout.setContentsMargins(0, 10, 0, 0)
        actions_layout.setSpacing(14)

        self.btn_play = QPushButton("▶  JUGAR", actions_widget)
        self.btn_play.setCursor(Qt.PointingHandCursor)
        self.btn_play.setFixedHeight(44)
        self.btn_play.setFixedWidth(160)
        self.btn_play.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00f0ff, stop:0.6 #00d2ff, stop:1 #0077b6);
                color: #00122e;
                font-weight: 800;
                font-size: 15px;
                letter-spacing: 2px;
                border: none;
                border-radius: 6px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #80f7ff, stop:0.6 #00f0ff, stop:1 #0096c7);
            }
            QPushButton:pressed {
                background: #0077b6;
                color: #e0f2fe;
            }
            QPushButton:disabled {
                background: #1e293b;
                color: #64748b;
            }
        """)
        self.btn_play.clicked.connect(self.launch_game)

        self.btn_save = QPushButton("Guardar ajustes", actions_widget)
        self.btn_save.setCursor(Qt.PointingHandCursor)
        self.btn_save.setFixedHeight(40)
        self.btn_save.setStyleSheet("""
            QPushButton {
                background: rgba(15, 23, 42, 0.75);
                border: 1px solid rgba(255, 255, 255, 0.25);
                color: #e2e8f0;
                font-size: 12px;
                font-weight: 600;
                border-radius: 6px;
                padding-left: 16px;
                padding-right: 16px;
            }
            QPushButton:hover {
                background: rgba(30, 41, 59, 0.85);
                border-color: #38bdf8;
                color: #38bdf8;
            }
        """)
        self.btn_save.clicked.connect(self.save_manual)

        actions_layout.addWidget(self.btn_play)
        actions_layout.addWidget(self.btn_save)
        actions_layout.addStretch()

        panel_layout.addWidget(actions_widget)

        # Añadir al layout principal y empujar hacia la izquierda
        main_layout.addWidget(left_panel)
        main_layout.addStretch()

        self.switch_tab(0, animated=False)

    def _start_ambient_effects(self):
        """Micro-animación sutil del ecualizador en la cabecera"""
        self._eq_patterns = [
            "PC LAUNCHER    ııllıl.ıllı",
            "PC LAUNCHER    ıl.ııll.ııl",
            "PC LAUNCHER    lıll.ııllıl",
            "PC LAUNCHER    ıllıl.ııllı",
            "PC LAUNCHER    ı.lıll.ııll",
            "PC LAUNCHER    lı.ııllıl.ı"
        ]
        self._eq_index = 0
        self._eq_timer = QTimer(self)
        self._eq_timer.timeout.connect(self._step_eq)
        self._eq_timer.start(340)

    def _step_eq(self):
        self._eq_index = (self._eq_index + 1) % len(self._eq_patterns)
        self.lbl_tag.setText(self._eq_patterns[self._eq_index])

    def switch_tab(self, index: int, animated: bool = True):
        if self.stacked.currentIndex() == index and animated:
            return

        self.stacked.setCurrentIndex(index)
        self.btn_tab_graphics.update_style(index == 0)
        self.btn_tab_controls.update_style(index == 1)
        self.btn_tab_log.update_style(index == 2)

        if animated:
            self.page_opacity_effect.setOpacity(0.0)
            self.page_fade_anim.stop()
            self.page_fade_anim.setStartValue(0.0)
            self.page_fade_anim.setEndValue(1.0)
            self.page_fade_anim.start()
        else:
            self.page_opacity_effect.setOpacity(1.0)

        if index == 2:
            self.refresh_log()

    # ─────────────────────────────────────────────────────────────
    # PÁGINA 1: GRÁFICOS
    # ─────────────────────────────────────────────────────────────
    def _create_page_graphics(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 6, 0, 0)
        layout.setSpacing(7)

        combo_style = """
            QComboBox {
                background-color: rgba(9, 19, 34, 0.92);
                border: 1px solid rgba(56, 189, 248, 0.35);
                border-radius: 6px;
                color: #f1f5f9;
                padding: 3px 10px;
                font-size: 11px;
                min-width: 175px;
                height: 26px;
            }
            QComboBox:hover, QComboBox:on {
                border-color: #00d2ff;
                background-color: rgba(14, 28, 48, 0.95);
            }
            QComboBox:disabled {
                color: #64748b;
                border-color: rgba(100, 116, 139, 0.3);
                background-color: rgba(9, 19, 34, 0.8);
            }
            QComboBox::drop-down {
                subcontrol-origin: padding;
                subcontrol-position: top right;
                width: 22px;
                border-left: none;
            }
            QComboBox::down-arrow {
                image: none;
                border: none;
                width: 0;
                height: 0;
            }
            QComboBox QAbstractItemView {
                background-color: #081426;
                border: 1px solid #00d2ff;
                border-radius: 6px;
                selection-background-color: #0284c7;
                selection-color: #ffffff;
                color: #e2e8f0;
                padding: 4px;
                outline: none;
            }
            QComboBox QAbstractItemView::item {
                height: 26px;
                padding: 2px 10px;
                border-radius: 4px;
                color: #cbd5e1;
            }
            QComboBox QAbstractItemView::item:hover {
                background-color: #0284c7;
                color: #ffffff;
            }
            QComboBox QAbstractItemView::item:selected {
                background-color: #0077b6;
                color: #ffffff;
            }
        """

        def add_row(label_text, widget):
            row = QWidget(page)
            h = QHBoxLayout(row)
            h.setContentsMargins(0, 0, 0, 0)
            lbl = QLabel(label_text, row)
            lbl.setStyleSheet("font-size: 12px; color: #e2e8f0; font-weight: 500;")
            h.addWidget(lbl)
            h.addStretch()
            h.addWidget(widget)
            layout.addWidget(row)

        self.cb_out_res = AnimatedComboBox(page)
        self.cb_out_res.setStyleSheet(combo_style)
        self.cb_out_res.addItems(["1920 × 1080", "2560 × 1440", "3840 × 2160", "1280 × 720", "2560 × 1080", "3440 × 1440"])
        add_row("Resolución de salida", self.cb_out_res)

        self.cb_in_res = AnimatedComboBox(page)
        self.cb_in_res.setStyleSheet(combo_style)
        self.cb_in_res.addItems(["Nativa", "0.75x", "1.25x", "1.5x", "2.0x (4K)"])
        self.cb_in_res.setEnabled(False)
        self.cb_in_res.setToolTip("La imagen del juego es 1920 × 1080. Las otras escalas requieren un parche de Persona todavía no implementado.")
        add_row("Resolución interna", self.cb_in_res)

        self.cb_mode = AnimatedComboBox(page)
        self.cb_mode.setStyleSheet(combo_style)
        self.cb_mode.addItems(["Sin bordes", "Pantalla completa", "Ventana"])
        add_row("Modo de pantalla", self.cb_mode)

        self.cb_fps = AnimatedComboBox(page)
        self.cb_fps.setStyleSheet(combo_style)
        self.cb_fps.addItems(["60 FPS", "30 FPS"])
        self.cb_fps.setToolTip("El reloj original se mantiene a 60 Hz para conservar la velocidad de la intro, el título y el juego. FPS superiores requieren separar renderizado y simulación; todavía no están disponibles.")
        add_row("Límite de FPS", self.cb_fps)

        self.cb_scale = AnimatedComboBox(page)
        self.cb_scale.setStyleSheet(combo_style)
        self.cb_scale.addItems(["Desactivado"])
        self.cb_scale.setEnabled(False)
        self.cb_scale.setToolTip("FSR 1 está desactivado en el motor y FSR 3 no está calibrado para Persona. RCAS se configura por separado.")
        add_row("Reescalado", self.cb_scale)

        self.cb_rcas = AnimatedComboBox(page)
        self.cb_rcas.setStyleSheet(combo_style)
        self.cb_rcas.addItems(["Activada", "Desactivada"])
        add_row("Nitidez RCAS", self.cb_rcas)

        self.cb_af = AnimatedComboBox(page)
        self.cb_af.setStyleSheet(combo_style)
        self.cb_af.addItems(["8×", "16×", "4×", "2×", "Predeterminado"])
        add_row("Filtro anisotrópico", self.cb_af)

        # Switches animados
        self.toggle_vsync = AnimatedToggle(page, checked=True)
        self.toggle_vsync.setAccessibleName("VSync")
        add_row("VSync", self.toggle_vsync)

        self.toggle_fps_osd = AnimatedToggle(page, checked=False)
        self.toggle_fps_osd.setAccessibleName("Mostrar FPS")
        add_row("Mostrar FPS", self.toggle_fps_osd)

        lbl_foot = QLabel("Reloj original: 60 Hz. FPS altos temporalmente desactivados.\nLos ajustes se aplican al iniciar el juego.", page)
        lbl_foot.setStyleSheet("font-size: 10px; color: #64748b; margin-top: 2px;")
        layout.addWidget(lbl_foot)
        layout.addStretch()

        self.cb_mode.currentIndexChanged.connect(self._update_graphics_availability)
        self.cb_out_res.currentIndexChanged.connect(self._update_graphics_availability)

        return page

    def _update_graphics_availability(self):
        borderless = self.cb_mode.currentText() == "Sin bordes"
        self.cb_out_res.setEnabled(not borderless)
        self.cb_out_res.setToolTip(
            "Sin bordes usa la resolución del escritorio; este valor se conserva para ventana o pantalla completa."
            if borderless else "Tamaño de ventana o modo de pantalla completa. Si el monitor no admite el modo, el registro indica la resolución utilizada."
        )
        screen = self.screen() or QApplication.primaryScreen()
        output = f"{screen.size().width()}x{screen.size().height()}" if borderless and screen else self.cb_out_res.currentText().replace(" × ", "x")
        sharpens = output == "1920x1080"
        self.cb_rcas.setEnabled(sharpens)
        if not sharpens:
            self.cb_rcas.setCurrentIndex(1)
        self.cb_rcas.setToolTip("RCAS se aplica cuando la salida coincide con la imagen nativa de 1920 × 1080; con otras salidas está desactivado.")

    # ─────────────────────────────────────────────────────────────
    # PÁGINA 2: CONTROLES
    # ─────────────────────────────────────────────────────────────
    def _create_page_controls(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 4, 0, 0)
        layout.setSpacing(6)

        lbl_title = QLabel("GUÍA DE CONTROLES", page)
        lbl_title.setStyleSheet("font-size: 13px; font-weight: 800; color: #00e5ff; letter-spacing: 1px;")
        layout.addWidget(lbl_title)

        lbl_sub = QLabel("Teclado y mando", page)
        lbl_sub.setStyleSheet("font-size: 11px; color: #94a3b8; margin-bottom: 2px;")
        layout.addWidget(lbl_sub)

        grid = QGridLayout()
        grid.setContentsMargins(0, 4, 0, 4)
        grid.setHorizontalSpacing(12)
        grid.setVerticalSpacing(7)

        # Encabezados
        h_act = QLabel("ACCIÓN", page)
        h_act.setStyleSheet("color: #38bdf8; font-weight: 700; font-size: 10px; letter-spacing: 1px; padding-bottom: 3px; border-bottom: 1px solid rgba(56, 189, 248, 0.3);")
        h_kbd = QLabel("TECLADO", page)
        h_kbd.setStyleSheet("color: #38bdf8; font-weight: 700; font-size: 10px; letter-spacing: 1px; padding-bottom: 3px; border-bottom: 1px solid rgba(56, 189, 248, 0.3);")
        h_pad = QLabel("MANDO", page)
        h_pad.setStyleSheet("color: #38bdf8; font-weight: 700; font-size: 10px; letter-spacing: 1px; padding-bottom: 3px; border-bottom: 1px solid rgba(56, 189, 248, 0.3);")

        grid.addWidget(h_act, 0, 0)
        grid.addWidget(h_kbd, 0, 1)
        grid.addWidget(h_pad, 0, 2)

        items = [
            ("Notas izquierdas", "W · A · S", "D-Pad ↑ ← ↓"),
            ("Notas derechas", "I · L · K / Flechas", "△ ○ ✕ / Y B A"),
            ("Scratch / Fever", "Espacio / Shift", "L1 / R1 / Sticks"),
            ("Pausa", "Esc / P", "Options / Start"),
            ("Avanzar diálogo", "Enter / X / K / ↓", "✕ / A"),
            ("Cancelar / Volver", "Retroceso / Z / L", "○ / B"),
            ("Automático", "I / ↑", "△ / Y"),
            ("Saltar diálogo", "E / 3", "R1"),
        ]

        for idx, (act, kbd, pad) in enumerate(items, start=1):
            l_act = QLabel(act, page)
            l_act.setStyleSheet("font-size: 11px; color: #cbd5e1;")

            l_kbd = QLabel(kbd, page)
            l_kbd.setStyleSheet("font-size: 11px; font-weight: 600; color: #ffffff;")

            l_pad = QLabel(pad, page)
            l_pad.setStyleSheet("font-size: 11px; color: #7dd3fc;")

            grid.addWidget(l_act, idx, 0)
            grid.addWidget(l_kbd, idx, 1)
            grid.addWidget(l_pad, idx, 2)

        layout.addLayout(grid)

        lbl_foot = QLabel("Mando: PlayStation / Xbox", page)
        lbl_foot.setStyleSheet("font-size: 10px; color: #64748b; margin-top: 4px;")
        layout.addWidget(lbl_foot)
        layout.addStretch()

        return page

    # ─────────────────────────────────────────────────────────────
    # PÁGINA 3: REGISTRO
    # ─────────────────────────────────────────────────────────────
    def _create_page_log(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 4, 0, 0)
        layout.setSpacing(6)

        lbl_title = QLabel("REGISTRO DE ACTIVIDAD", page)
        lbl_title.setStyleSheet("font-size: 13px; font-weight: 800; color: #00e5ff; letter-spacing: 1px;")
        layout.addWidget(lbl_title)

        lbl_sub = QLabel("Última ejecución", page)
        lbl_sub.setStyleSheet("font-size: 11px; color: #94a3b8;")
        layout.addWidget(lbl_sub)

        # Botones de Registro
        actions_bar = QWidget(page)
        bar_layout = QHBoxLayout(actions_bar)
        bar_layout.setContentsMargins(0, 2, 0, 4)
        bar_layout.setSpacing(10)

        btn_refresh = QPushButton("↺  Recargar", actions_bar)
        btn_refresh.setCursor(Qt.PointingHandCursor)
        btn_refresh.setStyleSheet("""
            QPushButton {
                background: rgba(10, 25, 47, 0.7);
                border: 1px solid rgba(56, 189, 248, 0.4);
                color: #38bdf8;
                padding: 5px 12px;
                font-size: 11px;
                font-weight: 600;
                border-radius: 6px;
            }
            QPushButton:hover {
                background: rgba(56, 189, 248, 0.2);
            }
        """)
        btn_refresh.clicked.connect(self.refresh_log)

        btn_caps = QPushButton("📁  Abrir capturas", actions_bar)
        btn_caps.setCursor(Qt.PointingHandCursor)
        btn_caps.setStyleSheet("""
            QPushButton {
                background: rgba(10, 25, 47, 0.7);
                border: 1px solid rgba(56, 189, 248, 0.4);
                color: #38bdf8;
                padding: 5px 12px;
                font-size: 11px;
                font-weight: 600;
                border-radius: 6px;
            }
            QPushButton:hover {
                background: rgba(56, 189, 248, 0.2);
            }
        """)
        btn_caps.clicked.connect(self.open_screenshots)

        bar_layout.addWidget(btn_refresh)
        bar_layout.addWidget(btn_caps)
        bar_layout.addStretch()
        layout.addWidget(actions_bar)

        lbl_file = QLabel("game_last_run.log", page)
        lbl_file.setStyleSheet("font-size: 10px; color: #7dd3fc; font-family: monospace;")
        layout.addWidget(lbl_file)

        self.txt_log = QTextEdit(page)
        self.txt_log.setReadOnly(True)
        self.txt_log.setFixedHeight(180)
        self.txt_log.setStyleSheet("""
            QTextEdit {
                background: rgba(6, 18, 36, 0.88);
                border: 1px solid rgba(56, 189, 248, 0.35);
                border-radius: 8px;
                padding: 8px;
                font-family: 'Consolas', monospace;
                font-size: 11px;
                color: #93c5fd;
                line-height: 1.4;
            }
            QScrollBar:vertical {
                background: rgba(6, 18, 36, 0.8);
                width: 6px;
                margin: 0;
                border-radius: 3px;
            }
            QScrollBar::handle:vertical {
                background: rgba(56, 189, 248, 0.4);
                border-radius: 3px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background: #00d2ff;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
        """)
        layout.addWidget(self.txt_log)

        lbl_foot = QLabel("Últimas 200 líneas del registro real del juego", page)
        lbl_foot.setStyleSheet("font-size: 10px; color: #64748b; margin-top: 2px;")
        layout.addWidget(lbl_foot)
        layout.addStretch()

        return page

    # ─────────────────────────────────────────────────────────────
    # LÓGICA DE DATOS Y EJECUCIÓN
    # ─────────────────────────────────────────────────────────────
    def _load_values_into_ui(self):
        # Resolución
        res = self.cfg.get("output_res", "1920x1080").replace("x", " × ")
        idx = self.cb_out_res.findText(res)
        if idx >= 0: self.cb_out_res.setCurrentIndex(idx)

        # Resolución interna
        in_res = self.cfg.get("render_resolution", "native")
        map_in = {"native": "Nativa", "0.75x": "0.75x", "1.25x": "1.25x", "1.5x": "1.5x", "2x": "2.0x (4K)"}
        idx = self.cb_in_res.findText(map_in.get(in_res, "Nativa"))
        if idx >= 0: self.cb_in_res.setCurrentIndex(idx)

        # Modo de pantalla
        mode = self.cfg.get("display_mode", "borderless")
        map_mode = {"borderless": "Sin bordes", "fullscreen": "Pantalla completa", "windowed": "Ventana"}
        idx = self.cb_mode.findText(map_mode.get(mode, "Sin bordes"))
        if idx >= 0: self.cb_mode.setCurrentIndex(idx)

        # Límite FPS
        fps = self.cfg.get("fps_limit", "60")
        map_fps = {"60": "60 FPS", "30": "30 FPS"}
        idx = self.cb_fps.findText(map_fps.get(fps, "60 FPS"))
        if idx >= 0: self.cb_fps.setCurrentIndex(idx)

        # Reescalado
        aa = self.cfg.get("anti_aliasing", "rcas")
        map_scale = {"off": "Desactivado", "fsr1": "FSR 1.0", "rcas": "RCAS", "fsr3": "FSR 3.1"}
        idx = self.cb_scale.findText(map_scale.get(aa, "RCAS"))
        if idx >= 0: self.cb_scale.setCurrentIndex(idx)

        # RCAS Nitidez
        rcas_on = self.cfg.get("rcas", "1") == "1"
        self.cb_rcas.setCurrentIndex(0 if rcas_on else 1)

        # Anisotropía
        af = self.cfg.get("anisotropy", "8")
        map_af = {"8": "8×", "16": "16×", "4": "4×", "2": "2×", "0": "Predeterminado"}
        idx = self.cb_af.findText(map_af.get(af, "8×"))
        if idx >= 0: self.cb_af.setCurrentIndex(idx)

        # Switches
        self.toggle_vsync.setChecked(self.cfg.get("vsync", "1") == "1")
        self.toggle_fps_osd.setChecked(self.cfg.get("show_fps", "0") == "1")
        self._update_graphics_availability()

    def _collect_ui_values(self):
        # Resolución salida
        self.cfg["output_res"] = self.cb_out_res.currentText().replace(" × ", "x")

        # Resolución interna
        map_in_rev = {"Nativa": "native", "0.75x": "0.75x", "1.25x": "1.25x", "1.5x": "1.5x", "2.0x (4K)": "2x"}
        self.cfg["render_resolution"] = map_in_rev.get(self.cb_in_res.currentText(), "native")

        # Modo de pantalla
        map_mode_rev = {"Sin bordes": "borderless", "Pantalla completa": "fullscreen", "Ventana": "windowed"}
        self.cfg["display_mode"] = map_mode_rev.get(self.cb_mode.currentText(), "borderless")

        # Límite FPS
        fps_text = self.cb_fps.currentText()
        fps_val = "60"
        for k in ("120", "144", "240", "30"):
            if k in fps_text: fps_val = k; break
        if "Ilimitado" in fps_text: fps_val = "0"
        self.cfg["fps_limit"] = fps_val

        # Sharpening is independent from the (unavailable) temporal upscalers.
        self.cfg["rcas"] = "1" if self.cb_rcas.currentText() == "Activada" else "0"
        self.cfg["anti_aliasing"] = "rcas" if self.cfg["rcas"] == "1" else "off"
        self.cfg["upscaler"] = "off"
        self.cfg["fsr1"] = "0"

        # Anisotropía
        af_text = self.cb_af.currentText()
        af_val = "8"
        for k in ("16", "4", "2"):
            if k in af_text: af_val = k; break
        if "Predeterminado" in af_text: af_val = "0"
        self.cfg["anisotropy"] = af_val

        # Toggles
        self.cfg["vsync"] = "1" if self.toggle_vsync.isChecked() else "0"
        self.cfg["show_fps"] = "1" if self.toggle_fps_osd.isChecked() else "0"

    def save_manual(self):
        self._collect_ui_values()
        if not self._save_settings():
            return
        self.btn_save.setText("✓ Guardado")
        QTimer.singleShot(1500, lambda: self.btn_save.setText("Guardar ajustes"))

    def _save_settings(self):
        try:
            save_config(self.cfg)
        except (OSError, UnicodeError) as error:
            QMessageBox.critical(self, "No se guardaron los ajustes", str(error))
            return False
        return True

    def refresh_log(self):
        self.txt_log.clear()
        if LOG_FILE.exists():
            try:
                lines = LOG_FILE.read_text(encoding="utf-8", errors="replace").splitlines()
                self.txt_log.setPlainText("\n".join(lines[-200:]))
                sb = self.txt_log.verticalScrollBar()
                sb.setValue(sb.maximum())
            except Exception as e:
                self.txt_log.setPlainText(str(e))
        else:
            self.txt_log.setPlainText("Todavía no hay registros. Inicia el juego para generar game_last_run.log.")

    def open_screenshots(self):
        try:
            SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)
            if sys.platform == "win32":
                os.startfile(str(SCREENSHOTS_DIR))
        except OSError as error:
            QMessageBox.critical(self, "No se pudo abrir la carpeta de capturas", str(error))

    def launch_game(self):
        if not self.btn_play.isEnabled():
            return
        self._collect_ui_values()
        if not self._save_settings():
            return

        target_exe = ENGINE_EXE if ENGINE_EXE.exists() else ROOT_P3D_EXE
        if not target_exe.exists():
            QMessageBox.critical(self, "Error", f"No se encontró el ejecutable del juego en:\n{target_exe}")
            return

        cmd = [str(target_exe)]
        if not BOOT_BIN.is_file() or not (APP0_DIR / "eboot.bin").is_file():
            QMessageBox.critical(self, "Faltan archivos del juego", "Se necesitan out/p3d_test/boot-linked.bin y CUSA12636/eboot.bin.")
            return
        cmd.extend([str(BOOT_BIN), "--app0", str(APP0_DIR), "--user", str(USER_DIR)])

        env = launch_environment(self.cfg, ROOT_DIR, CONFIG_FILE)

        self.btn_play.setEnabled(False)
        self.btn_play.setText("EJECUTANDO...")

        def _worker():
            exit_code, error_text = -1, ""
            try:
                with open(LOG_FILE, "w", encoding="utf-8", errors="replace") as log_f:
                    p = subprocess.Popen(cmd, cwd=str(ROOT_DIR), env=env, stdout=log_f, stderr=subprocess.STDOUT,
                                         creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0)
                    self.game_started.emit()
                    exit_code = p.wait()
            except Exception as e:
                error_text = str(e)
            finally:
                self.game_finished.emit(exit_code, error_text)

        threading.Thread(target=_worker, daemon=True).start()

    @Slot()
    def _on_game_started(self):
        if self.cfg.get("close_on_launch") == "1":
            QTimer.singleShot(1500, self.close)

    @Slot(int, str)
    def _on_game_finished(self, exit_code=0, error_text=""):
        self.btn_play.setEnabled(True)
        self.btn_play.setText("▶  JUGAR")
        if self.stacked.currentIndex() == 2:
            self.refresh_log()
        if error_text or exit_code != 0:
            QMessageBox.critical(self, "El juego no terminó correctamente",
                                 error_text or f"Código de salida: {exit_code}. Revisa la pestaña Registro.")


def main():
    QApplication.setHighDpiScaleFactorRoundingPolicy(Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setEffectEnabled(Qt.UI_AnimateCombo, False)

    try:
        launcher = P3DModernLauncher()
    except (OSError, UnicodeError) as error:
        QMessageBox.critical(None, "No se pudo leer la configuración", str(error))
        sys.exit(1)
    launcher.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
