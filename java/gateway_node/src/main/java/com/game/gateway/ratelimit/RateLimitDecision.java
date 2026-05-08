package com.game.gateway.ratelimit;

/**
 * Rate-limit decision returned by {@link AssignGateRateLimiter}.
 *
 * Three terminal states:
 * <ul>
 *   <li><b>PASS</b>   — request may proceed.</li>
 *   <li><b>QUEUE</b>  — over capacity or outside current wave; client should retry.</li>
 *   <li><b>DENY</b>   — hard reject (IP cap, account cooldown).</li>
 * </ul>
 */
public final class RateLimitDecision {

    public enum Kind { PASS, QUEUE, DENY }

    private final Kind kind;
    private final String reason;
    private final long retryAfterMs;
    private final long queuePosEstimate;

    private RateLimitDecision(Kind kind, String reason, long retryAfterMs, long queuePosEstimate) {
        this.kind = kind;
        this.reason = reason;
        this.retryAfterMs = retryAfterMs;
        this.queuePosEstimate = queuePosEstimate;
    }

    public static RateLimitDecision pass() {
        return new RateLimitDecision(Kind.PASS, null, 0, 0);
    }

    public static RateLimitDecision queue(long retryAfterMs, long queuePosEstimate) {
        return new RateLimitDecision(Kind.QUEUE, "QUEUEING", retryAfterMs, queuePosEstimate);
    }

    public static RateLimitDecision deny(String reason) {
        return new RateLimitDecision(Kind.DENY, reason, 0, 0);
    }

    public Kind getKind() { return kind; }
    public String getReason() { return reason; }
    public long getRetryAfterMs() { return retryAfterMs; }
    public long getQueuePosEstimate() { return queuePosEstimate; }

    public boolean isPass()  { return kind == Kind.PASS; }
    public boolean isQueue() { return kind == Kind.QUEUE; }
    public boolean isDeny()  { return kind == Kind.DENY; }
}
