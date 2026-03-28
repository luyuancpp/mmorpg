"""Excel reader — parse ``.xlsx`` metadata into :class:`TableSchema` objects.

Also provides data-access helpers so that generators never need to
re-open or re-parse workbooks themselves.
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
    ForeignKeyRef,
    GroupForeignKeyRef,
    GroupField,
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
    arrays, groups = _detect_layout(columns)

    multi_cell = ws.cell(row=5, column=1).value
    use_flat_multimap = isinstance(multi_cell, str) and multi_cell.strip().lower() == "multi"

    constants_idx = next(
        (c.excel_index for c in columns if c.name == "constants_name"), None
    )

    return TableSchema(
        name=sheet_name,
        source_path=file_path,
        columns=columns,
        arrays=arrays,
        groups=groups,
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
    grouped_names = _grouped_column_names(schema)

    rows: list[dict] = []
    for excel_row in ws.iter_rows(min_row=cfg.data_begin_row, values_only=False):
        row_data = _build_row(excel_row, schema, col_names, col_types,
                              server_names, grouped_names)
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
        owner = _cell_str(ws, meta.get("owner", 4), col_1based).lower()
        map_role = _cell_str(ws, meta.get("map_type", 3), col_1based).lower()
        multi_val = _cell_str(ws, meta.get("multi_key", 5), col_1based).lower()
        tkey_val = _cell_str(ws, meta.get("table_key", 6), col_1based).lower()
        expr_type = _cell_str(ws, meta.get("expression_type", 7), col_1based)
        expr_params_raw = _cell_str(ws, meta.get("expression_params", 8), col_1based)
        fk_raw = _cell_str(ws, meta.get("foreign_key", 9), col_1based)
        gfk_raw = _cell_str(ws, meta.get("group_foreign_key", 10), col_1based)
        bit_val = _cell_str(ws, meta.get("bit_index_marker_row", 6), col_1based).lower()

        expr_params = (
            [p.strip() for p in expr_params_raw.split(",") if p.strip()]
            if expr_params_raw else []
        )

        columns.append(ColumnDef(
            name=name,
            data_type=data_type,
            owner=owner,
            map_role=map_role,
            is_table_key=(tkey_val == "table_key"),
            is_multi_key=(multi_val == "multi"),
            expression_type=expr_type,
            expression_params=expr_params,
            has_bit_index=(bit_val == "bit_index"),
            foreign_key=ForeignKeyRef.parse(fk_raw),
            group_foreign_key=GroupForeignKeyRef.parse(gfk_raw),
            excel_index=idx,
        ))

    return columns


# ---------------------------------------------------------------------------
# Internal — layout detection
# ---------------------------------------------------------------------------

def _detect_layout(
    columns: list[ColumnDef],
) -> tuple[dict[str, ArrayField], dict[str, GroupField]]:
    """Detect array columns (consecutive same-name) and group columns
    (non-consecutive same-name pattern)."""
    names = [c.name for c in columns]
    n = len(names)

    # --- arrays: consecutive runs of the same name ---
    arrays: dict[str, ArrayField] = {}
    i = 0
    while i < n:
        j = i + 1
        while j < n and names[j] == names[i]:
            j += 1
        if j - i > 1:
            arrays[names[i]] = ArrayField(
                name=names[i],
                data_type=columns[i].data_type,
                indices=list(range(i, j)),
            )
        i = j

    # --- groups: same name appears non-consecutively ---
    groups: dict[str, GroupField] = {}
    grouped: set[str] = set()
    seen: dict[str, int] = {}
    for i, name in enumerate(names):
        if name in arrays or name in grouped:
            continue
        if name in seen:
            first = seen[name]
            indices = list(range(first, i + 1))
            prefix = _common_prefix(names[first], names[first + 1] if first + 1 < n else "")
            gname = prefix or name
            groups[gname] = GroupField(
                name=gname,
                columns=[columns[gi] for gi in indices],
                indices=indices,
            )
            grouped.update(names[gi] for gi in indices)
        else:
            seen[name] = i

    # Auto-assign map_value role to second column of map groups.
    for gf in groups.values():
        if len(gf.indices) >= 2:
            first_col = columns[gf.indices[0]]
            second_col = columns[gf.indices[1]]
            if first_col.map_role == "map_key" and not second_col.map_role:
                second_col.map_role = "map_value"

    return arrays, groups


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

def _grouped_column_names(schema: TableSchema) -> set[str]:
    """Return column names that belong to any group."""
    col_names = [c.name for c in schema.columns]
    result: set[str] = set()
    for g in schema.groups.values():
        result.update(col_names[i] for i in g.indices)
    return result


def _build_row(
    excel_row,
    schema: TableSchema,
    col_names: list[str],
    col_types: dict[str, str],
    server_names: set[str],
    grouped_names: set[str],
) -> dict:
    """Convert one Excel data row into a structured dict."""
    row_data: dict = {}
    prev_value = None

    for idx, cell in enumerate(excel_row):
        if idx >= len(col_names):
            break
        name = col_names[idx]
        if not name or name not in server_names:
            prev_value = cell.value
            continue

        value = _convert_cell(cell.value, col_types.get(name, ""))
        if value is None:
            prev_value = cell.value
            continue

        col = schema.columns[idx]

        if name in schema.arrays:
            if value not in (0, -1, ""):
                row_data.setdefault(name, []).append(value)
        elif col.map_role == "set":
            row_data.setdefault(name, {})[value] = True
        elif col.map_role == "map_value" and prev_value is not None:
            group_key = name.split("_")[0]
            row_data.setdefault(group_key, {})[prev_value] = value
        elif name in grouped_names:
            if value not in (0, -1, ""):
                _append_to_group(row_data, name, value)
        else:
            row_data[name] = value

        prev_value = cell.value

    return row_data


def _append_to_group(row_data: dict, name: str, value) -> None:
    """Append a value into the correct group sub-list."""
    group_key = name.split("_")[0]
    member = {name: value}
    if group_key in row_data:
        last = row_data[group_key][-1]
        if name in last:
            row_data[group_key].append(member)
        else:
            last[name] = value
    else:
        row_data[group_key] = [member]


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
