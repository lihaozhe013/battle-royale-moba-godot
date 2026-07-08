import math
from pathlib import Path

import pygame

from .map_model import MapData, WallData, load_map, serialize_map
from .commands import UndoStack, WallSnapshot, WallEdit

COLOR_BG = (26, 26, 31)
COLOR_GRID = (42, 42, 47)
COLOR_GRID_DECADE = (58, 58, 63)
COLOR_BOUNDS = (80, 80, 90)
COLOR_WALL_FILL = (102, 102, 115)
COLOR_WALL_OUTLINE = (153, 153, 163)
COLOR_WALL_SELECTED = (255, 209, 102)
COLOR_TEXT = (204, 204, 204)
COLOR_TEXT_DIM = (102, 102, 115)
COLOR_TEXT_SAVED = (80, 200, 80)
COLOR_ERROR_BG = (200, 0, 0, 100)
COLOR_ERROR_TEXT = (255, 80, 80)
COLOR_HELP_BG = (10, 10, 14, 220)
COLOR_ORIGIN = (100, 100, 100)
COLOR_ZONE = (60, 120, 200, 40)
COLOR_ZONE_OUTLINE = (80, 160, 240)
COLOR_BOX_SELECT = (255, 209, 102, 60)
COLOR_BOX_OUTLINE = (255, 209, 102)
COLOR_EDIT_BG = (20, 20, 26, 230)
COLOR_EDIT_FIELD = (40, 40, 50)
COLOR_EDIT_FIELD_ACTIVE = (255, 209, 102)
COLOR_INSPECT_BG = (10, 10, 14, 200)
FONT_SIZE = 18
FONT_SIZE_LARGE = 36
FONT_SIZE_ERROR = 22
FONT_SIZE_HELP = 18
FONT_SIZE_EDIT = 20
FONT_SIZE_INSPECT = 16


