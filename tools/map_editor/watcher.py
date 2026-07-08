from pathlib import Path

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer


class _MapFileHandler(FileSystemEventHandler):
    def __init__(self, target_path: Path):
        self._target = target_path.resolve()
        self._dirty = False

    def _check(self, src: str) -> None:
        if Path(src).resolve() == self._target:
            self._dirty = True

    def on_modified(self, event):
        if not event.is_directory:
            self._check(event.src_path)

    def on_created(self, event):
        if not event.is_directory:
            self._check(event.src_path)

    def on_moved(self, event):
        if event.dest_path:
            self._check(event.dest_path)


class MapWatcher:
    def __init__(self, path: Path):
        self._path = path.resolve()
        self._observer = Observer()
        self._handler = _MapFileHandler(path)
        self._observer.schedule(self._handler, str(path.parent), recursive=False)

    @property
    def is_dirty(self) -> bool:
        return self._handler._dirty

    def clear_dirty(self) -> None:
        self._handler._dirty = False

    def start(self) -> None:
        self._observer.start()

    def stop(self) -> None:
        self._observer.stop()
        self._observer.join()
