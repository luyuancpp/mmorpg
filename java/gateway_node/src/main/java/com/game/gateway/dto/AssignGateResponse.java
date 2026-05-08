package com.game.gateway.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class AssignGateResponse {
    /**
     * 0    = OK (gateIp/gatePort/token* populated)
     * 100  = QUEUEING (retryAfterMs / queuePos populated)
     * 101  = QUEUE_TIMEOUT
     * 429  = RATE_LIMITED
     * other = legacy error string in {@link #error}
     */
    private int code;
    private String gateIp;
    private int gatePort;
    private byte[] tokenPayload;
    private byte[] tokenSignature;
    private long tokenDeadline;
    private String error;

    // queue payload (only populated when code=100/101)
    private Long retryAfterMs;
    private Long queuePos;

    public static AssignGateResponse queueing(long retryAfterMs, long queuePos) {
        AssignGateResponse r = new AssignGateResponse();
        r.code = 100;
        r.retryAfterMs = retryAfterMs;
        r.queuePos = queuePos;
        return r;
    }

    public static AssignGateResponse rateLimited(String reason) {
        AssignGateResponse r = new AssignGateResponse();
        r.code = 429;
        r.error = reason;
        return r;
    }

    public int getCode() { return code; }
    public void setCode(int code) { this.code = code; }

    public String getGateIp() { return gateIp; }
    public void setGateIp(String gateIp) { this.gateIp = gateIp; }

    public int getGatePort() { return gatePort; }
    public void setGatePort(int gatePort) { this.gatePort = gatePort; }

    public byte[] getTokenPayload() { return tokenPayload; }
    public void setTokenPayload(byte[] tokenPayload) { this.tokenPayload = tokenPayload; }

    public byte[] getTokenSignature() { return tokenSignature; }
    public void setTokenSignature(byte[] tokenSignature) { this.tokenSignature = tokenSignature; }

    public long getTokenDeadline() { return tokenDeadline; }
    public void setTokenDeadline(long tokenDeadline) { this.tokenDeadline = tokenDeadline; }

    public String getError() { return error; }
    public void setError(String error) { this.error = error; }

    public Long getRetryAfterMs() { return retryAfterMs; }
    public void setRetryAfterMs(Long retryAfterMs) { this.retryAfterMs = retryAfterMs; }

    public Long getQueuePos() { return queuePos; }
    public void setQueuePos(Long queuePos) { this.queuePos = queuePos; }
}
