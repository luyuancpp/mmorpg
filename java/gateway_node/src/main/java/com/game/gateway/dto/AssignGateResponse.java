package com.game.gateway.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class AssignGateResponse {
    private String gateIp;
    private int gatePort;
    private byte[] tokenPayload;
    private byte[] tokenSignature;
    private long tokenDeadline;
    private String error;

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
}
