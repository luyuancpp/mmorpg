"""Excel reader — parse ``.xlsx`` metadata into :class:`TableSchema` objects.

Also provides data-access helpers so that generators never need to
re-open or re-parse workbooks themselves.

Excel header format (6 rows):
    Row 1: field names
    Row 2: proto types
    Row 3: struct — repeated | set | map_key | map_value | message:Name
    Row 4: owner
    Row 5: options (space-separated tokens)
    Row 6: comment
    Row 7+: data
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

import openpyxl
from openpyxl.worksheet.worksheet import Worksheet

from core.config_loader import ExporterConfig
from core.file_utils import list_xlsx
from core.schema import (
    ArrayField,
    ColumnDef,
    GroupField,
    MapField,
    TableSchema,
)

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Public API — schema reading
# ---------------------------------------------------------------------------

def read_table(file_path: Path, cfg: ExporterConfig) -> Optional[TableSchema]:
    """Read a single ``.xlsx`` and return its :class:`TableSchema`."""
    try:
        wb = openpyxl.load_workbook(file_path)
    except Exception as exc:
        logger.error("Cannot open %s: %s", file_path, exc)
        return None

    if not wb.sheetnames:
        logger.error("No sheets in %s", file_path)
        return None

    sheet_name = wb.sheetnames[0]
    ws = wb[sheet_name]

    if ws.cell(row=1, column=1).value != "id":
        logger.error("First column of '%s' must be 'id' (%s)", sheet_name, file_path)
        return None

    columns = _parse_columns(ws, cfg)
    arrays, groups, maps = _detect_layout(columns)

    first_col = columns[0] if columns else None
    use_flat_multimap = first_col is not None and first_col.is_multi_key

    constants_idx = next(
        (c.excel_index for c in columns if c.name == "constants_name"), None
    )

    return TableSchema(
        name=sheet_name,
        source_path=file_path,
        columns=columns,
        arrays=arrays,
        groups=groups,
        maps=maps,
        use_flat_multimap=use_flat_multimap,
        has_constants_name=constants_idx is not None,
        constants_name_index=constants_idx,
    )


def read_all_tables(cfg: ExporterConfig) -> list[TableSchema]:
    """Read every ``.xlsx`` in *cfg.data_dir* and return a list of schemas."""
    tables: list[TableSchema] = []
    for xlsx in list_xlsx(cfg.data_dir):
        schema = read_table(xlsx, cfg)
        if schema is not None:
            tables.append(schema)
    return tables


# ---------------------------------------------------------------------------
# Public API — data access helpers (for generators)
# ---------------------------------------------------------------------------

def open_worksheet(schema: TableSchema) -> Worksheet:
    """Open the source workbook and return the first worksheet."""
    wb = openpyxl.load_workbook(schema.source_path)
    return wb[schema.name]


def read_id_column(schema: TableSchema, cfg: ExporterConfig) -> list[int]:
    """Read all ``id`` values (first column) from data rows."""
    ws = open_worksheet(schema)
    return [
        int(row[0])
        for row in ws.iter_rows(min_row=cfg.data_begin_row, values_only=True)
        if row[0] is not None
    ]


def read_data_rows(schema: TableSchema, cfg: ExporterConfig) -> list[dict]:
    """Read data rows and return them as dicts with structural awareness.

    Arrays become lists, groups become nested dicts, maps become dicts,
    sets become ``{value: True}`` dicts.
    """
    ws = open_worksheet(schema)
    server_names = {c.name for c in schema.columns if c.is_server}
    col_names = [c.name for c in schema.columns]
    col_types = {c.name: c.data_type for c in schema.columns}
    col_to_group = schema.col_to_group

    rows: list[dict] = []
    for excel_row in ws.iter_rows(min_row=cfg.data_begin_row, values_only=False):
        row_data = _build_row(excel_row, schema, col_names, col_types,
                              server_names, col_to_group)
        if row_data:
            rows.append(row_data)
    return rows


# ---------------------------------------------------------------------------
# Internal — column parsing
# ---------------------------------------------------------------------------

def _cell_str(ws, row: int, col: int) -> str:
    v = ws.cell(row=row, column=col).value
    return str(v).strip() if v is not None else ""


def _parse_columns(ws, cfg: ExporterConfig) -> list[ColumnDef]:
    meta = cfg.metadata_rows
    max_col = ws.max_column or 0
    columns: list[ColumnDef] = []

    for idx in range(max_col):
        col_1based = idx + 1
        name = _cell_str(ws, meta.get("column_name", 1), col_1based)
        if not name:
            continue

        data_type = _cell_str(ws, meta.get("data_type", 2), col_1based) or "int32"
        struct = _cell_str(ws, meta.get("struct", 3), col_1based).lower()
        owner = _cell_str(ws, meta.get("owner", 4), col_1based).lower()
        options_raw = _cell_str(ws, meta.get("options", 5), col_1based)
        comment = _cell_str(ws, meta.get("comment", 6), col_1based)

        options = options_raw.split() if options_raw else []

        columns.append(ColumnDef(
            name=name,
            data_type=data_type,
            owner=owner,
            struct=struct,
            options=options,
            comment=comment,
            excel_index=idx,
        ))

    return columns


# ---------------------------------------------------------------------------
# Internal — layout detection (driven by struct row)
# ---------------------------------------------------------------------------

def _detect_layout(
    columns: list[ColumnDef],
) -> tuple[dict[str, ArrayField], dict[str, GroupField], dict[str, MapField]]:
    """Detect arrays, message groups, and maps from the struct row."""

    # --- repeated fields ---
    arrays: dict[str, ArrayField] = {}
    repeated_cols: dict[str, list[ColumnDef]] = {}
    for col in columns:
        if col.struct == "repeated":
            repeated_cols.setdefault(col.name, []).append(col)
    for name, cols in repeated_cols.items():
        arrays[name] = ArrayField(
            name=name,
            data_type=cols[0].data_type,
            indices=[c.excel_index for c in cols],
        )

    # --- message groups ---
    groups: dict[str, GroupField] = {}
    for col in columns:
        if col.struct.startswith("message:"):
            msg_name = col.struct.split(":", 1)[1]
            if msg_name not in groups:
                groups[msg_name] = GroupField(name=msg_name, columns=[], indices=[])
            # Only add unique column names to columns (for sub-message definition)
            existing = {c.name for c in groups[msg_name].columns}
            if col.name not in existing:
                groups[msg_name].columns.append(col)
            groups[msg_name].indices.append(col.excel_index)

    # --- maps (consecutive map_key + map_value pairs) ---
    maps: dict[str, MapField] = {}
    i = 0
    while i < len(columns):
        if columns[i].struct == "map_key" and i + 1 < len(columns) and columns[i + 1].struct == "map_value":
            prefix = _common_prefix(columns[i].name, columns[i + 1].name)
            if prefix and prefix not in maps:
                maps[prefix] = MapField(
                    name=prefix,
                    key_type=columns[i].data_type,
                    value_type=columns[i + 1].data_type,
                )
            i += 2
        else:
            i += 1

    return arrays, groups, maps


def _common_prefix(a: str, b: str) -> str:
    """Find common underscore-delimited prefix."""
    parts_a, parts_b = a.split("_"), b.split("_")
    common = []
    for pa, pb in zip(parts_a, parts_b):
        if pa == pb:
            common.append(pa)
        else:
            break
    return "_".join(common)


# ---------------------------------------------------------------------------
# Internal — data row building
# ---------------------------------------------------------------------------

def _build_row(
    excel_row,
    schema: TableSchema,
    col_names: list[str],
    col_types: dict[str, str],
    server_names: set[str],
    col_to_group: dict[int, str],
) -> dict:
    """Convert one Excel data row into a structured dict."""
    row_data: dict = {}
    pending_map_key = None

    for idx, cell in enumerate(excel_row):
        if idx >= len(col_names):
            break
        name = col_names[idx]
        if not name or name not in server_names:
            continue

        value = _convert_cell(cell.value, col_types.get(name, ""))
        if value is None:
            pending_map_key = None
            continue

        col = schema.columns[idx]

        if col.map_role == "set":
            row_data.setdefault(name, {})[value] = True
        elif col.map_role == "map_key":
            pending_map_key = value
        elif col.map_role == "map_value" and pending_map_key is not None:
            prefix = _common_prefix(col_names[idx - 1], name) if idx > 0 else name
            row_data.setdefault(prefix, {})[pending_map_key] = value
            pending_map_key = None
        elif name in schema.arrays:
            if value not in (0, -1, ""):
                row_data.setdefault(name, []).append(value)
        elif idx in col_to_group:
            group_name = col_to_group[idx]
            _append_to_group(row_data, group_name, name, value)
        else:
            row_data[name] = value

    return row_data


def _append_to_group(row_data: dict, group_name: str, col_name: str, value) -> None:
    """Append a value into the correct group sub-list."""
    member = {col_name: value}
    if group_name in row_data:
        last = row_data[group_name][-1]
        if col_name in last:
            row_data[group_name].append(member)
        else:
            last[col_name] = value
    else:
        row_data[group_name] = [member]


def _convert_cell(value, data_type: str):
    """Convert an Excel cell value to the appropriate Python type."""
    if value is None or value == "":
        return None
    if data_type == "string":
        return str(value)
    if data_type in ("float", "double"):
        try:
            return float(value)
        except (ValueError, TypeError):
            return None
    if isinstance(value, float) and value.is_integer():
        return int(value)
    return value
