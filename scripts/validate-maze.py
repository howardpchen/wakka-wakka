#!/usr/bin/env python3
"""Validate the embedded maze's corridor and energy-dot topology."""

from pathlib import Path
import re

source = Path(__file__).parents[1] / "src/c/wakka-wakka.c"
text = source.read_text(encoding="utf-8")
block = re.search(
    r"static const char \*const MAP\[MAP_H\] = \{(.*?)\n\};",
    text,
    re.DOTALL,
)
if not block:
    raise SystemExit("FAIL: MAP initializer not found")

rows = re.findall(r'"([^"]+)"', block.group(1))
if len(rows) != 21 or any(len(row) != 19 for row in rows):
    raise SystemExit("FAIL: maze must be exactly 19x21")

open_tiles = {
    (x, y)
    for y, row in enumerate(rows)
    for x, tile in enumerate(row)
    if tile not in "#X"
}


def neighbors(point):
    x, y = point
    result = []
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        candidate = (x + dx, y + dy)
        if y == 10 and dy == 0:
            candidate = ((x + dx) % 19, y)
        if candidate in open_tiles:
            result.append(candidate)
    return result


seen = set()
pending = [next(iter(open_tiles))]
while pending:
    point = pending.pop()
    if point in seen:
        continue
    seen.add(point)
    pending.extend(neighbors(point))

dead_ends = [point for point in open_tiles if len(neighbors(point)) < 2]
open_blocks = [
    (x, y)
    for y in range(20)
    for x in range(18)
    if all((x + dx, y + dy) in open_tiles for dx in (0, 1) for dy in (0, 1))
]
pellet_blocks = [
    (x, y)
    for y in range(20)
    for x in range(18)
    if all(rows[y + dy][x + dx] in ".o" for dx in (0, 1) for dy in (0, 1))
]
wall_blocks = [
    (x, y)
    for y in range(20)
    for x in range(18)
    if all(rows[y + dy][x + dx] == "#" for dx in (0, 1) for dy in (0, 1))
]

errors = []
if len(seen) != len(open_tiles):
    errors.append(f"{len(open_tiles) - len(seen)} open tiles are disconnected")
if dead_ends:
    errors.append(f"dead ends at {dead_ends}")
if open_blocks:
    errors.append(f"2x2 open corridor blocks at {open_blocks}")
if pellet_blocks:
    errors.append(f"2x2 energy-dot blocks at {pellet_blocks}")
if wall_blocks:
    errors.append(f"2x2 solid wall blocks at {wall_blocks}")
if rows[10][0] == "#" or rows[10][18] == "#":
    errors.append("center wrap tunnel endpoints are closed")

expected_enclosure = [
    "########",
    "#XXXXXX#",
    "#XXXXXX#",
    "########",
]
actual_enclosure = [row[6:14] for row in rows[8:12]]
if actual_enclosure != expected_enclosure:
    errors.append(
        f"center enclosure is not the expected hollow 8x4 rectangle: "
        f"{actual_enclosure}"
    )

actor_starts = [(9, 17), (7, 7), (9, 7), (11, 7), (13, 7)]
blocked_starts = [point for point in actor_starts if point not in open_tiles]
if blocked_starts:
    errors.append(f"blocked actor starts at {blocked_starts}")

if errors:
    raise SystemExit("FAIL: " + "; ".join(errors))

energy_dots = sum(tile in ".o" for row in rows for tile in row)
print(
    f"PASS: 19x21, {len(open_tiles)} connected open tiles, "
    f"{energy_dots} energy dots, no dead ends, 2x2 corridors, or 2x2 solid walls, "
    f"center wrap open, hollow 8x4 enclosure present"
)
