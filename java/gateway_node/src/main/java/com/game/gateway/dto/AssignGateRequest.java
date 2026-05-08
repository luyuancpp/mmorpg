package com.game.gateway.dto;

public class AssignGateRequest {
    private long zoneId;

    /** Optional account hint, used by the limiter for cooldown keying. */
    private String account;

    public long getZoneId() { return zoneId; }
    public void setZoneId(long zoneId) { this.zoneId = zoneId; }

    public String getAccount() { return account; }
    public void setAccount(String account) { this.account = account; }
}
