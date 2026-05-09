package com.game.gateway.ratelimit;

import org.junit.jupiter.api.Test;

import java.time.Instant;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Pure-logic test for the wave schedule. No Spring context, no Redis.
 *
 * <p>The scheduler's contract is "given a zone id and a wall-clock instant,
 * decide whether the zone is currently allowed to log in." We pin the
 * baseline to a fixed epoch in the past so {@code Instant.now()} comparisons
 * are deterministic without time mocking.
 */
class WaveScheduleTest {

    /** Build a schedule with a baseline {@code offsetFromNowSec} seconds ago. */
    private WaveSchedule build(boolean enabled, long offsetFromNowSec, List<RateLimitProperties.WaveStep> steps) {
        RateLimitProperties.WaveSchedule cfg = new RateLimitProperties.WaveSchedule();
        cfg.setEnabled(enabled);
        cfg.setSchedule(steps);
        long base = Instant.now().getEpochSecond() - offsetFromNowSec;
        cfg.setStartEpochSec(base);
        return new WaveSchedule(cfg, base);
    }

    private RateLimitProperties.WaveStep step(long offsetSec, Long... zones) {
        var s = new RateLimitProperties.WaveStep();
        s.setOffsetSec(offsetSec);
        s.setAllowZones(List.of(zones));
        return s;
    }

    @Test
    void disabled_alwaysOpen() {
        WaveSchedule s = build(false, 0, List.of());
        assertTrue(s.isOpen(1));
        assertTrue(s.isOpen(99));
        assertEquals(0, s.secondsUntilOpen(1));
    }

    @Test
    void emptySchedule_alwaysOpen_evenIfEnabled() {
        WaveSchedule s = build(true, 0, List.of());
        assertTrue(s.isOpen(7));
    }

    @Test
    void zoneInCurrentWave_isOpen() {
        // 60s elapsed; step at offsetSec=0 allows zones [1,2]
        WaveSchedule s = build(true, 60, List.of(step(0, 1L, 2L)));
        assertTrue(s.isOpen(1));
        assertTrue(s.isOpen(2));
    }

    @Test
    void zoneNotInCurrentWave_queuesUntilNextStep() {
        // baseline 10s ago; current step (offset 0) allows [1]; next step at offset 30 allows [2]
        WaveSchedule s = build(true, 10, List.of(
                step(0,  1L),
                step(30, 2L)
        ));
        assertTrue(s.isOpen(1));
        assertFalse(s.isOpen(2));
        // 20 seconds remain until step 2 unlocks zone 2
        long until = s.secondsUntilOpen(2);
        assertTrue(until >= 19 && until <= 21, "expected ~20s, got " + until);
    }

    @Test
    void scheduleNotStartedYet_allZonesQueued() {
        // baseline 30s in the future
        RateLimitProperties.WaveSchedule cfg = new RateLimitProperties.WaveSchedule();
        cfg.setEnabled(true);
        cfg.setStartEpochSec(Instant.now().getEpochSecond() + 30);
        cfg.setSchedule(List.of(step(0, 1L)));
        WaveSchedule s = new WaveSchedule(cfg, cfg.getStartEpochSec());
        assertFalse(s.isOpen(1));
    }

    @Test
    void minusOneZone_meansAllZones() {
        // After 120s the wildcard step opens every zone
        WaveSchedule s = build(true, 120, List.of(
                step(0,   1L),
                step(120, -1L)
        ));
        assertTrue(s.isOpen(1));
        assertTrue(s.isOpen(42));   // wildcard catches 42
    }

    @Test
    void laterStepsOverrideEarlier() {
        // Two steps at offset 0 and 30, both elapsed: the latest one wins.
        // First step opens zone 1; later step opens zone 2 only -> zone 1 closes.
        WaveSchedule s = build(true, 60, List.of(
                step(0,  1L),
                step(30, 2L)
        ));
        assertFalse(s.isOpen(1));
        assertTrue(s.isOpen(2));
    }
}
