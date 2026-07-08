import sys
from pathlib import Path

import yaml


def _load_config() -> dict:
    config_path = Path("tools/map_editor_config.yaml")
    if config_path.exists():
        with open(config_path) as f:
            cfg = yaml.safe_load(f)
            return cfg if isinstance(cfg, dict) else {}
    return {}


def main() -> None:
    cfg = _load_config()

    map_path_str = cfg.get("map")
    if not map_path_str:
        print(
            "Error: 'map' not set. Configure it in tools/map_editor_config.yaml",
            file=sys.stderr,
        )
        sys.exit(1)

    map_path = Path(map_path_str)
    if not map_path.exists():
        print(f"Error: map file not found: {map_path}", file=sys.stderr)
        sys.exit(1)

    from .viewer import MapViewer

    viewer = MapViewer(
        map_path=map_path,
        flip_y=cfg.get("flip_y", False),
        grid_unit=cfg.get("grid", 1.0),
        zoom=cfg.get("zoom", 0),
        width=cfg.get("width", 1280),
        height=cfg.get("height", 800),
    )

    watcher = None
    if cfg.get("watch", True):
        try:
            from .watcher import MapWatcher

            watcher = MapWatcher(map_path)
            watcher.start()
            viewer.set_watching(True)
        except Exception as e:
            print(f"Warning: could not start file watcher: {e}", file=sys.stderr)

    try:
        while viewer.running:
            if watcher and watcher.is_dirty:
                watcher.clear_dirty()
                viewer.reload()
            viewer.tick()
    except KeyboardInterrupt:
        pass
    finally:
        viewer.quit()
        if watcher:
            watcher.stop()


if __name__ == "__main__":
    main()
