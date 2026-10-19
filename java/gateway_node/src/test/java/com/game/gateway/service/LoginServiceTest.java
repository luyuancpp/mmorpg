package com.game.gateway.service;

import com.game.gateway.dto.LoginRequest;
import com.game.gateway.dto.LoginResponse;
import com.game.gateway.grpc.LoginRpcClient;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

/**
 * {@link LoginService} 对 login.rpc 以 gRPC status 失败时的映射。
 *
 * <p>回归点：login 内部故障（账号目录读 Redis 失败曾直接 return err，变成 UNKNOWN）以前被映射成 401，
 * 客户端会以为凭证被拒、要求玩家重走 OAuth。现在只有身份类 status 回 401，其余回 500。
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

    @Test
    void upstreamInBandError_stillCarriesUpstreamTipId() {
        LoginRpcClient.LoginResponseProto upstream = new LoginRpcClient.LoginResponseProto();
        upstream.errorCode = 2035;
        when(rpc.login(any(LoginRpcClient.LoginRequestProto.class), anyInt())).thenReturn(upstream);
        LoginRequest req = new LoginRequest();
        req.setAuthType("wechat");
        req.setAuthToken("code");

        LoginResponse rsp = service.login(req);

        assertEquals(LoginResponse.CODE_AUTH_REJECTED, rsp.getCode());
        assertEquals("upstream_err=2035", rsp.getMessage());
    }
}
