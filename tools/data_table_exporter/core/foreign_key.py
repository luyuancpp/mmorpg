"""Foreign key validation.

After all tables are read, validates that every FK reference points to
an existing table (and optionally an existing column).
"""

from __future__ import annotations

import logging

from core.schema import TableSchema

logger = logging.getLogger(__name__)


def validate_foreign_keys(tables: list[TableSchema]) -> list[str]:
    """Check all FK / GFK references across *tables*.

    Returns a list of human-readable warning strings (empty if all OK).
    """
    table_map = {t.name: t for t in tables}
    col_map: dict[str, set[str]] = {
        t.name: {c.name for c in t.columns} for t in tables
    }
    warnings: list[str] = []

    for table in tables:
        for col in table.foreign_key_columns:
            fk = col.foreign_key
            if fk is None:
                continue
            if fk.target_table not in table_map:
                msg = (
                    f"[{table.name}.{col.name}] FK target table "
                    f"'{fk.target_table}' does not exist"
                )
                warnings.append(msg)
                logger.warning(msg)
            elif fk.target_column not in col_map[fk.target_table]:
                msg = (
                    f"[{table.name}.{col.name}] FK target column "
                    f"'{fk.target_table}.{fk.target_column}' does not exist"
                )
                warnings.append(msg)
                logger.warning(msg)

        for col in table.columns:
            gfk = col.group_foreign_key
            if gfk is None:
                continue
            if gfk.target_table not in table_map:
                msg = (
                    f"[{table.name}.{col.name}] GFK target table "
                    f"'{gfk.target_table}' does not exist"
                )
                warnings.append(msg)
                logger.warning(msg)

    return warnings
