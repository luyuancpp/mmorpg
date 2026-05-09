package com.game.gateway.service;

import com.game.gateway.dto.RefreshTokenRequest;
import com.game.gateway.dto.RefreshTokenResponse;
import com.game.gateway.grpc.LoginRpcClient;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

/**
 * Behavioural test for {@link RefreshTokenService}.
 *
 * <p>We mock {@link LoginRpcClient} so the test does not open a real gRPC
 * channel. The service is a thin adapter — what we verify is the
 * translation between protobuf response shape and the HTTP response shape:
 * <ul>
 *   <li>empty refresh_token short-circuits to 401 without hitting gRPC</li>
 *   <li>successful rpc → CODE_OK and all four token fields populated</li>
 *   <li>upstream error message → CODE_AUTH_REJECTED carrying the message</li>
 *   <li>UNAVAILABLE / DEADLINE_EXCEEDED → CODE_INTERNAL</li>
 *   <li>other gRPC status → CODE_AUTH_REJECTED with the status name</li>
 * </ul>
 */
@ExtendWith(MockitoExtension.class)
class RefreshTokenServiceTest {

    @Mock
    private LoginRpcClient rpc;

    private RefreshTokenService service;

    @BeforeEach
    void setUp() {
        service = new RefreshTokenService(rpc);
    }

    @Test
    void emptyRefreshToken_rejectedWithoutRpc() {
        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("");

        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_AUTH_REJECTED, rsp.getCode());
        assertEquals("empty_refresh_token", rsp.getMessage());
        // gRPC must not be touched when the input is obviously invalid.
        org.mockito.Mockito.verifyNoInteractions(rpc);
    }

    @Test
    void nullRefreshToken_rejectedWithoutRpc() {
        RefreshTokenRequest req = new RefreshTokenRequest();
        RefreshTokenResponse rsp = service.refresh(req);
        assertEquals(RefreshTokenResponse.CODE_AUTH_REJECTED, rsp.getCode());
        org.mockito.Mockito.verifyNoInteractions(rpc);
    }

    @Test
    void rpcSuccess_propagatesAllFourTokenFields() {
        LoginRpcClient.RefreshTokenResponseProto upstream = new LoginRpcClient.RefreshTokenResponseProto();
        upstream.accessToken = "new-access-123";
        upstream.refreshToken = "new-refresh-456";
        upstream.accessTokenExpire = 1_700_000_000L;
        upstream.refreshTokenExpire = 1_702_000_000L;

        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class))).thenReturn(upstream);

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("old-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_OK, rsp.getCode());
        assertEquals("new-access-123", rsp.getAccessToken());
        assertEquals("new-refresh-456", rsp.getRefreshToken());
        assertEquals(1_700_000_000L, rsp.getAccessTokenExpire());
        assertEquals(1_702_000_000L, rsp.getRefreshTokenExpire());
    }

    @Test
    void upstreamErrorMessage_mapsToAuthRejected() {
        LoginRpcClient.RefreshTokenResponseProto upstream = new LoginRpcClient.RefreshTokenResponseProto();
        upstream.errorCode = 404;
        upstream.errorMessage = "refresh_token_not_found";

        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class))).thenReturn(upstream);

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("stale-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_AUTH_REJECTED, rsp.getCode());
        assertEquals("refresh_token_not_found", rsp.getMessage());
    }

    @Test
    void unavailable_mapsToInternal() {
        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class)))
                .thenThrow(new StatusRuntimeException(Status.UNAVAILABLE.withDescription("login down")));

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("some-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_INTERNAL, rsp.getCode());
        assertEquals("login_unavailable", rsp.getMessage());
    }

    @Test
    void deadlineExceeded_mapsToInternal() {
        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class)))
                .thenThrow(new StatusRuntimeException(Status.DEADLINE_EXCEEDED));

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("some-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_INTERNAL, rsp.getCode());
    }

    @Test
    void nonRetriableGrpcStatus_mapsToAuthRejectedWithStatusName() {
        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class)))
                .thenThrow(new StatusRuntimeException(Status.PERMISSION_DENIED));

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("some-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_AUTH_REJECTED, rsp.getCode());
        assertEquals("PERMISSION_DENIED", rsp.getMessage());
    }

    @Test
    void unexpectedException_mapsToInternal() {
        when(rpc.refreshToken(any(LoginRpcClient.RefreshTokenRequestProto.class)))
                .thenThrow(new RuntimeException("boom"));

        RefreshTokenRequest req = new RefreshTokenRequest();
        req.setRefreshToken("some-refresh");
        RefreshTokenResponse rsp = service.refresh(req);

        assertEquals(RefreshTokenResponse.CODE_INTERNAL, rsp.getCode());
        assertEquals("internal_error", rsp.getMessage());
    }
}
