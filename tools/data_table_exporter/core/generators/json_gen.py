"""JSON data generator.

Reads each data table and writes a ``{SheetName}.json`` file
containing the data rows.
"""

from __future__ import annotations

import json
import logging
from concurrent.futures import ThreadPoolExecutor

from core.config_loader import ExporterConfig
from core.excel_reader import read_data_rows
from core.file_utils import write_file, ensure_dirs
from core.schema import TableSchema

logger = logging.getLogger(__name__)


def generate_json(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate JSON files for all tables."""
    ensure_dirs(cfg.json_dir)
    if not tables:
        logger.warning("No tables to generate JSON for")
        return

    with ThreadPoolExecutor() as pool:
        pool.map(lambda t: _export_one(t, cfg), tables)


def _export_one(table: TableSchema, cfg: ExporterConfig) -> None:
    try:
        rows = read_data_rows(table, cfg)
        if not rows:
            return
        out = cfg.json_dir / f"{table.name}.json"
        content = json.dumps(
            {"data": rows},
            sort_keys=True,
            indent=1,
            separators=(",", ": "),
            ensure_ascii=False,
        )
        write_file(out, content)
        logger.info("Generated %s", out.name)
    except Exception as exc:
        logger.error("JSON generation failed for %s: %s", table.name, exc)
