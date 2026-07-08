.PHONY: build clean distclean rebuild package package-windows package-macos run help

# ---- GDExtension build ----
build:
	uv run build.py build

clean:
	uv run build.py clean

distclean:
	uv run build.py distclean

rebuild:
	uv run build.py rebuild

# ---- Packaging (build GDExtension + Godot export) ----
package:
	uv run package.py

package-windows:
	uv run package.py windows

package-macos:
	uv run package.py macos

package-all:
	uv run package.py all

# ---- Map Editor ----
run:
	uv run python -m tools.map_editor

help:
	@echo "Usage:"
	@echo "  make build/clean/rebuild    GDExtension"
	@echo "  make package*               Export"
	@echo "  make run                    Map editor (configure in tools/map_editor_config.yaml)"
