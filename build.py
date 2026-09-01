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


def configure(target, environment, lto_override=None):
    meson = find_meson()
    ensure_meson_build_directory()
    # The bundled Windows Clang uses lld-link for the extension but Meson
    # selects the system linker for standalone targets; that combination
    # rejects Clang's LTO object format.
    default_lto = target == "template_release" and os.name != "nt"
    lto = (
        ("true" if default_lto else "false")
        if lto_override is None
        else ("true" if lto_override else "false")
    )
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


def compile_build(jobs, verbose, environment, build_target=None):
    meson = find_meson()
    command = [meson, "compile", "-C", str(BUILD_DIR)]
    if build_target:
        command.append(build_target)
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


def cmd_test(args):
    environment = build_environment()
    configure(args.target, environment)
    result = compile_build(args.jobs, args.verbose, environment, "sim_tests")
    if result != 0:
        return result
    meson = find_meson()
    return run(
        [meson, "test", "-C", str(BUILD_DIR), "sim_tests", "--print-errorlogs"],
        environment,
    )


def cmd_benchmark(args):
    environment = build_environment()
    # Keep the standalone benchmark portable across Clang toolchains. Some
    # Windows installations select lld for the extension but use the system
    # linker for executables, where Meson's release LTO flag is unsupported.
    configure(args.target, environment, lto_override=False)
    result = compile_build(
        args.jobs,
        args.verbose,
        environment,
        "sim_perf_benchmark",
    )
    if result != 0:
        return result

    candidates = (
        BUILD_DIR / "sim_perf_benchmark.exe",
        BUILD_DIR / "sim_perf_benchmark",
    )
    benchmark = next((path for path in candidates if path.is_file()), None)
    if benchmark is None:
        fail("Meson built sim_perf_benchmark but its executable was not found")
    print(
        f"Running path benchmark (workers={args.workers}, requests={args.requests})..."
    )
    return run(
        [str(benchmark), str(args.workers), str(args.requests)],
        environment,
    )


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

    test_command = subparsers.add_parser("test")
    test_command.set_defaults(func=cmd_test)
    test_command.add_argument(
        "--target",
        "-t",
        choices=("editor", "template_debug", "template_release"),
        default="template_debug",
    )
    test_command.add_argument("--jobs", "-j", type=int, default=0)
    test_command.add_argument("--verbose", "-v", action="store_true")

    benchmark_command = subparsers.add_parser(
        "benchmark",
        help="Build and run the native path/job performance benchmark",
    )
    benchmark_command.set_defaults(func=cmd_benchmark)
    benchmark_command.add_argument(
        "--target",
        "-t",
        choices=("editor", "template_debug", "template_release"),
        default="template_release",
    )
    benchmark_command.add_argument("--jobs", "-j", type=int, default=0)
    benchmark_command.add_argument("--workers", type=int, default=4)
    benchmark_command.add_argument("--requests", type=int, default=64)
    benchmark_command.add_argument("--verbose", "-v", action="store_true")

    clean = subparsers.add_parser("clean")
    clean.set_defaults(func=cmd_clean)

    distclean = subparsers.add_parser("distclean")
    distclean.set_defaults(func=cmd_distclean)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
