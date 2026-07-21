#!/usr/bin/env python3
"""
Format table blocks between // clang-format off / @table / // clang-format on.

Aligns braced init list entries into fixed-width columns with right-aligned
numbers and left-aligned text values.

Usage:
    python scripts/format_table.py                        # format all sim files
    python scripts/format_table.py src_cpp/sim/skill_defs.h   # specific file
    python scripts/format_table.py --check                # check-only (CI mode)
"""

import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SIM_DIR = PROJECT_ROOT / "src_cpp" / "sim"

TABLE_MARKER = "// @table"
CLANG_OFF = "// clang-format off"
CLANG_ON = "// clang-format on"

_NUM_RE = re.compile(r'^-?\d+(?:\.\d+)?[fF]?$')


def is_numeric(s: str) -> bool:
    """Check if a value is a numeric literal (int, float, with optional f/F suffix)."""
    return bool(_NUM_RE.match(s.strip()))


def find_cpp_files(root: Path) -> list[Path]:
    return sorted(p for p in root.rglob("*") if p.suffix in {".h", ".cpp"} and p.is_file())


def parse_entries(lines: list[str], start: int, end: int) -> list[tuple[list[str], str]]:
    """Parse {v1, v2, ...}, // comment entries from a range of lines."""
    entries = []
    i = start
    while i < end:
        ls = lines[i].strip()
        if not ls or not ls.startswith("{") or ls.startswith("};"):
            i += 1
            continue

        merged = ls
        i += 1
        while i < end and "}," not in merged:
            merged += " " + lines[i].strip()
            i += 1

        m = re.match(r"\{(.*)\},\s*(.*)$", merged)
        if not m:
            continue
        values = [v.strip() for v in m.group(1).split(",")]
        comment = m.group(2).strip()
        entries.append((values, comment))
    return entries


def format_entries(entries: list[tuple[list[str], str]], indent: str) -> list[str]:
    """Reformat entries as aligned single-line rows.

    Padding for alignment is placed *after* the ", " separator to avoid
    trailing spaces before commas in left-aligned text columns.
    """
    if not entries:
        return []

    n_cols = max(len(v) for v, _ in entries)

    widths = [0] * n_cols
    align_right = [True] * n_cols

    for values, _ in entries:
        for i, v in enumerate(values):
            widths[i] = max(widths[i], len(v))
            if not is_numeric(v):
                align_right[i] = False

    # Column start positions (character index after '{')
    col_start = [1]
    for i in range(1, n_cols):
        col_start.append(col_start[i - 1] + widths[i - 1] + 2)  # +2 for ", "

    lines_out = []
    for values, comment in entries:
        line = "{"
        for i in range(n_cols):
            v = values[i] if i < len(values) else ""

            cur = len(line)
            if cur < col_start[i]:
                line += " " * (col_start[i] - cur)

            if align_right[i]:
                line += " " * (widths[i] - len(v)) + v
            else:
                line += v

            if i < n_cols - 1:
                line += ","

        line += "},"
        if comment:
            line += "  " + comment
        lines_out.append(indent + line)
    return lines_out


def process_table_block(lines: list[str], start: int, end: int, indent: str) -> list[str] | None:
    """Reformat a @table block; return new lines or None if unchanged."""
    # Identify header lines (ending with {) and footer lines (starting with };)
    header_end = start
    for i in range(start, end):
        ls = lines[i].strip()
        if not ls:
            header_end = i + 1
            continue
        if ls.endswith("{") and not ls.startswith("{"):
            header_end = i + 1
            break
        break

    footer_start = end
    for i in range(end - 1, start - 1, -1):
        ls = lines[i].strip()
        if not ls:
            footer_start = i
            continue
        if ls.startswith("};"):
            footer_start = i
            break
        break

    entries = parse_entries(lines, header_end, footer_start)
    if not entries:
        return None

    # Detect entry indent from existing lines
    entry_indent = indent + "    "
    for i in range(header_end, footer_start):
        ls = lines[i]
        if ls.strip():
            m = re.match(r"^(\s+)", ls)
            if m:
                entry_indent = m.group(1)
            break

    formatted = format_entries(entries, entry_indent)

    new_block = list(lines[start:header_end])  # header
    new_block.extend(formatted)               # entries
    new_block.extend(lines[footer_start:end])  # footer
    new_block = [ln.rstrip() for ln in new_block]
    return new_block


def process_file(file_path: Path, check_only: bool = False) -> bool:
    """Process one file. Returns True if modified (or would be modified in check mode)."""
    original = file_path.read_text(encoding='utf-8')
    lines = original.split("\n")
    new_lines = list(lines)
    modified = False

    i = 0
    while i < len(lines):
        if lines[i].strip() == CLANG_OFF and i + 1 < len(lines) and lines[i + 1].strip() == TABLE_MARKER:
            # Find closing clang-format on
            on_idx = None
            for j in range(i + 2, len(lines)):
                if lines[j].strip() == CLANG_ON:
                    on_idx = j
                    break
            if on_idx is None:
                i += 1
                continue

            indent = re.match(r"^(\s*)", lines[i]).group(1)
            new_block = process_table_block(lines, i + 2, on_idx, indent)
            if new_block is not None:
                old_block = lines[i + 2 : on_idx]
                if new_block != old_block:
                    if check_only:
                        rel = file_path.relative_to(PROJECT_ROOT)
                        print(f"  {rel} would be reformatted")
                        return True
                    new_lines[i + 2 : on_idx] = new_block
                    modified = True

            i = on_idx + 1
        else:
            i += 1

    if modified:
        result = "\n".join(new_lines)
        if not result.endswith("\n"):
            result += "\n"
        file_path.write_text(result, encoding='utf-8')
        return True
    return False


def main() -> int:
    args = sys.argv[1:]
    check_only = "--check" in args
    files = [Path(a).resolve() for a in args if a != "--check" and not a.startswith("-")]

    if not files:
        files = find_cpp_files(SIM_DIR)
        if not files:
            print(f"No C++ files found under {SIM_DIR}")
            return 0

    any_modified = False
    for fp in files:
        if process_file(fp, check_only=check_only):
            if not check_only:
                rel = fp.relative_to(PROJECT_ROOT)
                print(f"  formatted {rel}")
            any_modified = True

    if check_only:
        if any_modified:
            print("Table format check FAILED.")
            return 1
        print("All tables already formatted.")
        return 0
    if any_modified:
        print("Done.")
    else:
        print("All tables already formatted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
