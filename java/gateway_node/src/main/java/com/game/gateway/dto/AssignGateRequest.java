package com.game.gateway.dto;

public class AssignGateRequest {
    /**
     * Zone the player wants to enter. 0 means "any" — server picks the
     * least-loaded zone. Stored as int because the gRPC contract is
     * uint32; widening to long here just requires a cast on every send.
     */
    private int zoneId;

    /** Optional account hint, used by the limiter for cooldown keying
     *  AND by go-zero for reconnect-bypass detection. */
    private String account;

    /** Stable client device identifier. Threaded through to the queue
     *  meta so ops can audit "which device is queue position #N". */
    private String deviceId;

    /**
     * Re-entry token for clients already in the queue. Empty/null on the
     * first AssignGate call; populated by the response of an earlier
     * AssignGate (or QueryQueueStatus) when the zone was over capacity.
     *
     * Including this in the request lets a flaky client recover after a
     * dropped /api/queue-status poll: hitting AssignGate with the saved
     * token resumes the in-flight queue entry rather than enqueueing
     * twice (which would push the player further back in line).
     */
    private String queueToken;

    public int getZoneId() { return zoneId; }
    public void setZoneId(int zoneId) { this.zoneId = zoneId; }

    public String getAccount() { return account; }
    public void setAccount(String account) { this.account = account; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getQueueToken() { return queueToken; }
    public void setQueueToken(String queueToken) { this.queueToken = queueToken; }
}
