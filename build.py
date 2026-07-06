#!/usr/bin/env python3
"""Godot GDExtension build CLI.

Reads build_env.yaml for per-machine toolchain paths and invokes CMake.

Usage:
    uv run build.py build [-t target] [-j jobs] [-v]
    uv run build.py clean
    uv run build.py rebuild [-t target] [-j jobs]
"""

import argparse
import os
import shutil
import subprocess
import sys

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install with: uv add pyyaml")
    sys.exit(1)

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_CPP_DIR = os.path.join(PROJECT_DIR, "src_cpp")
BUILD_DIR = os.path.join(SRC_CPP_DIR, "build")
CONFIG_FILE = os.path.join(PROJECT_DIR, "build_env.yaml")
CONFIG_EXAMPLE = os.path.join(PROJECT_DIR, "build_env.yaml.example")


def _panic(msg):
    print(f"Error: {msg}")
    print(f"Fix your config at: {CONFIG_FILE}")
    print(f"See example:       {CONFIG_EXAMPLE}")
    sys.exit(1)


def load_config():
    if not os.path.exists(CONFIG_FILE):
        _panic(f"Configuration file not found: {CONFIG_FILE}")

    with open(CONFIG_FILE, "r") as f:
        cfg = yaml.safe_load(f)

    if not cfg:
        _panic("Config file is empty")
    if not cfg.get("toolchain"):
        _panic("'toolchain' is required (msvc | clang | gcc | mingw)")

    return cfg


def _path(p):
    return os.path.normpath(p) if p else p


def _require(raw, key, label):
    val = raw.get(key, "")
    if not val:
        _panic(f"'{label}' is required but not set")
    return _path(val)


def _find_cmake(cfg):
    """Find CMake executable. Checks config first, then PATH, then common VS paths."""
    cmake_bin_dir = cfg.get("cmake_bin_dir", "")
    if cmake_bin_dir:
        candidate = os.path.join(_path(cmake_bin_dir), "cmake")
        if sys.platform == "win32":
            candidate += ".exe"
        if os.path.isfile(candidate):
            return candidate
        _panic(f"cmake not found at configured path: {candidate}")

    cmake = shutil.which("cmake")
    if cmake:
        return cmake

    if sys.platform == "win32":
        candidates = [
            os.path.join(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"),
                         "Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"),
            os.path.join(os.environ.get("ProgramFiles", "C:/Program Files"),
                         "Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"),
            os.path.join(os.environ.get("ProgramFiles", "C:/Program Files"),
                         "CMake/bin/cmake.exe"),
        ]
        for c in candidates:
            if os.path.isfile(c):
                return c

    _panic("CMake not found. Install CMake or set 'cmake_bin_dir' in build_env.yaml")


def _find_python(cfg):
    """Find Python 3 executable for godot-cpp binding generation."""
    py = cfg.get("python3_executable", "")
    if py:
        return _path(py)

    # Check the project's .venv first
    for venv in [
        os.path.join(PROJECT_DIR, ".venv", "Scripts", "python.exe"),
        os.path.join(PROJECT_DIR, ".venv", "bin", "python3"),
    ]:
        if os.path.isfile(venv):
            return venv

    py = shutil.which("python3") or shutil.which("python")
    if py:
        return py

    _panic("Python3 not found. godot-cpp needs Python for binding generation.")


def _generator_name(cfg, tc):
    """Determine CMake generator name based on platform and toolchain."""
    VS_GENERATORS = {
        2017: "Visual Studio 15 2017",
        2019: "Visual Studio 16 2019",
        2022: "Visual Studio 17 2022",
        2026: "Visual Studio 18 2026",
    }
    if sys.platform == "win32":
        if tc == "msvc":
            vs_year = cfg.get("msvc", {}).get("vs_year", 2022)
            gen = VS_GENERATORS.get(vs_year)
            if gen is None:
                _panic(f"Unsupported VS year '{vs_year}' in msvc.vs_year. Supported: {list(VS_GENERATORS.keys())}")
            return gen
        elif tc == "mingw":
            return "MinGW Makefiles"
        elif tc == "clang":
            return "Ninja"
    elif sys.platform == "darwin":
        return "Ninja" if tc == "clang" else "Unix Makefiles"
    return "Unix Makefiles"


