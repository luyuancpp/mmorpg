package com.game.gateway.service;

import com.game.gateway.dto.AutoZoneStatus;
import com.game.gateway.dto.LoadLevel;
import com.game.gateway.dto.ServerListResponse;
import com.game.gateway.dto.ZoneDisplayStatus;
import com.game.gateway.dto.ZoneInfoDto;
import com.game.gateway.entity.ZoneConfig;
import com.game.gateway.repository.ZoneConfigRepository;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.dao.DataAccessResourceFailureException;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

/**
 * /api/server-list 的 zone_config 行短缓存(2026-10-15,G2b §9 第 7 条)。
 *
 * <p>修复前 {@link ServerListService#getServerList()} 每次请求都 {@code findAll}:开服登录潮或被刷时查库次数随请求数
 * 线性增长,压在只有 5 条的 Hikari 池上。本组用例钉住:TTL 内只查一次库、到期后重查、缓存过期瞬间的并发请求只查一次、
 * 查库失败不被缓存、探测态不跟着缓存变旧。
 *
 * <p>时钟用包级构造注入的假时钟({@link AtomicLong}),不 sleep、不依赖墙钟。探测态不是多数用例的关注点:
 * 没有打桩时 mock 的 getAutoStatus / getLoadLevel 返回 null,toDto 照常走 OPEN 分支。
 */
@ExtendWith(MockitoExtension.class)
class ServerListServiceZoneRowsCacheTest {

    @Mock
    private ZoneConfigRepository zoneConfigRepo;

    @Mock
    private ZoneHealthProbeService probeService;

    private final AtomicLong nowNanos = new AtomicLong(0L);
    private ServerListService service;

    @BeforeEach
    void setUp() {
        service = new ServerListService(zoneConfigRepo, probeService, nowNanos::get);
    }

    /** 修复前红:每次请求都 findAll,第二、三次请求各多查一次库(times(1) 不成立)。 */
    @Test
    void withinTtl_servesCachedRowsWithoutRequery() {
        when(zoneConfigRepo.findAll()).thenReturn(List.of(zone(1L, "zone-1", 0)));

        ServerListResponse first = service.getServerList();
        ServerListResponse second = service.getServerList();
        nowNanos.set(ServerListService.ZONE_ROWS_CACHE_TTL_NANOS - 1);
        ServerListResponse third = service.getServerList();

        assertEquals(List.of("zone-1"), names(first));
        assertEquals(List.of("zone-1"), names(second));
        assertEquals(List.of("zone-1"), names(third));
        verify(zoneConfigRepo, times(1)).findAll();
    }

    /** 防过度缓存:恰好到 TTL 就要重查,并且看得到库里的新值(运维改名 / 开关区 1 秒内生效)。 */
    @Test
    void ttlElapsed_requeriesAndSeesNewRows() {
        when(zoneConfigRepo.findAll())
                .thenReturn(List.of(zone(1L, "zone-1", 0)))
                .thenReturn(List.of(zone(1L, "zone-1-renamed", 1)));

        ServerListResponse before = service.getServerList();
        nowNanos.set(ServerListService.ZONE_ROWS_CACHE_TTL_NANOS);
        ServerListResponse after = service.getServerList();

        assertEquals(List.of("zone-1"), names(before));
        assertEquals(List.of("zone-1-renamed"), names(after));
        assertEquals(ZoneDisplayStatus.MAINTENANCE, after.getZones().getFirst().getStatus());
        verify(zoneConfigRepo, times(2)).findAll();
    }

    /** 防过度缓存:只缓存库里的行,探测态每次现算 —— TTL 内区服被探测为 DOWN,列表立刻显示维护。 */
    @Test
    void cachedRows_stillUseLiveProbeStatus() {
        when(zoneConfigRepo.findAll()).thenReturn(List.of(zone(1L, "zone-1", 0)));
        when(probeService.getAutoStatus(1L)).thenReturn(AutoZoneStatus.HEALTHY, AutoZoneStatus.DOWN);
        when(probeService.getLoadLevel(1L)).thenReturn(LoadLevel.SMOOTH);

        ZoneInfoDto healthy = service.getServerList().getZones().getFirst();
        ZoneInfoDto down = service.getServerList().getZones().getFirst();

        assertEquals(ZoneDisplayStatus.OPEN, healthy.getStatus());
        assertEquals(LoadLevel.SMOOTH, healthy.getLoadLevel());
        assertEquals(ZoneDisplayStatus.MAINTENANCE, down.getStatus());
        verify(zoneConfigRepo, times(1)).findAll();
    }

