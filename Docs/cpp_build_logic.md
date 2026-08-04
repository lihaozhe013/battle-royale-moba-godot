# C++ Build Logic

The native extension uses Meson 1.4 or newer with a Clang C++ compiler. All C++ targets, including the vendored `godot-cpp` sources, compile as C++20.

## Quick start

```bash
make build
```

The build wrapper discovers `clang++` from `PATH`. Set `CXX` only when the compiler is installed outside the normal search path or when selecting a specific Clang installation.

```bash
CXX=/path/to/clang++ make build
```

The build wrapper also finds Meson and Python from `PATH`. No `build_env.yaml` file is required.

## Build commands

```bash
make build
make build TARGET=template_release JOBS=8
make rebuild
make clean
make distclean
```

The Python wrapper accepts the following equivalent target command:

```bash
uv run build.py build --target template_release --jobs 8 --verbose
```

Supported targets are `editor`, `template_debug`, and `template_release`. The default target is `template_debug`.

| Target | Meson build type | Godot compatibility | Use |
| --- | --- | --- | --- |
| `editor` | `debug` | Editor | C++ debugging in the editor |
| `template_debug` | `debugoptimized` | Debug template | Normal development |
| `template_release` | `release` + LTO | Release template | Release testing |

## Build flow

```text
build.py
  ├─ discover Meson, Python, and clang++
  ├─ configure src_cpp/ with Meson
  ├─ trim extension_api.json with build_profile.py
  ├─ generate godot-cpp bindings
  ├─ build the C++20 godot-cpp static library
  ├─ build the C++20 battle_royale_sim shared library
  └─ install the platform library into addons/battle_royale_sim/
```

Generated bindings and Meson metadata stay under `src_cpp/build/`. The generated library is installed directly into the directory referenced by `addons/battle_royale_sim/battle_royale_sim.gdextension`.

## Compiler policy

The default compiler is `clang++`. Meson caches the compiler in each build directory, so changing `CXX` requires `make distclean` before configuring that directory again.

On macOS, install Xcode Command Line Tools. On Linux, install LLVM Clang and the platform C++ runtime. On Windows, use an LLVM Clang installation together with the required Windows SDK and linker, or use a complete MSYS2 Clang environment.

The compiler front end may be Clang on every platform, but the system linker and C++ runtime remain platform-specific.

## Generated bindings

The Meson configuration reads:

- `src_cpp/godot-cpp/gdextension/extension_api.json`
- `src_cpp/godot-cpp/gdextension/gdextension_interface.json`
- `src_cpp/build_profile.json`

The build profile trims the Godot API before binding generation. Changing the profile or the vendored Godot API causes Meson to regenerate the binding inputs during reconfiguration.

## Cleaning

`make clean` removes compiled outputs while keeping the Meson build directory. `make distclean` removes the complete `src_cpp/build/` directory and generated extension artifacts. The latter is required after changing the compiler or Meson configuration.
