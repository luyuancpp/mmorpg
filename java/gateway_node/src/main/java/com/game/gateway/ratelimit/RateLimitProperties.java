package com.game.gateway.ratelimit;

import org.springframework.boot.context.properties.ConfigurationProperties;

import java.util.List;
import java.util.Map;

/**
 * Rate-limit & wave-schedule config for /api/login and /api/assign-gate.
 *
 * Design: docs/design/open-server-rate-limit-design.md
 *
 * Default is <b>disabled</b> — limiter acts as a pass-through until operations
 * explicitly turn it on per-zone.
 */
@ConfigurationProperties(prefix = "gate.rate-limit")
public class RateLimitProperties {

    /** Master switch. false -> limiter is a no-op (fail-open). */
    private boolean enabled = false;

    /** Default tokens/sec refill for a zone bucket when no override is set. */
    private long zoneDefaultRps = 500;

    /** Default burst capacity for a zone bucket. */
    private long zoneDefaultBurst = 1000;

    /** Per-zone overrides, e.g. {"1": {rps:2000, burst:5000}}. */
    private Map<Long, ZoneLimit> zoneOverrides = Map.of();

    /** Per-IP requests per second. */
    private long ipRps = 2;

    /** Per-IP burst capacity. */
    private long ipBurst = 10;

    /** Same-account cooldown window (ms) between /api/login calls. */
    private long accountCooldownMs = 5000;

    /** Hard queue timeout (ms). After this the client gets QUEUE_TIMEOUT. */
    private long queueTimeoutMs = 300_000;

    /**
     * Open-server wave schedule. When present, zones outside the current wave
     * are forced to queue regardless of remaining token count.
     *
     * Example:
     *   schedule:
     *     - { offsetSec: 0,   allowZones: [1, 2] }
     *     - { offsetSec: 30,  allowZones: [3, 4] }
     *     - { offsetSec: 120, allowZones: ALL }   # -1 means "all zones"
     */
    private WaveSchedule wave = new WaveSchedule();

    public static class ZoneLimit {
        private long rps;
        private long burst;
        public long getRps() { return rps; }
        public void setRps(long rps) { this.rps = rps; }
        public long getBurst() { return burst; }
        public void setBurst(long burst) { this.burst = burst; }
    }

    public static class WaveSchedule {
        private boolean enabled = false;
        /** Epoch-seconds of the wave baseline. 0 = use application boot time. */
        private long startEpochSec = 0;
        private List<WaveStep> schedule = List.of();

        public boolean isEnabled() { return enabled; }
        public void setEnabled(boolean enabled) { this.enabled = enabled; }
        public long getStartEpochSec() { return startEpochSec; }
        public void setStartEpochSec(long startEpochSec) { this.startEpochSec = startEpochSec; }
        public List<WaveStep> getSchedule() { return schedule; }
        public void setSchedule(List<WaveStep> schedule) { this.schedule = schedule; }
    }

    public static class WaveStep {
        private long offsetSec;
        /** List of zone ids. Use the single value -1 to mean "all zones". */
        private List<Long> allowZones = List.of();

        public long getOffsetSec() { return offsetSec; }
        public void setOffsetSec(long offsetSec) { this.offsetSec = offsetSec; }
        public List<Long> getAllowZones() { return allowZones; }
        public void setAllowZones(List<Long> allowZones) { this.allowZones = allowZones; }
    }

    // getters / setters
    public boolean isEnabled() { return enabled; }
    public void setEnabled(boolean enabled) { this.enabled = enabled; }
    public long getZoneDefaultRps() { return zoneDefaultRps; }
    public void setZoneDefaultRps(long zoneDefaultRps) { this.zoneDefaultRps = zoneDefaultRps; }
    public long getZoneDefaultBurst() { return zoneDefaultBurst; }
    public void setZoneDefaultBurst(long zoneDefaultBurst) { this.zoneDefaultBurst = zoneDefaultBurst; }
    public Map<Long, ZoneLimit> getZoneOverrides() { return zoneOverrides; }
    public void setZoneOverrides(Map<Long, ZoneLimit> zoneOverrides) { this.zoneOverrides = zoneOverrides; }
    public long getIpRps() { return ipRps; }
    public void setIpRps(long ipRps) { this.ipRps = ipRps; }
    public long getIpBurst() { return ipBurst; }
    public void setIpBurst(long ipBurst) { this.ipBurst = ipBurst; }
    public long getAccountCooldownMs() { return accountCooldownMs; }
    public void setAccountCooldownMs(long accountCooldownMs) { this.accountCooldownMs = accountCooldownMs; }
    public long getQueueTimeoutMs() { return queueTimeoutMs; }
    public void setQueueTimeoutMs(long queueTimeoutMs) { this.queueTimeoutMs = queueTimeoutMs; }
    public WaveSchedule getWave() { return wave; }
    public void setWave(WaveSchedule wave) { this.wave = wave; }
}
