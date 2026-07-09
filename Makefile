.PHONY: build clean distclean rebuild package package-windows package-macos run format format-sim format-table edit-map help

# ---- GDExtension build ----
build:
	uv run build.py build

clean:
	uv run build.py clean

distclean:
	uv run build.py distclean

rebuild:
	uv run build.py rebuild
	
# ---- Formatting ----
format: format-sim format-table

format-sim:
	uv run python scripts/format_sim.py

format-table:
	uv run python scripts/format_table.py

# ---- Map Editor ----
edit-map:
	uv run python -m tools.map_editor

help:
	@echo "Usage:"
	@echo "  make build/clean/rebuild    GDExtension"
	@echo "  make package*               Export"
	@echo "  make format                 clang-format + table format"
	@echo "  make format-sim             clang-format src_cpp/sim/"
	@echo "  make format-table           table-align @table blocks"
	@echo "  make edit-map               Map editor (configure in tools/map_editor_config.yaml)"
