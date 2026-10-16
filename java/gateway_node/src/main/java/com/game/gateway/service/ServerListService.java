package com.game.gateway.service;

import com.game.gateway.dto.*;
import com.game.gateway.entity.ZoneConfig;
import com.game.gateway.repository.ZoneConfigRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.LongSupplier;

/**
 * {@code GET /api/server-list}:zone_config 目录 + 探测态 → 选服列表。
 *
 * <h2>zone_config 行的短缓存(2026-10-15,G2b §9 第 7 条,负责人按「最标准」定)</h2>
 * 这是只读、公开、不限流的列表。以前每个请求一次 {@code findAll}:开服登录潮或被刷时查库次数随请求数线性增长,
 * 而网关的 Hikari 池只有 5 条(application.yaml 的 hikari 段)。现在:
 * <ul>
 *   <li>只缓存库里的行,TTL {@link #ZONE_ROWS_CACHE_TTL_NANOS}(1 秒,与 AssignGateService 的准入缓存同口径:
 *       运维手工开关区要近实时生效)。探测态(ZoneHealthProbeService 的内存快照)与「新服」判定仍按请求现算,
 *       不跟着缓存变旧。管理接口改 zone_config 不主动失效本缓存:多副本下本地失效也管不到别的副本,上限就是 1 秒。</li>
 *   <li>缓存过期瞬间的并发请求合并成一次查库(single-flight):第一个请求查,其余 join 同一个 future。</li>
 *   <li>查库失败不缓存:异常原样抛给本次的查询方与全部等待方(与改动前相同,经 Spring 默认错误处理回 5xx),
 *       future 在 finally 里摘掉,下一个请求重新查。</li>
 * </ul>
 * 缓存里的 {@link ZoneConfig} 是脱管实体(open-in-view 关闭,findAll 返回时持久化上下文已关),没有懒加载字段,
 * 跨请求共享,本类只读不写。改成会改实体的写法之前,先把缓存换成不可变快照。
 */
@Service
public class ServerListService {

    /** zone_config 行的缓存有效期。年龄按单调时钟算,写入时刻取查库完成时。 */
    static final long ZONE_ROWS_CACHE_TTL_NANOS = TimeUnit.SECONDS.toNanos(1);

    private final ZoneConfigRepository zoneConfigRepo;
    private final ZoneHealthProbeService probeService;
    /** 单调时钟(纳秒),只用来算缓存年龄。 */
    private final LongSupplier nanoClock;

    /** 最近一次成功查库的结果(已按 sort_order 排好、不可变);null = 还没成功过。失败从不写这里。 */
    private volatile ZoneRowsSnapshot cachedRows;
    /** 正在进行的那次查库;null = 没有。由发起查库的请求在 finally 里摘掉,不留失败结果。 */
    private final AtomicReference<CompletableFuture<List<ZoneConfig>>> inflightLoad = new AtomicReference<>();

    @Autowired
    public ServerListService(ZoneConfigRepository zoneConfigRepo,
                             ZoneHealthProbeService probeService) {
        this(zoneConfigRepo, probeService, System::nanoTime);
    }

    // 包级时钟注入点(与 ZoneHealthProbeService 同一做法):TTL 用例推进假时钟,不依赖墙钟。
    ServerListService(ZoneConfigRepository zoneConfigRepo,
                      ZoneHealthProbeService probeService,
                      LongSupplier nanoClock) {
        this.zoneConfigRepo = zoneConfigRepo;
        this.probeService = probeService;
        this.nanoClock = nanoClock;
    }

    public ServerListResponse getServerList() {
        List<ZoneInfoDto> dtos = getZoneRows().stream().map(this::toDto).toList();
        return new ServerListResponse(dtos);
    }

    /**
     * 返回 zone_config 的全部行(按 sort_order 升序,不可变)。缓存新鲜直接返回;否则发起或加入那一次查库。
     * 查库异常原样抛出(等待方拿到的是同一个异常),不写缓存。
     */
    private List<ZoneConfig> getZoneRows() {
        ZoneRowsSnapshot cached = cachedRows;
        if (isFresh(cached)) {
            return cached.rows();
        }

        CompletableFuture<List<ZoneConfig>> mine = new CompletableFuture<>();
        CompletableFuture<List<ZoneConfig>> active = inflightLoad.compareAndExchange(null, mine);
        if (active != null) {
            return awaitLoad(active);
        }

        try {
            // 在上面的新鲜度检查与成为查库方之间,上一次查库可能刚写完缓存并摘掉 future;
            // 成为查库方后再看一次,避免紧邻的重复查库。
            cached = cachedRows;
            if (isFresh(cached)) {
                mine.complete(cached.rows());
                return cached.rows();
            }

            List<ZoneConfig> sorted = new ArrayList<>(zoneConfigRepo.findAll());
            sorted.sort(Comparator.comparingInt(ZoneConfig::getSortOrder));
            List<ZoneConfig> rows = List.copyOf(sorted);
            cachedRows = new ZoneRowsSnapshot(rows, nanoClock.getAsLong());
            mine.complete(rows);
            return rows;
        } catch (RuntimeException | Error e) {
            mine.completeExceptionally(e);
            throw e;
        } finally {
            inflightLoad.compareAndSet(mine, null);
        }
    }

