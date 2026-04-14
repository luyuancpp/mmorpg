package com.game.gateway.dto;

/**
 * Manual zone status set by operations.
 * Ordinal values match the DB column (zone_config.manual_status).
 */
public enum ManualZoneStatus {
    OPEN(0),
    MAINTENANCE(1),
    CLOSED(2),
    PREVIEW(3);

    private final int code;

    ManualZoneStatus(int code) { this.code = code; }

    public int getCode() { return code; }

    public static ManualZoneStatus fromCode(int code) {
        for (ManualZoneStatus s : values()) {
            if (s.code == code) return s;
        }
        return OPEN;
    }
}
