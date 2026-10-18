"""Build and run the original PS4 executable through the adapted native runtime.

The engine is built from engine/ and deployed to bin/p3d_engine.exe for the
root launcher. Prepared images and a diagnostic runtime copy stay isolated in
out/p3d_runtime. No original game files or existing saves are modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT / "engine"
OUTPUT = ROOT / "out/p3d_runtime"
DEPLOY = ROOT / "bin/p3d_engine.exe"


def environment(msys: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = str(msys / "clang64/bin") + os.pathsep + env.get("PATH", "")
    # The default upscaler targets a God of War shader hash, not Persona's pipeline.
    env["GOW3_UPSCALER"] = "none"
    env["GOW3_CONFIG"] = str(OUTPUT / "p3d.ini")
    return env


def run(command: list[str], env: dict[str, str], cwd: Path = ROOT) -> None:
    subprocess.run(command, env=env, cwd=cwd, check=True)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_output(*arguments: str) -> str | None:
    """Git metadata is optional in source packages or a repository without HEAD."""
    try:
        result = subprocess.run(["git", "-C", str(REF), *arguments],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                text=True, check=False)
    except FileNotFoundError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def source_snapshot() -> dict:
    sources = set((REF / "src").glob("*"))
    for folder in ("gpu/shim", "gpu/shadps4"):
        sources.update((REF / folder).rglob("*"))
    sources.add(REF / "gpu/CMakeLists.txt")
    return {
        "files": {p.relative_to(ROOT).as_posix(): digest(p)
                  for p in sorted(sources) if p.is_file()},
        "submodules": git_output("submodule", "status", "--recursive"),
    }


def verify_game(game: Path) -> None:
    analysis = json.loads((OUTPUT / "analysis.json").read_text(encoding="utf-8"))
    link = json.loads((OUTPUT / "link.json").read_text(encoding="utf-8"))
    if digest(game / "eboot.bin") != analysis["source_sha256"]:
        raise ValueError("El ejecutable preparado no coincide con este juego. Ejecuta prepare --game con su carpeta.")
    for module in link["modules"]:
        if digest(game / "sce_module" / module["file"]) != module["sha256"]:
            raise ValueError(f"El modulo {module['file']} cambio. Vuelve a ejecutar prepare.")


def prepare(game: Path, env: dict[str, str]) -> None:
    for script in ("prepare.py", "link_modules.py"):
        run([sys.executable, str(REF / "scripts" / script), str(game), "--out", str(OUTPUT)], env)


def build(msys: Path, env: dict[str, str]) -> None:
    cmake = msys / "clang64/bin/cmake.exe"
    if not cmake.is_file():
        raise FileNotFoundError(f"No se encontro CMake en {cmake}")
    build_dir = REF / "out/gpu"
    # Use the established build tree; don't reset the reference checkout or submodules.
    if not (build_dir / "CMakeCache.txt").is_file():
        run([str(cmake), "-S", str(REF / "gpu"), "-B", str(build_dir), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DGOW3_PGO=off", "-DGOW3_LTO=OFF",
             "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"], env)
    before = source_snapshot()
    run([str(cmake), "--build", str(build_dir), "--target", "p3d"], env)
    after = source_snapshot()
    if before != after:
        raise RuntimeError("Hubo cambios de codigo durante la compilacion. Repite build para obtener una version consistente.")
    executable = OUTPUT / "p3d-runtime.exe"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build_dir / "p3d.exe", executable)
    DEPLOY.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build_dir / "p3d.exe", DEPLOY)
    manifest = {
        "reference_commit": git_output("rev-parse", "--verify", "HEAD"),
        "executable_sha256": digest(executable),
        "source_snapshot": after,
        "configuration_sha256": digest(build_dir / "CMakeCache.txt"),
        "compiler": subprocess.check_output(
            [str(msys / "clang64/bin/clang.exe"), "--version"], text=True),
        "coverage": "Runtime and GPU project source hashes, available Git metadata; installed libraries are not hashed",
        "scope": "Original Persona PS4 executable; experimental compatibility runtime, gameplay unverified",
    }
    (OUTPUT / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Runtime compilado: {executable}", flush=True)
    print(f"Motor desplegado: {DEPLOY}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "build", "test", "check", "play", "setup"))
    parser.add_argument("--game", type=Path, default=ROOT / "CUSA12636")
    parser.add_argument("--msys", type=Path, default=Path("C:/msys64"))
    args, extra = parser.parse_known_args()
    if extra and args.action not in ("check", "play"):
        parser.error("Los argumentos adicionales solo se aceptan para check/play")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    env = environment(args.msys)
    game = args.game.resolve()
    if args.action in ("prepare", "setup"):
        prepare(game, env)
    if args.action in ("build", "setup"):
        build(args.msys, env)
    if args.action == "test":
        build_dir = REF / "out/gpu"
        run([str(args.msys / "clang64/bin/cmake.exe"), "--build", str(build_dir),
             "--target", "eventflag-test", "fiber-test"], env)
        for name in ("eventflag-test.exe", "fiber-test.exe"):
            subprocess.run([str(build_dir / name)], env=env, cwd=ROOT, check=True, timeout=20)
    if args.action in ("check", "play"):
        executable = OUTPUT / "p3d-runtime.exe"
        image = OUTPUT / "boot-linked.bin"
        for path in (executable, image, game / "sce_sys/param.sfo"):
            if not path.is_file():
                raise FileNotFoundError(f"Falta {path}. Ejecuta: python tools/p3d_runtime.py setup")
        verify_game(game)
        command = [str(executable), str(image), "--app0", str(game),
                   "--user", str(OUTPUT / "user"), "--timeout", "0"]
        if args.action == "check":
            command += ["--check-imports"]
        command += extra
        log_dir = OUTPUT / "logs"
        log_dir.mkdir(exist_ok=True)
        log_path = log_dir / ("imports.log" if args.action == "check" else "last_run.log")
        # Files, not a detached GUI console: preserve a useful log even on a crash.
        with log_path.open("wb") as log:
            child = subprocess.Popen(command, env=env, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            try:
                result = child.wait(timeout=60 if args.action == "check" else None)
            except (KeyboardInterrupt, subprocess.TimeoutExpired):
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
                print(f"Ejecucion detenida. Registro: {log_path}", flush=True)
                return 124
        print(f"Salida {result}. Registro: {log_path}", flush=True)
        if args.action == "check":
            for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith(("MISSING:", "Imports:", "ERROR:", "STOP:")):
                    print(line)
        return result
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
