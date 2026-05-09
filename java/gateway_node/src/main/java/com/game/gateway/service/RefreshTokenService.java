package com.game.gateway.service;

import com.game.gateway.dto.RefreshTokenRequest;
import com.game.gateway.dto.RefreshTokenResponse;
import com.game.gateway.grpc.LoginRpcClient;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

/**
 * Thin adapter for {@code POST /api/refresh-token} → {@code ClientPlayerLogin.RefreshToken}.
 *
 * <p>Refresh-token rotation is one-time-use server-side: each successful call
 * issues a <b>new</b> access/refresh pair and atomically deletes the old
 * refresh token. Clients must store the new values before the next call, or
 * they'll be locked out (which is by design — that's the anti-replay
 * property).
 */
@Service
public class RefreshTokenService {

    private static final Logger log = LoggerFactory.getLogger(RefreshTokenService.class);

    private final LoginRpcClient rpc;

    public RefreshTokenService(LoginRpcClient rpc) {
        this.rpc = rpc;
    }

    public RefreshTokenResponse refresh(RefreshTokenRequest req) {
        if (req.getRefreshToken() == null || req.getRefreshToken().isBlank()) {
            return RefreshTokenResponse.error(RefreshTokenResponse.CODE_AUTH_REJECTED, "empty_refresh_token");
        }
        var rpcReq = new LoginRpcClient.RefreshTokenRequestProto(req.getRefreshToken());
        try {
            LoginRpcClient.RefreshTokenResponseProto rsp = rpc.refreshToken(rpcReq);
            if (rsp.hasError()) {
                int upstream = rsp.errorCode == null ? -1 : rsp.errorCode;
                String msg = rsp.errorMessage != null ? rsp.errorMessage : ("upstream_err=" + upstream);
                return RefreshTokenResponse.error(RefreshTokenResponse.CODE_AUTH_REJECTED, msg);
            }
            return RefreshTokenResponse.ok(
                    rsp.accessToken,
                    rsp.refreshToken,
                    rsp.accessTokenExpire,
                    rsp.refreshTokenExpire
            );
        } catch (StatusRuntimeException e) {
            Status.Code c = e.getStatus().getCode();
            log.warn("refresh_token.rpc failed: {} — {}", c, e.getStatus().getDescription());
            if (c == Status.Code.UNAVAILABLE || c == Status.Code.DEADLINE_EXCEEDED) {
                return RefreshTokenResponse.error(RefreshTokenResponse.CODE_INTERNAL, "login_unavailable");
            }
            return RefreshTokenResponse.error(RefreshTokenResponse.CODE_AUTH_REJECTED, c.name());
        } catch (Exception e) {
            log.error("refresh_token.rpc unexpected error", e);
            return RefreshTokenResponse.error(RefreshTokenResponse.CODE_INTERNAL, "internal_error");
        }
    }
}
