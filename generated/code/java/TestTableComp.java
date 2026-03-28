
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for TestTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class TestTableComp {

    private TestTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(TestTable row) {
            return new Id(row.getId());
        }
    }

    public record Level(int value) {
        public static Level from(TestTable row) {
            return new Level(row.getLevel());
        }
    }

    public record Effect(List<Integer> values) {
        public static Effect from(TestTable row) {
            return new Effect(row.getEffectList());
        }
    }

}