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
import org.springframework.dao.QueryTimeoutException;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertInstanceOf;
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
 *
 * <p>2026-10-16 补注:另钉住等待方的时间预算(查库方挂住时等待方到点失败,不跟着无限期挂住)与「查库失败时全部等待方
 * 拿到同一个异常、不挂住、下次重查」。并发用例用 {@link ServerListService#inflightWaiterCountForTest()} 作屏障,
 * 等其余请求都挂上同一次在途查库再放行,不靠线程调度碰运气。
 */
@ExtendWith(MockitoExtension.class)
class ServerListServiceZoneRowsCacheTest {

    /**
     * 默认用例的等待预算:远大于本类所有 latch / 屏障的 5 秒上限,并发用例的等待方不会因机器慢而先超时、离开屏障。
     * 预算本身的行为由 {@link #loaderStuck_waiterTimesOutWithinBudget_thenRecoversAfterRelease()} 用短预算单独钉住。
     */
    private static final long GENEROUS_LOAD_WAIT_MILLIS = TimeUnit.SECONDS.toMillis(60);

    @Mock
    private ZoneConfigRepository zoneConfigRepo;

    @Mock
    private ZoneHealthProbeService probeService;

    private final AtomicLong nowNanos = new AtomicLong(0L);
    private ServerListService service;

    @BeforeEach
    void setUp() {
        service = new ServerListService(zoneConfigRepo, probeService, nowNanos::get, GENEROUS_LOAD_WAIT_MILLIS);
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
     * 查库失败时,查库方与全部等待方拿到同一个异常实例、不挂住;失败不留在途 future,下一次请求重新查库。
     * 单线程的 databaseFailure 用例走不到等待方路径(awaitLoad 的解包、completeExceptionally 的扇出)。
     *
     * <p>红在哪里:收窄 catch 或漏掉 completeExceptionally 时,等待方挂到 60 秒预算才失败,下面 result.get(5s) 先超时;
     * 等待方拿到包装异常或超时异常时 assertSame 不成立;没有在途合并时屏障等不齐。
     *
     * <p>确定性:查库方先单独提交并卡在 findAll 里,再放出其余请求,屏障等它们全部挂上在途 future 才放行,
     * 不会有晚到的第二个查库方。results.get(0) 是唯一的查库方,取到它的结果即保证它的 finally 已摘掉 future
     * (FutureTask 的 happens-before),所以最后一次调用一定重新查库、命中第二个打桩。
     */
    @Test
    void concurrentDatabaseFailure_allWaitersGetSameExceptionAndNextRequestRequeries() throws Exception {
        DataAccessResourceFailureException dbDown = new DataAccessResourceFailureException("mysql unreachable");
        CountDownLatch databaseEntered = new CountDownLatch(1);
        CountDownLatch releaseDatabase = new CountDownLatch(1);
        when(zoneConfigRepo.findAll())
                .thenAnswer(invocation -> {
                    databaseEntered.countDown();
                    assertTrue(releaseDatabase.await(5, TimeUnit.SECONDS));
                    throw dbDown;
                })
                .thenReturn(List.of(zone(1L, "zone-1", 0)));

        int callers = 8;
        ExecutorService pool = Executors.newFixedThreadPool(callers);
        try {
            List<Future<ServerListResponse>> results = new ArrayList<>();
            results.add(pool.submit(() -> service.getServerList()));
            assertTrue(databaseEntered.await(5, TimeUnit.SECONDS));
            for (int i = 1; i < callers; i++) {
                results.add(pool.submit(() -> service.getServerList()));
            }
            awaitInflightWaiters(callers - 1);
            releaseDatabase.countDown();

            for (Future<ServerListResponse> result : results) {
                ExecutionException failed = assertThrows(ExecutionException.class,
                        () -> result.get(5, TimeUnit.SECONDS));
                assertSame(dbDown, failed.getCause(), "查询方与等待方都要拿到同一个异常实例,不能是包装或超时异常");
            }
            // 「失败后不留在途查库」由下面两行钉住:若失败的 future 没被摘掉,下一次请求会挂到它上面、抛 dbDown。
            // (探针读的是依赖栈,future 完成后栈已清空,恒为 0,不能用它断言这一点。)
            ServerListResponse requeried = assertDoesNotThrow(service::getServerList,
                    "失败后不能留下在途查库:下一次请求必须重新查库");
            assertEquals(List.of("zone-1"), names(requeried));
            verify(zoneConfigRepo, times(2)).findAll();
        } finally {
            releaseDatabase.countDown();
            pool.shutdownNow();
        }
    }

    /**
     * 缓存过期瞬间的并发请求只查一次库(single-flight)。修复前红:每个请求各查一次(times(2) 不成立,实际 1 + 8)。
     *
     * <p>确定性:时钟停在到期那一刻不再走,重查写入的缓存对之后到达的请求一直新鲜;没赶上 join 的请求要么命中新缓存,
     * 要么在成为查库方后的二次检查里命中,都不会再查库。
     *
     * <p>2026-10-16 补注(上一段只说明了当前实现恒绿,不保证 join 路径被走到):现在查库方停在 findAll 里,
     * 直到另外 callers-1 个请求都挂上同一个在途 future 才放行({@link ServerListService#inflightWaiterCountForTest()}
     * 作屏障;5 秒只是屏障上限,与其余 latch 同口径,不参与判定)。所以等待路径每次都被走到:实现退化成「只有 TTL 缓存、
     * 没有在途合并」,或每个请求各装一个 future 时,屏障等不齐,用例必红,与核数和调度无关;此时「修复前红」也先红在屏障处。
     * 「成为查库方后的二次检查」只是窄窗口去重优化,不在本用例覆盖范围(这里所有非查库方都已挂上,走不到它)。
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
            // 屏障:另外 callers-1 个请求都挂上在途 future 后才放行查库,保证等待路径确实被走到。
            awaitInflightWaiters(callers - 1);
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

    /**
     * 查库方挂住时(半开 TCP、MDL 排队),等待方在预算内拿到超时异常,不跟着无限期挂住;放开查库后下一次请求正常返回。
     *
     * <p>修复前红:等待方无上限 join,waiter.get(5s) 抛 TimeoutException,assertThrows(ExecutionException) 不成立
     * (用例自己不会挂死,finally 放开查库)。结果不依赖墙钟快慢:查库方被 latch 卡住,直到等待方返回后才放开,等待方只能超时。
     */
    @Test
    void loaderStuck_waiterTimesOutWithinBudget_thenRecoversAfterRelease() throws Exception {
        ServerListService shortBudget = new ServerListService(zoneConfigRepo, probeService, nowNanos::get, 50L);
        CountDownLatch databaseEntered = new CountDownLatch(1);
        CountDownLatch releaseDatabase = new CountDownLatch(1);
        when(zoneConfigRepo.findAll()).thenAnswer(invocation -> {
            databaseEntered.countDown();
            assertTrue(releaseDatabase.await(5, TimeUnit.SECONDS));
            return List.of(zone(1L, "zone-1", 0));
        });

        ExecutorService pool = Executors.newFixedThreadPool(2);
        try {
            Future<ServerListResponse> loader = pool.submit(shortBudget::getServerList);
            assertTrue(databaseEntered.await(5, TimeUnit.SECONDS));

            Future<ServerListResponse> waiter = pool.submit(shortBudget::getServerList);
            ExecutionException failed = assertThrows(ExecutionException.class, () -> waiter.get(5, TimeUnit.SECONDS));
            assertInstanceOf(QueryTimeoutException.class, failed.getCause(), "等待方到点要以 5xx 语义失败,不能无限期挂住");
            assertFalse(loader.isDone(), "等待方超时不影响查库方,查库仍在进行");

            releaseDatabase.countDown();
            assertEquals(List.of("zone-1"), names(loader.get(5, TimeUnit.SECONDS)));
            assertEquals(List.of("zone-1"), names(shortBudget.getServerList()));
            verify(zoneConfigRepo, times(1)).findAll();
        } finally {
            releaseDatabase.countDown();
            pool.shutdownNow();
        }
    }

    /**
     * 屏障:等到恰好 expected 个请求挂在在途查库上。5 秒只是屏障上限(与其余 latch 同口径),不参与判定;
     * 到点仍不齐即失败,说明这些请求没有合并到同一次查库上。
     */
    private void awaitInflightWaiters(int expected) {
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5);
        while (service.inflightWaiterCountForTest() < expected && System.nanoTime() - deadline < 0) {
            Thread.yield();
        }
        assertEquals(expected, service.inflightWaiterCountForTest(), "其余请求要全部挂上同一次在途查库后才放行查库");
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
