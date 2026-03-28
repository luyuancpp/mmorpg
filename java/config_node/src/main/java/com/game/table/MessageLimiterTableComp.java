
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for MessageLimiterTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class MessageLimiterTableComp {

    private MessageLimiterTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(MessageLimiterTable row) {
            return new Id(row.getId());
        }
    }

    public record Max_requests(int value) {
        public static Max_requests from(MessageLimiterTable row) {
            return new Max_requests(row.getMaxRequests());
        }
    }

    public record Time_window(int value) {
        public static Time_window from(MessageLimiterTable row) {
            return new Time_window(row.getTimeWindow());
        }
    }

    public record Tip_message(int value) {
        public static Tip_message from(MessageLimiterTable row) {
            return new Tip_message(row.getTipMessage());
        }
    }

}