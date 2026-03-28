"""Main orchestrator — coordinates the full export pipeline."""

from __future__ import annotations

import logging
import sys

from core.config_loader import ExporterConfig, load_config
from core.excel_reader import read_all_tables
from core.file_utils import ensure_dirs, md5_copy
from core.foreign_key import validate_foreign_keys
from core.generators.bit_index_gen import generate_bit_indexes
from core.generators.comp_gen import generate_comp_headers
from core.generators.config_gen import generate_config_classes
from core.generators.constants_gen import generate_constants
from core.generators.enum_gen import generate_operator_enums, generate_tip_enums
from core.generators.json_gen import generate_json
from core.generators.proto_gen import (
    compile_proto_cpp,
    compile_proto_go,
    compile_proto_java,
    generate_proto_files,
)
from core.generators.table_id_gen import generate_table_ids

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------

def run(cfg: ExporterConfig) -> None:
    """Execute the full export pipeline."""
    logger.info("===== Data Table Exporter: START =====")
    _ensure_output_dirs(cfg)

    # Read schemas once — every generator receives this list.
    tables = read_all_tables(cfg)
    logger.info("Read %d table schema(s)", len(tables))

    warnings = validate_foreign_keys(tables)
    if warnings:
        logger.warning("FK validation: %d warning(s)", len(warnings))

    # Generate
    generate_json(cfg, tables)
    generate_proto_files(cfg, tables)
    generate_operator_enums(cfg)
    generate_tip_enums(cfg)
    compile_proto_cpp(cfg)
    compile_proto_go(cfg)
    compile_proto_java(cfg)
    generate_config_classes(cfg, tables)
    generate_comp_headers(cfg, tables)
    generate_table_ids(cfg, tables)
    generate_constants(cfg, tables)
    generate_bit_indexes(cfg, tables)

    # Deploy
    _deploy(cfg)
    logger.info("===== Data Table Exporter: DONE =====")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _ensure_output_dirs(cfg: ExporterConfig) -> None:
    dirs = [cfg.json_dir, cfg.proto_dir, cfg.state_dir]
    for lang in (cfg.cpp, cfg.go, cfg.java):
        if lang.enabled:
            dirs.extend([
                lang.code_dir, lang.proto_output_dir,
                lang.constants_dir, lang.table_id_dir, lang.bit_index_dir,
            ])
    ensure_dirs(*[d for d in dirs if d and str(d)])


def _deploy(cfg: ExporterConfig) -> None:
    """Copy generated outputs to final destinations (MD5-checked)."""
    logger.info("--- Deploying outputs ---")
    tasks: list[tuple] = []

    if cfg.cpp.enabled:
        tasks.extend((d["src"], d["dst"]) for d in cfg.cpp.deploy)

    if cfg.go.enabled:
        scan = cfg.go.grpc_service_scan_dir
        base = cfg.go.grpc_deploy_base
        if scan.exists() and scan.is_dir():
            for svc in scan.iterdir():
                if svc.is_dir():
                    tasks.append((cfg.go.code_dir.parent, base / svc.name / "generated"))

    if cfg.java.enabled:
        tasks.extend((d["src"], d["dst"]) for d in cfg.java.deploy)

    ok, fail = 0, 0
    for src, dst in tasks:
        try:
            md5_copy(src, dst)
            ok += 1
        except Exception as exc:
            logger.error("Deploy failed %s -> %s: %s", src, dst, exc)
            fail += 1
    logger.info("Deploy: %d OK, %d failed", ok, fail)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    config_path = sys.argv[1] if len(sys.argv) > 1 else None
    cfg = load_config(config_path)
    run(cfg)


if __name__ == "__main__":
    main()
