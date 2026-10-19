package com.game.gateway.service;

import com.game.gateway.dto.LoginRequest;
import com.game.gateway.dto.LoginResponse;
import com.game.gateway.grpc.LoginRpcClient;
import com.game.table.tip.TipFaults;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import java.util.stream.Stream;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import org.junit.jupiter.params.provider.MethodSource;
import org.junit.jupiter.params.provider.ValueSource;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

/**
 * {@link LoginService} 对 login.rpc 失败的映射。
 *
 * <p>回归点一：login 内部故障（账号目录读 Redis 失败曾直接 return err，变成 UNKNOWN）以前被映射成 401，
 * 客户端会以为凭证被拒、要求玩家重走 OAuth。现在只有身份类 status 回 401，其余回 500。
 *
 * <p>回归点二：login 回 in-band 故障码（Tip.xlsx 标了 fault 的码，如渠道故障、令牌签发失败）以前也一律 401。
 * 现在按生成的 {@link TipFaults} 回 500，message 仍带上游 tip。
 */
@ExtendWith(MockitoExtension.class)
class LoginServiceTest {

    @Mock
    private LoginRpcClient rpc;

    private LoginService service;

    @BeforeEach
    void setUp() {
        service = new LoginService(rpc);
    }

    private LoginResponse loginFailingWith(Status status) {
        when(rpc.login(any(LoginRpcClient.LoginRequestProto.class), anyInt()))
                .thenThrow(new StatusRuntimeException(status));
        LoginRequest req = new LoginRequest();
        req.setAuthType("wechat");
        req.setAuthToken("code");
        return service.login(req);
    }

    @Test
    void unknownStatus_mapsToInternalNotAuthRejected() {
        LoginResponse rsp = loginFailingWith(Status.UNKNOWN.withDescription("redis: connection refused"));
        assertEquals(LoginResponse.CODE_INTERNAL, rsp.getCode());
        assertEquals("UNKNOWN", rsp.getMessage());
    }

    @Test
    void internalStatus_mapsToInternal() {
        LoginResponse rsp = loginFailingWith(Status.INTERNAL);
        assertEquals(LoginResponse.CODE_INTERNAL, rsp.getCode());
    }

    @Test
    void unavailable_mapsToLoginUnavailable() {
        LoginResponse rsp = loginFailingWith(Status.UNAVAILABLE);
        assertEquals(LoginResponse.CODE_INTERNAL, rsp.getCode());
        assertEquals("login_unavailable", rsp.getMessage());
    }

    @Test
    void identityStatuses_mapToAuthRejected() {
        assertEquals(LoginResponse.CODE_AUTH_REJECTED, LoginService.httpCodeForRpcStatus(Status.Code.UNAUTHENTICATED));
        assertEquals(LoginResponse.CODE_AUTH_REJECTED, LoginService.httpCodeForRpcStatus(Status.Code.PERMISSION_DENIED));
    }

    /** 除身份类之外的每一个 status 都不得回 401。 */
    @ParameterizedTest
    @EnumSource(value = Status.Code.class, names = {"OK", "UNAUTHENTICATED", "PERMISSION_DENIED"},
            mode = EnumSource.Mode.EXCLUDE)
    void nonIdentityStatuses_neverMapToAuthRejected(Status.Code code) {
        assertEquals(LoginResponse.CODE_INTERNAL, LoginService.httpCodeForRpcStatus(code));
    }

    // ------------------------------------------------------------------ in-band tip 码（tip-code-axis.md §2.10）

    private LoginResponse loginReturningTip(int tipId) {
        LoginRpcClient.LoginResponseProto upstream = new LoginRpcClient.LoginResponseProto();
        upstream.errorCode = tipId;
        when(rpc.login(any(LoginRpcClient.LoginRequestProto.class), anyInt())).thenReturn(upstream);
        LoginRequest req = new LoginRequest();
        req.setAuthType("wechat");
        req.setAuthToken("code");
        return service.login(req);
    }

    /** 码值以导表结果为准，测试按码名从生成表里取，不手写数字。 */
    static int faultCode(String name) {
        return TipFaults.FAULTS.stream()
                .filter(f -> f.name().equals(name))
                .findFirst()
                .orElseThrow(() -> new AssertionError(name + " 不在 TipFaults 里：Tip.xlsx 的 fault 列没标，或还没重新导表"))
                .code();
    }

    static Stream<Integer> allFaultCodes() {
        return TipFaults.FAULTS.stream().map(TipFaults.Fault::code);
    }

    /** 本次要回 500 的三类 login 故障（渠道故障、签发失败、账号目录读 Redis 失败）必须在表里标为 fault。 */
    @ParameterizedTest
    @ValueSource(strings = {"LoginAuthProviderUnavailable", "LoginTokenIssueFailed", "LoginRedisError"})
    void loginFaultTip_mapsToInternalAndKeepsTip(String name) {
        int tip = faultCode(name);

        LoginResponse rsp = loginReturningTip(tip);

        assertEquals(LoginResponse.CODE_INTERNAL, rsp.getCode());
        assertEquals("upstream_err=" + tip, rsp.getMessage());
    }

    /** 表里每一个故障码都回 500：分类只认生成表，网关没有第二份清单。 */
    @ParameterizedTest
    @MethodSource("allFaultCodes")
    void everyFaultTip_mapsToInternal(int tip) {
        assertEquals(LoginResponse.CODE_INTERNAL, LoginService.httpCodeForUpstreamTip(tip));
    }

    @Test
    void faultTableIsNotEmpty() {
        assertFalse(TipFaults.FAULTS.isEmpty(), "Tip.xlsx 的 fault 列一个都没标，多半是列被清空了");
    }

    /** 凭证无效（kLoginAccountNotFound = 2000，业务拒绝）仍回 401，客户端重新认证。 */
    @Test
    void rejectionTip_staysAuthRejected() {
        assertFalse(TipFaults.isFault(2000), "前提：kLoginAccountNotFound 不是故障码");

        LoginResponse rsp = loginReturningTip(2000);

        assertEquals(LoginResponse.CODE_AUTH_REJECTED, rsp.getCode());
        assertEquals("upstream_err=2000", rsp.getMessage());
    }

    /** 没有码（只有 message）或本二进制不认识的码按业务拒绝处理，保持原行为。 */
    @Test
    void missingOrUnknownTip_staysAuthRejected() {
        assertEquals(LoginResponse.CODE_AUTH_REJECTED, LoginService.httpCodeForUpstreamTip(null));
        assertEquals(LoginResponse.CODE_AUTH_REJECTED, LoginService.httpCodeForUpstreamTip(Integer.MAX_VALUE));
    }
}
