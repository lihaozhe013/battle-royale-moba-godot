import math
import sys
from pathlib import Path

import pygame

from .map_model import MapData, WallData, load_map

COLOR_BG = (26, 26, 31)
COLOR_GRID = (42, 42, 47)
COLOR_GRID_DECADE = (58, 58, 63)
COLOR_BOUNDS = (80, 80, 90)
COLOR_WALL_FILL = (102, 102, 115)
COLOR_WALL_OUTLINE = (153, 153, 163)
COLOR_TEXT = (204, 204, 204)
COLOR_TEXT_DIM = (102, 102, 115)
COLOR_HUD_BG = (10, 10, 14, 180)
COLOR_ERROR_BG = (200, 0, 0, 100)
COLOR_ERROR_TEXT = (255, 80, 80)
COLOR_ORIGIN = (100, 100, 100)
COLOR_STATUS_OK = (80, 200, 80)
COLOR_STATUS_ERR = (200, 80, 80)
FONT_SIZE = 16
FONT_SIZE_LARGE = 36
FONT_SIZE_ERROR = 22


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

        self._map: MapData | None = None
        self._last_valid: MapData | None = None
        self._error: str | None = None
        self._watching = False

        self._screen: pygame.Surface | None = None
        self._clock: pygame.time.Clock | None = None
        self._running = True
        self._font: pygame.font.Font | None = None
        self._font_large: pygame.font.Font | None = None
        self._font_error: pygame.font.Font | None = None

        self._init_pygame()
        self._load_map()

    def _init_pygame(self) -> None:
        pygame.init()
        flags = pygame.RESIZABLE
        self._screen = pygame.display.set_mode(self._size, flags)
        pygame.display.set_caption(f"Map Editor — {self._map_path}")
        self._clock = pygame.time.Clock()
        self._font = pygame.font.Font(None, FONT_SIZE)
        self._font_large = pygame.font.Font(None, FONT_SIZE_LARGE)
        self._font_error = pygame.font.Font(None, FONT_SIZE_ERROR)

    def _load_map(self) -> None:
        try:
            self._map = load_map(self._map_path)
            self._last_valid = self._map
            self._error = None
            if self._zoom == 0:
                self._fit_map()
        except Exception as e:
            self._error = str(e)
            if self._last_valid is not None:
                self._map = self._last_valid
            else:
                self._map = None

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
        self._load_map()

    @property
    def running(self) -> bool:
        return self._running

    def tick(self) -> None:
        self._clock.tick(60)
        self._handle_events()
        self._render()
        pygame.display.flip()

    def quit(self) -> None:
        self._running = False
        pygame.quit()

    # ── events ──────────────────────────────────────────────────────────

    def _handle_events(self) -> None:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self._running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    self._running = False
                elif event.key == pygame.K_f:
                    self._fit_map()
                elif event.key == pygame.K_g:
                    self._show_grid = not self._show_grid

            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 3:
                    self._panning = True
                    self._pan_start_mouse = event.pos
                    self._pan_start_camera = (self._cx, self._cy)

            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 3:
                    self._panning = False

            elif event.type == pygame.MOUSEMOTION:
                if self._panning and self._pan_start_mouse is not None:
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

        for wall in self._map.walls:
            self._draw_wall(screen, wall)

        self._draw_hud(screen)

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

        # vertical lines
        i_start = int(math.ceil(wx_min / step))
        i_end = int(math.floor(wx_max / step))
        for i in range(i_start, i_end + 1):
            x = i * step
            sx, _ = self._w2s(x, 0)
            is_decade = i % 10 == 0
            color = COLOR_GRID_DECADE if is_decade else COLOR_GRID
            draw_line_pts((sx, 0), (sx, h), color)

        # horizontal lines
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

    def _draw_wall(self, screen: pygame.Surface, wall: WallData) -> None:
        nw = wall.normalized()
        p1 = self._w2s(nw.minX, nw.minY)
        p2 = self._w2s(nw.maxX, nw.maxY)
        rx = int(p1[0])
        ry = int(p1[1])
        rw = max(1, int(p2[0] - p1[0]))
        rh = max(1, int(p2[1] - p1[1]))
        rect = pygame.Rect(rx, ry, rw, rh)
        pygame.draw.rect(screen, COLOR_WALL_FILL, rect)
        pygame.draw.rect(screen, COLOR_WALL_OUTLINE, rect, 2)

    def _draw_hud(self, screen: pygame.Surface) -> None:
        lines = []
        if self._map:
            lines.append(f"Map: {self._map.name}")
            lines.append(f"Walls: {len(self._map.walls)}")
            lines.append(f"Half: {self._map.half}")

        status = f"{'WATCHING' if self._watching else 'STOPPED'}"
        lines.append(f"Status: {status}")

        y = 10
        for line in lines:
            self._draw_text(screen, line, self._font, COLOR_TEXT, (12, y))
            y += self._font.get_height() + 2

        zoom_text = f"Zoom: {self._zoom:.1f}x  [F] Fit  [G] Grid  ESC: Quit"
        tw = self._font.size(zoom_text)[0]
        self._draw_text(
            screen,
            zoom_text,
            self._font,
            COLOR_TEXT_DIM,
            (self._size[0] - tw - 12, self._size[1] - self._font.get_height() - 10),
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
