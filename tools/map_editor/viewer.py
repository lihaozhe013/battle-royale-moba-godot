import math
from pathlib import Path

import pygame

from .map_model import MapData, WallData, load_map, serialize_map

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
FONT_SIZE = 18
FONT_SIZE_LARGE = 36
FONT_SIZE_ERROR = 22
FONT_SIZE_HELP = 18


class MapViewer:
    def __init__(
        self,
        map_path: Path,
        flip_y: bool = False,
        grid_unit: float = 1.0,
        zoom: float = 0,
        width: int = 1280,
        height: int = 800,
    ):
        self._map_path = map_path
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

        self._map: MapData | None = None
        self._last_valid: MapData | None = None
        self._error: str | None = None
        self._watching = False
        self._suppress_reload = False

        self._selected: int = -1
        self._left_drag = False
        self._drag_idx: int = -1
        self._drag_anchor_cx = 0.0
        self._drag_anchor_cy = 0.0
        self._drag_start_wx = 0.0
        self._drag_start_wy = 0.0

        self._status_msg: str | None = None
        self._status_timer = 0
        self._last_key_time: dict[int, int] = {}

        self._screen: pygame.Surface | None = None
        self._clock: pygame.time.Clock | None = None
        self._running = True
        self._font: pygame.font.Font | None = None
        self._font_large: pygame.font.Font | None = None
        self._font_error: pygame.font.Font | None = None

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

    def _load_map(self) -> None:
        try:
            old_count = len(self._map.walls) if self._map else 0
            self._map = load_map(self._map_path)
            self._last_valid = self._map
            self._error = None
            if self._zoom == 0:
                self._fit_map()
            if self._selected >= len(self._map.walls):
                self._selected = -1
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
        if self._selected < 0 or self._map is None:
            return
        w = self._map.walls[self._selected]
        w.minX += dx
        w.maxX += dx
        w.minY += dy
        w.maxY += dy

    def _scale_selected(self, dx: float, dy: float) -> None:
        if self._selected < 0 or self._map is None:
            return
        w = self._map.walls[self._selected]
        nw = w.normalized()
        cx, cy = nw.cx, nw.cy
        hw = max(0.05, nw.width * 0.5 + dx * 0.5)
        hh = max(0.05, nw.height * 0.5 + dy * 0.5)
        w.minX = cx - hw
        w.maxX = cx + hw
        w.minY = cy - hh
        w.maxY = cy + hh

    def _save_map(self) -> None:
        if self._map is None:
            return
        text = serialize_map(self._map)
        self._suppress_reload = True
        self._map_path.write_text(text, encoding="utf-8")
        self._status_msg = "Saved!"
        self._status_timer = 90

    def _handle_events(self) -> None:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self._running = False

            elif event.type == pygame.KEYDOWN:
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
                    else:
                        self._running = False

                elif event.key == pygame.K_f:
                    if not is_repeat:
                        self._fit_map()
                elif event.key == pygame.K_g:
                    if not is_repeat:
                        self._show_grid = not self._show_grid
                elif event.key == pygame.K_h:
                    if not is_repeat:
                        self._show_help = not self._show_help
                        if self._show_help:
                            self._load_help_text()

                elif event.key == pygame.K_n and not is_repeat:
                    self._handle_new_wall()
                elif event.key == pygame.K_d and ctrl and not is_repeat:
                    self._handle_duplicate_wall()
                elif event.key in (pygame.K_DELETE, pygame.K_BACKSPACE) and not is_repeat:
                    self._handle_delete_wall()
                elif event.key == pygame.K_r and not is_repeat:
                    self._handle_rotate_wall()
                elif event.key == pygame.K_s and ctrl and not is_repeat:
                    self._save_map()

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

            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    wx, wy = self._s2w(event.pos[0], event.pos[1])
                    idx = self._hit_test_wall(wx, wy)
                    if idx >= 0:
                        self._selected = idx
                        self._left_drag = True
                        self._drag_idx = idx
                        w = self._map.walls[idx].normalized()
                        self._drag_anchor_cx = w.cx
                        self._drag_anchor_cy = w.cy
                        self._drag_start_wx = wx
                        self._drag_start_wy = wy
                    else:
                        self._selected = -1
                elif event.button == 3:
                    self._panning = True
                    self._pan_start_mouse = event.pos
                    self._pan_start_camera = (self._cx, self._cy)

            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:
                    self._left_drag = False
                    self._drag_idx = -1
                elif event.button == 3:
                    self._panning = False

            elif event.type == pygame.MOUSEMOTION:
                if self._left_drag and self._drag_idx >= 0 and self._map:
                    wx, wy = self._s2w(event.pos[0], event.pos[1])
                    dwx = wx - self._drag_start_wx
                    dwy = wy - self._drag_start_wy
                    w = self._map.walls[self._drag_idx]
                    nw = w.normalized()
                    hw = nw.width * 0.5
                    hh = nw.height * 0.5
                    new_cx = self._drag_anchor_cx + dwx
                    new_cy = self._drag_anchor_cy + dwy
                    w.minX = new_cx - hw
                    w.maxX = new_cx + hw
                    w.minY = new_cy - hh
                    w.maxY = new_cy + hh
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
                factor = 1.15 if event.y > 0 else 0.85
                self._zoom *= factor
                self._zoom = max(0.05, min(self._zoom, 2000.0))

            elif event.type == pygame.VIDEORESIZE:
                self._size = (event.w, event.h)
                self._screen = pygame.display.set_mode(self._size, pygame.RESIZABLE)

    def _handle_new_wall(self) -> None:
        if self._map is None:
            return
        w = WallData(
            minX=self._cx - 0.5, minY=self._cy - 0.5,
            maxX=self._cx + 0.5, maxY=self._cy + 0.5,
        )
        self._map.walls.append(w)
        self._selected = len(self._map.walls) - 1
        self._status_msg = "New wall"
        self._status_timer = 60

    def _handle_duplicate_wall(self) -> None:
        if self._selected < 0 or self._map is None:
            return
        src = self._map.walls[self._selected]
        nw = src.normalized()
        new_w = WallData(
            minX=nw.minX + 1, minY=nw.minY + 1,
            maxX=nw.maxX + 1, maxY=nw.maxY + 1,
        )
        self._map.walls.append(new_w)
        self._selected = len(self._map.walls) - 1
        self._status_msg = "Duplicated"
        self._status_timer = 60

    def _handle_delete_wall(self) -> None:
        if self._selected < 0 or self._map is None:
            return
        del self._map.walls[self._selected]
        self._selected = -1
        self._status_msg = "Deleted"
        self._status_timer = 60

    def _handle_rotate_wall(self) -> None:
        if self._selected < 0 or self._map is None:
            return
        w = self._map.walls[self._selected]
        nw = w.normalized()
        w_half = nw.width * 0.5
        h_half = nw.height * 0.5
        w.minX = nw.cx - h_half
        w.maxX = nw.cx + h_half
        w.minY = nw.cy - w_half
        w.maxY = nw.cy + w_half

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

        for i, wall in enumerate(self._map.walls):
            self._draw_wall(screen, wall, selected=(i == self._selected))

        self._draw_hud(screen)

        if self._show_help:
            self._draw_help_panel(screen)

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

        if self._selected >= 0 and self._map:
            w = self._map.walls[self._selected].normalized()
            lines.append(
                f"Selected #{self._selected}: "
                f"({w.cx:.1f}, {w.cy:.1f})  {w.width:.1f}×{w.height:.1f}"
            )
        else:
            lines.append("No selection")

        status = f"{'WATCHING' if self._watching else 'STOPPED'}"
        lines.append(f"Status: {status}")

        y = 10
        for line in lines:
            self._draw_text(screen, line, self._font, COLOR_TEXT, (12, y))
            y += self._font.get_height() + 2

        y += 4
        hints = [
            "WASD/Arrows: move  [];': scale  R: rotate",
            "N: new  D: dup  Del: delete  Ctrl+S: save",
            "Click: select  L-drag: move  R-drag: pan  Wheel: zoom",
            "F: fit  G: grid  Shift/Alt: ×5 / ×0.1",
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
    ) -> None:
        surf = font.render(text, True, color)
        w, h = self._size
        rect = surf.get_rect(center=(w // 2, h // 2 + offset_y))
        screen.blit(surf, rect)
