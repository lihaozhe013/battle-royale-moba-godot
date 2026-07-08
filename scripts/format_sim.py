import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SIM_DIR = PROJECT_ROOT / "src_cpp" / "sim"
CLANG_FORMAT = "clang-format"


def find_cpp_files(root: Path) -> list[Path]:
    extensions = {".h", ".cpp"}
    return sorted(p for p in root.rglob("*") if p.suffix in extensions and p.is_file())


def main() -> int:
    files = find_cpp_files(SIM_DIR)
    if not files:
        print(f"No C++ files found under {SIM_DIR}")
        return 0

    style_file = PROJECT_ROOT / ".clang-format"
    cmd = [
        CLANG_FORMAT,
        "-i",
        f"-style=file:{style_file}",
    ]

    print(f"Formatting {len(files)} files under {SIM_DIR} ...")
    for f in files:
        rel = f.relative_to(PROJECT_ROOT)
        print(f"  {rel}")
        result = subprocess.run(
            [*cmd, str(f)],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"  ERROR: {rel}: {result.stderr.strip()}", file=sys.stderr)

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
