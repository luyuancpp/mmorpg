package com.game.gateway.dto;

/**
 * Body for {@code POST /api/queue-status}: client polls this endpoint while
 * waiting in the AssignGate login queue.
 *
 * <p>Single field by design — the queue token is opaque to the client
 * (HMAC-signed server-side blob) and carries everything we need to look
 * up the entry: queueId, zoneId, expiry. Adding more fields here would be
 * a smell that the token isn't actually authoritative.
 */
public class QueueStatusRequest {
    private String queueToken;

    public String getQueueToken() { return queueToken; }
    public void setQueueToken(String queueToken) { this.queueToken = queueToken; }
}
