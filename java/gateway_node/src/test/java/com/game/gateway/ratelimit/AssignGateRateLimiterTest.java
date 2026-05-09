package com.game.gateway.ratelimit;

import io.github.bucket4j.BucketConfiguration;
import io.github.bucket4j.ConsumptionProbe;
import io.github.bucket4j.distributed.BucketProxy;
import io.github.bucket4j.distributed.proxy.ProxyManager;
import io.github.bucket4j.distributed.proxy.RemoteBucketBuilder;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.mockito.junit.jupiter.MockitoSettings;
import org.mockito.quality.Strictness;

import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

/**
 * Behavioural test for {@link AssignGateRateLimiter}.
 *
 * <p>We mock Bucket4j's {@link ProxyManager} so the test runs without a real
 * Redis. The intent is to pin the four user-visible outcomes:
 * <ul>
 *   <li>limiter disabled -> always pass</li>
 *   <li>wave closed -> queue with retry-after &gt; 0</li>
 *   <li>zone bucket exhausted -> queue</li>
 *   <li>IP bucket exhausted -> deny IP_RATE_LIMIT</li>
 *   <li>account cooldown hit -> deny ACCOUNT_COOLDOWN</li>
 *   <li>backend exception -> fail-open (pass)</li>
 * </ul>
 */
@ExtendWith(MockitoExtension.class)
@MockitoSettings(strictness = Strictness.LENIENT)
class AssignGateRateLimiterTest {

    @Mock
    @SuppressWarnings("unchecked")
    private ProxyManager<byte[]> proxyManager;

    private RateLimitProperties props;

    @BeforeEach
    void setUp() {
        props = new RateLimitProperties();
        props.setEnabled(true);
        props.setZoneDefaultRps(100);
        props.setZoneDefaultBurst(200);
        props.setIpRps(2);
        props.setIpBurst(10);
        props.setAccountCooldownMs(5000);
    }

    /**
     * Stub the proxyManager so zone/ip lookups return the supplied buckets.
     * Both layers go through the same mock; tests can decide order of returned
     * buckets via {@code thenReturn(zone, ip)}.
     *
     * <p>IMPORTANT: callers must build the BucketProxy mocks BEFORE invoking
     * this method. Constructing them inline inside thenReturn(...) leaves
     * Mockito with an unfinished outer stubbing.
     */
    @SuppressWarnings("unchecked")
    private void stubBuckets(BucketProxy zone, BucketProxy ip) {
        RemoteBucketBuilder<byte[]> builder = mock(RemoteBucketBuilder.class);
        when(proxyManager.builder()).thenReturn(builder);
        when(builder.build(any(byte[].class), any(Supplier.class))).thenReturn(zone, ip);
    }

    /**
     * Bucket that lets every {@code tryConsumeAndReturnRemaining(1)} succeed.
     * Used for the zone bucket which is consulted first.
     */
    private static BucketProxy zonePassing() {
        BucketProxy bp = mock(BucketProxy.class);
        ConsumptionProbe ok = ConsumptionProbe.consumed(99, 0);
        when(bp.tryConsumeAndReturnRemaining(1)).thenReturn(ok);
        return bp;
    }

    /** Bucket that always rejects, with the given wait/remaining numbers. */
    private static BucketProxy zoneRejecting(long nanosToWait, long remaining) {
        BucketProxy bp = mock(BucketProxy.class);
        ConsumptionProbe rej = ConsumptionProbe.rejected(remaining, nanosToWait, nanosToWait);
        when(bp.tryConsumeAndReturnRemaining(1)).thenReturn(rej);
        return bp;
    }

    /** Bucket that lets every {@code tryConsume(1)} succeed (the IP layer). */
    private static BucketProxy ipPassing() {
        BucketProxy bp = mock(BucketProxy.class);
        when(bp.tryConsume(1)).thenReturn(true);
        return bp;
    }

    /** Bucket that always denies {@code tryConsume(1)} (the IP layer). */
    private static BucketProxy ipRejecting() {
        BucketProxy bp = mock(BucketProxy.class);
        when(bp.tryConsume(1)).thenReturn(false);
        return bp;
    }

    private WaveSchedule disabledWave() {
        var cfg = new RateLimitProperties.WaveSchedule();
        cfg.setEnabled(false);
        return new WaveSchedule(cfg, Instant.now().getEpochSecond());
    }

    @Test
    void disabledLimiter_alwaysPasses() {
        props.setEnabled(false);
        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());

