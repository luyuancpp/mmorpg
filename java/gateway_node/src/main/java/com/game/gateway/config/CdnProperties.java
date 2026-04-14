package com.game.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

@Configuration
@ConfigurationProperties(prefix = "cdn")
public class CdnProperties {

    private String baseUrl = "https://cdn.example.com";
    private int signTtlSeconds = 3600;

    public String getBaseUrl() { return baseUrl; }
    public void setBaseUrl(String baseUrl) { this.baseUrl = baseUrl; }

    public int getSignTtlSeconds() { return signTtlSeconds; }
    public void setSignTtlSeconds(int signTtlSeconds) { this.signTtlSeconds = signTtlSeconds; }
}
