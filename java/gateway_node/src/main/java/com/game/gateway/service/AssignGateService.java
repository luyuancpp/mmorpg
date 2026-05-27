package com.game.gateway.service;

import com.game.gateway.dto.AssignGateRequest;
import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.grpc.LoginRpcClient;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

/**
 * Translates {@code POST /api/assign-gate} payloads into the go-zero
 * {@code LoginPreGate.AssignGate} gRPC and back to HTTP DTO.
 *
 * <p><b>Why the local sign-token implementation went away.</b> Before the
 * 2026-05 login-queue work, this service contained a faithful copy of the
 * Go-side "select least-loaded gate, build GateTokenPayload, HMAC-sign"
 * code. Two implementations of the same algorithm in two languages is a
 * drift hazard — and the queue feature requires a single authoritative
 * source for capacity / re-entry / reconnect-bypass decisions, all of
 * which live in go-zero login. The Java side is now a transport shim:
 *
 * <ul>
 *   <li>Bucket4j / wave / IP rate-limiting still runs on the controller
 *       (first-line filter against retry storms — keeps the gRPC fanout
 *       cheap).</li>
 *   <li>Anything past the rate limiter goes straight to go-zero and the
 *       response is mapped 1:1 onto {@link AssignGateResponse}.</li>
 * </ul>
 *
 * <p>Status mapping (go-zero status → HTTP code):
 * <ul>
 *   <li>0 ADMITTED → code=0, gateIp/port/token* populated</li>
 *   <li>1 QUEUEING → code=100 (queueSource=login), queueToken/queueRank/queueTotal populated</li>
 *   <li>2 ERROR    → code=500-ish via {@code error} string</li>
 *   <li>3 EXPIRED  → code=410 (intentionally "Gone") so client UI knows to
 *                    restart from /assign-gate rather than retry /queue-status</li>
 * </ul>
 */
@Service
public class AssignGateService {

    private static final Logger log = LoggerFactory.getLogger(AssignGateService.class);

    /** Mirrors loginqueue.Status enum in go/login/.../loginqueue/queue.go. */
    private static final int STATUS_ADMITTED = 0;
    private static final int STATUS_QUEUEING = 1;
    private static final int STATUS_ERROR    = 2;
    private static final int STATUS_EXPIRED  = 3;

    /** HTTP-shape codes used by the existing {@link AssignGateResponse}. */
    private static final int HTTP_OK             = 0;
    private static final int HTTP_QUEUE          = 100;
    private static final int HTTP_QUEUE_EXPIRED  = 410;
    private static final int HTTP_INTERNAL       = 500;

    private final LoginRpcClient rpc;

    public AssignGateService(LoginRpcClient rpc) {
        this.rpc = rpc;
    }

    public AssignGateResponse assignGate(AssignGateRequest req) {
        var rpcReq = new LoginRpcClient.AssignGateRequestProto(
                req.getZoneId(),
                req.getQueueToken(),
                req.getAccount(),
                req.getDeviceId()
        );
        try {
            var rsp = rpc.assignGate(rpcReq, req.getZoneId());
            return mapAssignGate(rsp);
        } catch (StatusRuntimeException e) {
            Status.Code c = e.getStatus().getCode();
            log.warn("assigngate.rpc failed: {} — {}", c, e.getStatus().getDescription());
            AssignGateResponse out = new AssignGateResponse();
            out.setCode(HTTP_INTERNAL);
            out.setError(c == Status.Code.UNAVAILABLE
                    || c == Status.Code.DEADLINE_EXCEEDED ? "login_unavailable" : c.name());
            return out;
        } catch (Exception e) {
            log.error("assigngate.rpc unexpected error", e);
            AssignGateResponse out = new AssignGateResponse();
            out.setCode(HTTP_INTERNAL);
            out.setError("internal_error");
            return out;
        }
    }