        var d = limiter.check(1, "10.0.0.1", "alice");
        assertTrue(d.isPass());
        verifyNoInteractions(proxyManager);
    }

    @Test
    void nullProxyManager_alwaysPasses() {
        AssignGateRateLimiter limiter = new AssignGateRateLimiter(null, props, disabledWave());
        assertTrue(limiter.check(1, "10.0.0.1", "alice").isPass());
    }

    @Test
    void waveClosed_queuesWithRetryAfter() {
        // Wave starts 30s in the future; step 0 only allows zone 1.
        var cfg = new RateLimitProperties.WaveSchedule();
        cfg.setEnabled(true);
        cfg.setStartEpochSec(Instant.now().getEpochSecond() + 30);
        var step = new RateLimitProperties.WaveStep();
        step.setOffsetSec(0);
        step.setAllowZones(List.of(1L));
        cfg.setSchedule(List.of(step));
        WaveSchedule wave = new WaveSchedule(cfg, cfg.getStartEpochSec());

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, wave);
        var d = limiter.check(2, "10.0.0.2", "bob");

        assertTrue(d.isQueue());
        assertTrue(d.getRetryAfterMs() > 0, "retry-after should be positive when wave is closed");
        verifyNoInteractions(proxyManager);
    }

    @Test
    void zoneBucketExhausted_queues() {
        // Zone rejects -> limiter short-circuits, the IP bucket is never reached.
        // Build the rejecting bucket FIRST so its inner when() finishes before
        // we open the builder's when() — otherwise Mockito flags an unfinished stub.
        BucketProxy zoneRej = zoneRejecting(2_500_000_000L, 0);
        @SuppressWarnings("unchecked")
        RemoteBucketBuilder<byte[]> builder = mock(RemoteBucketBuilder.class);
        when(proxyManager.builder()).thenReturn(builder);
        when(builder.build(any(byte[].class), any(Supplier.class))).thenReturn(zoneRej);

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());
        var d = limiter.check(1, "10.0.0.3", "carol");

        assertTrue(d.isQueue());
        assertEquals(2500, d.getRetryAfterMs());
    }

    @Test
    void ipBucketExhausted_denied() {
        stubBuckets(zonePassing(), ipRejecting());

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());
        var d = limiter.check(1, "10.0.0.4", "dave");

        assertTrue(d.isDeny());
        assertEquals("IP_RATE_LIMIT", d.getReason());
    }

    @Test
    void accountCooldownHit_secondCallDenied() {
        // Pre-build all bucket mocks BEFORE opening the outer when(); otherwise
        // helper-internal when() calls land inside an unfinished stubbing chain.
        BucketProxy z1 = zonePassing();
        BucketProxy i1 = ipPassing();
        BucketProxy z2 = zonePassing();
        BucketProxy i2 = ipPassing();
        @SuppressWarnings("unchecked")
        RemoteBucketBuilder<byte[]> builder = mock(RemoteBucketBuilder.class);
        when(proxyManager.builder()).thenReturn(builder);
        when(builder.build(any(byte[].class), any(Supplier.class))).thenReturn(z1, i1, z2, i2);

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());

        assertTrue(limiter.check(1, "10.0.0.5", "eve").isPass());
        var second = limiter.check(1, "10.0.0.5", "eve");

        assertTrue(second.isDeny());
        assertEquals("ACCOUNT_COOLDOWN", second.getReason());
    }

    @Test
    void backendException_failsOpen() {
        when(proxyManager.builder()).thenThrow(new RuntimeException("redis down"));

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());
        var d = limiter.check(1, "10.0.0.6", "frank");

        assertTrue(d.isPass(), "limiter must fail-open on backend errors");
    }

    @Test
    void zeroZoneSkipsZoneBucket() {
        // zoneId=0 -> only the ip bucket should be consulted.
        BucketProxy ip = ipPassing();
        @SuppressWarnings("unchecked")
        RemoteBucketBuilder<byte[]> builder = mock(RemoteBucketBuilder.class);
        when(proxyManager.builder()).thenReturn(builder);
        when(builder.build(any(byte[].class), any(Supplier.class))).thenReturn(ip);

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());
        assertTrue(limiter.check(0, "10.0.0.7", "grace").isPass());

        // Exactly one bucket build call (the ip bucket).
        verify(builder, times(1)).build(any(byte[].class), any(Supplier.class));
    }

    @Test
    void zoneOverride_used() {
        // Per-zone override doesn't change pass/fail behaviour in this test (the
        // mocked bucket is what decides), but it must not blow up on construction.
        var override = new RateLimitProperties.ZoneLimit();
        override.setRps(2000);
        override.setBurst(5000);
        props.setZoneOverrides(Map.of(1L, override));

        stubBuckets(zonePassing(), ipPassing());

        AssignGateRateLimiter limiter = new AssignGateRateLimiter(proxyManager, props, disabledWave());
        assertTrue(limiter.check(1, "10.0.0.8", "henry").isPass());
    }
}
