.PHONY: build clean rebuild

build:
	uv run build.py build

clean:
	uv run build.py clean

rebuild:
	uv run build.py rebuild
