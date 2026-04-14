package com.game.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

@Configuration
@ConfigurationProperties(prefix = "zone-probe")
public class ZoneProbeProperties {

    private long intervalMs = 5000;
    private int gateStaleThresholdSeconds = 30;

    public long getIntervalMs() { return intervalMs; }
    public void setIntervalMs(long intervalMs) { this.intervalMs = intervalMs; }

    public int getGateStaleThresholdSeconds() { return gateStaleThresholdSeconds; }
    public void setGateStaleThresholdSeconds(int s) { this.gateStaleThresholdSeconds = s; }
}
