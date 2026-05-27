package com.game.gateway.dto;

/**
 * Body for {@code POST /api/queue-status}: client polls this endpoint while
 * waiting in the AssignGate login queue.
 *
 * <p>The queue token is opaque to the client (HMAC-signed server-side blob)
 * and carries the lookup state (queueId, zoneId, expiry). The {@code zoneId}
 * field is a routing hint — Gateway uses it to dispatch the poll to the same
 * {@code login.rpc} instance that issued the token, since each instance only
 * watches its own zone's queue state in Redis. Without this, a 3-zone deploy
 * would fan polls round-robin across zone-pinned login instances and 2/3 of
 * them would answer EXPIRED for a token they never issued. See the 2026-05-24
 * stress postmortem §I.
 *
 * <p>Robots set this from their config; production clients echo whatever the
 * matching {@code /api/assign-gate} response carried in {@code zone_id}.
 */
public class QueueStatusRequest {
    private String queueToken;
    private int zoneId;

    public String getQueueToken() { return queueToken; }
    public void setQueueToken(String queueToken) { this.queueToken = queueToken; }
    public int getZoneId() { return zoneId; }
    public void setZoneId(int zoneId) { this.zoneId = zoneId; }
}
