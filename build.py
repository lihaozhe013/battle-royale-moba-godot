#!/usr/bin/env python3
"""Godot GDExtension build CLI.

Reads build_env.yaml for per-machine toolchain paths and invokes CMake.

Usage:
    uv run build.py build [-t target] [-j jobs] [-v]
    uv run build.py clean
    uv run build.py rebuild [-t target] [-j jobs]
"""

import argparse
import glob
import json
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


def _get_vs_env(vs_install_dir, arch="x64"):
    """Run vcvarsall.bat and return the resulting environment as a dict."""
    if sys.platform != "win32":
        return None
    vcvarsall = os.path.join(vs_install_dir, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    if not os.path.isfile(vcvarsall):
        return None
    script = "import json, os; print(json.dumps(dict(os.environ)))"
    cmd = f'call "{vcvarsall}" {arch} >nul 2>&1 && "{sys.executable}" -c "{script}"'
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        return None
    try:
        return json.loads(result.stdout.strip())
    except (json.JSONDecodeError, ValueError):
        return None


def _require(raw, key, label):
    val = raw.get(key, "")
    if not val:
        _panic(f"'{label}' is required but not set")
    return _path(val)


def _find_cmake(cfg):
    """Find CMake executable. Checks config first, then vs_install_dir, then PATH."""
    cmake_bin_dir = cfg.get("cmake_bin_dir", "")
    if cmake_bin_dir:
        candidate = os.path.join(_path(cmake_bin_dir), "cmake")
        if sys.platform == "win32":
            candidate += ".exe"
        if os.path.isfile(candidate):
            return candidate
        _panic(f"cmake not found at configured path: {candidate}")

    vs_dir = cfg.get("msvc", {}).get("vs_install_dir", "")
    if vs_dir:
        candidate = os.path.join(_path(vs_dir), "Common7", "IDE", "CommonExtensions",
                                 "Microsoft", "CMake", "CMake", "bin", "cmake.exe")
        if os.path.isfile(candidate):
            return candidate

    cmake = shutil.which("cmake")
    if cmake:
        return cmake

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
    """Ninja is used on all platforms for fast, parallel builds."""
    return "Ninja"


def _target_to_cmake_config(target):
    return {
        "template_debug": "RelWithDebInfo",
        "template_release": "Release",
        "editor": "Debug",
    }.get(target, "RelWithDebInfo")


def _target_to_cmake_build_type(target):
    return {
        "template_debug": "RelWithDebInfo",
        "template_release": "Release",
        "editor": "Debug",
    }.get(target, "Debug")

def _configure(cfg, tc, cmake, python, target, generator):
    if not os.path.isfile(cmake):
        _panic(f"CMake executable not found: {cmake}")

    build_type = _target_to_cmake_build_type(target)
    print(f"Configuring CMake (generator: {generator}, target: {target}, build_type: {build_type})...")
    os.makedirs(BUILD_DIR, exist_ok=True)

    configure_cmd = [
        cmake,
        "-B", BUILD_DIR,
        "-S", SRC_CPP_DIR,
        "-G", generator,
        f"-DGODOTCPP_TARGET={target}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DGODOTCPP_API_VERSION=4.7",
        f"-DPython3_EXECUTABLE={python}",
    ]

    env = None
    if tc == "msvc" and sys.platform == "win32":
        vs_dir = cfg.get("msvc", {}).get("vs_install_dir", "")
        if vs_dir:
            vs_env = _get_vs_env(vs_dir)
            if vs_env:
                env = vs_env

    result = subprocess.call(configure_cmd, cwd=SRC_CPP_DIR, env=env)
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

    env = None
    if tc == "msvc" and sys.platform == "win32":
        vs_dir = cfg.get("msvc", {}).get("vs_install_dir", "")
        if vs_dir:
            vs_env = _get_vs_env(vs_dir)
            if vs_env:
                env = vs_env

    return subprocess.call(build_cmd, cwd=SRC_CPP_DIR, env=env)


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


def _output_artifacts():
    """Return all output artifact paths (libs, pdbs, ilk, temp files) for cleanup."""
    addon_dir = os.path.join(PROJECT_DIR, "addons", "battle_royale_sim")
    patterns = ("battle_royale_sim.dll", "libbattle_royale_sim.dylib", "libbattle_royale_sim.so",
                "battle_royale_sim.pdb", "battle_royale_sim.ilk",
                "~battle_royale_sim.dll", "~battle_royale_sim_*.pdb")
    result = []
    for p in patterns:
        result.extend(glob.glob(os.path.join(addon_dir, p)))
    return result


def cmd_clean(cfg, args):
    tc = setup_toolchain(cfg)
    cmake = _find_cmake(cfg)

    if os.path.isdir(BUILD_DIR):
        print("Cleaning build artifacts (ninja -t clean)...")
        clean_cmd = [cmake, "--build", BUILD_DIR, "--target", "clean"]
        env = None
        if tc == "msvc" and sys.platform == "win32":
            vs_dir = cfg.get("msvc", {}).get("vs_install_dir", "")
            if vs_dir:
                vs_env = _get_vs_env(vs_dir)
                if vs_env:
                    env = vs_env
        subprocess.call(clean_cmd, cwd=SRC_CPP_DIR, env=env)

    for path in _output_artifacts():
        try:
            os.remove(path)
        except OSError:
            pass
    print("Done")
    return 0


def cmd_distclean(cfg, args):
    """Hard clean: nuke the entire build directory and all output artifacts."""
    if os.path.isdir(BUILD_DIR):
        print("Dist-cleaning: removing entire build directory...")
        shutil.rmtree(BUILD_DIR)
    for path in _output_artifacts():
        try:
            os.remove(path)
        except OSError:
            pass
    print("Done")
    return 0


def cmd_rebuild(cfg, args):
    """Rebuild using --clean-first to leverage Ninja's incremental build
    instead of nuking the entire build directory."""
    tc = setup_toolchain(cfg)
    cmake = _find_cmake(cfg)
    python = _find_python(cfg)
    build_cfg = cfg.get("build", {})

    target = args.target or build_cfg.get("target", "template_debug")
    generator = _generator_name(cfg, tc)
    jobs = args.jobs if args.jobs is not None else build_cfg.get("jobs", 0)
    verbose = args.verbose if args.verbose else build_cfg.get("verbose", False)

    _configure(cfg, tc, cmake, python, target, generator)

    print(f"Rebuilding (clean-first, target={target})...")
    config = _target_to_cmake_config(target)
    build_cmd = [
        cmake,
        "--build", BUILD_DIR,
        "--config", config,
        "--clean-first",
    ]
    if jobs > 0:
        build_cmd.extend(["-j", str(jobs)])
    elif jobs == 0:
        cpu = os.cpu_count() or 4
        build_cmd.extend(["-j", str(max(1, cpu - 1))])
    if verbose:
        build_cmd.append("--verbose")

    env = None
    if tc == "msvc" and sys.platform == "win32":
        vs_dir = cfg.get("msvc", {}).get("vs_install_dir", "")
        if vs_dir:
            vs_env = _get_vs_env(vs_dir)
            if vs_env:
                env = vs_env

    return subprocess.call(build_cmd, cwd=SRC_CPP_DIR, env=env)


def main():
    parser = argparse.ArgumentParser(description="Godot GDExtension build CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    bp = sub.add_parser("build", help="Build the GDExtension library")
    bp.set_defaults(func=cmd_build)
    bp.add_argument("--target", "-t", default=None, help="Build target (editor|template_debug|template_release)")
    bp.add_argument("--jobs", "-j", type=int, default=None, help="Parallel jobs")
    bp.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    sp = sub.add_parser("clean", help="Clean build artifacts (ninja -t clean)")
    sp.set_defaults(func=cmd_clean)

    dp = sub.add_parser("distclean", help="Clean build directory + all cached files entirely")
    dp.set_defaults(func=cmd_distclean)

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
