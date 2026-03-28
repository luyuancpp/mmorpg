"""Migrate all .xlsx files from proto-style 4-row header to 5-row header.

Old format (rows 1–4):
    1: field declarations — proto-like (type + name combined, e.g. "uint32 id")
    2: owner   3: options   4: comment
    5+: data

New format (rows 1–5):
    1: field name only (e.g. "id")
    2: type declaration only (e.g. "uint32", "map<string,string>",
       "repeated { uint32 item; uint32 count }")
    3: owner   4: options   5: comment
    6+: data

Usage:
    python migrate_xlsx.py [--dry-run]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import openpyxl

DATA_DIR = Path(__file__).resolve().parent.parent.parent / "data"
OLD_DATA_BEGIN = 5
NEW_DATA_BEGIN = 6


def cell_str(ws, row: int, col: int) -> str:
    v = ws.cell(row=row, column=col).value
    return str(v).strip() if v is not None else ""


# ---------------------------------------------------------------------------
# Split "type name" declarations into separate name and type parts
# ---------------------------------------------------------------------------

_RE_MAP_DECL = re.compile(r'^(map\s*<\s*\w+\s*,\s*\w+\s*>)\s+(\w+)$')
_RE_SET_DECL = re.compile(r'^(set\s*<\s*\w+\s*>)\s+(\w+)$')
_RE_REPEATED_MSG_DECL = re.compile(r'^repeated\s+(\w+)\s*(\{.+\})$')
_RE_REPEATED_DECL = re.compile(r'^(repeated\s+\w+)\s+(\w+)$')
_RE_SCALAR_DECL = re.compile(r'^(\w+)\s+(\w+)$')


def split_declaration(decl: str) -> tuple[str, str]:
    """Split a combined 'type name' declaration into (name, type_only)."""
    decl = decl.strip()

    m = _RE_MAP_DECL.match(decl)
    if m:
        return m.group(2), m.group(1)

    m = _RE_SET_DECL.match(decl)
    if m:
        return m.group(2), m.group(1)

    m = _RE_REPEATED_MSG_DECL.match(decl)
    if m:
        return m.group(1), f"repeated {m.group(2)}"

    m = _RE_REPEATED_DECL.match(decl)
    if m:
        return m.group(2), m.group(1)

    m = _RE_SCALAR_DECL.match(decl)
    if m:
        return m.group(2), m.group(1)

    raise ValueError(f"Cannot split declaration: '{decl}'")


# ---------------------------------------------------------------------------
# Migration
# ---------------------------------------------------------------------------

def migrate_file(path: Path, dry_run: bool = False) -> None:
    """Migrate a single .xlsx from 4-row (combined declaration) to 5-row (name + type)."""
    wb = openpyxl.load_workbook(path)
    ws = wb[wb.sheetnames[0]]
    max_col = ws.max_column or 0

    if max_col == 0:
        print(f"  SKIP (empty): {path.name}")
        return

    # Guard: detect already-migrated files.
    # Old format R1 cell 1 = "uint32 id" (has a space).
    # New format R1 cell 1 = "id" (no space).
    first_cell = str(ws.cell(row=1, column=1).value or "").strip()
    if " " not in first_cell:
        print(f"  SKIP (already split name+type): {path.name}")
        return

    # Read old 4-row header
    # R1 = declarations (combined type+name), R2 = owner, R3 = options, R4 = comment
    old_decls = [cell_str(ws, 1, c + 1) for c in range(max_col)]
    old_owners = [cell_str(ws, 2, c + 1) for c in range(max_col)]
    old_opts = [cell_str(ws, 3, c + 1) for c in range(max_col)]
    old_comments = [cell_str(ws, 4, c + 1) for c in range(max_col)]

    # Split declarations into name + type (only on non-empty cells)
    new_names = [''] * max_col
    new_types = [''] * max_col
    for c in range(max_col):
        if old_decls[c]:
            name, type_only = split_declaration(old_decls[c])
            new_names[c] = name
            new_types[c] = type_only

    # Read data rows
    data_rows: list[list] = []
    for row in ws.iter_rows(min_row=OLD_DATA_BEGIN, max_col=max_col, values_only=True):
        data_rows.append(list(row))
    while data_rows and all(v is None for v in data_rows[-1]):
        data_rows.pop()

    if dry_run:
        non_empty = [(c, new_names[c], new_types[c]) for c in range(max_col) if new_names[c]]
        print(f"\n  {path.name} ({len(non_empty)} fields, {len(data_rows)} data rows)")
        for c, name, typ in non_empty:
            print(f"    col {c+1}: name={name}  type={typ}")
        return

    # Write new workbook with 5-row header
    sheet_title = ws.title
    new_wb = openpyxl.Workbook()
    new_ws = new_wb.active
    new_ws.title = sheet_title

    for c in range(max_col):
        new_ws.cell(row=1, column=c + 1, value=new_names[c] if new_names[c] else None)
        new_ws.cell(row=2, column=c + 1, value=new_types[c] if new_types[c] else None)
        new_ws.cell(row=3, column=c + 1, value=old_owners[c] if old_owners[c] else None)
        new_ws.cell(row=4, column=c + 1, value=old_opts[c] if old_opts[c] else None)
        new_ws.cell(row=5, column=c + 1, value=old_comments[c] if old_comments[c] else None)

    for r, row_data in enumerate(data_rows):
        for c, val in enumerate(row_data):
            if val is not None:
                new_ws.cell(row=NEW_DATA_BEGIN + r, column=c + 1, value=val)

    new_wb.save(path)
    print(f"  OK: {path.name} ({len([n for n in new_names if n])} fields, {len(data_rows)} data rows)")


def main() -> None:
    dry_run = "--dry-run" in sys.argv

    xlsx_files = sorted(DATA_DIR.glob("*.xlsx"))
    if not xlsx_files:
        print(f"No .xlsx files found in {DATA_DIR}")
        return

    mode = "DRY RUN" if dry_run else "MIGRATING"
    print(f"{mode}: {len(xlsx_files)} files in {DATA_DIR}")

    for f in xlsx_files:
        if f.name.startswith("~$"):
            continue
        try:
            migrate_file(f, dry_run=dry_run)
        except Exception as exc:
            print(f"  FAIL: {f.name}: {exc}")
            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()