    /**
     * Polls the queue for status. Used by {@code POST /api/queue-status}.
     *
     * <p>{@code zoneId} is the routing hint (see {@link com.game.gateway.dto.QueueStatusRequest}):
     * the poll must reach the same login.rpc instance that issued the
     * queue_token, otherwise the lookup misses the queue entry and falsely
     * returns EXPIRED.
     */
    public AssignGateResponse queryQueueStatus(String queueToken, int zoneId) {
        if (queueToken == null || queueToken.isBlank()) {
            AssignGateResponse out = new AssignGateResponse();
            out.setCode(HTTP_QUEUE_EXPIRED);
            out.setError("missing_queue_token");
            return out;
        }
        var rpcReq = new LoginRpcClient.QueryQueueStatusRequestProto(queueToken);
        try {
            var rsp = rpc.queryQueueStatus(rpcReq, zoneId);
            return mapQueueStatus(rsp, queueToken);
        } catch (StatusRuntimeException e) {
            Status.Code c = e.getStatus().getCode();
            log.warn("queryqueuestatus.rpc failed: {} — {}", c, e.getStatus().getDescription());
            AssignGateResponse out = new AssignGateResponse();
            out.setCode(HTTP_INTERNAL);
            out.setError(c == Status.Code.UNAVAILABLE
                    || c == Status.Code.DEADLINE_EXCEEDED ? "login_unavailable" : c.name());
            return out;
        } catch (Exception e) {
            log.error("queryqueuestatus.rpc unexpected error", e);
            AssignGateResponse out = new AssignGateResponse();
            out.setCode(HTTP_INTERNAL);
            out.setError("internal_error");
            return out;
        }
    }

    // ── Mapping helpers ───────────────────────────────────────────

    private static AssignGateResponse mapAssignGate(LoginRpcClient.AssignGateResponseProto rsp) {
        switch (rsp.status) {
            case STATUS_QUEUEING:
                return AssignGateResponse.loginQueueing(
                        rsp.queueToken,
                        rsp.queueRank,
                        rsp.queueTotal,
                        rsp.retryAfterMs > 0 ? rsp.retryAfterMs : 2000
                );
            case STATUS_ERROR: {
                AssignGateResponse out = new AssignGateResponse();
                out.setCode(HTTP_INTERNAL);
                out.setError(rsp.error != null ? rsp.error : "unknown_error");
                return out;
            }
            case STATUS_EXPIRED: {
                AssignGateResponse out = new AssignGateResponse();
                out.setCode(HTTP_QUEUE_EXPIRED);
                out.setError(rsp.error != null ? rsp.error : "queue_token_expired");
                return out;
            }
            case STATUS_ADMITTED:
            default:
                return admittedResponse(rsp.ip, rsp.port, rsp.tokenPayload, rsp.tokenSignature, rsp.tokenDeadline, rsp.error);
        }
    }

    private static AssignGateResponse mapQueueStatus(LoginRpcClient.QueryQueueStatusResponseProto rsp, String queueToken) {
        switch (rsp.status) {
            case STATUS_QUEUEING: {
                // Echo the same queueToken back so the client can keep polling
                // without parsing the response — a small UX nicety that also
                // matches what AssignGate did on the initial enqueue.
                AssignGateResponse out = AssignGateResponse.loginQueueing(
                        queueToken,
                        rsp.queueRank,
                        rsp.queueTotal,
                        rsp.retryAfterMs > 0 ? rsp.retryAfterMs : 2000
                );
                return out;
            }
            case STATUS_EXPIRED: {
                AssignGateResponse out = new AssignGateResponse();
                out.setCode(HTTP_QUEUE_EXPIRED);
                out.setError(rsp.error != null ? rsp.error : "queue_token_expired");
                return out;
            }
            case STATUS_ERROR: {
                AssignGateResponse out = new AssignGateResponse();
                out.setCode(HTTP_INTERNAL);
                out.setError(rsp.error != null ? rsp.error : "unknown_error");
                return out;
            }
            case STATUS_ADMITTED:
            default:
                return admittedResponse(rsp.ip, rsp.port, rsp.tokenPayload, rsp.tokenSignature, rsp.tokenDeadline, rsp.error);
        }
    }

    private static AssignGateResponse admittedResponse(String ip, int port, byte[] payload, byte[] sig, long deadline, String err) {
        AssignGateResponse out = new AssignGateResponse();
        if (ip == null || ip.isBlank()) {
            // Defensive: ADMITTED without an endpoint shouldn't happen, but if
            // a future protocol bug leaves these blank we surface it as error
            // rather than handing the client an empty connect target.
            out.setCode(HTTP_INTERNAL);
            out.setError(err != null && !err.isBlank() ? err : "admitted_without_endpoint");
            return out;
        }
        out.setCode(HTTP_OK);
        out.setGateIp(ip);
        out.setGatePort(port);
        out.setTokenPayload(payload);
        out.setTokenSignature(sig);
        out.setTokenDeadline(deadline);
        return out;
    }
}
