"""Table-ID enum generator.

For tables listed in ``constant_tables`` config, generates per-row ID
enumerations in C++, Go, and Java.
"""

from __future__ import annotations

import logging

from core.config_loader import ExporterConfig
from core.excel_reader import read_id_column
from core.file_utils import ensure_dirs, write_file
from core.schema import TableSchema

logger = logging.getLogger(__name__)


def generate_table_ids(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate table-ID enum files for tables in ``cfg.constant_tables``."""
    for schema in tables:
        if schema.name not in cfg.constant_tables:
            continue

        ids = read_id_column(schema, cfg)

        if cfg.cpp.enabled:
            _gen_cpp(ids, schema.name, cfg)
        if cfg.go.enabled:
            _gen_go(ids, schema.name, cfg)
        if cfg.java.enabled:
            _gen_java(ids, schema.name, cfg)


def _gen_cpp(ids: list[int], name: str, cfg: ExporterConfig) -> None:
    ensure_dirs(cfg.cpp.table_id_dir)
    lines = [
        "#pragma once",
        f"enum e_{name}_table_id : uint32_t",
        "{",
    ]
    lines.extend(f"    {name}_table_id{i}," for i in ids)
    lines.append("};")
    write_file(cfg.cpp.table_id_dir / f"{name.lower()}_table_id.h", "\n".join(lines) + "\n")
    logger.info("Generated C++ table_id: %s", name)


def _gen_go(ids: list[int], name: str, cfg: ExporterConfig) -> None:
    ensure_dirs(cfg.go.table_id_dir)
    pascal = "".join(w.capitalize() for w in name.split("_"))
    lines = ["package table", "", "const ("]
    lines.extend(f"\t{pascal}TableID{i} = {i}" for i in ids)
    lines.append(")")
    write_file(cfg.go.table_id_dir / f"{name.lower()}_table_id.go", "\n".join(lines) + "\n")
    logger.info("Generated Go table_id: %s", name)


def _gen_java(ids: list[int], name: str, cfg: ExporterConfig) -> None:
    ensure_dirs(cfg.java.table_id_dir)
    lines = [
        f"package {cfg.java.package};",
        "",
        f"public final class {name}TableId {{",
    ]
    lines.extend(f"    public static final int TABLE_ID_{i} = {i};" for i in ids)
    lines.append("}")
    write_file(cfg.java.table_id_dir / f"{name}TableId.java", "\n".join(lines) + "\n")
    logger.info("Generated Java table_id: %s", name)
