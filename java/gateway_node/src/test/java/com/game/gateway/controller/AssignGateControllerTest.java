package com.game.gateway.controller;

import com.game.gateway.dto.AssignGateRequest;
import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.ratelimit.AssignGateRateLimiter;
import com.game.gateway.ratelimit.ClientIpResolver;
import com.game.gateway.ratelimit.RateLimitDecision;
import com.game.gateway.service.AssignGateService;
import jakarta.servlet.http.HttpServletRequest;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.*;

/**
 * AssignGateController 的契约测试(G4b-J0)。
 *
 * <p>只验控制器自己的承诺:带 {@code queue_token} 的重入不再整个绕过限流 —— 照样把 zone 与客户端 IP
 * 交给限流器(zone 桶 + IP 桶照过),只是把 account 传成 null 以跳过账号冷却;不带 token 的首次请求把
 * account 原样交给限流器。gateway 验不了 token(密钥在 login),所以「带了 token」不能当作放行依据。
 * 重入被挡时按判决映射回包:QUEUE → code=100、queue_source="ratelimit"、不带新 token;DENY → 429。
 *
 * <p>限流器内部各层怎么判由 {@code AssignGateRateLimiterTest} 验;login 拿到 token 之后怎么处理不在这里。
 * 纯 Mockito,不起 Spring 上下文。
 */
class AssignGateControllerTest {

    private static final int ZONE = 1;
    private static final String CLIENT_IP = "203.0.113.9";
    private static final String ACCOUNT = "alice";

    private AssignGateService assignGateService;
    private AssignGateRateLimiter limiter;
    private HttpServletRequest http;
    private AssignGateController controller;

    @BeforeEach
    void setUp() {
        assignGateService = mock(AssignGateService.class);
        limiter = mock(AssignGateRateLimiter.class);
        ClientIpResolver ipResolver = mock(ClientIpResolver.class);
        http = mock(HttpServletRequest.class);
        when(ipResolver.resolve(http)).thenReturn(CLIENT_IP);
        controller = new AssignGateController(assignGateService, limiter, ipResolver);
    }

    @Test
    void reentryWithQueueToken_stillRateLimited() {
        // 随便填一个 queue_token 换不来免限流:IP 桶耗尽时照样 429,请求到不了 login。
        when(limiter.check(anyLong(), anyString(), any())).thenReturn(RateLimitDecision.deny("IP_RATE_LIMIT"));

        AssignGateResponse resp = controller.assignGate(request("forged-or-real-token"), http);

        verify(assignGateService, never()).assignGate(any());
        assertEquals(429, resp.getCode());
        assertEquals("IP_RATE_LIMIT", resp.getError());
    }

    @Test
    void reentryWithQueueToken_zoneBucketEmpty_returnsRatelimitQueueWithoutToken() {
        // J0 之前带 token 的请求走不到这条路。客户端契约(spec_G4b §4.10 J0、§11.5):重入被 zone 桶 / 波次
        // 挡住时回 code=100、queue_source="ratelimit",不带新 token,请求不进 login;客户端保留原 token
        // 按 retry_after_ms 重试。回显 token、改走 login、换成别的码都会破坏这个契约。
        when(limiter.check(anyLong(), anyString(), any())).thenReturn(RateLimitDecision.queue(2500, 0));

        AssignGateResponse resp = controller.assignGate(request("tok"), http);

        verify(assignGateService, never()).assignGate(any());
        assertEquals(100, resp.getCode());
        assertEquals("ratelimit", resp.getQueueSource());
        assertNull(resp.getQueueToken());
        assertEquals(2500L, resp.getRetryAfterMs());
    }

    @Test
    void reentryWithQueueToken_skipsAccountCooldown() {
        when(limiter.check(anyLong(), anyString(), any())).thenReturn(RateLimitDecision.pass());
        AssignGateRequest req = request("tok");
        AssignGateResponse fromLogin = new AssignGateResponse();
        when(assignGateService.assignGate(req)).thenReturn(fromLogin);

        AssignGateResponse resp = controller.assignGate(req, http);

        // 第三个参数为 null:限流器对 null account 跳过冷却;zone 与解析出的客户端 IP 照传,两层桶照过。
        // check 的 zoneId 是 long,eq(int) 会按 Integer 比较而匹配不上,所以显式转成 long。
        verify(limiter).check(eq((long) ZONE), eq(CLIENT_IP), isNull());
        // 放行后原请求原样交给 login:account 只是不给限流器,并没有从请求里抹掉。
        verify(assignGateService).assignGate(req);
        assertSame(fromLogin, resp);
        assertEquals(ACCOUNT, req.getAccount());
        assertEquals("tok", req.getQueueToken());
    }

    @Test
    void freshRequest_passesAccountToLimiter() {
        when(limiter.check(anyLong(), anyString(), any())).thenReturn(RateLimitDecision.pass());

        // null 与空白 token 都算「不带 token」:首次请求,account 原样交给限流器参与冷却。
        controller.assignGate(request(null), http);
        controller.assignGate(request("   "), http);

        verify(limiter, times(2)).check(eq((long) ZONE), eq(CLIENT_IP), eq(ACCOUNT));
        verify(assignGateService, times(2)).assignGate(any());
    }

    private static AssignGateRequest request(String queueToken) {
        AssignGateRequest req = new AssignGateRequest();
        req.setZoneId(ZONE);
        req.setAccount(ACCOUNT);
        req.setQueueToken(queueToken);
        return req;
    }
}
