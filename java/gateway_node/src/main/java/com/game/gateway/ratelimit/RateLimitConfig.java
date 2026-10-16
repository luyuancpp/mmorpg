package com.game.gateway.ratelimit;

import io.github.bucket4j.distributed.ExpirationAfterWriteStrategy;
import io.github.bucket4j.distributed.proxy.ClientSideConfig;
import io.github.bucket4j.distributed.proxy.ProxyManager;
import io.github.bucket4j.redis.lettuce.cas.LettuceBasedProxyManager;
import io.lettuce.core.ClientOptions;
import io.lettuce.core.RedisClient;
import io.lettuce.core.RedisURI;
import io.lettuce.core.TimeoutOptions;
import io.lettuce.core.api.StatefulRedisConnection;
import io.lettuce.core.codec.ByteArrayCodec;
import java.time.Duration;
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

    /**
     * 限流器每条 Redis 命令的等待上限(2026-10-15)。
     *
     * <p>限流在 Tomcat 请求线程上同步调用(LoginController、AssignGateController),
     * 而 Bucket4j 8.10.1 的 Lettuce 后端在没配 ClientSideConfig 请求超时时对异步命令做无超时的
     * {@code RedisFuture.get()}(现已配上,见 {@link #BUCKET4J_REQUEST_TIMEOUT});Lettuce 6.4.2 缺省又不开命令超时(TimeoutOptions.DEFAULT_TIMEOUT_COMMANDS=false),
     * 断线时还会把命令缓存起来等重连(DisconnectedBehavior.DEFAULT)(以上三点按两个 jar 的字节码核对)。三者叠加:
     * Redis 卡住、断线或连接半开时,每个 login / assign-gate 请求都挂在这里,请求线程被逐个占住直到 Tomcat 线程池耗尽,
     * 连不碰 Redis 的端点和 liveness 一起超时,两个副本同时被杀 —— 与 fail-open 的承诺(Redis 异常 → 放行)正好相反。
     *
     * <p>取 300ms(经验值,待压测确认):同集群 Redis 正常往返是亚毫秒级,300ms 只在 Redis 已经不正常时触发;超时以异常形式落进
     * {@link AssignGateRateLimiter#check} 现有的 catch,按 fail-open 放行。代价:Redis 完全不回包时每个请求多等约
     * 300ms(第一次桶操作的首条命令超时即放行)并打一条 WARN;Redis 慢而未死时最坏约 1s(见 {@link #BUCKET4J_REQUEST_TIMEOUT});
     * 连接半开(没有 RST)时 Lettuce 不会因命令超时而重连,要等内核判定连接死亡后才自动重连,在那之前限流持续 fail-open。
     *
     * <p>这个值同时写进 RedisURI,Lettuce 用它约束建连后的握手(HELLO / AUTH):启动与自动重连时握手超过 300ms
     * 即算失败 —— 启动期失败即拒启(spec_G4b §9 R5 的启动期依赖,懒连接留作后续),重连失败按退避再试。
     *
     * <p>剩余风险:① 超时只把每个请求的占用压到约 300ms(慢而未死时约 1s),压不掉总量 —— 按 Tomcat 缺省 200 线程估算,
     * 单副本 login + assign-gate 合计在 Redis 完全卡死时超过约 600 次/秒、慢而未死时超过约 200 次/秒,仍会占满线程,
     * 根治要靠熔断(连续失败后短路一段时间;Java 版已有,本仓库未做,留作后续);
     * ② 超时的命令仍留在 Lettuce 的协议栈里等回包,连接半开期间会随请求量累积直到内核判死连接(有未确认数据时
     * Linux 缺省重传约 15 分钟)—— 已由 {@link #bucket4jClientOptions()} 的 requestQueueSize 设上限,超限的命令立即失败、
     * 同样走 fail-open,不会在 384m 的堆里无界堆积。
     */
    static final Duration REDIS_COMMAND_TIMEOUT = Duration.ofMillis(300);

    /**
     * Bucket4j 一次桶操作(读状态 + CAS 写回,CAS 冲突时重试)的总时限(2026-10-15)。
     *
     * <p>Bucket4j 8.10.1 的 ClientSideConfig 支持请求超时:从操作开始起算一个截止时间,每次等 Redis 回包只等剩余时间,
     * 到点抛 {@code io.github.bucket4j.TimeoutException}(RuntimeException 子类),同样落进
     * {@link AssignGateRateLimiter#check} 的 catch 走 fail-open。{@link #REDIS_COMMAND_TIMEOUT} 只管单条命令,
     * 管不住 CAS 冲突时的反复重试;这里补一个整次操作的上限。
     *
     * <p>取 500ms(待压测确认):一次桶操作至少两个往返,要容得下一条命令用满 300ms 的命令超时再加余量,
     * 正常情况下(往返亚毫秒)要冲突重试上百次才会触发。代价:Redis 慢而未死时,一次 check(zone 桶 + IP 桶两次桶操作)
     * 最坏约 1s。
     *
     * <p>负载下的语义变化(spec_G4b §9 R4):以前 {@code rl:zone:<id>} 热键 CAS 争用或 Redis 饱和只会让请求变慢、限流仍有效;
     * 现在超过本时限(或单条命令超过 {@link #REDIS_COMMAND_TIMEOUT})就按异常 fail-open 放行 —— 这个请求不扣 zone / IP 桶,
     * 也跳过账号冷却(catch 在冷却判定之前返回)。开服洪峰正是 zone 桶最可能这样漏的时候。这是「Redis 异常 → 放行」取舍的
     * 一部分;L-14 压测时在 Redis 健康的前提下统计 {@code rate-limit fail-open} 的 WARN 数,非零说明 500ms 偏小或热键已成瓶颈。
     */
    static final Duration BUCKET4J_REQUEST_TIMEOUT = Duration.ofMillis(500);

    /**
     * 限流专用连接在协议栈里最多挂多少条未完成的命令(2026-10-15)。正常时单副本在途命令不超过 Tomcat 线程数(缺省 200),
     * 1024 不会误触;连接半开时超时的命令不会出栈,没有上限就会随请求量无界堆积。超限的命令由 Lettuce 立即以
     * RedisException 失败,落进 {@link AssignGateRateLimiter#check} 的 catch 走 fail-open。
     */
    static final int REDIS_REQUEST_QUEUE_SIZE = 1024;

    @Bean(destroyMethod = "shutdown")
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public RedisClient bucket4jRedisClient() {
        log.info("Bucket4j Redis client -> {}:{} (command timeout {} ms, commands rejected while disconnected)",
                redisHost, redisPort, REDIS_COMMAND_TIMEOUT.toMillis());
        RedisClient client = RedisClient.create(bucket4jRedisUri(redisHost, redisPort, redisPassword));
        client.setOptions(bucket4jClientOptions());
        return client;
    }

    /**
     * 限流专用连接的地址、口令与缺省超时(RedisURI 缺省 60s)。真正约束 Bucket4j 异步命令的是
     * {@link #bucket4jClientOptions()} 的 TimeoutOptions;这里设成同一个值,免得连接的缺省超时仍是 60s、两处各说各话。
     * 包内可见,供单测钉住。
     */
    static RedisURI bucket4jRedisUri(String host, int port, String password) {
        RedisURI.Builder builder = RedisURI.builder()
                .withHost(host)
                .withPort(port)
                .withTimeout(REDIS_COMMAND_TIMEOUT);
        if (password != null && !password.isBlank()) {
            builder.withPassword(password.toCharArray());
        }
        return builder.build();
    }

    /**
     * 限流专用连接的客户端选项。包内可见,供单测钉住。
     * <ul>
     *   <li>REJECT_COMMANDS:断线期间命令立即失败,不进缓冲等重连 —— 立即抛异常、走 fail-open;
     *       自动重连保持缺省开启,Redis 恢复后限流自动恢复;</li>
     *   <li>TimeoutOptions:对所有命令(含 Bucket4j 用的异步命令)启用 {@link #REDIS_COMMAND_TIMEOUT} 固定超时,
     *       Redis 卡住时命令以异常结束,不再无限期占住请求线程;</li>
     *   <li>requestQueueSize:给协议栈里未完成的命令设上限 {@link #REDIS_REQUEST_QUEUE_SIZE},连接半开时超时命令不再无界堆积。</li>
     * </ul>
     */
    static ClientOptions bucket4jClientOptions() {
        return ClientOptions.builder()
                .disconnectedBehavior(ClientOptions.DisconnectedBehavior.REJECT_COMMANDS)
                .timeoutOptions(TimeoutOptions.enabled(REDIS_COMMAND_TIMEOUT))
                .requestQueueSize(REDIS_REQUEST_QUEUE_SIZE)
                .build();
    }

    /**
     * Bucket4j 的客户端侧配置:只设 {@link #BUCKET4J_REQUEST_TIMEOUT},其余保持缺省。包内可见,供单测钉住。
     *
     * <p>刻意不在这里设过期策略:Bucket4j 8.10.1 构建时优先取 ClientSideConfig 里的过期策略,没有才用构建器上
     * {@code withExpirationStrategy} 给的那个(按 AbstractRedisProxyManagerBuilder 的字节码核对)。这里保持为空,
     * {@link #bucket4jProxyManager} 现有的过期策略才不会被覆盖或绕开。
     */
    static ClientSideConfig bucket4jClientSideConfig() {
        return ClientSideConfig.getDefault().withRequestTimeout(BUCKET4J_REQUEST_TIMEOUT);
    }

    @Bean(destroyMethod = "close")
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public StatefulRedisConnection<byte[], byte[]> bucket4jConnection(RedisClient client) {
        return client.connect(ByteArrayCodec.INSTANCE);
    }

    /**
     * 桶空闲多久后允许 Redis 回收。取 1 小时:远大于任何一个桶的重填周期
     * (ip-rps/zone-rps 都是秒级),所以正在用的桶永远不会被提前删掉;
     * 又足够短,使一次性 IP / 一次性 zone 的桶不会长期占着内存。
     */
    private static final Duration BUCKET_IDLE_TTL = Duration.ofHours(1);

    @Bean
    @ConditionalOnProperty(prefix = "gate.rate-limit", name = "enabled", havingValue = "true")
    public ProxyManager<byte[]> bucket4jProxyManager(StatefulRedisConnection<byte[], byte[]> conn) {
        // 必须显式给过期策略,否则 rl:ip:* / rl:zone:* 是**永久 key**。
        //
        // 裸 build() 时 Bucket4j 的 AbstractRedisProxyManagerBuilder
        // 取不到 expirationStrategy,落到 ExpirationAfterWriteStrategy.none(),
        // 其 calculateTimeToLiveMillis 恒返回 -1;LettuceBasedProxyManager 在
        // ttl<=0 分支走的是不带 px 的 `SET key val NX`,于是每出现一个新的桶 key
        // 就在 Redis 里永久留一条。桶 key 里含客户端 IP(见 AssignGateRateLimiter
        // 的 ipKey),而 IP 本身可被 X-Forwarded-For 影响 —— 无上界增长且无自愈,
        // 只能人工 SCAN/DEL;而这个 Redis 与 login 的 token 是同一实例,
        // 撑到 maxmemory 会连带把登录态淘汰掉。
        //
        // basedOnTimeForRefillingBucketUpToMax:TTL = 把桶重填到满所需的时间 + 给定余量,
        // 保证"还可能被用到的桶"绝不会被提前回收。
        //
        // withClientSideConfig:整次桶操作的时限(见 BUCKET4J_REQUEST_TIMEOUT),Redis 卡住时不无限期占住请求线程。
        return LettuceBasedProxyManager.builderFor(conn)
                .withExpirationStrategy(
                        ExpirationAfterWriteStrategy.basedOnTimeForRefillingBucketUpToMax(BUCKET_IDLE_TTL))
                .withClientSideConfig(bucket4jClientSideConfig())
                .build();
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
