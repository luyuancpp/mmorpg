
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for SkillPermissionTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class SkillPermissionTableComp {

    private SkillPermissionTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(SkillPermissionTable row) {
            return new Id(row.getId());
        }
    }

    public record Skill_type(List<Integer> values) {
        public static Skill_type from(SkillPermissionTable row) {
            return new Skill_type(row.getSkillTypeList());
        }
    }

}