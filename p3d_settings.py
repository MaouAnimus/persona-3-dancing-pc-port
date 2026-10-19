"""Settings actually supported by the Persona runtime and its two launch paths."""
from pathlib import Path
import os

OUTPUT_RESOLUTIONS = ('1920x1080', '2560x1440', '3840x2160', '1280x720',
                      '2560x1080', '3440x1440')
FPS_LIMITS = ('60', '30')
DEFAULT_CONFIG = {
    'display_mode': 'borderless', 'output_res': '1920x1080',
    'render_resolution': 'native', 'vsync': '1', 'fps_limit': '60',
    'engine_fps': '60', 'upscaler': 'off', 'fsr1': '0', 'rcas': '1',
    'rcas_strength': '75', 'anisotropy': '8', 'show_fps': '0',
    'async_shaders': '1', 'frames_queued': '1', 'anti_aliasing': 'rcas',
    'close_on_launch': '0',
}


def normalize_config(values):
    cfg = {**DEFAULT_CONFIG, **values}
    for key, choices in {
        'display_mode': ('borderless', 'fullscreen', 'windowed'),
        'output_res': OUTPUT_RESOLUTIONS, 'fps_limit': FPS_LIMITS,
        'anisotropy': ('0', '2', '4', '8', '16'), 'frames_queued': ('0', '1', '2'),
    }.items():
        if cfg[key] not in choices:
            cfg[key] = DEFAULT_CONFIG[key]
    for key in ('vsync', 'rcas', 'show_fps', 'async_shaders', 'close_on_launch'):
        cfg[key] = '1' if str(cfg[key]).lower() in ('1', 'true', 'yes', 'on') else '0'
    if 'rcas' not in values and 'anti_aliasing' in values:
        cfg['rcas'] = '1' if values['anti_aliasing'] in ('rcas', 'fsr1_rcas') else '0'
    try:
        strength = int(cfg['rcas_strength'])
        cfg['rcas_strength'] = str(strength if 0 <= strength <= 100 else 75)
    except (ValueError, TypeError):
        cfg['rcas_strength'] = '75'
    # These startup patches/upscalers were inherited from GOW3. The Persona
    # launcher does not apply a guest resolution/timing patch or calibrated FSR.
    # Raising VideoOut frequency also accelerates the guest intro/title. Keep
    # its original 60 Hz clock until guest timing is independently patched.
    cfg.update(render_resolution='native', engine_fps='60', upscaler='off', fsr1='0')
    cfg['anti_aliasing'] = 'rcas' if cfg['rcas'] == '1' else 'off'
    return cfg


def read_config(path):
    values = {}
    try:
        content = Path(path).read_text(encoding='utf-8-sig')
    except FileNotFoundError:
        return normalize_config(values)
    for line in content.splitlines():
        if '=' in line and not line.lstrip().startswith(('#', ';')):
            key, value = line.split('=', 1)
            values[key.strip()] = value.strip()
    return normalize_config(values)


def write_config(path, values):
    """Atomic update, retaining comments and settings owned by the engine."""
    path = Path(path)
    cfg = normalize_config(values)
    updates = {key: cfg[key] for key in DEFAULT_CONFIG}
    try:
        lines = path.read_text(encoding='utf-8-sig').splitlines()
    except FileNotFoundError:
        lines = ['# Persona 3 Dancing - configuracion del launcher y del motor']
    done, output = set(), []
    for line in lines:
        key = line.split('=', 1)[0].strip() if '=' in line and not line.lstrip().startswith(('#', ';')) else None
        if key in updates:
            if key not in done:
                output.append(f'{key}={updates[key]}')
                done.add(key)
        else:
            output.append(line)
    output.extend(f'{key}={value}' for key, value in updates.items() if key not in done)
    temporary = path.with_name(path.name + '.tmp')
    try:
        temporary.write_text('\n'.join(output) + '\n', encoding='utf-8')
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
    values.update(cfg)


def launch_environment(cfg, root, config_path=None):
    cfg = normalize_config(cfg)
    root = Path(root).resolve()
    env = os.environ.copy()
    # An inherited override must not silently defeat a value selected in the UI.
    for key in ('GOW3_FULLSCREEN', 'GOW3_RENDER_RES', 'GOW3_INTERNAL_WIDTH', 'GOW3_INTERNAL_HEIGHT'):
        env.pop(key, None)
    env.update({
        'GOW3_CONFIG': str(Path(config_path).resolve() if config_path else root / 'p3d.ini'), 'GOW3_UPSCALER': 'off',
        'GOW3_FPS_LIMIT': cfg['fps_limit'],
        'GOW3_VBLANK_HZ': '60',
        'GOW3_ENGINE_FPS': '60', 'GOW3_FSR1': '0', 'GOW3_RCAS': cfg['rcas'],
        'GOW3_ANISOTROPY': cfg['anisotropy'], 'GOW3_FRAMES_AHEAD': cfg['frames_queued'],
        'GOW3_ASYNC_SHADERS': cfg['async_shaders'], 'GOW3_GPU_USER_DIR': str(root / 'user'),
        'GOW3_VK_RECORD_THREAD': '0', 'GOW3_PRESENT_THREAD': '0',
        'GOW3_DRAW_PIPE': '0', 'GOW3_PREP_WORKERS': '0',
        '__GL_THREADED_OPTIMIZATIONS': '0', 'DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1': '1',
        'VK_LOADER_DISABLE_INST_EXT_FILTER': '0',
    })
    prefix = [str(root / 'bin')]
    toolchain = Path('C:/msys64/clang64/bin')
    if toolchain.is_dir():
        prefix.append(str(toolchain))
    env['PATH'] = os.pathsep.join(prefix + [env.get('PATH', '')])
    return env
