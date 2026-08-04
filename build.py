#!/usr/bin/env python3
"""Build the Godot GDExtension with Meson and Clang."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent
SRC_CPP_DIR = PROJECT_DIR / "src_cpp"
BUILD_DIR = SRC_CPP_DIR / "build"
ADDON_DIR = PROJECT_DIR / "addons" / "battle_royale_sim"
ARTIFACT_NAMES = (
    "battle_royale_sim.dll",
    "libbattle_royale_sim.dylib",
    "libbattle_royale_sim.so",
    "battle_royale_sim.pdb",
    "battle_royale_sim.ilk",
)


def fail(message):
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(1)


def find_meson():
    meson = shutil.which("meson")
    if meson:
        return meson
    fail("Meson was not found on PATH. Install Meson before building.")


def find_clang():
    configured_compiler = os.environ.get("CXX")
    if configured_compiler:
        compiler = shutil.which(configured_compiler) or configured_compiler
        if Path(compiler).is_file() or shutil.which(configured_compiler):
            return compiler
        fail(f"CXX points to a compiler that was not found: {configured_compiler}")

    compiler = shutil.which("clang++")
    if compiler:
        return compiler
    fail("clang++ was not found on PATH. Install LLVM or set CXX to a Clang C++ compiler.")


def build_environment():
    environment = os.environ.copy()
    environment["CXX"] = find_clang()
    return environment


def run(command, environment):
    return subprocess.call(command, cwd=PROJECT_DIR, env=environment)


def ensure_meson_build_directory():
    if not BUILD_DIR.exists():
        return
    if (BUILD_DIR / "meson-private").is_dir():
        return
    if (BUILD_DIR / "CMakeCache.txt").is_file():
        shutil.rmtree(BUILD_DIR)
        return
    if any(BUILD_DIR.iterdir()):
        fail(f"Build directory is not a Meson build: {BUILD_DIR}. Run 'make distclean' first.")


def target_build_type(target):
    return {
        "editor": "debug",
        "template_debug": "debugoptimized",
        "template_release": "release",
    }[target]


def configure(target, environment):
    meson = find_meson()
    ensure_meson_build_directory()
    lto = "true" if target == "template_release" else "false"
    command = [
        meson,
        "setup",
        str(BUILD_DIR),
        str(SRC_CPP_DIR),
        f"-Dgodotcpp-target={target}",
        f"-Db_lto={lto}",
        f"--buildtype={target_build_type(target)}",
    ]
    if BUILD_DIR.exists() and (BUILD_DIR / "meson-private").is_dir():
        command.insert(2, "--reconfigure")
    print(f"Configuring Meson (target={target}, compiler={environment['CXX']})...")
    result = run(command, environment)
    if result != 0:
        fail("Meson configuration failed")


def compile_build(jobs, verbose, environment):
    meson = find_meson()
    command = [meson, "compile", "-C", str(BUILD_DIR)]
    if jobs > 0:
        command.extend(["-j", str(jobs)])
    if verbose:
        command.append("--verbose")
    print("Building the GDExtension...")
    return run(command, environment)


def install_build(environment):
    meson = find_meson()
    return run([meson, "install", "-C", str(BUILD_DIR), "--no-rebuild"], environment)


def remove_artifacts():
    for name in ARTIFACT_NAMES:
        path = ADDON_DIR / name
        if path.exists():
            path.unlink()


def cmd_build(args):
    environment = build_environment()
    configure(args.target, environment)
    result = compile_build(args.jobs, args.verbose, environment)
    if result != 0:
        return result
    return install_build(environment)


def cmd_clean(_args):
    if BUILD_DIR.exists() and (BUILD_DIR / "meson-private").is_dir():
        environment = build_environment()
        meson = find_meson()
        result = run([meson, "compile", "-C", str(BUILD_DIR), "--clean"], environment)
        if result != 0:
            return result
    remove_artifacts()
    return 0


def cmd_distclean(_args):
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    remove_artifacts()
    return 0


def cmd_rebuild(args):
    result = cmd_clean(args)
    if result != 0:
        return result
    return cmd_build(args)


def main():
    parser = argparse.ArgumentParser(description="Build the Godot GDExtension with Meson and Clang")
    subparsers = parser.add_subparsers(dest="command", required=True)

    for command_name, handler in (("build", cmd_build), ("rebuild", cmd_rebuild)):
        command = subparsers.add_parser(command_name)
        command.set_defaults(func=handler)
        command.add_argument(
            "--target",
            "-t",
            choices=("editor", "template_debug", "template_release"),
            default="template_debug",
        )
        command.add_argument("--jobs", "-j", type=int, default=0)
        command.add_argument("--verbose", "-v", action="store_true")

    clean = subparsers.add_parser("clean")
    clean.set_defaults(func=cmd_clean)

    distclean = subparsers.add_parser("distclean")
    distclean.set_defaults(func=cmd_distclean)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
