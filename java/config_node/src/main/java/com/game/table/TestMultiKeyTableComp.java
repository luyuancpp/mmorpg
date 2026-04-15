
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for TestMultiKeyTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class TestMultiKeyTableComp {

    private TestMultiKeyTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(TestMultiKeyTable row) {
            return new Id(row.getId());
        }
    }

    public record Level(int value) {
        public static Level from(TestMultiKeyTable row) {
            return new Level(row.getLevel());
        }
    }

    public record String_key(String value) {
        public static String_key from(TestMultiKeyTable row) {
            return new String_key(row.getStringKey());
        }
    }

    public record Uint32_key(int value) {
        public static Uint32_key from(TestMultiKeyTable row) {
            return new Uint32_key(row.getUint32Key());
        }
    }

    public record Int32_key(int value) {
        public static Int32_key from(TestMultiKeyTable row) {
            return new Int32_key(row.getInt32Key());
        }
    }

    public record M_string_key(String value) {
        public static M_string_key from(TestMultiKeyTable row) {
            return new M_string_key(row.getMStringKey());
        }
    }

    public record M_uint32_key(int value) {
        public static M_uint32_key from(TestMultiKeyTable row) {
            return new M_uint32_key(row.getMUint32Key());
        }
    }

    public record M_int32_key(int value) {
        public static M_int32_key from(TestMultiKeyTable row) {
            return new M_int32_key(row.getMInt32Key());
        }
    }

    public record Test_ref(int value) {
        public static Test_ref from(TestMultiKeyTable row) {
            return new Test_ref(row.getTestRef());
        }
    }

    public record Effect(List<Integer> values) {
        public static Effect from(TestMultiKeyTable row) {
            return new Effect(row.getEffectList());
        }
    }

    public record Test_refs(List<Integer> values) {
        public static Test_refs from(TestMultiKeyTable row) {
            return new Test_refs(row.getTestRefsList());
        }
    }

}