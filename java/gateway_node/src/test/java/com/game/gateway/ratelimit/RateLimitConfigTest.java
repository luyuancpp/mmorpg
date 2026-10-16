package com.game.gateway.ratelimit;

import io.github.bucket4j.distributed.proxy.ClientSideConfig;
import io.lettuce.core.ClientOptions;
import io.lettuce.core.RedisClient;
import io.lettuce.core.RedisURI;
import io.lettuce.core.TimeoutOptions;
import org.junit.jupiter.api.Test;
import org.springframework.test.util.ReflectionTestUtils;

import java.time.Duration;
import java.util.Optional;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * 钉住限流专用 Lettuce 连接的超时与断线语义,以及 Bucket4j 整次桶操作的时限(2026-10-15)。
 *
 * <p>为什么要钉:限流在 Tomcat 请求线程上同步调用,Bucket4j 的 Lettuce 后端在不配请求超时时对异步命令做无超时的等待。
 * 这里任何一项退回 Lettuce 缺省(连接缺省超时 60s、不开命令超时、断线时缓存命令等重连),Redis 卡住或断线时
 * 请求线程就会被逐个占住,直到线程池耗尽、liveness 超时、两个副本一起被杀,而不是按设计 fail-open 放行
 * (原因详见 {@link RateLimitConfig#REDIS_COMMAND_TIMEOUT})。
 *
 * <p>不起 Spring 上下文、不连 Redis。ProxyManager 构建链上的 withClientSideConfig 接线没有用例钉住
 * (要构造 Bucket4j 的 Lettuce 后端),靠评审与真实环境验证(spec_G4b §9 R5 更正注)。
 */
class RateLimitConfigTest {

    private static final Duration EXPECTED_TIMEOUT = Duration.ofMillis(300);

    @Test
    void commandTimeoutIs300ms() {
        assertEquals(EXPECTED_TIMEOUT, RateLimitConfig.REDIS_COMMAND_TIMEOUT,
                "限流 Redis 命令超时应为 300ms;改值要同步评估 fail-open 前每个请求多等的时长");
    }

    @Test
    void redisUri_carriesCommandTimeoutInsteadOfLettuceDefault60s() {
        RedisURI uri = RateLimitConfig.bucket4jRedisUri("redis.infra", 6379, "");

        assertEquals("redis.infra", uri.getHost());
        assertEquals(6379, uri.getPort());
        assertEquals(EXPECTED_TIMEOUT, uri.getTimeout(),
                "RedisURI 不写 timeout 时是 Lettuce 缺省 60s");
    }

    @Test
    @SuppressWarnings("deprecation") // getPassword 在 Lettuce 6.x 标了弃用,这里只用来确认口令是否写上
    void redisUri_setsPasswordOnlyWhenNonBlank() {
        assertNull(RateLimitConfig.bucket4jRedisUri("h", 1, "").getPassword(), "空口令不应设置(dev 档 Redis 无口令)");
        assertNull(RateLimitConfig.bucket4jRedisUri("h", 1, "   ").getPassword(), "空白口令不应设置");
        assertNull(RateLimitConfig.bucket4jRedisUri("h", 1, null).getPassword(), "null 口令不应设置");
        assertArrayEquals("s3cret".toCharArray(), RateLimitConfig.bucket4jRedisUri("h", 1, "s3cret").getPassword(),
                "非空口令必须原样设置,否则 release 档 Redis 认证失败、限流一律 fail-open");
    }

    @Test
    void clientOptions_rejectCommandsWhileDisconnected() {
        ClientOptions options = RateLimitConfig.bucket4jClientOptions();

        assertEquals(ClientOptions.DisconnectedBehavior.REJECT_COMMANDS, options.getDisconnectedBehavior(),
                "断线时命令必须立即失败走 fail-open,不能进缓冲等重连");
        assertTrue(options.isAutoReconnect(), "自动重连必须保持开启,Redis 恢复后限流才能自动恢复");
    }

    @Test
    void clientOptions_enableFixedCommandTimeout() {
        TimeoutOptions timeouts = RateLimitConfig.bucket4jClientOptions().getTimeoutOptions();

        assertTrue(timeouts.isTimeoutCommands(),
                "必须对所有命令开超时:Lettuce 缺省不开,Bucket4j 的异步等待又没有上限");
        TimeoutOptions.TimeoutSource source = timeouts.getSource();
        // 固定超时源不看具体命令,传 null 即可取到配置值。
        long timeoutMillis = source.getTimeUnit().toMillis(source.getTimeout(null));
        assertEquals(EXPECTED_TIMEOUT.toMillis(), timeoutMillis, "命令超时应为固定 300ms");
    }

    @Test
    void clientOptions_boundPendingCommandQueue() {
        assertEquals(1024, RateLimitConfig.bucket4jClientOptions().getRequestQueueSize(),
                "协议栈里未完成命令的上限应为 1024:连接半开时超时命令不出栈,不设上限会在 384m 的堆里无界堆积");
    }

    @Test
    void redisClientBean_appliesTheseOptions() {
        // 上面几例只钉工厂函数的返回值;这一例钉住接线:删掉 bucket4jRedisClient 里的 setOptions,
        // 运行时就退回 Lettuce 缺省(断线缓存命令、不开命令超时),而前面几例照样全绿。
        // RedisClient.create 不建连,这里不需要 Redis。
        RateLimitConfig config = new RateLimitConfig();
        ReflectionTestUtils.setField(config, "redisHost", "localhost");
        ReflectionTestUtils.setField(config, "redisPort", 6379);
        ReflectionTestUtils.setField(config, "redisPassword", "");
        RedisClient client = config.bucket4jRedisClient();
        try {
            ClientOptions options = client.getOptions();
            assertEquals(ClientOptions.DisconnectedBehavior.REJECT_COMMANDS, options.getDisconnectedBehavior(),
                    "bucket4jRedisClient 必须把限流专用选项设到客户端上");
            assertTrue(options.getTimeoutOptions().isTimeoutCommands(), "客户端上必须开命令超时");
            assertEquals(1024, options.getRequestQueueSize(), "客户端上必须带未完成命令上限");
        } finally {
            client.shutdown();
        }
    }

    @Test
    void bucket4jClientSideConfig_boundsWholeBucketOperation() {
        ClientSideConfig config = RateLimitConfig.bucket4jClientSideConfig();

        assertEquals(Optional.of(Duration.ofMillis(500).toNanos()), config.getRequestTimeoutNanos(),
                "Bucket4j 整次桶操作时限应为 500ms;不设时 Bucket4j 对 Redis 回包做无超时等待,CAS 重试也没有上限");
        assertTrue(RateLimitConfig.BUCKET4J_REQUEST_TIMEOUT.compareTo(RateLimitConfig.REDIS_COMMAND_TIMEOUT) > 0,
                "整次桶操作时限必须大于单条命令超时,否则一次读状态 + CAS 写回连一条命令的超时都容不下");
    }

    @Test
    void bucket4jClientSideConfig_leavesExpirationToBuilder() {
        // Bucket4j 8.10.1 构建时优先取 ClientSideConfig 里的过期策略;这里一旦带上(哪怕是 none),
        // bucket4jProxyManager 里 withExpirationStrategy 给的 TTL 就被绕开,rl:ip:* / rl:zone:* 会退回永久 key。
        assertTrue(RateLimitConfig.bucket4jClientSideConfig().getExpirationAfterWriteStrategy().isEmpty(),
                "ClientSideConfig 不得携带过期策略,过期策略只由构建器的 withExpirationStrategy 给");
    }
}
