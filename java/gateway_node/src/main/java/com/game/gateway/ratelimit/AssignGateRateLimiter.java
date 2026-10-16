package com.game.gateway.ratelimit;

import io.github.bucket4j.BucketConfiguration;
import io.github.bucket4j.ConsumptionProbe;
import io.github.bucket4j.distributed.BucketProxy;
import io.github.bucket4j.distributed.proxy.ProxyManager;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * Three-layer limiter for {@code /api/login} and {@code /api/assign-gate}.
 *
 * <ol>
 *   <li>Wave control: zones outside the current open-server wave -> queue.</li>
 *   <li>Zone bucket (distributed, Bucket4j+Redis): tokens/sec per zone.</li>
 *   <li>IP bucket (distributed): per-IP rate ceiling.</li>
 *   <li>账号冷却(进程内,带 TTL 的 ConcurrentHashMap):挡住同一客户端 IP 上同一账号的
 *       高频重试。冷却键 = 端点 scope + 账号 + 客户端 IP,原因见下。</li>
 * </ol>
 *
 * <p>The local account cooldown intentionally is <b>not</b> distributed: it's
 * a fast first-line filter against client retry storms; the authoritative
 * de-dup happens in go-zero login's {@code RedisLocker}. Multi-gateway
 * accuracy is fine because account spam is naturally bounded by the zone
 * bucket downstream.
 *
 * <p><b>冷却键为什么带客户端 IP(G4b-J2)。</b>{@code account} 在这一步未经认证。只按账号
 * 计时的话,任何人每隔一个冷却窗口替别人的账号调一次接口,对方在同一副本上就一直拿
 * {@code ACCOUNT_COOLDOWN} —— 限流一打开就多出一个定点封号面。键里带上 IP 之后,这样刷
 * 只能挡住同一个 IP 上的这个账号,也就是刷的人自己。代价:同一账号换网络后不共享冷却窗口;
 * 同一出口 IP(NAT)后面的人仍能互相挡;所有请求都解析成同一个 IP 时(部署在 LB 之后却没配
 * 可信代理,见 {@link ClientIpResolver})退化为只按账号,即改动前的行为。对单个客户端防重试
 * 风暴的作用不变。
 *
 * <p>Fail-open semantics: if Redis or Bucket4j throws, the limiter logs and
 * lets the request through. Reasoning: a broken limiter must not turn into a
 * second outage on top of the first.
 */
public class AssignGateRateLimiter {

    private static final Logger log = LoggerFactory.getLogger(AssignGateRateLimiter.class);

    private final ProxyManager<byte[]> proxyManager;   // null when limiter is disabled
    private final RateLimitProperties props;
    private final WaveSchedule wave;
    private final ConcurrentMap<String, Long> accountCooldown = new ConcurrentHashMap<>();

    public AssignGateRateLimiter(ProxyManager<byte[]> proxyManager,
                                 RateLimitProperties props,
                                 WaveSchedule wave) {
        this.proxyManager = proxyManager;
        this.props = props;
        this.wave = wave;
    }

    /**
     * @param zoneId   target zone (0 = "auto-pick" still allowed)
     * @param clientIp remote address; never null/blank — fall back to "?" if needed
     * @param account  account identifier or null when not yet known
     */
    public RateLimitDecision check(long zoneId, String clientIp, String account) {
        return check(zoneId, clientIp, account, "assign");
    }

