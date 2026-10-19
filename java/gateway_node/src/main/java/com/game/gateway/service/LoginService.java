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
            // Zone-aware route: each go-zero login.rpc instance only watches
            // its own zone's player_locator / scene_manager etc., so
            // round-robin would scatter Login calls across the wrong zones
            // and produce sporadic "no gate available" / session-bind errors.
            // See LoginRpcClient.unaryCall(method, req, zoneId) and the
            // 2026-05-24 stress postmortem §I.
            LoginRpcClient.LoginResponseProto rsp = rpc.login(rpcReq, (int) req.getZoneId());
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
            return LoginResponse.error(httpCodeForRpcStatus(c), messageForRpcStatus(c));
        } catch (Exception e) {
            log.error("login.rpc unexpected error", e);
            return LoginResponse.error(LoginResponse.CODE_INTERNAL, "internal_error");
        }
    }

    /**
     * login.rpc 以 gRPC status 失败时的 HTTP 业务码。login 的业务拒绝一律走 in-band tip（上面的 hasError 分支），
     * 以 status 失败只可能是传输、热关停闸门或服务端内部错误：只有 UNAUTHENTICATED / PERMISSION_DENIED 是
     * 「调用方身份不被接受」，回 401；其余（UNKNOWN、INTERNAL 等）回 500。以前除 UNAVAILABLE / DEADLINE_EXCEEDED
     * 外一律回 401，login 内部故障（例如账号目录读 Redis 失败变成 UNKNOWN）会让玩家被要求重走 OAuth。
     * {@link RefreshTokenService} 用同一套映射。
     */
    static int httpCodeForRpcStatus(Status.Code c) {
        return c == Status.Code.UNAUTHENTICATED || c == Status.Code.PERMISSION_DENIED
                ? LoginResponse.CODE_AUTH_REJECTED
                : LoginResponse.CODE_INTERNAL;
    }

    /** 可重试的传输类失败统一给 login_unavailable（客户端既有文案），其余带 status 名便于排障。 */
    static String messageForRpcStatus(Status.Code c) {
        return switch (c) {
            case UNAVAILABLE, DEADLINE_EXCEEDED, RESOURCE_EXHAUSTED -> "login_unavailable";
            default -> c.name();
        };
    }
}
