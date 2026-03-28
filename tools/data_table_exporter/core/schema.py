"""Schema models for data tables.

Defines the data structures that represent parsed Excel table metadata.
All generators consume these models instead of raw Excel data.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class ForeignKeyRef:
    """A column-level foreign key reference.

    Parsed from Excel metadata row content like:
        fk:Reward         → references Reward.id
        fk:Item.type      → references Item.type
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

    Parsed from Excel metadata row content like:
        gfk:Reward        → this column group references the Reward table
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
    """Metadata for a single Excel column, extracted from header rows."""
    name: str
    data_type: str                                  # proto type: int32, string, …
    owner: str = ""                                 # server | client | common | design
    map_role: str = ""                              # "map_key" | "map_value" | "set" | ""
    is_table_key: bool = False                      # secondary index column
    is_multi_key: bool = False                      # use multimap
    expression_type: str = ""                       # return type for expression eval
    expression_params: list[str] = field(default_factory=list)
    has_bit_index: bool = False                     # bit_index marker present
    foreign_key: Optional[ForeignKeyRef] = None
    group_foreign_key: Optional[GroupForeignKeyRef] = None
    excel_index: int = 0                            # 0-based column index
    constants_name: Optional[str] = None            # value of constants_name row

    @property
    def is_server(self) -> bool:
        return self.owner in ("server", "common")

    @property
    def is_excluded(self) -> bool:
        return self.owner in ("client", "design")


@dataclass
class ArrayField:
    """Consecutive columns with the same name → repeated proto field."""
    name: str
    data_type: str
    indices: list[int] = field(default_factory=list)


@dataclass
class GroupField:
    """Non-consecutive column pattern → repeated sub-message field."""
    name: str                           # common prefix of grouped columns
    columns: list[ColumnDef] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)


@dataclass
class TableSchema:
    """Complete schema for one Excel sheet / data table."""
    name: str
    source_path: Optional[Path] = None                # path to the source .xlsx
    columns: list[ColumnDef] = field(default_factory=list)
    arrays: dict[str, ArrayField] = field(default_factory=dict)
    groups: dict[str, GroupField] = field(default_factory=dict)
    use_flat_multimap: bool = False
    has_constants_name: bool = False
    constants_name_index: Optional[int] = None      # column index of constants_name

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
