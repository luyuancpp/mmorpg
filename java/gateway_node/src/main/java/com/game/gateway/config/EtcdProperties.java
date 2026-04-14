package com.game.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

import java.util.List;

@Configuration
@ConfigurationProperties(prefix = "etcd")
public class EtcdProperties {

    private List<String> endpoints = List.of("http://127.0.0.1:2379");
    private long dialTimeoutMs = 5000;

    public List<String> getEndpoints() { return endpoints; }
    public void setEndpoints(List<String> endpoints) { this.endpoints = endpoints; }

    public long getDialTimeoutMs() { return dialTimeoutMs; }
    public void setDialTimeoutMs(long dialTimeoutMs) { this.dialTimeoutMs = dialTimeoutMs; }
}
