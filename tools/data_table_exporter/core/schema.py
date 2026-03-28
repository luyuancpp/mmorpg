"""Schema models for data tables.

Defines the data structures that represent parsed Excel table metadata.
All generators consume these models instead of raw Excel data.

Excel header format (5 rows):
    Row 1: field name — one name per logical field span (first col of span)
    Row 2: type declaration — ``uint32``, ``map<K,V>``, ``set<T>``,
           ``repeated uint32``, ``repeated { uint32 f1; uint32 f2 }``
    Row 3: owner — "server", "client", "common", "design", or "constants_name"
    Row 4: options — space-separated tokens: bit_index, key, multi, fk:T, gfk:T, expr:type, expr_params:a,b
    Row 5: comment
    Row 6+: data
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class ForeignKeyRef:
    """A column-level foreign key reference.

    Parsed from option token ``fk:Table`` or ``fk:Table.column``.
    """
    target_table: str
    target_column: str = "id"

    @staticmethod
    def parse(text: str) -> Optional[ForeignKeyRef]:
        """Parse 'fk:Table' or 'fk:Table.column' syntax."""
        if not text or not text.strip().lower().startswith("fk:"):
            return None
        body = text.strip()[3:]
        if "." in body:
            table, column = body.split(".", 1)
            return ForeignKeyRef(target_table=table.strip(), target_column=column.strip())
        return ForeignKeyRef(target_table=body.strip())


@dataclass
class GroupForeignKeyRef:
    """A group-level foreign key reference.

    Parsed from option token ``gfk:Table``.
    """
    target_table: str

    @staticmethod
    def parse(text: str) -> Optional[GroupForeignKeyRef]:
        """Parse 'gfk:Table' syntax."""
        if not text or not text.strip().lower().startswith("gfk:"):
            return None
        body = text.strip()[4:]
        return GroupForeignKeyRef(target_table=body.strip())


@dataclass
class ColumnDef:
    """Metadata for a single Excel column, extracted from the 6-row header."""
    name: str
    data_type: str                                  # proto type: int32, string, …
    owner: str = ""                                 # server | client | common | design
    struct: str = ""                                # repeated | set | map_key | map_value | message:Name | ""
    options: list[str] = field(default_factory=list)  # space-separated tokens
    comment: str = ""
    excel_index: int = 0                            # 0-based column index
    constants_name: Optional[str] = None

    # ----- struct-derived properties -----

    @property
    def map_role(self) -> str:
        if self.struct in ("set", "map_key", "map_value"):
            return self.struct
        return ""

    @property
    def is_repeated(self) -> bool:
        return self.struct == "repeated"

    @property
    def is_set(self) -> bool:
        return self.struct == "set"

    @property
    def message_name(self) -> str:
        if self.struct.startswith("message:"):
            return self.struct.split(":", 1)[1]
        return ""

    # ----- option-derived properties -----

    @property
    def is_table_key(self) -> bool:
        return "key" in self.options

    @property
    def is_multi_key(self) -> bool:
        return "multi" in self.options

    @property
    def has_bit_index(self) -> bool:
        return "bit_index" in self.options

    @property
    def expression_type(self) -> str:
        for opt in self.options:
            if opt.startswith("expr:"):
                return opt[5:]
        return ""

    @property
    def expression_params(self) -> list[str]:
        for opt in self.options:
            if opt.startswith("expr_params:"):
                return [p.strip() for p in opt[12:].split(",") if p.strip()]
        return []

    @property
    def foreign_key(self) -> Optional[ForeignKeyRef]:
        for opt in self.options:
            if opt.lower().startswith("fk:"):
                return ForeignKeyRef.parse(opt)
        return None

    @property
    def group_foreign_key(self) -> Optional[GroupForeignKeyRef]:
        for opt in self.options:
            if opt.lower().startswith("gfk:"):
                return GroupForeignKeyRef.parse(opt)
        return None

    # ----- owner-derived properties -----

    @property
    def is_server(self) -> bool:
        return self.owner in ("server", "common")

    @property
    def is_excluded(self) -> bool:
        return self.owner in ("client", "design")


@dataclass
class ArrayField:
    """Columns with the same name and ``struct: repeated`` → repeated proto field."""
    name: str
    data_type: str
    indices: list[int] = field(default_factory=list)


@dataclass
class MapField:
    """Consecutive ``map_key`` + ``map_value`` column pairs → proto ``map`` field."""
    name: str           # common prefix (e.g. "tag" from "tag_key"/"tag_value")
    key_type: str
    value_type: str


@dataclass
class GroupField:
    """Columns sharing the same ``message:Name`` → repeated sub-message field."""
    name: str
    columns: list[ColumnDef] = field(default_factory=list)   # unique column defs
    indices: list[int] = field(default_factory=list)          # all column positions


@dataclass
class TableSchema:
    """Complete schema for one Excel sheet / data table."""
    name: str
    source_path: Optional[Path] = None
    columns: list[ColumnDef] = field(default_factory=list)
    arrays: dict[str, ArrayField] = field(default_factory=dict)
    groups: dict[str, GroupField] = field(default_factory=dict)
    maps: dict[str, MapField] = field(default_factory=dict)
    use_flat_multimap: bool = False
    has_constants_name: bool = False
    constants_name_index: Optional[int] = None

    # ----- convenience properties -----

    @property
    def server_columns(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.is_server]

    @property
    def table_keys(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.is_table_key]

    @property
    def expression_columns(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.expression_type]

    @property
    def foreign_key_columns(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.foreign_key]

    @property
    def bit_index_columns(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.has_bit_index]

    @property
    def map_keys(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.map_role == "map_key"]

    @property
    def set_columns(self) -> list[ColumnDef]:
        return [c for c in self.columns if c.map_role == "set"]

    @property
    def col_to_group(self) -> dict[int, str]:
        """Map column index → group name for grouped columns."""
        result: dict[int, str] = {}
        for g in self.groups.values():
            for idx in g.indices:
                result[idx] = g.name
        return result