class MapViewer:
    def __init__(
        self,
        map_path: Path,
        cfg: dict | None = None,
        flip_y: bool = False,
        grid_unit: float = 1.0,
        zoom: float = 0,
        width: int = 1280,
        height: int = 800,
    ):
        if cfg is None:
            cfg = {}
        self._map_path = map_path
        self._cfg = cfg
        self._flip_y = flip_y
        self._grid_unit = grid_unit
        self._size = (width, height)

        self._cx = 0.0
        self._cy = 0.0
        self._zoom = zoom

        self._panning = False
        self._pan_start_mouse: tuple[int, int] | None = None
        self._pan_start_camera: tuple[float, float] | None = None
        self._show_grid = True
        self._show_help = False
        self._help_lines: list[str] = []
        self._show_zone = False

        self._map: MapData | None = None
        self._last_valid: MapData | None = None
        self._error: str | None = None
        self._watching = False
        self._suppress_reload = False

        self._selected: set[int] = set()
        self._left_drag = False
        self._drag_walls: dict[int, tuple[float, float]] = {}
        self._drag_start_wx = 0.0
        self._drag_start_wy = 0.0
        self._box_start: tuple[int, int] | None = None
        self._box_end: tuple[int, int] | None = None
        self._box_selecting = False

        self._status_msg: str | None = None
        self._status_timer = 0
        self._last_key_time: dict[int, int] = {}
        self._undo = UndoStack()

        self._mode = "normal"
        self._edit_wall_idx = -1
        self._edit_fields: list[dict] = []
        self._edit_focus = 0
        self._save_as_path = ""

        self._inspect_lines: list[str] = []
        self._show_inspect = False

        zo = cfg.get("zone", {})
        self._zone_cx = zo.get("center_x", 0.0)
        self._zone_cy = zo.get("center_y", 0.0)
        self._zone_radius = zo.get("radius", 30.0)

        self._screen: pygame.Surface | None = None
        self._clock: pygame.time.Clock | None = None
        self._running = True
        self._font: pygame.font.Font | None = None
        self._font_large: pygame.font.Font | None = None
        self._font_error: pygame.font.Font | None = None
        self._font_help: pygame.font.Font | None = None
        self._font_edit: pygame.font.Font | None = None
        self._font_inspect: pygame.font.Font | None = None

        self._init_pygame()
        self._load_help_text()
        self._load_map()

    def _init_font(self, size: int) -> pygame.font.Font:
        for name in ["Menlo", "Monaco", "Courier New", "monospace"]:
            f = pygame.font.SysFont(name, size)
            if f.render("W", True, (255, 255, 255)).get_width() > 4:
                return f
        return pygame.font.Font(None, size)

    def _init_pygame(self) -> None:
        pygame.init()
        pygame.key.set_repeat(1000, 50)
        flags = pygame.RESIZABLE
        self._screen = pygame.display.set_mode(self._size, flags)
        pygame.display.set_caption(f"Map Editor — {self._map_path}")
        self._clock = pygame.time.Clock()
        self._font = self._init_font(FONT_SIZE)
        self._font_large = self._init_font(FONT_SIZE_LARGE)
        self._font_error = self._init_font(FONT_SIZE_ERROR)
        self._font_help = self._init_font(FONT_SIZE_HELP)
        self._font_edit = self._init_font(FONT_SIZE_EDIT)
        self._font_inspect = self._init_font(FONT_SIZE_INSPECT)

    def _load_map(self) -> None:
        try:
            self._map = load_map(self._map_path)
            self._last_valid = self._map
            self._error = None
            if self._zoom == 0:
                self._fit_map()
            self._selected.clear()
            self._undo.clear()
        except Exception as e:
            self._error = str(e)
            if self._last_valid is not None:
                self._map = self._last_valid
            else:
                self._map = None

    def _load_help_text(self) -> None:
        help_path = Path("tools/map_editor_help.txt")
        if help_path.exists():
            self._help_lines = help_path.read_text(encoding="utf-8").splitlines()
        else:
            self._help_lines = ["[help file not found: tools/map_editor_help.txt]"]

    def _fit_map(self) -> None:
        if self._map is None:
            return
        w, h = self._size
        half = self._map.half
        world_size = half * 2 + 4
        self._zoom = min(w / world_size, h / world_size)
        self._cx = 0.0
        self._cy = 0.0

    def _w2s(self, wx: float, wy: float) -> tuple[float, float]:
        w, h = self._size
        sx = w * 0.5 + (wx - self._cx) * self._zoom
        if self._flip_y:
            sy = h * 0.5 - (wy - self._cy) * self._zoom
        else:
            sy = h * 0.5 + (wy - self._cy) * self._zoom
        return sx, sy

    def _s2w(self, sx: float, sy: float) -> tuple[float, float]:
        w, h = self._size
        wx = (sx - w * 0.5) / self._zoom + self._cx
        if self._flip_y:
            wy = -(sy - h * 0.5) / self._zoom + self._cy
        else:
            wy = (sy - h * 0.5) / self._zoom + self._cy
        return wx, wy

    def set_watching(self, active: bool) -> None:
        self._watching = active

    def reload(self) -> None:
        if self._suppress_reload:
            self._suppress_reload = False
            return
        self._load_map()

    @property
    def running(self) -> bool:
        return self._running

    def tick(self) -> None:
        self._clock.tick(60)
        self._handle_events()
        self._render()
        pygame.display.flip()
        if self._status_timer > 0:
            self._status_timer -= 1
            if self._status_timer == 0:
                self._status_msg = None

    def quit(self) -> None:
        self._running = False
        pygame.quit()

    # ── snapshot / undo ─────────────────────────────────────────────────

    def _snap(self, indices: set[int]) -> list[WallSnapshot]:
        if self._map is None:
            return []
        return [WallSnapshot.from_wall(self._map.walls[i]) for i in sorted(indices)]

    def _push_undo(self, indices: set[int],
                   before: list[WallSnapshot],
                   after: list[WallSnapshot]) -> None:
        if before == after:
            return
        self._undo.push(WallEdit(sorted(indices), before, after))

    def _apply_snap(self, indices: list[int], snaps: list[WallSnapshot]) -> None:
        if self._map is None:
            return
        for i, s in zip(indices, snaps):
            if 0 <= i < len(self._map.walls):
                s.apply_to(self._map.walls[i])

    # ── events ──────────────────────────────────────────────────────────

    def _hit_test_wall(self, world_x: float, world_y: float) -> int:
        if self._map is None:
            return -1
        tol = 0.5 / max(self._zoom, 0.01)
        for i in range(len(self._map.walls) - 1, -1, -1):
            w = self._map.walls[i].normalized()
            if (w.minX - tol <= world_x <= w.maxX + tol and
                w.minY - tol <= world_y <= w.maxY + tol):
                return i
        return -1

    def _move_selected(self, dx: float, dy: float) -> None:
        if not self._selected or self._map is None:
            return
        before = self._snap(self._selected)
        for idx in self._selected:
            w = self._map.walls[idx]
            w.minX += dx
            w.maxX += dx
            w.minY += dy
            w.maxY += dy
        after = self._snap(self._selected)
        self._push_undo(self._selected, before, after)

    def _scale_selected(self, dx: float, dy: float) -> None:
        if not self._selected or self._map is None:
            return
        before = self._snap(self._selected)
        for idx in self._selected:
            w = self._map.walls[idx]
            nw = w.normalized()
            cx, cy = nw.cx, nw.cy
            hw = max(0.05, nw.width * 0.5 + dx * 0.5)
            hh = max(0.05, nw.height * 0.5 + dy * 0.5)
            w.minX = cx - hw
            w.maxX = cx + hw
            w.minY = cy - hh
            w.maxY = cy + hh
        after = self._snap(self._selected)
        self._push_undo(self._selected, before, after)

    def _handle_undo(self) -> None:
        edit = self._undo.undo()
        if edit is None:
            return
        self._apply_snap(edit.indices, edit.before)
        self._status_msg = "Undo"
        self._status_timer = 60

    def _handle_redo(self) -> None:
        edit = self._undo.redo()
        if edit is None:
            return
        self._apply_snap(edit.indices, edit.after)
        self._status_msg = "Redo"
        self._status_timer = 60

    def _handle_new_wall(self) -> None:
        if self._map is None:
            return
        w = WallData(
            minX=self._cx - 0.5, minY=self._cy - 0.5,
            maxX=self._cx + 0.5, maxY=self._cy + 0.5,
        )
        self._map.walls.append(w)
        idx = len(self._map.walls) - 1
        self._selected = {idx}
        self._undo.clear()
        self._status_msg = "New wall"
        self._status_timer = 60

    def _handle_duplicate_wall(self) -> None:
        if not self._selected or self._map is None:
            return
        new_idx = []
        for src_idx in sorted(self._selected):
            src = self._map.walls[src_idx]
            nw = src.normalized()
            new_w = WallData(
                minX=nw.minX + 1, minY=nw.minY + 1,
                maxX=nw.maxX + 1, maxY=nw.maxY + 1,
            )
            self._map.walls.append(new_w)
            new_idx.append(len(self._map.walls) - 1)
        self._selected = set(new_idx)
        self._undo.clear()
        n = len(new_idx)
        self._status_msg = f"Duplicated {n} wall{'s' if n > 1 else ''}"
        self._status_timer = 60

    def _handle_delete_wall(self) -> None:
        if not self._selected or self._map is None:
            return
        before = self._snap(self._selected)
        for idx in sorted(self._selected, reverse=True):
            del self._map.walls[idx]
        after = self._snap(set())
        self._push_undo(self._selected, before, after)
        n = len(self._selected)
        self._selected.clear()
        self._status_msg = f"Deleted {n} wall{'s' if n > 1 else ''}"
        self._status_timer = 60

    def _handle_rotate_wall(self) -> None:
        if not self._selected or self._map is None:
            return
        before = self._snap(self._selected)
        for idx in self._selected:
            w = self._map.walls[idx]
            nw = w.normalized()
            w_half = nw.width * 0.5
            h_half = nw.height * 0.5
            w.minX = nw.cx - h_half
            w.maxX = nw.cx + h_half
            w.minY = nw.cy - w_half
            w.maxY = nw.cy + w_half
        after = self._snap(self._selected)
        self._push_undo(self._selected, before, after)

    def _save_map(self) -> None:
        if self._map is None:
            return
        text = serialize_map(self._map)
        self._suppress_reload = True
        self._map_path.write_text(text, encoding="utf-8")
        self._status_msg = "Saved!"
        self._status_timer = 90

    def _handle_save_as(self) -> None:
        self._mode = "save_as"
        self._save_as_path = ""
        pygame.key.start_text_input()

    def _finish_save_as(self) -> None:
        self._mode = "normal"
        pygame.key.stop_text_input()
        path = self._save_as_path.strip()
        if not path or self._map is None:
            self._status_msg = "Save As cancelled"
            self._status_timer = 60
            return
        text = serialize_map(self._map)
        Path(path).write_text(text, encoding="utf-8")
        self._status_msg = f"Saved to {path}"
        self._status_timer = 90

    def _start_edit_wall(self, idx: int) -> None:
        if self._map is None or idx < 0 or idx >= len(self._map.walls):
            return
        self._mode = "edit_wall"
        self._edit_wall_idx = idx
        self._edit_focus = 0
        w = self._map.walls[idx].normalized()
        names = ["minX", "minY", "maxX", "maxY"]
        vals = [w.minX, w.minY, w.maxX, w.maxY]
        self._edit_fields = [
            {"name": n, "text": _fmt(v), "dirty": False} for n, v in zip(names, vals)
        ]
        pygame.key.start_text_input()

    def _finish_edit_wall(self) -> None:
        if self._map is None or self._edit_wall_idx < 0:
            return
        before = self._snap({self._edit_wall_idx})
        w = self._map.walls[self._edit_wall_idx]
        vals = []
        for f in self._edit_fields:
            try:
                vals.append(float(f["text"]))
            except ValueError:
                vals.append(None)
        if all(v is not None for v in vals):
            w.minX, w.minY, w.maxX, w.maxY = vals
        after = self._snap({self._edit_wall_idx})
        self._push_undo({self._edit_wall_idx}, before, after)
        self._mode = "normal"
        pygame.key.stop_text_input()
        self._status_msg = "Wall updated"
        self._status_timer = 60

    def _cancel_edit_wall(self) -> None:
        self._mode = "normal"
        pygame.key.stop_text_input()
        self._status_msg = "Edit cancelled"
        self._status_timer = 60

    def _show_inspect_overlay(self) -> None:
        if not self._selected or self._map is None:
            self._show_inspect = False
            return
        lines = ["=== Inspect ==="]
        for idx in sorted(self._selected):
            w = self._map.walls[idx].normalized()
            lines.append(
                f"#{idx}: ({_fmt(w.minX)}, {_fmt(w.minY)}) "
                f"→ ({_fmt(w.maxX)}, {_fmt(w.maxY)})  "
                f"[{_fmt(w.width)} × {_fmt(w.height)}]"
            )
        self._inspect_lines = lines
        self._show_inspect = True

    def _handle_events(self) -> None:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self._running = False

            elif event.type == pygame.KEYDOWN:
                if self._mode == "edit_wall":
                    self._handle_edit_key(event)
                elif self._mode == "save_as":
                    self._handle_save_as_key(event)
                else:
                    self._handle_normal_key(event)

            elif event.type == pygame.TEXTINPUT:
                if self._mode == "edit_wall":
                    self._handle_edit_text(event)
                elif self._mode == "save_as":
                    self._handle_save_as_text(event)

            elif event.type == pygame.MOUSEBUTTONDOWN:
                if self._mode == "edit_wall":
                    self._handle_edit_mouse(event)
                elif self._mode == "normal" and event.button == 1 and event.clicks == 2:
                    wx, wy = self._s2w(event.pos[0], event.pos[1])
                    idx = self._hit_test_wall(wx, wy)
                    if idx >= 0:
                        self._start_edit_wall(idx)
                    else:
                        self._handle_normal_mouse_down(event)
                else:
                    self._handle_normal_mouse_down(event)

            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:
                    if self._box_selecting:
                        self._finish_box_select()
                        self._box_selecting = False
                        self._box_start = None
                        self._box_end = None
                    self._left_drag = False
                    self._drag_walls.clear()
                elif event.button == 3:
                    self._panning = False

            elif event.type == pygame.MOUSEMOTION:
                if self._box_selecting and self._box_start:
                    self._box_end = event.pos
                elif self._left_drag and self._drag_walls and self._map:
                    wx, wy = self._s2w(event.pos[0], event.pos[1])
                    dwx = wx - self._drag_start_wx
                    dwy = wy - self._drag_start_wy
                    for idx, (acx, acy) in self._drag_walls.items():
                        w = self._map.walls[idx]
                        nw = w.normalized()
                        w.minX = acx - nw.width * 0.5 + dwx
                        w.maxX = acx + nw.width * 0.5 + dwx
                        w.minY = acy - nw.height * 0.5 + dwy
                        w.maxY = acy + nw.height * 0.5 + dwy
                elif self._panning and self._pan_start_mouse is not None:
                    dx = event.pos[0] - self._pan_start_mouse[0]
                    dy = event.pos[1] - self._pan_start_mouse[1]
                    cx0, cy0 = self._pan_start_camera
                    self._cx = cx0 - dx / self._zoom
                    if self._flip_y:
                        self._cy = cy0 + dy / self._zoom
                    else:
                        self._cy = cy0 - dy / self._zoom

            elif event.type == pygame.MOUSEWHEEL:
                if self._mode == "normal":
                    factor = 1.15 if event.y > 0 else 0.85
                    self._zoom *= factor
                    self._zoom = max(0.05, min(self._zoom, 2000.0))

            elif event.type == pygame.VIDEORESIZE:
                self._size = (event.w, event.h)
                self._screen = pygame.display.set_mode(self._size, pygame.RESIZABLE)

    def _handle_normal_key(self, event) -> None:
        now = pygame.time.get_ticks()
        last = self._last_key_time.get(event.key, 0)
        is_repeat = (now - last < 1100)
        self._last_key_time[event.key] = now

        mods = pygame.key.get_mods()
        shift = bool(mods & pygame.KMOD_SHIFT)
        alt = bool(mods & pygame.KMOD_ALT)
        ctrl = bool(mods & pygame.KMOD_CTRL)
        step = 5.0 if shift else (0.1 if alt else 1.0)

        if event.key == pygame.K_ESCAPE:
            if self._show_help:
                self._show_help = False
            elif self._show_inspect:
                self._show_inspect = False
            else:
                self._running = False

        elif event.key == pygame.K_f and not is_repeat:
            self._fit_map()
        elif event.key == pygame.K_g and not is_repeat:
            self._show_grid = not self._show_grid
        elif event.key == pygame.K_h and not is_repeat:
            self._show_help = not self._show_help
            if self._show_help:
                self._load_help_text()
        elif event.key == pygame.K_z and not is_repeat:
            self._show_zone = not self._show_zone

        elif event.key == pygame.K_n and not is_repeat:
            self._handle_new_wall()
        elif event.key == pygame.K_d and ctrl and not is_repeat:
            self._handle_duplicate_wall()
        elif event.key in (pygame.K_DELETE, pygame.K_BACKSPACE) and not is_repeat:
            self._handle_delete_wall()
        elif event.key == pygame.K_r and not is_repeat:
            self._handle_rotate_wall()
        elif event.key == pygame.K_s and ctrl and not shift and not is_repeat:
            self._save_map()
        elif event.key == pygame.K_s and ctrl and shift and not is_repeat:
            self._handle_save_as()

        elif event.key == pygame.K_z and ctrl and not shift and not is_repeat:
            self._handle_undo()
        elif event.key == pygame.K_y and ctrl and not is_repeat:
            self._handle_redo()
        elif event.key == pygame.K_z and ctrl and shift and not is_repeat:
            self._handle_redo()

        elif event.key == pygame.K_SPACE and ctrl and not is_repeat:
            self._show_inspect_overlay()

        elif event.key in (pygame.K_w, pygame.K_UP):
            self._move_selected(0, -step)
        elif event.key in (pygame.K_s, pygame.K_DOWN):
            self._move_selected(0, step)
        elif event.key in (pygame.K_a, pygame.K_LEFT):
            self._move_selected(-step, 0)
        elif event.key in (pygame.K_d, pygame.K_RIGHT):
            self._move_selected(step, 0)
        elif event.key == pygame.K_RIGHTBRACKET:
            self._scale_selected(step, 0)
        elif event.key == pygame.K_LEFTBRACKET:
            self._scale_selected(-step, 0)
        elif event.key in (pygame.K_QUOTEDBL, pygame.K_QUOTE):
            self._scale_selected(0, step)
        elif event.key == pygame.K_SEMICOLON:
            self._scale_selected(0, -step)

    def _handle_edit_key(self, event) -> None:
        if event.key == pygame.K_RETURN or event.key == pygame.K_KP_ENTER:
            self._finish_edit_wall()
        elif event.key == pygame.K_ESCAPE:
            self._cancel_edit_wall()
        elif event.key == pygame.K_TAB:
            self._edit_focus = (self._edit_focus + 1) % 4
        elif event.key == pygame.K_BACKSPACE:
            f = self._edit_fields[self._edit_focus]
            f["text"] = f["text"][:-1]
        elif event.key in (pygame.K_MINUS,):
            f = self._edit_fields[self._edit_focus]
            f["text"] += "-"

    def _handle_edit_text(self, event) -> None:
        ch = event.text
        if ch in "0123456789.-":
            f = self._edit_fields[self._edit_focus]
            f["text"] += ch

    def _handle_save_as_key(self, event) -> None:
        if event.key == pygame.K_RETURN or event.key == pygame.K_KP_ENTER:
            self._finish_save_as()
        elif event.key == pygame.K_ESCAPE:
            self._mode = "normal"
            pygame.key.stop_text_input()
            self._status_msg = "Save As cancelled"
            self._status_timer = 60

    def _handle_save_as_text(self, event) -> None:
        ch = event.text
        if ch.isprintable():
            self._save_as_path += ch

    def _handle_edit_mouse(self, event) -> None:
        if self._mode == "edit_wall" and self._edit_fields:
            _, h = self._size
            field_w = 140
            field_h = 32
            gap = 8
            total_w = field_w * 4 + gap * 3
            start_x = (self._size[0] - total_w) // 2
            fy = h // 2 + 20
            for i, f in enumerate(self._edit_fields):
                fx = start_x + i * (field_w + gap)
                if fx <= event.pos[0] <= fx + field_w and fy <= event.pos[1] <= fy + field_h:
                    self._edit_focus = i
                    break

    def _handle_normal_mouse_down(self, event) -> None:
        if event.button == 1:
            wx, wy = self._s2w(event.pos[0], event.pos[1])
            idx = self._hit_test_wall(wx, wy)
            mods = pygame.key.get_mods()
            ctrl = bool(mods & pygame.KMOD_CTRL)
            shift = bool(mods & pygame.KMOD_SHIFT)

            if idx >= 0:
                if ctrl:
                    if idx in self._selected:
                        self._selected.remove(idx)
                    else:
                        self._selected.add(idx)
                elif shift:
                    self._selected.add(idx)
                else:
                    self._selected = {idx}

                self._left_drag = True
                self._drag_walls.clear()
                for sidx in self._selected:
                    nw = self._map.walls[sidx].normalized()
                    self._drag_walls[sidx] = (nw.cx, nw.cy)
                self._drag_start_wx = wx
                self._drag_start_wy = wy
            else:
                if not ctrl and not shift:
                    self._selected.clear()
                self._box_start = event.pos
                self._box_end = event.pos
                self._box_selecting = True

        elif event.button == 3:
            self._panning = True
            self._pan_start_mouse = event.pos
            self._pan_start_camera = (self._cx, self._cy)

    def _finish_box_select(self) -> None:
        if self._map is None or self._box_start is None or self._box_end is None:
            return
        x1, y1 = self._box_start
        x2, y2 = self._box_end
        sx_min, sx_max = min(x1, x2), max(x1, x2)
        sy_min, sy_max = min(y1, y2), max(y1, y2)

        corners = [
            self._s2w(sx_min, sy_min),
            self._s2w(sx_max, sy_min),
            self._s2w(sx_max, sy_max),
            self._s2w(sx_min, sy_max),
        ]
        wx_min = min(c[0] for c in corners)
        wx_max = max(c[0] for c in corners)
        wy_min = min(c[1] for c in corners)
        wy_max = max(c[1] for c in corners)

        for i, w in enumerate(self._map.walls):
            nw = w.normalized()
            if (nw.minX >= wx_min and nw.maxX <= wx_max and
                nw.minY >= wy_min and nw.maxY <= wy_max):
                self._selected.add(i)

    # ── rendering ───────────────────────────────────────────────────────

    def _render(self) -> None:
        screen = self._screen
        screen.fill(COLOR_BG)

        if self._map is None:
            self._draw_text_centered(
                screen, "No map data", self._font_large, COLOR_TEXT_DIM
            )
            if self._error:
                self._draw_error_overlay(screen)
            return

        half = self._map.half

        if self._show_grid:
            self._draw_grid(screen, half)

        self._draw_bounds(screen, half)
        self._draw_origin(screen)

        if self._show_zone:
            self._draw_zone(screen)

        for i, wall in enumerate(self._map.walls):
            sel = i in self._selected
            self._draw_wall(screen, wall, selected=sel)

        if self._box_selecting and self._box_start and self._box_end:
            self._draw_selection_box(screen)

        self._draw_hud(screen)

        if self._show_help:
            self._draw_help_panel(screen)

        if self._show_inspect:
            self._draw_inspect_overlay(screen)

        if self._mode == "edit_wall":
            self._draw_edit_overlay(screen)
        elif self._mode == "save_as":
            self._draw_save_as_overlay(screen)

        if self._error:
            self._draw_error_overlay(screen)

    def _draw_grid(self, screen: pygame.Surface, half: float) -> None:
        w, h = self._size

        corners = [self._s2w(0, 0), self._s2w(w, 0), self._s2w(w, h), self._s2w(0, h)]
        wx_min = min(c[0] for c in corners)
        wx_max = max(c[0] for c in corners)
        wy_min = min(c[1] for c in corners)
        wy_max = max(c[1] for c in corners)

        margin = half * 0.2
        wx_min = max(-half - margin, wx_min)
        wx_max = min(half + margin, wx_max)
        wy_min = max(-half - margin, wy_min)
        wy_max = min(half + margin, wy_max)

        step = self._grid_unit

        def draw_line_pts(p1, p2, color):
            pygame.draw.line(screen, color,
                             (int(p1[0]), int(p1[1])),
                             (int(p2[0]), int(p2[1])), 1)

        i_start = int(math.ceil(wx_min / step))
        i_end = int(math.floor(wx_max / step))
        for i in range(i_start, i_end + 1):
            x = i * step
            sx, _ = self._w2s(x, 0)
            is_decade = i % 10 == 0
            color = COLOR_GRID_DECADE if is_decade else COLOR_GRID
            draw_line_pts((sx, 0), (sx, h), color)

        j_start = int(math.ceil(wy_min / step))
        j_end = int(math.floor(wy_max / step))
        for j in range(j_start, j_end + 1):
            y = j * step
            _, sy = self._w2s(0, y)
            is_decade = j % 10 == 0
            color = COLOR_GRID_DECADE if is_decade else COLOR_GRID
            draw_line_pts((0, sy), (w, sy), color)

    def _draw_bounds(self, screen: pygame.Surface, half: float) -> None:
        corners = [
            self._w2s(-half, -half),
            self._w2s(half, -half),
            self._w2s(half, half),
            self._w2s(-half, half),
        ]
        pts = [(int(x), int(y)) for x, y in corners]
        pygame.draw.lines(screen, COLOR_BOUNDS, True, pts, 2)

    def _draw_origin(self, screen: pygame.Surface) -> None:
        cx_s, cy_s = self._w2s(0, 0)
        size = 8
        pygame.draw.line(screen, COLOR_ORIGIN,
                         (cx_s - size, cy_s), (cx_s + size, cy_s), 1)
        pygame.draw.line(screen, COLOR_ORIGIN,
                         (cx_s, cy_s - size), (cx_s, cy_s + size), 1)

    def _draw_zone(self, screen: pygame.Surface) -> None:
        sx, sy = self._w2s(self._zone_cx, self._zone_cy)
        radius = int(self._zone_radius * self._zoom)
        if radius < 1:
            return
        overlay = pygame.Surface(self._size, pygame.SRCALPHA)
        pygame.draw.circle(overlay, COLOR_ZONE, (int(sx), int(sy)), radius)
        screen.blit(overlay, (0, 0))
        pygame.draw.circle(screen, COLOR_ZONE_OUTLINE,
                           (int(sx), int(sy)), radius, 2)

    def _draw_selection_box(self, screen: pygame.Surface) -> None:
        if self._box_start is None or self._box_end is None:
            return
        x1, y1 = self._box_start
        x2, y2 = self._box_end
        rx, ry = min(x1, x2), min(y1, y2)
        rw, rh = abs(x2 - x1), abs(y2 - y1)
        if rw < 1 or rh < 1:
            return
        overlay = pygame.Surface((rw, rh), pygame.SRCALPHA)
        overlay.fill(COLOR_BOX_SELECT)
        screen.blit(overlay, (rx, ry))
        pygame.draw.rect(screen, COLOR_BOX_OUTLINE, (rx, ry, rw, rh), 1)

    def _draw_wall(self, screen: pygame.Surface, wall: WallData,
                   selected: bool = False) -> None:
        nw = wall.normalized()
        p1 = self._w2s(nw.minX, nw.minY)
        p2 = self._w2s(nw.maxX, nw.maxY)
        rx = int(p1[0])
        ry = int(p1[1])
        rw = max(1, int(p2[0] - p1[0]))
        rh = max(1, int(p2[1] - p1[1]))
        rect = pygame.Rect(rx, ry, rw, rh)
        pygame.draw.rect(screen, COLOR_WALL_FILL, rect)
        outline_color = COLOR_WALL_SELECTED if selected else COLOR_WALL_OUTLINE
        outline_width = 3 if selected else 2
        pygame.draw.rect(screen, outline_color, rect, outline_width)

    def _draw_hud(self, screen: pygame.Surface) -> None:
        lines = []
        if self._map:
            lines.append(f"Map: {self._map.name}")
            lines.append(f"Walls: {len(self._map.walls)}")
            lines.append(f"Half: {self._map.half}")
        else:
            lines.append("No map loaded")

        if self._selected and self._map:
            if len(self._selected) == 1:
                idx = next(iter(self._selected))
                w = self._map.walls[idx].normalized()
                lines.append(
                    f"Selected #{idx}: "
                    f"({w.cx:.1f}, {w.cy:.1f})  {w.width:.1f}×{w.height:.1f}"
                )
            else:
                lines.append(f"Selected: {len(self._selected)} walls")
        else:
            lines.append("No selection")

        status = f"{'WATCHING' if self._watching else 'STOPPED'}"
        lines.append(f"Status: {status}")
        if self._undo.can_undo:
            lines.append("Undo available")
        if self._undo.can_redo:
            lines.append("Redo available")

        y = 10
        for line in lines:
            self._draw_text(screen, line, self._font, COLOR_TEXT, (12, y))
            y += self._font.get_height() + 2

        y += 4
        hints = [
            "WASD/Arrows: move  [];': scale  R: rotate",
            "N: new  Ctrl+D: dup  Del: delete  Ctrl+S: save",
            "Click: select  Ctrl+click: toggle  Shift+click: add",
            "L-drag: move  R-drag: pan  Wheel: zoom  Box-drag: select",
            "F: fit  G: grid  Z: zone  H: help  Shift/Alt: ×5 / ×0.1",
            "Ctrl+Z/Y: undo/redo  Ctrl+Space: inspect  DblClick: edit",
        ]
        for hint in hints:
            self._draw_text(screen, hint, self._font, COLOR_TEXT_DIM, (12, y))
            y += self._font.get_height() + 2

        zoom_text = f"Zoom: {self._zoom:.1f}x"
        tw = self._font.size(zoom_text)[0]
        self._draw_text(
            screen,
            zoom_text,
            self._font,
            COLOR_TEXT_DIM,
            (self._size[0] - tw - 12, self._size[1] - self._font.get_height() - 10),
        )

        if self._status_msg:
            stw = self._font.size(self._status_msg)[0]
            self._draw_text(
                screen,
                self._status_msg,
                self._font,
                COLOR_TEXT_SAVED,
                (self._size[0] - stw - 12, 12),
            )

    def _draw_edit_overlay(self, screen: pygame.Surface) -> None:
        w, h = self._size
        field_w = 140
        field_h = 32
        label_h = 18
        gap = 8
        total_w = field_w * 4 + gap * 3
        panel_w = total_w + 40
        panel_h = field_h + label_h + 60
        px = (w - panel_w) // 2
        py = (h - panel_h) // 2

        overlay = pygame.Surface((panel_w, panel_h), pygame.SRCALPHA)
        overlay.fill(COLOR_EDIT_BG)
        screen.blit(overlay, (px, py))
        pygame.draw.rect(screen, COLOR_TEXT_DIM, (px, py, panel_w, panel_h), 1)

        title = f"Edit Wall #{self._edit_wall_idx}"
        self._draw_text_centered(screen, title, self._font_edit, COLOR_TEXT, 0,
                                 cy=py + 14)

        start_x = px + 20
        fy = py + panel_h - field_h - 16
        for i, f in enumerate(self._edit_fields):
            fx = start_x + i * (field_w + gap)
            active = i == self._edit_focus
            bg = COLOR_EDIT_FIELD_ACTIVE if active else COLOR_EDIT_FIELD
            pygame.draw.rect(screen, bg, (fx, fy, field_w, field_h))
            pygame.draw.rect(screen, COLOR_TEXT_DIM, (fx, fy, field_w, field_h), 1)

            self._draw_text(screen, f["name"], self._font_inspect,
                            COLOR_TEXT_DIM, (fx + 4, fy - label_h + 2))
            self._draw_text(screen, f["text"] + ("|" if active else ""),
                            self._font_edit, COLOR_TEXT, (fx + 6, fy + 6))

        hint = "Tab: switch field  Enter: confirm  Esc: cancel"
        self._draw_text_centered(screen, hint, self._font_inspect, COLOR_TEXT_DIM, 0,
                                 cy=py + panel_h + 14)

    def _draw_save_as_overlay(self, screen: pygame.Surface) -> None:
        w, h = self._size
        panel_w = 500
        panel_h = 100
        px = (w - panel_w) // 2
        py = (h - panel_h) // 2

        overlay = pygame.Surface((panel_w, panel_h), pygame.SRCALPHA)
        overlay.fill(COLOR_EDIT_BG)
        screen.blit(overlay, (px, py))
        pygame.draw.rect(screen, COLOR_TEXT_DIM, (px, py, panel_w, panel_h), 1)

        self._draw_text_centered(screen, "Save As — enter file path",
                                 self._font, COLOR_TEXT, 0, cy=py + 16)
        self._draw_text_centered(screen, self._save_as_path + "|",
                                 self._font_edit, COLOR_WALL_SELECTED, 0, cy=py + 50)
        self._draw_text_centered(screen, "Enter: confirm  Esc: cancel",
                                 self._font_inspect, COLOR_TEXT_DIM, 0, cy=py + 78)

    def _draw_error_overlay(self, screen: pygame.Surface) -> None:
        overlay = pygame.Surface(self._size, pygame.SRCALPHA)
        overlay.fill(COLOR_ERROR_BG)
        screen.blit(overlay, (0, 0))

        w, h = self._size
        self._draw_text_centered(
            screen, "SYNTAX ERROR", self._font_large, COLOR_ERROR_TEXT,
            offset_y=-30,
        )
        if self._error:
            lines = self._error.split("\n")
            for i, line in enumerate(lines):
                self._draw_text_centered(
                    screen,
                    line,
                    self._font_error,
                    COLOR_ERROR_TEXT,
                    offset_y=10 + i * (FONT_SIZE_ERROR + 4),
                )

    def _draw_help_panel(self, screen: pygame.Surface) -> None:
        w, h = self._size
        pad = 20
        font = self._font_help
        line_h = font.get_height() + 2
        pw = 460
        ph = len(self._help_lines) * line_h + pad * 2
        px = (w - pw) // 2
        py = (h - ph) // 2

        overlay = pygame.Surface((pw, ph), pygame.SRCALPHA)
        overlay.fill(COLOR_HELP_BG)
        screen.blit(overlay, (px, py))
        pygame.draw.rect(screen, COLOR_TEXT_DIM, (px, py, pw, ph), 1)

        for i, line in enumerate(self._help_lines):
            is_header = line.startswith("===")
            color = COLOR_TEXT if is_header else COLOR_TEXT_DIM
            text = line.strip("= ") if is_header else line
            line_surf = font.render(text, True, color)
            lx = px + pad
            ly = py + pad + i * line_h
            screen.blit(line_surf, (lx, ly))

    def _draw_inspect_overlay(self, screen: pygame.Surface) -> None:
        w, h = self._size
        pad = 16
        line_h = self._font_inspect.get_height() + 2
        pw = 520
        ph = len(self._inspect_lines) * line_h + pad * 2 + 30
        px = (w - pw) // 2
        py = (h - ph) // 2

        overlay = pygame.Surface((pw, ph), pygame.SRCALPHA)
        overlay.fill(COLOR_INSPECT_BG)
        screen.blit(overlay, (px, py))
        pygame.draw.rect(screen, COLOR_TEXT_DIM, (px, py, pw, ph), 1)

        for i, line in enumerate(self._inspect_lines):
            is_header = line.startswith("===")
            color = COLOR_TEXT if is_header else COLOR_TEXT_DIM
            text = line.strip("= ") if is_header else line
            if not is_header:
                text = "  " + text
            line_surf = self._font_inspect.render(text, True, color)
            lx = px + pad
            ly = py + pad + 20 + i * line_h
            screen.blit(line_surf, (lx, ly))

        close_hint = "Press Ctrl+Space or Esc to close"
        ch_surf = self._font_inspect.render(close_hint, True, COLOR_TEXT_DIM)
        ch_rect = ch_surf.get_rect(center=(w // 2, py + ph - 8))
        screen.blit(ch_surf, ch_rect)

    # ── text helpers ────────────────────────────────────────────────────

    def _draw_text(
        self,
        screen: pygame.Surface,
        text: str,
        font: pygame.font.Font,
        color: tuple[int, int, int],
        pos: tuple[int, int],
    ) -> None:
        surf = font.render(text, True, color)
        screen.blit(surf, pos)

    def _draw_text_centered(
        self,
        screen: pygame.Surface,
        text: str,
        font: pygame.font.Font,
        color: tuple[int, int, int],
        offset_y: int = 0,
        cy: int | None = None,
    ) -> None:
        surf = font.render(text, True, color)
        w, h = self._size
        y_center = cy if cy is not None else h // 2 + offset_y
        rect = surf.get_rect(center=(w // 2, y_center))
        screen.blit(surf, rect)


def _fmt(f: float) -> str:
    f = round(f, 6)
    if f == int(f):
        return str(int(f))
    return f"{f}"