    /** 防缓存失败结果:时钟不动,第一次查库失败后第二次请求仍要重查,并拿到成功结果;失败按原样抛出。 */
    @Test
    void databaseFailure_propagatesAndIsNotCached() {
        DataAccessResourceFailureException dbDown = new DataAccessResourceFailureException("mysql unreachable");
        when(zoneConfigRepo.findAll())
                .thenThrow(dbDown)
                .thenReturn(List.of(zone(1L, "zone-1", 0)));

        RuntimeException thrown = assertThrows(RuntimeException.class, service::getServerList);
        ServerListResponse retried = service.getServerList();

        assertSame(dbDown, thrown, "查库异常要原样抛出,错误语义与改动前相同");
        assertEquals(List.of("zone-1"), names(retried));
        verify(zoneConfigRepo, times(2)).findAll();
    }

    /**
     * 缓存过期瞬间的并发请求只查一次库(single-flight)。修复前红:每个请求各查一次(times(2) 不成立,实际 1 + 8)。
     *
     * <p>确定性:时钟停在到期那一刻不再走,重查写入的缓存对之后到达的请求一直新鲜;没赶上 join 的请求要么命中新缓存,
     * 要么在成为查库方后的二次检查里命中,都不会再查库。
     */
    @Test
    void concurrentRequestsAtExpiry_shareOneDatabaseQuery() throws Exception {
        CountDownLatch databaseEntered = new CountDownLatch(1);
        CountDownLatch releaseDatabase = new CountDownLatch(1);
        when(zoneConfigRepo.findAll())
                .thenReturn(List.of(zone(1L, "zone-1", 0)))
                .thenAnswer(invocation -> {
                    databaseEntered.countDown();
                    assertTrue(releaseDatabase.await(5, TimeUnit.SECONDS));
                    return List.of(zone(1L, "zone-1-reloaded", 0));
                });

        assertEquals(List.of("zone-1"), names(service.getServerList()));
        nowNanos.set(ServerListService.ZONE_ROWS_CACHE_TTL_NANOS);

        int callers = 8;
        CountDownLatch ready = new CountDownLatch(callers);
        CountDownLatch start = new CountDownLatch(1);
        ExecutorService pool = Executors.newFixedThreadPool(callers);
        try {
            List<Future<ServerListResponse>> results = new ArrayList<>();
            for (int i = 0; i < callers; i++) {
                results.add(pool.submit(() -> {
                    ready.countDown();
                    assertTrue(start.await(5, TimeUnit.SECONDS));
                    return service.getServerList();
                }));
            }
            assertTrue(ready.await(5, TimeUnit.SECONDS));
            start.countDown();
            assertTrue(databaseEntered.await(5, TimeUnit.SECONDS));
            releaseDatabase.countDown();

            for (Future<ServerListResponse> result : results) {
                assertEquals(List.of("zone-1-reloaded"), names(result.get(5, TimeUnit.SECONDS)));
            }
            verify(zoneConfigRepo, times(2)).findAll();
        } finally {
            releaseDatabase.countDown();
            pool.shutdownNow();
        }
    }

    private static List<String> names(ServerListResponse response) {
        return response.getZones().stream().map(ZoneInfoDto::getName).toList();
    }

    private static ZoneConfig zone(long id, String name, int manualStatus) {
        ZoneConfig z = new ZoneConfig();
        z.setZoneId(id);
        z.setName(name);
        z.setManualStatus(manualStatus);
        z.setCapacity(5000);
        z.setSortOrder(0);
        return z;
    }
}
