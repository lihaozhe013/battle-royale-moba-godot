import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import List


@dataclass
class WallData:
    minX: float
    minY: float
    maxX: float
    maxY: float

    @property
    def width(self) -> float:
        return abs(self.maxX - self.minX)

    @property
    def height(self) -> float:
        return abs(self.maxY - self.minY)

    @property
    def cx(self) -> float:
        return (self.minX + self.maxX) * 0.5

    @property
    def cy(self) -> float:
        return (self.minY + self.maxY) * 0.5

    def normalized(self) -> "WallData":
        return WallData(
            minX=min(self.minX, self.maxX),
            minY=min(self.minY, self.maxY),
            maxX=max(self.minX, self.maxX),
            maxY=max(self.minY, self.maxY),
        )


@dataclass
class MapData:
    name: str
    half: float
    walls: List[WallData]


def load_map(path: Path) -> MapData:
    text = path.read_text(encoding="utf-8")
    return parse_map_json(text)


def parse_map_json(text: str) -> MapData:
    data = json.loads(text)
    name = data.get("name", "unnamed")
    bounds = data.get("bounds", {})
    half = float(bounds.get("half", 50.0))
    walls = []
    for w in data.get("walls", []):
        walls.append(
            WallData(
                minX=float(w["minX"]),
                minY=float(w["minY"]),
                maxX=float(w["maxX"]),
                maxY=float(w["maxY"]),
            )
        )
    return MapData(name=name, half=half, walls=walls)
