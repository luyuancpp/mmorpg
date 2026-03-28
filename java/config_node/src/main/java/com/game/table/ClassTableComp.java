
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for ClassTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class ClassTableComp {

    private ClassTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(ClassTable row) {
            return new Id(row.getId());
        }
    }

    public record Skill(List<Integer> values) {
        public static Skill from(ClassTable row) {
            return new Skill(row.getSkillList());
        }
    }

}