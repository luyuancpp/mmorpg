"""Component struct generator.

Produces per-table component files for C++ (ECS headers), Go (comp structs),
and Java (comp records) using Jinja2 templates.
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
    """Generate per-column component files for all tables and enabled languages."""
    env = Environment(loader=FileSystemLoader(str(cfg.template_dir), encoding="utf-8"))

    if cfg.cpp.enabled:
        _gen_cpp_comps(cfg, tables, env)

    if cfg.go.enabled:
        _gen_go_comps(cfg, tables, env)

    if cfg.java.enabled:
        _gen_java_comps(cfg, tables, env)


def _gen_cpp_comps(cfg: ExporterConfig, tables: list[TableSchema], env: Environment) -> None:
    ensure_dirs(cfg.cpp.code_dir)
    tpl = env.get_template("cpp_table_comp.h.j2")

    for t in tables:
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


def _gen_go_comps(cfg: ExporterConfig, tables: list[TableSchema], env: Environment) -> None:
    ensure_dirs(cfg.go.code_dir)
    tpl = env.get_template("go_table_comp.go.j2")

    for t in tables:
        if not t.scalar_comp_columns and not t.repeated_comp_arrays:
            continue
        ctx = dict(
            table=t,
            sheetname=t.name,
            to_go_type=type_mapping.to_go_type,
            to_go_proto_field=type_mapping.to_go_proto_field,
            to_go_repeated_elem_type=type_mapping.to_go_repeated_elem_type,
            proto_import_path=cfg.go.proto_import_path,
        )
        out_path = cfg.go.code_dir / f"{t.name.lower()}_table_comp.go"
        write_file(out_path, tpl.render(**ctx))
        logger.info("Generated Go comp: %s", t.name)


def _gen_java_comps(cfg: ExporterConfig, tables: list[TableSchema], env: Environment) -> None:
    ensure_dirs(cfg.java.code_dir)
    tpl = env.get_template("java_table_comp.java.j2")

    for t in tables:
        if not t.scalar_comp_columns and not t.repeated_comp_arrays:
            continue
        ctx = dict(
            table=t,
            sheetname=t.name,
            to_java_type=type_mapping.to_java_type,
            to_java_boxed=type_mapping.to_java_boxed,
            to_java_proto_getter=type_mapping.to_java_proto_getter,
            to_java_repeated_elem_type=type_mapping.to_java_repeated_elem_type,
            package=cfg.java.package,
        )
        out_path = cfg.java.code_dir / f"{t.name}TableComp.java"
        write_file(out_path, tpl.render(**ctx))
        logger.info("Generated Java comp: %s", t.name)