def _target_to_cmake_config(target):
    return {
        "template_debug": "RelWithDebInfo",
        "template_release": "Release",
        "editor": "Debug",
    }.get(target, "RelWithDebInfo")


def _configure(cfg, tc, cmake, python, target, generator):
    if not os.path.isfile(cmake):
        _panic(f"CMake executable not found: {cmake}")

    cache_file = os.path.join(BUILD_DIR, "CMakeCache.txt")
    if os.path.isfile(cache_file):
        print("CMake build directory already configured.")
        return 0

    print(f"Configuring CMake (generator: {generator})...")
    os.makedirs(BUILD_DIR, exist_ok=True)

    configure_cmd = [
        cmake,
        "-B", BUILD_DIR,
        "-S", SRC_CPP_DIR,
        "-G", generator,
        f"-DGODOTCPP_TARGET={target}",
        f"-DGODOTCPP_API_VERSION=4.7",
        f"-DPython3_EXECUTABLE={python}",
    ]

    if sys.platform == "win32" and tc == "msvc":
        configure_cmd.extend(["-A", "x64"])

    result = subprocess.call(configure_cmd, cwd=SRC_CPP_DIR)
    if result != 0:
        _panic("CMake configuration failed")

    return result


def _build(cfg, tc, cmake, target, jobs, verbose):
    print(f"Building (target={target})...")
    config = _target_to_cmake_config(target)

    build_cmd = [
        cmake,
        "--build", BUILD_DIR,
        "--config", config,
    ]
    if jobs > 0:
        build_cmd.extend(["-j", str(jobs)])
    elif jobs == 0:
        cpu = os.cpu_count() or 4
        build_cmd.extend(["-j", str(max(1, cpu - 1))])
    if verbose:
        build_cmd.append("--verbose")

    return subprocess.call(build_cmd, cwd=SRC_CPP_DIR)


def setup_toolchain(cfg):
    tc = cfg.get("toolchain", "").lower()
    if tc not in ("msvc", "clang", "gcc", "mingw"):
        _panic(f"Unknown toolchain '{tc}'")
    return tc


def cmd_build(cfg, args):
    tc = setup_toolchain(cfg)
    cmake = _find_cmake(cfg)
    python = _find_python(cfg)
    build_cfg = cfg.get("build", {})

    target = args.target or build_cfg.get("target", "template_debug")
    generator = _generator_name(cfg, tc)
    jobs = args.jobs if args.jobs is not None else build_cfg.get("jobs", 0)
    verbose = args.verbose if args.verbose else build_cfg.get("verbose", False)

    _configure(cfg, tc, cmake, python, target, generator)
    return _build(cfg, tc, cmake, target, jobs, verbose)


def cmd_clean(cfg, args):
    if os.path.isdir(BUILD_DIR):
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR)
    # Also clean output DLL
    output_dll = os.path.join(PROJECT_DIR, "addons", "topdown_sim", "topdown_sim.dll")
    if os.path.isfile(output_dll):
        os.remove(output_dll)
    print("Done")
    return 0


def cmd_rebuild(cfg, args):
    code = cmd_clean(cfg, args)
    if code != 0:
        return code
    # Create a namespace with the same args but swap func to cmd_build
    build_args = args
    return cmd_build(cfg, build_args)


def main():
    parser = argparse.ArgumentParser(description="Godot GDExtension build CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    bp = sub.add_parser("build", help="Build the GDExtension library")
    bp.set_defaults(func=cmd_build)
    bp.add_argument("--target", "-t", default=None, help="Build target (editor|template_debug|template_release)")
    bp.add_argument("--jobs", "-j", type=int, default=None, help="Parallel jobs")
    bp.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    sp = sub.add_parser("clean", help="Clean build artifacts")
    sp.set_defaults(func=cmd_clean)

    rp = sub.add_parser("rebuild", help="Clean and rebuild")
    rp.set_defaults(func=cmd_rebuild)
    rp.add_argument("--target", "-t", default=None)
    rp.add_argument("--jobs", "-j", type=int, default=None)
    rp.add_argument("--verbose", "-v", action="store_true")

    args = parser.parse_args()
    cfg = load_config()
    return args.func(cfg, args)


if __name__ == "__main__":
    sys.exit(main())
