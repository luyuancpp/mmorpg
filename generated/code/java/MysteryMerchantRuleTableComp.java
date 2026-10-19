
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MysteryMerchantRuleTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MysteryMerchantRuleTableComp {

    private MysteryMerchantRuleTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MysteryMerchantRuleTable row) {
            return new Id(row.getId());
        }
    }

    public record Interval_seconds(int value) {
        public static Interval_seconds from(MysteryMerchantRuleTable row) {
            return new Interval_seconds(row.getIntervalSeconds());
        }
    }

    public record Stay_seconds(int value) {
        public static Stay_seconds from(MysteryMerchantRuleTable row) {
            return new Stay_seconds(row.getStaySeconds());
        }
    }

    public record Shelf_size(int value) {
        public static Shelf_size from(MysteryMerchantRuleTable row) {
            return new Shelf_size(row.getShelfSize());
        }
    }

    public record Weight_discount60(int value) {
        public static Weight_discount60 from(MysteryMerchantRuleTable row) {
            return new Weight_discount60(row.getWeightDiscount60());
        }
    }

    public record Weight_discount50(int value) {
        public static Weight_discount50 from(MysteryMerchantRuleTable row) {
            return new Weight_discount50(row.getWeightDiscount50());
        }
    }

    public record Weight_discount40(int value) {
        public static Weight_discount40 from(MysteryMerchantRuleTable row) {
            return new Weight_discount40(row.getWeightDiscount40());
        }
    }

    public record Interact_range(int value) {
        public static Interact_range from(MysteryMerchantRuleTable row) {
            return new Interact_range(row.getInteractRange());
        }
    }

    public record Desc(String value) {
        public static Desc from(MysteryMerchantRuleTable row) {
            return new Desc(row.getDesc());
        }
    }

}