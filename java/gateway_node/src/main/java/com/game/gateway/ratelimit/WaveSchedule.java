package com.game.gateway.ratelimit;

import java.time.Instant;
import java.util.List;

/**
 * Evaluates the open-server wave schedule: decides whether a given zone is
 * currently "open" for login, and if not, how many seconds until it opens.
 *
 * <p>Immutable. Built from {@link RateLimitProperties.WaveSchedule} at startup.
 * Values are read from a single volatile reference so an admin reload can swap
 * the whole schedule atomically.
 *
 * <p>Sentinel: an allowZones entry containing {@code -1} means "all zones".
 */
public final class WaveSchedule {

    private final boolean enabled;
    private final long startEpochSec;
    private final List<RateLimitProperties.WaveStep> steps;

    public WaveSchedule(RateLimitProperties.WaveSchedule cfg, long defaultStartEpochSec) {
        this.enabled = cfg.isEnabled();
        long s = cfg.getStartEpochSec();
        this.startEpochSec = s > 0 ? s : defaultStartEpochSec;
        // Sort by offsetSec ascending so "current step" is the last one whose
        // offset has elapsed.
        var sorted = cfg.getSchedule().stream()
                .sorted((a, b) -> Long.compare(a.getOffsetSec(), b.getOffsetSec()))
                .toList();
        this.steps = sorted;
    }

    /** Returns true when the zone is allowed to log in right now. */
    public boolean isOpen(long zoneId) {
        if (!enabled || steps.isEmpty()) {
            return true;
        }
        long now = Instant.now().getEpochSecond();
        long elapsed = now - startEpochSec;
        // Find the latest step whose offsetSec <= elapsed.
        RateLimitProperties.WaveStep current = null;
        for (var step : steps) {
            if (step.getOffsetSec() <= elapsed) {
                current = step;
            } else {
                break;
            }
        }
        if (current == null) {
            return false;            // schedule has not started yet
        }
        List<Long> allow = current.getAllowZones();
        if (allow.contains(-1L)) {
            return true;
        }
        return allow.contains(zoneId);
    }

    /** Seconds until the given zone becomes open. 0 if already open. */
    public long secondsUntilOpen(long zoneId) {
        if (isOpen(zoneId)) {
            return 0;
        }
        long now = Instant.now().getEpochSecond();
        for (var step : steps) {
            long stepStart = startEpochSec + step.getOffsetSec();
            if (stepStart <= now) {
                continue;
            }
            var allow = step.getAllowZones();
            if (allow.contains(-1L) || allow.contains(zoneId)) {
                return stepStart - now;
            }
        }
        // Never opens according to current schedule — use a large sentinel
        // so the client retries later rather than loops tightly.
        return 3600;
    }

    public boolean isEnabled() { return enabled; }
}
