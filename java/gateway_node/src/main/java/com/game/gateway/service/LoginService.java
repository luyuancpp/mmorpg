package com.game.gateway.service;

import com.game.gateway.dto.LoginRequest;
import com.game.gateway.dto.LoginResponse;
import com.game.gateway.grpc.LoginRpcClient;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

/**
 * Translates {@code POST /api/login} payloads into the go-zero
 * {@code ClientPlayerLogin.Login} gRPC call and back.
 *
 * <p>The HTTP layer is intentionally narrow: rate-limit decisions and
 * controller plumbing live in {@link com.game.gateway.controller.LoginController};
 * this service is the gRPC adapter.
 */
@Service
public class LoginService {

    private static final Logger log = LoggerFactory.getLogger(LoginService.class);

    private final LoginRpcClient rpc;

    public LoginService(LoginRpcClient rpc) {
        this.rpc = rpc;
    }

    public LoginResponse login(LoginRequest req) {
        var rpcReq = new LoginRpcClient.LoginRequestProto(
                req.getAccount(),
                req.getPassword(),
                req.getAuthType(),
                req.getAuthToken()
        );
        try {
            LoginRpcClient.LoginResponseProto rsp = rpc.login(rpcReq);
            if (rsp.hasError()) {
                int upstream = rsp.errorCode == null ? -1 : rsp.errorCode;
                String msg = rsp.errorMessage != null ? rsp.errorMessage : ("upstream_err=" + upstream);
                // Map any upstream auth/business error to 401 — clients re-do OAuth.
                return LoginResponse.error(LoginResponse.CODE_AUTH_REJECTED, msg);
            }
            return LoginResponse.ok(
                    rsp.players,
                    rsp.accessToken,
                    rsp.refreshToken,
                    rsp.accessTokenExpire,
                    rsp.refreshTokenExpire
            );
        } catch (StatusRuntimeException e) {
            Status.Code c = e.getStatus().getCode();
            log.warn("login.rpc failed: {} — {}", c, e.getStatus().getDescription());
            if (c == Status.Code.UNAVAILABLE || c == Status.Code.DEADLINE_EXCEEDED) {
                return LoginResponse.error(LoginResponse.CODE_INTERNAL, "login_unavailable");
            }
            return LoginResponse.error(LoginResponse.CODE_AUTH_REJECTED, c.name());
        } catch (Exception e) {
            log.error("login.rpc unexpected error", e);
            return LoginResponse.error(LoginResponse.CODE_INTERNAL, "internal_error");
        }
    }
}
