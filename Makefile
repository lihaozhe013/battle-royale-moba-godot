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
	
# ---- Map Editor ----
edit-map:
	uv run python -m tools.map_editor

help:
	@echo "Usage:"
	@echo "  make build/clean/rebuild    GDExtension"
	@echo "  make package*               Export"
	@echo "  make edit-map               Map editor (configure in tools/map_editor_config.yaml)"
