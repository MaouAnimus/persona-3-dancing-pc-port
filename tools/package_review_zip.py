import os
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT_ZIP = ROOT / "persona3_dancing_pc_source.zip"

EXCLUDE_DIRS = {
    ".git", ".vscode", ".vs", "build", "out", "bin", "user",
    "CUSA12636", "extracted_assets", "ghidra_12.1.4_PUBLIC_20260921",
    "backup_dlls", "__pycache__", "ghidra_projects"
}

# Subdirectories inside engine/gpu/third_party to exclude to keep zip lightweight
EXCLUDE_THIRD_PARTY_DIRS = {
    "fsr-vulkan", "sirit", "miniz", "xbyak", "magic_enum", "fonts"
}

EXCLUDE_EXTENSIONS = {
    ".exe", ".dll", ".o", ".obj", ".a", ".lib", ".pdb", ".cpk", ".bin", ".prx", ".iso"
}

def should_exclude(rel_path: Path) -> bool:
    parts = rel_path.parts
    for part in parts:
        if part in EXCLUDE_DIRS:
            return True
    if len(parts) >= 4 and parts[0] == "engine" and parts[1] == "gpu" and parts[2] == "third_party":
        if parts[3] in EXCLUDE_THIRD_PARTY_DIRS:
            return True
    if rel_path.name == OUTPUT_ZIP.name:
        return True
    if rel_path.suffix.lower() in EXCLUDE_EXTENSIONS:
        return True
    return False

def package_project():
    if OUTPUT_ZIP.exists():
        OUTPUT_ZIP.unlink()

    total_files = 0
    total_bytes = 0

    print(f"Creating package: {OUTPUT_ZIP.name} ...")
    with zipfile.ZipFile(OUTPUT_ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root, dirs, files in os.walk(ROOT):
            # Prune directory search
            rel_dir = Path(root).relative_to(ROOT)
            dirs[:] = [d for d in dirs if not should_exclude(rel_dir / d)]

            for file in files:
                file_path = Path(root) / file
                rel_file = file_path.relative_to(ROOT)
                if should_exclude(rel_file):
                    continue

                zf.write(file_path, str(rel_file))
                total_files += 1
                total_bytes += file_path.stat().st_size

    zip_size_mb = OUTPUT_ZIP.stat().st_size / (1024 * 1024)
    raw_size_mb = total_bytes / (1024 * 1024)
    print(f"Package created successfully!")
    print(f"Total files included: {total_files}")
    print(f"Uncompressed size:    {raw_size_mb:.2f} MB")
    print(f"Zip archive size:     {zip_size_mb:.2f} MB")
    print(f"Path:                 {OUTPUT_ZIP}")

if __name__ == "__main__":
    package_project()