    /**
     * @param cooldownScope account 冷却的命名空间。/api/login 与 /api/assign-gate
     *                      共用本单例,若共用同一冷却 key,正常的
     *                      「login 成功 → 紧接着 assign-gate」顺序调用会在第二步
     *                      必然命中 ACCOUNT_COOLDOWN(默认 5s)被 429。按端点
     *                      隔离后,各端点保留自己的防重试风暴窗口。
     */
    public RateLimitDecision check(long zoneId, String clientIp, String account, String cooldownScope) {
        if (!props.isEnabled() || proxyManager == null) {
            return RateLimitDecision.pass();
        }

        // 1. wave
        if (!wave.isOpen(zoneId)) {
            long waitSec = wave.secondsUntilOpen(zoneId);
            return RateLimitDecision.queue(waitSec * 1000L, -1);
        }

        // IP 桶键与账号冷却键共用同一个归一化后的 IP,所以放在 try 之外(纯本地运算,不会抛)。
        // ClientIpResolver 约定永不返回 null;万一传进来 null / 空白,统一落到 "?" 这一个占位值:
        // 这些请求共用一个 IP 桶,冷却退化为只按账号(J2 之前的行为)—— 既不抛异常,也不会因为
        // 缺 IP 而绕过冷却或各得一个新键。
        String ip = (clientIp == null || clientIp.isBlank()) ? "?" : clientIp;

        try {
            // 2. zone bucket (skip when zoneId=0, i.e. "any zone" — the auto
            //    selection still consumes downstream zone capacity once routed)
            if (zoneId > 0) {
                BucketProxy zoneBucket = proxyManager.builder().build(
                        zoneKey(zoneId), () -> zoneBucketConfig(zoneId));
                ConsumptionProbe probe = zoneBucket.tryConsumeAndReturnRemaining(1);
                if (!probe.isConsumed()) {
                    long retryMs = nanosToMs(probe.getNanosToWaitForRefill());
                    return RateLimitDecision.queue(retryMs, probe.getRemainingTokens());
                }
            }

            // 3. ip bucket
            BucketProxy ipBucket = proxyManager.builder().build(ipKey(ip), this::ipBucketConfig);
            if (!ipBucket.tryConsume(1)) {
                return RateLimitDecision.deny("IP_RATE_LIMIT");
            }
        } catch (RuntimeException e) {
            log.warn("rate-limit fail-open due to backend error: {}", e.toString());
            return RateLimitDecision.pass();
        }

        // 4. account cooldown (best-effort, process-local, per-endpoint scope)
        //    键带客户端 IP(G4b-J2,原因见类注释):account 未经认证,只按账号计时等于让任何人都能
        //    替别人的账号触发冷却。IP 放在末尾:地址字面量与占位值都不含 '@',所以账号里带 '@'
        //    (如邮箱)也不会和别的 (账号, IP) 拼成同一个键。唯一能含 '@' 的 IP 是 ClientIpResolver
        //    在可信代理之后原样返回的非字面量 XFF 段;能把这种值送进来的一方本来就能填任意 IP,
        //    键撞不撞都不多给它什么。
        if (account != null && !account.isBlank()) {
            String cooldownKey = cooldownScope + ":" + account + "@" + ip;
            long now = System.currentTimeMillis();
            Long prev = accountCooldown.get(cooldownKey);
            if (prev != null && now - prev < props.getAccountCooldownMs()) {
                return RateLimitDecision.deny("ACCOUNT_COOLDOWN");
            }
            accountCooldown.put(cooldownKey, now);
            // Opportunistic GC: every ~1k inserts, drop entries older than the cooldown window.
            if ((accountCooldown.size() & 0x3FF) == 0) {
                long stale = now - props.getAccountCooldownMs();
                accountCooldown.entrySet().removeIf(e -> e.getValue() < stale);
            }
        }

        return RateLimitDecision.pass();
    }

    // ── bucket configs ────────────────────────────────────────────────

    private BucketConfiguration zoneBucketConfig(long zoneId) {
        long rps = props.getZoneDefaultRps();
        long burst = props.getZoneDefaultBurst();
        var override = props.getZoneOverrides().get(zoneId);
        if (override != null) {
            if (override.getRps() > 0) rps = override.getRps();
            if (override.getBurst() > 0) burst = override.getBurst();
        }
        final long finalRps = rps;
        final long finalBurst = burst;
        return BucketConfiguration.builder()
                .addLimit(l -> l.capacity(finalBurst).refillGreedy(finalRps, Duration.ofSeconds(1)))
                .build();
    }

    private BucketConfiguration ipBucketConfig() {
        return BucketConfiguration.builder()
                .addLimit(l -> l.capacity(props.getIpBurst())
                        .refillGreedy(props.getIpRps(), Duration.ofSeconds(1)))
                .build();
    }

    // ── key encoding ──────────────────────────────────────────────────

    private static byte[] zoneKey(long zoneId) {
        return ("rl:zone:" + zoneId).getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] ipKey(String ip) {
        return ("rl:ip:" + ip).getBytes(StandardCharsets.UTF_8);
    }

    private static long nanosToMs(long nanos) {
        return Math.max(0, nanos / 1_000_000L);
    }
}
