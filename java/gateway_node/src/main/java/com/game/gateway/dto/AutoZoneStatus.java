package com.game.gateway.dto;

/**
 * Auto-detected health status from probing etcd/Redis.
 */
public enum AutoZoneStatus {
    HEALTHY,
    DEGRADED,
    DOWN
}
