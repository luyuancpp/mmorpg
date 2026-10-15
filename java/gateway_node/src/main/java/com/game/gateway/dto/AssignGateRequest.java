package com.game.gateway.dto;

public class AssignGateRequest {
    /**
     * Zone the player wants to enter. 0 means "any" — server picks the
     * least-loaded zone. Stored as int because the gRPC contract is
     * uint32; widening to long here just requires a cast on every send.
     */
    private int zoneId;

    /** Optional account hint, used by the limiter for cooldown keying
     *  AND by go-zero for reconnect-bypass detection.
     *  带 queue_token 的重入不把 account 交给限流器做冷却(G4b-J0),account 仍原样转给 login。 */
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
     *
     * G4b-J0 之后,带 token 重调 assign-gate 照样过波次、zone 桶和 IP 桶。被波次或 zone 桶挡住时
     * 回 code=100、queue_source="ratelimit",且不带新 token;IP 桶耗尽时回 code=429(IP_RATE_LIMIT)。
     * 两种情况客户端都应保留原 token,按 retry_after_ms(429 时按客户端退避)重试,不要当成排位丢失。
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
