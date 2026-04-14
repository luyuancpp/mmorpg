package com.game.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

@Configuration
@ConfigurationProperties(prefix = "gate")
public class GateProperties {

    private String tokenSecret = "change-me";
    private int tokenTtlSeconds = 60;
    private long discoveryTimeoutMs = 5000;

    public String getTokenSecret() { return tokenSecret; }
    public void setTokenSecret(String tokenSecret) { this.tokenSecret = tokenSecret; }

    public int getTokenTtlSeconds() { return tokenTtlSeconds; }
    public void setTokenTtlSeconds(int tokenTtlSeconds) { this.tokenTtlSeconds = tokenTtlSeconds; }

    public long getDiscoveryTimeoutMs() { return discoveryTimeoutMs; }
    public void setDiscoveryTimeoutMs(long discoveryTimeoutMs) { this.discoveryTimeoutMs = discoveryTimeoutMs; }
}
