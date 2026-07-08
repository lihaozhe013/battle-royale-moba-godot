from dataclasses import dataclass
from typing import List


@dataclass
class WallSnapshot:
    minX: float
    minY: float
    maxX: float
    maxY: float

    @staticmethod
    def from_wall(w) -> "WallSnapshot":
        return WallSnapshot(
            minX=w.minX, minY=w.minY, maxX=w.maxX, maxY=w.maxY
        )

    def apply_to(self, w) -> None:
        w.minX = self.minX
        w.minY = self.minY
        w.maxX = self.maxX
        w.maxY = self.maxY


@dataclass
class WallEdit:
    indices: List[int]
    before: List[WallSnapshot]
    after: List[WallSnapshot]


class UndoStack:
    def __init__(self, max_size: int = 200):
        self._stack: list[WallEdit] = []
        self._ptr: int = -1
        self._max = max_size

    def push(self, edit: WallEdit) -> None:
        self._stack = self._stack[: self._ptr + 1]
        self._stack.append(edit)
        if len(self._stack) > self._max:
            self._stack.pop(0)
        self._ptr = len(self._stack) - 1

    def undo(self) -> WallEdit | None:
        if self._ptr < 0 or not self._stack:
            return None
        edit = self._stack[self._ptr]
        self._ptr -= 1
        return edit

    def redo(self) -> WallEdit | None:
        if self._ptr + 1 >= len(self._stack):
            return None
        self._ptr += 1
        return self._stack[self._ptr]

    def clear(self) -> None:
        self._stack.clear()
        self._ptr = -1

    @property
    def can_undo(self) -> bool:
        return self._ptr >= 0 and len(self._stack) > 0

    @property
    def can_redo(self) -> bool:
        return self._ptr + 1 < len(self._stack)
