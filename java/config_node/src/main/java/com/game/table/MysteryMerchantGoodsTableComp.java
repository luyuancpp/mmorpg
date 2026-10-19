
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MysteryMerchantGoodsTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MysteryMerchantGoodsTableComp {

    private MysteryMerchantGoodsTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MysteryMerchantGoodsTable row) {
            return new Id(row.getId());
        }
    }

    public record Name(String value) {
        public static Name from(MysteryMerchantGoodsTable row) {
            return new Name(row.getName());
        }
    }

    public record Item_id(int value) {
        public static Item_id from(MysteryMerchantGoodsTable row) {
            return new Item_id(row.getItemId());
        }
    }

    public record Item_count(int value) {
        public static Item_count from(MysteryMerchantGoodsTable row) {
            return new Item_count(row.getItemCount());
        }
    }

    public record Currency_type(int value) {
        public static Currency_type from(MysteryMerchantGoodsTable row) {
            return new Currency_type(row.getCurrencyType());
        }
    }

    public record Price(long value) {
        public static Price from(MysteryMerchantGoodsTable row) {
            return new Price(row.getPrice());
        }
    }

    public record Weight(int value) {
        public static Weight from(MysteryMerchantGoodsTable row) {
            return new Weight(row.getWeight());
        }
    }

    public record Limit_count(int value) {
        public static Limit_count from(MysteryMerchantGoodsTable row) {
            return new Limit_count(row.getLimitCount());
        }
    }

}