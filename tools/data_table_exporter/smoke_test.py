"""Quick smoke test for the new 6-row Excel header format."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from core.config_loader import load_config
from core.excel_reader import read_table, read_data_rows


def main():
    cfg = load_config()
    test_files = ["Test.xlsx", "Buff.xlsx", "Reward.xlsx", "Mission.xlsx", "Skill.xlsx"]

    for fname in test_files:
        path = cfg.data_dir / fname
        if not path.exists():
            print(f"SKIP: {fname} not found")
            continue

        schema = read_table(path, cfg)
        if schema is None:
            print(f"FAIL: {fname} could not be parsed")
            continue

        # Print summary
        print(f"\n=== {schema.name} ({len(schema.columns)} cols) ===")

        # Arrays
        if schema.arrays:
            print(f"  arrays: {list(schema.arrays.keys())}")

        # Groups
        if schema.groups:
            for gn, gf in schema.groups.items():
                col_names = [c.name for c in gf.columns]
                print(f"  group '{gn}': columns={col_names}, indices={gf.indices}")

        # Maps
        if schema.maps:
            for mn, mf in schema.maps.items():
                print(f"  map '{mn}': <{mf.key_type}, {mf.value_type}>")

        # Table keys
        if schema.table_keys:
            names = [(c.name, "multi" if c.is_multi_key else "single") for c in schema.table_keys]
            print(f"  table_keys: {names}")

        # Bit index
        if schema.bit_index_columns:
            print(f"  bit_index: {[c.name for c in schema.bit_index_columns]}")

        # Expression
        if schema.expression_columns:
            for c in schema.expression_columns:
                print(f"  expr: {c.name} → type={c.expression_type}, params={c.expression_params}")

        # Sets
        if schema.set_columns:
            unique_sets = list(dict.fromkeys(c.name for c in schema.set_columns))
            print(f"  sets: {unique_sets}")

        # Flat multimap
        if schema.use_flat_multimap:
            print(f"  flat_multimap: True")

        # Read data
        rows = read_data_rows(schema, cfg)
        print(f"  data rows: {len(rows)}")
        if rows:
            print(f"  first row keys: {list(rows[0].keys())}")

    print("\n✓ All smoke tests passed")


if __name__ == "__main__":
    main()
