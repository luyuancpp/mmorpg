
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MysteryMerchantSpotTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MysteryMerchantSpotTableComp {

    private MysteryMerchantSpotTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MysteryMerchantSpotTable row) {
            return new Id(row.getId());
        }
    }

    public record Scene_config_id(int value) {
        public static Scene_config_id from(MysteryMerchantSpotTable row) {
            return new Scene_config_id(row.getSceneConfigId());
        }
    }

    public record X(double value) {
        public static X from(MysteryMerchantSpotTable row) {
            return new X(row.getX());
        }
    }

    public record Y(double value) {
        public static Y from(MysteryMerchantSpotTable row) {
            return new Y(row.getY());
        }
    }

    public record Z(double value) {
        public static Z from(MysteryMerchantSpotTable row) {
            return new Z(row.getZ());
        }
    }

    public record Desc(String value) {
        public static Desc from(MysteryMerchantSpotTable row) {
            return new Desc(row.getDesc());
        }
    }

}