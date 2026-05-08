package com.game.gateway.ratelimit;

import io.github.bucket4j.distributed.proxy.ProxyManager;
import io.github.bucket4j.redis.lettuce.cas.LettuceBasedProxyManager;
import io.lettuce.core.RedisClient;
import io.lettuce.core.RedisURI;
import io.lettuce.core.api.StatefulRedisConnection;
import io.lettuce.core.codec.ByteArrayCodec;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

/**
 * Wires up Bucket4j over the same Redis the rest of the gateway uses.
 *
 * <p>Only activates when {@code gate.rate-limit.enabled=true}. When disabled
 * the limiter is constructed with a null ProxyManager and falls back to a
 * pass-through (see {@link AssignGateRateLimiter#check}).
 *
 * <p>Why a separate Lettuce client (instead of reusing
 * {@code spring-boot-starter-data-redis}'s connection): Spring's auto-config
 * gives a {@code RedisConnectionFactory}/{@code LettuceConnectionFactory},
 * but Bucket4j's Lettuce integration wants a raw
 * {@link StatefulRedisConnection}. Building a small dedicated Lettuce client
 * is the documented path and keeps Bucket4j's wire format isolated from any
 * Spring serialization the rest of the app may layer on later.
 */
@Configuration
@EnableConfigurationProperties(RateLimitProperties.class)
public class RateLimitConfig {

    private static final Logger log = LoggerFactory.getLogger(RateLimitConfig.class);

    @Value("${spring.data.redis.host:localhost}")
    private String redisHost;

    @Value("${spring.data.redis.port:6379}")
    private int redisPort;

    @Value("${spring.data.redis.password:}")
    private String redisPassword;

    @Bean(destroyMethod = "shutdown")
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public RedisClient bucket4jRedisClient() {
        RedisURI.Builder builder = RedisURI.builder().withHost(redisHost).withPort(redisPort);
        if (redisPassword != null && !redisPassword.isBlank()) {
            builder.withPassword(redisPassword.toCharArray());
        }
        log.info("Bucket4j Redis client -> {}:{}", redisHost, redisPort);
        return RedisClient.create(builder.build());
    }

    @Bean(destroyMethod = "close")
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public StatefulRedisConnection<byte[], byte[]> bucket4jConnection(RedisClient client) {
        return client.connect(ByteArrayCodec.INSTANCE);
    }

    @Bean
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public ProxyManager<byte[]> bucket4jProxyManager(StatefulRedisConnection<byte[], byte[]> conn) {
        return LettuceBasedProxyManager.builderFor(conn).build();
    }

    @Bean
    public AssignGateRateLimiter assignGateRateLimiter(
            org.springframework.beans.factory.ObjectProvider<ProxyManager<byte[]>> proxyManager,
            RateLimitProperties props) {
        long boot = System.currentTimeMillis() / 1000L;
        WaveSchedule wave = new WaveSchedule(props.getWave(), boot);
        ProxyManager<byte[]> pm = proxyManager.getIfAvailable();
        if (pm == null) {
            log.info("RateLimiter disabled (gate.rate-limit.enabled=false) — pass-through mode");
        }
        return new AssignGateRateLimiter(pm, props, wave);
    }
}
