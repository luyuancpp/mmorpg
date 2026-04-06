#!/usr/bin/env python3
"""pb2json — Convert proto-binary table files (.pb) to human-readable JSON.

Usage:
    # View a single table
    python pb2json.py Buff

    # View a single table, write to file
    python pb2json.py Buff -o buff_preview.json

    # View all tables in the directory
    python pb2json.py --all

    # Specify custom paths
    python pb2json.py Buff --pb-dir ../../generated/tables --proto-dir ../../generated/code/proto/python
"""

from __future__ import annotations

import argparse
import importlib
import json
import sys
from pathlib import Path

from google.protobuf import json_format


def find_project_root() -> Path:
    """Walk up from this script to find the repo root (contains 'generated/')."""
    d = Path(__file__).resolve().parent
    for _ in range(10):
        if (d / "generated").is_dir():
            return d
        d = d.parent
    return Path(__file__).resolve().parent


def convert_one(table_name: str, pb_dir: Path, proto_py_dir: Path) -> str:
    """Read ``{table_name}.pb`` and return formatted JSON string."""
    # Ensure compiled proto modules are importable
    py_dir = str(proto_py_dir)
    if py_dir not in sys.path:
        sys.path.insert(0, py_dir)

    module_name = f"{table_name.lower()}_table_pb2"
    try:
        mod = importlib.import_module(module_name)
    except ModuleNotFoundError:
        raise FileNotFoundError(
            f"Proto module '{module_name}' not found under {proto_py_dir}.\n"
            f"Run the exporter first to generate *_pb2.py files."
        )

    # Find the wrapper message class: {PascalName}TableData
    pascal = table_name[0].upper() + table_name[1:]
    cls_name = f"{pascal}TableData"
    msg_cls = getattr(mod, cls_name, None)
    if msg_cls is None:
        raise AttributeError(f"Message class '{cls_name}' not found in {module_name}")

    pb_path = pb_dir / f"{table_name.lower()}.pb"
    if not pb_path.exists():
        raise FileNotFoundError(f"Binary file not found: {pb_path}")

    msg = msg_cls()
    with open(pb_path, "rb") as f:
        msg.ParseFromString(f.read())

    return json_format.MessageToJson(
        msg,
        indent=2,
        preserving_proto_field_name=True,
        ensure_ascii=False,
    )


def list_tables(pb_dir: Path) -> list[str]:
    """Return table names (without extension) found in pb_dir."""
    return sorted(p.stem for p in pb_dir.glob("*.pb"))


def main() -> None:
    root = find_project_root()
    default_pb_dir = root / "generated" / "tables"
    default_proto_dir = root / "generated" / "code" / "proto" / "python"

    parser = argparse.ArgumentParser(
        description="Convert proto-binary table files (.pb) to JSON for inspection."
    )
    parser.add_argument(
        "table",
        nargs="?",
        help="Table name, e.g. 'Buff' or 'buff' (case-insensitive).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert all .pb files in the directory.",
    )
    parser.add_argument(
        "--pb-dir",
        type=Path,
        default=default_pb_dir,
        help=f"Directory containing .pb files (default: {default_pb_dir}).",
    )
    parser.add_argument(
        "--proto-dir",
        type=Path,
        default=default_proto_dir,
        help=f"Directory containing *_pb2.py modules (default: {default_proto_dir}).",
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        default=None,
        help="Write output to file instead of stdout.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory for --all mode (default: print to stdout).",
    )

    args = parser.parse_args()

    if not args.table and not args.all:
        tables = list_tables(args.pb_dir)
        if tables:
            print("Available tables:", ", ".join(tables))
        else:
            print(f"No .pb files found in {args.pb_dir}")
        parser.print_help()
        sys.exit(1)

    if args.all:
        tables = list_tables(args.pb_dir)
        if not tables:
            print(f"No .pb files found in {args.pb_dir}", file=sys.stderr)
            sys.exit(1)
        for name in tables:
            try:
                result = convert_one(name, args.pb_dir, args.proto_dir)
                if args.out_dir:
                    args.out_dir.mkdir(parents=True, exist_ok=True)
                    out_path = args.out_dir / f"{name}.json"
                    out_path.write_text(result, encoding="utf-8")
                    print(f"  {name} -> {out_path}")
                else:
                    print(f"=== {name} ===")
                    print(result)
            except Exception as exc:
                print(f"  {name}: ERROR - {exc}", file=sys.stderr)
    else:
        try:
            result = convert_one(args.table, args.pb_dir, args.proto_dir)
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(result, encoding="utf-8")
                print(f"Written to {args.output}")
            else:
                print(result)
        except Exception as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