    /** 年龄用差值比较(System.nanoTime 的约定写法,数值回绕时仍正确)。 */
    private boolean isFresh(ZoneRowsSnapshot snapshot) {
        return snapshot != null
                && nanoClock.getAsLong() - snapshot.loadedAtNanos() < ZONE_ROWS_CACHE_TTL_NANOS;
    }

    private static List<ZoneConfig> awaitLoad(CompletableFuture<List<ZoneConfig>> active) {
        try {
            return active.join();
        } catch (CompletionException e) {
            Throwable cause = e.getCause();
            if (cause instanceof RuntimeException runtime) {
                throw runtime;
            }
            if (cause instanceof Error error) {
                throw error;
            }
            throw new IllegalStateException("zone_config load failed", cause);
        }
    }

    private record ZoneRowsSnapshot(List<ZoneConfig> rows, long loadedAtNanos) {}

    private ZoneInfoDto toDto(ZoneConfig zone) {
        ZoneInfoDto dto = new ZoneInfoDto();
        dto.setZoneId(zone.getZoneId());
        dto.setName(zone.getName());
        dto.setRecommended(zone.isRecommended());

        ManualZoneStatus manualStatus = ManualZoneStatus.fromCode(zone.getManualStatus());
        AutoZoneStatus autoStatus = probeService.getAutoStatus(zone.getZoneId());
        LoadLevel loadLevel = probeService.getLoadLevel(zone.getZoneId());

        // Manual status takes priority
        ZoneDisplayStatus displayStatus = resolveDisplayStatus(manualStatus, autoStatus);
        dto.setStatus(displayStatus);

        if (displayStatus == ZoneDisplayStatus.OPEN && autoStatus != AutoZoneStatus.UNKNOWN) {
            dto.setLoadLevel(loadLevel);
        }

        if (displayStatus == ZoneDisplayStatus.MAINTENANCE || displayStatus == ZoneDisplayStatus.CLOSED) {
            dto.setMaintenanceMsg(zone.getMaintenanceMsg());
        }

        if (manualStatus == ManualZoneStatus.PREVIEW && zone.getOpenTime() != null) {
            dto.setOpenTime(zone.getOpenTime().toEpochSecond(ZoneOffset.UTC));
        }

        // Mark as "new" if created within 7 days
        if (zone.getCreatedAt() != null &&
                zone.getCreatedAt().isAfter(LocalDateTime.now().minusDays(7))) {
            dto.setNew(true);
        }

        return dto;
    }

    /**
     * Merge manual override with auto-detected status.
     * Manual status always wins when not OPEN.
     * When manual is OPEN, auto DOWN overrides to MAINTENANCE.
     */
    private ZoneDisplayStatus resolveDisplayStatus(ManualZoneStatus manual, AutoZoneStatus auto) {
        return switch (manual) {
            case CLOSED -> ZoneDisplayStatus.CLOSED;
            case MAINTENANCE -> ZoneDisplayStatus.MAINTENANCE;
            case PREVIEW -> ZoneDisplayStatus.PREVIEW;
            case OPEN -> {
                if (auto == AutoZoneStatus.DOWN) {
                    yield ZoneDisplayStatus.MAINTENANCE;
                }
                if (auto == AutoZoneStatus.UNKNOWN) {
                    // 对外 status 是既有四值协议（OPEN/MAINTENANCE/CLOSED/PREVIEW），
                    // 不能把内部探测态 UNKNOWN 直接扩成新的 wire enum；旧客户端会
                    // 无法解析。探测面不可用不等于区服 DOWN，因此保持手工 OPEN，
                    // 同时 getLoadLevel() 在 UNKNOWN 时返回 null，不发布伪造负载。
                    // 真正不可登录仍由 AssignGate 的实时节点选择 fail-closed。
                    yield ZoneDisplayStatus.OPEN;
                }
                yield ZoneDisplayStatus.OPEN;
            }
        };
    }
}
