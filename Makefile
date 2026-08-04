.PHONY: build clean distclean godot rebuild package package-windows package-macos run format format-sim format-table edit-map help

default: build

TARGET ?= template_debug
JOBS ?= 0

# ---- GDExtension build ----
build:
	uv run build.py build --target $(TARGET) --jobs $(JOBS)

godot:
	godot -e

clean:
	uv run build.py clean

rebuild:
	uv run build.py rebuild --target $(TARGET) --jobs $(JOBS)

distclean:
	uv run build.py distclean

# ---- Formatting ----
format: format-sim format-godot

format-sim:
	uv run python scripts/format_sim.py

format-godot:
	gdformat -l 80 .

# ---- Map Editor ----
edit-map:
	uv run python -m tools.map_editor

help:
	@echo "Usage:"
	@echo "  make build/clean/rebuild    GDExtension (Meson + clang++)"
	@echo "  make distclean              Remove the Meson build directory"
	@echo "  make package*               Export"
	@echo "  make format                 clang-format + table format"
	@echo "  make format-sim             clang-format src_cpp/sim/"
	@echo "  make format-table           table-align @table blocks"
	@echo "  make edit-map               Map editor (configure in tools/map_editor_config.yaml)"
