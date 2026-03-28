"""Config class generator.

Produces per-table config manager classes for enabled languages (C++, Go,
Java) using Jinja2 templates, plus aggregated "all_table" files.
"""

from __future__ import annotations

import logging

from jinja2 import Environment, FileSystemLoader

from core.config_loader import ExporterConfig
from core.file_utils import ensure_dirs, write_file
from core.schema import TableSchema
from core import type_mapping

logger = logging.getLogger(__name__)


def generate_config_classes(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate config manager classes for all tables and all enabled languages."""
    env = Environment(loader=FileSystemLoader(str(cfg.template_dir), encoding="utf-8"))

    if cfg.cpp.enabled:
        ensure_dirs(cfg.cpp.code_dir)
        _gen_all_cpp(tables, env, cfg)

    if cfg.go.enabled:
        ensure_dirs(cfg.go.code_dir)
        _gen_all_go(tables, env, cfg)

    if cfg.java.enabled:
        ensure_dirs(cfg.java.code_dir)
        _gen_all_java(tables, env, cfg)


# ---------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------

def _gen_all_cpp(tables: list[TableSchema], env: Environment, cfg: ExporterConfig) -> None:
    h_tpl = env.get_template("cpp_config.h.j2")
    cpp_tpl = env.get_template("cpp_config.cpp.j2")

    for t in tables:
        ctx = _cpp_ctx(t)
        write_file(cfg.cpp.code_dir / f"{t.name.lower()}_table.h", h_tpl.render(**ctx))
        write_file(cfg.cpp.code_dir / f"{t.name.lower()}_table.cpp", cpp_tpl.render(**ctx))
        logger.info("Generated C++ config: %s", t.name)

    # all_table aggregator
    names = sorted(t.name for t in tables)
    ah_tpl = env.get_template("cpp_all_table.h.j2")
    ac_tpl = env.get_template("cpp_all_table.cpp.j2")
    write_file(cfg.cpp.code_dir / "all_table.h", ah_tpl.render())
    write_file(cfg.cpp.code_dir / "all_table.cpp", ac_tpl.render(sheetnames=names))


def _cpp_ctx(table: TableSchema) -> dict:
    return dict(
        table=table,
        sheetname=table.name,
        to_cpp_type=type_mapping.to_cpp_type,
        to_cpp_param_type=type_mapping.to_cpp_param_type,
        cpp_map_type=type_mapping.cpp_map_type,
    )


# ---------------------------------------------------------------------------
# Go
# ---------------------------------------------------------------------------

def _gen_all_go(tables: list[TableSchema], env: Environment, cfg: ExporterConfig) -> None:
    go_tpl = env.get_template("go_config.go.j2")

    for t in tables:
        ctx = _go_ctx(t, cfg)
        write_file(cfg.go.code_dir / f"{t.name.lower()}_table.go", go_tpl.render(**ctx))
        logger.info("Generated Go config: %s", t.name)

    names = sorted(t.name for t in tables)
    all_tpl = env.get_template("go_all_table.go.j2")
    write_file(cfg.go.code_dir / "all_table.go", all_tpl.render(sheetnames=names))


def _go_ctx(table: TableSchema, cfg: ExporterConfig) -> dict:
    return dict(
        table=table,
        sheetname=table.name,
        to_go_type=type_mapping.to_go_type,
        proto_import_path=cfg.go.proto_import_path,
    )


# ---------------------------------------------------------------------------
# Java
# ---------------------------------------------------------------------------

def _gen_all_java(tables: list[TableSchema], env: Environment, cfg: ExporterConfig) -> None:
    java_tpl = env.get_template("java_config.java.j2")

    for t in tables:
        ctx = _java_ctx(t, cfg)
        write_file(cfg.java.code_dir / f"{t.name}TableManager.java", java_tpl.render(**ctx))
        logger.info("Generated Java config: %s", t.name)

    names = sorted(t.name for t in tables)
    all_tpl = env.get_template("java_all_table.java.j2")
    write_file(cfg.java.code_dir / "AllTable.java", all_tpl.render(
        sheetnames=names, package=cfg.java.package
    ))


def _java_ctx(table: TableSchema, cfg: ExporterConfig) -> dict:
    return dict(
        table=table,
        sheetname=table.name,
        to_java_type=type_mapping.to_java_type,
        to_java_boxed=type_mapping.to_java_boxed,
        package=cfg.java.package,
    )
