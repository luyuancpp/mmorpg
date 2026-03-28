"""Component struct generator.

Produces per-table ECS component header files for C++ using the
``cpp_table_comp.h.j2`` template.
"""

from __future__ import annotations

import logging

from jinja2 import Environment, FileSystemLoader

from core.config_loader import ExporterConfig
from core.file_utils import ensure_dirs, write_file
from core.schema import TableSchema
from core import type_mapping

logger = logging.getLogger(__name__)


def generate_comp_headers(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate per-column ECS component headers for all tables (C++ only)."""
    if not cfg.cpp.enabled:
        return

    ensure_dirs(cfg.cpp.code_dir)
    env = Environment(loader=FileSystemLoader(str(cfg.template_dir), encoding="utf-8"))
    tpl = env.get_template("cpp_table_comp.h.j2")

    for t in tables:
        # Skip tables with no component-eligible columns
        if not t.scalar_comp_columns and not t.repeated_comp_arrays:
            continue
        ctx = dict(
            table=t,
            sheetname=t.name,
            to_cpp_comp_type=type_mapping.to_cpp_comp_type,
            to_cpp_repeated_elem_type=type_mapping.to_cpp_repeated_elem_type,
        )
        out_path = cfg.cpp.code_dir / f"{t.name.lower()}_table_comp.h"
        write_file(out_path, tpl.render(**ctx))
        logger.info("Generated C++ comp: %s", t.name)
