"""Named-constant generator.

For tables with a ``constants_name`` column, generates constant definition
files: C++ ``constexpr``, Go ``const``, Java ``static final``.
"""

from __future__ import annotations

import logging

from jinja2 import Environment, FileSystemLoader

from core.config_loader import ExporterConfig
from core.excel_reader import open_worksheet
from core.file_utils import ensure_dirs, write_file
from core.schema import TableSchema

logger = logging.getLogger(__name__)


def generate_constants(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate constant files for tables that have a ``constants_name`` column."""
    env = Environment(loader=FileSystemLoader(str(cfg.template_dir), encoding="utf-8"))

    for schema in tables:
        if not schema.has_constants_name:
            continue

        ws = open_worksheet(schema)
        is_global = "globalvariable" in schema.name.lower()
        groups, singles = _extract_constants(ws, schema, cfg.data_begin_row, is_global)

        if cfg.cpp.enabled:
            _gen_cpp(groups, singles, schema.name, env, cfg)
        if cfg.go.enabled:
            _gen_go(groups, singles, schema.name, env, cfg)
        if cfg.java.enabled:
            _gen_java(groups, singles, schema.name, env, cfg)


# ---------------------------------------------------------------------------
# Extraction
# ---------------------------------------------------------------------------

def _extract_constants(ws, schema, begin_row, is_global):
    """Extract constants from data rows.

    Returns ``(groups, singles)`` where:
      - *groups*: ``{(prefix1, prefix2): [{name, value}, ...]}``
      - *singles*: ``[{name, value}, ...]``

    For GlobalVariable tables, constants with underscores are split into
    groups by their first two underscore-delimited parts.
    """
    ci = schema.constants_name_index
    groups: dict[tuple[str, str], list[dict]] = {}
    singles: list[dict] = []

    for row in ws.iter_rows(min_row=begin_row, values_only=True):
        id_val = row[0]
        if id_val is None:
            continue
        raw_name = row[ci] if ci is not None and ci < len(row) else None
        if not raw_name:
            continue

        name = str(raw_name).strip()
        entry = {"name": name, "value": int(id_val)}

        if is_global and "_" in name:
            parts = name.split("_", 2)
            if len(parts) >= 2:
                groups.setdefault((parts[0], parts[1]), []).append(entry)
                continue
        singles.append(entry)

    return groups, singles


# ---------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------

def _gen_cpp(groups, singles, sheet, env, cfg):
    tpl = env.get_template("cpp_constants.h.j2")
    ensure_dirs(cfg.cpp.constants_dir)

    for (p1, p2), items in groups.items():
        constants = [{"name": f"k{sheet}_{e['name']}", "value": e["value"]} for e in items]
        fname = f"global_{p1}_{p2}_table_id_constants.h".lower()
        write_file(cfg.cpp.constants_dir / fname, tpl.render(constants=constants))

    if singles:
        constants = [{"name": f"k{sheet}_{e['name']}", "value": e["value"]} for e in singles]
        write_file(
            cfg.cpp.constants_dir / f"{sheet.lower()}_table_id_constants.h",
            tpl.render(constants=constants),
        )


# ---------------------------------------------------------------------------
# Go
# ---------------------------------------------------------------------------

def _gen_go(groups, singles, sheet, env, cfg):
    tpl = env.get_template("go_constants.go.j2")
    ensure_dirs(cfg.go.constants_dir)

    for (p1, p2), items in groups.items():
        constants = [{"name": f"K{sheet}_{e['name']}", "value": e["value"]} for e in items]
        fname = f"global_{p1}_{p2}_table_id_constants.go".lower()
        write_file(cfg.go.constants_dir / fname, tpl.render(constants=constants))

    if singles:
        constants = [{"name": f"K{sheet}_{e['name']}", "value": e["value"]} for e in singles]
        write_file(
            cfg.go.constants_dir / f"{sheet.lower()}_table_id_constants.go",
            tpl.render(constants=constants),
        )


# ---------------------------------------------------------------------------
# Java
# ---------------------------------------------------------------------------

def _gen_java(groups, singles, sheet, env, cfg):
    tpl = env.get_template("java_constants.java.j2")
    ensure_dirs(cfg.java.constants_dir)

    all_entries = []
    for items in groups.values():
        all_entries.extend(items)
    all_entries.extend(singles)

    constants = [{"name": e["name"].upper(), "value": e["value"]} for e in all_entries]
    write_file(
        cfg.java.constants_dir / f"{sheet}Constants.java",
        tpl.render(constants=constants, classname=f"{sheet}Constants", package=cfg.java.package),
    )
